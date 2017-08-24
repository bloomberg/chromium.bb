// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MemoryCoordinator.h"

#include "base/sys_info.h"
#include "build/build_config.h"
#include "platform/WebTaskRunner.h"
#include "platform/fonts/FontGlobalContext.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/allocator/Partitions.h"
#include "public/platform/WebThread.h"
#include "public/web/WebKit.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#endif

namespace blink {

// Wrapper function defined in WebKit.h
void DecommitFreeableMemory() {
  WTF::Partitions::DecommitFreeableMemory();
}

// static
float MemoryCoordinator::approximated_device_memory_gb_ = 0.0;
bool MemoryCoordinator::is_low_end_device_ = false;
int64_t MemoryCoordinator::physical_memory_mb_ = 0;

// static
bool MemoryCoordinator::IsLowEndDevice() {
  return is_low_end_device_;
}

// static
int64_t MemoryCoordinator::GetPhysicalMemoryMB() {
  return physical_memory_mb_;
}

// static
void MemoryCoordinator::SetPhysicalMemoryMBForTesting(
    int64_t physical_memory_mb) {
  physical_memory_mb_ = physical_memory_mb;
  CalculateAndSetApproximatedDeviceMemory();
}

// static
float MemoryCoordinator::GetApproximatedDeviceMemory() {
  return approximated_device_memory_gb_;
}

// static
void MemoryCoordinator::CalculateAndSetApproximatedDeviceMemory() {
  // The calculations in this method are described in the specifcations:
  // https://github.com/WICG/device-memory.
  DCHECK_GT(physical_memory_mb_, 0);
  int lower_bound = physical_memory_mb_;
  int power = 0;

  // Extract the most significant 2-bits and their location.
  while (lower_bound >= 4) {
    lower_bound >>= 1;
    power++;
  }
  // The lower_bound value is either 0b10 or 0b11.
  DCHECK(lower_bound & 2);

  int64_t upper_bound = lower_bound + 1;
  lower_bound = lower_bound << power;
  upper_bound = upper_bound << power;

  // Find the closest bound, and convert it to GB.
  if (physical_memory_mb_ - lower_bound <= upper_bound - physical_memory_mb_)
    approximated_device_memory_gb_ = static_cast<float>(lower_bound) / 1024.0;
  else
    approximated_device_memory_gb_ = static_cast<float>(upper_bound) / 1024.0;
}

// static
bool MemoryCoordinator::IsCurrentlyLowMemory() {
#if defined(OS_ANDROID)
  return base::android::SysUtils::IsCurrentlyLowMemory();
#else
  return false;
#endif
}

// static
void MemoryCoordinator::Initialize() {
  is_low_end_device_ = ::base::SysInfo::IsLowEndDevice();
  physical_memory_mb_ = ::base::SysInfo::AmountOfPhysicalMemoryMB();
  CalculateAndSetApproximatedDeviceMemory();
}

// static
void MemoryCoordinator::SetIsLowEndDeviceForTesting(bool is_low_end_device) {
  is_low_end_device_ = is_low_end_device;
}

// static
MemoryCoordinator& MemoryCoordinator::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<MemoryCoordinator>, external,
                      (new MemoryCoordinator));
  DCHECK(IsMainThread());
  return *external.Get();
}

void MemoryCoordinator::RegisterThread(WebThread* thread) {
  MemoryCoordinator::Instance().web_threads_.insert(thread);
}

void MemoryCoordinator::UnregisterThread(WebThread* thread) {
  MemoryCoordinator::Instance().web_threads_.erase(thread);
}

MemoryCoordinator::MemoryCoordinator() {}

void MemoryCoordinator::RegisterClient(MemoryCoordinatorClient* client) {
  DCHECK(IsMainThread());
  DCHECK(client);
  DCHECK(!clients_.Contains(client));
  clients_.insert(client);
}

void MemoryCoordinator::UnregisterClient(MemoryCoordinatorClient* client) {
  DCHECK(IsMainThread());
  clients_.erase(client);
}

void MemoryCoordinator::OnMemoryPressure(WebMemoryPressureLevel level) {
  TRACE_EVENT0("blink", "MemoryCoordinator::onMemoryPressure");
  for (auto& client : clients_)
    client->OnMemoryPressure(level);
  if (level == kWebMemoryPressureLevelCritical)
    ClearMemory();
  WTF::Partitions::DecommitFreeableMemory();
}

void MemoryCoordinator::OnMemoryStateChange(MemoryState state) {
  for (auto& client : clients_)
    client->OnMemoryStateChange(state);
}

void MemoryCoordinator::OnPurgeMemory() {
  for (auto& client : clients_)
    client->OnPurgeMemory();
  // Don't call clearMemory() because font cache invalidation always causes full
  // layout. This increases tab switching cost significantly (e.g.
  // en.wikipedia.org/wiki/Wikipedia). So we should not invalidate the font
  // cache in purge+throttle.
  ImageDecodingStore::Instance().Clear();
  WTF::Partitions::DecommitFreeableMemory();

  // Thread-specific data never issues a layout, so we are safe here.
  for (auto thread : web_threads_) {
    if (!thread->GetWebTaskRunner())
      continue;

    thread->GetWebTaskRunner()->PostTask(
        FROM_HERE, WTF::Bind(MemoryCoordinator::ClearThreadSpecificMemory));
  }
}

void MemoryCoordinator::ClearMemory() {
  // Clear the image cache.
  // TODO(tasak|bashi): Make ImageDecodingStore and FontCache be
  // MemoryCoordinatorClients rather than clearing caches here.
  ImageDecodingStore::Instance().Clear();
  FontGlobalContext::ClearMemory();
}

void MemoryCoordinator::ClearThreadSpecificMemory() {
  FontGlobalContext::ClearMemory();
}

DEFINE_TRACE(MemoryCoordinator) {
  visitor->Trace(clients_);
}

}  // namespace blink
