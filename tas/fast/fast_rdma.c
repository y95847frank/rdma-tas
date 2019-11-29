#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "fastpath.h"
#include "packet_defs.h"
#include "internal.h"
#include "tas_memif.h"
#include "tas.h"
#include "tas_rdma.h"
#include "tcp_common.h"

static inline void rdma_poll_workqueue(struct dataplane_context* ctx, 
      struct flextcp_pl_flowst* fl);
static inline void fast_rdma_txbuf_copy(struct flextcp_pl_flowst* fl, 
      uint32_t len, void* src);


int fast_rdmaqueue_bump(struct dataplane_context *ctx, uint32_t flow_id,
    uint32_t new_wq_head, uint32_t new_cq_tail)
{
  struct flextcp_pl_flowst *fs = &fp_state->flowst[flow_id];

  fs_lock(fs);

/**
 * Work queue regions
 *  
 *    !!!!!!!!!!!!+++++++++++++%%%%%%%%%%%%%^^^^^^^^^^^^^!!!!!!!!!!!!!  
 *  ||------------A------------B------------C------------D------------||
 * 
 *  A: cq_tail            !: Free/Unallocated WQEs
 *  B: cq_head            +: Completed WQEs unread by app
 *  C: wq_tail            %: Unack'd WQEs - req. sent but not ack'd
 *  D: wq_head            ^: In-progress WQEs - req. not yet sent by fp
 * 
 *  NOTE: head is always non-inclusive - i.e. [tail, head)
 */

  uint32_t wq_len, wq_head, wq_tail;
  uint32_t cq_head, cq_tail;
  wq_len = fs->wq_len;
  wq_head = fs->wq_head;
  wq_tail = fs->wq_tail;
  cq_head = fs->cq_head;
  cq_tail = fs->cq_tail;

  /**
   *  TODO: Validate wq bump
   * 
   *  NOTE: Is this too expensive ?!
   *  Should we trust the application input blindly ?
   */
  uint8_t invalid = ((new_wq_head >= wq_len)
        || (wq_tail < new_wq_head && new_wq_head < wq_head)
        || (new_wq_head < wq_head && wq_head < wq_tail)
        || (wq_head < wq_tail && wq_tail <= new_wq_head)
        || (new_cq_tail >= wq_len)
        || (new_cq_tail < cq_tail && cq_tail < cq_head)
        || (cq_tail < cq_head && cq_head < new_cq_tail)
        || (cq_head < new_cq_tail && new_cq_tail < cq_tail)
        || (wq_tail < new_wq_head && wq_tail < cq_head && cq_head < new_wq_head)
        || (wq_tail < new_wq_head && wq_tail < new_cq_tail && new_cq_tail < new_wq_head)
        || (new_wq_head < wq_tail && cq_head > wq_tail)
        || (cq_head < new_wq_head && new_wq_head < wq_tail)
        || (new_wq_head < wq_tail && new_cq_tail > wq_tail)
        || (new_cq_tail < new_wq_head && new_wq_head < wq_tail));

  if (UNLIKELY(invalid))
  {
    goto RDMA_BUMP_ERROR;
  }

  /* Update the queue */
  fs->wq_head = new_wq_head;
  fs->cq_tail = new_cq_tail;

  /* No pending workqueue requests previously !*/
  if (wq_head == wq_tail)
  {
    uint32_t old_avail, new_avail;
    // copy packets into txbuf
    old_avail = tcp_txavail(fs, NULL);
    rdma_poll_workqueue(ctx, fs);
    new_avail = tcp_txavail(fs, NULL);

    if (old_avail < new_avail) {
      if (qman_set(&ctx->qman, flow_id, fs->tx_rate, new_avail -
            old_avail, TCP_MSS, QMAN_SET_RATE | QMAN_SET_MAXCHUNK
            | QMAN_ADD_AVAIL) != 0)
      {
        fprintf(stderr, "fast_rdmaqueue_bump: qman_set failed, UNEXPECTED\n");
        abort();
      }
    }
  }
  fs_unlock(fs);
  return 0;

RDMA_BUMP_ERROR:
  fs_unlock(fs);
  fprintf(stderr, "Invalid bump flowid=%u len=%u wq_head=%u wq_tail=%u \
          cq_head=%u cq_tail=%u new_wq_head=%u new_cq_tail=%u\n",
          flow_id, wq_len, wq_head, wq_tail, cq_head, cq_tail,
          new_wq_head, new_cq_tail);
  return -1;
}

