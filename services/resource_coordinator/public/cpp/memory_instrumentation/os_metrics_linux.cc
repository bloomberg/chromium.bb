// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdint.h>
#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace memory_instrumentation {

namespace {

const uint32_t kMaxLineSize = 4096;

base::ScopedFD OpenStatm(base::ProcessId pid) {
  std::string name =
      "/proc/" +
      (pid == base::kNullProcessId ? "self" : base::IntToString(pid)) +
      "/statm";
  base::ScopedFD fd = base::ScopedFD(open(name.c_str(), O_RDONLY));
  DCHECK(fd.is_valid());
  return fd;
}

bool GetResidentAndSharedPagesFromStatmFile(int fd,
                                            uint64_t* resident_pages,
                                            uint64_t* shared_pages) {
  lseek(fd, 0, SEEK_SET);
  char line[kMaxLineSize];
  int res = read(fd, line, kMaxLineSize - 1);
  if (res <= 0)
    return false;
  line[res] = '\0';
  int num_scanned =
      sscanf(line, "%*s %" SCNu64 " %" SCNu64, resident_pages, shared_pages);
  return num_scanned == 2;
}

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessId pid) {
  if (pid == base::kNullProcessId) {
    return base::ProcessMetrics::CreateCurrentProcessMetrics();
  }
  return base::ProcessMetrics::CreateProcessMetrics(pid);
}

}  // namespace

void FillOSMemoryDump(base::ProcessId pid, mojom::RawOSMemDump* dump) {
  base::ScopedFD autoclose = OpenStatm(pid);
  int statm_fd = autoclose.get();

  if (statm_fd == -1)
    return;

  uint64_t resident_pages;
  uint64_t shared_pages;
  bool success = GetResidentAndSharedPagesFromStatmFile(
      statm_fd, &resident_pages, &shared_pages);

  if (!success)
    return;

  auto process_metrics = CreateProcessMetrics(pid);

  const static size_t page_size = base::GetPageSize();
  uint64_t rss_anon_bytes = (resident_pages - shared_pages) * page_size;
  uint64_t vm_swap_bytes = (resident_pages - shared_pages) * page_size;

  dump->platform_private_footprint.rss_anon_bytes = rss_anon_bytes;
  dump->platform_private_footprint.vm_swap_bytes = vm_swap_bytes;
  dump->resident_set_kb = process_metrics->GetWorkingSetSize() / 1024;
}

}  // namespace memory_instrumentation
