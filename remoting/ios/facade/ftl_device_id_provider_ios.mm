// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/ios/facade/ftl_device_id_provider_ios.h"

#import <UIKit/UIKit.h>

#include "base/strings/sys_string_conversions.h"

namespace remoting {

FtlDeviceIdProviderIos::FtlDeviceIdProviderIos() {
  std::string vendor_id = base::SysNSStringToUTF8(
      UIDevice.currentDevice.identifierForVendor.UUIDString);
  device_id_.set_type(ftl::DeviceIdType_Type_IOS_VENDOR_ID);
  device_id_.set_id(vendor_id);
}

FtlDeviceIdProviderIos::~FtlDeviceIdProviderIos() = default;

ftl::DeviceId FtlDeviceIdProviderIos::GetDeviceId() {
  return device_id_;
}

}  // namespace remoting
