#include "include/rdma_verbs.h"

int rdma_create_id(struct rdma_event_channel *channel,
                   struct rdma_cm_id **id, void *context,
                   enum rdma_port_space ps)
{
    if(!rdma_init()){
        fprintf(stderr, "[ERROR] %s():%u failed\n", __func__, __LINE__);
        return -1;        
    }
    struct rdma_cm_id *id_priv = calloc(1, sizeof(struct rdma_cm_id));
    if(id_priv == NULL){
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
int rdma_destroy_id(struct rdma_cm_id *id){
    rdma_destroy_event_channel(id->channel);

    // It might need destrution fuction
    if (id->verbs)
        free(id->verbs);
    if (id->qp)
        free(id->qp);
    if (id->recv_cq)
        free(id->recv_cq);
    if(id->recv_cq_channel)
        free(id->recv_cq_channel);
    if(id->send_cq)
        free(id->send_cq);
    if(id->send_cq_channel)
        free(id->send_cq_channel);
    if(id->srq)
        free(id->srq);
    if(id->pd)
        free(id->pd);
}