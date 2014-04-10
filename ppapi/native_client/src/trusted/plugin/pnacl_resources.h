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

#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/cpp/completion_callback.h"

#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"

namespace plugin {

class Manifest;
class Plugin;
class PnaclCoordinator;

// Constants for loading LLC and LD.
class PnaclUrls {
 public:
  // Get the base URL prefix for Pnacl resources (without platform prefix).
  static nacl::string GetBaseUrl();

  static bool IsPnaclComponent(const nacl::string& full_url);
  static nacl::string PnaclComponentURLToFilename(
      const nacl::string& full_url);

  // Get the URL for the resource info JSON file that contains information
  // about loadable resources.
  static const nacl::string GetResourceInfoUrl() {
    return nacl::string(kResourceInfoUrl);
  }
 private:
  static const char kResourceInfoUrl[];
};

// Loads a list of resources, providing a way to get file descriptors for
// these resources.  URLs for resources are resolved by the manifest
// and point to pnacl component filesystem resources.
class PnaclResources {
 public:
  PnaclResources(Plugin* plugin,
                 PnaclCoordinator* coordinator)
      : plugin_(plugin),
        coordinator_(coordinator) {
  }
  virtual ~PnaclResources();

  // Read the resource info JSON file.  This is the first step after
  // construction; it has to be completed before StartLoad is called.
  virtual void ReadResourceInfo(
      const nacl::string& resource_info_url,
      const pp::CompletionCallback& resource_info_read_cb);

  // Start loading the resources.
  virtual void StartLoad(
      const pp::CompletionCallback& all_loaded_callback);

  const nacl::string& GetLlcUrl() {
    return llc_tool_name;
  }

  const nacl::string& GetLdUrl() {
    return ld_tool_name;
  }

  nacl::string GetFullUrl(const nacl::string& partial_url,
                          const nacl::string& sandbox_arch) const;

  // Get file descs by name. Only valid after StartLoad's completion callback
  // fired.
  nacl::DescWrapper* WrapperForUrl(const nacl::string& url);

  static int32_t GetPnaclFD(Plugin* plugin, const char* filename);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclResources);

  // The plugin requesting the resource loading.
  Plugin* plugin_;
  // The coordinator responsible for reporting errors, etc.
  PnaclCoordinator* coordinator_;
  // The descriptor wrappers for the downloaded URLs.  Only valid
  // once all_loaded_callback_ has been invoked.
  std::map<nacl::string, nacl::DescWrapper*> resource_wrappers_;

  // Tool names for llc and ld; read from the resource info file.
  nacl::string llc_tool_name;
  nacl::string ld_tool_name;

  // Parses resource info json data in |buf|.  Returns true if successful.
  // Otherwise returns false and places an error message in |errmsg|.
  bool ParseResourceInfo(const nacl::string& buf, nacl::string& errmsg);

  // Convenience function for reporting an error while reading the resource
  // info file.
  void ReadResourceInfoError(const nacl::string& msg);
};

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_
