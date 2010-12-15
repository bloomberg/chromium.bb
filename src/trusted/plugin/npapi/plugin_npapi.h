/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_PLUGIN_NPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_PLUGIN_NPAPI_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/npapi/npinstance.h"
#include "native_client/src/trusted/plugin/plugin.h"

namespace nacl {

class NPModule;

}  // namespace

namespace plugin {

class MultimediaSocket;
class VideoMap;

class PluginNpapi : public nacl::NPInstance, public Plugin {
 public:
  static PluginNpapi* New(NPP npp,
                          int argc,
                          char* argn[],
                          char* argv[]);

  // Each of the following methods corresponds to an NPP_ method in NPAPI.
  // Please consult the relevant Mozilla documentation for details.
  // See https://developer.mozilla.org/En/NPP_<method_name>.
  NPError Destroy(NPSavedData** save);
  NPError SetWindow(NPWindow* window);
  NPError GetValue(NPPVariable variable, void* value);
  int16_t HandleEvent(void* event);
  NPError NewStream(NPMIMEType type,
                    NPStream* stream,
                    NPBool seekable,
                    uint16_t* stype);
  int32_t WriteReady(NPStream* stream);
  int32_t Write(NPStream* stream,
                int32_t offset,
                int32_t len,
                void* buf);

  void StreamAsFile(NPStream* stream, const char* filename);
  NPError DestroyStream(NPStream* stream, NPError reason);
  void URLNotify(const char* url, NPReason reason, void* notify_data);

  // Request a nacl module download.
  bool RequestNaClModule(const nacl::string& url);

  // These are "constants" initialized when a PluginNpapi has been created.
  static NPIdentifier kHrefIdent;
  static NPIdentifier kLengthIdent;
  static NPIdentifier kLocationIdent;

  // The multimedia API's video interface.
  VideoMap* video() const { return video_; }
  virtual bool InitializeModuleMultimedia(ScriptableHandle* raw_channel,
                                          ServiceRuntime* service_runtime);
  void ShutdownMultimedia();

  // Support for the NaCl NPAPI proxy.
  nacl::NPModule* module() const { return module_; }
  void set_module(nacl::NPModule* module);
  NPObject* nacl_instance() const { return nacl_instance_; }
  void StartProxiedExecution(NaClSrpcChannel* srpc_channel);

  static bool SetAsyncCallback(void* obj, SrpcParams* params);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginNpapi);
  PluginNpapi();
  ~PluginNpapi();
  nacl::NPModule* module_;
  NPObject* nacl_instance_;
  VideoMap* video_;
  MultimediaSocket* multimedia_channel_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_PLUGIN_NPAPI_H_
