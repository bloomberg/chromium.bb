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
