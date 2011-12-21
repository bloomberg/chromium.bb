/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <nacl/nacl_srpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nacl_syscalls.h>
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
 *  There, furthermore, are limitations on the number of IOV entries that
 *  a header can have.
 *
 *  This test works by mocking imc_sendmsg and imc_recvmsg to use a queue
 *  of buffers.  Then main sends and receives various combinations of bytes
 *  and descriptors and checks the return values.
 */

#define kIovEntryCount        (4 * IMC_IOVEC_MAX)
#define kDescCount            (4 * IMC_USER_DESC_MAX)

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
int32_t *gTestArray;

/* An array used for recieving messages smaller than a fragment. */
int32_t *gShortBuf;
/* The length of the messages smaller than a fragment. */
size_t g_short_message_length;

/* An array used for recieving messages larger than a fragment. */
int32_t *gLongBuf;
/* The length of the messages larger than a fragment. */
size_t g_long_message_length;
/*
 * The size of an IOV chunk for a long message. Long messages will be
 * uniformly chunked.
 */
size_t g_long_message_iov_chunk_size;


void SendShortMessage(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[1] = { 1 };

  /* One iov entry, short enough to pass in one fragment. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = g_short_message_length;
  /* One descriptor, just for good measure. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Check that the entire message was sent. */
  assert(NaClSrpcMessageChannelSend(channel, &header) ==
         g_short_message_length);
}

void ReceiveShortMessage(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gShortBuf;
  iovec[0].length = g_short_message_length;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gShortBuf, 0, g_short_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         g_short_message_length);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void PeekShortMessage(struct NaClSrpcMessageChannel* channel,
                      int expected_flags) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[IMC_USER_DESC_MAX];
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
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gShortBuf, 0, g_short_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelPeek(channel, &header) ==
         g_short_message_length);
  /* Check that the data bytes are as expected. */
  for (i = 0; i < header.iov[0].length / sizeof(int32_t); ++i) {
    assert(gShortBuf[i] == i);
  }
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == expected_flags);
}

void SendLongMessage(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[1] = { 1 };

  assert(g_long_message_length <= g_max_message_size);
  /* One iov entry, with enough bytes to require fragmentation. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = g_long_message_length;
  /* One descriptor. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  assert(NaClSrpcMessageChannelSend(channel, &header) == g_long_message_length);
}

void ReceiveLongMessage(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = gLongBuf;
  iovec[0].length = g_long_message_length;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         g_long_message_length);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void SendMessageOfLength(struct NaClSrpcMessageChannel* channel,
                         size_t message_bytes) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[1] = { 1 };

  /* One iov entry, with the given number of bytes. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = message_bytes;
  /* One descriptor. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  assert(NaClSrpcMessageChannelSend(channel, &header) == message_bytes);
}

void ReceiveMessageOfLength(struct NaClSrpcMessageChannel* channel,
                            size_t message_bytes) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = gLongBuf;
  iovec[0].length = message_bytes;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == message_bytes);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void ReceiveShorterMessage(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[1];
  int descs[IMC_USER_DESC_MAX];

  /*
   * One iov entry pointing to a buffer large enough to read the expected
   * result.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = gLongBuf;
  iovec[0].length = g_long_message_length / 2;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         g_long_message_length / 2);
  /* Check that the data bytes are as expected. */
  assert(memcmp(header.iov[0].base, gTestArray, header.iov[0].length) == 0);
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == RECVMSG_DATA_TRUNCATED);
}

void SendLotsOfIovs(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[kIovEntryCount];
  int descs[1] = { 1 };
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
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Make sure that the entire message was sent. */
  assert(NaClSrpcMessageChannelSend(channel, &header) == g_long_message_length);
}

