// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPaintPropertyNode_h
#define ClipPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/paint/GeometryMapperClipCache.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

class GeometryMapperClipCache;

// A clip rect created by a css property such as "overflow" or "clip".
// Along with a reference to the transform space the clip rect is based on,
// and a parent ClipPaintPropertyNode for inherited clips.
//
// The clip tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT ClipPaintPropertyNode
    : public RefCounted<ClipPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real clip
  // space.
  static ClipPaintPropertyNode* Root();

  static PassRefPtr<ClipPaintPropertyNode> Create(
      PassRefPtr<const ClipPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> local_transform_space,
      const FloatRoundedRect& clip_rect,
      CompositingReasons direct_compositing_reasons = kCompositingReasonNone) {
    return AdoptRef(new ClipPaintPropertyNode(
        std::move(parent), std::move(local_transform_space), clip_rect,
        direct_compositing_reasons));
  }

  void Update(
      PassRefPtr<const ClipPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> local_transform_space,
      const FloatRoundedRect& clip_rect) {
    DCHECK(!IsRoot());
    DCHECK(parent != this);
    parent_ = std::move(parent);
    local_transform_space_ = std::move(local_transform_space);
    clip_rect_ = clip_rect;
  }

  const TransformPaintPropertyNode* LocalTransformSpace() const {
    return local_transform_space_.Get();
  }
  const FloatRoundedRect& ClipRect() const { return clip_rect_; }

  // Reference to inherited clips, or nullptr if this is the only clip.
  const ClipPaintPropertyNode* Parent() const { return parent_.Get(); }
  bool IsRoot() const { return !parent_; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a clip node before it has been updated, to later detect changes.
  PassRefPtr<ClipPaintPropertyNode> Clone() const {
    return AdoptRef(new ClipPaintPropertyNode(parent_, local_transform_space_,
                                              clip_rect_,
                                              direct_compositing_reasons_));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a clip node has changed.
  bool operator==(const ClipPaintPropertyNode& o) const {
    return parent_ == o.parent_ &&
           local_transform_space_ == o.local_transform_space_ &&
           clip_rect_ == o.clip_rect_ &&
           direct_compositing_reasons_ == o.direct_compositing_reasons_;
  }

  String ToTreeString() const;
#endif

  String ToString() const;

  bool HasDirectCompositingReasons() const {
    return direct_compositing_reasons_ != kCompositingReasonNone;
  }

 private:
  ClipPaintPropertyNode(
      PassRefPtr<const ClipPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> local_transform_space,
      const FloatRoundedRect& clip_rect,
      CompositingReasons direct_compositing_reasons)
      : parent_(std::move(parent)),
        local_transform_space_(std::move(local_transform_space)),
        clip_rect_(clip_rect),
        direct_compositing_reasons_(direct_compositing_reasons) {}

  // For access to getClipCache();
  friend class GeometryMapper;
  friend class GeometryMapperTest;

  GeometryMapperClipCache& GetClipCache() const {
    return const_cast<ClipPaintPropertyNode*>(this)->GetClipCache();
  }

  GeometryMapperClipCache& GetClipCache() {
    if (!geometry_mapper_clip_cache_)
      geometry_mapper_clip_cache_.reset(new GeometryMapperClipCache());
    return *geometry_mapper_clip_cache_.get();
  }

  RefPtr<const ClipPaintPropertyNode> parent_;
  RefPtr<const TransformPaintPropertyNode> local_transform_space_;
  FloatRoundedRect clip_rect_;
  CompositingReasons direct_compositing_reasons_;

  std::unique_ptr<GeometryMapperClipCache> geometry_mapper_clip_cache_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ClipPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ClipPaintPropertyNode_h
