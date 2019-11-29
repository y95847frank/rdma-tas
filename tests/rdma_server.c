#include <stdio.h>
#include <stdint.h>

#include <tas_rdma.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int main()
{
    const char ip[] = "10.0.0.101";
    rdma_init();
    struct sockaddr_in localaddr, remoteaddr;
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr(ip);
    localaddr.sin_port = htons(5005);

    void *mr_base;
    uint32_t mr_len;

    int lfd = rdma_listen(&localaddr, 8);
    int fd = rdma_accept(lfd, &remoteaddr, &mr_base, &mr_len);

    if (fd < 0)
        fprintf(stderr, "Connection failed\n");

    const char name[] = "RAJATH";
    int i = 0;
    char* new_mr_base = mr_base;
    while (1)
    {
      int len = snprintf(new_mr_base, 100, "%s%u", name, i);
      int ret = rdma_write(fd, len, new_mr_base - (char*) mr_base, 0);
      new_mr_base += len;
      fprintf(stderr, "WRITE ret=%d\n", ret);

      if (ret < 0)
        break;
      i++;
    }

    return 0;
}
