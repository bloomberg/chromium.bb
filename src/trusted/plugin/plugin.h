/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// The portable representation of an instance and root scriptable object.
// The ActiveX, NPAPI, and Pepper versions of the plugin instantiate
// subclasses of this class.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_

#include <stdio.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/api_defines.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace nacl {
class DescWrapper;
class DescWrapperFactory;
}  // namespace nacl

namespace plugin {

class ScriptableHandle;

class Plugin : public PortableHandle {
 public:
  void Invalidate();

  // Check that full_url meets the origin requirements for loading a nexe.
  bool IsValidNexeOrigin(nacl::string full_url,
                         nacl::string local_path,
                         nacl::string* error_string);

  // Load support.
  // NaCl module can be loaded given a DescWrapper.
  // Updates nacl_module_origin() and nacl_module_url().
  bool LoadNaClModule(nacl::DescWrapper* wrapper, nacl::string* error_string);

  // Returns the argument value for the specified key, or NULL if not found.
  // The callee retains ownership of the result.
  char* LookupArgument(const char* key);

  // Report successful loading of a module.
  virtual void ReportLoadSuccess(bool length_computable,
                                 uint64_t loaded_bytes,
                                 uint64_t total_bytes) = 0;
  // Report an error that was encountered while loading a module.
  // TODO(sehr,polina): make this an error code rather than a string.
  virtual void ReportLoadError(const nacl::string& error) = 0;
  // Report loading a module was aborted, typically due to user action.
  virtual void ReportLoadAbort() = 0;
  // Dispatch a JavaScript event to indicate a key step in loading.
  // |event_type| is a character string indicating which type of progress
  // event (loadstart, progress, error, abort, load, loadend).
  virtual void DispatchProgressEvent(const char* event_type,
                                     bool length_computable,
                                     uint64_t loaded_bytes,
                                     uint64_t total_bytes) = 0;

  // overriding virtual methods
  virtual bool InvokeEx(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params);
  virtual bool HasMethodEx(uintptr_t method_id, CallType call_type);
  virtual bool InitParamsEx(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params);

  // The unique identifier for this plugin instance.
  InstanceIdentifier instance_id() const { return instance_id_; }

  // The embed/object tag argument list.
  int argc() const { return argc_; }
  char** argn() const { return argn_; }
  char** argv() const { return argv_; }

  virtual BrowserInterface* browser_interface() const {
    return browser_interface_;
  }
  virtual Plugin* plugin() const { return const_cast<Plugin*>(this); }

  // Origin of the page with the <embed> tag that created this plugin instance.
  nacl::string origin() const { return origin_; }

  // The full path of the locally-cached copy of the downloaded NaCl
  // module, if any. May be unset, e.g. in Chrome. Set by LoadNaclModule().
  nacl::string nacl_module_path() const { return nacl_module_path_; }
  void set_nacl_module_path(nacl::string path) { nacl_module_path_ = path; }

  // The full URL of the NaCl module as defined by the "src" or (indirectly) by
  // the "nacl" attribute (and thus seen by the user). Set by LoadNaclModule().
  nacl::string nacl_module_url() const { return nacl_module_url_; }
  void set_nacl_module_url(nacl::string url) { nacl_module_url_ = url; }

  // The origin of the NaCl module.
  nacl::string nacl_module_origin() const { return nacl_module_origin_; }
  void set_nacl_module_origin(nacl::string origin) {
    nacl_module_origin_ = origin;
  }

  // The full URL of the manifest file as set by the "nacl" attribute.  This is
  // a read-only property.
  const nacl::string& nacl_manifest_url() const { return nacl_manifest_url_; }
  void set_nacl_manifest_url(const nacl::string& manifest_url) {
    nacl_manifest_url_ = manifest_url;
  }

  // Set when connection and proxy are available.
  bool nacl_module_ready() const { return nacl_module_ready_; }
  void set_nacl_module_ready(bool nacl_module_ready) {
    nacl_module_ready_ = nacl_module_ready;
  }

  // Each nexe has a canonical socket address that it will respond to
  // Connect requests on.
  ScriptableHandle* socket_address() const { return socket_address_; }
  // TODO(sehr): document this.
  ScriptableHandle* socket() const { return socket_; }

  // Shut down the multimedia system, destroying the listener thread.
  virtual void ShutdownMultimedia() { }

  // Width is the width in pixels of the region this instance occupies.
  int32_t width() const { return width_; }
  void set_width(int32_t width) { width_ = width; }

  // Height is the height in pixels of the region this instance occupies.
  int32_t height() const { return height_; }
  void set_height(int32_t height) { height_ = height; }

  nacl::DescWrapperFactory* wrapper_factory() const { return wrapper_factory_; }

  // Complex method to set member data.
  void SetSrcPropertyImpl(const nacl::string &url);

  // Requesting a nacl manifest from a specified url.
  virtual void RequestNaClManifest(const nacl::string& url) = 0;

  // Start up proxied execution of the browser API.
  virtual bool StartProxiedExecution(NaClSrpcChannel* srpc_channel,
                                     nacl::string* error_string) = 0;

  // Determines whether experimental APIs are usable.
  static bool ExperimentalJavaScriptApisAreEnabled();

  // The size returned when a file download operation is unable to determine
  // the size of the file to load.  W3C ProgressEvents specify that unknown
  // sizes return 0.
  static const uint64_t kUnknownBytes = 0;

 protected:
  Plugin();
  virtual ~Plugin();
  bool Init(BrowserInterface* browser_interface,
            InstanceIdentifier instance_id,
            int argc,
            char* argn[],
            char* argv[]);
  void LoadMethods();
  // Shuts down socket connection, service runtime, multimedia and
  // receive thread, in this order.
  void ShutDownSubprocess();

  static void UnrefScriptableHandle(ScriptableHandle** handle);

  ScriptableHandle* scriptable_handle() const { return scriptable_handle_; }
  void set_scriptable_handle(ScriptableHandle* scriptable_handle) {
    scriptable_handle_ = scriptable_handle;
  }

  ServiceRuntime* service_runtime_;

  bool receive_thread_running_;
  struct NaClThread receive_thread_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Plugin);

  InstanceIdentifier instance_id_;
  BrowserInterface* browser_interface_;
  ScriptableHandle* scriptable_handle_;

  int argc_;
  char** argn_;
  char** argv_;

  // These will be taken over from service_runtime_ on load.
  ScriptableHandle* socket_address_;
  ScriptableHandle* socket_;

  nacl::string origin_;
  bool origin_valid_;

  nacl::string nacl_manifest_url_;
  nacl::string nacl_module_path_;
  nacl::string nacl_module_url_;
  nacl::string nacl_module_origin_;
  bool nacl_module_ready_;

  int32_t height_;
  int32_t width_;
  int32_t video_update_mode_;

  nacl::DescWrapperFactory* wrapper_factory_;

  static bool SendAsyncMessage(void* obj, SrpcParams* params,
                               nacl::DescWrapper** fds, int fds_count);
  static bool SendAsyncMessage0(void* obj, SrpcParams* params);
  static bool SendAsyncMessage1(void* obj, SrpcParams* params);
  bool StartSrpcServices(nacl::string* error);
  static bool StartSrpcServicesWrapper(void* obj, SrpcParams* params);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
