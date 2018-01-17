// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPaintPropertyNode_h
#define ClipPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/paint/GeometryMapperClipCache.h"
#include "platform/graphics/paint/PaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
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
    : public PaintPropertyNode<ClipPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real clip
  // space.
  static ClipPaintPropertyNode* Root();

  static scoped_refptr<ClipPaintPropertyNode> Create(
      scoped_refptr<const ClipPaintPropertyNode> parent,
      scoped_refptr<const TransformPaintPropertyNode> local_transform_space,
      const FloatRoundedRect& clip_rect,
      const FloatRoundedRect* clip_rect_excluding_overlay_scrollbars = nullptr,
      CompositingReasons direct_compositing_reasons =
          CompositingReason::kNone) {
    return base::AdoptRef(new ClipPaintPropertyNode(
        std::move(parent), std::move(local_transform_space), clip_rect,
        clip_rect_excluding_overlay_scrollbars
            ? *clip_rect_excluding_overlay_scrollbars
            : clip_rect,
        direct_compositing_reasons));
  }

  bool Update(
      scoped_refptr<const ClipPaintPropertyNode> parent,
      scoped_refptr<const TransformPaintPropertyNode> local_transform_space,
      const FloatRoundedRect& clip_rect,
      const FloatRoundedRect* clip_rect_excluding_overlay_scrollbars =
          nullptr) {
    bool parent_changed = PaintPropertyNode::Update(std::move(parent));

    if (local_transform_space == local_transform_space_ &&
        clip_rect == clip_rect_ &&
        (!clip_rect_excluding_overlay_scrollbars ||
         *clip_rect_excluding_overlay_scrollbars ==
             clip_rect_excluding_overlay_scrollbars_))
      return parent_changed;

    SetChanged();
    local_transform_space_ = std::move(local_transform_space);
    clip_rect_ = clip_rect;
    clip_rect_excluding_overlay_scrollbars_ =
        clip_rect_excluding_overlay_scrollbars
            ? *clip_rect_excluding_overlay_scrollbars
            : clip_rect;
    return true;
  }

  const TransformPaintPropertyNode* LocalTransformSpace() const {
    return local_transform_space_.get();
  }
  const FloatRoundedRect& ClipRect() const { return clip_rect_; }
  const FloatRoundedRect& ClipRectExcludingOverlayScrollbars() const {
    return clip_rect_excluding_overlay_scrollbars_;
  }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a clip node before it has been updated, to later detect changes.
  scoped_refptr<ClipPaintPropertyNode> Clone() const {
    return base::AdoptRef(
        new ClipPaintPropertyNode(Parent(), local_transform_space_, clip_rect_,
                                  clip_rect_, direct_compositing_reasons_));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a clip node has changed.
  bool operator==(const ClipPaintPropertyNode& o) const {
    return Parent() == o.Parent() &&
           local_transform_space_ == o.local_transform_space_ &&
           clip_rect_ == o.clip_rect_ &&
           direct_compositing_reasons_ == o.direct_compositing_reasons_;
  }

  String ToTreeString() const;
#endif

  std::unique_ptr<JSONObject> ToJSON() const;

  bool HasDirectCompositingReasons() const {
    return direct_compositing_reasons_ != CompositingReason::kNone;
  }

 private:
  ClipPaintPropertyNode(
      scoped_refptr<const ClipPaintPropertyNode> parent,
      scoped_refptr<const TransformPaintPropertyNode> local_transform_space,
      const FloatRoundedRect& clip_rect,
      const FloatRoundedRect& clip_rect_excluding_overlay_scrollbars,
      CompositingReasons direct_compositing_reasons)
      : PaintPropertyNode(std::move(parent)),
        local_transform_space_(std::move(local_transform_space)),
        clip_rect_(clip_rect),
        clip_rect_excluding_overlay_scrollbars_(
            clip_rect_excluding_overlay_scrollbars),
        direct_compositing_reasons_(direct_compositing_reasons) {}

  // For access to GetClipCache();
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

  scoped_refptr<const TransformPaintPropertyNode> local_transform_space_;
  FloatRoundedRect clip_rect_;
  FloatRoundedRect clip_rect_excluding_overlay_scrollbars_;
  CompositingReasons direct_compositing_reasons_;

  std::unique_ptr<GeometryMapperClipCache> geometry_mapper_clip_cache_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ClipPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ClipPaintPropertyNode_h