static inline void fast_rdma_txbuf_copy(struct flextcp_pl_flowst* fl, 
      uint32_t len, void* src)
{
  uintptr_t buf1, buf2;
  uint32_t len1, len2;
  
  uint32_t txbuf_len = fl->tx_len;
  uint32_t txbuf_head = fl->txb_head;

  if (txbuf_head + len > txbuf_len)
  {
    len1 = (txbuf_len - txbuf_head);
    len2 = (len - len1);
  }
  else
  {
    len1 = len;
    len2 = 0;
  }
  
  buf1 = (uintptr_t) dma_pointer(fl->tx_base + txbuf_head, len1);
  buf2 = (uintptr_t) dma_pointer(fl->tx_base, len2);

  dma_write(buf1, len1, src);
  if (!len2)
    dma_write(buf2, len2, src + len1);

  txbuf_head += len;
  if (txbuf_head >= txbuf_len)
    txbuf_head -= txbuf_len;
  
  fl->txb_head = txbuf_head;
  fl->tx_avail += len;
}

static inline void rdma_poll_workqueue(struct dataplane_context* ctx, 
        struct flextcp_pl_flowst* fl)
{
  // NOTE: Flow state lock is already acquired !

  /* There is atleast one unprocessed workqueue entry */
  assert(fl->wq_head != fl->wq_tail);

  uint32_t wq_head, wq_tail, tx_seq, tx_len;
  uint32_t free_txbuf_len, wqe_tx_pending_len;
  struct rdma_wqe* wqe;
  struct rdma_hdr hdr;
  void* mr_buf;

  wq_head = fl->wq_head;
  wq_tail = fl->wq_tail;
  tx_seq = fl->wqe_tx_seq;
  free_txbuf_len = fl->tx_len - fl->tx_avail - fl->tx_sent;
  while (wq_tail != wq_head && free_txbuf_len > 0)
  {
    wqe = dma_pointer(fl->wq_base + wq_tail, sizeof(struct rdma_wqe));
    wqe = (struct rdma_wqe*) (fl->wq_base + wq_tail);

    /* Partially transmitted workqueue entry */
    if (wqe->status == RDMA_TX_PENDING)
    {
      wqe_tx_pending_len = wqe->len - tx_seq;
    }
    else
    {
      // Do not segment RDMA header
      if (free_txbuf_len < sizeof(struct rdma_hdr))
      {
        break;
      }

      // Handle error case when [loff + len] is outside MR
      if (UNLIKELY(wqe->loff + wqe->len > fl->mr_len))
      {
        wqe->status = RDMA_OUT_OF_BOUNDS;
        goto NEXT_WQE;
      }

      wqe->status = RDMA_TX_PENDING;

      hdr.type = RDMA_REQUEST | (wqe->type == RDMA_OP_READ ? RDMA_READ : RDMA_WRITE);
      hdr.status = 0;
      hdr.length = t_beui32(wqe->len);
      hdr.offset = t_beui32(wqe->roff);
      hdr.id = t_beui32(wqe->id);
      hdr.flags = t_beui16(0);

      fast_rdma_txbuf_copy(fl, sizeof(struct rdma_hdr), &hdr);
      
      free_txbuf_len -= sizeof(struct rdma_hdr);
      tx_seq = 0;
      wqe_tx_pending_len = wqe->len;

      if (wqe->type == RDMA_OP_READ)
      {
        wqe->status = RDMA_RESP_PENDING;
        goto NEXT_WQE;
      }
    }

    tx_len = MIN(wqe_tx_pending_len, free_txbuf_len);
    mr_buf = dma_pointer(fl->mr_base + wqe->loff + tx_seq, wqe_tx_pending_len);
    fast_rdma_txbuf_copy(fl, tx_len, mr_buf);
    tx_seq += tx_len;

    free_txbuf_len -= tx_len;
    if (tx_seq == wqe->len)
    {
      wqe->status = RDMA_RESP_PENDING;

NEXT_WQE:
      wq_tail += sizeof(struct rdma_wqe);
      if (wq_tail >= fl->wq_len)
        wq_tail -= fl->wq_len;

      tx_seq = 0;
    }
  }

  fl->wq_tail = wq_tail;
  fl->wqe_tx_seq = tx_seq;
}