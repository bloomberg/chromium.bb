/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

// Stub class declarations to avoid excessive includes.
namespace nacl_srpc {
class Plugin;
class SharedMemory;
}  // namespace nacl_srpc

namespace nacl {

class VideoMap;
class NPModule;

class StreamBuffer {
 public:
  explicit StreamBuffer(NPStream* stream);
  ~StreamBuffer() { free(buffer_); }
  NPStream* get_stream() { return stream_id_; }
  int32_t write(int32_t offset, int32_t len, void *buf);
  int32_t size() { return current_size_; }
  void* get_buffer() { return buffer_; }
 private:
  void *buffer_;
  int32_t current_size_;
  NPStream *stream_id_;
};

class SRPC_Plugin : public NPInstance, public PortablePluginInterface {
 public:
  explicit SRPC_Plugin(NPP npp, int argc, char* argn[], char* argv[]);

  ~SRPC_Plugin();

  NPError Destroy(NPSavedData** save);
  NPError SetWindow(NPWindow* window);
  NPError GetValue(NPPVariable variable, void *value);
  int16_t HandleEvent(void* event);
  NPObject* GetScriptableInstance();
  NPError NewStream(NPMIMEType type,
                    NPStream* stream,
                    NPBool seekable,
                    uint16_t* stype);
  int32_t WriteReady(NPStream* stream);
  int32_t Write(NPStream* stream,
                int32 offset,
                int32 len,
                void* buf);

  void StreamAsFile(NPStream* stream, const char* filename);
  NPError DestroyStream(NPStream *stream, NPError reason);
  void URLNotify(const char* url, NPReason reason, void* notify_data);
  NPP GetNPP() { return npp_; }

  virtual nacl_srpc::PluginIdentifier GetPluginIdentifier() { return GetNPP(); }
  virtual VideoMap* video() { return video_; }
  nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>* plugin() { return plugin_; }
  // NPAPI proxy support.
  virtual nacl::NPModule* module() { return module_; }
  virtual void set_module(nacl::NPModule* module);
  NPObject* nacl_instance() { return nacl_instance_; }
  char** argn() { return npapi_argn_; }
  char** argv() { return npapi_argv_; }
  int argc() { return npapi_argc_; }

 private:
  NPP npp_;
  nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>* plugin_;
  VideoMap* video_;
  // NPAPI proxy support.
  NPModule* module_;
  NPObject* nacl_instance_;
  char** npapi_argn_;
  char** npapi_argv_;
  int npapi_argc_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_
