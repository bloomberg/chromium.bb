// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/notification_presenter.h"

#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotification.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPermissionCallback.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"

using WebKit::WebNotification;
using WebKit::WebNotificationPresenter;
using WebKit::WebNotificationPermissionCallback;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebURL;

void TestNotificationPresenter::grantPermission(const std::string& origin) {
  // Make sure it's in the form of an origin.
  GURL url(origin);
  allowed_origins_.insert(url.GetOrigin().spec());
}

// The output from all these methods matches what DumpRenderTree produces.
bool TestNotificationPresenter::show(const WebNotification& notification) {
  if (notification.isHTML()) {
    printf("DESKTOP NOTIFICATION: contents at %s\n",
           notification.url().spec().data());
  } else {
    printf("DESKTOP NOTIFICATION: icon %s, title %s, text %s\n",
           notification.icon().utf8().data(),
           notification.title().utf8().data(),
           notification.body().utf8().data());
  }

  WebNotification event_target(notification);
  event_target.dispatchDisplayEvent();
  return true;
}

void TestNotificationPresenter::cancel(const WebNotification& notification) {
  WebString identifier;
  if (notification.isHTML())
    identifier = notification.url().spec().utf16();
  else
    identifier = notification.title();

  printf("DESKTOP NOTIFICATION CLOSED: %s\n", identifier.utf8().data());
  WebNotification event_target(notification);
  event_target.dispatchCloseEvent(false);
}

void TestNotificationPresenter::objectDestroyed(
    const WebKit::WebNotification& notification) {
  // Nothing to do.  Not storing the objects.
}

WebNotificationPresenter::Permission TestNotificationPresenter::checkPermission(
    const WebURL& url) {
  // Check with the layout test controller
  std::string origin = static_cast<GURL>(url).GetOrigin().spec();
  bool allowed = allowed_origins_.find(origin) != allowed_origins_.end();
  return allowed ? WebNotificationPresenter::PermissionAllowed
                 : WebNotificationPresenter::PermissionDenied;
}

void TestNotificationPresenter::requestPermission(
    const WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback) {
  printf("DESKTOP NOTIFICATION PERMISSION REQUESTED: %s\n",
         origin.toString().utf8().data());
  callback->permissionRequestComplete();
}
