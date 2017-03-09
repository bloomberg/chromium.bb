// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapperClipCache_h
#define GeometryMapperClipCache_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/FloatClipRect.h"
#include "wtf/Vector.h"

namespace blink {

class ClipPaintPropertyNode;
class FloatClipRect;
class TransformPaintPropertyNode;

// A GeometryMapperClipCache hangs off a ClipPaintPropertyNode. It stores
// cached "clip visual rects" (See GeometryMapper.h) from that node in
// ancestor spaces.
class PLATFORM_EXPORT GeometryMapperClipCache {
 public:
  GeometryMapperClipCache();

  struct ClipAndTransform {
    const ClipPaintPropertyNode* ancestorClip;
    const TransformPaintPropertyNode* ancestorTransform;
    bool operator==(const ClipAndTransform& other) const {
      return ancestorClip == other.ancestorClip &&
             ancestorTransform == other.ancestorTransform;
    }
    ClipAndTransform(const ClipPaintPropertyNode* ancestorClipArg,
                     const TransformPaintPropertyNode* ancestorTransformArg)
        : ancestorClip(ancestorClipArg),
          ancestorTransform(ancestorTransformArg) {}
  };

  // Returns the clip visual rect  of the owning
  // clip of |this| in the space of |ancestors|, if there is one cached.
  // Otherwise returns null.
  const FloatClipRect* getCachedClip(const ClipAndTransform& ancestors);
  // Stores the "clip visual rect" of |this in the space of |ancestors|,
  // into a local cache.

  void setCachedClip(const ClipAndTransform&, const FloatClipRect&);

  static void clearCache();

 private:
  struct ClipCacheEntry {
    const ClipAndTransform clipAndTransform;
    const FloatClipRect clipRect;
    ClipCacheEntry(const ClipAndTransform& clipAndTransformArg,
                   const FloatClipRect& clipRectArg)
        : clipAndTransform(clipAndTransformArg), clipRect(clipRectArg) {}
  };

  void invalidateCacheIfNeeded();

  Vector<ClipCacheEntry> m_clipCache;
  unsigned m_cacheGeneration;

  DISALLOW_COPY_AND_ASSIGN(GeometryMapperClipCache);
};

}  // namespace blink

#endif  // GeometryMapperClipCache_h
