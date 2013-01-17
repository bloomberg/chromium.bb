// -*- c++ -*-
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The portable representation of an instance and root scriptable object.
// The PPAPI version of the plugin instantiates a subclass of this class.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_

#include <stdio.h>

#include <map>
#include <queue>
#include <set>
#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/file_downloader.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/private/var_private.h"
// for pp::VarPrivate
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"

struct NaClSrpcChannel;

namespace nacl {
class DescWrapper;
class DescWrapperFactory;
}  // namespace nacl

namespace pp {
class Find_Dev;
class MouseLock;
class Printing_Dev;
class Selection_Dev;
class URLLoader;
class URLUtil_Dev;
class Zoom_Dev;
}

namespace ppapi_proxy {
class BrowserPpp;
}

namespace plugin {

class ErrorInfo;
class Manifest;
class ProgressEvent;
class ScriptablePlugin;

class Plugin : public pp::InstancePrivate {
 public:
  // Factory method for creation.
  static Plugin* New(PP_Instance instance);

  // ----- Methods inherited from pp::Instance:

  // Initializes this plugin with <embed/object ...> tag attribute count |argc|,
  // names |argn| and values |argn|. Returns false on failure.
  // Gets called by the browser right after New().
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  // Handles document load, when the plugin is a MIME type handler.
  virtual bool HandleDocumentLoad(const pp::URLLoader& url_loader);

  // Returns a scriptable reference to this plugin element.
  // Called by JavaScript document.getElementById(plugin_id).
  virtual pp::Var GetInstanceObject();

  // ----- Plugin interface support.

  // Load support.
  // NaCl module can be loaded given a DescWrapper.
  //
  // Starts NaCl module but does not wait until low-level
  // initialization (e.g., ld.so dynamic loading of manifest files) is
  // done.  The module will become ready later, asynchronously.  Other
  // event handlers should block until the module is ready before
  // trying to communicate with it, i.e., until nacl_ready_state is
  // DONE.
  //
  // NB: currently we do not time out, so if the untrusted code
  // does not signal that it is ready, then we will deadlock the main
  // thread of the renderer on this subsequent event delivery.  We
  // should include a time-out at which point we declare the
  // nacl_ready_state to be done, and let the normal crash detection
  // mechanism(s) take over.
  //
  // Updates nacl_module_origin() and nacl_module_url().
  bool LoadNaClModule(nacl::DescWrapper* wrapper, ErrorInfo* error_info,
                      pp::CompletionCallback init_done_cb,
                      pp::CompletionCallback crash_cb);

  // Finish hooking interfaces up, after low-level initialization is
  // complete.
  bool LoadNaClModuleContinuationIntern(ErrorInfo* error_info);

  // Continuation for starting SRPC/JSProxy services as appropriate.
  // This is invoked as a callback when the NaCl module makes the
  // init_done reverse RPC to tell us that low-level initialization
  // such as ld.so processing is done.  That initialization requires
  // that the main thread be free in order to do Pepper
  // main-thread-only operations such as file processing.
  bool LoadNaClModuleContinuation(int32_t pp_error);

  // Load support.
  // A helper SRPC NaCl module can be loaded given a DescWrapper.
  // Blocks until the helper module signals initialization is done.
  // Does not update nacl_module_origin().
  // Returns NULL or the NaClSubprocess of the new helper NaCl module.
  NaClSubprocess* LoadHelperNaClModule(nacl::DescWrapper* wrapper,
                                       const Manifest* manifest,
                                       ErrorInfo* error_info);

  // Returns the argument value for the specified key, or NULL if not found.
  // The callee retains ownership of the result.
  char* LookupArgument(const char* key);

  enum LengthComputable {
    LENGTH_IS_NOT_COMPUTABLE = 0,
    LENGTH_IS_COMPUTABLE = 1
  };
  // Report successful loading of a module.
  void ReportLoadSuccess(LengthComputable length_computable,
                         uint64_t loaded_bytes,
                         uint64_t total_bytes);
  // Report an error that was encountered while loading a module.
  void ReportLoadError(const ErrorInfo& error_info);
  // Report loading a module was aborted, typically due to user action.
  void ReportLoadAbort();

  // Write a text string on the JavaScript console.
  void AddToConsole(const nacl::string& text);

