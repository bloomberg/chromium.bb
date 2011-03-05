// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/user_agent.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

// Generated
#include "webkit_version.h"  // NOLINT

namespace webkit_glue {

// Forward declare GetProductVersionInfo.  This is implemented in
// renderer_glue.cc as part of the renderer lib.
std::string GetProductVersion();

std::string GetWebKitVersion() {
  return base::StringPrintf("%d.%d (%s)",
                            WEBKIT_VERSION_MAJOR,
                            WEBKIT_VERSION_MINOR,
                            WEBKIT_SVN_REVISION);
}

std::string GetWebKitRevision() {
  return WEBKIT_SVN_REVISION;
}

std::string BuildOSCpuInfo() {
  std::string os_cpu;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32)) {  // NOLINT
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

#if defined(OS_WIN)
  std::string architecture_token;
  if (base::win::GetWOW64Status() == base::win::WOW64_ENABLED) {
    architecture_token = "; WOW64";
  } else {
    base::win::WindowsArchitecture windows_architecture =
        base::win::GetWindowsArchitecture();
    if (windows_architecture == base::win::X64_ARCHITECTURE)
      architecture_token = "; Win64; x64";
    else if (windows_architecture == base::win::IA64_ARCHITECTURE)
      architecture_token = "; Win64; IA64";
  }
#endif

  base::StringAppendF(
      &os_cpu,
#if defined(OS_WIN)
      "Windows NT %d.%d%s",
      os_major_version,
      os_minor_version,
      architecture_token.c_str()
#elif defined(OS_MACOSX)
      "Intel Mac OS X %d_%d_%d",
      os_major_version,
      os_minor_version,
      os_bugfix_version
#elif defined(OS_CHROMEOS)
      "CrOS %s %d.%d.%d",
      cputype.c_str(),   // e.g. i686
      os_major_version,
      os_minor_version,
      os_bugfix_version
#else
      "%s %s",
      unixinfo.sysname,  // e.g. Linux
      cputype.c_str()    // e.g. i686
#endif
  );  // NOLINT

  return os_cpu;
}

void BuildUserAgent(bool mimic_windows, std::string* result) {
  const char kUserAgentPlatform[] =
#if defined(OS_WIN)
      "";
#elif defined(OS_MACOSX)
      "Macintosh; ";
#elif defined(USE_X11)
      "X11; ";           // strange, but that's what Firefox uses
#else
      "Unknown; ";
#endif

  // Get the product name and version, and replace Safari's Version/X string
  // with it.  This is done to expose our product name in a manner that is
  // maximally compatible with Safari, we hope!!
  std::string product = GetProductVersion();

  // Derived from Safari's UA string.
  base::StringAppendF(
      result,
      "Mozilla/5.0 (%s%s) AppleWebKit/%d.%d"
      " (KHTML, like Gecko) %s Safari/%d.%d",
      mimic_windows ? "Windows " : kUserAgentPlatform,
      BuildOSCpuInfo().c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR,
      product.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR);
}

}  // namespace webkit_glue

