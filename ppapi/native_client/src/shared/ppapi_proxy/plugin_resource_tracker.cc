// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_resource_tracker.h"

#include <limits>
#include <set>

#include "base/basictypes.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/untrusted/srpcgen/ppb_rpc.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi_proxy {

// static
PluginResourceTracker* PluginResourceTracker::Get() {
  CR_DEFINE_STATIC_LOCAL(PluginResourceTracker, tracker, ());
  return &tracker;
}

PluginResourceTracker::ResourceAndRefCounts::ResourceAndRefCounts(
    PluginResource* r, size_t browser_count)
    : resource(r), browser_refcount(browser_count), plugin_refcount(1) {
}

PluginResourceTracker::ResourceAndRefCounts::~ResourceAndRefCounts() {
}

scoped_refptr<PluginResource> PluginResourceTracker::GetExistingResource(
    PP_Resource res) const {
  ResourceMap::const_iterator result = live_resources_.find(res);
  if (result == live_resources_.end())
    return scoped_refptr<PluginResource>();
  else
    return result->second.resource;
}

PluginResourceTracker::PluginResourceTracker() {
}

void PluginResourceTracker::AddResource(PluginResource* resource,
                                        PP_Resource id,
                                        size_t browser_refcount) {
  // Add the resource with plugin use-count 1.
  live_resources_.insert(
      std::make_pair(id, ResourceAndRefCounts(resource, browser_refcount)));
}

bool PluginResourceTracker::AddRefResource(PP_Resource res) {
  ResourceMap::iterator i = live_resources_.find(res);
  if (i == live_resources_.end()) {
    return false;
  } else {
    // We don't protect against overflow, since a plugin as malicious as to ref
    // once per every byte in the address space could have just as well unrefed
    // one time too many.
    i->second.plugin_refcount++;
    if (i->second.browser_refcount == 0) {
      // If we don't currently have any refcount with the browser, try to
      // obtain one.
      i->second.browser_refcount++;
      ObtainBrowserResource(res);
    }
    return true;
  }
}

bool PluginResourceTracker::UnrefResource(PP_Resource res) {
  ResourceMap::iterator i = live_resources_.find(res);
  if (i != live_resources_.end()) {
    i->second.plugin_refcount--;
    if (0 == i->second.plugin_refcount) {
      size_t browser_refcount = i->second.browser_refcount;
      live_resources_.erase(i);

      // Release all browser references.
      ReleaseBrowserResource(res, browser_refcount);
    }
    return true;
  } else {
    return false;
  }
}

void PluginResourceTracker::ObtainBrowserResource(PP_Resource res) {
  if (res) {
    NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
    PpbCoreRpcClient::PPB_Core_AddRefResource(channel, res);
  }
}

void PluginResourceTracker::ReleaseBrowserResource(PP_Resource res,
                                                   size_t browser_refcount) {
  // Release all browser references.
  if (res && (browser_refcount > 0)) {
    NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
    PpbCoreRpcClient::ReleaseResourceMultipleTimes(channel, res,
                                                   browser_refcount);
  }
}

}  // namespace ppapi_proxy

