// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor.h"

#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

namespace {
constexpr TimeDelta kPingInterval = TimeDelta::FromSeconds(1);
}

MemoryUsageMonitor::MemoryUsageMonitor()
    : timer_(Thread::MainThread()->GetTaskRunner(),
             this,
             &MemoryUsageMonitor::TimerFired) {}

void MemoryUsageMonitor::AddObserver(Observer* observer) {
  StartMonitoringIfNeeded();
  observers_.AddObserver(observer);
}

void MemoryUsageMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MemoryUsageMonitor::StartMonitoringIfNeeded() {
  if (timer_.IsActive())
    return;
  timer_.StartRepeating(kPingInterval, FROM_HERE);
}

void MemoryUsageMonitor::StopMonitoring() {
  timer_.Stop();
}

MemoryUsage MemoryUsageMonitor::GetCurrentMemoryUsage() {
  MemoryUsage usage;
  GetV8MemoryUsage(usage);
  GetBlinkMemoryUsage(usage);
  GetProcessMemoryUsage(usage);
  return usage;
}

void MemoryUsageMonitor::GetV8MemoryUsage(MemoryUsage& usage) {
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  DCHECK(isolate);
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  // TODO: Add memory usage for worker threads.
  usage.v8_bytes =
      heap_statistics.total_heap_size() + heap_statistics.malloced_memory();
}

void MemoryUsageMonitor::GetBlinkMemoryUsage(MemoryUsage& usage) {
  usage.blink_gc_bytes = ProcessHeap::TotalAllocatedObjectSize() +
                         ProcessHeap::TotalMarkedObjectSize();
  usage.partition_alloc_bytes = WTF::Partitions::TotalSizeOfCommittedPages();
}

void MemoryUsageMonitor::TimerFired(TimerBase*) {
  MemoryUsage usage = GetCurrentMemoryUsage();
  for (auto& observer : observers_)
    observer.OnMemoryPing(usage);
  if (!observers_.might_have_observers())
    StopMonitoring();
}

}  // namespace blink
