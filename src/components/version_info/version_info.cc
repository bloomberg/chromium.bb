// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_info/version_info.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/version_info/version_info_values.h"

namespace version_info {

const std::string& GetProductNameAndVersionForUserAgent() {
  static const base::NoDestructor<std::string> product_and_version(
      "Chrome/" + GetVersionNumber());
  return *product_and_version;
}

std::string GetProductName() {
  return PRODUCT_NAME;
}

std::string GetVersionNumber() {
  return PRODUCT_VERSION;
}

int GetMajorVersionNumberAsInt() {
  DCHECK(GetVersion().IsValid());
  return GetVersion().components()[0];
}

std::string GetMajorVersionNumber() {
  return base::NumberToString(GetMajorVersionNumberAsInt());
}

const base::Version& GetVersion() {
  static const base::NoDestructor<base::Version> version(GetVersionNumber());
  return *version;
}

std::string GetLastChange() {
  return LAST_CHANGE;
}

bool IsOfficialBuild() {
  return IS_OFFICIAL_BUILD;
}

std::string GetOSType() {
#if defined(OS_WIN)
  return "Windows";
#elif defined(OS_IOS)
  return "iOS";
#elif defined(OS_MAC)
  return "Mac OS X";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
# if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "Chrome OS";
# else
  return "Chromium OS";
# endif
#elif defined(OS_ANDROID)
  return "Android";
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return "Linux";
#elif defined(OS_FREEBSD)
  return "FreeBSD";
#elif defined(OS_OPENBSD)
  return "OpenBSD";
#elif defined(OS_SOLARIS)
  return "Solaris";
#elif defined(OS_FUCHSIA)
  return "Fuchsia";
#else
  return "Unknown";
#endif
}

std::string GetChannelString(Channel channel) {
  switch (channel) {
    case Channel::STABLE:
      return "stable";
    case Channel::BETA:
      return "beta";
    case Channel::DEV:
      return "dev";
    case Channel::CANARY:
      return "canary";
    case Channel::UNKNOWN:
      return "unknown";
  }
  NOTREACHED();
  return std::string();
}

std::string GetSanitizerList() {
  std::string sanitizers;
#if defined(ADDRESS_SANITIZER)
  sanitizers += "address ";
#endif
#if BUILDFLAG(IS_HWASAN)
  sanitizers += "hwaddress ";
#endif
#if defined(LEAK_SANITIZER)
  sanitizers += "leak ";
#endif
#if defined(MEMORY_SANITIZER)
  sanitizers += "memory ";
#endif
#if defined(THREAD_SANITIZER)
  sanitizers += "thread ";
#endif
#if defined(UNDEFINED_SANITIZER)
  sanitizers += "undefined ";
#endif
  return sanitizers;
}

}  // namespace version_info
