// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_

#include <map>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/delayed_callback.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace plugin {

class Manifest;
class Plugin;
class PnaclCoordinator;

class PnaclUrls {
 public:
  static nacl::string GetExtensionUrl() {
    return nacl::string(kExtensionOrigin) + GetSandboxISA() + "/";
  }
  static const nacl::string GetLlcUrl() { return nacl::string(kLlcUrl); }
  static const nacl::string GetLdUrl() { return nacl::string(kLdUrl); }
 private:
  static const char kExtensionOrigin[];
  static const char kLlcUrl[];
  static const char kLdUrl[];
};

// Loads a list of remote resources, providing a way to get file descriptors for
// thse resources.  All URLs in relative to resource_base_url_.
class PnaclResources {
 public:
  PnaclResources(Plugin* plugin,
                 PnaclCoordinator* coordinator,
                 const Manifest* manifest,
                 const std::vector<nacl::string>& resource_urls,
                 const pp::CompletionCallback& all_loaded_callback)
      : plugin_(plugin),
        coordinator_(coordinator),
        manifest_(manifest),
        resource_urls_(resource_urls),
        all_loaded_callback_(all_loaded_callback) {
    callback_factory_.Initialize(this);
  }

  virtual ~PnaclResources();

  // Start fetching the URLs.  After construction, this is the first step.
  void StartDownloads();
  // Get the wrapper for the downloaded resource.
  // Only valid after all_loaded_callback_ has been run.
  nacl::DescWrapper* WrapperForUrl(const nacl::string& url) {
    return resource_wrappers_[url];
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclResources);

  // Callback invoked each time one resource has been loaded.
  void ResourceReady(int32_t pp_error,
                     const nacl::string& url,
                     const nacl::string& full_url);

  // The plugin requesting the resource loading.
  Plugin* plugin_;
  // The coordinator responsible for reporting errors, etc.
  PnaclCoordinator* coordinator_;
  // The manifest for looking up resource URLs.
  const Manifest* manifest_;
  // The list of resource URLs (relative to resource_base_url_) to load.
  std::vector<nacl::string> resource_urls_;
  // Callback to be invoked when all resources can be guaranteed available.
  pp::CompletionCallback all_loaded_callback_;
  // The descriptor wrappers for the downloaded URLs.  Only valid
  // once all_loaded_callback_ has been invoked.
  std::map<nacl::string, nacl::DescWrapper*> resource_wrappers_;
  // Because we may be loading multiple resources, we need a callback that
  // is invoked each time a resource arrives, and finally invokes
  // all_loaded_callback_ when done.
  nacl::scoped_ptr<DelayedCallback> delayed_callback_;
  // Factory for ready callbacks, etc.
  pp::CompletionCallbackFactory<PnaclResources> callback_factory_;
};

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_
