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


/*
 * NaCl simple RPC over IMC mechanism.
 * This module is used to build SRPC connections in both trusted (e.g., the
 * browser plugin, and untrusted (e.g., a NaCl module) settings.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#ifdef __native_client__
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#else
#include "native_client/src/include/portability.h"
#endif

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"


/*
 * SRPC headers include a protocol (version) number.
 */

static const int kSrpcProtocolVersion = 0xc0da0002;

/*
 * Buffers differ between trusted code and untrusted primarily by
 * the use of NaClImcTypedMsgHdr (trusted) or NaClImcMsgHdr (untrusted).
 * These two structs differ by whether they contain vectors of NaClDesc*
 * (ndescv, for trusted) or int (descv, untrusted), and correspondingly,
 * their counts, ndesc_length (trusted) and desc_length (untrusted).  To
 * avoid replicating unnecessarily, we define these two macros.
 */
#ifdef __native_client__
#define NACL_SRPC_IMC_HEADER_DESCV(buf)        (buf).header.descv
#define NACL_SRPC_IMC_HEADER_DESC_LENGTH(buf)  (buf).header.desc_length
#define NACL_SRPC_IMC_INVALID_DESC             -1
#else  /* trusted code */
#define NACL_SRPC_IMC_HEADER_DESCV(buf)        (buf).header.ndescv
#define NACL_SRPC_IMC_HEADER_DESC_LENGTH(buf)  (buf).header.ndesc_length
#define NACL_SRPC_IMC_INVALID_DESC             NULL
#endif  /* __native_client__ */

/*
 * IMC wrapper functions.
 */

void __NaClSrpcImcBufferCtor(NaClSrpcImcBuffer* buffer, int is_write_buf) {
  buffer->iovec[0].base = buffer->bytes;
  buffer->iovec[0].length = sizeof(buffer->bytes);
  buffer->header.iov = buffer->iovec;
  buffer->header.iov_length = sizeof(buffer->iovec) / sizeof(buffer->iovec[0]);
  NACL_SRPC_IMC_HEADER_DESCV(*buffer) = buffer->descs;
  if (0 != is_write_buf) {
    NACL_SRPC_IMC_HEADER_DESC_LENGTH(*buffer) = 0;
  } else {
    NACL_SRPC_IMC_HEADER_DESC_LENGTH(*buffer) =
        sizeof(buffer->descs) / sizeof(buffer->descs[0]);
  }
  buffer->header.flags = 0;
  /* Buffers start out empty */
  buffer->next_byte = 0;
  buffer->last_byte = 0;
}

NaClSrpcImcDescType NaClSrpcImcDescTypeFromHandle(NaClHandle handle) {
#ifdef __native_client__
  return handle;
#else
  struct NaClDescImcDesc* desc =
      (struct NaClDescImcDesc*) malloc(sizeof(struct NaClDescImcDesc));
  if (NULL == desc) {
    return NULL;
  }
  if (0 == NaClDescImcDescCtor(desc, handle)) {
    free(desc);
    return NULL;
  }
  return (struct NaClDesc*) desc;
#endif  /* __native_client__ */
}

void __NaClSrpcImcMarkReadBufferEmpty(NaClSrpcChannel* channel) {
  channel->receive_buf.last_byte = 0;
  channel->receive_buf.next_byte = 0;
}

/*
 * __NaClSrpcImcFillBuf fills the read buffer from an IMC channel.
 * It returns 1 if successful, and 0 otherwise.
 */
int __NaClSrpcImcFillbuf(NaClSrpcChannel* channel) {
  int            retval;
  double         start_usec = 0.0;
  double         this_usec;

  channel->receive_buf.iovec[0].base = channel->receive_buf.bytes;
  channel->receive_buf.iovec[0].length = sizeof(channel->receive_buf.bytes);
  if (0 != channel->timing_enabled) {
    start_usec = __NaClSrpcGetUsec();
  }
  NACL_SRPC_IMC_HEADER_DESC_LENGTH(channel->receive_buf) = SRPC_DESC_MAX;
#ifdef __native_client__
  retval = imc_recvmsg(channel->imc_handle, &channel->receive_buf.header, 0);
#else
  retval = NaClImcRecvTypedMessage(channel->imc_handle,
                                   (struct NaClDescEffector*) &channel->eff,
                                   &channel->receive_buf.header,
                                   0);
#endif
  channel->receive_buf.next_desc = 0;
  if (0 != channel->timing_enabled) {
    this_usec = __NaClSrpcGetUsec();
    channel->imc_read_usec += this_usec;
  }
  dprintf((SIDE "READ: received %d bytes\n", retval));
  if (0 <= retval) {
    channel->receive_buf.next_byte = 0;
    channel->receive_buf.last_byte = retval;
  } else {
    dprintf((SIDE "READ: read failed.\n"));
    return 0;
  }
  return 1;
}

