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
  explicit Plugin(PP_Instance instance);

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
                                       int32_t manifest_id,
                                       ErrorInfo* error_info);

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
  void EnqueueProgressEvent(PP_NaClEventType event_type,
                            const nacl::string& url,
                            LengthComputable length_computable,
                            uint64_t loaded_bytes,
                            uint64_t total_bytes);

  // Report the error code that sel_ldr produces when starting a nexe.
  void ReportSelLdrLoadStatus(int status);

  nacl::DescWrapperFactory* wrapper_factory() const { return wrapper_factory_; }

  // Requests a NaCl manifest download from a |url| relative to the page origin.
  void RequestNaClManifest(const nacl::string& url);

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

  // set_exit_status may be called off the main thread.
  void set_exit_status(int exit_status);

  const PPB_NaCl_Private* nacl_interface() const { return nacl_interface_; }
  pp::UMAPrivate& uma_interface() { return uma_interface_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Plugin);
  // The browser will invoke the destructor via the pp::Instance
  // pointer to this object, not from base's Delete().
  ~Plugin();

  // Shuts down socket connection, service runtime, and receive thread,
  // in this order, for the main nacl subprocess.
  void ShutDownSubprocesses();

  // Histogram helper functions, internal to Plugin so they can use
  // uma_interface_ normally.
  void HistogramTimeSmall(const std::string& name, int64_t ms);
  void HistogramEnumerateLoadStatus(PP_NaClError error_code);
  void HistogramEnumerateSelLdrLoadStatus(NaClErrorCode error_code);

  // Load a nacl module from the file specified in wrapper.
  // Only to be used from a background (non-main) thread.
  // This will fully initialize the |subprocess| if the load was successful.
  bool LoadNaClModuleFromBackgroundThread(nacl::DescWrapper* wrapper,
                                          NaClSubprocess* subprocess,
                                          int32_t manifest_id,
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

  // Callback used when getting the manifest file as a local file descriptor.
  void NaClManifestFileDidOpen(int32_t pp_error);

  // Processes the JSON manifest string and starts loading the nexe.
  void ProcessNaClManifest(const nacl::string& manifest_json);

  // Logs timing information to a UMA histogram, and also logs the same timing
  // information divided by the size of the nexe to another histogram.
  void HistogramStartupTimeSmall(const std::string& name, float dt);
  void HistogramStartupTimeMedium(const std::string& name, float dt);

  // Callback used when loading a URL for SRPC-based StreamAsFile().
  void UrlDidOpenForStreamAsFile(int32_t pp_error,
                                 FileDownloader* url_downloader,
                                 pp::CompletionCallback pp_callback);

  // Open an app file by requesting a file descriptor from the browser. This
  // method first checks that the url is for an installed file before making the
  // request so it won't slow down non-installed file downloads.
  bool OpenURLFast(const nacl::string& url, FileDownloader* downloader);

  void SetExitStatusOnMainThread(int32_t pp_error, int exit_status);

  // Keep track of the NaCl module subprocess that was spun up in the plugin.
  NaClSubprocess main_subprocess_;

  bool uses_nonsfi_mode_;

  nacl::DescWrapperFactory* wrapper_factory_;

  // Original, unresolved URL for the .nexe program to load.
  std::string program_url_;

  pp::CompletionCallbackFactory<Plugin> callback_factory_;

  nacl::scoped_ptr<PnaclCoordinator> pnacl_coordinator_;

  // Keep track of the FileDownloaders created to fetch urls.
  std::set<FileDownloader*> url_downloaders_;
  // Keep track of file descriptors opened by StreamAsFile().
  // These are owned by the browser.
  std::map<nacl::string, NaClFileInfoAutoCloser*> url_file_info_map_;

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

  int32_t manifest_id_;

  PP_FileHandle nexe_handle_;

  const PPB_NaCl_Private* nacl_interface_;
  pp::UMAPrivate uma_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PLUGIN_H_
