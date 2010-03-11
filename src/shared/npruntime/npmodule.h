// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPMODULE_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPMODULE_H_

#if !NACL_WINDOWS
#include <pthread.h>
#endif
#include <string.h>
#include <stdlib.h>

#include <string>

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace nacl {

// Represents the plugin end of the connection to the NaCl module. The opposite
// end is the NPNavigator.
class NPModule : public NPBridge {
 public:
  // Creates a new instance of NPModule.
  explicit NPModule(NaClSrpcChannel* channel);
  ~NPModule();

  static NPModule* GetModule(int32_t wire_npp);

  static bool IsWebkit() { return is_webkit; }

  std::string origin() { return origin_; }
  void set_origin(std::string origin) { origin_ = origin; }
  std::string nacl_module_origin() { return nacl_module_origin_; }
  void set_nacl_module_origin(std::string nacl_module_origin) {
    nacl_module_origin_ = nacl_module_origin;
  }

  //
  // Processing calls from the NaCl module to the browser.
  //

  NaClSrpcError Device2DInitialize(NPP npp,
                                   NaClSrpcImcDescType* shm_desc,
                                   int32_t* stride,
                                   int32_t* left,
                                   int32_t* top,
                                   int32_t* right,
                                   int32_t* bottom);
  // Flush may be called from other than the NPAPI thread.
  NaClSrpcError Device2DFlush(NPP npp,
                              int32_t* stride,
                              int32_t* left,
                              int32_t* top,
                              int32_t* right,
                              int32_t* bottom);
  NaClSrpcError Device2DDestroy(NPP npp);
  NaClSrpcError Device3DInitialize(NPP npp,
                                   int32_t entries_requested,
                                   NaClSrpcImcDescType* shm_desc,
                                   int32_t* entries_obtained,
                                   int32_t* get_offset,
                                   int32_t* put_offset);
  // Flush may be called from other than the NPAPI thread.
  NaClSrpcError Device3DFlush(NPP npp,
                              int32_t put_offset,
                              int32_t* get_offset,
                              int32_t* token,
                              int32_t* error);
  NaClSrpcError Device3DDestroy(NPP npp);
  NaClSrpcError Device3DGetState(NPP npp,
                                 int32_t state,
                                 int32_t* value);
  NaClSrpcError Device3DSetState(NPP npp,
                                 int32_t state,
                                 int32_t value);
  NaClSrpcError Device3DCreateBuffer(NPP npp,
                                     int32_t size,
                                     NaClSrpcImcDescType* shm_desc,
                                     int32_t* id);
  NaClSrpcError Device3DDestroyBuffer(NPP npp, int32_t id);

  NaClSrpcError AudioInitialize(NPP npp,
                                int32_t closure_number,
                                int32_t sample_rate,
                                int32_t sample_type,
                                int32_t output_channel_map,
                                int32_t input_channel_map,
                                int32_t sample_frame_count,
                                int32_t flags);
  NaClSrpcError AudioDestroy(NPP npp);
  NaClSrpcError AudioGetState(NPP npp,
                              int32_t state,
                              int32_t* value);
  NaClSrpcError AudioSetState(NPP npp,
                              int32_t state,
                              int32_t value);

  //
  // NPInstance methods
  //

  // Invokes NPP_Initialize() in the child process.
  NPError Initialize();
  // Invokes NPP_New() in the child process.
  NPError New(char* mimetype, NPP npp, int argc, char* argn[], char* argv[]);
  // Processes NPP_Destroy() invocation from the browser.
  NPError Destroy(NPP npp, NPSavedData** save);
  // Processes NPP_SetWindow() invocation from the browser.
  NPError SetWindow(NPP npp, NPWindow* window);
  // Processes NPP_GetValue() invocation from the browser.
  NPError GetValue(NPP npp, NPPVariable variable, void *value);
  // Processes NPP_HandleEvent() invocation from the browser.
  int16_t HandleEvent(NPP npp, void* event);
  // Processes NPP_NewStream() invocation from the browser.
  NPError NewStream(NPP npp,
                    NPMIMEType type,
                    NPStream* stream,
                    NPBool seekable,
                    uint16_t* stype);
  // Send NPP_StreamAsFile to the child process.
  void StreamAsFile(NPP npp, NaClDesc* file, char* url, uint32_t size);
  // Processes NPP_DestroyStream() invocation from the browser.
  NPError DestroyStream(NPP npp, NPStream *stream, NPError reason);
  // Processes NPP_URLNotify() invocation from the browser.
  void URLNotify(NPP npp, const char* url, NPReason reason, void* notify_data);

 private:
  // The proxy NPObject that represents the plugin's scriptable object.
  NPObject* proxy_;

  // The URL origin of the HTML page that started this module.
  std::string origin_;
  // The URL origin of the NaCl module implementing the plugin.
  std::string nacl_module_origin_;

  // The NPWindow of this plugin instance.
  NPWindow* window_;

  // The NaClThread that handles the upcalls for e.g., PluginThreadAsyncCall.
  struct NaClThread upcall_thread_;

  // There are some identifier differences if the browser is based on WebKit.
  static bool is_webkit;

  // Extension state.
  NPExtensions* extensions_;

  // 2D graphics device state.
  NPDevice* device2d_;
  NPDeviceContext2D* context2d_;

  // 3D graphics device state.
  NPDevice* device3d_;
  NPDeviceContext3D* context3d_;

  // Audio device state.
  NPDevice* device_audio_;
  NPDeviceContextAudio* context_audio_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPMODULE_H_
