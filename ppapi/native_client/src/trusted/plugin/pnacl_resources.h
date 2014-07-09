// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_

#include <map>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/completion_callback.h"

#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"

namespace plugin {

class Plugin;

// Loads a list of resources, providing a way to get file descriptors for
// these resources.  URLs for resources are resolved by the manifest
// and point to pnacl component filesystem resources.
class PnaclResources {
 public:
  explicit PnaclResources(Plugin* plugin);
  virtual ~PnaclResources();

  // Read the resource info JSON file.  This is the first step after
  // construction; it has to be completed before StartLoad is called.
  bool ReadResourceInfo();

  // Start loading the resources.
  bool StartLoad();

  const nacl::string& GetLlcUrl() { return llc_tool_name_; }
  const nacl::string& GetLdUrl() { return ld_tool_name_; }

  PP_NaClFileInfo TakeLlcFileInfo();
  PP_NaClFileInfo TakeLdFileInfo();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclResources);

  // The plugin requesting the resource loading.
  Plugin* plugin_;

  // Tool names for llc and ld; read from the resource info file.
  nacl::string llc_tool_name_;
  nacl::string ld_tool_name_;

  // File info for llc and ld executables, after they've been opened.
  // Only valid after the callback for StartLoad() has been called, and until
  // TakeLlcFileInfo()/TakeLdFileInfo() is called.
  PP_NaClFileInfo llc_file_info_;
  PP_NaClFileInfo ld_file_info_;
};

}  // namespace plugin
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_
