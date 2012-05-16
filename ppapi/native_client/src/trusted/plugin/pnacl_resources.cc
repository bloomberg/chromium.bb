// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_resources.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/manifest.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/c/pp_errors.h"

namespace plugin {

const char PnaclUrls::kExtensionOrigin[] =
    "chrome-extension://gcodniebolpnpaiggndmcmmfpldlknih/";
const char PnaclUrls::kLlcUrl[] = "llc";
const char PnaclUrls::kLdUrl[] = "ld";

PnaclResources::~PnaclResources() {
  for (std::map<nacl::string, nacl::DescWrapper*>::iterator
           i = resource_wrappers_.begin(), e = resource_wrappers_.end();
       i != e;
       ++i) {
    delete i->second;
  }
  resource_wrappers_.clear();
}

void PnaclResources::StartDownloads() {
  PLUGIN_PRINTF(("PnaclResources::StartDownloads\n"));
  // Create a counter (barrier) callback to track when all of the resources
  // are loaded.
  uint32_t resource_count = static_cast<uint32_t>(resource_urls_.size());
  delayed_callback_.reset(
      new DelayedCallback(all_loaded_callback_, resource_count));

  // Schedule the downloads.
  CHECK(resource_urls_.size() > 0);
  for (size_t i = 0; i < resource_urls_.size(); ++i) {
    nacl::string full_url;
    ErrorInfo error_info;
    if (!manifest_->ResolveURL(resource_urls_[i], &full_url, &error_info)) {
      coordinator_->ReportNonPpapiError(nacl::string("failed to resolve ") +
                                        resource_urls_[i] + ": " +
                                        error_info.message() + ".");
      break;
    }
    pp::CompletionCallback ready_callback =
        callback_factory_.NewCallback(&PnaclResources::ResourceReady,
                                      resource_urls_[i],
                                      full_url);
    if (!plugin_->StreamAsFile(full_url,
                               ready_callback.pp_completion_callback())) {
      coordinator_->ReportNonPpapiError(nacl::string("failed to download ") +
                                        resource_urls_[i] + ".");
      break;
    }
  }
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
    coordinator_->ReportPpapiError(pp_error,
                                   "PnaclResources::ResourceReady failed.");
  } else {
    resource_wrappers_[url] =
        plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY);
    delayed_callback_->RunIfTime();
  }
}

}  // namespace plugin
