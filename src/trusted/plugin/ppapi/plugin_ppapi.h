/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// PPAPI-based implementation of the interface for a plugin instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_

#include "native_client/src/trusted/plugin/plugin.h"
#include "ppapi/cpp/instance.h"

struct NaClSrpcChannel;

namespace plugin {

// Encapsulates a PPAPI plugin.
class PluginPpapi : public pp::Instance, public Plugin {
 public:
  // Factory method for creation.
  static PluginPpapi* New(PP_Instance instance);

  // Overriden pp::Instance methods:

  // Initializes this plugin with argument count |argc|, names |argn|
  // and values |argn|. Called right after New.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  // Overriden Plugin methods:

  // Request a nacl module download.
  virtual bool RequestNaClModule(const nacl::string& url);

  // Support for proxied execution.
  virtual void StartProxiedExecution(NaClSrpcChannel* srpc_channel);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginPpapi);
  // Prevent construction and destruction from outside the class:
  // must use factory New() and base's Delete() methods instead.
  explicit PluginPpapi(PP_Instance instance);
  virtual ~PluginPpapi();
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_PLUGIN_PPAPI_H_
