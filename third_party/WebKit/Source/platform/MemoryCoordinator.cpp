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
bool MemoryCoordinator::s_isLowEndDevice = false;

// static
bool MemoryCoordinator::isLowEndDevice() {
  return s_isLowEndDevice;
}

// static
void MemoryCoordinator::initialize() {
  s_isLowEndDevice = ::base::SysInfo::IsLowEndDevice();
}

// static
void MemoryCoordinator::setIsLowEndDeviceForTesting(bool isLowEndDevice) {
  s_isLowEndDevice = isLowEndDevice;
}

// static
MemoryCoordinator& MemoryCoordinator::instance() {
  DEFINE_STATIC_LOCAL(Persistent<MemoryCoordinator>, external,
                      (new MemoryCoordinator));
  DCHECK(isMainThread());
  return *external.get();
}


MemoryCoordinator::MemoryCoordinator() {}

void MemoryCoordinator::registerClient(MemoryCoordinatorClient* client) {
  DCHECK(isMainThread());
  DCHECK(client);
  DCHECK(!m_clients.contains(client));
  m_clients.insert(client);
}

void MemoryCoordinator::unregisterClient(MemoryCoordinatorClient* client) {
  DCHECK(isMainThread());
  m_clients.erase(client);
}

void MemoryCoordinator::onMemoryPressure(WebMemoryPressureLevel level) {
  TRACE_EVENT0("blink", "MemoryCoordinator::onMemoryPressure");
  for (auto& client : m_clients)
    client->onMemoryPressure(level);
  if (level == WebMemoryPressureLevelCritical)
    clearMemory();
  WTF::Partitions::decommitFreeableMemory();
}

void MemoryCoordinator::onMemoryStateChange(MemoryState state) {
  for (auto& client : m_clients)
    client->onMemoryStateChange(state);
}

void MemoryCoordinator::onPurgeMemory() {
  // Don't call clearMemory() because font cache invalidation always causes full
  // layout. This increases tab switching cost significantly (e.g.
  // en.wikipedia.org/wiki/Wikipedia). So we should not invalidate the font
  // cache in purge+throttle.
  ImageDecodingStore::instance().clear();
  WTF::Partitions::decommitFreeableMemory();
}

void MemoryCoordinator::clearMemory() {
  // Clear the image cache.
  // TODO(tasak|bashi): Make ImageDecodingStore and FontCache be
  // MemoryCoordinatorClients rather than clearing caches here.
  ImageDecodingStore::instance().clear();
  FontCache::fontCache()->invalidate();
}

DEFINE_TRACE(MemoryCoordinator) {
  visitor->trace(m_clients);
}

}  // namespace blink
