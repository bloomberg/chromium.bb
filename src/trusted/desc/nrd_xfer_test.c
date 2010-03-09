/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Test code for NRD xfer library.
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/nacl_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"

int server = 0;

int main(int ac, char **av) {
  int                         opt;
  char const                  *message = NULL;
  char                        *conn_addr = NULL;
  ssize_t                     rv;
  struct NaClDesc             *channel;
  struct NaClNrdXferEffector  eff;
  struct NaClDescEffector     *effp;
  struct NaClDesc             *pair[2];
  struct NaClSocketAddress    nsa;
  struct NaClDescConnCap      ndcc;
  struct NaClImcTypedMsgHdr   msg_hdr;
  struct NaClImcMsgIoVec      iov[1];
  struct NaClDesc             *desc_buffer[NACL_ABI_IMC_USER_DESC_MAX];
  char                        data_buffer[4096];
  size_t                      i;
  char                        *transfer_file = NULL;

  printf("Hello world\n");

  NaClNrdAllModulesInit();

  printf("Learning to walk... (parsing command line)\n");

  while (EOF != (opt = getopt(ac, av, "c:m:st:v"))) {
    switch (opt) {
      case 'c':
        conn_addr = optarg;
        break;
      case 'm':
        message = optarg;
        break;
      case 's':
        server = 1;
        break;
      case 't':
        transfer_file = optarg;
        break;
      case 'v':
        NaClLogIncrVerbosity();
        break;
      default:
        fprintf(stderr,
                "Usage: nrd_xfer_test [-sv] [-c connect-addr] [-m message]\n"
                "         [-t transfer_file_name]\n"
                "\n");
        fprintf(stderr,
                 "    -s run in server mode (prints NaCl sock addr)\n"
                 "    -v increases verbosity in the NRD xfer library\n"
                 "    -c run in client mode, with server NaCl sock addr as\n"
                 "       parameter\n"
                 "    -m message to be sent to peer (client sends message\n"
                 "       as payload data in IMC datagram; and if -t was\n"
                 "       specifed in the client to transfer a file\n"
                 "       descriptor to the server, the server will write its\n"
                 "       message into the file via the transferred\n"
                 "       descriptor\n");
        return 1;
    }
  }

  printf("Learning to talk... (setting up channels)\n");

  if (NULL == message) {
    if (server) {
      message = "\"Hello world!\", from server\n";
    } else {
      message = "\"Goodbye cruel world!\", from client\n";
    }
  }

  if (0 != (rv = NaClCommonDescMakeBoundSock(pair))) {
    fprintf(stderr, "make bound sock returned %"NACL_PRIdS"\n", rv);
    return 2;
  }

  if (!NaClNrdXferEffectorCtor(&eff, pair[0])) {
    fprintf(stderr, "EffectorCtor failed\n");
    return 3;
  }
  effp = (struct NaClDescEffector *) &eff;
  memset(desc_buffer, 0, sizeof desc_buffer);
  memset(data_buffer, 0, sizeof data_buffer);

  if (server) {
    /*
     * print out our sockaddr, accept a connection, then receive a message,
     * and print it out
     */

    /* not opaque type */
    printf("Server socket address:\n%.*s\n",
           NACL_PATH_MAX,
           ((struct NaClDescConnCap *) pair[1])->cap.path);
    fflush(stdout);

    if (0 != (rv = (*pair[0]->vtbl->AcceptConn)(pair[0], effp))) {
      fprintf(stderr, "AcceptConn returned %"NACL_PRIdS"\n", rv);
      return 4;
    }

    channel = NaClNrdXferEffectorTakeDesc(&eff);
    if (NULL == channel) {
      fprintf(stderr, "Could not take descriptor from accept\n");
      return 5;
    }

    iov[0].base = data_buffer;
    iov[0].length = sizeof data_buffer;

    msg_hdr.iov = iov;
    msg_hdr.iov_length = NACL_ARRAY_SIZE(iov);
    msg_hdr.ndescv = desc_buffer;
    msg_hdr.ndesc_length = NACL_ARRAY_SIZE(desc_buffer);

    rv = NaClImcRecvTypedMessage(channel, effp, &msg_hdr, 0);

    printf("Receive returned %"NACL_PRIdS"\n", rv);

    if (!NaClIsNegErrno(rv)) {
      /* Sanity check: make sure the return value is within range.
       * This is a panic check because NaClImcRecvTypedMessage should
       * never return more than the amount of data we asked for, and
       * that should never be more than INT_MAX.
       */
      if(((size_t)rv > sizeof data_buffer) || (rv > INT_MAX)) {
        NaClLog(LOG_FATAL, "Buffer overflow in NaClImcRecvTypedMessage. "
                "Requested %"NACL_PRIdS" bytes, received %"NACL_PRIdS".",
                sizeof data_buffer, rv);
      }
      /* Casting rv to int here because otherwise the pedantic Mac compiler
       * will complain. Cast is safe due to the range check above.
       */
      printf("Data bytes: %.*s\n", (int)rv, data_buffer);
      printf("Got %"NACL_PRIdNACL_SIZE" NaCl descriptors\n",
             msg_hdr.ndesc_length);

      for (i = 0; i < msg_hdr.ndesc_length; ++i) {
        struct NaClDesc *ndp;
        size_t msglen = strlen(message);
        ssize_t write_result;
        /*
         * TODO(bsy): a bit gross; we should expose type tags and RTTI
         * in a better way, e.g, downcast functions.  (Though exposing
         * type tags allows users to use a switch statement on the
         * type tag, rather than linearly trying to downcast to all
         * subclasses.)
         */
        ndp = msg_hdr.ndescv[i];
        printf(" type %d\n", ndp->vtbl->typeTag);

        write_result = (*ndp->vtbl->Write)(ndp,
                                           effp,
                                           (void *) message,
                                           msglen);
        if (-1 == write_result || msglen != (size_t) write_result) {
          printf("Write failed: got %"NACL_PRIdS", expected %"NACL_PRIuS"\n",
                 write_result, msglen);
        }

        NaClDescUnref(ndp);
      }
    }

    NaClDescUnref(channel);
    NaClDescUnref(pair[0]);
    NaClDescUnref(pair[1]);

  } else {
    if (NULL == conn_addr) {
      fprintf(stderr,
              "Client needs server socket address to which to connect\n");
      return 100;
    }

    memset(&nsa, 0, sizeof nsa);
    strncpy(nsa.path, conn_addr, sizeof nsa.path);  /* not nec'y NUL term'd */

    if (!NaClDescConnCapCtor(&ndcc, &nsa)) {
      fprintf(stderr,
              "Client conn cap initialization failed\n");
      return 101;
    }

    rv = (*ndcc.base.vtbl->ConnectAddr)((struct NaClDesc *) &ndcc, effp);

    printf("Connect returned %"NACL_PRIdS"\n", rv);

    if (0 != rv) {
      fprintf(stderr, "Client could not connect\n");
      return 102;
    }

    channel = NaClNrdXferEffectorTakeDesc(&eff);
    if (NULL == channel) {
      fprintf(stderr, "Could not take descriptor from connect\n");
      return 103;
    }

    strncpy(data_buffer, message, sizeof data_buffer);
    iov[0].base = data_buffer;
    iov[0].length = strlen(data_buffer);

    msg_hdr.iov = iov;
    msg_hdr.iov_length = NACL_ARRAY_SIZE(iov);
    msg_hdr.ndesc_length = 0;
    msg_hdr.ndescv = desc_buffer;

    if (NULL != transfer_file) {
      int                 xfer_fd;
      struct NaClHostDesc *nhdp = malloc(sizeof *nhdp);

      xfer_fd = OPEN(transfer_file, O_CREAT| O_WRONLY | O_TRUNC, 0777);
      if (-1 == xfer_fd) {
        fprintf(stderr, "Could not open file \"%s\" to transfer descriptor.\n",
                transfer_file);
        return 104;
      }
      NaClHostDescPosixTake(nhdp, xfer_fd, O_RDWR);
      desc_buffer[0] = (struct NaClDesc *) NaClDescIoDescMake(nhdp);
      msg_hdr.ndesc_length = 1;
    }

    rv = NaClImcSendTypedMessage(channel, effp, &msg_hdr, 0);

    if (NULL != desc_buffer[0]) {
      NaClDescUnref(desc_buffer[0]);
      desc_buffer[0] = NULL;
    }

    printf("Send returned %"NACL_PRIdS"\n", rv);
  }

  (*effp->vtbl->Dtor)(effp);

  NaClNrdAllModulesFini();
  return 0;
}
