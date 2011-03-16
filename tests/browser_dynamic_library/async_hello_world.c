/*
 * Copyright 2011 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <string.h>

#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>


#define NACL_SEND_FD 6
#define NACL_RECEIVE_FD 7


/* This is a minimal test for checking, from the context of a page,
   whether a dynamically-linked executable has run successfully.  Once
   ppruntime works when dynamically loaded, we can test this via
   PPAPI's communication channels instead. */

int main() {
  char *text = "Hello world! (async message)";
  struct NaClImcMsgIoVec iov[2] = {
    /* Include 4-byte message type header to distinguish this from
       other message types. */
    { "TEST", 4 },
    { text, strlen(text) },
  };
  struct NaClImcMsgHdr message = { iov, 2, NULL, 0, 0 };
  int sent = imc_sendmsg(NACL_SEND_FD, &message, 0);
  assert(sent > 0);
  return 0;
}
