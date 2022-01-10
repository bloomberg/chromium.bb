// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/user_agent.h"

#include <stdint.h>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/util/chromium_git_revision.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_POSIX) && !defined(OS_MAC)
#include <sys/utsname.h>
#endif

namespace content {

namespace {

std::string GetUserAgentPlatform() {
#if defined(OS_WIN)
  return "";
#elif defined(OS_MAC)
  return "Macintosh; ";
#elif defined(USE_OZONE)
  return "X11; ";  // strange, but that's what Firefox uses
#elif defined(OS_ANDROID)
  return "Linux; ";
#elif defined(OS_FUCHSIA)
  // TODO(https://crbug.com/1225812): Determine what to report for Fuchsia,
  // considering both backwards compatibility and User-Agent Reduction.
  return "X11; ";
#elif defined(OS_POSIX)
  return "Unknown; ";
#endif
}

}  // namespace

std::string GetUnifiedPlatform() {
#if defined(OS_ANDROID)
  return frozen_user_agent_strings::kUnifiedPlatformAndroid;
#elif defined(OS_CHROMEOS)
  return frozen_user_agent_strings::kUnifiedPlatformCrOS;
#elif defined(OS_MAC)
  return frozen_user_agent_strings::kUnifiedPlatformMacOS;
#elif defined(OS_WIN)
  return frozen_user_agent_strings::kUnifiedPlatformWindows;
#else
  return frozen_user_agent_strings::kUnifiedPlatformLinux;
#endif
}

// Inaccurately named for historical reasons
std::string GetWebKitVersion() {
  return base::StringPrintf("537.36 (%s)", CHROMIUM_GIT_REVISION);
}

std::string GetChromiumGitRevision() {
  return CHROMIUM_GIT_REVISION;
}

std::string BuildCpuInfo() {
  std::string cpuinfo;

#if defined(OS_MAC)
  cpuinfo = "Intel";
#elif defined(OS_WIN)
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->IsWowX86OnAMD64()) {
    cpuinfo = "WOW64";
  } else {
    base::win::OSInfo::WindowsArchitecture windows_architecture =
        os_info->GetArchitecture();
    if (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE)
      cpuinfo = "Win64; x64";
    else if (windows_architecture == base::win::OSInfo::IA64_ARCHITECTURE)
      cpuinfo = "Win64; IA64";
  }
#elif defined(OS_POSIX) && !defined(OS_MAC)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32_t)) {
    cpuinfo.assign("i686 (x86_64)");
  } else {
    cpuinfo.assign(unixinfo.machine);
  }
#endif

  return cpuinfo;
}

// Return the CPU architecture in Windows/Mac/POSIX and the empty string
// elsewhere.
std::string GetLowEntropyCpuArchitecture() {
#if defined(OS_WIN)
  base::win::OSInfo::WindowsArchitecture windows_architecture =
      base::win::OSInfo::GetInstance()->GetArchitecture();
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  // When running a Chrome x86_64 (AMD64) build on an ARM64 device,
  // the OS lies and returns 0x9 (PROCESSOR_ARCHITECTURE_AMD64)
  // for wProcessorArchitecture.
  if (windows_architecture == base::win::OSInfo::ARM64_ARCHITECTURE ||
      os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64()) {
    return "arm";
  } else if ((windows_architecture == base::win::OSInfo::X86_ARCHITECTURE) ||
             (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE)) {
    return "x86";
  }
#elif defined(OS_MAC)
  base::mac::CPUType cpu_type = base::mac::GetCPUType();
  if (cpu_type == base::mac::CPUType::kIntel) {
    return "x86";
  } else if (cpu_type == base::mac::CPUType::kArm ||
             cpu_type == base::mac::CPUType::kTranslatedIntel) {
    return "arm";
  }
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  std::string cpu_info = BuildCpuInfo();
  if (base::StartsWith(cpu_info, "arm") ||
      base::StartsWith(cpu_info, "aarch")) {
    return "arm";
  } else if ((base::StartsWith(cpu_info, "i") &&
              cpu_info.substr(2, 2) == "86") ||
             base::StartsWith(cpu_info, "x86")) {
    return "x86";
  }
#endif
  return std::string();
}

std::string GetLowEntropyCpuBitness() {
#if defined(OS_WIN)
  return (base::win::OSInfo::GetInstance()->GetArchitecture() ==
          base::win::OSInfo::X86_ARCHITECTURE)
             ? "32"
             : "64";
#elif defined(OS_MAC)
  return "64";
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  return base::Contains(BuildCpuInfo(), "64") ? "64" : "32";
#else
  return std::string();
#endif
}

