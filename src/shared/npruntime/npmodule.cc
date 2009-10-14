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

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"

namespace nacl {

static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf(stderr, "@@@ MODULE ");

  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fflush(stderr);
}

// Class static variable declarations.
bool NPModule::IsWebKit = false;

NPModule::NPModule(NaClSrpcChannel* channel)
    : proxy_(NULL),
      window_(NULL) {
  // Remember the channel we will be communicating over.
  channel_ = channel;
  // Remember the bridge for this channel.
  channel->server_instance_data = static_cast<void*>(this);
  // All NPVariants will be transferred in the format of the browser.
  set_peer_npvariant_size(sizeof(NPVariant));
  // Set up a service for the browser-provided NPN methods.
  NaClSrpcService* service = new(std::nothrow) NaClSrpcService;
  if (NULL == service) {
    DebugPrintf("Couldn't create upcall services.\n");
    return;
  }
  if (!NaClSrpcServiceHandlerCtor(service, srpc_methods)) {
    DebugPrintf("Couldn't construct upcall services.\n");
    return;
  }
  // Export the service on the channel.
  channel->server = service;
  // And inform the client of the available services.
  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeByName(channel,
                                                  "NP_SetUpcallServices",
                                                  service->service_string)) {
    DebugPrintf("Couldn't set upcall services.\n");
  }
}

NPModule::~NPModule() {
  // The corresponding stub is released by the Navigator.
  if (proxy_) {
    NPN_ReleaseObject(proxy_);
  }
}

