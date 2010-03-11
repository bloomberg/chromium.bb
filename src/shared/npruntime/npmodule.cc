// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/checked_cast.h"
#include "native_client/src/shared/npruntime/npmodule.h"

#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/npruntime/npupcall_server.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/npapi/bindings/npapi_extensions_private.h"
#ifndef NACL_STANDALONE
#include "base/shared_memory.h"
#endif  // NACL_STANDALONE

// Used to convey pixel BGRA values to the 2D API.
class TransportDIB;

namespace nacl {

// Class static variable declarations.
bool NPModule::is_webkit = false;

NPModule::NPModule(NaClSrpcChannel* channel)
    : proxy_(NULL),
      window_(NULL),
      extensions_(NULL),
      device2d_(NULL),
      context2d_(NULL),
      device3d_(NULL),
      context3d_(NULL) {
  // Remember the channel we will be communicating over.
  channel_ = channel;
  // Remember the bridge for this channel.
  channel->server_instance_data = static_cast<void*>(this);
  // Set up a service for the browser-provided NPN methods.
  NaClSrpcService* service = new(std::nothrow) NaClSrpcService;
  if (NULL == service) {
    DebugPrintf("Couldn't create upcall services.\n");
    return;
  }
  if (!NaClSrpcServiceHandlerCtor(service, NPModuleRpcs::srpc_methods)) {
    DebugPrintf("Couldn't construct upcall services.\n");
    return;
  }
  // Export the service on the channel.
  channel->server = service;
  // And inform the client of the available services.
  char* str = const_cast<char*>(service->service_string);
  if (NACL_SRPC_RESULT_OK !=
      NPNavigatorRpcClient::NP_SetUpcallServices(channel, str)) {
    DebugPrintf("Couldn't set upcall services.\n");
  }
}

NPModule::~NPModule() {
  // The corresponding stub is released by the Navigator.
  if (proxy_) {
    NPN_ReleaseObject(proxy_);
  }
  // TODO(sehr): release contexts, etc., here.
}

NPModule* NPModule::GetModule(int32_t wire_npp) {
  NPP npp = WireFormatToNPP(wire_npp);
  return static_cast<NPModule*>(NPBridge::LookupBridge(npp));
}

NPError NPModule::Initialize() {
  NPError err = NPERR_GENERIC_ERROR;
  DescWrapper* wrapper = NULL;

  DebugPrintf("Initialize\n");
  // Start the upcall server on a separate thread.
  wrapper = NPUpcallServer::Start(this, &upcall_thread_);
  if (NULL != wrapper) {
    // Invoke the NaCl module's NP_Initialize function.
    int32_t nacl_pid;
    NaClSrpcError retval =
        NPNavigatorRpcClient::NP_Initialize(channel(),
                                            GETPID(),
                                            wrapper->desc(),
                                            &nacl_pid);
    // Return the appropriate error code.
    if (NACL_SRPC_RESULT_OK != retval) {
      goto done;
    }
    set_peer_pid(nacl_pid);
    err = NPERR_NO_ERROR;
  }

 done:
  DescWrapper::SafeDelete(wrapper);
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
    DebugPrintf("  %"NACL_PRIu32": argn=%s argv=%s\n", i, argn[i], argv[i]);
  }
  uint32_t argn_size = static_cast<uint32_t>(sizeof(argn_serial));
  uint32_t argv_size = static_cast<uint32_t>(sizeof(argv_serial));
  if (!SerializeArgArray(argc, argn, argn_serial, &argn_size) ||
      !SerializeArgArray(argc, argv, argv_serial, &argv_size)) {
    DebugPrintf("New: serialize failed\n");
    return NPERR_GENERIC_ERROR;
  }
  NaClSrpcError retval = NPNavigatorRpcClient::NPP_New(channel(),
                                                       mimetype,
                                                       NPPToWireFormat(npp),
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
  NaClSrpcError retval =
      NPNavigatorRpcClient::NPP_Destroy(channel(),
                                        NPPToWireFormat(npp),
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
  NaClSrpcError retval =
      NPNavigatorRpcClient::NPP_SetWindow(channel(),
                                          NPPToWireFormat(npp),
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
    DebugPrintf("Getting scriptable instance: npp %p\n",
                reinterpret_cast<void*>(npp));
    if (NULL == proxy_) {
      NPCapability capability;
      nacl_abi_size_t cap_size = capability.size();
      char* cap_ptr = capability.char_addr();
      NaClSrpcError retval =
          NPNavigatorRpcClient::GetScriptableInstance(channel(),
                                                      NPPToWireFormat(npp),
                                                      &cap_size,
                                                      cap_ptr);
      if (NACL_SRPC_RESULT_OK != retval) {
        return NULL;
      }
      proxy_ = NPBridge::CreateProxy(npp, capability);
    }
    if (NULL == proxy_) {
      return NPERR_GENERIC_ERROR;
    } else {
      *reinterpret_cast<NPObject**>(value) = NPN_RetainObject(proxy_);
      return NPERR_NO_ERROR;
    }
  }
  return NPERR_INVALID_PARAM;
}

int16_t NPModule::HandleEvent(NPP npp, void* event) {
  static const uint32_t kEventSize =
      static_cast<uint32_t>(sizeof(NPPepperEvent));
  int32_t return_int16;

  NaClSrpcError retval =
      NPNavigatorRpcClient::NPP_HandleEvent(channel(),
                                            NPPToWireFormat(npp),
                                            kEventSize,
                                            reinterpret_cast<char*>(event),
                                            &return_int16);
  if (NACL_SRPC_RESULT_OK == retval) {
    return static_cast<int16_t>(return_int16 & 0xffff);
  } else {
    return -1;
  }
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

void NPModule::StreamAsFile(NPP npp,
                            NaClDesc* file,
                            char* url,
                            uint32_t size) {
  NPNavigatorRpcClient::NPP_StreamAsFile(channel(),
                                         NPPToWireFormat(npp),
                                         file,
                                         url,
                                         static_cast<int32_t>(size));
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
  // NPNavigatorRpcClient::NPP_URLNotify(channel(), desc, reason);
}

NaClSrpcError NPModule::Device2DInitialize(NPP npp,
                                           NaClSrpcImcDescType* shm_desc,
                                           int32_t* stride,
                                           int32_t* left,
                                           int32_t* top,
                                           int32_t* right,
                                           int32_t* bottom) {
  // Initialize the return values in case of failure.
  *shm_desc =
      const_cast<NaClDesc*>(
          reinterpret_cast<const NaClDesc*>(NaClDescInvalidMake()));
  *stride = -1;
  *left = -1;
  *top = -1;
  *right = -1;
  *bottom = -1;

  if (NULL == extensions_) {
    if (NPERR_NO_ERROR !=
        NPN_GetValue(npp, NPNVPepperExtensions, &extensions_)) {
      // Because this variable is not implemented in other browsers, this path
      // should always be taken except in Pepper-enabled browsers.
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    if (NULL == extensions_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == device2d_) {
    device2d_ = extensions_->acquireDevice(npp, NPPepper2DDevice);
    if (NULL == device2d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == context2d_) {
    context2d_ = new(std::nothrow) NPDeviceContext2D;
    if (NULL == context2d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    NPError retval = device2d_->initializeContext(npp, NULL, context2d_);
    if (NPERR_NO_ERROR != retval) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  DescWrapperFactory factory;
  // Get the TransportDIB used for the context buffer.
  intptr_t dib_intptr;
  device2d_->getStateContext(npp,
                             context2d_,
                             NPExtensionsReservedStateSharedMemory,
                             &dib_intptr);
  TransportDIB* dib = reinterpret_cast<TransportDIB*>(dib_intptr);
  DescWrapper* wrapper = factory.ImportTransportDIB(dib);
  if (NULL == wrapper) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Increase reference count for SRPC return value, since wrapper Delete
  // would cause Dtor to fire.
  *shm_desc = NaClDescRef(wrapper->desc());
  // Free the wrapper.
  wrapper->Delete();
  *stride = context2d_->stride;
  *left = context2d_->dirty.left;
  *top = context2d_->dirty.top;
  *right = context2d_->dirty.right;
  *bottom = context2d_->dirty.bottom;

  return NACL_SRPC_RESULT_OK;
}

// Note: this function may be invoked from other than the NPAPI thread.
NaClSrpcError NPModule::Device2DFlush(NPP npp,
                                      int32_t* stride,
                                      int32_t* left,
                                      int32_t* top,
                                      int32_t* right,
                                      int32_t* bottom) {
  if (NULL == extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPError retval = device2d_->flushContext(npp, context2d_, NULL, NULL);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  *stride = context2d_->stride;
  *left = context2d_->dirty.left;
  *top = context2d_->dirty.top;
  *right = context2d_->dirty.right;
  *bottom = context2d_->dirty.bottom;

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModule::Device2DDestroy(NPP npp) {
  if (NULL == extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPError retval = device2d_->destroyContext(npp, context2d_);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }

  return NACL_SRPC_RESULT_OK;
}

namespace base {
class SharedMemory;
class SyncSocket;
}  // namespace base

struct Device3DImpl {
  gpu::CommandBuffer* command_buffer;
};

NaClSrpcError NPModule::Device3DInitialize(NPP npp,
                                           int32_t entries_requested,
                                           NaClSrpcImcDescType* shm_desc,
                                           int32_t* entries_obtained,
                                           int32_t* get_offset,
                                           int32_t* put_offset) {
  // Initialize the return values in case of failure.
  *shm_desc =
      const_cast<NaClDesc*>(
          reinterpret_cast<const NaClDesc*>(NaClDescInvalidMake()));
  *entries_obtained = -1;
  *get_offset = -1;
  *put_offset = -1;

#if defined(NACL_STANDALONE)
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(entries_requested);
  UNREFERENCED_PARAMETER(shm_desc);
  UNREFERENCED_PARAMETER(entries_obtained);
  UNREFERENCED_PARAMETER(get_offset);
  UNREFERENCED_PARAMETER(put_offset);

  return NACL_SRPC_RESULT_APP_ERROR;
#else
  if (NULL == extensions_) {
    if (NPERR_NO_ERROR !=
        NPN_GetValue(npp, NPNVPepperExtensions, &extensions_)) {
      // Because this variable is not implemented in other browsers, this path
      // should always be taken except in Pepper-enabled browsers.
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    if (NULL == extensions_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == device3d_) {
    device3d_ = extensions_->acquireDevice(npp, NPPepper3DDevice);
    if (NULL == device3d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == context3d_) {
    context3d_ = new(std::nothrow) NPDeviceContext3D;
    if (NULL == context3d_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    static NPDeviceContext3DConfig config;
    config.commandBufferSize = entries_requested;
    NPError retval =
        device3d_->initializeContext(npp, &config, context3d_);
    if (NPERR_NO_ERROR != retval) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  Device3DImpl* impl =
      reinterpret_cast<Device3DImpl*>(context3d_->reserved);
  ::base::SharedMemory* shm =
      impl->command_buffer->GetRingBuffer().shared_memory;
  if (NULL == shm) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  DescWrapperFactory factory;
  size_t shm_size = context3d_->commandBufferSize * sizeof(int32_t);
  DescWrapper* wrapper =
      factory.ImportSharedMemory(shm, static_cast<size_t>(shm_size));
  if (NULL == wrapper) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Increase reference count for SRPC return value, since wrapper Delete
  // would cause Dtor to fire.
  *shm_desc = NaClDescRef(wrapper->desc());
  // Free the wrapper.
  wrapper->Delete();
  *entries_obtained = context3d_->commandBufferSize;
  *get_offset = context3d_->getOffset;
  *put_offset = context3d_->putOffset;

  return NACL_SRPC_RESULT_OK;
#endif  // defined(NACL_STANDALONE)
}

// Note: this function may be invoked from other than the NPAPI thread.
NaClSrpcError NPModule::Device3DFlush(NPP npp,
                                      int32_t put_offset,
                                      int32_t* get_offset,
                                      int32_t* token,
                                      int32_t* error) {
  if (NULL == extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  context3d_->putOffset = put_offset;
  NPError retval = device3d_->flushContext(npp, context3d_, NULL, NULL);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  *get_offset = context3d_->getOffset;
  *token = context3d_->token;
  *error = static_cast<int32_t>(context3d_->error);

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModule::Device3DDestroy(NPP npp) {
  if (NULL == extensions_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPError retval = device3d_->destroyContext(npp, context3d_);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModule::Device3DGetState(NPP npp,
                                         int32_t state,
                                         int32_t* value) {
  intptr_t value_ptr;
  NPError retval =
      device3d_->getStateContext(npp, context3d_, state, &value_ptr);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  *value = static_cast<int32_t>(value_ptr);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModule::Device3DSetState(NPP npp,
                                         int32_t state,
                                         int32_t value) {
  intptr_t value_ptr = static_cast<intptr_t>(value);
  NPError retval =
      device3d_->setStateContext(npp, context3d_, state, value_ptr);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModule::Device3DCreateBuffer(NPP npp,
                                             int32_t size,
                                             NaClSrpcImcDescType* shm_desc,
                                             int32_t* id) {
  // Initialize buffer id and returned handle to allow error returns.
  int buffer_id = -1;
  *shm_desc =
      const_cast<NaClDesc*>(
          reinterpret_cast<const NaClDesc*>(NaClDescInvalidMake()));
  *id = buffer_id;

#if defined(NACL_STANDALONE)
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(shm_desc);
  UNREFERENCED_PARAMETER(id);
  return NACL_SRPC_RESULT_APP_ERROR;
#else
  // Call the Pepper API.
  NPError retval = device3d_->createBuffer(npp, context3d_, size, &buffer_id);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Look up the base::SharedMemory for the returned id.
  Device3DImpl* impl =
      reinterpret_cast<Device3DImpl*>(context3d_->reserved);
  ::base::SharedMemory* shm =
      impl->command_buffer->GetTransferBuffer(buffer_id).shared_memory;
  if (NULL == shm) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Create a NaCl descriptor to return.
  DescWrapperFactory factory;
  DescWrapper* wrapper =
      factory.ImportSharedMemory(shm, static_cast<size_t>(size));
  if (NULL == wrapper) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Increase reference count for SRPC return value, since wrapper Delete
  // would cause Dtor to fire.
  *shm_desc = NaClDescRef(wrapper->desc());
  *id = buffer_id;
  // Clean up.
  wrapper->Delete();

  return NACL_SRPC_RESULT_OK;
#endif  // defined(NACL_STANDALONE)
}

NaClSrpcError NPModule::Device3DDestroyBuffer(NPP npp, int32_t id) {
  NPError retval = device3d_->destroyBuffer(npp, context3d_, id);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }

  return NACL_SRPC_RESULT_OK;
}

struct AudioCallbackInfo {
  NPP npp_;
  int32_t closure_number_;
  NaClSrpcChannel* channel_;
  NPDevice* device_audio_;
  NPDeviceContextAudio* context_audio_;
};

void AudioCallback(NPDeviceContextAudio* user_data) {
  AudioCallbackInfo* info =
      reinterpret_cast<AudioCallbackInfo*>(user_data->config.userData);
  DescWrapperFactory factory;
  // Get the shared memory size used for audio.
  intptr_t shm_size_intptr;
  NPError retval =
      info->device_audio_->getStateContext(
          info->npp_,
          info->context_audio_,
          NPExtensionsReservedStateSharedMemorySize,
          &shm_size_intptr);
  if (NPERR_NO_ERROR != retval) {
    return;
  }
  int32_t size = static_cast<int32_t>(shm_size_intptr);
  // Get the shared memory used for audio.
  intptr_t shm_int;
  retval =
      info->device_audio_->getStateContext(
          info->npp_,
          info->context_audio_,
          NPExtensionsReservedStateSharedMemory,
          &shm_int);
  if (NPERR_NO_ERROR != retval) {
    return;
  }
  ::base::SharedMemory* shm =
      reinterpret_cast< ::base::SharedMemory*>(shm_int);
  DescWrapper* shm_wrapper =
      factory.ImportSharedMemory(shm, static_cast<size_t>(size));
  if (NULL == shm_wrapper) {
    return;
  }
  // Get the sync channel used for audio.
  intptr_t sync_int;
  retval =
      info->device_audio_->getStateContext(info->npp_,
                                           info->context_audio_,
                                           NPExtensionsReservedStateSyncChannel,
                                           &sync_int);
  if (NPERR_NO_ERROR != retval) {
    return;
  }
  ::base::SyncSocket* sync =
      reinterpret_cast< ::base::SyncSocket*>(sync_int);
  DescWrapper* sync_wrapper = NULL;
  if (NULL == sync) {
    // The high-latency audio interface does not use a sync socket.
    sync_wrapper = factory.MakeInvalid();
  } else {
    sync_wrapper = factory.ImportSyncSocket(sync);
  }
  if (NULL == sync_wrapper) {
    DescWrapper::SafeDelete(shm_wrapper);
    return;
  }

  // Send the RPC to the NaCl module, conveying the descriptors and
  // starting the thread and the prefill.
  NPNavigatorRpcClient::AudioCallback(info->channel_,
                                      info->closure_number_,
                                      NaClDescRef(shm_wrapper->desc()),
                                      size,
                                      NaClDescRef(sync_wrapper->desc()));

  DescWrapper::SafeDelete(shm_wrapper);
  DescWrapper::SafeDelete(sync_wrapper);
}

NaClSrpcError NPModule::AudioInitialize(NPP npp,
                                        int32_t closure_number,
                                        int32_t sample_rate,
                                        int32_t sample_type,
                                        int32_t output_channel_map,
                                        int32_t input_channel_map,
                                        int32_t sample_frame_count,
                                        int32_t flags) {
#if defined(NACL_STANDALONE)
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(closure_number);
  UNREFERENCED_PARAMETER(sample_rate);
  UNREFERENCED_PARAMETER(sample_type);
  UNREFERENCED_PARAMETER(output_channel_map);
  UNREFERENCED_PARAMETER(input_channel_map);
  UNREFERENCED_PARAMETER(sample_frame_count);
  UNREFERENCED_PARAMETER(flags);
  return NACL_SRPC_RESULT_APP_ERROR;
#else
  if (NULL == extensions_) {
    if (NPERR_NO_ERROR !=
        NPN_GetValue(npp, NPNVPepperExtensions, &extensions_)) {
      // Because this variable is not implemented in other browsers, this path
      // should always be taken except in Pepper-enabled browsers.
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    if (NULL == extensions_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == device_audio_) {
    device_audio_ = extensions_->acquireDevice(npp, NPPepperAudioDevice);
    if (NULL == device_audio_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  if (NULL == context_audio_) {
    context_audio_ = new(std::nothrow) NPDeviceContextAudio;
    if (NULL == context_audio_) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    static NPDeviceContextAudioConfig config;
    config.sampleRate = sample_rate;
    config.sampleType = sample_type;
    config.outputChannelMap = output_channel_map;
    config.inputChannelMap = input_channel_map;
    config.sampleFrameCount = sample_frame_count;
    config.flags = flags;
    config.callback = AudioCallback;
    AudioCallbackInfo* info = new(std::nothrow) AudioCallbackInfo;
    if (NULL == info) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    info->npp_ = npp;
    info->closure_number_ = closure_number;
    info->channel_ = channel();
    info->device_audio_ = device_audio_;
    info->context_audio_ = context_audio_;
    config.userData = reinterpret_cast<void*>(info);
    NPError retval =
        device_audio_->initializeContext(npp, &config, context_audio_);
    if (NPERR_NO_ERROR != retval) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }

  return NACL_SRPC_RESULT_OK;
#endif  // defined(NACL_STANDALONE)
}

NaClSrpcError NPModule::AudioDestroy(NPP npp) {
  if (NULL == device_audio_ || NULL == context_audio_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  device_audio_->destroyContext(npp, context_audio_);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModule::AudioGetState(NPP npp,
                                      int32_t state,
                                      int32_t* value) {
  if (NULL == device_audio_ || NULL == context_audio_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  intptr_t value_ptr;
  NPError retval =
      device_audio_->getStateContext(npp, context_audio_, state, &value_ptr);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  *value = static_cast<int32_t>(value_ptr);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModule::AudioSetState(NPP npp,
                                      int32_t state,
                                      int32_t value) {
  if (NULL == device_audio_ || NULL == context_audio_) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  intptr_t value_ptr = static_cast<intptr_t>(value);
  NPError retval =
      device_audio_->setStateContext(npp, context_audio_, state, value_ptr);
  if (NPERR_NO_ERROR != retval) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

}  // namespace nacl
