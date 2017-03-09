// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapperClipCache.h"

#include "platform/graphics/paint/FloatClipRect.h"

namespace blink {

// All clip caches invalidate themselves by tracking a local cache generation,
// and invalidating their cache if their cache generation disagrees with
// s_clipCacheGeneration.
static unsigned s_clipCacheGeneration = 0;

GeometryMapperClipCache::GeometryMapperClipCache()
    : m_cacheGeneration(s_clipCacheGeneration) {}

void GeometryMapperClipCache::clearCache() {
  s_clipCacheGeneration++;
}

void GeometryMapperClipCache::invalidateCacheIfNeeded() {
  if (m_cacheGeneration != s_clipCacheGeneration) {
    m_clipCache.clear();
    m_cacheGeneration = s_clipCacheGeneration;
  }
}

const FloatClipRect* GeometryMapperClipCache::getCachedClip(
    const ClipAndTransform& clipAndTransform) {
  invalidateCacheIfNeeded();
  for (const auto& entry : m_clipCache) {
    if (entry.clipAndTransform == clipAndTransform) {
      return &entry.clipRect;
    }
  }
  return nullptr;
}

void GeometryMapperClipCache::setCachedClip(
    const ClipAndTransform& clipAndTransform,
    const FloatClipRect& clip) {
  invalidateCacheIfNeeded();
#if DCHECK_IS_ON()
  for (const auto& entry : m_clipCache) {
    if (entry.clipAndTransform == clipAndTransform)
      DCHECK(false);  // There should be no existing entry.
  }
#endif
  m_clipCache.push_back(ClipCacheEntry(clipAndTransform, clip));
}

}  // namespace blink