std::string GetOSVersion(IncludeAndroidBuildNumber include_android_build_number,
                         IncludeAndroidModel include_android_model) {
  std::string os_version;
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

#if defined(OS_MAC)
  // A significant amount of web content breaks if the reported "Mac
  // OS X" major version number is greater than 10. Continue to report
  // this as 10_15_7, the last dot release for that macOS version.
  if (os_major_version > 10) {
    os_major_version = 10;
    os_minor_version = 15;
    os_bugfix_version = 7;
  }
#endif

#endif

#if defined(OS_ANDROID)
  std::string android_version_str = base::SysInfo::OperatingSystemVersion();
  std::string android_info_str =
      GetAndroidOSInfo(include_android_build_number, include_android_model);
#endif

  base::StringAppendF(&os_version,
#if defined(OS_WIN)
                      "%d.%d", os_major_version, os_minor_version
#elif defined(OS_MAC)
                      "%d_%d_%d", os_major_version, os_minor_version,
                      os_bugfix_version
#elif defined(OS_CHROMEOS)
                      "%d.%d.%d", os_major_version, os_minor_version,
                      os_bugfix_version
#elif defined(OS_ANDROID)
                      "%s%s", android_version_str.c_str(),
                      android_info_str.c_str()
#else
                      ""
#endif
  );
  return os_version;
}

std::string BuildOSCpuInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model) {
  return BuildOSCpuInfoFromOSVersionAndCpuType(
      GetOSVersion(include_android_build_number, include_android_model),
      BuildCpuInfo());
}

std::string BuildOSCpuInfoFromOSVersionAndCpuType(const std::string& os_version,
                                                  const std::string& cpu_type) {
  std::string os_cpu;

#if !defined(OS_ANDROID) && defined(OS_POSIX) && !defined(OS_MAC)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);
#endif

#if defined(OS_WIN)
  if (!cpu_type.empty())
    base::StringAppendF(&os_cpu, "Windows NT %s; %s", os_version.c_str(),
                        cpu_type.c_str());
  else
    base::StringAppendF(&os_cpu, "Windows NT %s", os_version.c_str());
#else
  base::StringAppendF(&os_cpu,
#if defined(OS_MAC)
                      "%s Mac OS X %s", cpu_type.c_str(), os_version.c_str()
#elif defined(OS_CHROMEOS)
                      "CrOS "
                      "%s %s",
                      cpu_type.c_str(),  // e.g. i686
                      os_version.c_str()
#elif defined(OS_ANDROID)
                      "Android %s", os_version.c_str()
#elif defined(OS_FUCHSIA)
                      "Fuchsia"
#elif defined(OS_POSIX)
                      "%s %s",
                      unixinfo.sysname,  // e.g. Linux
                      cpu_type.c_str()   // e.g. i686
#endif
  );
#endif

  return os_cpu;
}

std::string GetReducedUserAgent(bool mobile, std::string major_version) {
  std::string user_agent;
#if defined(OS_ANDROID)
  std::string device_compat;
  // Note: The extra space after Mobile is meaningful here, to avoid
  // "MobileSafari", but unneeded for non-mobile Android devices.
  device_compat = mobile ? "Mobile " : "";
  user_agent = base::StringPrintf(frozen_user_agent_strings::kAndroid,
                                  GetUnifiedPlatform().c_str(),
                                  major_version.c_str(), device_compat.c_str());
#else
  user_agent =
      base::StringPrintf(frozen_user_agent_strings::kDesktop,
                         GetUnifiedPlatform().c_str(), major_version.c_str());
#endif

  return user_agent;
}

std::string BuildUserAgentFromProduct(const std::string& product) {
  std::string os_info;
  base::StringAppendF(&os_info, "%s%s", GetUserAgentPlatform().c_str(),
                      BuildOSCpuInfo(IncludeAndroidBuildNumber::Exclude,
                                     IncludeAndroidModel::Include)
                          .c_str());
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string BuildModelInfo() {
  std::string model;
#if defined(OS_ANDROID)
  // Only send the model information if on the release build of Android,
  // matching user agent behaviour.
  if (base::SysInfo::GetAndroidBuildCodename() == "REL")
    model = base::SysInfo::HardwareModelName();
#endif
  return model;
}

#if defined(OS_ANDROID)
std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    IncludeAndroidBuildNumber include_android_build_number) {
  std::string os_info;
  base::StrAppend(&os_info, {GetUserAgentPlatform(),
                             BuildOSCpuInfo(include_android_build_number,
                                            IncludeAndroidModel::Include),
                             extra_os_info});
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string GetAndroidOSInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model) {
  std::string android_info_str;

  // Send information about the device.
  bool semicolon_inserted = false;
  if (include_android_model == IncludeAndroidModel::Include) {
    std::string android_device_name = BuildModelInfo();
    if (!android_device_name.empty()) {
      android_info_str += "; " + android_device_name;
      semicolon_inserted = true;
    }
  }

  // Append the build ID.
  if (include_android_build_number == IncludeAndroidBuildNumber::Include) {
    std::string android_build_id = base::SysInfo::GetAndroidBuildID();
    if (!android_build_id.empty()) {
      if (!semicolon_inserted)
        android_info_str += ";";
      android_info_str += " Build/" + android_build_id;
    }
  }

  return android_info_str;
}
#endif  // defined(OS_ANDROID)

std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product) {
  // Derived from Safari's UA string.
  // This is done to expose our product name in a manner that is maximally
  // compatible with Safari, we hope!!
  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (%s) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "%s Safari/537.36",
                      os_info.c_str(), product.c_str());
  return user_agent;
}

}  // namespace content
