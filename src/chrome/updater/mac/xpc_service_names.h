// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
#define CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <string>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

std::string GetUpdateServiceLaunchdName(UpdaterScope scope);
std::string GetUpdateServiceInternalLaunchdName(UpdaterScope scope);

base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceLaunchdName(
    UpdaterScope scope);
base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName(UpdaterScope scope);
base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceInternalLaunchdName(
    UpdaterScope scope);
base::scoped_nsobject<NSString> GetUpdateServiceLaunchdLabel(
    UpdaterScope scope);
base::scoped_nsobject<NSString> GetWakeLaunchdLabel(UpdaterScope scope);
base::scoped_nsobject<NSString> GetUpdateServiceInternalLaunchdLabel(
    UpdaterScope scope);
base::scoped_nsobject<NSString> GetUpdateServiceMachName(UpdaterScope scope);
base::scoped_nsobject<NSString> GetUpdateServiceInternalMachName(
    UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
