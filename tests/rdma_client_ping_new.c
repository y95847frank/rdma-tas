#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <tas_rdma.h>
#include <rdma_verbs.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define NUM_CONNECTIONS     65535
#define MESSAGE_SIZE        64
#define MRSIZE              64*1024
#define WQSIZE              1024

struct rdma_cm_id ** id;
void* mr_base[NUM_CONNECTIONS];
uint32_t mr_len[NUM_CONNECTIONS];
int count[NUM_CONNECTIONS];
struct rdma_wqe ev[WQSIZE];
uint64_t latency[8 * WQSIZE * 30];

static inline uint64_t get_nanos(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long) lo)| (((unsigned long long) hi) << 32);
}

int main(int argc, char* argv[])
{
    assert(argc == 6);
    char* rip = argv[1];
    int rport = atoi(argv[2]);
    int num_conns = atoi(argv[3]);
    uint32_t msg_len = (uint32_t) atoi(argv[4]);
    int pending_msgs = atoi(argv[5]);
    uint64_t compl_msgs = 0;

    assert(num_conns < NUM_CONNECTIONS);
    assert(msg_len < MRSIZE);
    assert(pending_msgs < WQSIZE);

    struct sockaddr_in remoteaddr;
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_addr.s_addr = inet_addr(rip);
    remoteaddr.sin_port = htons(rport);
    
    id = calloc(NUM_CONNECTIONS, sizeof(struct rdma_cm_id*));
    for (int i = 0; i < num_conns; i++)
    {
        struct rdma_event_channel *ec = rdma_create_event_channel();
        int ret = rdma_create_id(ec, &id[i], NULL, RDMA_PS_TCP);
        if (ret < 0)
        {
            fprintf(stderr, "Connection failed\n");
            return -1;
        }
        ret = rdma_resolve_addr(id[i], NULL, (struct sockaddr *)&remoteaddr, 1000);
        if(ret < 0){
                fprintf(stderr, "Resolve address failed\n");
                return -1;       
        }
        ret = rdma_connect(id[i], NULL);
        if (ret < 0)
        {
            fprintf(stderr, "Connection failed\n");
            return -1;
        }
    }

    fprintf(stderr, "Connections established: %d\n", num_conns);
    fprintf(stderr, "Type Enter to start PingPong.\n");
    getchar();

    fprintf(stderr, "Pinging %s with %d of data:\n", rip, msg_len);

    for(int i = 0; i < num_conns; i++){
        // Init local buffer
        mr_len[i] = id[i]->mr->length;
        mr_base[i] = malloc(mr_len[i]);
    }
    
    char* c = mr_base[0];
    char f = 'a';
    int read_base = msg_len * pending_msgs;
    for (int i = 0; i < mr_len[0]; i++)
    {
        if(i >= read_base) {
            f = '-';
        }
        else if(i % msg_len == 0 && i > 0) {
            f++;
            if(f > 'z') {
                f = 'a';
            }
        }
        *c = f;
        c++;
    }
    printf("mem size %d, mem: %.*s\n", mr_len[0], read_base*4, (char*)mr_base[0]);
    
    count[0] = pending_msgs;
    for (int i = 0; i < num_conns; i++)
    {
        memcpy(mr_base[i], mr_base[0], mr_len[0]);
        memcpy(id[i]->mr->addr, mr_base[0], mr_len[0]);
        count[i] = pending_msgs;
    }
    

    uint64_t start_time = get_nanos();
    uint64_t iter = 0;
    uint64_t latency_count = 0;
    uint64_t total_latency = 0;
    bool write_flag = true;
    int stopCount = 0;
    
    while (1)
    {
        iter ++;
        for (int i = 0; i < num_conns; i++)
        {
            int ret = rdma_cq_poll(id[i]->send_cq_channel->fd, ev, WQSIZE);
            if (ret < 0)
            {
                fprintf(stderr, "%s():%d\n", __func__, __LINE__);
                return -1;
            }
            else if (ret > 0) {
                if (write_flag) {
                    printf("Finished Read!\n");
                }
                else {
                    printf("Finished Write!\n");
                }
                printf("Current mem size %d, mem: %.*s\n", mr_len[0], read_base*4, (char*)id[i]->mr->addr);
                stopCount += 1;
                if (stopCount > 5) {
                    return -1;
                }
            }

            count[i] += ret;
            compl_msgs += ret;

            int j = 0;
            for (j = 0; j < count[i] && write_flag; j++)
            {
                /*
                    TODO:
                    rdma_reg_write();
                */
                uint32_t loff =  msg_len*j;
                int ret = rdma_post_write(id[i], NULL, &loff, msg_len, NULL, 0, msg_len*j, 0);
                if (ret < 0)
                {
                    fprintf(stderr, "%s():%d\n", __func__, __LINE__);
                    return -1;
                }
                int ret_val = id[i]->op_id;

                if (ret_val % 200 == 0)
                {
                    latency[ret_val] = rdtsc();
                }
            }
            if (j > 0) {
                write_flag = false;
                printf("Start writing %d msg to server. Msg should be like: %.*s\n", j, msg_len*j, (char*)mr_base[i]);
                count[i] -= j;
            }
            
            int k = 0;
            for (k = 0; k < count[i] && !write_flag; k++)
            {
                /*
                    TODO:
                    rdma_reg_read();
                */
                uint32_t loff = read_base+msg_len*k;
                int ret = rdma_post_read(id[i], NULL, &loff, 
                                        msg_len, NULL, 0, msg_len *k, 0);
                if (ret < 0)
                {
                    fprintf(stderr, "%s():%d\n", __func__, __LINE__);
                    return -1;
                }
                int ret_val = id[i]->op_id;
                if (ret_val % 200 == 0)
                {
                    latency[ret_val] = rdtsc();
                }
            }
            
            if (k > 0) {
                write_flag = true;
                printf("Read %d msg from server. Msg should be like: %.*s\n\n", k, msg_len*k, (char*)mr_base[i]);
                count[i] -= k;
            }
        }

        if (iter % 50000000 == 0)
        {
            uint64_t cur_time = get_nanos();
            double diff = (cur_time - start_time)/1000000000.;
            double tpt = (compl_msgs*msg_len*8)/(diff * 1024);
            double latency = (total_latency/3000.)/latency_count;
            fprintf(stderr, "Msgs: %lu Bytes: %lu Time: %lf Throughput=%lf Kbps Latency=%lf us\n", compl_msgs, compl_msgs*msg_len, 
                diff, tpt, latency);
        }
    }
    return 0;
}
