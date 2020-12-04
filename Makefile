-include Makefile.local

PREFIX ?= /usr/local
SBINDIR ?= $(PREFIX)/sbin
LIBDIR ?= $(PREFIX)/lib
INCDIR ?= $(PREFIX)/include

RTE_SDK ?= /usr/

CFLAGS += -std=gnu99 -O3 -g -Wall -Wno-address-of-packed-member -Werror -I. -Iinclude/ -march=native -fno-omit-frame-pointer
LDFLAGS += -pthread -g

RTE_SDK ?= $(HOME)/dpdk/x86_64-native-linuxapp-gcc
DPDK_PMDS ?= ixgbe i40e tap virtio

CFLAGS+= -I$(RTE_SDK)/include -I$(RTE_SDK)/include/dpdk
CFLAGS+= -I$(RTE_SDK)/include/x86_64-linux-gnu/dpdk/
LDFLAGS+= -L$(RTE_SDK)/lib/

LIBS_DPDK= -Wl,--whole-archive
LIBS_DPDK+= $(addprefix -lrte_pmd_,$(DPDK_PMDS))
LIBS_DPDK+= -lrte_eal -lrte_mempool -lrte_mempool_ring \
	    -lrte_hash -lrte_ring -lrte_kvargs -lrte_ethdev \
	    -lrte_mbuf -lnuma -lrte_bus_pci -lrte_pci \
	    -lrte_cmdline -lrte_timer -lrte_net -lrte_kni \
	    -lrte_bus_vdev -lrte_gso \
	    -Wl,--no-whole-archive -ldl $(EXTRA_LIBS_DPDK)

LDLIBS += -lm -lpthread -lrt -ldl

UTILS_OBJS = $(addprefix lib/utils/,utils.o rng.o timeout.o)
TASCOMMON_OBJS = $(addprefix tas/,tas.o config.o shm.o)
SLOWPATH_OBJS = $(addprefix tas/slow/,kernel.o packetmem.o appif.o appif_ctx.o \
	nicif.o cc.o tcp.o arp.o routing.o kni.o)
FASTPATH_OBJS = $(addprefix tas/fast/,fastemu.o network.o \
		    qman.o trace.o fast_kernel.o fast_appctx.o fast_flows.o \
			fast_rdma.o)
STACK_OBJS = $(addprefix lib/tas/,init.o kernel.o conn.o connect.o)
SOCKETS_OBJS = $(addprefix lib/sockets/,transfer.o context.o manage_fd.o \
	epoll.o libc.o)
INTERPOSE_OBJS = $(addprefix lib/sockets/,interpose.o)
RDMA_OBJS = $(addprefix lib/rdma/,rdma_verbs.o)

CFLAGS += -I. -Ilib/tas/include -Ilib/rdma/include

shared_objs = $(patsubst %.o,%.shared.o,$(1))

TESTS_AUTO= \
	tests/libtas/tas_ll \
	tests/tas_unit/fastpath \

TESTS_AUTO_FULL= \
	tests/full/tas_linux \

TESTS_PING= \
	tests/rdma_client_ping \
	tests/rdma_server_pong \
	tests/rdma_client_ping_new \
	tests/rdma_server_pong_new

TESTS= \
	tests/lowlevel \
	tests/lowlevel_echo \
	tests/bench_ll_echo \
	tests/rdma_client \
	tests/rdma_server \
	tests/rdma_multi_server \
	tests/rdma_multi_client \
	tests/rdma_multi_client_read \
	$(TESTS_AUTO) \
	$(TESTS_AUTO_FULL)\
	$(TESTS_PING)


all: lib/libtas_rdma.so lib/libtas.so \
	tools/tracetool tools/statetool tools/scaletool \
	tas/tas

tests: $(TESTS)
tests_ping: $(TESTS_PING)

# run all simple testcases
run-tests: $(TESTS_AUTO)
	tests/libtas/tas_ll
	tests/tas_unit/fastpath

