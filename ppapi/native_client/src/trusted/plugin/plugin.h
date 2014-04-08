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
#include "native_client/src/trusted/validator/nacl_file_info.h"

#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/uma_private.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"

#include "ppapi/native_client/src/trusted/plugin/file_downloader.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_subprocess.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "ppapi/native_client/src/trusted/plugin/service_runtime.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace nacl {
class DescWrapper;
class DescWrapperFactory;
}  // namespace nacl

namespace pp {
class CompletionCallback;
class URLLoader;
class URLUtil_Dev;
}

namespace plugin {

class ErrorInfo;
class Manifest;

class Plugin : public pp::Instance {
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
  void LoadNaClModule(nacl::DescWrapper* wrapper,
                      bool uses_nonsfi_mode,
                      bool enable_dyncode_syscalls,
                      bool enable_exception_handling,
                      bool enable_crash_throttling,
                      const pp::CompletionCallback& init_done_cb,
                      const pp::CompletionCallback& crash_cb);

  // Finish hooking interfaces up, after low-level initialization is
  // complete.
  bool LoadNaClModuleContinuationIntern();

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
  NaClSubprocess* LoadHelperNaClModule(const nacl::string& helper_url,
                                       nacl::DescWrapper* wrapper,
                                       const Manifest* manifest,
                                       ErrorInfo* error_info);

  // Returns the argument value for the specified key, or NULL if not found.
  std::string LookupArgument(const std::string& key) const;

  enum LengthComputable {
    LENGTH_IS_NOT_COMPUTABLE = 0,
    LENGTH_IS_COMPUTABLE = 1
  };
  // Report successful loading of a module.
  void ReportLoadSuccess(uint64_t loaded_bytes, uint64_t total_bytes);
  // Report an error that was encountered while loading a module.
  void ReportLoadError(const ErrorInfo& error_info);
  // Report loading a module was aborted, typically due to user action.
  void ReportLoadAbort();

  // Dispatch a JavaScript event to indicate a key step in loading.
  // |event_type| is a character string indicating which type of progress
  // event (loadstart, progress, error, abort, load, loadend).  Events are
  // enqueued on the JavaScript event loop, which then calls back through
  // DispatchProgressEvent.
  void EnqueueProgressEvent(PP_NaClEventType event_type);
  void EnqueueProgressEvent(PP_NaClEventType event_type,
                            const nacl::string& url,
                            LengthComputable length_computable,
                            uint64_t loaded_bytes,
                            uint64_t total_bytes);

  // Report the error code that sel_ldr produces when starting a nexe.
  void ReportSelLdrLoadStatus(int status);

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

  nacl::DescWrapperFactory* wrapper_factory() const { return wrapper_factory_; }

  // Requests a NaCl manifest download from a |url| relative to the page origin.
  void RequestNaClManifest(const nacl::string& url);

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
                    const pp::CompletionCallback& callback);

  // Returns rich information for a file retrieved by StreamAsFile(). This info
  // contains a file descriptor. The caller must take ownership of this
  // descriptor.
  struct NaClFileInfo GetFileInfo(const nacl::string& url);

  // A helper function that indicates if |url| can be requested by the document
  // under the same-origin policy. Strictly speaking, it may be possible for the
  // document to request the URL using CORS even if this function returns false.
  bool DocumentCanRequest(const std::string& url);

  // The MIME type used to instantiate this instance of the NaCl plugin.
  // Typically, the MIME type will be application/x-nacl.  However, if the NEXE
  // is being used as a content type handler for another content type (such as
  // PDF), then this function will return that type.
  const nacl::string& mime_type() const { return mime_type_; }
  // The default MIME type for the NaCl plugin.
  static const char* const kNaClMIMEType;
  // The MIME type for the plugin when using PNaCl.
  static const char* const kPnaclMIMEType;
  // Returns true if PPAPI Dev interfaces should be allowed.
  bool enable_dev_interfaces() { return enable_dev_interfaces_; }

  Manifest const* manifest() const { return manifest_.get(); }
  const pp::URLUtil_Dev* url_util() const { return url_util_; }

  // set_exit_status may be called off the main thread.
  void set_exit_status(int exit_status);

  const PPB_NaCl_Private* nacl_interface() const { return nacl_interface_; }
  pp::UMAPrivate& uma_interface() { return uma_interface_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Plugin);
  // Prevent construction and destruction from outside the class:
  // must use factory New() method instead.
  explicit Plugin(PP_Instance instance);
  // The browser will invoke the destructor via the pp::Instance
  // pointer to this object, not from base's Delete().
  ~Plugin();

