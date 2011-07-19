// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_NOTIFICATION_PRESENTER_H_
#define WEBKIT_TOOLS_TEST_SHELL_NOTIFICATION_PRESENTER_H_

#include <map>
#include <set>
#include <string>

#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "webkit/tools/test_shell/test_shell.h"

// A class that implements NotificationPresenter for the test shell.
class TestNotificationPresenter : public WebKit::WebNotificationPresenter {
 public:
  explicit TestNotificationPresenter(TestShell* shell);
  virtual ~TestNotificationPresenter();

  void Reset();

  // Called by the LayoutTestController to simulate a user granting
  // permission.
  void grantPermission(const std::string& origin);

  // WebKit::WebNotificationPresenter interface
  virtual bool show(const WebKit::WebNotification&);
  virtual void cancel(const WebKit::WebNotification&);
  virtual void objectDestroyed(const WebKit::WebNotification&);
  virtual Permission checkPermission(const WebKit::WebURL& url);
  virtual void requestPermission(const WebKit::WebSecurityOrigin& origin,
      WebKit::WebNotificationPermissionCallback* callback);

 private:
  // Non-owned pointer.  The NotificationPresenter is owned by the test shell.
  TestShell* shell_;

  // List of allowed origins.
  std::set<std::string> allowed_origins_;

  // Map of active replacement IDs to the titles of those notifications
  std::map<std::string, std::string> replacements_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_NOTIFICATION_PRESENTER_H_