# run full tests that run full TAS
run-tests-full: $(TESTS_AUTO_FULL) tas/tas
	tests/full/tas_linux

docs:
	cd doc && doxygen

tas/%.o: CFLAGS+=-Itas/include
tas/tas: LDLIBS+=$(LIBS_DPDK)
tas/tas: $(TASCOMMON_OBJS) $(FASTPATH_OBJS) $(SLOWPATH_OBJS) $(UTILS_OBJS)

flexnic/tests/tcp_common: flexnic/tests/tcp_common.o

tests/lowlevel: tests/lowlevel.o lib/libtas.so
tests/lowlevel_echo: tests/lowlevel_echo.o lib/libtas.so

tests/bench_ll_echo: tests/bench_ll_echo.o lib/libtas.so

tests/rdma_client: tests/rdma_client.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_server: tests/rdma_server.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_multi_client: tests/rdma_multi_client.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_multi_server: tests/rdma_multi_server.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_multi_client_read: tests/rdma_multi_client_read.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_client_ping: tests/rdma_client_ping.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_server_pong: tests/rdma_server_pong.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_client_ping_new: tests/rdma_client_ping_new.o lib/libtas_rdma.so lib/libtas.so
tests/rdma_server_pong_new: tests/rdma_server_pong_new.o lib/libtas_rdma.so lib/libtas.so

tests/libtas/tas_ll: tests/libtas/tas_ll.o tests/libtas/harness.o \
	tests/libtas/harness.o tests/testutils.o lib/libtas.so

tests/tas_unit/%.o: CFLAGS+=-Itas/include
tests/tas_unit/fastpath: LDLIBS+=-lrte_eal
tests/tas_unit/fastpath: tests/tas_unit/fastpath.o tests/testutils.o \
  tas/fast/fast_flows.o tas/fast/fast_rdma.o

tests/full/%.o: CFLAGS+=-Itas/include
tests/full/tas_linux: tests/full/tas_linux.o tests/full/fulltest.o lib/libtas.so

tools/tracetool: tools/tracetool.o
tools/statetool: tools/statetool.o lib/libtas.so
tools/scaletool: tools/scaletool.o lib/libtas.so

lib/libtas_rdma.so: $(call shared_objs, \
	$(RDMA_OBJS) $(STACK_OBJS) $(UTILS_OBJS))

lib/libtas.so: $(call shared_objs, $(UTILS_OBJS) $(STACK_OBJS))

%.shared.o: %.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

%.so:
	$(CC) $(LDFLAGS) -shared $^ $(LOADLIBES) $(LDLIBS) -o $@


clean:
	rm -fv *.o tas/*.o tas/fast/*.o tas/slow/*.o lib/utils/*.o \
	  lib/tas/*.o lib/sockets/*.o lib/rdma/*.o tests/*.o tests/*/*.o \
	  tools/*.o \
	  lib/libtas_rdma.so \
	  lib/libtas.so \
	  $(TESTS) \
	  tools/tracetool tools/statetool tools/scaletool \
	  tas/tas

install: tas/tas lib/libtas_rdam.so lib/libtas.so tools/statetool
	mkdir -p $(DESTDIR)$(SBINDIR)
	cp tas/tas $(DESTDIR)$(SBINDIR)/tas
	cp tools/statetool $(DESTDIR)$(SBINDIR)/tas-statetool
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp lib/libtas_rdma.so $(DESTDIR)$(LIBDIR)/libtas_rdma.so
	cp lib/libtas.so $(DESTDIR)$(LIBDIR)/libtas.so

uninstall:
	rm -f $(DESTDIR)$(SBINDIR)/tas
	rm -f $(DESTDIR)$(SBINDIR)/tas-statetool
	rm -f $(DESTDIR)$(LIBDIR)/libtas_rdma.so
	rm -f $(DESTDIR)$(LIBDIR)/libtas.so

.PHONY: all tests clean docs install uninstall run-tests
