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


#include <string.h>

#include <map>
#include <string>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/platform/nacl_log.h"

#include "native_client/src/trusted/plugin/srpc/srpc_client.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"

namespace nacl_srpc {

int SrpcClient::number_alive_counter = 0;

SrpcClient::SrpcClient() :
    portable_plugin_(NULL) {
  dprintf(("SrpcClient::SrpcClient(%p, %d)\n",
           static_cast<void *>(this), ++number_alive_counter));
}

bool SrpcClient::Init(PortablePluginInterface *portable_plugin,
                      ConnectedSocket* socket) {
  dprintf(("SrpcClient::SrpcClient(%p, %p, %p, %d)\n",
           static_cast<void *>(this),
           static_cast<void *>(portable_plugin),
           static_cast<void *>(socket),
           number_alive_counter));
  // Open the channel to pass RPC information back and forth
  if (!NaClSrpcClientCtor(&srpc_channel_, socket->desc())) {
    return false;
  }
  // Set the relevant state
  portable_plugin_ = portable_plugin;
  dprintf(("SrpcClient::SrpcClient: Ctor worked\n"));
  // Record the method names in a convenient way for later dispatches.
  GetMethods();
  dprintf(("SrpcClient::SrpcClient: GetMethods worked\n"));
  return true;
}

SrpcClient::~SrpcClient() {
  dprintf(("SrpcClient::~SrpcClient(%p, %d)\n",
           static_cast<void *>(this), --number_alive_counter));
  if (NULL == portable_plugin_) {
    // Client was never correctly initialized.
    dprintf(("SrpcClient::~SrpcClient: no plugin\n"));
    return;
  }
  dprintf(("SrpcClient::~SrpcClient: destroying the channel\n"));
  // And delete the connection.
  NaClSrpcDtor(&srpc_channel_);
  dprintf(("SrpcClient::~SrpcClient: done\n"));
}

void SrpcClient::GetMethods() {
  dprintf(("SrpcClient::GetMethods(%p)\n", static_cast<void *>(this)));
  // Get the exported methods.
  const struct NaClSrpcDesc *descrs = srpc_channel_.rpc_descr;
  uint32_t count = srpc_channel_.rpc_count;
  // Intern the methods into a mapping from NPIdentifiers to MethodInfo.
  for (uint32_t i = 0; i < count; ++i) {
    const char* name = descrs[i].rpc_name;
    int ident = PortablePluginInterface::GetStrIdentifierCallback(name);
    MethodInfo* method_info = new(std::nothrow) MethodInfo(NULL,
                                                           name,
                                                           descrs[i].in_args,
                                                           descrs[i].out_args,
                                                           i);
    if (NULL == method_info) {
      return;
    }
    // Install in the map only if successfully read.
    methods_[ident] = method_info;

  }
}

NaClSrpcArg* SrpcClient::GetSignatureObject() {
  dprintf(("SrpcClient::GetSignatureObject(%p)\n", static_cast<void *>(this)));
  // Get the exported methods.
  const struct NaClSrpcDesc *descrs = srpc_channel_.rpc_descr;
  uint32_t count = srpc_channel_.rpc_count;
  NaClSrpcArg* ret_array = new(std::nothrow) NaClSrpcArg;
  if ((NULL == ret_array) || !InitSrpcArgArray(ret_array, count)) {
    if (0 != count) {
      return NULL;
    }
  }

  for (uint32_t i = 0; i < count; ++i) {
    const char* name = descrs[i].rpc_name;
    int ident = PortablePluginInterface::GetStrIdentifierCallback(name);
    methods_[ident]->Signature(&ret_array->u.vaval.varr[i]);
  }
  return ret_array;
}

bool SrpcClient::HasMethod(int method_id) {
  dprintf(("SrpcClient::HasMethod(%p, %s)\n",
           static_cast<void *>(this),
           PortablePluginInterface::IdentToString(method_id)));
  if (PortablePluginInterface::kNullNpapiMethodIdent == method_id) {
    return true;
  }
  return NULL != methods_[method_id];
}

bool SrpcClient::InitParams(int method_id,
                            SrpcParams *params) {
  MethodInfo *method_info = methods_[method_id];

  if (method_info) {
    return params->Init(method_info->ins_, method_info->outs_);
  }
  return false;
}

bool SrpcClient::Invoke(int method_id,
                        SrpcParams *params) {
  // It would be better if we could set the exception on each detailed failure
  // case.  However, there are calls to Invoke from within the plugin itself,
  // and these could leave residual exceptions pending.  This seems to be
  // happening specifically with hard_shutdowns.
  dprintf(("SrpcClient::Invoke(%p, %s, %p)\n",
           static_cast<void *>(this),
           PortablePluginInterface::IdentToString(method_id),
           static_cast<void *>(params)));

  // Support to measure the cost of invoking an NPAPI method.
  if (PortablePluginInterface::kNullNpapiMethodIdent == method_id) {
    dprintf(("SrpcClient::Invoke: null method\n"));
    params->Output(0)->tag = NACL_SRPC_ARG_TYPE_INT;
    params->Output(0)->u.ival = 0;
    return true;
  }

  // Ensure Invoke was called with an identifier that had a binding.
  if (NULL == methods_[method_id]) {
    dprintf(("SrpcClient::Invoke: ident not in methods_\n"));
    return false;
  }

  // Catch signals from SRPC/IMC/etc.
  ScopedCatchSignals sigcatcher(
      (ScopedCatchSignals::SigHandlerType) SignalHandler);

  dprintf(("SrpcClient::Invoke: sending the rpc\n"));
  // Call the method
  NaClSrpcError err = NaClSrpcInvokeV(&srpc_channel_,
                                      methods_[method_id]->index_,
                                      params->ins,
                                      params->outs);
  dprintf(("SrpcClient::Invoke: got response %d\n", err));
  if (NACL_SRPC_RESULT_OK != err) {
    dprintf(("SrpcClient::Invoke: returned err %s\n",
            NaClSrpcErrorString(err)));
    return false;
  }

  dprintf(("SrpcClient::Invoke: done\n"));
  return true;
}

void SrpcClient::SignalHandler(int value) {
  dprintf(("SrpcClient::SignalHandler()\n"));
  PLUGIN_LONGJMP(srpc_env, value);
}

PLUGIN_JMPBUF SrpcClient::srpc_env;

}  // namespace nacl_srpc