NACL_SRPC_METHOD_ARRAY(NPModule::srpc_methods) = {
  // Exported from NPModule
  { "NPN_GetValue:ii:iC", GetValue },
  { "NPN_SetStatus:is:", SetStatus },
  { "NPN_InvalidateRect:iC:", InvalidateRect },
  { "NPN_ForceRedraw:i:", ForceRedraw },
  { "NPN_CreateArray:i:iC", CreateArray },
  { "NPN_OpenURL:is:i", OpenURL },
  { "NPN_GetIntIdentifier:i:i", GetIntIdentifier },
  { "NPN_UTF8FromIdentifier:i:is", Utf8FromIdentifier },
  { "NPN_GetStringIdentifier:s:i", GetStringIdentifier },
  { "NPN_IntFromIdentifier:i:i", IntFromIdentifier },
  { "NPN_IdentifierIsString:i:i", IdentifierIsString },
  // Exported from NPObjectStub
  { "NPN_Deallocate:C:", NPObjectStub::Deallocate },
  { "NPN_Invalidate:C:", NPObjectStub::Invalidate },
  { "NPN_HasMethod:Ci:i", NPObjectStub::HasMethod },
  { "NPN_Invoke:CiCCi:iCC", NPObjectStub::Invoke },
  { "NPN_InvokeDefault:CiCi:iCC", NPObjectStub::InvokeDefault },
  { "NPN_HasProperty:Ci:i", NPObjectStub::HasProperty },
  { "NPN_GetProperty:Ci:iCC", NPObjectStub::GetProperty },
  { "NPN_SetProperty:CiCC:i", NPObjectStub::SetProperty },
  { "NPN_RemoveProperty:Ci:i", NPObjectStub::RemoveProperty },
  { "NPN_Enumerate:C:iCi", NPObjectStub::Enumerate },
  { "NPN_Construct:CCCi:iCC", NPObjectStub::Construct },
  { "NPN_SetException:Cs:", NPObjectStub::SetException }
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
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("GetValue\n");
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
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
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("SetStatus\n");
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
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
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("InvalidateRect\n");
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = reinterpret_cast<NPModule*>(NPBridge::LookupBridge(npp));
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
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("ForceRedraw\n");
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = reinterpret_cast<NPModule*>(NPBridge::LookupBridge(npp));

  if (module->window_ && module->window_->window) {
    NPN_ForceRedraw(npp);
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// outputs:
// (int) success
// (char[]) capability
NaClSrpcError NPModule::CreateArray(NaClSrpcChannel* channel,
                                    NaClSrpcArg** inputs,
                                    NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("CreateArray\n");
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
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
  outputs[0]->u.ival = success;
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
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("OpenURL\n");
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
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
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("GetIntIdentifier\n");
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
  outputs[0]->u.ival = NPBridge::NpidentifierToInt(identifier);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) NPIdentifier
// outputs:
// (int) int
NaClSrpcError NPModule::IntFromIdentifier(NaClSrpcChannel* channel,
                                          NaClSrpcArg** inputs,
                                          NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  NPIdentifier identifier = NPBridge::IntToNpidentifier(inputs[0]->u.ival);
  outputs[0]->u.ival = NPN_IntFromIdentifier(identifier);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char*) name
// outputs:
// (int) NPIdentifier
NaClSrpcError NPModule::GetStringIdentifier(NaClSrpcChannel* channel,
                                            NaClSrpcArg** inputs,
                                            NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  char* name = inputs[0]->u.sval;
  NPIdentifier identifier = NPN_GetStringIdentifier(name);
  outputs[0]->u.ival = NPBridge::NpidentifierToInt(identifier);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) NPIdentifier
// outputs:
// (int) bool
NaClSrpcError NPModule::IdentifierIsString(NaClSrpcChannel* channel,
                                           NaClSrpcArg** inputs,
                                           NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  NPIdentifier identifier = NPBridge::IntToNpidentifier(inputs[0]->u.ival);
  outputs[0]->u.ival = NPN_IdentifierIsString(identifier);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) NPIdentifier
// outputs:
// (int) error code
// (char*) name
NaClSrpcError NPModule::Utf8FromIdentifier(NaClSrpcChannel* channel,
                                           NaClSrpcArg** inputs,
                                           NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  NPIdentifier identifier = NPBridge::IntToNpidentifier(inputs[0]->u.ival);
  char* name = NPN_UTF8FromIdentifier(identifier);
  if (NULL == name) {
    outputs[0]->u.ival = NPERR_GENERIC_ERROR;
    outputs[1]->u.sval = strdup("");
  } else {
    outputs[0]->u.ival = NPERR_NO_ERROR;
    // Need to use NPN_MemFree on returned value, whereas srpc will do free().
    outputs[1]->u.sval = strdup(name);
    NPN_MemFree(name);
  }
  return NACL_SRPC_RESULT_OK;
}

NPError NPModule::Initialize() {
  NaClSrpcError retval;

  DebugPrintf("Initialize\n");
  retval = NaClSrpcInvokeByName(channel(),
                                "NP_Initialize",
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
                              uint32_t* serial_size) {
  size_t used = 0;

  for (int i = 0; i < argc; ++i) {
    size_t len = strlen(array[i]) + 1;
    if (*serial_size < used + len) {
      // Length of the serialized array was exceeded.
      return false;
    }
    strncpy(serial_array + used, array[i], len);
    used += len;
  }
  *serial_size = used;
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

  DebugPrintf("New\n");
  for (int i = 0; i < argc; ++i) {
    DebugPrintf("  %"PRIu32": argn=%s argv=%s\n", i, argn[i], argv[i]);
  }
  uint32_t argn_size = static_cast<uint32_t>(sizeof(argn_serial));
  uint32_t argv_size = static_cast<uint32_t>(sizeof(argv_serial));
  if (!SerializeArgArray(argc, argn, argn_serial, &argn_size) ||
      !SerializeArgArray(argc, argv, argv_serial, &argv_size)) {
    DebugPrintf("New: serialize failed\n");
    return NPERR_GENERIC_ERROR;
  }
  NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                              "NPP_New",
                                              mimetype,
                                              NPBridge::NppToInt(npp),
                                              argc,
                                              argn_size,
                                              argn_serial,
                                              argv_size,
                                              argv_serial,
                                              &nperr);
  if (NACL_SRPC_RESULT_OK != retval) {
    DebugPrintf("New: invocation returned %x, %d\n", retval, nperr);
    return NPERR_GENERIC_ERROR;
  }
  return nperr;
}

//
// NPInstance methods
//

NPError NPModule::Destroy(NPP npp, NPSavedData** save) {
  UNREFERENCED_PARAMETER(save);
  NPError nperr;
  NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                              "NPP_Destroy",
                                              NPBridge::NppToInt(npp),
                                              &nperr);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return nperr;
}

NPError NPModule::SetWindow(NPP npp, NPWindow* window) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(window);
  // TODO(sehr): RPC to module.
  return NPERR_NO_ERROR;
}

