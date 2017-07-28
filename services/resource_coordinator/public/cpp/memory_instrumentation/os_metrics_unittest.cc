// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include "base/files/file_util.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include <libgen.h>
#include <mach-o/dyld.h>
#endif

#if defined(OS_WIN)
#include <base/strings/sys_string_conversions.h>
#endif

namespace memory_instrumentation {

#if defined(OS_LINUX) || defined(OS_ANDROID)
namespace {
const char kTestSmaps1[] =
    "00400000-004be000 r-xp 00000000 fc:01 1234              /file/1\n"
    "Size:                760 kB\n"
    "Rss:                 296 kB\n"
    "Pss:                 162 kB\n"
    "Shared_Clean:        228 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:        68 kB\n"
    "Referenced:          296 kB\n"
    "Anonymous:            68 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  4 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd\n"
    "ff000000-ff800000 -w-p 00001080 fc:01 0            /file/name with space\n"
    "Size:                  0 kB\n"
    "Rss:                 192 kB\n"
    "Pss:                 128 kB\n"
    "Shared_Clean:        120 kB\n"
    "Shared_Dirty:          4 kB\n"
    "Private_Clean:        60 kB\n"
    "Private_Dirty:         8 kB\n"
    "Referenced:          296 kB\n"
    "Anonymous:             0 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd";

const char kTestSmaps2[] =
    // An invalid region, with zero size and overlapping with the last one
    // (See crbug.com/461237).
    "7fe7ce79c000-7fe7ce79c000 ---p 00000000 00:00 0 \n"
    "Size:                  4 kB\n"
    "Rss:                   0 kB\n"
    "Pss:                   0 kB\n"
    "Shared_Clean:          0 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:         0 kB\n"
    "Referenced:            0 kB\n"
    "Anonymous:             0 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd\n"
    // A invalid region with its range going backwards.
    "00400000-00200000 ---p 00000000 00:00 0 \n"
    "Size:                  4 kB\n"
    "Rss:                   0 kB\n"
    "Pss:                   0 kB\n"
    "Shared_Clean:          0 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:         0 kB\n"
    "Referenced:            0 kB\n"
    "Anonymous:             0 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd\n"
    // A good anonymous region at the end.
    "7fe7ce79c000-7fe7ce7a8000 ---p 00000000 00:00 0 \n"
    "Size:                 48 kB\n"
    "Rss:                  40 kB\n"
    "Pss:                  32 kB\n"
    "Shared_Clean:         16 kB\n"
    "Shared_Dirty:         12 kB\n"
    "Private_Clean:         8 kB\n"
    "Private_Dirty:         4 kB\n"
    "Referenced:           40 kB\n"
    "Anonymous:            16 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd wr mr mw me ac sd\n";

void CreateTempFileWithContents(const char* contents, base::ScopedFILE* file) {
  base::FilePath temp_path;
  FILE* temp_file = CreateAndOpenTemporaryFile(&temp_path);
  file->reset(temp_file);
  ASSERT_TRUE(temp_file);

  ASSERT_TRUE(
      base::WriteFileDescriptor(fileno(temp_file), contents, strlen(contents)));
}

}  // namespace
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

TEST(OSMetricsTest, GivesNonZeroResults) {
  base::ProcessId pid = base::kNullProcessId;
  mojom::RawOSMemDump dump;
  EXPECT_TRUE(OSMetrics::FillOSMemoryDump(pid, &dump));
#if defined(OS_LINUX) || defined(OS_ANDROID)
  EXPECT_GT(dump.platform_private_footprint.rss_anon_bytes, 0u);
#elif defined(OS_WIN)
  EXPECT_GT(dump.platform_private_footprint.private_bytes, 0u);
#elif defined(OS_MACOSX)
  EXPECT_GT(dump.platform_private_footprint.internal_bytes, 0u);
#endif
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
TEST(OSMetricsTest, ParseProcSmaps) {
  const uint32_t kProtR =
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsRead;
  const uint32_t kProtW =
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsWrite;
  const uint32_t kProtX =
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsExec;
  const base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};

  // Emulate an empty /proc/self/smaps.
  base::trace_event::ProcessMemoryDump pmd_invalid(nullptr /* session_state */,
                                                   dump_args);
  base::ScopedFILE empty_file(OpenFile(base::FilePath("/dev/null"), "r"));
  ASSERT_TRUE(empty_file.get());
  OSMetrics::SetProcSmapsForTesting(empty_file.get());
  OSMetrics::FillProcessMemoryMapsDeprecated(base::kNullProcessId,
                                             &pmd_invalid);
  ASSERT_FALSE(pmd_invalid.has_process_mmaps());

  // Parse the 1st smaps file.
  base::trace_event::ProcessMemoryDump pmd_1(nullptr /* session_state */,
                                             dump_args);
  base::ScopedFILE temp_file1;
  CreateTempFileWithContents(kTestSmaps1, &temp_file1);
  OSMetrics::SetProcSmapsForTesting(temp_file1.get());
  OSMetrics::FillProcessMemoryMapsDeprecated(base::kNullProcessId, &pmd_1);
  ASSERT_TRUE(pmd_1.has_process_mmaps());
  const auto& regions_1 = pmd_1.process_mmaps()->vm_regions();
  ASSERT_EQ(2UL, regions_1.size());

  EXPECT_EQ(0x00400000UL, regions_1[0].start_address);
  EXPECT_EQ(0x004be000UL - 0x00400000UL, regions_1[0].size_in_bytes);
  EXPECT_EQ(kProtR | kProtX, regions_1[0].protection_flags);
  EXPECT_EQ("/file/1", regions_1[0].mapped_file);
  EXPECT_EQ(162 * 1024UL, regions_1[0].byte_stats_proportional_resident);
  EXPECT_EQ(228 * 1024UL, regions_1[0].byte_stats_shared_clean_resident);
  EXPECT_EQ(0UL, regions_1[0].byte_stats_shared_dirty_resident);
  EXPECT_EQ(0UL, regions_1[0].byte_stats_private_clean_resident);
  EXPECT_EQ(68 * 1024UL, regions_1[0].byte_stats_private_dirty_resident);
  EXPECT_EQ(4 * 1024UL, regions_1[0].byte_stats_swapped);

  EXPECT_EQ(0xff000000UL, regions_1[1].start_address);
  EXPECT_EQ(0xff800000UL - 0xff000000UL, regions_1[1].size_in_bytes);
  EXPECT_EQ(kProtW, regions_1[1].protection_flags);
  EXPECT_EQ("/file/name with space", regions_1[1].mapped_file);
  EXPECT_EQ(128 * 1024UL, regions_1[1].byte_stats_proportional_resident);
  EXPECT_EQ(120 * 1024UL, regions_1[1].byte_stats_shared_clean_resident);
  EXPECT_EQ(4 * 1024UL, regions_1[1].byte_stats_shared_dirty_resident);
  EXPECT_EQ(60 * 1024UL, regions_1[1].byte_stats_private_clean_resident);
  EXPECT_EQ(8 * 1024UL, regions_1[1].byte_stats_private_dirty_resident);
  EXPECT_EQ(0 * 1024UL, regions_1[1].byte_stats_swapped);

  // Parse the 2nd smaps file.
  base::trace_event::ProcessMemoryDump pmd_2(nullptr /* session_state */,
                                             dump_args);
  base::ScopedFILE temp_file2;
  CreateTempFileWithContents(kTestSmaps2, &temp_file2);
  OSMetrics::SetProcSmapsForTesting(temp_file2.get());
  OSMetrics::FillProcessMemoryMapsDeprecated(base::kNullProcessId, &pmd_2);
  ASSERT_TRUE(pmd_2.has_process_mmaps());
  const auto& regions_2 = pmd_2.process_mmaps()->vm_regions();
  ASSERT_EQ(1UL, regions_2.size());
  EXPECT_EQ(0x7fe7ce79c000UL, regions_2[0].start_address);
  EXPECT_EQ(0x7fe7ce7a8000UL - 0x7fe7ce79c000UL, regions_2[0].size_in_bytes);
  EXPECT_EQ(0U, regions_2[0].protection_flags);
  EXPECT_EQ("", regions_2[0].mapped_file);
  EXPECT_EQ(32 * 1024UL, regions_2[0].byte_stats_proportional_resident);
  EXPECT_EQ(16 * 1024UL, regions_2[0].byte_stats_shared_clean_resident);
  EXPECT_EQ(12 * 1024UL, regions_2[0].byte_stats_shared_dirty_resident);
  EXPECT_EQ(8 * 1024UL, regions_2[0].byte_stats_private_clean_resident);
  EXPECT_EQ(4 * 1024UL, regions_2[0].byte_stats_private_dirty_resident);
  EXPECT_EQ(0 * 1024UL, regions_2[0].byte_stats_swapped);
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
void DummyFunction() {}

TEST(OSMetricsTest, TestWinModuleReading) {
  using VMRegion = base::trace_event::ProcessMemoryMaps::VMRegion;

  base::trace_event::MemoryDumpArgs args;
  base::trace_event::ProcessMemoryDump dump(nullptr, args);
  ASSERT_TRUE(
      OSMetrics::FillProcessMemoryMapsDeprecated(base::kNullProcessId, &dump));
  ASSERT_TRUE(dump.has_process_mmaps());

  wchar_t module_name[MAX_PATH];
  DWORD result = GetModuleFileName(nullptr, module_name, MAX_PATH);
  ASSERT_TRUE(result);
  std::string executable_name = base::SysWideToNativeMB(module_name);

  HMODULE module_containing_dummy = nullptr;
  uintptr_t dummy_function_address =
      reinterpret_cast<uintptr_t>(&DummyFunction);
  result = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                             reinterpret_cast<LPCWSTR>(dummy_function_address),
                             &module_containing_dummy);
  ASSERT_TRUE(result);
  result = GetModuleFileName(nullptr, module_name, MAX_PATH);
  ASSERT_TRUE(result);
  std::string module_containing_dummy_name =
      base::SysWideToNativeMB(module_name);

  bool found_executable = false;
  bool found_region_with_dummy = false;
  for (const VMRegion& region : dump.process_mmaps()->vm_regions()) {
    // We add a region just for byte_stats_proportional_resident which
    // is empty other than that one stat.
    if (region.byte_stats_proportional_resident > 0) {
      EXPECT_EQ(0u, region.start_address);
      EXPECT_EQ(0u, region.size_in_bytes);
      continue;
    }
    EXPECT_NE(0u, region.start_address);
    EXPECT_NE(0u, region.size_in_bytes);

    if (region.mapped_file.find(executable_name) != std::string::npos)
      found_executable = true;

    if (dummy_function_address >= region.start_address &&
        dummy_function_address < region.start_address + region.size_in_bytes) {
      found_region_with_dummy = true;
      EXPECT_EQ(module_containing_dummy_name, region.mapped_file);
    }
  }
  EXPECT_TRUE(found_executable);
  EXPECT_TRUE(found_region_with_dummy);
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
TEST(OSMetricsTest, TestMachOReading) {
  using VMRegion = base::trace_event::ProcessMemoryMaps::VMRegion;
  base::trace_event::MemoryDumpArgs args;
  base::trace_event::ProcessMemoryDump dump(nullptr, args);
  ASSERT_TRUE(
      OSMetrics::FillProcessMemoryMapsDeprecated(base::kNullProcessId, &dump));
  ASSERT_TRUE(dump.has_process_mmaps());
  uint32_t size = 100;
  char full_path[size];
  int result = _NSGetExecutablePath(full_path, &size);
  ASSERT_EQ(0, result);
  std::string name = basename(full_path);

  uint64_t components_unittests_resident_pages = 0;
  bool found_appkit = false;
  for (const VMRegion& region : dump.process_mmaps()->vm_regions()) {
    EXPECT_NE(0u, region.start_address);
    EXPECT_NE(0u, region.size_in_bytes);

    EXPECT_LT(region.size_in_bytes, 1ull << 32);
    uint32_t required_protection_flags =
        VMRegion::kProtectionFlagsRead | VMRegion::kProtectionFlagsExec;
    if (region.mapped_file.find(name) != std::string::npos &&
        region.protection_flags == required_protection_flags) {
      components_unittests_resident_pages +=
          region.byte_stats_private_dirty_resident +
          region.byte_stats_shared_dirty_resident +
          region.byte_stats_private_clean_resident +
          region.byte_stats_shared_clean_resident;
    }

    if (region.mapped_file.find("AppKit") != std::string::npos) {
      found_appkit = true;
    }
  }
  EXPECT_GT(components_unittests_resident_pages, 0u);
  EXPECT_TRUE(found_appkit);
}
#endif  // defined(OS_MACOSX)

}  // namespace memory_instrumentation
