// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <errno.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info_internal.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <sys/vfs.h>
#define statvfs statfs  // Android uses a statvfs-like statfs struct and call.
#else
#include <sys/statvfs.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <linux/magic.h>
#include <sys/vfs.h>
#endif

namespace {

#if !BUILDFLAG(IS_OPENBSD)
int NumberOfProcessors() {
  // sysconf returns the number of "logical" (not "physical") processors on both
  // Mac and Linux.  So we get the number of max available "logical" processors.
  //
  // Note that the number of "currently online" processors may be fewer than the
  // returned value of NumberOfProcessors(). On some platforms, the kernel may
  // make some processors offline intermittently, to save power when system
  // loading is low.
  //
  // One common use case that needs to know the processor count is to create
  // optimal number of threads for optimization. It should make plan according
  // to the number of "max available" processors instead of "currently online"
  // ones. The kernel should be smart enough to make all processors online when
  // it has sufficient number of threads waiting to run.
  long res = sysconf(_SC_NPROCESSORS_CONF);
  if (res == -1) {
    NOTREACHED();
    return 1;
  }

  int num_cpus = static_cast<int>(res);

#if BUILDFLAG(IS_LINUX)
  // Restrict the CPU count based on the process's CPU affinity mask, if
  // available.
  cpu_set_t* cpu_set = CPU_ALLOC(num_cpus);
  size_t cpu_set_size = CPU_ALLOC_SIZE(num_cpus);
  int ret = sched_getaffinity(0, cpu_set_size, cpu_set);
  if (ret == 0) {
    num_cpus = CPU_COUNT_S(cpu_set_size, cpu_set);
  }
  CPU_FREE(cpu_set);
#endif  // BUILDFLAG(IS_LINUX)

  return num_cpus;
}

base::LazyInstance<base::internal::LazySysInfoValue<int, NumberOfProcessors>>::
    Leaky g_lazy_number_of_processors = LAZY_INSTANCE_INITIALIZER;
#endif  // !BUILDFLAG(IS_OPENBSD)

int64_t AmountOfVirtualMemory() {
  struct rlimit limit;
  int result = getrlimit(RLIMIT_DATA, &limit);
  if (result != 0) {
    NOTREACHED();
    return 0;
  }
  return limit.rlim_cur == RLIM_INFINITY ? 0 : limit.rlim_cur;
}

base::LazyInstance<
    base::internal::LazySysInfoValue<int64_t, AmountOfVirtualMemory>>::Leaky
    g_lazy_virtual_memory = LAZY_INSTANCE_INITIALIZER;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool IsStatsZeroIfUnlimited(const base::FilePath& path) {
  struct statfs stats;

  if (HANDLE_EINTR(statfs(path.value().c_str(), &stats)) != 0)
    return false;

  switch (stats.f_type) {
    case TMPFS_MAGIC:
    case static_cast<int>(HUGETLBFS_MAGIC):
    case static_cast<int>(RAMFS_MAGIC):
      return true;
  }
  return false;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

bool GetDiskSpaceInfo(const base::FilePath& path,
                      int64_t* available_bytes,
                      int64_t* total_bytes) {
  struct statvfs stats;
  if (HANDLE_EINTR(statvfs(path.value().c_str(), &stats)) != 0)
    return false;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const bool zero_size_means_unlimited =
      stats.f_blocks == 0 && IsStatsZeroIfUnlimited(path);
#else
  const bool zero_size_means_unlimited = false;
#endif

  if (available_bytes) {
    *available_bytes =
        zero_size_means_unlimited
            ? std::numeric_limits<int64_t>::max()
            : static_cast<int64_t>(stats.f_bavail) * stats.f_frsize;
  }

  if (total_bytes) {
    *total_bytes = zero_size_means_unlimited
                       ? std::numeric_limits<int64_t>::max()
                       : static_cast<int64_t>(stats.f_blocks) * stats.f_frsize;
  }
  return true;
}

}  // namespace

namespace base {

#if !BUILDFLAG(IS_OPENBSD)
int SysInfo::NumberOfProcessors() {
  return g_lazy_number_of_processors.Get().value();
}
#endif  // !BUILDFLAG(IS_OPENBSD)

// static
int64_t SysInfo::AmountOfVirtualMemory() {
  return g_lazy_virtual_memory.Get().value();
}

// static
int64_t SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int64_t available;
  if (!GetDiskSpaceInfo(path, &available, nullptr))
    return -1;
  return available;
}

// static
int64_t SysInfo::AmountOfTotalDiskSpace(const FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int64_t total;
  if (!GetDiskSpaceInfo(path, nullptr, &total))
    return -1;
  return total;
}

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
// static
std::string SysInfo::OperatingSystemName() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  return std::string(info.sysname);
}
#endif  //! BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// static
std::string SysInfo::OperatingSystemVersion() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  return std::string(info.release);
}
#endif

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// static
void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    *major_version = 0;
    *minor_version = 0;
    *bugfix_version = 0;
    return;
  }
  int num_read = sscanf(info.release, "%d.%d.%d", major_version, minor_version,
                        bugfix_version);
  if (num_read < 1)
    *major_version = 0;
  if (num_read < 2)
    *minor_version = 0;
  if (num_read < 3)
    *bugfix_version = 0;
}
#endif

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_IOS)
// static
std::string SysInfo::OperatingSystemArchitecture() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  std::string arch(info.machine);
  if (arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686") {
    arch = "x86";
  } else if (arch == "amd64") {
    arch = "x86_64";
  } else if (std::string(info.sysname) == "AIX") {
    arch = "ppc64";
  }
  return arch;
}
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_IOS)

// static
size_t SysInfo::VMAllocationGranularity() {
  return getpagesize();
}

}  // namespace base
