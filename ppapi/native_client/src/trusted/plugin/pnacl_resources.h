// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "native_client/src/trusted/plugin/plugin_error.h"

#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class Plugin;
class PnaclCoordinator;

class PnaclResources {
 public:
  PnaclResources(Plugin* plugin, PnaclCoordinator* coordinator)
      : plugin_(plugin),
        coordinator_(coordinator),
        all_loaded_(true),
        client_callback_is_valid_(false)
        { }

  virtual ~PnaclResources();

  void Initialize();

  nacl::DescWrapper* WrapperForUrl(const nacl::string& url) {
    return resource_wrappers_[url];
  }

  // Add an URL for download.
  void AddResourceUrl(const nacl::string& url);
  // Start fetching the URLs.
  void StartDownloads();
  // Set the callback for what to do when all the resources are available.
  void RunWhenAllLoaded(pp::CompletionCallback& client_callback);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclResources);

  // Callback invoked each time one resource has been loaded.
  void ResourceReady(int32_t pp_error, const nacl::string& url);
  // Callback invoked when all resources have been loaded.
  void AllLoaded(int32_t pp_error);

  Plugin* plugin_;
  PnaclCoordinator* coordinator_;
  std::vector<nacl::string> resource_urls_;
  std::map<nacl::string, nacl::DescWrapper*> resource_wrappers_;
  nacl::scoped_ptr<DelayedCallback> delayed_callback_;
  // Set once all resources have been loaded to indicate that future calls to
  // RunWhenAllLoaded should immediately invoke the client callback.  This
  // simplifies the client while allowing resources to be used for subsequent
  // translations.
  bool all_loaded_;
  // Callback to be invoked when all resources can be guaranteed available.
  pp::CompletionCallback client_callback_;
  // If RunWhenAllLoaded was called before all resources have been loaded,
  // this indicates that the registered client callback should be used when
  // the last resource arrives.
  bool client_callback_is_valid_;
  // Factory for ready callbacks, etc.
  pp::CompletionCallbackFactory<PnaclResources> callback_factory_;
};

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_RESOURCES_H_
