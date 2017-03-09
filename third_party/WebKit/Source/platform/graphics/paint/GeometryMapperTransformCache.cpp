// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapperTransformCache.h"

namespace blink {

// All transform caches invalidate themselves by tracking a local cache
// generation, and invalidating their cache if their cache generation disagrees
// with s_transformCacheGeneration.
static unsigned s_transformCacheGeneration = 0;

GeometryMapperTransformCache::GeometryMapperTransformCache()
    : m_cacheGeneration(s_transformCacheGeneration) {}

void GeometryMapperTransformCache::clearCache() {
  s_transformCacheGeneration++;
}

void GeometryMapperTransformCache::invalidateCacheIfNeeded() {
  if (m_cacheGeneration != s_transformCacheGeneration) {
    m_transformCache.clear();
    m_cacheGeneration = s_transformCacheGeneration;
  }
}

const TransformationMatrix* GeometryMapperTransformCache::getCachedTransform(
    const TransformPaintPropertyNode* ancestorTransform) {
  invalidateCacheIfNeeded();
  for (const auto& entry : m_transformCache) {
    if (entry.ancestorNode == ancestorTransform) {
      return &entry.toAncestor;
    }
  }
  return nullptr;
}

void GeometryMapperTransformCache::setCachedTransform(
    const TransformPaintPropertyNode* ancestorTransform,
    const TransformationMatrix& matrix) {
  invalidateCacheIfNeeded();
#if DCHECK_IS_ON()
  for (const auto& entry : m_transformCache) {
    if (entry.ancestorNode == ancestorTransform)
      DCHECK(false);  // There should be no existing entry.
  }
#endif
  m_transformCache.push_back(TransformCacheEntry(ancestorTransform, matrix));
}

}  // namespace blink
