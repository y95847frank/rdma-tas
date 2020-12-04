#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tas_ll.h"
#include "tas_rdma.h"

#include "internal.h"
#include "include/rdma_verbs.h"

static int init = 0;
struct rdma_event_channel *rdma_create_event_channel(void)
{
    struct rdma_event_channel *ec;
    ec = malloc(sizeof(struct rdma_event_channel));
    ec->fd = -1; // We don't use event channel, just set it to -1
    return ec;
}
void rdma_destroy_event_channel(struct rdma_event_channel *channel)
{
    if (channel)
        free(channel);
}

int rdma_create_id(struct rdma_event_channel *channel,
                   struct rdma_cm_id **id, void *context,
                   enum rdma_port_space ps)
{
    if(!init){
        if (rdma_init() == -1)
        {
            fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
            return -1;
        }
        init = 1;
    }
    struct rdma_cm_id *id_priv = calloc(1, sizeof(struct rdma_cm_id));
    if (id_priv == NULL)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    id_priv->channel = channel;
    id_priv->context = context;
    id_priv->ps = ps;
    id_priv->send_cq_channel = calloc(sizeof(struct ibv_comp_channel), 1);
    id_priv->recv_cq_channel = calloc(sizeof(struct ibv_comp_channel), 1);
    id_priv->mr = calloc(sizeof(struct ibv_mr), 1);

    // Actually, this implementation isn't RC.
    id_priv->qp_type = IBV_QPT_UC;
    // port_num -> -1 unset
    id_priv->port_num = -1;

    *id = id_priv;
    return 0;
}
int rdma_destroy_id(struct rdma_cm_id *id)
{
    rdma_destroy_event_channel(id->channel);

    // It might need destrution fuction
    if (id->verbs)
        free(id->verbs);
    if (id->qp)
        free(id->qp);
    if (id->recv_cq)
        free(id->recv_cq);
    if (id->recv_cq_channel)
        free(id->recv_cq_channel);
    if (id->send_cq)
        free(id->send_cq);
    if (id->send_cq_channel)
        free(id->send_cq_channel);
    if (id->srq)
        free(id->srq);
    if (id->pd)
        free(id->pd);
    if (id->mr)
        free(id->mr);
    return 0;
}
int rdma_bind_addr(struct rdma_cm_id *id, struct sockaddr *addr)
{
    memcpy((void*)&id->route.addr.src_addr, addr, sizeof(struct sockaddr));
    return 0;
}
int rdma_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
                      struct sockaddr *dst_addr, int timeout_ms)
{
    if (src_addr != NULL)
    {
        memcpy((void*)&id->route.addr.src_addr, src_addr, sizeof(struct sockaddr));
    }

    memcpy(&id->route.addr.dst_addr, dst_addr, sizeof(struct sockaddr));
    return 0;
}
int rdma_connect(struct rdma_cm_id *id, struct rdma_conn_param *conn_param){
    return rdma_establish(id);
}



int rdma_establish(struct rdma_cm_id *id){

    // Call rdma_connect
    struct sockaddr_in *remoteaddr =  &id->route.addr.dst_sin;
    
    // add mr to rdma_cm_id, because rdma_tas_connect does the work of mem reg?? 
    int fd = rdma_tas_connect(remoteaddr,&id->mr->addr,(uint32_t*)&id->mr->length);

    // after we got fd from connect/accept, store it to id->send_cq_channel->fd
    if(fd < 0){
        return -1;
    }
    id->send_cq_channel->fd = fd;
    return 0;
    
}

int rdma_listen(struct rdma_cm_id *id, int backlog){

    // Call rdma_listen here
    struct sockaddr_in *localaddr = &id->route.addr.src_sin;
    int lfd = rdma_tas_listen(localaddr, backlog);

    // after we got fd from listen, store it to id->recv_cq_channel->fd
     if(lfd){
        id->recv_cq_channel->fd = lfd;
        return 0;
    }
    else return -1;

}

int rdma_accept(struct rdma_cm_id *id, struct rdma_conn_param *conn_param){

    // Do nothing here
    return 0;
}

int rdma_get_request (struct rdma_cm_id *listen, struct rdma_cm_id **id){
    // Call rdma_accept here
    if (*id == NULL){
        struct rdma_event_channel * ec = rdma_create_event_channel();
        rdma_create_id(ec, id, NULL, listen->ps);
    }
    int fd =  rdma_tas_accept(listen->recv_cq_channel->fd, &(*id)->route.addr.dst_sin , &(*id)->mr->addr, (uint32_t *)&(*id)->mr->length);

    // after we got fd from connect/accept, store it to id->send_cq_channel->fd
    if(fd){
        (*id)->send_cq_channel->fd = fd;
        return 0;
    }
    else return -1;    
}

int rdma_post_write(struct rdma_cm_id *id, void *context, void *addr,
		size_t length, struct ibv_mr *mr, int flags,
		uint64_t remote_addr, uint32_t rkey)
{
    /* ignore flags, rkey
       let context hold the return value (id) of rdma_write
       ignore mr because mr is now put into id
       from doc: addr - The local address of the source of the write request. Let it hold loffset here?
       from doc: remote_addr - The address of the remote registered memory to write into.
       But remote mem is always registered to the config value, so let remote_addr here hold roffset?
    */

    // write addr to remote_addr
    // addr -> copy -> mr->addr length -> length
    if(length > id->mr->length){
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;        
    }
    memcpy((void*)id->mr->addr, addr, length);

    int ret = rdma_write(id->send_cq_channel->fd, length, 0, remote_addr);
    if(ret == -1){
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }    
    id->op_id = ret;
    return 0;

}
int rdma_post_read(struct rdma_cm_id *id, void *context, void *addr,
	       size_t length, struct ibv_mr *mr, int flags,
	       uint64_t remote_addr, uint32_t rkey)
{
    // similarly with write
    // fd we use id->send_cq_channel->fd

    int ret = rdma_read(id->send_cq_channel->fd, length, 0, remote_addr);

    if(ret == -1){
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    id->op_id = ret;
    memcpy(addr, id->mr->addr, length);
    return 0;
}
