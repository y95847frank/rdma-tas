#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <tas_rdma.h>
#include <rdma_verbs.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

struct rdma_cm_id ** id;

int main()
{
  const char ip[] = "10.0.0.101";
  //rdma_tas_init();
  struct sockaddr_in remoteaddr;
  remoteaddr.sin_family = AF_INET;
  remoteaddr.sin_addr.s_addr = inet_addr(ip);
  remoteaddr.sin_port = htons(5005);

  void *mr_base;
  //uint32_t mr_len;

  id = calloc(1, sizeof(struct rdma_cm_id*));
  struct rdma_event_channel *ec = rdma_create_event_channel();
  int ret = rdma_create_id(ec, &id[0], NULL, RDMA_PS_TCP);

  if (ret < 0)
  {
      fprintf(stderr, "Connection failed\n");
      return -1;
  }
  ret = rdma_resolve_addr(id[0], NULL, (struct sockaddr *)&remoteaddr, 1000);
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

  //int fd = rdma_tas_connect(&remoteaddr, &mr_base, &mr_len);

  //if (fd < 0)
  //  fprintf(stderr, "Connection failed\n");

  getchar();

  char *ptr = (char*) mr_base;
  for (int i = 0; i < 512; i++)
  {
    fprintf(stderr, "%c", ptr[i]);
  }
  fprintf(stderr, "\n");

  return 0;
}
