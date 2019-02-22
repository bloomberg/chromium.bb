// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/allocator/partition_allocator/oom_callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

namespace {

constexpr uint32_t kMaxLineSize = 4096;
bool ReadFileContents(int fd, char contents[kMaxLineSize]) {
  lseek(fd, 0, SEEK_SET);
  int res = read(fd, contents, kMaxLineSize - 1);
  if (res <= 0)
    return false;
  contents[res] = '\0';
  return true;
}

// Since the measurement is done every second in background, optimizations are
// in place to get just the metrics we need from the proc files. So, this
// calculation exists here instead of using the cross-process memory-infra code.
bool CalculateProcessMemoryFootprint(int statm_fd,
                                     int status_fd,
                                     uint64_t* private_footprint,
                                     uint64_t* swap_footprint,
                                     uint64_t* vm_size) {
  // Get total resident and shared sizes from statm file.
  static size_t page_size = getpagesize();
  uint64_t resident_pages;
  uint64_t shared_pages;
  uint64_t vm_size_pages;
  char line[kMaxLineSize];
  if (!ReadFileContents(statm_fd, line))
    return false;
  int num_scanned = sscanf(line, "%" SCNu64 " %" SCNu64 " %" SCNu64,
                           &vm_size_pages, &resident_pages, &shared_pages);
  if (num_scanned != 3)
    return false;

  // Get swap size from status file. The format is: VmSwap :  10 kB.
  if (!ReadFileContents(status_fd, line))
    return false;
  char* swap_line = strstr(line, "VmSwap");
  if (!swap_line)
    return false;
  num_scanned = sscanf(swap_line, "VmSwap: %" SCNu64 " kB", swap_footprint);
  if (num_scanned != 1)
    return false;

  *swap_footprint *= 1024;
  *private_footprint =
      (resident_pages - shared_pages) * page_size + *swap_footprint;
  *vm_size = vm_size_pages * page_size;
  return true;
}

// Roughly calculates amount of memory which is used to execute pages.
uint64_t BlinkMemoryWorkloadCalculator() {
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  DCHECK(isolate);
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  // TODO: Add memory usage for worker threads.
  size_t v8_size =
      heap_statistics.total_heap_size() + heap_statistics.malloced_memory();
  size_t blink_gc_size = ProcessHeap::TotalAllocatedObjectSize() +
                         ProcessHeap::TotalMarkedObjectSize();
  size_t partition_alloc_size = WTF::Partitions::TotalSizeOfCommittedPages();
  return v8_size + blink_gc_size + partition_alloc_size;
}

}  // namespace

// static
void CrashMemoryMetricsReporterImpl::Bind(
    mojom::blink::CrashMemoryMetricsReporterRequest request) {
  // This should be called only once per process on RenderProcessWillLaunch.
  DCHECK(!CrashMemoryMetricsReporterImpl::Instance().binding_.is_bound());
  CrashMemoryMetricsReporterImpl::Instance().binding_.Bind(std::move(request));
}

CrashMemoryMetricsReporterImpl& CrashMemoryMetricsReporterImpl::Instance() {
  DEFINE_STATIC_LOCAL(CrashMemoryMetricsReporterImpl,
                      crash_memory_metrics_reporter_impl, ());
  return crash_memory_metrics_reporter_impl;
}

CrashMemoryMetricsReporterImpl::CrashMemoryMetricsReporterImpl()
    : binding_(this) {
  base::SetPartitionAllocOomCallback(
      CrashMemoryMetricsReporterImpl::OnOOMCallback);
}

CrashMemoryMetricsReporterImpl::~CrashMemoryMetricsReporterImpl() = default;

void CrashMemoryMetricsReporterImpl::SetSharedMemory(
    base::UnsafeSharedMemoryRegion shared_metrics_buffer) {
  // This method should be called only once per process.
  DCHECK(!shared_metrics_mapping_.IsValid());
  shared_metrics_mapping_ = shared_metrics_buffer.Map();
}

void CrashMemoryMetricsReporterImpl::WriteIntoSharedMemory(
    const OomInterventionMetrics& metrics) {
  if (!shared_metrics_mapping_.IsValid())
    return;
  auto* metrics_shared =
      shared_metrics_mapping_.GetMemoryAs<OomInterventionMetrics>();
  memcpy(metrics_shared, &metrics, sizeof(OomInterventionMetrics));
}

void CrashMemoryMetricsReporterImpl::OnOOMCallback() {
  // TODO(yuzus: Support allocation failures on other threads as well.
  if (!IsMainThread())
    return;
  // If shared_metrics_mapping_ is not set, it means OnNoMemory happened before
  // initializing render process host sets the shared memory.
  if (!CrashMemoryMetricsReporterImpl::Instance()
           .shared_metrics_mapping_.IsValid())
    return;
  // Else, we can send the allocation_failed bool.
  OomInterventionMetrics metrics;
  // TODO(yuzus): Report this UMA on all the platforms. Currently this is only
  // reported on Android.
  metrics.allocation_failed = 1;  // true
  CrashMemoryMetricsReporterImpl::Instance().WriteIntoSharedMemory(metrics);
}

OomInterventionMetrics
CrashMemoryMetricsReporterImpl::GetCurrentMemoryMetrics() {
  // This can only be called after ResetFileDescriptors().
  DCHECK(statm_fd_.is_valid() && status_fd_.is_valid());

  OomInterventionMetrics metrics = {};
  metrics.current_blink_usage_kb = BlinkMemoryWorkloadCalculator() / 1024;
  uint64_t private_footprint, swap, vm_size;
  if (CalculateProcessMemoryFootprint(statm_fd_.get(), status_fd_.get(),
                                      &private_footprint, &swap, &vm_size)) {
    metrics.current_private_footprint_kb = private_footprint / 1024;
    metrics.current_swap_kb = swap / 1024;
    metrics.current_vm_size_kb = vm_size / 1024;
  }
  metrics.allocation_failed = 0;  // false
  return metrics;
}

bool CrashMemoryMetricsReporterImpl::ResetFileDiscriptors() {
  // See https://goo.gl/KjWnZP For details about why we read these files from
  // sandboxed renderer. Keep these files open when detection is enabled.
  if (!statm_fd_.is_valid())
    statm_fd_.reset(open("/proc/self/statm", O_RDONLY));
  if (!status_fd_.is_valid())
    status_fd_.reset(open("/proc/self/status", O_RDONLY));
  return !statm_fd_.is_valid() || !status_fd_.is_valid();
}

}  // namespace blink
