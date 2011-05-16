/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// PPAPI-based implementation of the interface for a NaCl plugin instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_

#include <map>
#include <set>
#include <string>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/ppapi/file_downloader.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"

struct NaClSrpcChannel;
struct NaClDesc;
namespace ppapi_proxy {
class BrowserPpp;
}

namespace pp {
class Find_Dev;
class Printing_Dev;
class Selection_Dev;
class URLLoader;
class WidgetClient_Dev;
class Zoom_Dev;
}

namespace plugin {

// Encapsulates a PPAPI NaCl plugin.
class PluginPpapi : public pp::Instance, public Plugin {
 public:
  // Factory method for creation.
  static PluginPpapi* New(PP_Instance instance);

  // ----- Methods inherited from pp::Instance:

  // Initializes this plugin with <embed/object ...> tag attribute count |argc|,
  // names |argn| and values |argn|. Returns false on failure.
  // Gets called by the browser right after New().
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  // Handles view changes from the browser.
  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip);

  // Handles gaining or losing focus.
  virtual void DidChangeFocus(bool has_focus);

  // Handles input events delivered from the browser to this plugin element.
  virtual bool HandleInputEvent(const PP_InputEvent& event);

  // Handles gaining or losing focus.
  virtual bool HandleDocumentLoad(const pp::URLLoader& url_loader);

  // Returns a scriptable reference to this plugin element.
  // Called by JavaScript document.getElementById(plugin_id).
  virtual pp::Var GetInstanceObject();

  // Handles postMessage from browser
  virtual void HandleMessage(const pp::Var& message);

  // ----- Methods inherited from Plugin:

  // Requests a NaCl manifest download from a |url| relative to the page origin.
  virtual void RequestNaClManifest(const nacl::string& url);

  // Support for proxied execution.
  virtual bool StartProxiedExecution(NaClSrpcChannel* srpc_channel,
                                     nacl::string* error_string);

  // Getter for PPAPI proxy interface.
  ppapi_proxy::BrowserPpp* ppapi_proxy() const { return ppapi_proxy_; }

  // Report successful loading of a module.
  virtual void ReportLoadSuccess(bool length_computable,
                                 uint64_t loaded_bytes,
                                 uint64_t total_bytes);
  // Report an error encountered while loading a module.
  virtual void ReportLoadError(const nacl::string& error);
  // Report loading a module was aborted, typically due to user action.
  virtual void ReportLoadAbort();
  // Dispatch a JavaScript event to indicate a key step in loading.
  // |event_type| is a character string indicating which type of progress
  // event (loadstart, progress, error, abort, load, loadend).
  virtual void DispatchProgressEvent(const char* event_type,
                                     bool length_computable,
                                     uint64_t loaded_bytes,
                                     uint64_t total_bytes);

  // ----- Methods unique to PluginPpapi:

