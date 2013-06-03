/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This test uses asserts, which are disabled whenever NDEBUG is defined. */
#if defined(NDEBUG)
#undef NDEBUG
#endif

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__native_client__)
#include <pthread.h>
#include <stdint.h>
#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/platform_init.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#endif
#include "native_client/src/shared/srpc/nacl_srpc_message.h"

/*
 * Tests the NaClSrpcMessageChannel component.  This component is a layer
 * over IMC that provides the ability to send and receive messages that are
 * larger than the size limits imposed by IMC.  In particular, IMC imposes
 * two limits:
 *   IMC_USER_BYTES_MAX is the maximum number of bytes that imc_sendmsg
 *                      can possibly send.  The actual value may be lower.
 *   IMC_USER_DESC_MAX is the maximum number of descriptors that imc_sedmsg
 *                     can possibly send.
 *  There remain, furthermore, limitations on the number of IOV entries that
 *  a header can have.
 *
 *  main sends and receives various combinations of bytes and descriptors and
 *  checks the return values.
 */

#if defined(__native_client__)
#define NaClImcMsgIoVec NaClAbiNaClImcMsgIoVec
#endif

/* TODO(sehr): test larger than IMC_IOVEC_MAX / 2 iov entries. */
#define kIovEntryCount        (NACL_ABI_IMC_IOVEC_MAX / 2)
#define kDescCount            (4 * NACL_ABI_IMC_USER_DESC_MAX)

#define ARRAY_SIZE(a)         (sizeof a / sizeof a[0])

#define MAX(a, b)             (((a) > (b)) ? (a) : (b))
#define MIN(a, b)             (((a) < (b)) ? (a) : (b))

/* The largest message to be tested. */
size_t g_max_message_size;
/* The size of a fragment. */
size_t g_fragment_bytes;
/* The size of the neighborhood around k * g_fragment_bytes to consider. */
size_t g_message_delta;

/* An array filled with 0, 1, 2, ... for sending and comparison. */
int32_t* gTestArray;

/* An array used for recieving messages smaller than a fragment. */
int32_t* gShortBuf;
/* The length of the messages smaller than a fragment. */
size_t g_short_message_length;

/* An array used for recieving messages larger than a fragment. */
int32_t* gLongBuf;
/* The length of the messages larger than a fragment. */
size_t g_long_message_length;

/*
 * The size of an IOV chunk for a long message. Long messages will be
 * uniformly chunked.
 */
size_t g_long_message_iov_chunk_size;

/* An invalid descriptor to be transferred back and forth. */
NaClSrpcMessageDesc g_bogus_desc;

void SendShortMessage(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[1];

  /* One iov entry, short enough to pass in one fragment. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = g_short_message_length;
  /* One descriptor. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  descs[0] = g_bogus_desc;
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH != 0);
  /* Check that the entire message was sent. */
  assert(NaClSrpcMessageChannelSend(channel, &header) ==
         (ssize_t) g_short_message_length);
}

void ReceiveShortMessage(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[NACL_ABI_IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gShortBuf;
  iovec[0].length = g_short_message_length;
  /* Accept some descriptors. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive buffer. */
  memset(gShortBuf, 0, g_short_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         (ssize_t) g_short_message_length);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received. */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == 1);
  assert(header.flags == 0);
}

void PeekShortMessage(struct NaClSrpcMessageChannel* channel,
                      int expected_flags) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[NACL_ABI_IMC_USER_DESC_MAX];
  size_t i;

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gShortBuf;
  iovec[0].length = g_short_message_length;
  /* Accept some descriptors. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive buffer. */
  memset(gShortBuf, 0, g_short_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelPeek(channel, &header) ==
         (ssize_t) g_short_message_length);
  /* Check that the data bytes are as expected. */
  for (i = 0; i < header.iov[0].length / sizeof(int32_t); ++i) {
    assert(gShortBuf[i] == (int32_t) i);
  }
  /* Check that one descriptor was received. */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == 1);
  assert(header.flags == expected_flags);
}

void SendLongMessage(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[1];

  /* One iov entry, with enough bytes to require fragmentation. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = g_long_message_length;
  /* One descriptor. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  descs[0] = g_bogus_desc;
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH != 0);
  assert(NaClSrpcMessageChannelSend(channel, &header) ==
         (ssize_t) g_long_message_length);
}

void ReceiveLongMessage(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[NACL_ABI_IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = gLongBuf;
  iovec[0].length = g_long_message_length;
  /* Accept some descriptors. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         (ssize_t) g_long_message_length);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received. */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == 1);
  assert(header.flags == 0);
}

