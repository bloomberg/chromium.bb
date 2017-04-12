// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayerClipRecorder_h
#define LayerClipRecorder_h

#include "core/CoreExport.h"
#include "core/paint/PaintLayerPaintingInfo.h"
#include "core/paint/PaintPhase.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ClipRect;
class GraphicsContext;
class LayoutBoxModelObject;

class CORE_EXPORT LayerClipRecorder {
  USING_FAST_MALLOC(LayerClipRecorder);

 public:
  enum BorderRadiusClippingRule {
    kIncludeSelfForBorderRadius,
    kDoNotIncludeSelfForBorderRadius
  };

  // Set rounded clip rectangles defined by border radii all the way from the
  // PaintLayerPaintingInfo "root" layer down to the specified layer (or the
  // parent of said layer, in case BorderRadiusClippingRule says to skip self).
  // fragmentOffset is used for multicol, to specify the translation required to
  // get from flow thread coordinates to visual coordinates for a certain
  // column.
  // FIXME: The BorderRadiusClippingRule parameter is really useless now. If we
  // want to skip self,
  // why not just supply the parent layer as the first parameter instead?
  // FIXME: The ClipRect passed is in visual coordinates (not flow thread
  // coordinates), but at the same time we pass a fragmentOffset, so that we can
  // translate from flow thread coordinates to visual coordinates. This may look
  // rather confusing/redundant, but it is needed for rounded border clipping.
  // Would be nice to clean up this.
  explicit LayerClipRecorder(
      GraphicsContext&,
      const LayoutBoxModelObject&,
      DisplayItem::Type,
      const ClipRect&,
      const PaintLayer* clip_root,
      const LayoutPoint& fragment_offset,
      PaintLayerFlags,
      BorderRadiusClippingRule = kIncludeSelfForBorderRadius);

  ~LayerClipRecorder();

 private:
  void CollectRoundedRectClips(PaintLayer&,
                               const PaintLayer* clip_root,
                               GraphicsContext&,
                               const LayoutPoint& fragment_offset,
                               PaintLayerFlags,
                               BorderRadiusClippingRule,
                               Vector<FloatRoundedRect>& rounded_rect_clips);

  GraphicsContext& graphics_context_;
  const LayoutBoxModelObject& layout_object_;
  DisplayItem::Type clip_type_;
};

}  // namespace blink

#endif  // LayerClipRecorder_h