/*
 * __NaClSrpcImcRead attempts to read n_elt entities of size elt_size into
 * buffer from channel.  It returns the number of elements read if successful,
 * and -1 otherwise.
 */
int __NaClSrpcImcRead(void* buffer,
                      size_t elt_size,
                      size_t n_elt,
                      NaClSrpcChannel* channel) {
  size_t request_bytes = n_elt * elt_size;
  size_t avail_bytes;
  /*
   * Reads must be satisfiable from exactly one datagram receive for now.
   */
  if (channel->receive_buf.last_byte <= channel->receive_buf.next_byte) {
    if (1 != __NaClSrpcImcFillbuf(channel)) {
      return -1;
    }
  }

  avail_bytes = channel->receive_buf.last_byte -
                channel->receive_buf.next_byte;
  if (avail_bytes >= request_bytes) {
    /* Channel receive buffer contains enough data to fully satisfy request */
    memcpy(buffer,
           (void*)(channel->receive_buf.bytes +
                   channel->receive_buf.next_byte),
           request_bytes);
    channel->receive_buf.next_byte += request_bytes;
    return n_elt;
  } else {
    dprintf((SIDE "READ: insufficient bytes read to satisfy request.\n"));
    return -1;
  }
}

/*
 * __NaClSrpcImcReadRpc attempts to read a message header from the
 * specified channel.  It returns 1 if successful, and 0 otherwise.
 */
int __NaClSrpcImcReadRpc(NaClSrpcChannel* channel,
                         NaClSrpcRpc* rpc) {
  int retval;
  int server_protocol;
  uint8_t tmp_is_req = 0;
  uint64_t tmp_msg_id = 0;
  uint32_t tmp_rpc_num = 0;
  NaClSrpcError tmp_app_err = 0;

  retval = __NaClSrpcImcRead(&server_protocol,
                             sizeof(server_protocol),
                             1,
                             channel);
  if (1 != retval) {
    dprintf((SIDE "READ: protocol read fail\n"));
    return 0;
  }
  if (kSrpcProtocolVersion != server_protocol) {
    dprintf((SIDE "READ: protocol version mismatch\n"));
    return 0;
  }
  dprintf((SIDE "READ: protocol version %x\n", server_protocol));
  retval = __NaClSrpcImcRead(&tmp_msg_id, sizeof(tmp_msg_id), 1, channel);
  if (1 != retval) {
    dprintf((SIDE "READ: request_id read fail\n"));
    return 0;
  }
  rpc->request_id = tmp_msg_id;
  dprintf((SIDE "READ: request_id: %"PRIx64".\n", tmp_msg_id));
  retval = __NaClSrpcImcRead(&tmp_is_req, sizeof(tmp_is_req), 1, channel);
  if (1 != retval) {
    dprintf((SIDE "READ: is_request read fail\n"));
    return 0;
  }
  rpc->is_request = tmp_is_req;
  dprintf((SIDE "READ: is_request: %u.\n", tmp_is_req));
  retval = __NaClSrpcImcRead(&tmp_rpc_num, sizeof(tmp_rpc_num), 1, channel);
  if (1 != retval) {
    dprintf((SIDE "READ: rpc_number read fail\n"));
    return 0;
  }
  rpc->rpc_number = tmp_rpc_num;
  dprintf((SIDE "READ: rpc_number: %"PRIu32".\n", tmp_rpc_num));
  if (0 == rpc->is_request) {
    /* Responses also need to read the app_error member */
    retval = __NaClSrpcImcRead(&tmp_app_err, sizeof(tmp_app_err), 1, channel);
    if (1 != retval) {
      dprintf((SIDE "READ: app_error read fail\n"));
      return 0;
    }
    rpc->app_error = tmp_app_err;
  } else {
    /* Otherwise the app_error is initialized to something innocuous */
    rpc->app_error = NACL_SRPC_RESULT_OK;
  }
  dprintf((SIDE "READ: app_error: %d.\n", rpc->app_error));
  return 1;
}

/*
 * __NaClSrpcImcReadDesc reads a NaCl resource descriptor from the specified
 * channel.  It returns a valid descriptor if successful and -1 (untrusted)
 * or NULL (trusted) otherwise.
 */