void ReceiveLotsOfIovs(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  struct NaClImcMsgIoVec iovec[kIovEntryCount];
  int descs[IMC_USER_DESC_MAX];
  char *buf_chunk;
  char *test_chunk;
  size_t i;

  /*
   * kIovEntryCount iov entries, each of g_long_message_iov_chunk_size.
   * The total data payload fills gLongBuf.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  buf_chunk = (char *) gLongBuf;
  for(i = 0; i < kIovEntryCount; ++i) {
    iovec[i].base = buf_chunk;
    iovec[i].length = g_long_message_iov_chunk_size;
    buf_chunk += iovec[i].length;
  }
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, g_long_message_length);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         g_long_message_length);
  /* Check that the data bytes are as expected. */
  test_chunk = (char *) gTestArray;
  for(i = 0; i < kIovEntryCount; ++i) {
    assert(memcmp(header.iov[i].base, test_chunk, header.iov[i].length) == 0);
    test_chunk += header.iov[i].length;
  }
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void SendLotsOfDescs(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;

  /* No iov entries. */
  header.iov_length = 0;
  header.iov = 0;
  /* kDescCount descriptors. */
  header.desc_length = kDescCount;
  header.descv = gTestArray;
  /* Check that no error was returned. */
  assert(NaClSrpcMessageChannelSend(channel, &header) == 0);
}

void ReceiveLotsOfDescs(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  int descs[4 * kDescCount];
  int i;

  /* No iov entries. */
  header.iov_length = 0;
  header.iov = 0;
  /* Accept descriptors than we expect to get. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive descriptor array. */
  memset(descs, 0, sizeof descs);
  /* Make sure there were no errors. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == 0);
  /*
   * Check that the right number of descriptors was passed and that they have
   * the correct value.
   */
  assert(header.desc_length = kDescCount);
  for (i = 0; i < kDescCount; ++i) {
    assert(descs[i] == gTestArray[i]);
  }
  assert(header.flags == 0);
}

void ReceiveFewerDescs(struct NaClSrpcMessageChannel* channel) {
  struct NaClImcMsgHdr header;
  int descs[kDescCount / 2];
  int i;

  /* No iov entries. */
  header.iov_length = 0;
  header.iov = 0;
  /* Accept descriptors than we expect to get. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive descriptor array. */
  memset(descs, 0, sizeof descs);
  /* Make sure there were no errors. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == 0);
  /*
   * Check that the right number of descriptors was passed and that they have
   * the correct value.
   */
  assert(header.desc_length = ARRAY_SIZE(descs));
  for (i = 0; i < ARRAY_SIZE(descs); ++i) {
    assert(descs[i] == gTestArray[i]);
  }
  assert(header.flags == RECVMSG_DESC_TRUNCATED);
}

struct MessageList {
  char* buf;
  size_t buf_len;
  int* descv;
  size_t desc_len;
  struct MessageList* next;
};
struct MessageList* head = NULL;
struct MessageList** tail = &head;

void InitArrays(size_t large_array_size) {
  /* Round up to a multiple of sizeof(int32_t). */
  size_t long_size =
      (large_array_size + sizeof(int32_t) - 1) & ~(sizeof(int32_t) - 1);
  size_t short_size =
      ((large_array_size / 2) + sizeof(int32_t) - 1) & ~(sizeof(int32_t) - 1);
  int i;
  /*
   * An array filled with monotonically increasing values.  Used to check
   * payload sanity.
   */
  gTestArray = (int32_t *) malloc(long_size);
  assert(gTestArray != NULL);
  for (i = 0; i < long_size / sizeof(int32_t); ++i) {
    gTestArray[i] = i;
  }
  /* Large buffer used for receiving messages. */
  gLongBuf = (int32_t *) malloc(long_size);
  assert(gLongBuf != NULL);
  /* Small buffer used for receiving messages. */
  gShortBuf = (int32_t *) malloc(short_size);
  assert(gShortBuf != NULL);
}

int main(int argc, char* argv[]) {
  size_t msg_len;
  size_t fragments;
  size_t max_fragment_count;
  struct NaClSrpcMessageChannel* channel;

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

  assert(NaClSrpcModuleInit());
  channel = NaClSrpcMessageChannelNew(1);
  assert(channel != NULL);

  /* Setup */
  InitArrays(g_max_message_size + g_message_delta + sizeof(int32_t));
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
      SendMessageOfLength(channel, msg_len);
      ReceiveMessageOfLength(channel, msg_len);
    }
  }
  /* Test send/receive of message that fits in one message. */
  SendShortMessage(channel);
  PeekShortMessage(channel, 0);
  ReceiveShortMessage(channel);
  /* Test send/receive of message that requires multiple messages. */
  SendLongMessage(channel);
  PeekShortMessage(channel, RECVMSG_DATA_TRUNCATED);
  ReceiveLongMessage(channel);
  /* Test that fragmentation sets data truncated flag. */
  SendLongMessage(channel);
  ReceiveShorterMessage(channel);
  /* Test send with lots of iov entries that equals one long send. */
  SendLotsOfIovs(channel);
  ReceiveLongMessage(channel);
  /* Test receive with lots of iov entries that equals one long receive. */
  SendLongMessage(channel);
  ReceiveLotsOfIovs(channel);
  /* Test fragmentation of large numbers of descriptors. */
  SendLotsOfDescs(channel);
  ReceiveLotsOfDescs(channel);
  /* Test that fragmentation sets descriptor truncated flag. */
  SendLotsOfDescs(channel);
  ReceiveFewerDescs(channel);
  /* Test that the queue is empty. */
  assert(head == NULL);
  assert(tail == &head);
  NaClSrpcMessageChannelDelete(channel);
  NaClSrpcModuleFini();
  return 0;
}