  // Requests a URL asynchronously resulting in a call to js_callback.onload
  // with NaClDesc-wrapped file descriptor on success and js_callback.onfail
  // with an error string on failure.
  // This is used by JS-based __urlAsNaClDesc().
  void UrlAsNaClDesc(const nacl::string& url, pp::Var js_callback);
  // Requests a URL asynchronously resulting in a call to pp_callback with
  // a PP_Error indicating status. On success an open file descriptor
  // corresponding to the url body is recorded for further lookup.
  // This is used by SRPC-based StreamAsFile().
  bool StreamAsFile(const nacl::string& url, PP_CompletionCallback pp_callback);
  // Returns an open POSIX file descriptor retrieved by StreamAsFile()
  // or NACL_NO_FILE_DESC. The caller must take ownership of the descriptor.
  int32_t GetPOSIXFileDesc(const nacl::string& url);

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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginPpapi);
  // Prevent construction and destruction from outside the class:
  // must use factory New() method instead.
  explicit PluginPpapi(PP_Instance instance);
  // The browser will invoke the destructor via the pp::Instance
  // pointer to this object, not from base's Delete().
  virtual ~PluginPpapi();

  // File download support.  |nexe_downloader_| can be opened with a specific
  // callback to run when the file has been downloaded and is opened for
  // reading.  We use one downloader for all URL downloads to prevent issuing
  // multiple GETs that might arrive out of order.  For example, this will
  // prevent a GET of a NaCl manifest while a .nexe GET is pending.  Note that
  // this will also prevent simultaneous handling of multiple .nexes on a page.
  FileDownloader nexe_downloader_;
  pp::CompletionCallbackFactory<PluginPpapi> callback_factory_;

  // Callback used when getting the URL for the .nexe file.  If the URL loading
  // is successful, the file descriptor is opened and can be passed to sel_ldr
  // with the sandbox on.
  void NexeFileDidOpen(int32_t pp_error);

  // NaCl ISA selection manifest file support.  The manifest file is specified
  // using the "nacl" attribute in the <embed> tag.  First, the manifest URL (or
  // data: URI) is fetched, then the JSON is parsed.  Once a valid .nexe is
  // chosen for the sandbox ISA, any current service runtime is shut down, the
  // .nexe is loaded and run.

  // Callback used when getting the URL for the NaCl manifest file.
  void NaClManifestFileDidOpen(int32_t pp_error);

  // Parses the JSON in |manifest_json| and retains a scriptable object in
  // |manifest_object_| for use by subsequent resource lookups.
  // On success, |true| is returned and |manifest_object_| is updated to
  // contain a scriptable object that is used by SelectNexeURLFromManifest.
  // On failure, |false| is returned, and |manifest_object_| is unchanged.
  bool SetManifestObject(const nacl::string& manifest_json,
                         nacl::string* error_string);

  // Determines the URL of the nexe module appropriate for the NaCl sandbox
  // implemented by the installed sel_ldr.  The URL is determined from the
  // scriptable object in |manifest_object_|.  On success, |true| is returned
  // and |result| is set to the URL to use for the nexe.  On failure, |false|
  // is returned.
  bool SelectNexeURLFromManifest(nacl::string* result,
                                 nacl::string* error_string);

  // Determines the appropriate nexe for the sandbox and requests a load.
  void RequestNexeLoad();

  // Callback used when loading a URL for JS-based __urlAsNaClDesc().
  void UrlDidOpenForUrlAsNaClDesc(int32_t pp_error,
                                  FileDownloader*& url_downloader,
                                  pp::Var& js_callback);
  // Callback used when loading a URL for SRPC-based StreamAsFile().
  void UrlDidOpenForStreamAsFile(int32_t pp_error,
                                 FileDownloader*& url_downloader,
                                 PP_CompletionCallback pp_callback);

  // Shuts down the proxy for PPAPI nexes.
  void ShutdownProxy();

  // Handles the __setAsyncCallback() method.  Spawns a thread to receive
  // IMC messages from the NaCl process and pass them on to Javascript.
  static bool SetAsyncCallback(void* obj, SrpcParams* params);

  // The manifest dictionary.  Used for looking up resources to be loaded.
  pp::Var manifest_object_;

  // A string containing the text description of the last error produced by
  // this plugin.
  nacl::string last_error_string_;

  // A pointer to the browser end of a proxy pattern connecting the
  // NaCl plugin to the PPAPI .nexe's PPP interface
  // (InitializeModule, Shutdown, and GetInterface).
  // TODO(sehr): this should be a scoped_ptr for shutdown.
  ppapi_proxy::BrowserPpp* ppapi_proxy_;

  // If we get a DidChangeView event before the nexe is loaded, we store it and
  // replay it to nexe after it's loaded.
  bool replayDidChangeView;
  pp::Rect replayDidChangeViewPosition;
  pp::Rect replayDidChangeViewClip;

  nacl::string mime_type_;

  // Keep track of the FileDownloaders created to fetch urls.
  std::set<FileDownloader*> url_downloaders_;
  // Keep track of file descriptors opened by StreamAsFile().
  // These are owned by the browser.
  std::map<nacl::string, int32_t> url_fd_map_;

  // Adapter class constructors require a reference to 'this', so we can't
  // contain them directly.
  nacl::scoped_ptr<pp::Find_Dev> find_adapter_;
  nacl::scoped_ptr<pp::Printing_Dev> printing_adapter_;
  nacl::scoped_ptr<pp::Selection_Dev> selection_adapter_;
  nacl::scoped_ptr<pp::WidgetClient_Dev> widget_client_adapter_;
  nacl::scoped_ptr<pp::Zoom_Dev> zoom_adapter_;

  int64_t init_time_;
  size_t nexe_size_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