void SendMessageOfLength(struct NaClSrpcMessageChannel* channel,
                         size_t message_bytes) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[1];

  /* One iov entry, with the given number of bytes. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  assert(message_bytes < g_max_message_size);
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = message_bytes;
  /* One descriptor. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  descs[0] = g_bogus_desc;
  assert(NaClSrpcMessageChannelSend(channel, &header) ==
         (ssize_t) message_bytes);
}

void ReceiveMessageOfLength(struct NaClSrpcMessageChannel* channel,
                            size_t message_bytes) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[NACL_ABI_IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = gLongBuf;
  iovec[0].length = message_bytes;
  assert(message_bytes < g_max_message_size);
  /* Accept some descriptors. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         (ssize_t) message_bytes);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received. */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == 1);
  assert(header.flags == 0);
}

void ReceiveShorterMessage(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[1];
  NaClSrpcMessageDesc descs[NACL_ABI_IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = gLongBuf;
  iovec[0].length = g_long_message_length / 2;
  /* Accept some descriptors. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         (ssize_t) g_long_message_length / 2);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received. */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == 1);
  assert(header.flags == NACL_ABI_RECVMSG_DATA_TRUNCATED);
}

void SendLotsOfIovs(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[kIovEntryCount];
  NaClSrpcMessageDesc descs[1];
  size_t i;

  /*
   * kIovEntryCount iov entries, each of g_long_message_iov_chunk_size.
   * The total data payload fills gTestArray.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  for(i = 0; i < kIovEntryCount; ++i) {
    iovec[i].base =
        (void*) ((char*) gTestArray + i * g_long_message_iov_chunk_size);
    iovec[i].length = g_long_message_iov_chunk_size;
  }
  /* And one descriptor. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  descs[0] = g_bogus_desc;
  /* Make sure that the entire message was sent. */
  assert(NaClSrpcMessageChannelSend(channel, &header) ==
         (ssize_t) g_long_message_length);
}

void ReceiveLotsOfIovs(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  struct NaClImcMsgIoVec iovec[kIovEntryCount];
  NaClSrpcMessageDesc descs[NACL_ABI_IMC_USER_DESC_MAX];
  char* buf_chunk;
  char* test_chunk;
  size_t i;

  /*
   * kIovEntryCount iov entries, each of g_long_message_iov_chunk_size.
   * The total data payload fills gLongBuf.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  buf_chunk = (char*) gLongBuf;
  for(i = 0; i < kIovEntryCount; ++i) {
    iovec[i].base = buf_chunk;
    iovec[i].length = g_long_message_iov_chunk_size;
    buf_chunk += iovec[i].length;
  }
  /* Accept some descriptors. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         (ssize_t) g_long_message_length);
  /* Check that the data bytes are as expected. */
  test_chunk = (char*) gTestArray;
  for(i = 0; i < kIovEntryCount; ++i) {
    assert(memcmp(header.iov[i].base, test_chunk, header.iov[i].length) == 0);
    test_chunk += header.iov[i].length;
  }
  /* Check that one descriptor was received. */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == 1);
  assert(header.flags == 0);
}

void SendLotsOfDescs(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  NaClSrpcMessageDesc descs[kDescCount];
  size_t i;

  /* No iov entries. */
  header.iov_length = 0;
  header.iov = 0;
  /* kDescCount descriptors. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = kDescCount;
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  for (i = 0; i < kDescCount; ++i) {
    descs[i] = g_bogus_desc;
  }
  /* Check that no error was returned. */
  assert(NaClSrpcMessageChannelSend(channel, &header) == 0);
}

void ReceiveLotsOfDescs(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  NaClSrpcMessageDesc descs[4 * kDescCount];
  size_t i;

  /* No iov entries. */
  header.iov_length = 0;
  header.iov = 0;
  /* Accept descriptors than we expect to get. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive descriptor array. */
  memset(descs, 0, sizeof descs);
  /* Make sure there were no errors. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == 0);
  /*
   * Check that the right number of descriptors was passed and that they have
   * the correct value.
   */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == kDescCount);
  for (i = 0; i < kDescCount; ++i) {
    assert(descs[i] == g_bogus_desc);
  }
  assert(header.flags == 0);
}

