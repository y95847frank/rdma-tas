#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <tas_rdma.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define NUM_CONNECTIONS     65535
#define MESSAGE_SIZE        64
#define NUM_PENDING_MSGS    63

struct rdma_cm_id **id;
/*
int fd[NUM_CONNECTIONS];
void* mr_base[NUM_CONNECTIONS];
uint32_t mr_len[NUM_CONNECTIONS];
*/

int main(int argc, char* argv[])
{
    assert(argc == 4);

    char* ip = argv[1];
    int port = atoi(argv[2]);
    int num_connections = atoi(argv[3]);
    assert(num_connections < NUM_CONNECTIONS);
    fprintf(stderr, "Params: ip=%s port=%d conns=%d\n", ip, port, num_connections);

    struct rdma_event_channel * ec = rdma_create_event_channel();
    struct rdma_cm_id * listen_id;
    rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP);

    struct sockaddr_in localaddr;
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr(ip);
    localaddr.sin_port = htons(port);
    int ret = rdma_bind_addr(listen_id, (struct sockaddr *)&localaddr);
    if(ret < 0){
            fprintf(stderr, "Bind failed\n");
            return -1;       
    }
    ret = rdma_listen(listen_id, 1024);
    if(ret < 0){
            fprintf(stderr, "Accept failed\n");
            return -1;       
    }

    id = calloc(NUM_CONNECTIONS, sizeof(struct rdma_cm_id*));
    for (int i = 0; i < num_connections; i++)
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
    /*
    for (int i = 0; i < num_connections; i++)
    {
        fd[i] = rdma_tas_accept(lfd, &remoteaddr, &mr_base[i], &mr_len[i]);

        if (fd[i] < 0)
        {
            fprintf(stderr, "Connection failed\n");
            return -1;
        }
    }
    */

    fprintf(stderr, "Connections established: %d\n", num_connections);
    getchar();
    return 0;
}