// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_AX_OBJECT_CACHE_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_AX_OBJECT_CACHE_BASE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class LayoutObject;
class Node;
class AXObject;

// AXObjectCacheBase is a temporary class that sits between AXObjectCache and
// AXObject and contains methods required by web/ that we don't want to be
// available in the public API (AXObjectCache).
// TODO(dmazzoni): Once all dependencies in web/ use this class instead of
// AXObjectCacheImpl, refactor usages to use AXObjectCache instead (introducing
// new public API methods or similar) and remove this class.
class CORE_EXPORT AXObjectCacheBase : public AXObjectCache {
 public:
  ~AXObjectCacheBase() override = default;

  virtual AXObject* Get(const Node*) = 0;
  virtual AXObject* GetOrCreate(LayoutObject*) = 0;

 protected:
  AXObjectCacheBase() = default;
  DISALLOW_COPY_AND_ASSIGN(AXObjectCacheBase);
};

template <>
struct DowncastTraits<AXObjectCacheBase> {
  // All concrete implementations of AXObjectCache are derived from
  // AXObjectCacheBase.
  static bool AllowFrom(const AXObjectCache& cache) { return true; }
};

}  // namespace blink

#endif
