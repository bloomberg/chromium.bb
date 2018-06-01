// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
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
                                     uint64_t* swap_footprint) {
  // Get total resident and shared sizes from statm file.
  static size_t page_size = getpagesize();
  uint64_t resident_pages;
  uint64_t shared_pages;
  char line[kMaxLineSize];
  if (!ReadFileContents(statm_fd, line))
    return false;
  int num_scanned =
      sscanf(line, "%*s %" SCNu64 " %" SCNu64, &resident_pages, &shared_pages);
  if (num_scanned != 2)
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
  return true;
}

// Roughly caclculates amount of memory which is used to execute pages.
uint64_t BlinkMemoryWorkloadCaculator() {
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
void OomInterventionImpl::Create(mojom::blink::OomInterventionRequest request) {
  mojo::MakeStrongBinding(std::make_unique<OomInterventionImpl>(),
                          std::move(request));
}

OomInterventionImpl::OomInterventionImpl()
    : timer_(Platform::Current()->MainThread()->GetTaskRunner(),
             this,
             &OomInterventionImpl::Check) {}

OomInterventionImpl::~OomInterventionImpl() {}

void OomInterventionImpl::StartDetection(
    mojom::blink::OomInterventionHostPtr host,
    base::UnsafeSharedMemoryRegion shared_metrics_buffer,
    mojom::blink::DetectionArgsPtr detection_args,
    bool trigger_intervention) {
  host_ = std::move(host);
  shared_metrics_buffer_ = shared_metrics_buffer.Map();

  // See https://goo.gl/KjWnZP For details about why we read these files from
  // sandboxed renderer. Keep these files open when detection is enabled.
  if (!statm_fd_.is_valid())
    statm_fd_.reset(open("/proc/self/statm", O_RDONLY));
  if (!status_fd_.is_valid())
    status_fd_.reset(open("/proc/self/status", O_RDONLY));
  // Disable intervention if we cannot get memory details of current process.
  // TODO(ssid): Add UMA here to make sure we don't stop disable intervention
  // more often than expected.
  if (!statm_fd_.is_valid() || !status_fd_.is_valid())
    return;

  detection_args_ = std::move(detection_args);
  trigger_intervention_ = trigger_intervention;

  timer_.Start(TimeDelta(), TimeDelta::FromSeconds(1), FROM_HERE);
}

OomInterventionMetrics OomInterventionImpl::GetCurrentMemoryMetrics() {
  OomInterventionMetrics metrics = {};
  metrics.current_blink_usage_kb = BlinkMemoryWorkloadCaculator();
  uint64_t private_footprint, swap;
  if (CalculateProcessMemoryFootprint(statm_fd_.get(), status_fd_.get(),
                                      &private_footprint, &swap)) {
    metrics.current_private_footprint_kb = private_footprint / 1024;
    metrics.current_swap_kb = swap / 1024;
  }
  return metrics;
}

void OomInterventionImpl::Check(TimerBase*) {
  DCHECK(host_);
  DCHECK(statm_fd_.is_valid());
  DCHECK(status_fd_.is_valid());

  OomInterventionMetrics current_memory = GetCurrentMemoryMetrics();
  bool oom_detected = false;

  oom_detected |= detection_args_->blink_workload_threshold > 0 &&
                  current_memory.current_blink_usage_kb * 1024 >
                      detection_args_->blink_workload_threshold;
  oom_detected |= detection_args_->private_footprint_threshold > 0 &&
                  current_memory.current_private_footprint_kb * 1024 >
                      detection_args_->private_footprint_threshold;
  oom_detected |=
      detection_args_->swap_threshold > 0 &&
      current_memory.current_swap_kb * 1024 > detection_args_->swap_threshold;

  if (oom_detected) {
    host_->OnHighMemoryUsage(trigger_intervention_);

    if (trigger_intervention_) {
      // The ScopedPagePauser is destroyed when the intervention is declined and
      // mojo strong binding is disconnected.
      pauser_.reset(new ScopedPagePauser);
    }
  }

  OomInterventionMetrics* metrics_shared =
      static_cast<OomInterventionMetrics*>(shared_metrics_buffer_.memory());
  memcpy(metrics_shared, &current_memory, sizeof(OomInterventionMetrics));
}

}  // namespace blink
