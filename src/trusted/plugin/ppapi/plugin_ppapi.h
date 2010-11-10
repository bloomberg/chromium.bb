/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// PPAPI-based implementation of the interface for a NaCl plugin instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_

#include <string>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/plugin.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_io_dev.h"
#include "ppapi/cpp/dev/url_loader_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"


struct NaClSrpcChannel;
namespace ppapi_proxy {
class BrowserPpp;
}

namespace pp {
class Rect;
class URLLoader_Dev;
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
  virtual bool HandleDocumentLoad(const pp::URLLoader_Dev& url_loader);

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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginPpapi);
  // Prevent construction and destruction from outside the class:
  // must use factory New() method instead.
  explicit PluginPpapi(PP_Instance instance);
  // The browser will invoke the destructor via the pp::Instance
  // pointer to this object, not from base's Delete().
  virtual ~PluginPpapi();

  // Nexe url loading support.
  // Our goal is to have the browser download the file and give us an open
  // file descriptor that we can pass to sel_ldr with the sandbox on.
  // This is accomplished with a 3-step process:
  // 1) Ask the browser to start streaming nexe as a file.
  // 2) Ask the browser to finish streaming if headers indicate success.
  // 3) Ask the browser to open the file, so we can get the file descriptor.
  // Each step is done asynchronously using callbacks.
  // We create callbacks through a factory to take advantage of ref-counting.
  // TODO(polina): Add a file-download wrapper for all this to ppapi repo. This
  // will remove the intermediate functions and member variables from our code.
  void NexeURLLoadStartNotify(int32_t pp_error);
  void NexeURLLoadFinishNotify(int32_t pp_error);
  void NexeFileOpenNotify(int32_t pp_error);
  pp::FileIO_Dev nexe_reader_;
  pp::URLLoader_Dev nexe_loader_;
  pp::CompletionCallbackFactory<PluginPpapi> callback_factory_;

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

  // A pointer to the browser (proxy) end of a Proxy pattern connecting the
  // plugin to the .nexe's PPP interface (InitializeModule, Shutdown, and
  // GetInterface).
  // TODO(sehr): this should be a scoped_ptr for shutdown.
  ppapi_proxy::BrowserPpp* ppapi_proxy_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
