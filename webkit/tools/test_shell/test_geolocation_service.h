// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_GEOLOCATION_SERVICE_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_GEOLOCATION_SERVICE_H_

#include "base/id_map.h"
#include "base/timer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebGeolocationService.h"

namespace WebKit {
class WebURL;
}  // namespace WebKit

// This class short-circuits the browser side of Geolocation permission
// management: instead of going all the way up to GeolocationPermissionContext,
// InfoBar, GeolocationContentSettingsMap, etc., we send a pre-arranged response
// here. The flow is basically:
//
// 1. LayoutTestController::setGeolocationPermission is exposed to JS,
// which then calls TestGeolocationService::SetGeolocationPermission(). This
// response will be used for all subsequent geolocation requests.
//
// 2. WebKit::WebGeolocationServiceBridge attaches to us via attachBridge(), and
// eventually calls requestPermissionForFrame().
//
// 3. We then callback into it, setting its permission (we yield the callstack
// using a timer since WebKit doesn't expect the response to be synchronous).
//
// Note: WebKit provides a mock for position and error updates. For browser-side
// and end-to-end tests, check geolocation_browsertest.cc and
// geolocation_permission_context_unittest.cc.
class TestGeolocationService : public WebKit::WebGeolocationService {
 public:
  TestGeolocationService();
  virtual ~TestGeolocationService();

  void SetGeolocationPermission(bool allowed);

  virtual void requestPermissionForFrame(int bridgeId,
                                         const WebKit::WebURL& url);

  virtual int attachBridge(WebKit::WebGeolocationServiceBridge* bridge);
  virtual void detachBridge(int bridgeId);

 private:
  void TryToSendPermissions();
  void SendPermission();

  // Holds the value of |allowed| in most recent SetGeolocationPermission call.
  bool allowed_;
  // Remains false until the first SetGeolocationPermission call.
  bool permission_set_;

  IDMap<WebKit::WebGeolocationServiceBridge> bridges_map_;

  base::OneShotTimer<TestGeolocationService> permission_timer_;

  // In-order vector of pending bridge IDs. Is not pumped by
  // TryToSendPermissions until the first call to SetGeolocationPermission.
  std::vector<int> pending_permissions_;

  DISALLOW_COPY_AND_ASSIGN(TestGeolocationService);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_GEOLOCATION_SERVICE_H_
