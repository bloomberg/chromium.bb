// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/get_session_name_mac.h"

#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"

namespace syncer {
namespace internal {

std::string GetHardwareModelName() {
  // Do not use NSHost currentHost, as it's very slow. http://crbug.com/138570
  SCDynamicStoreContext context = { 0, NULL, NULL, NULL };
  base::mac::ScopedCFTypeRef<SCDynamicStoreRef> store(
      SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("policy_subsystem"),
                           NULL, &context));
  CFStringRef machine_name = SCDynamicStoreCopyLocalHostName(store.get());
  return base::SysCFStringRefToUTF8(machine_name);
}

}  // namespace internal
}  // namespace syncer
