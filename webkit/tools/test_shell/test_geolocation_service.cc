// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_geolocation_service.h"

#include "third_party/WebKit/WebKit/chromium/public/WebGeolocationServiceBridge.h"

TestGeolocationService::TestGeolocationService()
    : allowed_(false),
      permission_set_(false) {
}

TestGeolocationService::~TestGeolocationService() {
}

void TestGeolocationService::SetGeolocationPermission(bool allowed) {
  allowed_ = allowed;
  permission_set_ = true;
  TryToSendPermissions();
}

void TestGeolocationService::requestPermissionForFrame(
    int bridgeId, const WebKit::WebURL& url) {
  DCHECK(bridges_map_.Lookup(bridgeId)) << "Unknown bridge " << bridgeId;
  pending_permissions_.push_back(bridgeId);
  TryToSendPermissions();
}

int TestGeolocationService::attachBridge(
    WebKit::WebGeolocationServiceBridge* bridge) {
  return bridges_map_.Add(bridge);
}

void TestGeolocationService::detachBridge(int bridgeId) {
  bridges_map_.Remove(bridgeId);
  std::vector<int>::iterator i = pending_permissions_.begin();
  while (i != pending_permissions_.end()) {
    if (*i == bridgeId)
      pending_permissions_.erase(i);
    else
      ++i;
  }
}

void TestGeolocationService::TryToSendPermissions() {
  if (permission_set_ && !permission_timer_.IsRunning())
    permission_timer_.Start(base::TimeDelta::FromMilliseconds(0),
                            this, &TestGeolocationService::SendPermission);
}

void TestGeolocationService::SendPermission() {
  DCHECK(permission_set_);
  std::vector<int> pending_permissions;
  pending_permissions.swap(pending_permissions_);
  for (std::vector<int>::const_iterator i = pending_permissions.begin();
      i != pending_permissions.end(); ++i) {
    WebKit::WebGeolocationServiceBridge* bridge =
       bridges_map_.Lookup(*i);
    DCHECK(bridge);
    bridge->setIsAllowed(allowed_);
  }
}