  bool EarlyInit(int argc, const char* argn[], const char* argv[]);
  // Shuts down socket connection, service runtime, and receive thread,
  // in this order, for the main nacl subprocess.
  void ShutDownSubprocesses();

  // Access the service runtime for the main NaCl subprocess.
  ServiceRuntime* main_service_runtime() const {
    return main_subprocess_.service_runtime();
  }

  // Histogram helper functions, internal to Plugin so they can use
  // uma_interface_ normally.
  void HistogramTimeSmall(const std::string& name, int64_t ms);
  void HistogramTimeMedium(const std::string& name, int64_t ms);
  void HistogramTimeLarge(const std::string& name, int64_t ms);
  void HistogramSizeKB(const std::string& name, int32_t sample);
  void HistogramEnumerate(const std::string& name,
                          int sample,
                          int maximum,
                          int out_of_range_replacement);
  void HistogramEnumerateOsArch(const std::string& sandbox_isa);
  void HistogramEnumerateLoadStatus(PP_NaClError error_code);
  void HistogramEnumerateSelLdrLoadStatus(NaClErrorCode error_code);
  void HistogramEnumerateManifestIsDataURI(bool is_data_uri);
  void HistogramHTTPStatusCode(const std::string& name, int status);

  // Load a nacl module from the file specified in wrapper.
  // Only to be used from a background (non-main) thread.
  // This will fully initialize the |subprocess| if the load was successful.
  bool LoadNaClModuleFromBackgroundThread(nacl::DescWrapper* wrapper,
                                          NaClSubprocess* subprocess,
                                          const Manifest* manifest,
                                          const SelLdrStartParams& params);

  // Start sel_ldr from the main thread, given the start params.
  // |pp_error| is set by CallOnMainThread (should be PP_OK).
  void StartSelLdrOnMainThread(int32_t pp_error,
                               ServiceRuntime* service_runtime,
                               const SelLdrStartParams& params,
                               pp::CompletionCallback callback);

  // Signals that StartSelLdr has finished.
  void SignalStartSelLdrDone(int32_t pp_error,
                             bool* started,
                             ServiceRuntime* service_runtime);

  void LoadNexeAndStart(int32_t pp_error,
                        nacl::DescWrapper* wrapper,
                        ServiceRuntime* service_runtime,
                        const pp::CompletionCallback& crash_cb);

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

  // This NEXE is being used as a content type handler rather than directly by
  // an HTML document.
  bool NexeIsContentHandler() const;

  // Callback used when loading a URL for SRPC-based StreamAsFile().
  void UrlDidOpenForStreamAsFile(int32_t pp_error,
                                 FileDownloader* url_downloader,
                                 pp::CompletionCallback pp_callback);

  // Open an app file by requesting a file descriptor from the browser. This
  // method first checks that the url is for an installed file before making the
  // request so it won't slow down non-installed file downloads.
  bool OpenURLFast(const nacl::string& url, FileDownloader* downloader);

  void SetExitStatusOnMainThread(int32_t pp_error, int exit_status);

  std::map<std::string, std::string> args_;

  // Keep track of the NaCl module subprocess that was spun up in the plugin.
  NaClSubprocess main_subprocess_;

  nacl::string plugin_base_url_;
  nacl::string manifest_base_url_;
  nacl::string manifest_url_;
  bool uses_nonsfi_mode_;
  bool nexe_error_reported_;  // error or crash reported

  nacl::DescWrapperFactory* wrapper_factory_;

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

  // PPAPI Dev interfaces are disabled by default.
  bool enable_dev_interfaces_;

  // A flag indicating if the NaCl executable is being loaded from an installed
  // application.  This flag is used to bucket UMA statistics more precisely to
  // help determine whether nexe loading problems are caused by networking
  // issues.  (Installed applications will be loaded from disk.)
  // Unfortunately, the definition of what it means to be part of an installed
  // application is a little murky - for example an installed application can
  // register a mime handler that loads NaCl executables into an arbitrary web
  // page.  As such, the flag actually means "our best guess, based on the URLs
  // for NaCl resources that we have seen so far".
  bool is_installed_;

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
  std::map<nacl::string, NaClFileInfoAutoCloser*> url_file_info_map_;

  // Used for NexeFileDidOpenContinuation
  int64_t load_start_;

  int64_t init_time_;
  int64_t ready_time_;

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
  int exit_status_;

  const PPB_NaCl_Private* nacl_interface_;
  pp::UMAPrivate uma_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
