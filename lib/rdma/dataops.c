#include <stdlib.h>

#include "tas_ll.h"
#include "tas_rdma.h"

#include "internal.h"

/* NOTE: Two data operations must not be called concurrently on the same
 * 'fd'
 */

int rdma_read(int fd, uint32_t len, uint32_t loffset, uint32_t roffset)
{
    // 1. Find listener socket
    if (fd < 1 || fd >= MAX_FD_NUM)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    struct rdma_socket* s = fdmap[fd];
    if (s->type != RDMA_CONN_SOCKET)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    struct flextcp_connection* c = &s->c;

    // 2. Validate address in memory region
    if (((uint64_t) loffset + len) > c->mr_len)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }

    // 3. Acquire Work Queue Entry
	// NOTE: c->wq_len must be a multiple of sizeof(struct rdma_wqe)
    if (c->wq_head == c->cq_tail){
        // Queue full!
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    struct rdma_wqe* wqe_pos = (c->wq_base + c->wq_head);

    // 4. Fill entries of Work Queue
    int32_t id;
    id = wqe_pos->id = c->wq_head;
    wqe_pos->type = RDMA_OP_READ;
    wqe_pos->status = RDMA_PENDING;
    wqe_pos->loff = loffset;
    wqe_pos->roff = roffset;
    wqe_pos->len = len;

    // 5. Increment Queue head
	uint32_t old_head = c->wq_head
	wqe_pos += 1
	uint32_t updated_head = (uint32_t) ((uint8_t*) wqe_pos - c->wq_base)
	if updated_head == c->wq_len:
		updated_head = 0
    MEM_BARRIER();
    c->wq_head = updated_head;

	// TODO: Handle the case where bump queue is full
	// 6. Bump the fast path
	if rdma_conn_bump(appctx, c) < 0 {
		// Move back the head (effectively revert adding wqe)
		c->wq_head = old_head
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
	}

    return id;
}

int rdma_write(int fd, uint32_t len, uint32_t loffset, uint32_t roffset)
{
    // 1. Find listener socket
    if (fd < 1 || fd >= MAX_FD_NUM)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    struct rdma_socket* s = fdmap[fd];
    if (s->type != RDMA_CONN_SOCKET)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    struct flextcp_connection* c = &s->c;

    // 2. Validate address in memory region
    if (((uint64_t) loffset + len) > c->mr_len)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }

    // 3. Acquire Work Queue Entry
	// NOTE: c->wq_len must be a multiple of sizeof(struct rdma_wqe)
    if (c->wq_head == c->cq_tail){
        // Queue full!
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    struct rdma_wqe* wqe_pos = (c->wq_base + c->wq_head);

    // 4. Fill entries of Work Queue
    int32_t id;
    id = wqe_pos->id = c->wq_head;
    wqe_pos->type = RDMA_OP_WRITE;
    wqe_pos->status = RDMA_PENDING;
    wqe_pos->loff = loffset;
    wqe_pos->roff = roffset;
    wqe_pos->len = len;

    // 5. Increment Queue head
	wqe_pos += 1
	uint32_t updated_head = (uint32_t) ((uint8_t*) wqe_pos - c->wq_base)
	if updated_head == c->wq_len:
		updated_head = 0
    MEM_BARRIER();
    c->wq_head = updated_head;

	// TODO: Handle the case where bump queue is full
	// 6. Bump the fast path
	if rdma_conn_bump(appctx, c) < 0 {
		// Move back the head (effectively revert adding wqe)
		c->wq_head = old_head
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
	}

    return id;
}

int rdma_cq_poll(int fd, struct rdma_wqe** compl_evs, uint32_t num, uint32_t timeout)
{
    return -1;
}