NaClSrpcImcDescType __NaClSrpcImcReadDesc(NaClSrpcChannel* channel) {
  uint32_t desc_index = channel->receive_buf.next_desc;

  if (desc_index >= NACL_SRPC_IMC_HEADER_DESC_LENGTH(channel->receive_buf)) {
    return NACL_SRPC_IMC_INVALID_DESC;
  }
  else {
    NaClSrpcImcDescType desc = channel->receive_buf.descs[desc_index];
    channel->receive_buf.next_desc++;
    return desc;
  }
}

/*
 * __NaClSrpcImcWrite attempts to write n_elt elements of size elt_size from
 * buffer to the specified channel.  It returns the number of elements it
 * wrote if successful, and -1 otherwise.
 */
int __NaClSrpcImcWrite(const void* buffer,
                       size_t elt_size,
                       size_t n_elt,
                       NaClSrpcChannel* channel) {
  size_t request_bytes = n_elt * elt_size;
  size_t avail_bytes = sizeof(channel->send_buf.bytes) -
                       channel->send_buf.next_byte;
  /*
   * Writes are not broken into multiple datagram sends for now either.
   */
  if (avail_bytes < request_bytes) {
    dprintf((SIDE "WRITE: insufficient space available to satisfy.\n"));
    return -1;
  } else {
    memcpy((void*)(channel->send_buf.bytes + channel->send_buf.next_byte),
           buffer,
           request_bytes);
    channel->send_buf.next_byte += request_bytes;
    dprintf((SIDE "WRITE: wrote %u bytes.\n", (unsigned) request_bytes));
    return n_elt;
  }
}

/*
 * __NaClSrpcImcWriteRpc writes an RPC on the specified channel.
 */
void __NaClSrpcImcWriteRpc(NaClSrpcChannel* channel,
                           NaClSrpcRpc* rpc) {
  __NaClSrpcImcWrite(&kSrpcProtocolVersion,
                     sizeof(kSrpcProtocolVersion),
                     1,
                     channel);
  __NaClSrpcImcWrite(&rpc->request_id, sizeof(rpc->request_id), 1, channel);
  __NaClSrpcImcWrite(&rpc->is_request, sizeof(rpc->is_request), 1, channel);
  __NaClSrpcImcWrite(&rpc->rpc_number, sizeof(rpc->rpc_number), 1, channel);
  /* Responses also need to send the app_error member */
  if (0 == rpc->is_request) {
    __NaClSrpcImcWrite(&rpc->app_error, sizeof(rpc->app_error), 1, channel);
  }
}

/*
 * __NaClSrpcImcWriteDesc writes a NaCl resource descriptor to the specified
 * channel.  It returns 1 if successful, 0 otherwise.
 */
int __NaClSrpcImcWriteDesc(NaClSrpcChannel* channel,
                           NaClSrpcImcDescType desc) {
  int desc_index = NACL_SRPC_IMC_HEADER_DESC_LENGTH(channel->send_buf);

  if (SRPC_DESC_MAX <= desc_index) {
    return 0;
  } else {
    channel->send_buf.descs[desc_index] = desc;
    NACL_SRPC_IMC_HEADER_DESC_LENGTH(channel->send_buf)++;
    return 1;
  }
}

/*
 * __NaClSrpcImcFlush send the contents of the write buffer in channel
 * over the IMC channel it contains. It returns 1 if successful, or 0
 * otherwise.
 */
int __NaClSrpcImcFlush(NaClSrpcChannel* channel) {
  int            retval;
  double         start_usec = 0.0;
  double         this_usec;
  channel->send_buf.iovec[0].length = channel->send_buf.next_byte;
  if (0 != channel->timing_enabled) {
    start_usec = __NaClSrpcGetUsec();
  }
#ifdef __native_client__
  retval = imc_sendmsg(channel->imc_handle, &channel->send_buf.header, 0);
#else
  retval = NaClImcSendTypedMessage(channel->imc_handle,
                                   (struct NaClDescEffector*) &channel->eff,
                                   &channel->send_buf.header,
                                   0);
#endif
  NACL_SRPC_IMC_HEADER_DESC_LENGTH(channel->send_buf) = 0;
  channel->send_buf.next_desc = 0;
  if (0 != channel->timing_enabled) {
    this_usec = __NaClSrpcGetUsec();
    channel->imc_write_usec += this_usec;
  }
  dprintf((SIDE "FLUSH: retval %d, expected %d, errno %d\n",
           retval, (int) channel->send_buf.iovec[0].length, errno));
  channel->send_buf.next_byte = 0;
  if (retval != channel->send_buf.iovec[0].length) {
    dprintf((SIDE "FLUSH: send error.\n"));
    return 0;
  }
  dprintf((SIDE "FLUSH: complete send.\n"));
  return 1;
}
