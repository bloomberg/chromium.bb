# prerequisite: NACL_CC must be specified.
# Optionally you could define CFLAGS (like -m32 or -m64)

NACL_CC = ../../../toolchain/linux_x86/bin/nacl64-gcc

ifneq ($(shell [ -e ../../../scons-out/nacl-x86-64/lib ] ; echo $$?), 0)
  $(error ../../../scons-out/nacl-x86-64/lib not found. Run ./scons --mode=nacl,opt-linux platform=x86-64 from native_client directory )
endif

ifneq ($(shell [ -e ../../../scons-out/opt-linux-x86-64/lib ] ; echo $$?), 0)
  $(error ../../../scons-out/opt-linux-x86-64/lib not found. Run ./scons --mode=nacl,opt-linux platform=x86-64 from native_client directory )
endif


all: hello.nexe launch

hello.nexe: nacl/hello.c
	$(NACL_CC) $(CFLAGS) -o nacl/hello.nexe nacl/hello.c -L ../../../scons-out/nacl-x86-64/lib -limc_c -lsrpc -lpthread -lstdc++

launch: launcher/launch.cc
	$(CC) -o launcher/launch -I../../../../ launcher/launch.cc -L ../../../scons-out/opt-linux-x86-64/lib/ -lnonnacl_util -lnonnacl_srpc -lnrd_xfer -lplatform -limc_c -lgio -lcrypto -lstdc++ -lrt -ldl

run: nacl/hello.nexe launcher/launch
	launcher/launch ../../../scons-out/opt-linux-x86-64/staging/ nacl/hello.nexe

clean:
	rm -rf nacl/hello.o nacl/hello.nexe launcher/launch
