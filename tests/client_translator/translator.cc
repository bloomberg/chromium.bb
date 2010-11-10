/* Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* NativeClient shared memory handle return test. */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <nacl/nacl_srpc.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/nacl_syscalls.h>
#include <sys/stat.h>

#define ONE_K 1024

extern int get_real_fd(int dd);
extern int NaClFile_fd(int fd, int mode);
extern int NaClFile_new(int mode);
extern int NaClFile_new_of_size(int mode, size_t size);

/*
 * Translate processes the input file descriptor and
 * returns a descriptor to the output shared memory region.
 */
void Translate(NaClSrpcRpc *rpc,
               NaClSrpcArg **in_args,
               NaClSrpcArg **out_args,
               NaClSrpcClosure *done) {
  int fd_in = in_args[0]->u.hval;
  int nacl_fd_in;
  int nacl_fd_out;
  int bufsize = 16;
  int nchar;
  char buf[16];

  /* Chrome integration requires shared memory descriptors. */
  nacl_fd_in = NaClFile_fd(fd_in, O_RDONLY);
  printf("Translate: Got fd %d = %d \n", fd_in, nacl_fd_in);

  nacl_fd_out = NaClFile_new_of_size(O_WRONLY, ONE_K);
  printf("Translate: Got new fd %d \n", nacl_fd_out);

  printf("Translate: To upper \n");
  while (read(nacl_fd_in, buf, bufsize) > 0) {
    for (nchar = 0; nchar < bufsize; nchar++) {
      buf[nchar] = toupper(buf[nchar]);
    }
    write(nacl_fd_out, buf, bufsize);
  }
  printf("Translate: Closing \n");

  close(nacl_fd_in);

  /* Return the shm descriptor. */
  out_args[0]->u.hval = get_real_fd(nacl_fd_out);

  close(nacl_fd_out);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * Run returns the length of the file.
 */
void Run(NaClSrpcRpc *rpc,
         NaClSrpcArg **in_args,
         NaClSrpcArg **out_args,
         NaClSrpcClosure *done) {
  int fd_in = in_args[0]->u.hval;
  int nacl_fd_in;
  struct stat stb;

  nacl_fd_in = NaClFile_fd(fd_in, O_RDONLY);
  printf("Run: Got fd %d = %d \n", fd_in, nacl_fd_in);

  if (0 != fstat(get_real_fd(nacl_fd_in), &stb)) {
    printf("Run: Can not fstat fd %d = %d \n", fd_in, nacl_fd_in);
  }

  /* Return the length of the file. */
  out_args[0]->u.ival = stb.st_size;

  close(nacl_fd_in);

  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "translate:h:h", Translate },
  { "run:h:i", Run },
  { NULL, NULL },
};

int main() {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}

