// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/npmodule.h"

#include "gpu/command_buffer/common/command_buffer.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

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
      window_(NULL),
      extensions_(NULL),
      device2d_(NULL),
      context2d_(NULL) {
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
  { "NPN_InvokeDefault:CCCi:iCC", NPObjectStub::InvokeDefault },
  { "NPN_HasProperty:Ci:i", NPObjectStub::HasProperty },
  { "NPN_GetProperty:Ci:iCC", NPObjectStub::GetProperty },
  { "NPN_SetProperty:CiCC:i", NPObjectStub::SetProperty },
  { "NPN_RemoveProperty:Ci:i", NPObjectStub::RemoveProperty },
  { "NPN_Enumerate:C:iCi", NPObjectStub::Enumerate },
  { "NPN_Construct:CCCi:iCC", NPObjectStub::Construct },
  { "NPN_SetException:Cs:", NPObjectStub::SetException },
  { "Device2DInitialize:i:hiiiii", Device2DInitialize },
  { "Device2DFlush:i:iiiii", Device2DFlush },
  { "Device2DDestroy:i:", Device2DDestroy },
  { "Device3DInitialize:i:hiii", Device3DInitialize },
  { "Device3DFlush:i:ii", Device3DFlush },
  { "Device3DDestroy:i:", Device3DDestroy },
  { "Device3DGetState:ii:i", Device3DGetState },
  { "Device3DSetState:iii:", Device3DSetState },
  { "Device3DCreateBuffer:ii:hi", Device3DCreateBuffer },
  { "Device3DDestroyBuffer:ii:", Device3DDestroyBuffer },
  { NULL, NULL }
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
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
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
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));

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
  if (NPERR_NO_ERROR != NPN_GetValue(npp, NPNVWindowNPObject, &window)) {
    DebugPrintf("NPNVWindowNPObject returned false\n");
    outputs[0]->u.ival = 0;
    return NACL_SRPC_RESULT_OK;
  }
  NPString script;
  const char scriptText[] = "new Array();";
  script.UTF8Characters = scriptText;
  script.UTF8Length = strlen(scriptText);
  NPVariant result;
  int success = NPN_Evaluate(npp, window, &script, &result) &&
                NPVARIANT_IS_OBJECT(result);
  if (success) {
    RpcArg ret0(npp, outputs[1]);
    ret0.PutObject(NPVARIANT_TO_OBJECT(result));
    // TODO(sehr): We're leaking result here.
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
  NPUTF8* name = inputs[0]->u.sval;
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

// Device2D support
class TransportDIB;
struct Device2DImpl {
  ::TransportDIB* dib;
};

// inputs:
// (int) npp
// outputs:
// (handle) shared memory
// (int) stride
// (int) left
// (int) top
// (int) right
// (int) bottom
NaClSrpcError NPModule::Device2DInitialize(NaClSrpcChannel* channel,
                                           NaClSrpcArg** inputs,
                                           NaClSrpcArg** outputs) {
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
  UNREFERENCED_PARAMETER(channel);
  // Initialize the return values in case of failure.
  outputs[0]->u.hval = NULL;
  outputs[1]->u.ival = -1;
  outputs[2]->u.ival = -1;
  outputs[3]->u.ival = -1;
  outputs[4]->u.ival = -1;
  outputs[5]->u.ival = -1;

  if (NULL == module->extensions_) {
    if (NPERR_NO_ERROR !=
        NPN_GetValue(npp, NPNVPepperExtensions, &module->extensions_)) {
      // Because this variable is not implemented in other browsers, this path
      // should always be taken except in Pepper-enabled browsers.
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    if (NULL == module->extensions_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == module->device2d_) {
    module->device2d_ =
        module->extensions_->acquireDevice(npp, NPPepper2DDevice);
    if (NULL == module->device2d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == module->context2d_) {
    module->context2d_ = new(std::nothrow) NPDeviceContext2D;
    if (NULL == module->context2d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    NPError retval =
        module->device2d_->initializeContext(npp, NULL, module->context2d_);
    if (NPERR_NO_ERROR != retval) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  DescWrapperFactory factory;
  Device2DImpl* impl =
      reinterpret_cast<Device2DImpl*>(module->context2d_->reserved);
  DescWrapper* wrapper = factory.ImportTransportDIB(impl->dib);
  if (NULL == wrapper) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Increase reference count for SRPC return value, since wrapper Delete
  // would cause Dtor to fire.
  outputs[0]->u.hval = NaClDescRef(wrapper->desc());
  // Free the wrapper.
  wrapper->Delete();
  outputs[1]->u.ival = module->context2d_->stride;
  outputs[2]->u.ival = module->context2d_->dirty.left;
  outputs[3]->u.ival = module->context2d_->dirty.top;
  outputs[4]->u.ival = module->context2d_->dirty.right;
  outputs[5]->u.ival = module->context2d_->dirty.bottom;
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// outputs:
// (handle) shared memory
// (int) stride
// (int) left
// (int) top
// (int) right
// (int) bottom
NaClSrpcError NPModule::Device2DFlush(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs) {
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
  NPError retval;
  UNREFERENCED_PARAMETER(channel);

  if (NULL == module->extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  retval = module->device2d_->flushContext(npp, module->context2d_, NULL, NULL);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  outputs[0]->u.ival = module->context2d_->stride;
  outputs[1]->u.ival = module->context2d_->dirty.left;
  outputs[2]->u.ival = module->context2d_->dirty.top;
  outputs[3]->u.ival = module->context2d_->dirty.right;
  outputs[4]->u.ival = module->context2d_->dirty.bottom;
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// outputs:
// none
NaClSrpcError NPModule::Device2DDestroy(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs) {
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
  NPError retval;
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);

  if (NULL == module->extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  retval = module->device2d_->destroyContext(npp, module->context2d_);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

namespace base {
class SharedMemory;
}  // namespace base

struct Device3DImpl {
  gpu::CommandBuffer* command_buffer;
};

// inputs:
// (int) npp
// outputs:
// (handle) shared memory
// (int) commandBufferEntries
// (int) getOffset
// (int) putOffset
NaClSrpcError NPModule::Device3DInitialize(NaClSrpcChannel* channel,
                                           NaClSrpcArg** inputs,
                                           NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);

  // Initialize the return values in case of failure.
  outputs[0]->u.hval =
      const_cast<NaClDesc*>(
          reinterpret_cast<const NaClDesc*>(NaClDescInvalidMake()));
  outputs[1]->u.ival = -1;
  outputs[2]->u.ival = -1;
  outputs[3]->u.ival = -1;

#if defined(NACL_STANDALONE)
  UNREFERENCED_PARAMETER(inputs);
  return NACL_SRPC_RESULT_APP_ERROR;
#else
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));

  if (NULL == module->extensions_) {
    if (NPERR_NO_ERROR !=
        NPN_GetValue(npp, NPNVPepperExtensions, &module->extensions_)) {
      // Because this variable is not implemented in other browsers, this path
      // should always be taken except in Pepper-enabled browsers.
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    if (NULL == module->extensions_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == module->device3d_) {
    module->device3d_ =
        module->extensions_->acquireDevice(npp, NPPepper3DDevice);
    if (NULL == module->device3d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == module->context3d_) {
    module->context3d_ = new(std::nothrow) NPDeviceContext3D;
    if (NULL == module->context3d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    NPError retval =
        module->device3d_->initializeContext(npp, NULL, module->context3d_);
    if (NPERR_NO_ERROR != retval) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  Device3DImpl* impl =
      reinterpret_cast<Device3DImpl*>(module->context3d_->reserved);
  ::base::SharedMemory* shm =
      impl->command_buffer->GetRingBuffer().shared_memory;
  if (NULL == shm) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  DescWrapperFactory factory;
  DescWrapper* wrapper =
      factory.ImportSharedMemory(shm);
  if (NULL == wrapper) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Increase reference count for SRPC return value, since wrapper Delete
  // would cause Dtor to fire.
  outputs[0]->u.hval = NaClDescRef(wrapper->desc());
  // Free the wrapper.
  wrapper->Delete();
  outputs[1]->u.ival = module->context3d_->commandBufferEntries;
  outputs[2]->u.ival = module->context3d_->getOffset;
  outputs[3]->u.ival = module->context3d_->putOffset;
  return NACL_SRPC_RESULT_OK;
#endif  // defined(NACL_STANDALONE)
}

// inputs:
// (int) npp
// outputs:
// (int) getOffset
// (int) putOffset
NaClSrpcError NPModule::Device3DFlush(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));

  if (NULL == module->extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPError retval =
      module->device3d_->flushContext(npp, module->context3d_, NULL, NULL);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  outputs[0]->u.ival = module->context3d_->getOffset;
  outputs[1]->u.ival = module->context3d_->putOffset;
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// outputs:
// none
NaClSrpcError NPModule::Device3DDestroy(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));

  if (NULL == module->extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPError retval = module->device3d_->destroyContext(npp, module->context3d_);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

class NppClosure {
 public:
  NppClosure(uint32_t number, NPModule* module) {
    number_ = number;
    module_ = module;
  }
  uint32_t number() const { return number_; }
  NPModule* module() const { return module_; }
 private:
  uint32_t number_;
  NPModule* module_;
};

// The thunk enqueued on the browser's foreground thread.
static void doNppAsyncCall(void* arg) {
  NppClosure* closure = reinterpret_cast<NppClosure*>(arg);
  if (NULL != closure) {
    NaClSrpcInvokeByName(closure->module()->channel(),
                         "NPP_PluginThreadAsyncCall",
                         closure->number());
  }
  delete closure;
}

// inputs:
// (int) npp
// (int) state
// outputs:
// (int) value
// none
NaClSrpcError NPModule::Device3DGetState(NaClSrpcChannel* channel,
                                         NaClSrpcArg** inputs,
                                         NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
  NPError retval = module->device3d_->getStateContext(npp,
                                                      module->context3d_,
                                                      inputs[1]->u.ival,
                                                      &(outputs[0]->u.ival));
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// (int) state
// (int) value
// outputs:
// none
NaClSrpcError NPModule::Device3DSetState(NaClSrpcChannel* channel,
                                         NaClSrpcArg** inputs,
                                         NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
  NPError retval = module->device3d_->setStateContext(npp,
                                                      module->context3d_,
                                                      inputs[1]->u.ival,
                                                      inputs[2]->u.ival);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// (int) size
// outputs:
// (handle) buffer
// (int) id
// none
NaClSrpcError NPModule::Device3DCreateBuffer(NaClSrpcChannel* channel,
                                             NaClSrpcArg** inputs,
                                             NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);

  // Initialize buffer id and returned handle to allow error returns.
  int buffer_id = -1;
  outputs[1]->u.ival = buffer_id;
  outputs[0]->u.hval =
      const_cast<NaClDesc*>(
          reinterpret_cast<const NaClDesc*>(NaClDescInvalidMake()));

#if defined(NACL_STANDALONE)
  UNREFERENCED_PARAMETER(inputs);
  return NACL_SRPC_RESULT_APP_ERROR;
#else
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));

  // Call the Pepper API.
  NPError retval = module->device3d_->createBuffer(npp,
                                                   module->context3d_,
                                                   inputs[1]->u.ival,
                                                   &buffer_id);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Look up the base::SharedMemory for the returned id.
  Device3DImpl* impl =
      reinterpret_cast<Device3DImpl*>(module->context3d_->reserved);
  ::base::SharedMemory* shm =
      impl->command_buffer->GetTransferBuffer(buffer_id).shared_memory;
  if (NULL == shm) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Create a NaCl descriptor to return.
  DescWrapperFactory factory;
  DescWrapper* wrapper = factory.ImportSharedMemory(shm);
  if (NULL == wrapper) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Increase reference count for SRPC return value, since wrapper Delete
  // would cause Dtor to fire.
  outputs[0]->u.hval = NaClDescRef(wrapper->desc());
  // Clean up.
  wrapper->Delete();
  return NACL_SRPC_RESULT_OK;
#endif  // defined(NACL_STANDALONE)
}

// inputs:
// (int) npp
// (int) id
// outputs:
// none
NaClSrpcError NPModule::Device3DDestroyBuffer(NaClSrpcChannel* channel,
                                              NaClSrpcArg** inputs,
                                              NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
  NPError retval = module->device3d_->destroyBuffer(npp,
                                                    module->context3d_,
                                                    inputs[1]->u.ival);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// (int) plugin closure number to be invoked by callback.
// outputs:
// none
static NaClSrpcError handleAsyncCall(NaClSrpcChannel* channel,
                                     NaClSrpcArg** inputs,
                                     NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("handleAsyncCall\n");
  NPP npp = NPBridge::IntToNpp(inputs[0]->u.ival);
  NPModule* module = static_cast<NPModule*>(NPBridge::LookupBridge(npp));
  uint32_t number = static_cast<uint32_t>(inputs[1]->u.ival);

  // Place a closure on the browser's javascript foreground thread.
  NppClosure* closure = new(std::nothrow) NppClosure(number, module);
  if (NULL == closure) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPN_PluginThreadAsyncCall(npp,
                            doNppAsyncCall,
                            static_cast<void*>(closure));
  return NACL_SRPC_RESULT_OK;
}

// Structure for passing information to the thread.  Shares ownership of
// the descriptor with the creating routine.  This allows passing ownership
// to the upcall thread.
struct UpcallInfo {
 public:
  DescWrapper* desc_;
  NPModule* module_;
  UpcallInfo(DescWrapper* desc, NPModule* module) {
    desc_ = desc;
    module_ = module;
  }
  ~UpcallInfo() {
    DebugPrintf("deleting upcall info\n");
    desc_->Delete();
  }
};

static void WINAPI UpcallThread(void* arg) {
  UpcallInfo* info = reinterpret_cast<UpcallInfo*>(arg);
  NaClSrpcHandlerDesc handlers[] = {
    { "NPN_PluginThreadAsyncCall:ii:", handleAsyncCall },
    { NULL, NULL }
  };
  DebugPrintf("UpcallThread(%p)\n", arg);
  // Run the SRPC server.
  NaClSrpcServerLoop(info->desc_->desc(), handlers, info->module_);
  // Free the info node.
  delete info;
  DebugPrintf("UpcallThread: End\n");
}

NPError NPModule::Initialize() {
  NaClSrpcError retval;
  DescWrapperFactory factory;
  DescWrapper* pair[2] = { NULL, NULL };
  UpcallInfo* info = NULL;
  NPError err = NPERR_GENERIC_ERROR;

  DebugPrintf("Initialize\n");

  // Create a socket pair for the upcall server.
  if (factory.MakeSocketPair(pair)) {
    goto done;
  }
  // Create an info node to pass to the thread.
  info = new(std::nothrow) UpcallInfo(pair[0], this);
  if (NULL == info) {
    goto done;
  }
  // info takes ownership of pair[0].
  pair[0] = NULL;
  // Create a thread and an SRPC "upcall" server.
  if (!NaClThreadCtor(&upcall_thread_, UpcallThread, info, 128 << 10)) {
    goto done;
  }
  // On success, ownership of info passes to the thread.
  info = NULL;
  // Invoke the NaCl module's NP_Initialize function.
  retval = NaClSrpcInvokeByName(channel(),
                                "NP_Initialize",
                                GETPID(),
                                static_cast<int>(sizeof(NPVariant)),
                                pair[1]->desc());
  // Return the appropriate error code.
  if (NACL_SRPC_RESULT_OK != retval) {
    goto done;
  }
  err = NPERR_NO_ERROR;

 done:
  DebugPrintf("deleting pairs\n");
  DescWrapper::SafeDelete(pair[0]);
  DescWrapper::SafeDelete(pair[1]);
  delete info;
  return err;
}

static bool SerializeArgArray(int argc,
                              char* array[],
                              char* serial_array,
                              uint32_t* serial_size) {
  size_t used = 0;

  for (int i = 0; i < argc; ++i) {
    // Note that strlen() cannot ever return SIZE_T_MAX, since
    // that would imply that there were no nulls anywhere in memory,
    // which would lead to strlen() never terminating. So this
    // assignment is safe.
    size_t len = strlen(array[i]) + 1;

    if (len > std::numeric_limits<uint32_t>::max()) {
      // overflow, input string is too long
      return false;
    }

    if (used > std::numeric_limits<uint32_t>::max() - len) {
      // overflow, output string is too long
      return false;
    }

    if (used > *serial_size - len) {
      // Length of the serialized array was exceeded.
      return false;
    }
    strncpy(serial_array + used, array[i], len);
    used += len;
  }
  // Note that there is a check against numeric_limits<uint32_t> in
  // the code above, which is why this cast is safe.
  *serial_size = static_cast<uint32_t>(used);
  return true;
}

NPError NPModule::New(char* mimetype,
                      NPP npp,
                      int argc,
                      char* argn[],
                      char* argv[]) {
  // NPError is shorter than an int, causing stack corruption reports
  // on Windows, because SRPC doesn't have an int16 type.
  int nperr;
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
  return static_cast<NPError>(nperr);
}

//
// NPInstance methods
//

NPError NPModule::Destroy(NPP npp, NPSavedData** save) {
  UNREFERENCED_PARAMETER(save);
  int nperr;
  NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                              "NPP_Destroy",
                                              NPBridge::NppToInt(npp),
                                              &nperr);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return static_cast<NPError>(nperr);
}

NPError NPModule::SetWindow(NPP npp, NPWindow* window) {
  if (NULL == window) {
    return NPERR_NO_ERROR;
  }
  int nperr;
  NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                              "NPP_SetWindow",
                                              NPBridge::NppToInt(npp),
                                              window->height,
                                              window->width,
                                              &nperr);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return static_cast<NPError>(nperr);
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
  static const uint32_t kEventSize =
      static_cast<uint32_t>(sizeof(NPPepperEvent));
  int32_t return_int16;

  NaClSrpcError retval = NaClSrpcInvokeByName(channel(),
                                              "NPP_HandleEvent",
                                              NPBridge::NppToInt(npp),
                                              kEventSize,
                                              reinterpret_cast<char*>(event),
                                              &return_int16);
  if (NACL_SRPC_RESULT_OK == retval) {
    return static_cast<int16_t>(return_int16 & 0xffff);
  } else {
    return -1;
  }
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
