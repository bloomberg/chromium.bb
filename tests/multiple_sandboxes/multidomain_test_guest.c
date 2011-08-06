/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>


/* Use the same descriptor number for both:  this demonstrates that the
   two sandboxes have different descriptor tables. */
#define SEND_DESC 3
#define RECEIVE_DESC 3


void domain1() {
  char *message = "hello";
  struct NaClImcMsgIoVec iov = { message, strlen(message) };
  struct NaClImcMsgHdr msg = { &iov, 1, NULL, 0, 0 };
  int sent;

  printf("In domain 1: Sending message, \"%s\"\n", message);
  sent = imc_sendmsg(SEND_DESC, &msg, 0);
  assert(sent >= 0);
}

void domain2() {
  char buffer[100];
  struct NaClImcMsgIoVec iov = { buffer, sizeof(buffer) - 1 };
  struct NaClImcMsgHdr msg = { &iov, 1, NULL, 0, 0 };
  int got = imc_recvmsg(RECEIVE_DESC, &msg, 0);
  assert(got >= 0);

  buffer[got] = 0;
  printf("In domain 2: Received message, \"%s\"\n", buffer);
}

int main(int argc, char **argv) {
  assert(argc >= 2);
  if (strcmp(argv[1], "domain1") == 0) {
    domain1();
    return 1001;
  } else if (strcmp(argv[1], "domain2") == 0) {
    domain2();
    return 1002;
  } else {
    printf("Unrecognised argument: \"%s\"\n", argv[1]);
  }
  return 0;
}
