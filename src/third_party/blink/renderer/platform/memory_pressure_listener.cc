// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/memory_pressure_listener.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#endif

namespace blink {

// Wrapper function defined in WebKit.h
void DecommitFreeableMemory() {
  WTF::Partitions::DecommitFreeableMemory();
}

// static
bool MemoryPressureListenerRegistry::is_low_end_device_ = false;

// static
bool MemoryPressureListenerRegistry::IsLowEndDevice() {
  return is_low_end_device_;
}

// static
bool MemoryPressureListenerRegistry::IsCurrentlyLowMemory() {
#if defined(OS_ANDROID)
  return base::android::SysUtils::IsCurrentlyLowMemory();
#else
  return false;
#endif
}

// static
void MemoryPressureListenerRegistry::Initialize() {
  is_low_end_device_ = ::base::SysInfo::IsLowEndDevice();
  ApproximatedDeviceMemory::Initialize();
}

// static
void MemoryPressureListenerRegistry::SetIsLowEndDeviceForTesting(
    bool is_low_end_device) {
  is_low_end_device_ = is_low_end_device;
}

// static
MemoryPressureListenerRegistry& MemoryPressureListenerRegistry::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CrossThreadPersistent<MemoryPressureListenerRegistry>, external,
      (MakeGarbageCollected<MemoryPressureListenerRegistry>()));
  return *external.Get();
}

void MemoryPressureListenerRegistry::RegisterThread(Thread* thread) {
  MutexLocker lock(threads_mutex_);
  threads_.insert(thread);
}

void MemoryPressureListenerRegistry::UnregisterThread(Thread* thread) {
  MutexLocker lock(threads_mutex_);
  threads_.erase(thread);
}

MemoryPressureListenerRegistry::MemoryPressureListenerRegistry() = default;

void MemoryPressureListenerRegistry::RegisterClient(
    MemoryPressureListener* client) {
  DCHECK(IsMainThread());
  DCHECK(client);
  DCHECK(!clients_.Contains(client));
  clients_.insert(client);
}

void MemoryPressureListenerRegistry::UnregisterClient(
    MemoryPressureListener* client) {
  DCHECK(IsMainThread());
  clients_.erase(client);
}

void MemoryPressureListenerRegistry::OnMemoryPressure(
    WebMemoryPressureLevel level) {
  TRACE_EVENT0("blink", "MemoryPressureListenerRegistry::onMemoryPressure");
  for (auto& client : clients_)
    client->OnMemoryPressure(level);
  WTF::Partitions::DecommitFreeableMemory();
}

void MemoryPressureListenerRegistry::OnPurgeMemory() {
  for (auto& client : clients_)
    client->OnPurgeMemory();
  ImageDecodingStore::Instance().Clear();
  WTF::Partitions::DecommitFreeableMemory();

  // Thread-specific data never issues a layout, so we are safe here.
  MutexLocker lock(threads_mutex_);
  for (auto* thread : threads_) {
    if (!thread->GetTaskRunner())
      continue;

    PostCrossThreadTask(
        *thread->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            MemoryPressureListenerRegistry::ClearThreadSpecificMemory));
  }
}

void MemoryPressureListenerRegistry::ClearThreadSpecificMemory() {
  FontGlobalContext::ClearMemory();
}

void MemoryPressureListenerRegistry::Trace(blink::Visitor* visitor) {
  visitor->Trace(clients_);
}

}  // namespace blink
