/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

// TODO(neb): Remove this hack when it stops being required.
// http://code.google.com/p/nativeclient/issues/detail?id=1421
// <HACK>
#include "ppapi/cpp/dev/context_3d_dev.h"
#include "ppapi/cpp/dev/surface_3d_dev.h"
// </HACK>

struct NaClSrpcChannel;
struct NaClDesc;
namespace ppapi_proxy {
class BrowserPpp;
}

namespace pp {
class URLLoader;
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

  // ----- Methods inherited from Plugin:

  // Requests a NaCl module download from a |url| relative to the page origin.
  // Returns false on failure.
  virtual bool RequestNaClModule(const nacl::string& url);

  // Support for proxied execution.
  virtual void StartProxiedExecution(NaClSrpcChannel* srpc_channel);

  // Getter for PPAPI proxy interface.
  ppapi_proxy::BrowserPpp* ppapi_proxy() const { return ppapi_proxy_; }

  // ----- Methods unique to PluginPpapi:

  // Requests a URL asynchronously resulting in a call to js_callback.onload
  // with NaClDesc-wrapped file descriptor on success and js_callback.onfail
  // with an error string on failure.
  // This is used by JS-based __urlAsNaClDesc().
  bool UrlAsNaClDesc(const nacl::string& url, pp::Var js_callback);
  // Requests a URL asynchronously resulting in a call to pp_callback with
  // a PP_Error indicating status. On success an open file descriptor
  // corresponding to the url body is recorded for further lookup.
  // This is used by SRPC-based StreamAsFile().
  bool StreamAsFile(const nacl::string& url, PP_CompletionCallback pp_callback);
  // Returns an open POSIX file descriptor retrieved by StreamAsFile()
  // or NACL_NO_FILE_DESC. The caller must take ownership of the descriptor.
  int32_t GetPOSIXFileDesc(const nacl::string& url);

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

  // Requests a NaCl manifest download from a |url| relative to the page origin.
  // Returns false on failure.
  bool RequestNaClManifest(const nacl::string& url);
  // Callback used when getting the URL for the NaCl manifest file.
  void NaClManifestFileDidOpen(int32_t pp_error);

  // Parses the JSON pointed at by |nexe_manifest_json| and determines the URL
  // of the nexe module appropriate for the NaCl sandbox implemented by the
  // installed sel_ldr.  The URL is determined from the JSON in
  // |nexe_manifest_json|, see issue:
  //   http://code.google.com/p/nativeclient/issues/detail?id=1040
  // for more details.  The JSON for a .nexe with base name 'hello' that has
  // builds for x86-32, x86-64 and ARM (for example) is expected to look like
  // this:
  // {
  //   "nexes": {
  //     "x86-64": "hello-x86-64.nexe",
  //     "x86-32": "hello-x86-32.nexe",
  //     "ARM": "hello-arm.nexe"
  //   }
  // }
  // On success, |true| is returned and |*result| is updated with the URL from
  // the JSON.  On failure, |false| is returned, and |*result| is unchanged.
  // TODO(dspringer): Note that this routine uses the 'eval' routine on the
  // browser's window object, and can potentially expose security issues.  See
  // bug http://code.google.com/p/nativeclient/issues/detail?id=1038.
  bool SelectNexeURLFromManifest(const nacl::string& nexe_manifest_json,
                                 nacl::string* result);

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

  // Logs the error to JS console, shuts down the proxy and returns false,
  // so this function can be used as a return value.
  bool Failure(const nacl::string& error);

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

  // Keep track of the FileDownloaders created to fetch urls.
  std::set<FileDownloader*> url_downloaders_;
  // Keep track of file descriptors opened by StreamAsFile().
  // These are owned by the browser.
  std::map<nacl::string, int32_t> url_fd_map_;

  // TODO(neb): Remove this hack.
  pp::Context3D_Dev context_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
