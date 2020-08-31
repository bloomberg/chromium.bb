// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_tab_helper.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/usb/frame_usb_services.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "content/public/browser/navigation_handle.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/usb/web_usb_chooser_android.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/web_usb_chooser_desktop.h"
#endif  // defined(OS_ANDROID)

using content::WebContents;

// static
UsbTabHelper* UsbTabHelper::GetOrCreateForWebContents(
    WebContents* web_contents) {
  UsbTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper) {
    CreateForWebContents(web_contents);
    tab_helper = FromWebContents(web_contents);
  }
  return tab_helper;
}

UsbTabHelper::~UsbTabHelper() {
  DCHECK_EQ(0, device_connection_count_);
}

UsbTabHelper::UsbTabHelper(WebContents* web_contents)
    : web_contents_(web_contents) {}

void UsbTabHelper::NotifyIsDeviceConnectedChanged(
    bool is_device_connected) const {
  performance_manager::PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged(
      web_contents_, is_device_connected);

  // TODO(https://crbug.com/601627): Implement tab indicator for Android.
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    tab_strip_model->UpdateWebContentsStateAt(
        tab_strip_model->GetIndexOfWebContents(web_contents_),
        TabChangeType::kAll);
  }
#endif
}

void UsbTabHelper::IncrementConnectionCount() {
  device_connection_count_++;
  if (device_connection_count_ == 1)
    NotifyIsDeviceConnectedChanged(/*is_device_connected=*/true);
}

void UsbTabHelper::DecrementConnectionCount() {
  DCHECK_GT(device_connection_count_, 0);
  device_connection_count_--;
  if (device_connection_count_ == 0)
    NotifyIsDeviceConnectedChanged(/*is_device_connected=*/false);
}

bool UsbTabHelper::IsDeviceConnected() const {
  return device_connection_count_ > 0;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UsbTabHelper)
