// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around NaclFile functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "ppapi/c/pp_completion_callback.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::AreDevInterfacesEnabled;
using ppapi_proxy::DebugPrintf;
using ppapi_proxy::LookupBrowserPppForInstance;
using ppapi_proxy::MakeRemoteCompletionCallback;

void NaClFileRpcServer::StreamAsFile(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    char* url,
    int32_t callback_id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL)
    return;

  plugin::Plugin* plugin = LookupBrowserPppForInstance(instance)->plugin();
  // Will always call the callback on success or failure.
  bool success = plugin->StreamAsFile(url, remote_callback);
  DebugPrintf("NaClFile::StreamAsFile: success=%d\n", success);

  rpc->result = NACL_SRPC_RESULT_OK;
}

// GetFileDesc() will only provide file descriptors if the PPAPI Dev interface
// is enabled. By default, it is _not_ enabled. See AreDevInterfacesEnabled()
// for information on how to enable.
void NaClFileRpcServer::GetFileDesc(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    char* url,
    // outputs
    NaClSrpcImcDescType* file_desc) {
  nacl::DescWrapperFactory factory;
  nacl::scoped_ptr<nacl::DescWrapper> desc_wrapper(factory.MakeInvalid());
  // NOTE: |runner| must be created after the desc_wrapper, so it's destroyed
  // first. This way ~NaClSrpcClosureRunner gets to transmit the underlying
  // NaClDesc before it is unref'ed and freed by ~DescWrapper.
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (AreDevInterfacesEnabled()) {
    plugin::Plugin* plugin = LookupBrowserPppForInstance(instance)->plugin();
    int32_t posix_file_desc = plugin->GetPOSIXFileDesc(url);
    DebugPrintf("NaClFile::GetFileDesc: posix_file_desc=%"NACL_PRId32"\n",
                posix_file_desc);

    desc_wrapper.reset(factory.MakeFileDesc(posix_file_desc,
        NACL_ABI_O_RDONLY));
    if (desc_wrapper.get() == NULL)
      return;
  } else {
    DebugPrintf("NaClFile::GetFileDesc is disabled (and experimental.)\n");
    // Return invalid descriptor (from factory.MakeInvalid() above.)
  }
  *file_desc = desc_wrapper->desc();
  rpc->result = NACL_SRPC_RESULT_OK;
}