void ReceiveFewerDescs(struct NaClSrpcMessageChannel* channel) {
  NaClSrpcMessageHeader header;
  NaClSrpcMessageDesc descs[kDescCount / 2];
  size_t i;

  /* No iov entries. */
  header.iov_length = 0;
  header.iov = 0;
  /* Accept descriptors than we expect to get. */
  header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH = ARRAY_SIZE(descs);
  header.NACL_SRPC_MESSAGE_HEADER_DESCV = descs;
  /* Clear the receive descriptor array. */
  memset(descs, 0, sizeof descs);
  /* Make sure there were no errors. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == 0);
  /*
   * Check that the right number of descriptors was passed and that they have
   * the correct value.
   */
  assert(header.NACL_SRPC_MESSAGE_HEADER_DESC_LENGTH == ARRAY_SIZE(descs));
  for (i = 0; i < ARRAY_SIZE(descs); ++i) {
    assert(descs[i] == g_bogus_desc);
  }
  assert(header.flags == NACL_ABI_RECVMSG_DESC_TRUNCATED);
}

void InitArrays(size_t large_array_size) {
  /* Round up to a multiple of sizeof(int32_t). */
  size_t long_size =
      (large_array_size + sizeof(int32_t) - 1) & ~(sizeof(int32_t) - 1);
  size_t short_size =
      ((large_array_size / 2) + sizeof(int32_t) - 1) & ~(sizeof(int32_t) - 1);
  size_t i;
  /*
   * An array filled with monotonically increasing values.  Used to check
   * payload sanity.
   */
  gTestArray = (int32_t*) malloc(long_size);
  assert(gTestArray != NULL);
  for (i = 0; i < long_size / sizeof(int32_t); ++i) {
    gTestArray[i] = (int32_t) i;
  }
  /* Large buffer used for receiving messages. */
  gLongBuf = (int32_t*) malloc(long_size);
  assert(gLongBuf != NULL);
  /* Small buffer used for receiving messages. */
  gShortBuf = (int32_t*) malloc(short_size);
  assert(gShortBuf != NULL);
}

NaClSrpcMessageDesc g_bound_sock_pair[2];
/* Forward declarations. */
static NaClSrpcMessageDesc Connect(void);
static NaClSrpcMessageDesc Accept(void);

static void Sender(void) {
  size_t msg_len;
  size_t fragments;
  size_t max_fragment_count;
  struct NaClSrpcMessageChannel* send_channel =
      NaClSrpcMessageChannelNew(Connect());
  assert(send_channel != NULL);
  /*
   * Test values within g_message_delta of n * g_max_fragment_count on
   * either side.  Tests for header sizes and boundary cases.
   * If g_max_message_size == g_message_delta / 2, this will cover all
   * values from 0 to g_max_message_size.
   */
  max_fragment_count = g_max_message_size / g_fragment_bytes;
  for (fragments = 0; fragments <= max_fragment_count; ++fragments) {
    size_t lower =
        MAX((fragments * g_fragment_bytes) - g_message_delta, 0);
    size_t upper =
        MIN((fragments * g_fragment_bytes) + g_message_delta,
            g_max_message_size);
    for (msg_len = lower; msg_len < upper; ++msg_len) {
      SendMessageOfLength(send_channel, msg_len);
    }
  }
  /* Test send/receive of message that fits in one message. */
  SendShortMessage(send_channel);
  /* Test send/receive of message that requires multiple messages. */
  SendLongMessage(send_channel);
  /* Test that fragmentation sets data truncated flag. */
  SendLongMessage(send_channel);
  /* Test send with lots of iov entries that equals one long send. */
  SendLotsOfIovs(send_channel);
  /* Test receive with lots of iov entries that equals one long receive. */
  SendLongMessage(send_channel);
  /* Test fragmentation of large numbers of descriptors. */
  SendLotsOfDescs(send_channel);
  /* Test that fragmentation sets descriptor truncated flag. */
  SendLotsOfDescs(send_channel);
  /* Clean up the channel. */
  NaClSrpcMessageChannelDelete(send_channel);
}

