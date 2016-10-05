// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MemoryCoordinator.h"

#include "platform/fonts/FontCache.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/tracing/TraceEvent.h"
#include "wtf/allocator/Partitions.h"

namespace blink {

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
  m_clients.add(client);
}

void MemoryCoordinator::unregisterClient(MemoryCoordinatorClient* client) {
  DCHECK(isMainThread());
  m_clients.remove(client);
}

void MemoryCoordinator::prepareToSuspend() {
  for (auto& client : m_clients)
    client->prepareToSuspend();
  WTF::Partitions::decommitFreeableMemory();
}

void MemoryCoordinator::onMemoryPressure(WebMemoryPressureLevel level) {
  TRACE_EVENT0("blink", "MemoryCoordinator::onMemoryPressure");
  for (auto& client : m_clients)
    client->onMemoryPressure(level);
  if (level == WebMemoryPressureLevelCritical) {
    // Clear the image cache.
    // TODO(tasak|bashi): Make ImageDecodingStore and FontCache be
    // MemoryCoordinatorClients rather than clearing caches here.
    ImageDecodingStore::instance().clear();
    FontCache::fontCache()->invalidate();
  }
  WTF::Partitions::decommitFreeableMemory();
}

DEFINE_TRACE(MemoryCoordinator) {
  visitor->trace(m_clients);
}

}  // namespace blink
