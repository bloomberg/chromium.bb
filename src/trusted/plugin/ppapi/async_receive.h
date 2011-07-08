// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_ASYNC_RECEIVE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_ASYNC_RECEIVE_H_

#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "native_client/src/third_party/ppapi/cpp/private/var_private.h"

namespace plugin {

// This implements a thread for receiving IMC messages from a NaCl
// process and sending them to a JavaScript callback.  Since
// imc_recvmsg() blocks, this must be done in a thread.  The thread
// will receive zero or more messages until it reaches an end-of-file
// condition on the socket.

struct AsyncNaClToJSThreadArgs {
  // Callback registered with __setAsyncCallback().
  pp::VarPrivate callback;
  // IMC socket to receive messages from.
  nacl::scoped_ptr<nacl::DescWrapper> socket;
};

// This function is passed to NaClThreadCreateJoinable() which means
// that it will be called in the newly-created thread.
void WINAPI AsyncNaClToJSThread(void* argument_to_thread);

}  // namespace plugin

#endif
