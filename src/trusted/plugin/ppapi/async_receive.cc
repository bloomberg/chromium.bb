// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/ppapi/async_receive.h"

#include <string>

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/string_encoding.h"
#include "native_client/src/third_party/ppapi/cpp/private/var_private.h"

namespace plugin {

namespace {

struct ReceiveCallbackArgs {
  pp::VarPrivate* callback;
  nacl::scoped_ptr_malloc<char> data;
  size_t size;
};

// Called via PPB_Core's CallOnMainThread().
void DoReceiveCallback(void* user_data, int32_t result) {
  UNREFERENCED_PARAMETER(result);
  ReceiveCallbackArgs* call = reinterpret_cast<ReceiveCallbackArgs*>(user_data);

  call->callback->Call(pp::Var(), pp::Var(std::string(call->data.get(),
                                                      call->size)));
  delete call;
  // We do not Release() the pp::Var callback function here because we
  // could not AddRef() it while in the thread.
}

// Called via PPB_Core's CallOnMainThread().
// This deletes a pp::Var (i.e. it Release()s a PPB_Var) so needs to
// be done on the main thread.
void DoReleaseThreadState(void* user_data, int32_t result) {
  UNREFERENCED_PARAMETER(result);
  AsyncNaClToJSThreadArgs* args =
      reinterpret_cast<AsyncNaClToJSThreadArgs*>(user_data);

  delete args;
}

}  // namespace

void WINAPI AsyncNaClToJSThread(void* argument_to_thread) {
  AsyncNaClToJSThreadArgs* thread_args =
      reinterpret_cast<AsyncNaClToJSThreadArgs*>(argument_to_thread);
  pp::Core* core = pp::Module::Get()->core();

  const size_t kBufferSize = NACL_ABI_IMC_USER_BYTES_MAX;
  nacl::scoped_array<char> buffer(new(std::nothrow) char[kBufferSize]);
  if (buffer != NULL) {
    while (1) {
      nacl::DescWrapper::MsgIoVec iovec;
      nacl::DescWrapper::MsgHeader header;
      iovec.base = buffer.get();
      iovec.length = kBufferSize;
      header.iov = &iovec;
      header.iov_length = 1;
      // TODO(mseaborn): Receive FDs too and pass them to the callback.
      header.ndescv = NULL;
      header.ndescv_length = 0;
      header.flags = 0;
      ssize_t got_bytes = thread_args->socket->RecvMsg(&header, 0);
      if (got_bytes < 0) {
        break;
      }
      ReceiveCallbackArgs* callback_args =
          new(std::nothrow) ReceiveCallbackArgs;
      if (callback_args == NULL) {
        break;
      }
      char* data;
      if (!ByteStringAsUTF8(buffer.get(), got_bytes,
                            &data, &callback_args->size)) {
        delete callback_args;
        break;
      }
      callback_args->data.reset(data);
      // We save a pointer to a pp::VarPrivate here rather than copying the
      // pp::VarPrivate because it is not safe to do AddRef() in this thread.
      // We assume that CallOnMainThread() schedules functions to be
      // called in order, so that the callback function is not freed
      // until later.
      callback_args->callback = &thread_args->callback;
      core->CallOnMainThread(
          0, pp::CompletionCallback(DoReceiveCallback, callback_args), 0);
    }
  }

  core->CallOnMainThread(
      0, pp::CompletionCallback(DoReleaseThreadState, thread_args), 0);
}

}  // namespace plugin