  // Dispatch a JavaScript event to indicate a key step in loading.
  // |event_type| is a character string indicating which type of progress
  // event (loadstart, progress, error, abort, load, loadend).  Events are
  // enqueued on the JavaScript event loop, which then calls back through
  // DispatchProgressEvent.
  void EnqueueProgressEvent(const char* event_type);
  void EnqueueProgressEvent(const char* event_type,
                            const nacl::string& url,
                            LengthComputable length_computable,
                            uint64_t loaded_bytes,
                            uint64_t total_bytes);

  // Progress event types.
  static const char* const kProgressEventLoadStart;
  static const char* const kProgressEventProgress;
  static const char* const kProgressEventError;
  static const char* const kProgressEventAbort;
  static const char* const kProgressEventLoad;
  static const char* const kProgressEventLoadEnd;
  static const char* const kProgressEventCrash;

  // Report the error code that sel_ldr produces when starting a nexe.
  void ReportSelLdrLoadStatus(int status);

  // Report nexe death after load to JS and shut down the proxy.
  void ReportDeadNexe();

  // The embed/object tag argument list.
  int argc() const { return argc_; }
  char** argn() const { return argn_; }
  char** argv() const { return argv_; }

  Plugin* plugin() const { return const_cast<Plugin*>(this); }

  // URL resolution support.
  // plugin_base_url is the URL used for resolving relative URLs used in
  // src="...".
  nacl::string plugin_base_url() const { return plugin_base_url_; }
  void set_plugin_base_url(const nacl::string& url) { plugin_base_url_ = url; }
  // manifest_base_url is the URL used for resolving relative URLs mentioned
  // in manifest files.  If the manifest is a data URI, this is an empty string.
  nacl::string manifest_base_url() const { return manifest_base_url_; }
  void set_manifest_base_url(const nacl::string& url) {
    manifest_base_url_ = url;
  }

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
  bool nexe_error_reported() const { return nexe_error_reported_; }
  void set_nexe_error_reported(bool val) {
    nexe_error_reported_ = val;
  }

  nacl::DescWrapperFactory* wrapper_factory() const { return wrapper_factory_; }

  // Requests a NaCl manifest download from a |url| relative to the page origin.
  void RequestNaClManifest(const nacl::string& url);

  // Support for property getting.
  typedef void (Plugin::* PropertyGetter)(NaClSrpcArg* prop_value);
  void AddPropertyGet(const nacl::string& prop_name, PropertyGetter getter);
  bool HasProperty(const nacl::string& prop_name);
  bool GetProperty(const nacl::string& prop_name, NaClSrpcArg* prop_value);
  // The supported property getters.
  void GetExitStatus(NaClSrpcArg* prop_value);
  void GetLastError(NaClSrpcArg* prop_value);
  void GetReadyStateProperty(NaClSrpcArg* prop_value);

  // The size returned when a file download operation is unable to determine
  // the size of the file to load.  W3C ProgressEvents specify that unknown
  // sizes return 0.
  static const uint64_t kUnknownBytes = 0;

  // Called back by CallOnMainThread.  Dispatches the first enqueued progress
  // event.
  void DispatchProgressEvent(int32_t result);

  // Requests a URL asynchronously resulting in a call to pp_callback with
  // a PP_Error indicating status. On success an open file descriptor
  // corresponding to the url body is recorded for further lookup.
  bool StreamAsFile(const nacl::string& url,
                    PP_CompletionCallback pp_callback);
  // Returns an open POSIX file descriptor retrieved by StreamAsFile()
  // or NACL_NO_FILE_DESC. The caller must take ownership of the descriptor.
  int32_t GetPOSIXFileDesc(const nacl::string& url);

  // A helper function that gets the scheme type for |url|. Uses URLUtil_Dev
  // interface which this class has as a member.
  UrlSchemeType GetUrlScheme(const std::string& url);

  // Get the text description of the last error reported by the plugin.
  const nacl::string& last_error_string() const { return last_error_string_; }
  void set_last_error_string(const nacl::string& error) {
    last_error_string_ = error;
  }

  // The MIME type used to instantiate this instance of the NaCl plugin.
  // Typically, the MIME type will be application/x-nacl.  However, if the NEXE
  // is being used as a content type handler for another content type (such as
  // PDF), then this function will return that type.
  const nacl::string& mime_type() const { return mime_type_; }
  // The default MIME type for the NaCl plugin.
  static const char* const kNaClMIMEType;
  // Returns true if PPAPI Dev interfaces should be allowed.
  bool enable_dev_interfaces() { return enable_dev_interfaces_; }

