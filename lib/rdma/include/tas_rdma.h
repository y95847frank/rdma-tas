#ifndef FLEXTCP_RDMA_H_
#define FLEXTCP_RDMA_H_

#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * RDMA operation types.
 */
enum rdma_op_type_e {
    RDMA_READ,
    RDMA_WRITE
};

/**
 * Status of RDMA operation.
 * 
 * TODO: Dummy values. Modify as necessary.
 */
enum rdma_op_status_e {
    RDMA_SUCCESS,
    RDMA_CONN_FAILURE,
    RDMA_OUT_OF_BOUNDS
};

/**
 * RDMA Work Completion event.
 * 
 * TODO: Dummy attributes. Modify as necessary.
 */
struct rdma_wc_event {
    int id;  /**> Operation ID returned in RDMA read()/write() call */
    enum rdma_op_type_e type;
    enum rdma_op_status_e status;
};

/**
 * Initialize application library to communicate with TAS.
 * [1] Setup IPC mechanisms with TAS
 * [2] Reset the state
 * 
 * Application must ensure that this is first called before
 * using any RDMA APIs
 * 
 * @return SUCCESS/FAILURE
 */
int rdma_init(void);

/**
 * Listen to RDMA connections on a TCP port.
 * 
 * This is a wrapper around socket(), bind() and listen().
 * NOTE: Forced *synchronous* mode.
 * 
 * @param localaddr IPv4 address and TCP port number
 * @param backlog   Number of pending TCP connections in queue
 *                  (Refer to *listen()* semantics)
 * @return File Descriptor on SUCCESS. -1 on FAILURE.
 */
int rdma_listen(const struct sockaddr_in* localaddr, int backlog);

/**
 * Accept pending RDMA connections on a listen socket.
 * 
 * This is a wrapper around accept().
 * NOTE: Forced *synchronous* mode.
 * 
 * @param listenfd      File descriptor returned on rdma_listen()
 * @param remoteaddr    IPv4 address and TCP port number of remote peer
 * 
 * @return File Descriptor on SUCCESS. -1 on FAILURE.
 */
int rdma_accept(int listenfd, struct sockaddr_in* remoteaddr);

/**
 * Connect to a remote RDMA-capable server.
 * 
 * This is a wrapper around connect().
 * NOTE: Forced *synchronous* mode.
 * 
 * @param remoteaddr    IPv4 address and TCP port number of remote server
 * 
 * @return File Descriptor on SUCCESS. -1 on FAILURE.
 */
int rdma_connect(const struct sockaddr_in* remoteaddr);

/**
 * One-sided communication primitive to read data
 * from remote peer's memory.
 * 
 * Copies len bytes of data from roffset in the remote peer's memory
 * to loffset in local memory.
 * 
 * NOTE: *Asynchronous*
 * 
 * @param fd    File Descriptor obtained on successful accept()/connect()
 * @param len   Number of bytes to read
 * @param loffset Offset into local memory region where the data is copied
 * @param roffset Offset into remote memory region from where the data is read
 * 
 * @return Operation identifier (op_id) on SUCCESS. -1 on FAILURE.
 *         op_id is guaranteed to increase monotonically across 
 *         READ/WRITE calls on the same connection. It may be used to
 *         query the SUCCESS or FAILURE of an operation using completion queue.
 */
int rdma_read(int fd, uint32_t len, uint32_t loffset, uint32_t roffset);

/**
 * One-sided communication primitive to write data
 * to remote peer's memory.
 * 
 * Copies len bytes of data to roffset in the remote peer's memory
 * from loffset in local memory.
 * 
 * NOTE: *Asynchronous*
 * 
 * @param fd    File Descriptor obtained on successful accept()/connect()
 * @param len   Number of bytes to read
 * @param loffset Offset into local memory region where the data is copied
 * @param roffset Offset into remote memory region to where the data is written
 * 
 * @return Operation identifier (op_id) on SUCCESS. -1 on FAILURE.
 *         op_id is guaranteed to increase monotonically across 
 *         READ/WRITE calls on the same connection. It may be used to
 *         query the SUCCESS or FAILURE of an operation using completion queue.
 */
int rdma_write(int fd, uint32_t len, uint32_t loffset, uint32_t roffset);

/**
 * Fetch completion event with the status of a completed operation.
 * 
 * NOTE: *Blocking* or *Non-blocking* depending on params
 * 
 * @param fd    File Descriptor obtained on successful accept()/connect()
 * @param ev    Reference to RDMA event descriptor.
 * @param timeout Maximum amount of time to block to wait for event completion.
 *                Returns immediately if 0.
 * @return SUCCESS/FAILURE. If successful, completion event is copied to *ev*.
 */
int rdma_poll_event(int fd, struct rdma_wc_event* ev, uint32_t timeout);

#endif /* FLEXTCP_RDMA_H_ */
