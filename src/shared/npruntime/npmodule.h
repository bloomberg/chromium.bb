// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_NPAPI_PLUGIN_NPAPI_BRIDGE_NPMODULE_H_
#define NATIVE_CLIENT_NPAPI_PLUGIN_NPAPI_BRIDGE_NPMODULE_H_

#if !NACL_WINDOWS
#include <pthread.h>
#endif
#include <string.h>
#include <stdlib.h>

#include <string>

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/nprpc.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace nacl {

// Represents the plugin end of the connection to the NaCl module. The opposite
// end is the NPNavigator.
class NPModule : public NPBridge {
  // The proxy NPObject that represents the plugin's scriptable object.
  NPObject* proxy_;

  // The URL origin of the HTML page that started this module.
  std::string origin_;

  // The NPWindow of this plugin instance.
  NPWindow* window_;
  bool origin_valid_;

  // The NaClThread that handles the upcalls for e.g., PluginThreadAsyncCall.
  struct NaClThread upcall_thread_;

  // There are some identifier differences if the browser is based on WebKit.
  static bool IsWebKit;

  // The SRPC methods exported visible to the NaCl module.
  static NACL_SRPC_METHOD_ARRAY(srpc_methods);

  // Extension state.
  NPExtensions* extensions_;

  // 2D graphics device state.
  NPDevice* device2d_;
  NPDeviceContext2D* context2d_;

  // 3D graphics device state.
  NPDevice* device3d_;
  NPDeviceContext3D* context3d_;

 public:
  // Creates a new instance of NPModule.
  explicit NPModule(NaClSrpcChannel* channel);
  ~NPModule();

  // Processes NPN_GetValue() request from the child process.
  static NaClSrpcError GetValue(NaClSrpcChannel* channel,
                                NaClSrpcArg** inputs,
                                NaClSrpcArg** outputs);
  // Processes NPN_Status() request from the child process.
  static NaClSrpcError SetStatus(NaClSrpcChannel* channel,
                                 NaClSrpcArg** inputs,
                                 NaClSrpcArg** outputs);
  // Processes NPN_InvalidateRect() request from the child process.
  static NaClSrpcError InvalidateRect(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs);
  // Processes NPN_ForceRedraw() request from the child process.
  static NaClSrpcError ForceRedraw(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs);
  // Processes NPN_CreateArray() request from the child process.
  static NaClSrpcError CreateArray(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs);
  // Processes NPN_OpenURL() request from the child process.
  static NaClSrpcError OpenURL(NaClSrpcChannel* channel,
                               NaClSrpcArg** inputs,
                               NaClSrpcArg** outputs);
  // Processes NPN_GetIntIdentifier() request from the child process.
  static NaClSrpcError GetIntIdentifier(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs);
  static NaClSrpcError IntFromIdentifier(NaClSrpcChannel* channel,
                                         NaClSrpcArg** inputs,
                                         NaClSrpcArg** outputs);
  // Processes NPN_GetStringIdentifier() request from the child process.
  static NaClSrpcError GetStringIdentifier(NaClSrpcChannel* channel,
                                           NaClSrpcArg** inputs,
                                           NaClSrpcArg** outputs);
  static NaClSrpcError Utf8FromIdentifier(NaClSrpcChannel* channel,
                                          NaClSrpcArg** inputs,
                                          NaClSrpcArg** outputs);
  static NaClSrpcError IdentifierIsString(NaClSrpcChannel* channel,
                                          NaClSrpcArg** inputs,
                                          NaClSrpcArg** outputs);
  static NaClSrpcError Device2DInitialize(NaClSrpcChannel* channel,
                                          NaClSrpcArg** inputs,
                                          NaClSrpcArg** outputs);
  static NaClSrpcError Device2DFlush(NaClSrpcChannel* channel,
                                     NaClSrpcArg** inputs,
                                     NaClSrpcArg** outputs);
  static NaClSrpcError Device2DDestroy(NaClSrpcChannel* channel,
                                       NaClSrpcArg** inputs,
                                       NaClSrpcArg** outputs);
  static NaClSrpcError Device3DInitialize(NaClSrpcChannel* channel,
                                          NaClSrpcArg** inputs,
                                          NaClSrpcArg** outputs);
  static NaClSrpcError Device3DFlush(NaClSrpcChannel* channel,
                                     NaClSrpcArg** inputs,
                                     NaClSrpcArg** outputs);
  static NaClSrpcError Device3DDestroy(NaClSrpcChannel* channel,
                                       NaClSrpcArg** inputs,
                                       NaClSrpcArg** outputs);
  static NaClSrpcError Device3DGetState(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs);
  static NaClSrpcError Device3DSetState(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs);
  static NaClSrpcError Device3DCreateBuffer(NaClSrpcChannel* channel,
                                            NaClSrpcArg** inputs,
                                            NaClSrpcArg** outputs);
  static NaClSrpcError Device3DDestroyBuffer(NaClSrpcChannel* channel,
                                             NaClSrpcArg** inputs,
                                             NaClSrpcArg** outputs);

  // Invokes NPP_Initialize() in the child process.
  NPError Initialize();

  // Invokes NPP_New() in the child process.
  NPError New(char* mimetype, NPP npp, int argc, char* argn[], char* argv[]);

  //
  // NPInstance methods
  //

  // Processes NPP_Destroy() invocation from the browser.
  NPError Destroy(NPP npp, NPSavedData** save);
  // Processes NPP_SetWindow() invocation from the browser.
  NPError SetWindow(NPP npp, NPWindow* window);
  // Processes NPP_GetValue() invocation from the browser.
  NPError GetValue(NPP npp, NPPVariable variable, void *value);
  // Processes NPP_HandleEvent() invocation from the browser.
  int16_t HandleEvent(NPP npp, void* event);
  // Processes NPP_GetScriptableInstance() invocation from the browser.
  NPObject* GetScriptableInstance(NPP npp);
  // Processes NPP_NewStream() invocation from the browser.
  NPError NewStream(NPP npp,
                    NPMIMEType type,
                    NPStream* stream,
                    NPBool seekable,
                    uint16_t* stype);
  // Processes NPP_StreamAsFile() invocation from the browser.
  void StreamAsFile(NPP npp, NPStream* stream, const char* filename);
  // Processes NPP_DestroyStream() invocation from the browser.
  NPError DestroyStream(NPP npp, NPStream *stream, NPError reason);
  // Processes NPP_URLNotify() invocation from the browser.
  void URLNotify(NPP npp, const char* url, NPReason reason, void* notify_data);
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_NPAPI_PLUGIN_NPAPI_BRIDGE_NPMODULE_H_
