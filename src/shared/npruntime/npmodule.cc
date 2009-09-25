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


#include "native_client/src/shared/npruntime/npmodule.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"

namespace nacl {

// Class static variable declarations.
bool NPModule::IsWebKit = false;

NPModule::NPModule()
    : proxy_(NULL),
      window_(NULL) {
}

NPModule::~NPModule() {
  // The corresponding stub is released by the Navigator.
  if (proxy_) {
    proxy_->Detach();
    NPN_ReleaseObject(proxy_);
  }
}

NACL_SRPC_METHOD_ARRAY(NPModule::srpc_methods) = {
  { "NPN_GetValue:ii:iC", GetValue },
  { "NPN_SetStatus:is:", SetStatus },
  { "NPN_InvalidateRect:iC:", InvalidateRect },
  { "NPN_ForceRedraw:i:", ForceRedraw },
  { "NPN_CreateArray::iC", OpenURL },
  { "NPN_OpenURL:is:i", OpenURL },
  { "NPN_GetIntIdentifier:i:i", GetIntIdentifier },
  { "NPN_GetStringIdentifier:s:i", GetStringIdentifier },
};

// inputs:
// (int) npp
// (int) variable
// outputs:
// (int) error_code
// (char[]) result
NaClSrpcError NPModule::GetValue(NaClSrpcChannel* channel,
                                 NaClSrpcArg** inputs,
                                 NaClSrpcArg** outputs) {
  NPP npp = reinterpret_cast<NPP>(inputs[0]->u.ival);
  NPNVariable variable = static_cast<NPNVariable>(inputs[1]->u.ival);
  NPObject* object;
  outputs[0]->u.ival = NPN_GetValue(npp, variable, &object);
  RpcArg ret1(npp, outputs[1]);
  ret1.PutObject(object);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// (char*) string
// outputs:
// (none)
NaClSrpcError NPModule::SetStatus(NaClSrpcChannel* channel,
                                  NaClSrpcArg** inputs,
                                  NaClSrpcArg** outputs) {
  NPP npp = reinterpret_cast<NPP>(inputs[0]->u.ival);
  const char* status = inputs[1]->u.sval;
  if (status) {
    NPN_Status(npp, status);
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// (char[]) rect
// outputs:
// (none)
NaClSrpcError NPModule::InvalidateRect(NaClSrpcChannel* channel,
                                       NaClSrpcArg** inputs,
                                       NaClSrpcArg** outputs) {
  NPP npp = reinterpret_cast<NPP>(inputs[0]->u.ival);
  NPModule* module = reinterpret_cast<NPModule*>(npp->pdata);
  RpcArg arg1(npp, inputs[1]);
  const NPRect* nprect = arg1.GetRect();

  if (module->window_ && module->window_->window && nprect) {
    NPN_InvalidateRect(npp, const_cast<NPRect*>(nprect));
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// outputs:
// (none)
NaClSrpcError NPModule::ForceRedraw(NaClSrpcChannel* channel,
                                    NaClSrpcArg** inputs,
                                    NaClSrpcArg** outputs) {
  NPP npp = reinterpret_cast<NPP>(inputs[0]->u.ival);
  NPModule* module = reinterpret_cast<NPModule*>(npp->pdata);

  if (module->window_ && module->window_->window) {
    NPN_ForceRedraw(npp);
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (none)
// outputs:
// (bool) success
// (char[]) capability
NaClSrpcError NPModule::CreateArray(NaClSrpcChannel* channel,
                                    NaClSrpcArg** inputs,
                                    NaClSrpcArg** outputs) {
  NPP npp = reinterpret_cast<NPP>(inputs[0]->u.ival);
  NPObject* window;
  NPN_GetValue(npp, NPNVWindowNPObject, &window);
  NPString script;
  script.utf8characters = "new Array();";
  script.utf8length = strlen(script.utf8characters);
  NPVariant result;
  int success = NPN_Evaluate(npp, window, &script, &result) &&
                NPVARIANT_IS_OBJECT(result);
  if (success) {
    RpcArg ret0(npp, outputs[1]);
    ret0.PutObject(NPVARIANT_TO_OBJECT(result));
  }
  NPN_ReleaseObject(window);
  outputs[1]->u.ival = success;
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// (char*) url
// outputs:
// (int) error_code
NaClSrpcError NPModule::OpenURL(NaClSrpcChannel* channel,
                                NaClSrpcArg** inputs,
                                NaClSrpcArg** outputs) {
  NPP npp = reinterpret_cast<NPP>(inputs[0]->u.ival);
  const char* url = inputs[1]->u.sval;
  NPError nperr;
  if (url) {
    nperr = NPN_GetURLNotify(npp, url, NULL, NULL);
  } else {
    nperr = NPERR_GENERIC_ERROR;
  }
  if (nperr == NPERR_NO_ERROR) {
    // NPP_NewStream, NPP_DestroyStream, and NPP_URLNotify will be invoked
    // later.
  }
  outputs[0]->u.ival = static_cast<int>(nperr);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) value
// outputs:
// (int) NPIdentifier
NaClSrpcError NPModule::GetIntIdentifier(NaClSrpcChannel* channel,
                                         NaClSrpcArg** inputs,
                                         NaClSrpcArg** outputs) {
  int32_t value = static_cast<int32_t>(inputs[0]->u.ival);
  NPIdentifier identifier;
  if (IsWebKit) {
    // Webkit needs to look up integer IDs as strings.
    char index[11];
    SNPRINTF(index, sizeof(index), "%u", static_cast<unsigned>(value));
    identifier = NPN_GetStringIdentifier(index);
  } else {
    identifier = NPN_GetIntIdentifier(value);
  }
  outputs[0]->u.ival = reinterpret_cast<int>(identifier);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char*) name
// outputs:
// (int) NPIdentifier
NaClSrpcError NPModule::GetStringIdentifier(NaClSrpcChannel* channel,
                                            NaClSrpcArg** inputs,
                                            NaClSrpcArg** outputs) {
  char* name = inputs[0]->u.sval;
  NPIdentifier identifier = NPN_GetStringIdentifier(name);
  outputs[0]->u.ival = reinterpret_cast<int>(identifier);
  return NACL_SRPC_RESULT_OK;
}

NPError NPModule::Initialize() {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeByName(channel(),
                                "NPP_Initialize",
                                GETPID(),
                                static_cast<int>(sizeof(NPVariant)));
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}

static bool SerializeArgArray(int argc,
                              char* array[],
                              char* serial_array,
                              size_t serial_size) {
  size_t used = 0;

  for (int i = 0; i < argc; ++i) {
    size_t len = strlen(array[i]) + 1;
    if (serial_size < used + len) {
      // Length of the serialized array was exceeded.
      return false;
    }
    strncpy(serial_array + used, array[i], len + 1);
    used += len + 1;
  }
  return true;
}

NPError NPModule::New(char* mimetype,
                      NPP npp,
                      int argc,
                      char* argn[],
                      char* argv[]) {
  NPError nperr;
  char argn_serial[kMaxArgc * kMaxArgLength];
  char argv_serial[kMaxArgc * kMaxArgLength];

  if (!SerializeArgArray(argc, argn, argn_serial, sizeof(argn_serial)) ||
      !SerializeArgArray(argc, argn, argn_serial, sizeof(argn_serial))) {
    return NPERR_GENERIC_ERROR;
  }
  NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                              "NPP_New",
                                              mimetype,
                                              npp,
                                              argc,
                                              argn_serial,
                                              argv_serial,
                                              &nperr);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return nperr;
}

//
// NPInstance methods
//

NPError NPModule::Destroy(NPP npp, NPSavedData** save) {
  NPError nperr;
  NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                              "NPP_Destroy",
                                              npp,
                                              &nperr);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return nperr;
}

NPError NPModule::SetWindow(NPP npp, NPWindow* window) {
  // TODO(sehr): RPC to module.
  return NPERR_NO_ERROR;
}

NPError NPModule::GetValue(NPP npp, NPPVariable variable, void *value) {
  // TODO(sehr): RPC to module for most.
  switch (variable) {
  case NPPVpluginNameString:
    *static_cast<const char**>(value) = "NativeClient NPAPI bridge plug-in";
    break;
  case NPPVpluginDescriptionString:
    *static_cast<const char**>(value) =
        "A plug-in for NPAPI based NativeClient modules.";
    break;
  case NPPVpluginScriptableNPObject:
    *reinterpret_cast<NPObject**>(value) = GetScriptableInstance(npp);
    if (!*reinterpret_cast<NPObject**>(value)) {
      return NPERR_GENERIC_ERROR;
    }
    break;
  default:
    return NPERR_INVALID_PARAM;
  }
  return NPERR_NO_ERROR;
}

int16_t NPModule::HandleEvent(NPP npp, void* event) {
  // TODO(sehr): Need to make an RPC to pass the event.
  return 0;
}

NPObject* NPModule::GetScriptableInstance(NPP npp) {
  if (NULL == proxy_) {
    // TODO(sehr): Need to make an RPC to the module to get the proxy.
  }
  if (NULL != proxy_) {
    NPN_RetainObject(proxy_);
  }
  return proxy_;
}

NPError NPModule::NewStream(NPP npp,
                            NPMIMEType type,
                            NPStream* stream,
                            NPBool seekable,
                            uint16_t* stype) {
  *stype = NP_ASFILEONLY;
  return NPERR_NO_ERROR;
}

void NPModule::StreamAsFile(NPP npp, NPStream* stream, const char* filename) {
  // TODO(sehr): Implement using UrlAsNaClDesc.
}

NPError NPModule::DestroyStream(NPP npp, NPStream *stream, NPError reason) {
  return NPERR_NO_ERROR;
}

void NPModule::URLNotify(NPP npp,
                         const char* url,
                         NPReason reason,
                         void* notify_data) {
  // TODO(sehr): need a call when reason is failure.
  if (NPRES_DONE != reason) {
    return;
  }
  // TODO(sehr): Need to set the descriptor appropriately and call.
  // NaClSrpcInvokeByName(channel(), "NPP_URLNotify", desc, reason);
}

}  // namespace nacl
