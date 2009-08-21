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


/** @file
 *
 * Tests the Nrd Xfer protocol implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>

void thread_sleep(unsigned int num_seconds) {
  pthread_mutex_t mu;
  pthread_cond_t  cv;
  struct timeval  tv;
  struct timespec ts;

  if (0 == num_seconds) {
    return;
  }
  pthread_mutex_init(&mu, NULL);
  pthread_cond_init(&cv, NULL);

  gettimeofday(&tv, NULL);
  ts.tv_sec = tv.tv_sec + num_seconds;
  ts.tv_nsec = 0;

  pthread_mutex_lock(&mu);
  pthread_cond_timedwait(&cv, &mu, &ts);  /* just for the timeout */
  pthread_mutex_unlock(&mu);

  pthread_cond_destroy(&cv);
  pthread_mutex_destroy(&mu);
}

int main(int ac, char **av) {
  int                         opt;
  char const                  *message = NULL;
  int                         server = 0;
  int                         client_desc = -1;
  int                         channel;
  int                         rv;
  struct NaClImcMsgIoVec      iov[1];
  struct NaClImcMsgHdr        msg_hdr;
  char                        data_buffer[4096];
  int                         desc_buffer[IMC_USER_DESC_MAX];
  size_t                      i;
  char                        *transfer_file = NULL;
  unsigned long               loop_iter = 1;
  unsigned int                sleep_seconds = 0;

  printf("Hello world\n");

  printf("Learning to walk... (parsing command line)\n");

  while (EOF != (opt = getopt(ac, av, "c:l:m:sS:t:v"))) {
    switch (opt) {
      case 'c':
        client_desc = strtol(optarg, (char **) 0, 0);
        /* descriptor holds a connection capability for the server */
        break;
      case 'l':
        loop_iter = strtoul(optarg, (char **) 0, 0);
        break;
      case 'm':
        message = optarg;
        break;
      case 's':
        server = 1;
        break;
      case 'S':
        sleep_seconds = strtoul(optarg, (char **) 0, 0);
        break;
      case 't':
        transfer_file = optarg;
        break;
      default:
        fprintf(stderr,
                "Usage: sel_ldr [sel_ldr_args] -- "  /* no newline */
                "nrd_xfer_test.nexe [-c server-addr-desc] [-s]\n"
                "    [-l loop_count] [-S server-sleep-sec]\n"
                "    [-t file-to-open-and-transfer]\n"
                "    [-m message-string]\n"
                "\n"
                "Typically, server is run using a command such as\n"
                "\n"
                "    sel_ldr -X -1 -D 1 -- nrd_xfer_test.nexe -s\n"
                "\n"
                "so the socket address is printed to standard output,\n"
                "and then the client is run with a command such as\n"
                "\n"
                "    sel_ldr -X -1 -a 6:<addr-from-server> " /* no \n */
                "-- nrd_xfer_test.nexe -c 6\n"
                "\n"
                "to have descriptor 6 be used to represent the socket\n"
                "address for connecting to the server\n"
                "\n"
                "If the client includes -t to open a file\n"
                "for the server to write into,\n"
                "remember that the -d (debug) flag is needed for\n"
                "sel_ldr to enable file opens.\n");
        return 1;
    }
  }

  for (i = 0; i < sizeof desc_buffer / sizeof desc_buffer[0]; ++i) {
    desc_buffer[i] = -1;
  }

  printf("Learning to talk... (setting up channels)\n");

  if (NULL == message) {
    if (server) {
      message = "\"Hello world!\", from server\n";
    } else {
      message = "\"Goodbye cruel world!\", from client\n";
    }
  }

  do {
    if (server) {
      /*
       * accept a connection, then receive a message, and print it out
       */
      printf("Accepting a client connection...\n");
      channel = imc_accept(3);
      printf("...got channel descriptor %d\n", channel);

      iov[0].base = data_buffer;
      iov[0].length = sizeof data_buffer;

      msg_hdr.iov = iov;
      msg_hdr.iov_length = 1;
      msg_hdr.descv = desc_buffer;
      msg_hdr.desc_length = sizeof desc_buffer / sizeof desc_buffer[0];

      rv = imc_recvmsg(channel, &msg_hdr, 0);

      printf("Receive returned %d\n", rv);

      if (rv >= 0) {
        printf("Data bytes: %.*s\n", rv, data_buffer);

        /* newlib printf does not implement %zd */
        printf("Got %u NaCl descriptors\n", msg_hdr.desc_length);

        for (i = 0; i < msg_hdr.desc_length; ++i) {
          int   rcvd_desc;
          int   msg_len = strlen(message);
          int   write_ret;

          rcvd_desc = msg_hdr.descv[i];
          if (msg_len != (write_ret = write(rcvd_desc, message, msg_len))) {
            fprintf(stderr, "write of %.*s returned %d, expected %d\n",
                    msg_len, message, write_ret, msg_len);
          }
          if (-1 == close(rcvd_desc)) {
            fprintf(stderr, "close of transferred descriptor %d failed\n",
                    rcvd_desc);
          }
        }
        printf("done processing descriptors\n");
      }

      if (-1 == close(channel)) {
        fprintf(stderr, "close of channel %d failed\n", channel);
      }
      if (sleep_seconds > 0) {
        printf("sleeping for %d seconds...\n", sleep_seconds);
        thread_sleep(sleep_seconds);
      }
    } else {
      if (-1 == client_desc) {
        fprintf(stderr,
                "Client needs server socket address to which to connect\n");
        return 100;
      }

      channel = imc_connect(client_desc);

      printf("Connect returned %d\n", channel);

      if (-1 == channel) {
        fprintf(stderr, "Client could not connect\n");
        return 102;
      }

      strncpy(data_buffer, message, sizeof data_buffer);
      iov[0].base = data_buffer;
      iov[0].length = strlen(data_buffer);

      msg_hdr.iov = iov;
      msg_hdr.iov_length = 1;
      msg_hdr.desc_length = 0;
      msg_hdr.descv = desc_buffer;
      /*
       * this is not strictly needed except for when transfer_file is non-NULL
       */

      if (NULL != transfer_file) {
        int                 xfer_fd;

        xfer_fd = open(transfer_file, O_CREAT | O_WRONLY | O_TRUNC, 0777);
        if (-1 == xfer_fd) {
          fprintf(stderr,
                  "Could not open file \"%s\" to transfer descriptor.\n",
                  transfer_file);
          return 104;
        }

        desc_buffer[0] = xfer_fd;
        msg_hdr.desc_length = 1;
      }

      rv = imc_sendmsg(channel, &msg_hdr, 0);

      printf("Send returned %d\n", rv);

      if (-1 != desc_buffer[0]) {
        if (-1 == close(desc_buffer[0])) {
          fprintf(stderr,
                  "close of %d (src xfer fd) failed\n", desc_buffer[0]);
        }
      }
      if (-1 == close(channel)) {
        fprintf(stderr, "close of %d (channel) failed\n", channel);
      }
    }
  } while (--loop_iter > 0);
  return 0;
}
