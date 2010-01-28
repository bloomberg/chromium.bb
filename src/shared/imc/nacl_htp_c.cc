/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NaCl Handle Transfer Protocol
// NOTE: only used for trusted version

#include "native_client/src/shared/imc/nacl_htp_c.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "native_client/src/shared/imc/nacl_htp.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"


int NaClHtpSendDatagram(NaClHtpHandle socket, const NaClHtpHeader* message,
                        int flags) {
  return nacl::SendDatagram(socket,
                            reinterpret_cast<const nacl::HtpHeader*>(message),
                            flags);
}


int NaClHtpReceiveDatagram(NaClHtpHandle socket, NaClHtpHeader* message,
                           int flags) {
  return nacl::ReceiveDatagram(socket,
                               reinterpret_cast<nacl::HtpHeader*>(message),
                               flags);
}


int NaClHtpClose(NaClHtpHandle handle) {
  return nacl::Close(*reinterpret_cast<nacl::HtpHandle*>(&handle));
}
