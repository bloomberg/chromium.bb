// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/get_session_name_mac.h"

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <sys/sysctl.h>  // sysctlbyname()

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"

namespace syncer {
namespace internal {

std::string GetHardwareModelName() {
  NSHost* myHost = [NSHost currentHost];
  return base::SysNSStringToUTF8([myHost localizedName]);
}

}  // namespace internal
}  // namespace syncer