NPError NPModule::GetValue(NPP npp, NPPVariable variable, void *value) {
  // NOTE: we do not use a switch statement because of compiler warnings */
  // TODO(sehr): RPC to module for most.
  if (NPPVpluginNameString == variable) {
    *static_cast<const char**>(value) = "NativeClient NPAPI bridge plug-in";
    return NPERR_NO_ERROR;
  } else if (NPPVpluginDescriptionString == variable) {
    *static_cast<const char**>(value) =
      "A plug-in for NPAPI based NativeClient modules.";
    return NPERR_NO_ERROR;
  } else if (NPPVpluginScriptableNPObject == variable) {
    *reinterpret_cast<NPObject**>(value) = GetScriptableInstance(npp);
    if (*reinterpret_cast<NPObject**>(value)) {
      return NPERR_NO_ERROR;
    } else {
      return NPERR_GENERIC_ERROR;
    }
  } else {
    return NPERR_INVALID_PARAM;
  }
}

int16_t NPModule::HandleEvent(NPP npp, void* event) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(event);
  // TODO(sehr): Need to make an RPC to pass the event.
  return 0;
}

NPObject* NPModule::GetScriptableInstance(NPP npp) {
  DebugPrintf("GetScriptableInstance: npp %p\n", npp);
  if (NULL == proxy_) {
    // TODO(sehr): Not clear we should be caching on the browser plugin side.
    NPCapability capability;
    NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                                "NPP_GetScriptableInstance",
                                                NPBridge::NppToInt(npp),
                                                sizeof capability,
                                                &capability);
    if (NACL_SRPC_RESULT_OK != retval) {
      DebugPrintf("    Got return code %x\n", retval);
      return NULL;
    }
    proxy_ = NPBridge::CreateProxy(npp, capability);
    DebugPrintf("    Proxy is %p\n", reinterpret_cast<void*>(proxy_));
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
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(type);
  UNREFERENCED_PARAMETER(stream);
  UNREFERENCED_PARAMETER(seekable);
  *stype = NP_ASFILEONLY;
  return NPERR_NO_ERROR;
}

void NPModule::StreamAsFile(NPP npp, NPStream* stream, const char* filename) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(stream);
  UNREFERENCED_PARAMETER(filename);
  // TODO(sehr): Implement using UrlAsNaClDesc.
}

NPError NPModule::DestroyStream(NPP npp, NPStream *stream, NPError reason) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(stream);
  UNREFERENCED_PARAMETER(reason);
  return NPERR_NO_ERROR;
}

void NPModule::URLNotify(NPP npp,
                         const char* url,
                         NPReason reason,
                         void* notify_data) {
  UNREFERENCED_PARAMETER(url);
  UNREFERENCED_PARAMETER(notify_data);
  DebugPrintf("URLNotify: npp %p, rsn %d\n", static_cast<void*>(npp), reason);
  // TODO(sehr): need a call when reason is failure.
  if (NPRES_DONE != reason) {
    return;
  }
  // TODO(sehr): Need to set the descriptor appropriately and call.
  // NaClSrpcInvokeByName(channel(), "NPP_URLNotify", desc, reason);
}

}  // namespace nacl
