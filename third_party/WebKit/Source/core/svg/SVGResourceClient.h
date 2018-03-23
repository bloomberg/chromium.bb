// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResourceClient_h
#define SVGResourceClient_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

typedef unsigned InvalidationModeMask;

class CORE_EXPORT SVGResourceClient : public GarbageCollectedMixin {
 public:
  virtual ~SVGResourceClient() = default;

  // When adding modes, make sure we don't overflow
  // |LayoutSVGResourceContainer::completed_invalidation_mask_|.
  enum InvalidationMode {
    kLayoutInvalidation = 1 << 0,
    kBoundariesInvalidation = 1 << 1,
    kPaintInvalidation = 1 << 2,
    kParentOnlyInvalidation = 1 << 3
  };
  virtual void ResourceContentChanged(InvalidationModeMask) = 0;
  virtual void ResourceElementChanged() = 0;

 protected:
  SVGResourceClient() = default;
};

}  // namespace blink

#endif  // SVGResourceClient_h
