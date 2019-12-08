#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <tas_rdma.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define NUM_CONNECTIONS     1024
#define MESSAGE_SIZE        64
#define NUM_PENDING_MSGS    32
#define MRSIZE              16*1024
#define WQSIZE              64

int fd[NUM_CONNECTIONS];
void* mr_base[NUM_CONNECTIONS];
uint32_t mr_len[NUM_CONNECTIONS];

int main(int argc, char* argv[])
{
    assert(argc == 6);
    char* rip = argv[1];
    int rport = atoi(argv[2]);
    int num_conns = atoi(argv[3]);
    uint32_t msg_len = (uint32_t) atoi(argv[4]);
    int pending_msgs = atoi(argv[5]);

    assert(num_conns < NUM_CONNECTIONS);
    assert(msg_len < MRSIZE);
    assert(pending_msgs < WQSIZE);

    rdma_init();
    struct sockaddr_in remoteaddr;
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_addr.s_addr = inet_addr(rip);
    remoteaddr.sin_port = htons(rport);

    for (int i = 0; i < num_conns; i++)
    {
        fd[i] = rdma_connect(&remoteaddr, &mr_base[i], &mr_len[i]);

        if (fd[i] < 0)
        {
            fprintf(stderr, "Connection failed\n");
            return -1;
        }
    }

    fprintf(stderr, "Connections established: %d\n", num_conns);
    getchar();
    return 0;
}