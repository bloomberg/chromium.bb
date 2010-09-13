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
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"

struct NaClSrpcChannel;
namespace ppapi_proxy {
class BrowserPpp;
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
  // A pointer to the browser (proxy) end of a Proxy pattern connecting the
  // plugin to the .nexe's PPP interface (InitializeModule, Shutdown, and
  // GetInterface).
  // TODO(sehr): this should be a scoped_ptr for shutdown.
  ppapi_proxy::BrowserPpp* ppapi_proxy_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
