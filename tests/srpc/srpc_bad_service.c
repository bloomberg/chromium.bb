/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>
#include <sys/time.h>

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
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
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
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
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
  int       first = (int) arg;
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
    first = 0;
  }
  return NULL;
}

/* Returns 1 if successful, 0 otherwise. */
/*
 * TODO(sehr,bsy): rewrite to use nanosleep syscall and/or sleep library
 * function when those are available.
 */
int my_sleep(uint32_t sleep_seconds) {
  pthread_mutex_t mu;
  pthread_cond_t cv;
  struct timeval tv;
  struct timespec ts;

  /* Set up the mutex and condition variable. */
  pthread_mutex_init(&mu, NULL);
  pthread_cond_init(&cv, NULL);

  /* Get the current time */
  if (0 != gettimeofday(&tv, NULL)) {
    /* return failure */
    return 0;
  }
  /* Set the timeout for the current time plus the requested interval. */
  ts.tv_sec = tv.tv_sec + sleep_seconds;
  ts.tv_nsec = tv.tv_usec * 1000;
  /* Do a timed condition variable loop. */
  pthread_mutex_lock(&mu);
  while (1) {
    int rval = pthread_cond_timedwait_abs(&cv, &mu, &ts);
    if (ETIMEDOUT == rval) {
      break;
    }
  }
  pthread_mutex_unlock(&mu);
  /* Destroy the mutex and condition variable. */
  pthread_cond_destroy(&cv);
  pthread_mutex_destroy(&mu);
  /* return success */
  return 1;
}

int main() {
  pthread_t acceptor_tid;
  int       is_embedded;

  is_embedded = (srpc_get_fd() != -1);
  if (is_embedded) {
    /* Start the acceptor thread.  */
    pthread_create(&acceptor_tid, NULL, acceptor, (void *) 1);
    pthread_detach(acceptor_tid);
  }
  /* Wait forever so that acceptor and clients can run. */
  while (1) {
    static const uint32_t kSleepSeconds = 1;
    if (!my_sleep(kSleepSeconds)) {
      return 1;
    }
  }
  return 0;
}