  Manifest const* manifest() const { return manifest_.get(); }
  const pp::URLUtil_Dev* url_util() const { return url_util_; }

  // Extracts the exit status from the (main) service runtime.
  int exit_status() const {
    if (NULL == main_service_runtime()) {
      return -1;
    }
    return main_service_runtime()->exit_status();
  }

  const PPB_NaCl_Private* nacl_interface() const { return nacl_interface_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Plugin);
  // Prevent construction and destruction from outside the class:
  // must use factory New() method instead.
  explicit Plugin(PP_Instance instance);
  // The browser will invoke the destructor via the pp::Instance
  // pointer to this object, not from base's Delete().
  ~Plugin();

  bool Init(int argc, char* argn[], char* argv[]);
  // Shuts down socket connection, service runtime, and receive thread,
  // in this order, for the main nacl subprocess.
  void ShutDownSubprocesses();

  ScriptablePlugin* scriptable_plugin() const { return scriptable_plugin_; }
  void set_scriptable_plugin(ScriptablePlugin* scriptable_plugin) {
    scriptable_plugin_ = scriptable_plugin;
  }

  // Access the service runtime for the main NaCl subprocess.
  ServiceRuntime* main_service_runtime() const {
    return main_subprocess_.service_runtime();
  }

  // Help load a nacl module, from the file specified in wrapper.
  // This will fully initialize the |subprocess| if the load was successful.
  bool LoadNaClModuleCommon(nacl::DescWrapper* wrapper,
                            NaClSubprocess* subprocess,
                            const Manifest* manifest,
                            bool should_report_uma,
                            bool uses_irt,
                            bool uses_ppapi,
                            ErrorInfo* error_info,
                            pp::CompletionCallback init_done_cb,
                            pp::CompletionCallback crash_cb);

  // Callback used when getting the URL for the .nexe file.  If the URL loading
  // is successful, the file descriptor is opened and can be passed to sel_ldr
  // with the sandbox on.
  void NexeFileDidOpen(int32_t pp_error);
  void NexeFileDidOpenContinuation(int32_t pp_error);

  // Callback used when the reverse channel closes.  This is an
  // asynchronous event that might turn into a JavaScript error or
  // crash event -- this is controlled by the two state variables
  // nacl_ready_state_ and nexe_error_reported_: If an error or crash
  // had already been reported, no additional crash event is
  // generated.  If no error has been reported but nacl_ready_state_
  // is not DONE, then the loadend event has not been reported, and we
  // enqueue an error event followed by loadend.  If nacl_ready_state_
  // is DONE, then we are in the post-loadend (we need temporal
  // predicate symbols), and we enqueue a crash event.
  void NexeDidCrash(int32_t pp_error);

  // Callback used when a .nexe is translated from bitcode.  If the translation
  // is successful, the file descriptor is opened and can be passed to sel_ldr
  // with the sandbox on.
  void BitcodeDidTranslate(int32_t pp_error);
  void BitcodeDidTranslateContinuation(int32_t pp_error);

  // NaCl ISA selection manifest file support.  The manifest file is specified
  // using the "nacl" attribute in the <embed> tag.  First, the manifest URL (or
  // data: URI) is fetched, then the JSON is parsed.  Once a valid .nexe is
  // chosen for the sandbox ISA, any current service runtime is shut down, the
  // .nexe is loaded and run.

  // Callback used when getting the manifest file as a buffer (e.g., data URIs)
  void NaClManifestBufferReady(int32_t pp_error);

  // Callback used when getting the manifest file as a local file descriptor.
  void NaClManifestFileDidOpen(int32_t pp_error);

  // Processes the JSON manifest string and starts loading the nexe.
  void ProcessNaClManifest(const nacl::string& manifest_json);

  // Parses the JSON in |manifest_json| and retains a Manifest in
  // |manifest_| for use by subsequent resource lookups.
  // On success, |true| is returned and |manifest_| is updated to
  // contain a Manifest that is used by SelectNexeURLFromManifest.
  // On failure, |false| is returned, and |manifest_| is unchanged.
  bool SetManifestObject(const nacl::string& manifest_json,
                         ErrorInfo* error_info);

