#include <stdio.h>

#include <tas_rdma.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int main()
{
  const char ip[] = "10.0.0.4";
  rdma_init();
  struct sockaddr_in remoteaddr;
  remoteaddr.sin_family = AF_INET;
  remoteaddr.sin_addr.s_addr = inet_addr(ip);
  remoteaddr.sin_port = htons(5005);

  int fd = rdma_connect(&remoteaddr);

  if (fd < 0)
    fprintf(stderr, "Connection failed\n");
  return 0;
}