/*
 * Mocked versions of imc_sendmsg and imc_recvmsg using a queue of MessageList.
 */
int imc_sendmsg(int desc, struct NaClImcMsgHdr const* header, int flags) {
  size_t i;
  size_t offset = 0;
  struct MessageList* elt;
  char* buf;
  int* descv;

  /* Create a new MessageList entry. */
  for (i = 0; i < header->iov_length; ++i) {
    offset += header->iov[i].length;
  }
  elt = (struct MessageList*) malloc(sizeof *elt);
  assert(elt != NULL);
  buf = (char*) malloc(offset);
  assert(buf != NULL);
  descv = (int*) malloc(header->desc_length * sizeof *descv);
  assert(descv != NULL);
  elt->buf = buf;
  elt->buf_len = offset;
  elt->descv = descv;
  elt->desc_len = header->desc_length;
  elt->next = NULL;
  /* Copy the data bytes into elt->buf. */
  offset = 0;
  for (i = 0; i < header->iov_length; ++i) {
    memcpy(buf + offset, header->iov[i].base, header->iov[i].length);
    offset += header->iov[i].length;
  }
  /* Copy the descriptors into elt->descv. */
  memcpy(descv, header->descv, header->desc_length * sizeof *descv);
  /* Insert the element into the list. */
  *tail = elt;
  tail = &elt->next;
  /* Return the number of bytes written. */
  return offset;
}

int imc_recvmsg(int desc, struct NaClImcMsgHdr* header, int flags) {
  size_t i;
  size_t offset = 0;
  struct MessageList* elt;

  header->flags = 0;
  /* Remove the first element of the list. */
  elt = head;
  assert(elt != NULL);
  head = elt->next;
  if (head == NULL) {
    tail = &head;
  }
  /* Parse elt->buf into the chunks described by header->iov. */
  for (i = 0; i < header->iov_length; ++i) {
    if (elt->buf_len - offset < header->iov[i].length) {
      /* This iov consumes the last byte. */
      memcpy(header->iov[i].base, elt->buf + offset, elt->buf_len - offset);
      offset = elt->buf_len;
      break;
    } else {
      /* This iov entry is filled. */
      memcpy(header->iov[i].base, elt->buf + offset, header->iov[i].length);
      offset += header->iov[i].length;
    }
  }
  /*
   * If the caller requested fewer bytes than the message provided, set
   * flags.
   */
  if (offset < elt->buf_len) {
    header->flags = RECVMSG_DATA_TRUNCATED;
  }
  /*
   * Copy the descriptors and set a flag if fewer were requested than the
   * message contained.
   */
  if (elt->desc_len > header->desc_length) {
    memcpy(header->descv,
           elt->descv,
           header->desc_length * sizeof *header->descv);
    header->flags = RECVMSG_DESC_TRUNCATED;
  } else {
    memcpy(header->descv,
           elt->descv,
           elt->desc_len * sizeof *header->descv);
    header->desc_length = elt->desc_len;
  }
  /* Clean up the memory and return the number of data bytes read. */
  free(elt->buf);
  free(elt->descv);
  free(elt);
  return offset;
}
