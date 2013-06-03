/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/imc_types.h"

#define BOUND_SOCKET  3

struct worker_state {
  int d;
};

/*
 * Requests for service discovery should be answered with a response with the
 * service discovery string.  This test ensures that the caller behaves as
 * expected when a request is sent in return.
 */

/*
 * An arbitrary buffer size.
 */
#define kBufferSize (16 * 1024)

/*
 * Receive a message from the browser.
 */
static void ReceiveRequest(int d) {
  char recvbuf[kBufferSize];
  struct NaClAbiNaClImcMsgHdr header;
  struct NaClAbiNaClImcMsgIoVec iovec[1];
  int retval;
  /* Construct the IO vector to have one data payload. */
  iovec[0].base = recvbuf;
  iovec[0].length = sizeof(recvbuf);
  /* Set the header to have the IO vector and no descriptors. */
  header.iov = iovec;
  header.iov_length = sizeof(iovec) / sizeof(iovec[0]);
  header.descv = 0;
  header.desc_length = 0;
  /* Receive the message. */
  retval = imc_recvmsg(d, &header, 0);
  if (0 >= retval) {
    fprintf(stderr, "Message not received: %d, errno %d.\n", retval, errno);
    exit(1);
  }
}

/*
 * A bogus SRPC message.  This is a request when the caller expects a response.
 */
static const uint8_t kRequestHeader[] = {
  0x02, 0x00, 0xda, 0xc0,                         /* protocol_version */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* request_id */
  0x01,                                           /* is_request */
  0x00, 0x00, 0x00, 0x00,                         /* rpc_number */
};

/*
 * Send a bad response (a request, actually).
 */
static void SendBadResponse(int d) {
  struct NaClAbiNaClImcMsgHdr header;
  struct NaClAbiNaClImcMsgIoVec iovec[1];
  /* Construct the IO vector to have one data payload. */
  iovec[0].base = (char*) kRequestHeader;
  iovec[0].length = sizeof(kRequestHeader);
  /* Set the header to have the IO vector and no descriptors. */
  header.iov = iovec;
  header.iov_length = sizeof(iovec) / sizeof(iovec[0]);
  header.descv = NULL;
  header.desc_length = 0;
  /* Send the message. */
  imc_sendmsg(d, &header, 0);
}

/*
 * Fake SRPC worker thread: receive requests, send a bad response.
 */
static void *worker(void *arg) {
  struct worker_state *state = (struct worker_state *) arg;
  /* Receive message, send request back. */
  while (1) {
    /* Get the message. */
    ReceiveRequest(state->d);
    /* Send back a request */
    SendBadResponse(state->d);
  }
  free(arg);
  return 0;
}

/*
 * Acceptor loop: accept client connections, and for each, spawn a
 * worker thread that invokes the bad message loop.
 */
static void *acceptor(void *arg) {
  int       d;

  while (-1 != (d = imc_accept(BOUND_SOCKET))) {
    struct worker_state *state = malloc(sizeof *state);
    pthread_t           worker_tid;

    if (NULL == state) {
      fprintf(stderr, "No memory for accept\n");
      exit(1);
    }
    state->d = d;
    /* worker thread is responsible for state and d. */
    pthread_create(&worker_tid, NULL, worker, state);
  }
  return NULL;
}

int main(void) {
  pthread_t acceptor_tid;

  /* This test can only run as embedded. */
  /* Start the acceptor thread.  */
  pthread_create(&acceptor_tid, NULL, acceptor, (void *) 1);
  pthread_detach(acceptor_tid);
  /* Wait forever so that acceptor and clients can run. */
  while (1) {
    sleep(1);
  }
  return 0;
}
