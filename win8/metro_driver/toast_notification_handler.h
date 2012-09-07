// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_TOAST_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_TOAST_NOTIFICATION_HANDLER_H_

#include <windows.ui.notifications.h>

#include "base/string16.h"

// Provides functionality to display a metro style toast notification.
class ToastNotificationHandler {
 public:
  // Holds information about a desktop notification to be displayed.
  struct DesktopNotification {
    std::string origin_url;
    std::string icon_url;
    string16 title;
    string16 body;
    string16 display_source;
    std::string id;

    DesktopNotification(const char* notification_origin,
                        const char* notification_icon,
                        const wchar_t* notification_title,
                        const wchar_t* notification_body,
                        const wchar_t* notification_display_source,
                        const char* notification_id);
  };

  ToastNotificationHandler();
  ~ToastNotificationHandler();

  void DisplayNotification(const DesktopNotification& notification);
  void CancelNotification();

  HRESULT OnActivate(winui::Notifications::IToastNotification* notification,
                     IInspectable* inspectable);

 private:
  mswr::ComPtr<winui::Notifications::IToastNotifier> notifier_;
  mswr::ComPtr<winui::Notifications::IToastNotification> notification_;

  EventRegistrationToken activated_token_;
};

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_TOAST_NOTIFICATION_HANDLER_H_
