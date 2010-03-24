/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>

#include <map>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/npruntime/npmodule.h"

#include "native_client/src/trusted/plugin/srpc/srpc_client.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"

namespace nacl_srpc {

int SrpcClient::number_alive_counter = 0;

SrpcClient::SrpcClient(bool can_use_proxied_npapi)
    : portable_plugin_(NULL),
      can_use_proxied_npapi_(can_use_proxied_npapi) {
  dprintf(("SrpcClient::SrpcClient(%p, %d, %d)\n",
           static_cast<void *>(this),
           can_use_proxied_npapi,
           ++number_alive_counter));
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
  if (can_use_proxied_npapi_) {
    // TODO(sehr): this needs to be revisited when we allow groups of instances
    // in one NaCl module.
    uintptr_t npapi_ident =
        PortablePluginInterface::GetStrIdentifierCallback("NP_Initialize");
    if (methods_.find(npapi_ident) != methods_.end()) {
      dprintf(("SrpcClient::SrpcClient: Is an NPAPI plugin\n"));
      // Start up NPAPI interaction.
      nacl::NPModule* npmodule =
          new(std::nothrow) nacl::NPModule(&srpc_channel_);
      if (NULL != npmodule) {
        portable_plugin->set_module(npmodule);
      }
    }
  }
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
  if (NULL == srpc_channel_.client) {
    return;
  }
  uint32_t method_count = NaClSrpcServiceMethodCount(srpc_channel_.client);
  // Intern the methods into a mapping from NPIdentifiers to MethodInfo.
  for (uint32_t i = 0; i < method_count; ++i) {
    int retval;
    const char* name;
    const char* input_types;
    const char* output_types;

    retval = NaClSrpcServiceMethodNameAndTypes(srpc_channel_.client,
                                               i,
                                               &name,
                                               &input_types,
                                               &output_types);
    if (!nacl::NPModule::IsValidIdentifierString(name, NULL)) {
      // If name is not an ECMAScript identifier, do not enter it into the
      // methods_ table.
      continue;
    }
    uintptr_t ident = PortablePluginInterface::GetStrIdentifierCallback(name);
    MethodInfo* method_info = new(std::nothrow) MethodInfo(NULL,
                                                           name,
                                                           input_types,
                                                           output_types,
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
  if (NULL == srpc_channel_.client) {
    return NULL;
  }
  uint32_t method_count = NaClSrpcServiceMethodCount(srpc_channel_.client);
  NaClSrpcArg* ret_array = reinterpret_cast<NaClSrpcArg*>(
      calloc(1, sizeof(*ret_array)));
  if ((NULL == ret_array) || !InitSrpcArgArray(ret_array, method_count)) {
    if (0 != method_count) {
      return NULL;
    }
  }

  for (uint32_t i = 0; i < method_count; ++i) {
    int retval;
    const char* name;
    const char* input_types;
    const char* output_types;

    retval = NaClSrpcServiceMethodNameAndTypes(srpc_channel_.client,
                                               i,
                                               &name,
                                               &input_types,
                                               &output_types);
    uintptr_t ident = PortablePluginInterface::GetStrIdentifierCallback(name);
    methods_[ident]->Signature(&ret_array->u.vaval.varr[i]);
  }
  return ret_array;
}

bool SrpcClient::HasMethod(uintptr_t method_id) {
  dprintf(("SrpcClient::HasMethod(%p, %s)\n",
           static_cast<void *>(this),
           PortablePluginInterface::IdentToString(method_id)));
  return NULL != methods_[method_id];
}

bool SrpcClient::InitParams(uintptr_t method_id,
                            SrpcParams *params) {
  MethodInfo *method_info = methods_[method_id];

  if (method_info) {
    return params->Init(method_info->ins_, method_info->outs_);
  }
  return false;
}

bool SrpcClient::Invoke(uintptr_t method_id,
                        SrpcParams *params) {
  // It would be better if we could set the exception on each detailed failure
  // case.  However, there are calls to Invoke from within the plugin itself,
  // and these could leave residual exceptions pending.  This seems to be
  // happening specifically with hard_shutdowns.
  dprintf(("SrpcClient::Invoke(%p, %s, %p)\n",
           static_cast<void *>(this),
           PortablePluginInterface::IdentToString(method_id),
           static_cast<void *>(params)));

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
