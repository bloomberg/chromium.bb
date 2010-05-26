// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_geolocation_service.h"

#include "third_party/WebKit/WebKit/chromium/public/WebGeolocationServiceBridge.h"

TestGeolocationService::TestGeolocationService()
    : allowed_(false) {
}

TestGeolocationService::~TestGeolocationService() {

}

void TestGeolocationService::SetGeolocationPermission(bool allowed) {
  allowed_ = allowed;
}

void TestGeolocationService::requestPermissionForFrame(
    int bridgeId, const WebKit::WebURL& url) {
  pending_permissions_.push_back(std::make_pair(bridgeId, allowed_));
  permission_timer_.Start(base::TimeDelta::FromMilliseconds(0),
                          this, &TestGeolocationService::SendPermission);
}

int TestGeolocationService::attachBridge(
    WebKit::WebGeolocationServiceBridge* bridge) {
  return bridges_map_.Add(bridge);
}

void TestGeolocationService::detachBridge(int bridgeId) {
  bridges_map_.Remove(bridgeId);
}

void TestGeolocationService::SendPermission() {
  for (std::vector<std::pair<int, bool> >::const_iterator i =
      pending_permissions_.begin(); i != pending_permissions_.end(); ++i) {
    WebKit::WebGeolocationServiceBridge* bridge =
       bridges_map_.Lookup(i->first);
    DCHECK(bridge);
    bridge->setIsAllowed(i->second);
  }
  pending_permissions_.clear();
}
