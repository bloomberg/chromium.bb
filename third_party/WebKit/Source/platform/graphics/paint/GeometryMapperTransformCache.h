// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapperTransformCache_h
#define GeometryMapperTransformCache_h

#include "platform/PlatformExport.h"

#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Vector.h"

namespace blink {

class TransformPaintPropertyNode;

// A GeometryMapperTransformCache hangs off a TransformPaintPropertyNode. It
// stores cached "transformed rects" (See GeometryMapper.h) from that node in
// ancestor spaces.
class PLATFORM_EXPORT GeometryMapperTransformCache {
  USING_FAST_MALLOC(GeometryMapperTransformCache);

 public:
  GeometryMapperTransformCache();

  // Returns the transformed rect (see GeometryMapper.h) of |this| in the
  // space of |ancestorTransform|, if there is one cached. Otherwise returns
  // null.
  //
  // These transforms are not flattened to 2d.
  const TransformationMatrix* GetCachedTransform(
      const TransformPaintPropertyNode* ancestor_transform);

  // Stores the "transformed rect" of |this| in the space of |ancestors|,
  // into a local cache.
  void SetCachedTransform(const TransformPaintPropertyNode* ancestor_transform,
                          const TransformationMatrix& to_ancestor);

  static void ClearCache();

 private:
  struct TransformCacheEntry {
    const TransformPaintPropertyNode* ancestor_node;
    TransformationMatrix to_ancestor;
    TransformCacheEntry(const TransformPaintPropertyNode* ancestor_node_arg,
                        const TransformationMatrix& to_ancestor_arg)
        : ancestor_node(ancestor_node_arg), to_ancestor(to_ancestor_arg) {}
  };

  void InvalidateCacheIfNeeded();

  Vector<TransformCacheEntry> transform_cache_;
  unsigned cache_generation_;

  DISALLOW_COPY_AND_ASSIGN(GeometryMapperTransformCache);
};

}  // namespace blink

#endif  // GeometryMapperTransformCache_h
