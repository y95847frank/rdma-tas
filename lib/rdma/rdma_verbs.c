#include "include/rdma_verbs.h"

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
    if (!rdma_init())
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
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
}
int rdma_bind_addr(struct rdma_cm_id *id, struct sockaddr *addr)
{
    int ret = memcpy(&id->route.addr.src_addr, addr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
    return ret;
}
int rdma_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
                      struct sockaddr *dst_addr, int timeout_ms)
{
    if (src_addr != NULL)
    {
        int ret = memcpy(&id->route.addr.src_addr, src_addr, sizeof(struct sockaddr));
        if (ret < 0)
        {
            fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
            return -1;
        }
    }

    ret = memcpy(&id->route.addr.dst_addr, dst_addr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;
    }
}
int rdma_connect(struct rdma_cm_id *id, struct rdma_conn_param *conn_param){
    // Do nothing
    return 0;
}



int rdma_establish(struct rdma_cm_id *id){
    // Call rdma_connect
    // after we got fd from connect/accept, store it to id->send_cq_channel->fd
}

int rdma_listen(struct rdma_cm_id *id, int backlog){
    // Call rdma_listen here
    // after we got fd from listen, store it to id->recv_cq_channel->fd
}

int rdma_accept(struct rdma_cm_id *id, struct rdma_conn_param *conn_param){
    // Call rdma_accept here
    // after we got fd from connect/accept, store it to id->send_cq_channel->fd
}

int rdma_post_write(struct rdma_cm_id *id, void *context, void *addr,
		size_t length, struct ibv_mr *mr, int flags,
		uint64_t remote_addr, uint32_t rkey)
{
    //ignore flags, rkey, and context
    // write addr to remote_addr
    // fd we use id->send_cq_channel->fd
}
int rdma_post_read(struct rdma_cm_id *id, void *context, void *addr,
	       size_t length, struct ibv_mr *mr, int flags,
	       uint64_t remote_addr, uint32_t rkey)
{
    // similarly with write
    // fd we use id->send_cq_channel->fd
}