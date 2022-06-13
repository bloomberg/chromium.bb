// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/xpc_service_names.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"

namespace updater {

namespace {

const char kUpdateServiceInternalLaunchdPrefix[] =
    MAC_BUNDLE_IDENTIFIER_STRING ".update-internal.";
const char kUpdateServiceLaunchdName[] = MAC_BUNDLE_IDENTIFIER_STRING ".update";
const char kSystemLevelKeyword[] = ".system";

std::string GetNameWithScope(const std::string& name, UpdaterScope scope) {
  return scope == UpdaterScope::kSystem
             ? base::StrCat({name, kSystemLevelKeyword})
             : name;
}

}  // namespace

std::string GetUpdateServiceLaunchdName(UpdaterScope scope) {
  return GetNameWithScope(kUpdateServiceLaunchdName, scope);
}

std::string GetUpdateServiceInternalLaunchdName(UpdaterScope scope) {
  return GetNameWithScope(
      base::StrCat({kUpdateServiceInternalLaunchdPrefix, kUpdaterVersion}),
      scope);
}

base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceLaunchdName(
    UpdaterScope scope) {
  return base::SysUTF8ToCFStringRef(GetUpdateServiceLaunchdName(scope));
}

base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName(UpdaterScope scope) {
  return scope == UpdaterScope::kSystem
             ? base::SysUTF8ToCFStringRef(
                   base::StrCat({MAC_BUNDLE_IDENTIFIER_STRING ".wake.",
                                 kUpdaterVersion, kSystemLevelKeyword}))
             : base::SysUTF8ToCFStringRef(base::StrCat(
                   {MAC_BUNDLE_IDENTIFIER_STRING ".wake.", kUpdaterVersion}));
}

base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceInternalLaunchdName(
    UpdaterScope scope) {
  return base::SysUTF8ToCFStringRef(GetUpdateServiceInternalLaunchdName(scope));
}

base::scoped_nsobject<NSString> GetUpdateServiceLaunchdLabel(
    UpdaterScope scope) {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyUpdateServiceLaunchdName(scope).release()));
}

base::scoped_nsobject<NSString> GetWakeLaunchdLabel(UpdaterScope scope) {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyWakeLaunchdName(scope).release()));
}

base::scoped_nsobject<NSString> GetUpdateServiceInternalLaunchdLabel(
    UpdaterScope scope) {
  return base::scoped_nsobject<NSString>(base::mac::CFToNSCast(
      CopyUpdateServiceInternalLaunchdName(scope).release()));
}

base::scoped_nsobject<NSString> GetUpdateServiceMachName(
    base::scoped_nsobject<NSString> name) {
  return base::scoped_nsobject<NSString>(
      [name stringByAppendingString:@".mach"], base::scoped_policy::RETAIN);
}

base::scoped_nsobject<NSString> GetUpdateServiceMachName(UpdaterScope scope) {
  return GetUpdateServiceMachName(GetUpdateServiceLaunchdLabel(scope));
}

base::scoped_nsobject<NSString> GetUpdateServiceInternalMachName(
    UpdaterScope scope) {
  return GetUpdateServiceMachName(GetUpdateServiceInternalLaunchdLabel(scope));
}

}  // namespace updater
