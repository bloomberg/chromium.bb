/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// TODO(sehr): this file is the NPAPI interface exported by the plugin.
// It should be moved to a location that indicates that and renamed.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/npapi/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

struct NaClGioShmUnbounded;
struct NaClDesc;

// Stub class declarations to avoid excessive includes.
namespace nacl_srpc {
class Plugin;
class SharedMemory;
}  // namespace nacl_srpc

namespace nacl {

class VideoMap;
class NPModule;

class StreamShmBuffer {
 public:
  StreamShmBuffer();
  ~StreamShmBuffer();
  int32_t read(int32_t offset, int32_t len, void* buf);
  int32_t write(int32_t offset, int32_t len, void* buf);
  NaClDesc* shm(int32_t* size);
 private:
  NaClGioShmUnbounded *shmbufp_;
};

class SRPC_Plugin : public NPInstance, public BrowserInterface {
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
                int32_t offset,
                int32_t len,
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
  virtual char** argn() { return npapi_argn_; }
  virtual char** argv() { return npapi_argv_; }
  virtual int argc() { return npapi_argc_; }

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
