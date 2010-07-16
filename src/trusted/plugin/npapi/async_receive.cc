/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/npapi/async_receive.h"

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/npapi/scriptable_impl_npapi.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/string_encoding.h"

namespace {

struct ReceiveCallbackArgs {
  NPP plugin;
  NPObject* callback;
  char* data;
  size_t size;
};

void DoReceiveCallback(void* handle) {
  ReceiveCallbackArgs* call = reinterpret_cast<ReceiveCallbackArgs*>(handle);

  NPVariant args[1];
  NPVariant result;
  STRINGN_TO_NPVARIANT(call->data, call->size, args[0]);
  NPN_InvokeDefault(call->plugin, call->callback, args, 1, &result);
  // There is not much we can do with the result of the function call.
  NPN_ReleaseVariantValue(&result);
  free(call->data);
  delete call;
  // We do not do NPN_ReleaseObject() on the callback here because we
  // could not do NPN_RetainObject() earlier.
}

void DoReleaseObject(void* npobj) {
  NPN_ReleaseObject(reinterpret_cast<NPObject*>(npobj));
}

}  // namespace

namespace plugin {

void WINAPI AsyncReceiveThread(void* handle) {
  ReceiveThreadArgs* args = reinterpret_cast<ReceiveThreadArgs*>(handle);

  int buffer_size = NACL_ABI_IMC_USER_BYTES_MAX;
  char* buffer = reinterpret_cast<char*>(malloc(buffer_size));
  if (buffer == NULL) {
    delete args;
    return;
  }

  while (1) {
    nacl::DescWrapper::MsgIoVec iovec;
    nacl::DescWrapper::MsgHeader header;
    iovec.base = buffer;
    iovec.length = buffer_size;
    header.iov = &iovec;
    header.iov_length = 1;
    // TODO(mseaborn): Receive FDs too and pass them to the callback.
    header.ndescv = NULL;
    header.ndescv_length = 0;
    header.flags = 0;
    ssize_t got_bytes = args->socket->RecvMsg(&header, 0);
    if (got_bytes < 0) {
      break;
    }
    ReceiveCallbackArgs* call = new(std::nothrow) ReceiveCallbackArgs;
    if (call == NULL) {
      break;
    }
    if (!ByteStringAsUTF8(buffer, got_bytes, &call->data, &call->size)) {
      delete call;
      break;
    }
    // We cannot call NPN_RetainObject() here because it is not safe
    // to call it from this thread.  We assume that
    // NPN_PluginThreadAsyncCall() schedules functions to be called in
    // order, so that the function is not freed until later.
    call->plugin = args->plugin;
    call->callback = args->callback;
    NPN_PluginThreadAsyncCall(args->plugin, DoReceiveCallback, call);
  }

  args->socket->Delete();
  NPN_PluginThreadAsyncCall(args->plugin, DoReleaseObject, args->callback);
  free(buffer);
  delete args;
}

}  // namespace plugin
