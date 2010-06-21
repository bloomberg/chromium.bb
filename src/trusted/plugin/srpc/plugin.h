/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// The portable representation of an instance and root scriptable object.
// The ActiveX, NPAPI, and Pepper versions of the plugin instantiate
// subclasses of this class.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PLUGIN_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PLUGIN_H_

#include <stdio.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/api_defines.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
// #include "native_client/src/trusted/plugin/srpc/video.h"

namespace nacl {
class DescWrapperFactory;
}  // namespace nacl

namespace plugin {

class StreamShmBuffer;
class ServiceRuntimeInterface;
class ScriptableHandle;
class VideoMap;

class Plugin : public PortableHandle {
 public:
  // Create new Plugin objects.  Returns NULL on failure.
  static Plugin* New(BrowserInterface* browser_interface,
                     InstanceIdentifier instance_id,
                     int argc,
                     char* argn[],
                     char* argv[]);

  // Load from the local URL saved in local_url.
  // Updates local_url(), nacl_module_origin() and logical_url().
  bool Load(nacl::string logical_url, const char* local_url);
  // Load nexe binary from the provided buffer.
  // Updates local_url(), nacl_module_origin() and logical_url().
  bool Load(nacl::string logical_url,
            const char* local_url,
            plugin::StreamShmBuffer *buffer);

  // Log a message by sending it to the service runtime.
  bool LogAtServiceRuntime(int severity, nacl::string msg);

  // Returns the argument value for the specified key, or NULL if not found.
  // The callee retains ownership of the result.
  char* LookupArgument(const char* key);

  // To indicate successful loading of a module, invoke the onload handler.
  bool RunOnloadHandler();
  // To indicate unsuccessful loading of a module, invoke the onfail handler.
  bool RunOnfailHandler();

  // overriding virtual methods
  virtual bool InvokeEx(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params);
  virtual bool HasMethodEx(uintptr_t method_id, CallType call_type);
  virtual bool InitParamsEx(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params);

  // The unique identifier for this plugin instance.
  virtual InstanceIdentifier instance_id() { return instance_id_; }

  // The embed/object tag argument list.
  int argc() const { return argc_; }
  char** argn() const { return argn_; }
  char** argv() const { return argv_; }

  virtual BrowserInterface* browser_interface() const {
    return browser_interface_;
  }
  virtual Plugin* plugin() const { return const_cast<Plugin*>(this); }
  ScriptableHandle* scriptable_handle() const { return scriptable_handle_; }
  void set_scriptable_handle(ScriptableHandle* scriptable_handle) {
    scriptable_handle_ = scriptable_handle;
  }

  // Returns the URL of the locally-cached copy of the downloaded NaCl
  // module, if any.  May be NULL, e.g. in Chrome.
  const char* local_url() const { return local_url_; }
  void set_local_url(const char *url);

  // Returns the logical URL of NaCl module as defined by the "src" or
  // "nexes" attribute (and thus seen by the user).
  // TODO(sehr): this is derived from what the NPStream object reports, and
  // we need to investigate how streams report redirection (or if they do)
  // for our origin checks, etc.
  const char* logical_url() const { return logical_url_; }
  void set_logical_url(const char *url);

  // Origin of page with the embed tag that created this plugin instance.
  nacl::string origin() const { return origin_; }

  // Origin of NaCl module.
  nacl::string nacl_module_origin() const { return nacl_module_origin_; }
  void set_nacl_module_origin(nacl::string origin) {
    nacl_module_origin_ = origin;
  }

  // The multimedia interface's video support.
  virtual VideoMap* video() const { return video_; }

  // Each nexe has a canonical socket address that it will respond to
  // Connect requests on.
  ScriptableHandle* socket_address() const { return socket_address_; }
  // TODO(sehr): document this.
  ScriptableHandle* socket() const { return socket_; }

  // Width is the width in pixels of the region this instance occupies.
  int32_t width() const { return width_; }
  void set_width(int32_t width) { width_ = width; }

  // Height is the height in pixels of the region this instance occupies.
  int32_t height() const { return height_; }
  void set_height(int32_t height) { height_ = height; }

  // Video_update_mode tells how the multimedia API should update its window.
  int32_t video_update_mode() const { return video_update_mode_; }
  void set_video_update_mode(int32_t video_update_mode) {
    video_update_mode_ = video_update_mode;
  }

  nacl::DescWrapperFactory* wrapper_factory() const { return wrapper_factory_; }

  // Complex methods to set member data.
  bool SetNexesPropertyImpl(const char* nexes_attrs);
  bool SetSrcPropertyImpl(const nacl::string &url);

 protected:
  Plugin();
  virtual ~Plugin();
  bool Init(BrowserInterface* browser_interface,
            InstanceIdentifier instance_id,
            int argc,
            char* argn[],
            char* argv[]);
  void LoadMethods();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Plugin);
  InstanceIdentifier instance_id_;
  BrowserInterface* browser_interface_;
  ScriptableHandle* scriptable_handle_;

  int argc_;
  char** argn_;
  char** argv_;

  VideoMap* video_;
  ScriptableHandle* socket_address_;
  ScriptableHandle* socket_;
  ServiceRuntimeInterface* service_runtime_interface_;
  char* local_url_;  // (from malloc)
  char* logical_url_;  // (from malloc)

  nacl::string origin_;
  nacl::string nacl_module_origin_;

  bool origin_valid_;
  int32_t height_;
  int32_t width_;
  int32_t video_update_mode_;

  nacl::DescWrapperFactory* wrapper_factory_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PLUGIN_H_
