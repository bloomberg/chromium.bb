// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_resources.h"

#include <utility>
#include <vector>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/c/pp_errors.h"

namespace plugin {

class Plugin;

PnaclResources::~PnaclResources() {
  for (std::map<nacl::string, nacl::DescWrapper*>::iterator
           i = resource_wrappers_.begin(), e = resource_wrappers_.end();
       i != e;
       ++i) {
    delete i->second;
  }
  resource_wrappers_.clear();
}

void PnaclResources::Initialize() {
  callback_factory_.Initialize(this);
}

void PnaclResources::AddResourceUrl(const nacl::string& url) {
  // Use previously loaded resources if available.
  if (resource_wrappers_.find(url) != resource_wrappers_.end()) {
    return;
  }
  all_loaded_ = false;
  resource_urls_.push_back(url);
}

void PnaclResources::StartDownloads() {
  // If there are no resources to be loaded, report all loaded to invoke
  // client callbacks as needed.
  if (all_loaded_) {
    AllLoaded(PP_OK);
    return;
  }
  pp::CompletionCallback all_loaded_callback =
      callback_factory_.NewCallback(&PnaclResources::AllLoaded);
  // Create a counter (barrier) callback to track when all of the resources
  // are loaded.
  uint32_t resource_count = static_cast<uint32_t>(resource_urls_.size());
  delayed_callback_.reset(
      new DelayedCallback(all_loaded_callback, resource_count));

  // All resource URLs are relative to the coordinator's resource_base_url().
  nacl::string resource_base_url = coordinator_->resource_base_url();

  // Schedule the downloads.
  CHECK(resource_urls_.size() > 0);
  for (size_t i = 0; i < resource_urls_.size(); ++i) {
    const nacl::string& full_url = resource_base_url + resource_urls_[i];
    pp::CompletionCallback ready_callback =
        callback_factory_.NewCallback(&PnaclResources::ResourceReady,
                                      resource_urls_[i],
                                      full_url);
    if (!plugin_->StreamAsFile(full_url,
                               ready_callback.pp_completion_callback())) {
      ErrorInfo error_info;
      error_info.SetReport(ERROR_UNKNOWN,
                           "PnaclCoordinator: Failed to download file: " +
                           resource_urls_[i] + "\n");
      coordinator_->ReportLoadError(error_info);
      coordinator_->PnaclNonPpapiError();
      break;
    }
  }
  resource_urls_.clear();
}

void PnaclResources::ResourceReady(int32_t pp_error,
                                   const nacl::string& url,
                                   const nacl::string& full_url) {
  PLUGIN_PRINTF(("PnaclResources::ResourceReady (pp_error=%"
                 NACL_PRId32", url=%s)\n", pp_error, url.c_str()));
  // pp_error is checked by GetLoadedFileDesc.
  int32_t fd = coordinator_->GetLoadedFileDesc(pp_error,
                                               full_url,
                                               "resource " + url);
  if (fd < 0) {
    coordinator_->PnaclPpapiError(pp_error);
  } else {
    resource_wrappers_[url] =
        plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY);
    delayed_callback_->RunIfTime();
  }
}

void PnaclResources::AllLoaded(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclResources::AllLoaded (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  all_loaded_ = true;
  // Run the client-specified callback if one was set.
  if (client_callback_is_valid_) {
    pp::Core* core = pp::Module::Get()->core();
    core->CallOnMainThread(0, client_callback_, PP_OK);
  }
}

void PnaclResources::RunWhenAllLoaded(pp::CompletionCallback& client_callback) {
  if (all_loaded_) {
    pp::Core* core = pp::Module::Get()->core();
    core->CallOnMainThread(0, client_callback, PP_OK);
  }
  client_callback_ = client_callback;
  client_callback_is_valid_ = true;
}

}  // namespace plugin
