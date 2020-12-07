#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <tas_rdma.h>
#include <rdma_verbs.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

struct rdma_cm_id ** id;
#define WQSIZE              1024
struct rdma_wqe ev[WQSIZE];

int main()
{
    const char ip[] = "10.0.0.101";
    struct rdma_event_channel *ec = rdma_create_event_channel();
    struct rdma_cm_id * listen_id;
    rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP);
    
    struct sockaddr_in localaddr;
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr(ip);
    localaddr.sin_port = htons(5005);

    void *mr_base;
    uint32_t mr_len;

    ret = rdma_bind_addr(listen_id, (struct sockaddr *)&localaddr);
    if(ret < 0){
            fprintf(stderr, "Bind failed\n");
            return -1;       
    }
    ret = rdma_listen(listen_id, 1024);
    if(ret < 0){
            fprintf(stderr, "Accept failed\n");
            return -1;       
    }

    id = calloc(1, sizeof(struct rdma_cm_id*));
    for (int i = 0; i < 1; i++)
    {
        int ret = rdma_get_request(listen_id, &id[i]);
        if (ret < 0){
            fprintf(stderr, "Connection failed\n");
            return -1;
        }
        ret = rdma_accept(id[i], NULL);
        if (ret < 0){
            fprintf(stderr, "Connection failed\n");
            return -1;
        }        
    }

    const char name[] = "foobar";
    int i = 0;
    char* new_mr_base = mr_base;
    //struct rdma_wqe cqe[50];
    while (1)
    {
        if (new_mr_base + 100 >= ((char*) mr_base + mr_len))
            new_mr_base = mr_base;

        int len = snprintf(new_mr_base, 100, "%s%u", name, i);
        //int ret = rdma_tas_write(fd, len, new_mr_base - (char*) mr_base, new_mr_base - (char*) mr_base);
        int ret = rdma_post_write(id[0], NULL, 0, len, NULL, 0, 0, 0);
        new_mr_base += len;
        fprintf(stderr, "WRITE ret=%d\n", ret);
        if (ret >= 0)
            continue;
        ret = rdma_tas_cq_poll(id[0]->send_cq_channel->fd, ev, WQSIZE);
        //ret = rdma_tas_cq_poll(fd, cqe, 64);
        fprintf(stderr, "CQ_POLL ret=%d\n", ret);
        if (ret < 0)
            break;
        int j;
        for(j = 0; j < ret; j++){
            if(ev[j].status != RDMA_SUCCESS){
                fprintf(stderr, "RDMA_STATUS: id=%d, status=%d\n",
                        ev[j].id, ev[j].status);
                return -1;
            }
        }
    }

    return 0;
}
