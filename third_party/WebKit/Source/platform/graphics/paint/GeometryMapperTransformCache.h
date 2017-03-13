// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapperTransformCache_h
#define GeometryMapperTransformCache_h

#include "platform/PlatformExport.h"

#include "platform/transforms/TransformationMatrix.h"
#include "wtf/Vector.h"

namespace blink {

class TransformPaintPropertyNode;

// A GeometryMapperTransformCache hangs off a TransformPaintPropertyNode. It
// stores cached "transformed rects" (See GeometryMapper.h) from that node in
// ancestor spaces.
class PLATFORM_EXPORT GeometryMapperTransformCache {
 public:
  GeometryMapperTransformCache();

  // Returns the transformed rect (see GeometryMapper.h) of |this| in the
  // space of |ancestorTransform|, if there is one cached. Otherwise returns
  // null.
  const TransformationMatrix* getCachedTransform(
      const TransformPaintPropertyNode* ancestorTransform);

  // Stores the "transformed rect" of |this| in the space of |ancestors|,
  // into a local cache.
  void setCachedTransform(const TransformPaintPropertyNode* ancestorTransform,
                          const TransformationMatrix& toAncestor);

  static void clearCache();

 private:
  struct TransformCacheEntry {
    const TransformPaintPropertyNode* ancestorNode;
    TransformationMatrix toAncestor;
    TransformCacheEntry(const TransformPaintPropertyNode* ancestorNodeArg,
                        const TransformationMatrix& toAncestorArg)
        : ancestorNode(ancestorNodeArg), toAncestor(toAncestorArg) {}
  };

  void invalidateCacheIfNeeded();

  Vector<TransformCacheEntry> m_transformCache;
  unsigned m_cacheGeneration;

  DISALLOW_COPY_AND_ASSIGN(GeometryMapperTransformCache);
};

}  // namespace blink

#endif  // GeometryMapperTransformCache_h
