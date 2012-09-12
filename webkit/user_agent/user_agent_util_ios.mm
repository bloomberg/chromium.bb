// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/user_agent/user_agent_util.h"

#import <UIKit/UIKit.h>

#include <string>
#include <sys/sysctl.h>

#include "base/memory/scoped_nsobject.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"

namespace {

struct UAVersions {
  const char* safari_version_string;
  const char* webkit_version_string;
};

struct OSVersionMap {
  int32 major_os_version;
  int32 minor_os_version;
  UAVersions ua_versions;
};

const UAVersions& GetUAVersionsForCurrentOS() {
  // The WebKit version can be extracted dynamically from UIWebView, but the
  // Safari version can't be, so a lookup table is used instead (for both, since
  // the reported versions should stay in sync).
  static const OSVersionMap version_map[] = {
    { 6, 0, { "8536.25",   "536.26" } },  // TODO(ios): Update for 6.0 final.
    // 5.1 has the same values as 5.0.
    { 5, 0, { "7534.48.3", "534.46" } },
    { 4, 3, { "6533.18.5", "533.17.9" } },
  };

  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);

  // Return the versions corresponding to the first (and thus highest) OS
  // version less than or equal to the given OS version.
  for (unsigned int i = 0; i < arraysize(version_map); ++i) {
    if (os_major_version > version_map[i].major_os_version ||
        (os_major_version == version_map[i].major_os_version &&
         os_minor_version >= version_map[i].minor_os_version))
      return version_map[i].ua_versions;
  }
  NOTREACHED();
  return version_map[arraysize(version_map) - 1].ua_versions;
}

}  // namespace

namespace webkit_glue {

std::string BuildOSCpuInfo() {
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  std::string os_version;
  if (os_bugfix_version == 0) {
    base::StringAppendF(&os_version,
                        "%d_%d",
                        os_major_version,
                        os_minor_version);
  } else {
    base::StringAppendF(&os_version,
                        "%d_%d_%d",
                        os_major_version,
                        os_minor_version,
                        os_bugfix_version);
  }

  // Remove the end of the platform name. For example "iPod touch" becomes
  // "iPod".
  std::string platform = base::SysNSStringToUTF8(
      [[UIDevice currentDevice] model]);
  size_t position = platform.find_first_of(" ");
  if (position != std::string::npos)
    platform = platform.substr(0, position);

  // The locale string is not easy to set correctly. Safari uses a language
  // code and a dialect code. However, there is no setting allowing the user
  // to set the dialect code, and no API to retrieve it.
  // Note: The NSLocale methods (currentIdentifier:,
  // objectForKey:NSLocaleLanguageCode and objectForKey:NSLocaleCountryCode) are
  // not useful here because they return information related to the "Region
  // Format" setting, which is different from the "Language" setting.
  scoped_nsobject<NSDictionary> dialects([[NSDictionary alloc]
      initWithObjectsAndKeys:
          @"ar",    @"ar",  // No dialect code in Safari.
          @"ca-es", @"ca",
          @"cs-cz", @"cs",
          @"da-dk", @"da",
          @"el-gr", @"el",
          @"en-gb", @"en-GB",
          @"en-us", @"en",
          @"he-il", @"he",
          @"id",    @"id",  // No dialect code in Safari.
          @"ja-jp", @"ja",
          @"ko-kr", @"ko",
          @"nb-no", @"nb",
          @"pt-br", @"pt",
          @"pt-pt", @"pt-PT",
          @"sv-se", @"sv",
          @"uk-ua", @"uk",
          @"vi-vn", @"vi",
          @"zh-cn", @"zh-Hans",
          @"zh-tw", @"zh-Hant",
          nil]);

  NSArray* preferredLanguages = [NSLocale preferredLanguages];
  NSString* language = ([preferredLanguages count] > 0) ?
      [preferredLanguages objectAtIndex:0] : @"en";
  NSString* localeIdentifier = [dialects objectForKey:language];
  if (!localeIdentifier) {
    // No entry in the dictionary, so duplicate the language string.
    localeIdentifier = [NSString stringWithFormat:@"%@-%@", language, language];
  }

  std::string os_cpu;
  base::StringAppendF(
      &os_cpu,
      "%s;%s CPU %sOS %s like Mac OS X; %s",
      platform.c_str(),
      (os_major_version < 5) ? " U;" : "",  // iOS < 5 has a "U;" in the UA.
      (platform == "iPad") ? "" : "iPhone ",  // iPad has an empty string here.
      os_version.c_str(),
      base::SysNSStringToUTF8(localeIdentifier).c_str());

  return os_cpu;
}

std::string BuildUserAgentFromProduct(const std::string& product) {
  // Retrieve the kernel build number.
  int mib[2] = {CTL_KERN, KERN_OSVERSION};
  unsigned int namelen = sizeof(mib) / sizeof(mib[0]);
  size_t bufferSize = 0;
  sysctl(mib, namelen, NULL, &bufferSize, NULL, 0);
  char kernel_version[bufferSize];
  int result = sysctl(mib, namelen, kernel_version, &bufferSize, NULL, 0);
  DCHECK(result == 0);

  UAVersions ua_versions = GetUAVersionsForCurrentOS();

  std::string user_agent;
  base::StringAppendF(
      &user_agent,
      "Mozilla/5.0 (%s) AppleWebKit/%s"
      " (KHTML, like Gecko) %s Mobile/%s Safari/%s",
      webkit_glue::BuildOSCpuInfo().c_str(),
      ua_versions.webkit_version_string,
      product.c_str(),
      kernel_version,
      ua_versions.safari_version_string);

  return user_agent;
}

}  // namespace webkit_glue
