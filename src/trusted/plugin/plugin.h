/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// The portable representation of an instance and root scriptable object.
// The PPAPI version of the plugin instantiates a subclass of this class.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_

#include <stdio.h>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/api_defines.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace nacl {
class DescWrapper;
class DescWrapperFactory;
}  // namespace nacl

namespace plugin {

class ErrorInfo;
class ScriptableHandle;

class Plugin : public PortableHandle {
 public:
  // Load support.
  // NaCl module can be loaded given a DescWrapper.
  // Updates nacl_module_origin() and nacl_module_url().
  bool LoadNaClModule(nacl::DescWrapper* wrapper, ErrorInfo* error_info);

  // Load support.
  // A helper SRPC NaCl module can be loaded given a DescWrapper.
  // Does not update nacl_module_origin().
  // Returns kInvalidNaClSubprocessId or the ID of the new helper NaCl module.
  NaClSubprocessId LoadHelperNaClModule(nacl::DescWrapper* wrapper,
                                        ErrorInfo* error_info);

  // Returns the argument value for the specified key, or NULL if not found.
  // The callee retains ownership of the result.
  char* LookupArgument(const char* key);

  enum LengthComputable {
    LENGTH_IS_NOT_COMPUTABLE = 0,
    LENGTH_IS_COMPUTABLE = 1
  };
  // Report successful loading of a module.
  virtual void ReportLoadSuccess(LengthComputable length_computable,
                                 uint64_t loaded_bytes,
                                 uint64_t total_bytes) = 0;
  // Report an error that was encountered while loading a module.
  virtual void ReportLoadError(const ErrorInfo& error_info) = 0;
  // Report loading a module was aborted, typically due to user action.
  virtual void ReportLoadAbort() = 0;
  // Dispatch a JavaScript event to indicate a key step in loading.
  // |event_type| is a character string indicating which type of progress
  // event (loadstart, progress, error, abort, load, loadend).
  virtual void EnqueueProgressEvent(const char* event_type,
                                    LengthComputable length_computable,
                                    uint64_t loaded_bytes,
                                    uint64_t total_bytes) = 0;

  // Report the error code that sel_ldr produces when starting a nexe.
  virtual void ReportSelLdrLoadStatus(int status) = 0;

  // Report the error when nexe dies after loading.
  virtual void ReportDeadNexe() = 0;

