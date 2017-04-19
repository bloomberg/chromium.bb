// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapperTransformCache.h"

namespace blink {

// All transform caches invalidate themselves by tracking a local cache
// generation, and invalidating their cache if their cache generation disagrees
// with s_transformCacheGeneration.
static unsigned g_transform_cache_generation = 0;

GeometryMapperTransformCache::GeometryMapperTransformCache()
    : cache_generation_(g_transform_cache_generation) {}

void GeometryMapperTransformCache::ClearCache() {
  g_transform_cache_generation++;
}

void GeometryMapperTransformCache::InvalidateCacheIfNeeded() {
  if (cache_generation_ != g_transform_cache_generation) {
    transform_cache_.clear();
    cache_generation_ = g_transform_cache_generation;
  }
}

const TransformationMatrix* GeometryMapperTransformCache::GetCachedTransform(
    const TransformPaintPropertyNode* ancestor_transform) {
  InvalidateCacheIfNeeded();
  for (const auto& entry : transform_cache_) {
    if (entry.ancestor_node == ancestor_transform) {
      return &entry.to_ancestor;
    }
  }
  return nullptr;
}

void GeometryMapperTransformCache::SetCachedTransform(
    const TransformPaintPropertyNode* ancestor_transform,
    const TransformationMatrix& matrix) {
  InvalidateCacheIfNeeded();
#if DCHECK_IS_ON()
  for (const auto& entry : transform_cache_) {
    if (entry.ancestor_node == ancestor_transform)
      DCHECK(false);  // There should be no existing entry.
  }
#endif
  transform_cache_.push_back(TransformCacheEntry(ancestor_transform, matrix));
}

}  // namespace blink
