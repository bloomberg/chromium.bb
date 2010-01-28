/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NaCl Handle Transfer Protocol
//
// The NaCl sel_ldr uses the NaCl Handle Transfer Protocol to send/receive
// messages between NaCl modules. This module provides the utility functions
// to communicate with a NaCl module from the trusted code.

#ifndef NATIVE_CLIENT_SRC_SHARED_IMC_NACL_HTP_C_H_
#define NATIVE_CLIENT_SRC_SHARED_IMC_NACL_HTP_C_H_

#ifdef __native_client__
/**
 * TODO(ilewis): uncomment this line once we make it safe to include
 * nacl_imc_c.h from untrusted code.
 *
 * #include <nacl/nacl_imc_c.h>
 */

#else
#include "native_client/src/shared/imc/nacl_imc_c.h"
#endif  /* __native_client__ */


#if !defined(__native_client__) && !defined(NO_SEL_LDR)
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#if defined(__native_client__) || defined(NO_SEL_LDR)

typedef NaClHandle        NaClHtpHandle;
typedef NaClMessageHeader NaClHtpHeader;

#else  // defined(__native_client__) || defined(NO_SEL_LDR)

// NaCl resource descriptor type compatible with sel_ldr.
typedef struct NaClDesc* NaClHtpHandle;

// Message header used by SendDatagram() and ReceiveDatagram().
typedef struct NaClHtpHeader {
  NaClIOVec*     iov;            // scatter/gather array
  size_t         iov_length;     // number of elements in iov
  NaClHtpHandle* handles;        // array of handles to be transferred
  size_t         handle_count;   // number of handles in handles
  int            flags;
} NaClHtpHeader;

// Sends a message on a socket. Except for the fact that NaClHtpHeader is used,
// the runtime behavior of this function is the same as NaClSendDatagram() for
// NaClMessageHeader.
// On success, NaClHtpSendDatagram() returns the number of bytes sent excluding
// the internal NaCl Handle Transfer Protocol header bytes.
int NaClHtpSendDatagram(NaClHtpHandle socket, const NaClHtpHeader* message,
                        int flags);

// Receives a message from a socket.  Except for the fact that NaClHtpHeader is
// used, the runtime behavior of this function is the same as
// NaClReceiveDatagram() for NaClMessageHeader.
// On success, NaClHtpReceiveDatagram() returns the number of bytes received
// excluding the internal NaCl Handle Transfer Protocol header bytes.
int NaClHtpReceiveDatagram(NaClHtpHandle socket, NaClHtpHeader* message,
                           int flags);

// Closes a sel_ldr compatible NaCl descriptor.
int NaClHtpClose(NaClHtpHandle handle);

#endif  // defined(__native_client__) || defined(NO_SEL_LDR)

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // NATIVE_CLIENT_SRC_SHARED_IMC_NACL_HTP_C_H_
