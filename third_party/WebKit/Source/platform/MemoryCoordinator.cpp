// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MemoryCoordinator.h"

#include "base/sys_info.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "wtf/allocator/Partitions.h"

namespace blink {

// static
bool MemoryCoordinator::is_low_end_device_ = false;

// static
bool MemoryCoordinator::IsLowEndDevice() {
  return is_low_end_device_;
}

// static
void MemoryCoordinator::Initialize() {
  is_low_end_device_ = ::base::SysInfo::IsLowEndDevice();
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
}

void MemoryCoordinator::ClearMemory() {
  // Clear the image cache.
  // TODO(tasak|bashi): Make ImageDecodingStore and FontCache be
  // MemoryCoordinatorClients rather than clearing caches here.
  ImageDecodingStore::Instance().Clear();
  FontCache::GetFontCache()->Invalidate();
}

DEFINE_TRACE(MemoryCoordinator) {
  visitor->Trace(clients_);
}

}  // namespace blink
