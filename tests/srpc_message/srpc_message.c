/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"
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

#define kShortMessageBytes    (IMC_USER_BYTES_MAX / 2)
#define kLongMessageBytes     (4 * IMC_USER_BYTES_MAX)
#define kIovEntryCount        (4 * IMC_IOVEC_MAX)
#define kIovChunkSize         (kLongMessageBytes / kIovEntryCount)
#define kDescCount            (4 * IMC_USER_DESC_MAX)

#define ARRAY_SIZE(a)  (sizeof a / sizeof a[0])

int gTestArray[kLongMessageBytes / sizeof(int)];

int gShortBuf[kShortMessageBytes / sizeof(int)];
int gLongBuf[kLongMessageBytes / sizeof(int)];


void SendShortMessage(struct NaClSrpcMessageChannel* channel) {
  static struct NaClImcMsgHdr header;
  static struct NaClImcMsgIoVec iovec[1];
  static int descs[1] = { 1 };

  /* One iov entry, short enough to pass in one fragment. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = kShortMessageBytes;
  /* One descriptor, just for good measure. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Check that the entire message was sent. */
  assert(NaClSrpcMessageChannelSend(channel, &header) == kShortMessageBytes);
}

void ReceiveShortMessage(struct NaClSrpcMessageChannel* channel) {
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
  iovec[0].length = kShortMessageBytes;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gShortBuf, 0, sizeof gShortBuf);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == kShortMessageBytes);
  /* Check that the data bytes are as expected. */
  for (i = 0; i < header.iov[0].length / sizeof(int); ++i) {
    assert(gShortBuf[i] == i);
  }
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void PeekShortMessage(struct NaClSrpcMessageChannel* channel) {
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
  iovec[0].length = kShortMessageBytes;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gShortBuf, 0, sizeof gShortBuf);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelPeek(channel, &header) == kShortMessageBytes);
  /* Check that the data bytes are as expected. */
  for (i = 0; i < header.iov[0].length / sizeof(int); ++i) {
    assert(gShortBuf[i] == i);
  }
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void SendLongMessage(struct NaClSrpcMessageChannel* channel) {
  static struct NaClImcMsgHdr header;
  static struct NaClImcMsgIoVec iovec[1];
  static int descs[1] = { 1 };

  /* One iov entry, with enough bytes to require fragmentation. */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  iovec[0].base = (void*) gTestArray;
  iovec[0].length = kLongMessageBytes;
  /* One descriptor. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  assert(NaClSrpcMessageChannelSend(channel, &header) == kLongMessageBytes);
}

void ReceiveLongMessage(struct NaClSrpcMessageChannel* channel) {
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
  iovec[0].base = gLongBuf;
  iovec[0].length = kLongMessageBytes;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, sizeof gLongBuf);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == kLongMessageBytes);
  /* Check that the data bytes are as expected. */
  for (i = 0; i < header.iov[0].length / sizeof(int); ++i) {
    assert(gLongBuf[i] == i);
  }
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void ReceiveShorterMessage(struct NaClSrpcMessageChannel* channel) {
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
  iovec[0].base = gLongBuf;
  iovec[0].length = kLongMessageBytes / 2;
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, sizeof gLongBuf);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) ==
         kLongMessageBytes / 2);
  /* Check that the data bytes are as expected. */
  for (i = 0; i < header.iov[0].length / sizeof(int); ++i) {
    assert(gLongBuf[i] == i);
  }
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == RECVMSG_DATA_TRUNCATED);
}

void SendLotsOfIovs(struct NaClSrpcMessageChannel* channel) {
  static struct NaClImcMsgHdr header;
  static struct NaClImcMsgIoVec iovec[kIovEntryCount];
  static int descs[1] = { 1 };
  size_t i;

  /*
   * kIovEntryCount iov entries, each of kIovChunkSize.
   * The total data payload fills gTestArray.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  for(i = 0; i < kIovEntryCount; ++i) {
    iovec[i].base = (void*) ((char*) gTestArray + i * kIovChunkSize);
    iovec[i].length = kIovChunkSize;
  }
  /* And one descriptor. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Make sure that the entire message was sent. */
  assert(NaClSrpcMessageChannelSend(channel, &header) == kLongMessageBytes);
}

void ReceiveLotsOfIovs(struct NaClSrpcMessageChannel* channel) {
  static struct NaClImcMsgHdr header;
  static struct NaClImcMsgIoVec iovec[kIovEntryCount];
  static int descs[IMC_USER_DESC_MAX];
  size_t i;
  size_t j;

  /*
   * kIovEntryCount iov entries, each of kIovChunkSize.
   * The total data payload fills gLongBuf.
   */
  header.iov_length = ARRAY_SIZE(iovec);
  header.iov = iovec;
  for(i = 0; i < kIovEntryCount; ++i) {
    iovec[i].base = (void*) ((char*) gLongBuf + i * kIovChunkSize);
    iovec[i].length = kIovChunkSize;
  }
  /* Accept some descriptors. */
  header.desc_length = ARRAY_SIZE(descs);
  header.descv = descs;
  /* Clear the receive buffer. */
  memset(gLongBuf, 0, sizeof gLongBuf);
  /* Check that all of it was received. */
  assert(NaClSrpcMessageChannelReceive(channel, &header) == kLongMessageBytes);
  /* Check that the data bytes are as expected. */
  for(i = 0; i < kIovEntryCount; ++i) {
    for (j = 0; j < header.iov[0].length / sizeof(int); ++j) {
      size_t index = i * kIovChunkSize / sizeof(int) + j;
      assert(gLongBuf[index] == gTestArray[index]);
    }
  }
  /* Check that one descriptor was received and that it had the right value. */
  assert(header.desc_length == 1);
  assert(descs[0] == 1);
  assert(header.flags == 0);
}

void SendLotsOfDescs(struct NaClSrpcMessageChannel* channel) {
  static struct NaClImcMsgHdr header;

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
  static struct NaClImcMsgHdr header;
  static int descs[4 * kDescCount];
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
  assert(header.desc_length == kDescCount);
  for (i = 0; i < kDescCount; ++i) {
    assert(descs[i] == gTestArray[i]);
  }
  assert(header.flags == 0);
}

void ReceiveFewerDescs(struct NaClSrpcMessageChannel* channel) {
  static struct NaClImcMsgHdr header;
  static int descs[kDescCount / 2];
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
  assert(header.desc_length == ARRAY_SIZE(descs));
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

int main(int argc, char* argv[]) {
  int i;
  struct NaClSrpcMessageChannel* channel;
  for (i = 0; i < ARRAY_SIZE(gTestArray); ++i) {
    gTestArray[i] = i;
  }
  assert(NaClSrpcModuleInit());
  channel = NaClSrpcMessageChannelNew(1);
  assert(channel != NULL);
  /* Test send/receive of message that fits in one message. */
  SendShortMessage(channel);
  PeekShortMessage(channel);
  ReceiveShortMessage(channel);
  /* Test send/receive of message that requires multiple messages. */
  SendLongMessage(channel);
  PeekShortMessage(channel);
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
   * a flags.
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