static void Receiver(void* arg) {
  size_t msg_len;
  size_t fragments;
  size_t max_fragment_count;
  struct NaClSrpcMessageChannel* recv_channel =
      NaClSrpcMessageChannelNew(Accept());
  assert(recv_channel != NULL);

#if !defined(__native_client__)
  UNREFERENCED_PARAMETER(arg);
#endif
  /*
   * Test values within g_message_delta of n * g_max_fragment_count on
   * either side.  Tests for header sizes and boundary cases.
   * If g_max_message_size == g_message_delta / 2, this will cover all
   * values from 0 to g_max_message_size.
   */
  max_fragment_count = g_max_message_size / g_fragment_bytes;
  for (fragments = 0; fragments <= max_fragment_count; ++fragments) {
    size_t lower =
        MAX((fragments * g_fragment_bytes) - g_message_delta, 0);
    size_t upper =
        MIN((fragments * g_fragment_bytes) + g_message_delta,
            g_max_message_size);
    for (msg_len = lower; msg_len < upper; ++msg_len) {
      ReceiveMessageOfLength(recv_channel, msg_len);
    }
  }
  /* Test send/receive of message that fits in one message. */
  PeekShortMessage(recv_channel, 0);
  ReceiveShortMessage(recv_channel);
  /* Test send/receive of message that requires multiple messages. */
  PeekShortMessage(recv_channel, NACL_ABI_RECVMSG_DATA_TRUNCATED);
  ReceiveLongMessage(recv_channel);
  /* Test that fragmentation sets data truncated flag. */
  ReceiveShorterMessage(recv_channel);
  /* Test send with lots of iov entries that equals one long send. */
  ReceiveLongMessage(recv_channel);
  /* Test receive with lots of iov entries that equals one long receive. */
  ReceiveLotsOfIovs(recv_channel);
  /* Test fragmentation of large numbers of descriptors. */
  ReceiveLotsOfDescs(recv_channel);
  /* Test that fragmentation sets descriptor truncated flag. */
  ReceiveFewerDescs(recv_channel);
  /* Clean up the channel. */
  NaClSrpcMessageChannelDelete(recv_channel);
}

#if defined(__native_client__)
static int InitBoundSock(void) {
  return (imc_makeboundsock(g_bound_sock_pair) == 0);
}

static int RunTests(void) {
  pthread_t recv_thread;
  void* result;
  void* (*fp)(void* arg) = (void *(*)(void*)) Receiver;
  if (pthread_create(&recv_thread, NULL, fp, NULL) != 0) {
    return 0;
  }
  /* Send a bunch of messages. */
  Sender();
  /* Close down the receive thread. */
  pthread_join(recv_thread, &result);
  return 1;
}

static NaClSrpcMessageDesc Connect(void) {
  NaClSrpcMessageDesc desc = imc_connect(g_bound_sock_pair[1]);
  assert(desc != -1);
  return desc;
}

static NaClSrpcMessageDesc Accept(void) {
  NaClSrpcMessageDesc desc = imc_accept(g_bound_sock_pair[0]);
  assert(desc != -1);
  return desc;
}

#else

static int InitBoundSock(void) {
  return (NaClCommonDescMakeBoundSock(g_bound_sock_pair) == 0);
}

static int RunTests(void) {
  struct NaClThread recv_thread;
  void (WINAPI *fp)(void* arg) = (void (WINAPI *)(void*)) Receiver;
  if (NaClThreadCreateJoinable(&recv_thread, fp, NULL, 1024 * 1024) == 0) {
    return 0;
  }
  /* Send a bunch of messages. */
  Sender();
  /* Close down the receive thread. */
  NaClThreadJoin(&recv_thread);
  return 1;
}

static NaClSrpcMessageDesc Connect(void) {
  NaClSrpcMessageDesc desc;
  int result =
      (*NACL_VTBL(NaClDesc, g_bound_sock_pair[1])->ConnectAddr)(
          g_bound_sock_pair[1], &desc);
  assert(result == 0);
  return desc;
}

static NaClSrpcMessageDesc Accept(void) {
  NaClSrpcMessageDesc desc;
  int result =
      (*NACL_VTBL(NaClDesc, g_bound_sock_pair[0])->AcceptConn)(
          g_bound_sock_pair[0], &desc);
  assert(result == 0);
  return desc;
}
#endif

int main(int argc, char* argv[]) {
  if (argc < 4) {
    fprintf(stderr, "usage: srpc_message max_msg_sz frag_sz delta\n");
    return 1;
  }
  g_max_message_size = atoi(argv[1]);
  g_fragment_bytes = atoi(argv[2]);
  g_message_delta = atoi(argv[3]);

  g_short_message_length = g_fragment_bytes / 2;
  g_long_message_length = 4 * g_fragment_bytes;
  g_long_message_iov_chunk_size = g_long_message_length / kIovEntryCount;

#if defined(__native_client__)
  assert(NaClSrpcModuleInit());
  g_bogus_desc = (NaClSrpcMessageDesc) -1;
#else
  NaClPlatformInit();
  NaClNrdAllModulesInit();
  NaClSrpcModuleInit();
  g_bogus_desc = (NaClSrpcMessageDesc) NaClDescInvalidMake();
#endif
  /* Create the bound socket / socket address pair used to connect. */
  assert(InitBoundSock());
  /* Set up arrays for sending/receiving messages. */
  InitArrays(g_max_message_size + g_message_delta + sizeof(int32_t));
  assert(RunTests());
  NaClSrpcModuleFini();
  return 0;
}
