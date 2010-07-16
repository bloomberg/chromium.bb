/*
 * Copyright 2010  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>


int send_message(int sock_fd, char *data, size_t data_size,
                 int *fds, int fds_size) {
  struct NaClImcMsgIoVec iov;
  struct NaClImcMsgHdr msg;
  iov.base = data;
  iov.length = data_size;
  msg.iov = &iov;
  msg.iov_length = 1;
  msg.descv = fds;
  msg.desc_length = fds_size;
  return imc_sendmsg(sock_fd, &msg, 0);
}

int receive_message(int sock_fd, char *buffer, size_t buffer_size,
                    int *fds, int fds_size, int *fds_got) {
  struct NaClImcMsgIoVec iov;
  struct NaClImcMsgHdr msg;
  int received;
  iov.base = buffer;
  iov.length = buffer_size;
  msg.iov = &iov;
  msg.iov_length = 1;
  msg.descv = fds;
  msg.desc_length = fds_size;
  received = imc_recvmsg(sock_fd, &msg, 0);
  if (fds_got != NULL) {
    *fds_got = msg.desc_length;
  }
  return received;
}

const uint8_t srpc_reply_message[] = {
  /* protocol_version (fixed, kNaClSrpcProtocolVersion) */
  2, 0, 0xad, 0xc0,
  /* request_id (sequence number) */
  0, 0, 0, 0, 0, 0, 0, 0,
  /* is_request */
  0,
  /* rpc_number (method number): 0 for service_discovery */
  0, 0, 0, 0,
  /* app_error (return code): 256 (NACL_SRPC_RESULT_OK) */
  0, 1, 0, 0,
  /* number of return values: 1 */
  1, 0, 0, 0,
  /* argument 1 type: NACL_SRPC_ARG_TYPE_CHAR_ARRAY */
  'C',
  /* argument 1 length: 0 (empty method list) */
  0, 0, 0, 0,
};

int keep_plugin_happy() {
  int sock_fd;
  char buf[100];

  sock_fd = imc_accept(3);
  if (sock_fd < 0)
    return -1;

  /* Receive service_discovery SRPC request.  This should always be
     the first message. */
  if (receive_message(sock_fd, buf, sizeof(buf), NULL, 0, NULL) < 0)
    return -1;

  /* Send reply, listing no SRPC methods. */
  if (send_message(sock_fd, (char *) srpc_reply_message,
                   sizeof(srpc_reply_message), NULL, 0) < 0)
    return -1;

  if (close(sock_fd) < 0)
    return -1;

  return 0;
}

#define NACL_SEND_FD 6
#define NACL_RECEIVE_FD 7

int main(int argc, char **argv) {
  char message1[] = "Aye carumba! This is an async message.";
  char message2[] = "Message\0containing\0NULLs";
  char message3[] = "Top-bit-set bytes: \x80 and \xff";
  char buf[1000];
  char *prefix = "Echoed: ";
  int rc;
  int bytes;
  int fds[8];
  int fds_count;

  assert(argc == 1);

  rc = keep_plugin_happy();
  assert(rc == 0);

  /* Test sending messages. */
  rc = send_message(NACL_SEND_FD, message1, strlen(message1), NULL, 0);
  assert(rc == strlen(message1));
  rc = send_message(NACL_SEND_FD, message2, sizeof(message2), NULL, 0);
  assert(rc == sizeof(message2));
  rc = send_message(NACL_SEND_FD, message3, strlen(message3), NULL, 0);
  assert(rc == strlen(message3));

  /* Test receiving a message. */
  bytes = receive_message(NACL_RECEIVE_FD, buf, sizeof(buf), NULL, 0, NULL);
  assert(bytes >= 0);
  /* Echo the message back with a prefix. */
  memmove(buf + strlen(prefix), buf, bytes);
  memcpy(buf, prefix, strlen(prefix));
  bytes += strlen(prefix);
  rc = send_message(NACL_SEND_FD, buf, bytes, NULL, 0);
  assert(rc == bytes);

  /* Test receiving a message with an FD. */
  bytes = receive_message(NACL_RECEIVE_FD, buf, sizeof(buf), fds, 8,
                          &fds_count);
  assert(bytes >= 0);
  bytes = sprintf(buf, "Received %i bytes, %i FDs", bytes, fds_count);
  rc = send_message(NACL_SEND_FD, buf, bytes, fds, fds_count);
  assert(rc == bytes);

  return 0;
}
