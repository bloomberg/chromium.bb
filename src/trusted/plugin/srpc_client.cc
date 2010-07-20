/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>

#include <map>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/connected_socket.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace {

// TODO(sehr): again, not reentrant.  See bug 605.
PLUGIN_JMPBUF srpc_env;

void SignalHandler(int value) {
  PLUGIN_PRINTF(("SrpcClient::SignalHandler()\n"));
  PLUGIN_LONGJMP(srpc_env, value);
}

}  // namespace

namespace plugin {

SrpcClient::SrpcClient()
    : browser_interface_(NULL) {
  PLUGIN_PRINTF(("SrpcClient::SrpcClient(%p)\n",
                 static_cast<void*>(this)));
}

bool SrpcClient::Init(BrowserInterface* browser_interface,
                      ConnectedSocket* socket) {
  PLUGIN_PRINTF(("SrpcClient::SrpcClient(%p, %p, %p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(browser_interface),
                 static_cast<void*>(socket)));
  // Open the channel to pass RPC information back and forth
  if (!NaClSrpcClientCtor(&srpc_channel_, socket->wrapper()->desc())) {
    return false;
  }
  browser_interface_ = browser_interface;
  PLUGIN_PRINTF(("SrpcClient::SrpcClient: Ctor worked\n"));
  // Record the method names in a convenient way for later dispatches.
  GetMethods();
  PLUGIN_PRINTF(("SrpcClient::SrpcClient: GetMethods worked\n"));
  return true;
}

SrpcClient::~SrpcClient() {
  PLUGIN_PRINTF(("SrpcClient::~SrpcClient(%p)\n", static_cast<void*>(this)));
  PLUGIN_PRINTF(("SrpcClient::~SrpcClient: destroying the channel\n"));
  // And delete the connection.
  NaClSrpcDtor(&srpc_channel_);
  PLUGIN_PRINTF(("SrpcClient::~SrpcClient: done\n"));
}

void SrpcClient::StartJSObjectProxy(Plugin* plugin) {
  // TODO(sehr): this needs to be revisited when we allow groups of instances
  // in one NaCl module.
  uintptr_t npapi_ident =
      browser_interface_->StringToIdentifier("NP_Initialize");
  if (methods_.find(npapi_ident) != methods_.end()) {
    PLUGIN_PRINTF(("SrpcClient::SrpcClient: Is an NPAPI plugin\n"));
    // Start up NPAPI interaction.
    plugin->StartProxiedExecution(&srpc_channel_);
  }
  // TODO(polina,sehr): this also needs to be revisited for PPAPI proxying.
}

void SrpcClient::GetMethods() {
  PLUGIN_PRINTF(("SrpcClient::GetMethods(%p)\n", static_cast<void*>(this)));
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
    if (!IsValidIdentifierString(name, NULL)) {
      // If name is not an ECMAScript identifier, do not enter it into the
      // methods_ table.
      continue;
    }
    uintptr_t ident = browser_interface_->StringToIdentifier(name);
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

bool SrpcClient::HasMethod(uintptr_t method_id) {
  PLUGIN_PRINTF(("SrpcClient::HasMethod(%p, %s)\n",
                 static_cast<void*>(this),
                 browser_interface_->IdentifierToString(method_id).c_str()));
  return NULL != methods_[method_id];
}

bool SrpcClient::InitParams(uintptr_t method_id, SrpcParams* params) {
  MethodInfo* method_info = methods_[method_id];
  if (method_info) {
    return params->Init(method_info->ins(), method_info->outs());
  }
  return false;
}

bool SrpcClient::Invoke(uintptr_t method_id,
                        SrpcParams* params) {
  // It would be better if we could set the exception on each detailed failure
  // case.  However, there are calls to Invoke from within the plugin itself,
  // and these could leave residual exceptions pending.  This seems to be
  // happening specifically with hard_shutdowns.
  PLUGIN_PRINTF(("SrpcClient::Invoke(%p, %s, %p)\n",
                 static_cast<void*>(this),
                 browser_interface_->IdentifierToString(method_id).c_str(),
                 static_cast<void*>(params)));

  // Ensure Invoke was called with an identifier that had a binding.
  if (NULL == methods_[method_id]) {
    PLUGIN_PRINTF(("SrpcClient::Invoke: ident not in methods_\n"));
    return false;
  }

  // Catch signals from SRPC/IMC/etc.
  ScopedCatchSignals sigcatcher(
      (ScopedCatchSignals::SigHandlerType) SignalHandler);

  PLUGIN_PRINTF(("SrpcClient::Invoke: sending the rpc\n"));
  // Call the method
  NaClSrpcError err = NaClSrpcInvokeV(&srpc_channel_,
                                      methods_[method_id]->index(),
                                      params->ins(),
                                      params->outs());
  PLUGIN_PRINTF(("SrpcClient::Invoke: got response %d\n", err));
  if (NACL_SRPC_RESULT_OK != err) {
    PLUGIN_PRINTF(("SrpcClient::Invoke: returned err %s\n",
                   NaClSrpcErrorString(err)));
    return false;
  }

  PLUGIN_PRINTF(("SrpcClient::Invoke: done\n"));
  return true;
}

}  // namespace plugin
