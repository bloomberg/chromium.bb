// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include <psapi.h>
#include <tchar.h>
#include <windows.h>

#include <base/strings/sys_string_conversions.h>
#include <base/win/pe_image.h>
#include <base/win/win_util.h>
#include "base/process/process_metrics.h"

namespace memory_instrumentation {

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessId pid,
                                 mojom::RawOSMemDump* dump) {
  // Creating process metrics for child processes in mac or windows requires
  // additional information like ProcessHandle or port provider.
  DCHECK_EQ(base::kNullProcessId, pid);
  auto process_metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();

  size_t private_bytes = 0;
  process_metrics->GetMemoryBytes(&private_bytes, nullptr);
  dump->platform_private_footprint.private_bytes = private_bytes;
  dump->resident_set_kb = process_metrics->GetWorkingSetSize() / 1024;
  return true;
}

// static
bool OSMetrics::FillProcessMemoryMaps(
    base::ProcessId pid,
    base::trace_event::ProcessMemoryDump* pmd) {
  std::vector<HMODULE> modules;
  if (!base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &modules))
    return false;

  // Query the base address for each module, and attach it to the dump.
  for (size_t i = 0; i < modules.size(); ++i) {
    wchar_t module_name[MAX_PATH];
    if (!::GetModuleFileName(modules[i], module_name, MAX_PATH))
      continue;

    MODULEINFO module_info;
    if (!::GetModuleInformation(::GetCurrentProcess(), modules[i], &module_info,
                                sizeof(MODULEINFO))) {
      continue;
    }
    base::trace_event::ProcessMemoryMaps::VMRegion region;
    region.size_in_bytes = module_info.SizeOfImage;
    region.mapped_file = base::SysWideToNativeMB(module_name);
    region.start_address = reinterpret_cast<uint64_t>(module_info.lpBaseOfDll);

    // The PE header field |TimeDateStamp| is required to build the PE code
    // identifier which is used as a key to query symbols servers.
    base::win::PEImage pe_image(module_info.lpBaseOfDll);
    region.module_timestamp = pe_image.GetNTHeaders()->FileHeader.TimeDateStamp;

    pmd->process_mmaps()->AddVMRegion(region);
  }
  if (!pmd->process_mmaps()->vm_regions().empty())
    pmd->set_has_process_mmaps();
  return true;
}

}  // namespace memory_instrumentation