  // Get the method signature so ScriptableHandle can marshal the inputs
  virtual bool InitParams(uintptr_t method_id,
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

  // URL resolution support.
  // plugin_base_url is the URL used for resolving relative URLs used in
  // src="...".
  nacl::string plugin_base_url() const { return plugin_base_url_; }
  void set_plugin_base_url(nacl::string url) { plugin_base_url_ = url; }
  // manifest_base_url is the URL used for resolving relative URLs mentioned
  // in manifest files.  If the manifest is a data URI, this is an empty string.
  nacl::string manifest_base_url() const { return manifest_base_url_; }
  void set_manifest_base_url(nacl::string url) { manifest_base_url_ = url; }

  // The URL of the manifest file as set by the "src" attribute.
  // It is not the fully resolved URL if it was set as relative.
  const nacl::string& manifest_url() const { return manifest_url_; }
  void set_manifest_url(const nacl::string& manifest_url) {
    manifest_url_ = manifest_url;
  }

  // The state of readiness of the plugin.
  enum ReadyState {
    // The trusted plugin begins in this ready state.
    UNSENT = 0,
    // The manifest file has been requested, but not yet received.
    OPENED = 1,
    // This state is unused.
    HEADERS_RECEIVED = 2,
    // The manifest file has been received and the nexe successfully requested.
    LOADING = 3,
    // The nexe has been loaded and the proxy started, so it is ready for
    // interaction with the page.
    DONE = 4
  };
  ReadyState nacl_ready_state() const { return nacl_ready_state_; }
  void set_nacl_ready_state(ReadyState nacl_ready_state) {
    nacl_ready_state_ = nacl_ready_state;
  }

  // Get the NaCl module subprocess that was assigned the ID |id|.
  NaClSubprocess* nacl_subprocess(NaClSubprocessId id) const {
    if (kInvalidNaClSubprocessId == id) {
      return NULL;
    }
    return nacl_subprocesses_[id];
  }
  NaClSubprocessId next_nacl_subprocess_id() const {
    return static_cast<NaClSubprocessId>(nacl_subprocesses_.size());
  }

  nacl::DescWrapperFactory* wrapper_factory() const { return wrapper_factory_; }

  // Requesting a nacl manifest from a specified url.
  virtual void RequestNaClManifest(const nacl::string& url) = 0;

  // Start up proxied execution of the browser API.
  virtual bool StartProxiedExecution(NaClSrpcChannel* srpc_channel,
                                     ErrorInfo* error_info) = 0;

  // Determines whether experimental APIs are usable.
  static bool ExperimentalJavaScriptApisAreEnabled();

  // Override virtual methods for method and property dispatch.
  virtual bool HasMethod(uintptr_t method_id, CallType call_type);
  virtual bool Invoke(uintptr_t method_id,
                      CallType call_type,
                      SrpcParams* params);
  virtual std::vector<uintptr_t>* GetPropertyIdentifiers() {
    return property_get_methods_.Keys();
  }

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
  // Shuts down socket connection, service runtime, and receive thread,
  // in this order, for all spun up NaCl module subprocesses.
  void ShutDownSubprocesses();

  ScriptableHandle* scriptable_handle() const { return scriptable_handle_; }
  void set_scriptable_handle(ScriptableHandle* scriptable_handle) {
    scriptable_handle_ = scriptable_handle;
  }

  // Access the service runtime for the main NaCl subprocess.
  ServiceRuntime* main_service_runtime() const {
    return main_subprocess_.service_runtime();
  }

  // Setting the properties and methods exported.
  void AddPropertyGet(RpcFunction function_ptr,
                      const char* name,
                      const char* outs);
  void AddPropertySet(RpcFunction function_ptr,
                      const char* name,
                      const char* ins);
  void AddMethodCall(RpcFunction function_ptr,
                     const char* name,
                     const char* ins,
                     const char* outs);

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

  // Keep track of the NaCl module subprocesses that were spun up in the plugin.
  NaClSubprocess main_subprocess_;
  std::vector<NaClSubprocess*> nacl_subprocesses_;

  nacl::string plugin_base_url_;
  nacl::string manifest_base_url_;
  nacl::string manifest_url_;
  ReadyState nacl_ready_state_;

  nacl::DescWrapperFactory* wrapper_factory_;

  MethodMap methods_;
  MethodMap property_get_methods_;
  MethodMap property_set_methods_;

  static bool SendAsyncMessage(void* obj, SrpcParams* params,
                               nacl::DescWrapper** fds, int fds_count);
  static bool SendAsyncMessage0(void* obj, SrpcParams* params);
  static bool SendAsyncMessage1(void* obj, SrpcParams* params);
  // Help load a nacl module, from the file specified in wrapper.
  // This will fully initialize the |subprocess| if the load was successful.
  bool LoadNaClModuleCommon(nacl::DescWrapper* wrapper,
                            NaClSubprocess* subprocess,
                            ErrorInfo* error_info);
  bool StartSrpcServices(NaClSubprocess* subprocess, ErrorInfo* error_info);
  bool StartSrpcServicesCommon(NaClSubprocess* subprocess,
                               ErrorInfo* error_info);
  bool StartJSObjectProxy(NaClSubprocess* subprocess, ErrorInfo* error_info);
  static bool StartSrpcServicesWrapper(void* obj, SrpcParams* params);

  MethodInfo* GetMethodInfo(uintptr_t method_id, CallType call_type);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
