// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RarePaintData_h
#define RarePaintData_h

#include "core/CoreExport.h"
#include "core/paint/ObjectPaintProperties.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PaintLayer;
class PropertyTreeState;

// This is for paint-related data on LayoutObject that is not needed on all
// objects.
class CORE_EXPORT RarePaintData {
  WTF_MAKE_NONCOPYABLE(RarePaintData);
  USING_FAST_MALLOC(RarePaintData);

 public:
  RarePaintData(const LayoutPoint& location_in_backing);
  ~RarePaintData();

  PaintLayer* Layer() { return layer_.get(); }
  void SetLayer(std::unique_ptr<PaintLayer>);

  // An id for this object that is unique for the lifetime of the WebView.
  UniqueObjectId UniqueId() const { return unique_id_; }

  // See PaintInvalidatorContext::old_location for details. This will be removed
  // for SPv2.
  LayoutPoint LocationInBacking() const { return location_in_backing_; }
  void SetLocationInBacking(const LayoutPoint& p) { location_in_backing_ = p; }

  // Visual rect of the selection on this object, in the same coordinate space
  // as DisplayItemClient::VisualRect().
  LayoutRect SelectionVisualRect() const { return selection_visual_rect_; }
  void SetSelectionVisualRect(const LayoutRect& r) {
    selection_visual_rect_ = r;
  }

  LayoutRect PartialInvalidationRect() const {
    return partial_invalidation_rect_;
  }
  void SetPartialInvalidationRect(const LayoutRect& r) {
    partial_invalidation_rect_ = r;
  }

  ObjectPaintProperties* PaintProperties() const {
    return paint_properties_.get();
  }
  ObjectPaintProperties& EnsurePaintProperties();
  void ClearPaintProperties();

  // The complete set of property nodes that should be used as a
  // starting point to paint this fragment. See also the comment for
  // |local_border_box_properties_|.
  // LocalBorderBoxProperties() includes fragment clip.
  PropertyTreeState* LocalBorderBoxProperties() const {
    return local_border_box_properties_.get();
  }

  void ClearLocalBorderBoxProperties();
  void SetLocalBorderBoxProperties(PropertyTreeState&);

  // This is the complete set of property nodes that is inherited
  // from the ancestor before applying any local CSS properties,
  // but includes paint offset transform.
  PropertyTreeState PreEffectProperties() const;

  // This is the complete set of property nodes that can be used to
  // paint the contents of this fragment. It is similar to
  // |local_border_box_properties_| but includes properties (e.g.,
  // overflow clip, scroll translation) that apply to contents.
  PropertyTreeState ContentsProperties() const;

  // The pagination offset is the additional factor to add in to map
  // from flow thread coordinates relative to the enclosing pagination
  // layer, to visual coordiantes relative to that pagination layer.
  LayoutPoint PaginationOffset() const { return pagination_offset_; }
  void SetPaginationOffset(const LayoutPoint& pagination_offset) {
    pagination_offset_ = pagination_offset;
  }

 private:
  const TransformPaintPropertyNode* GetPreTransform() const;
  const TransformPaintPropertyNode* GetPostScrollTranslation() const;
  const ClipPaintPropertyNode* GetPreCssClip() const;
  const ClipPaintPropertyNode* GetPostOverflowClip() const;
  const EffectPaintPropertyNode* GetPreEffect() const;

  // The PaintLayer associated with this LayoutBoxModelObject. This can be null
  // depending on the return value of LayoutBoxModelObject::layerTypeRequired().
  std::unique_ptr<PaintLayer> layer_;

  UniqueObjectId unique_id_;

  LayoutPoint location_in_backing_;
  LayoutRect selection_visual_rect_;
  LayoutRect partial_invalidation_rect_;
  LayoutPoint pagination_offset_;

  // Holds references to the paint property nodes created by this object.
  std::unique_ptr<ObjectPaintProperties> paint_properties_;

  // This is a complete set of property nodes that should be used as a
  // starting point to paint a LayoutObject. This data is cached because some
  // properties inherit from the containing block chain instead of the
  // painting parent and cannot be derived in O(1) during the paint walk.
  //
  // For example: <div style='opacity: 0.3;'/>
  //   The div's local border box properties would have an opacity 0.3 effect
  //   node. Even though the div has no transform, its local border box
  //   properties would have a transform node that points to the div's
  //   ancestor transform space.
  std::unique_ptr<PropertyTreeState> local_border_box_properties_;
};

}  // namespace blink

#endif