  // Logs timing information to a UMA histogram, and also logs the same timing
  // information divided by the size of the nexe to another histogram.
  void HistogramStartupTimeSmall(const std::string& name, float dt);
  void HistogramStartupTimeMedium(const std::string& name, float dt);

  // Determines the appropriate nexe for the sandbox and requests a load.
  void RequestNexeLoad();

  // This NEXE is being used as a content type handler rather than directly by
  // an HTML document.
  bool NexeIsContentHandler() const;

  // Callback used when loading a URL for SRPC-based StreamAsFile().
  void UrlDidOpenForStreamAsFile(int32_t pp_error,
                                 FileDownloader*& url_downloader,
                                 PP_CompletionCallback pp_callback);

  // Copy the main service runtime's most recent NaClLog output to the
  // JavaScript console.  Valid to use only after a crash, e.g., via a
  // detail level LOG_FATAL NaClLog entry.  If the crash was not due
  // to a LOG_FATAL this method will do nothing.
  void CopyCrashLogToJsConsole();

  ScriptablePlugin* scriptable_plugin_;

  int argc_;
  char** argn_;
  char** argv_;

  // Keep track of the NaCl module subprocess that was spun up in the plugin.
  NaClSubprocess main_subprocess_;

  nacl::string plugin_base_url_;
  nacl::string manifest_base_url_;
  nacl::string manifest_url_;
  ReadyState nacl_ready_state_;
  bool nexe_error_reported_;  // error or crash reported

  nacl::DescWrapperFactory* wrapper_factory_;

  std::map<nacl::string, PropertyGetter> property_getters_;

  // File download support.  |nexe_downloader_| can be opened with a specific
  // callback to run when the file has been downloaded and is opened for
  // reading.  We use one downloader for all URL downloads to prevent issuing
  // multiple GETs that might arrive out of order.  For example, this will
  // prevent a GET of a NaCl manifest while a .nexe GET is pending.  Note that
  // this will also prevent simultaneous handling of multiple .nexes on a page.
  FileDownloader nexe_downloader_;
  pp::CompletionCallbackFactory<Plugin> callback_factory_;

  nacl::scoped_ptr<PnaclCoordinator> pnacl_coordinator_;

  // The manifest dictionary.  Used for looking up resources to be loaded.
  nacl::scoped_ptr<Manifest> manifest_;
  // URL processing interface for use in looking up resources in manifests.
  const pp::URLUtil_Dev* url_util_;

  // A string containing the text description of the last error
  // produced by this plugin.
  nacl::string last_error_string_;

  // PPAPI Dev interfaces are disabled by default.
  bool enable_dev_interfaces_;

  // If we get a DidChangeView event before the nexe is loaded, we store it and
  // replay it to nexe after it's loaded. We need to replay when this View
  // resource is non-is_null().
  pp::View view_to_replay_;

  // If we get a HandleDocumentLoad event before the nexe is loaded, we store
  // it and replay it to nexe after it's loaded. We need to replay when this
  // URLLoader resource is non-is_null().
  pp::URLLoader document_load_to_replay_;

  nacl::string mime_type_;

  // Keep track of the FileDownloaders created to fetch urls.
  std::set<FileDownloader*> url_downloaders_;
  // Keep track of file descriptors opened by StreamAsFile().
  // These are owned by the browser.
  std::map<nacl::string, int32_t> url_fd_map_;

  // Pending progress events.
  std::queue<ProgressEvent*> progress_events_;

  // Used for NexeFileDidOpenContinuation
  int64_t load_start_;

  int64_t init_time_;
  int64_t ready_time_;
  size_t nexe_size_;

  // Callback to receive .nexe and .dso download progress notifications.
  static void UpdateDownloadProgress(
      PP_Instance pp_instance,
      PP_Resource pp_resource,
      int64_t bytes_sent,
      int64_t total_bytes_to_be_sent,
      int64_t bytes_received,
      int64_t total_bytes_to_be_received);

  // Finds the file downloader which owns the given URL loader. This is used
  // in UpdateDownloadProgress to map a url loader back to the URL being
  // downloaded.
  const FileDownloader* FindFileDownloader(PP_Resource url_loader) const;

  int64_t time_of_last_progress_event_;

  const PPB_NaCl_Private* nacl_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
