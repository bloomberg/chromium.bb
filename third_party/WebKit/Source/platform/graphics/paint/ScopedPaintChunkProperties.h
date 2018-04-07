// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_PROPERTIES_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/optional.h"

namespace blink {

class ScopedPaintChunkProperties {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(ScopedPaintChunkProperties);

 public:
  // Use new PaintChunkProperties for the scope.
  ScopedPaintChunkProperties(PaintController& paint_controller,
                             const PaintChunkProperties& properties,
                             const DisplayItemClient& client,
                             DisplayItem::Type type)
      : paint_controller_(paint_controller),
        previous_properties_(paint_controller.CurrentPaintChunkProperties()) {
    paint_controller_.UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(client, type), properties);
  }

  // Use new PropertyTreeState, and keep the current backface_hidden.
  ScopedPaintChunkProperties(PaintController& paint_controller,
                             const PropertyTreeState& state,
                             const DisplayItemClient& client,
                             DisplayItem::Type type)
      : ScopedPaintChunkProperties(
            paint_controller,
            GetPaintChunkProperties(state, paint_controller),
            client,
            type) {}

  // Use new transform state, and keep the current other properties.
  ScopedPaintChunkProperties(
      PaintController& paint_controller,
      scoped_refptr<const TransformPaintPropertyNode> transform,
      const DisplayItemClient& client,
      DisplayItem::Type type)
      : ScopedPaintChunkProperties(
            paint_controller,
            GetPaintChunkProperties(transform, paint_controller),
            client,
            type) {}

  // Use new clip state, and keep the current other properties.
  ScopedPaintChunkProperties(PaintController& paint_controller,
                             scoped_refptr<const ClipPaintPropertyNode> clip,
                             const DisplayItemClient& client,
                             DisplayItem::Type type)
      : ScopedPaintChunkProperties(
            paint_controller,
            GetPaintChunkProperties(clip, paint_controller),
            client,
            type) {}

  // Use new effect state, and keep the current other properties.
  ScopedPaintChunkProperties(
      PaintController& paint_controller,
      scoped_refptr<const EffectPaintPropertyNode> effect,
      const DisplayItemClient& client,
      DisplayItem::Type type)
      : ScopedPaintChunkProperties(
            paint_controller,
            GetPaintChunkProperties(effect, paint_controller),
            client,
            type) {}

  ~ScopedPaintChunkProperties() {
    // We should not return to the previous id, because that may cause a new
    // chunk to use the same id as that of the previous chunk before this
    // ScopedPaintChunkProperties. The painter should create another scope of
    // paint properties with new id, or the new chunk will use the id of the
    // first display item as its id.
    paint_controller_.UpdateCurrentPaintChunkProperties(WTF::nullopt,
                                                        previous_properties_);
  }

 private:
  static PaintChunkProperties GetPaintChunkProperties(
      const PropertyTreeState& state,
      PaintController& paint_controller) {
    PaintChunkProperties properties(state);
    properties.backface_hidden =
        paint_controller.CurrentPaintChunkProperties().backface_hidden;
    return properties;
  }

  static PaintChunkProperties GetPaintChunkProperties(
      scoped_refptr<const TransformPaintPropertyNode> transform,
      PaintController& paint_controller) {
    PaintChunkProperties properties(
        paint_controller.CurrentPaintChunkProperties());
    properties.property_tree_state.SetTransform(std::move(transform));
    return properties;
  }

  static PaintChunkProperties GetPaintChunkProperties(
      scoped_refptr<const ClipPaintPropertyNode> clip,
      PaintController& paint_controller) {
    PaintChunkProperties properties(
        paint_controller.CurrentPaintChunkProperties());
    properties.property_tree_state.SetClip(std::move(clip));
    return properties;
  }

  static PaintChunkProperties GetPaintChunkProperties(
      scoped_refptr<const EffectPaintPropertyNode> effect,
      PaintController& paint_controller) {
    PaintChunkProperties properties(
        paint_controller.CurrentPaintChunkProperties());
    properties.property_tree_state.SetEffect(std::move(effect));
    return properties;
  }

  PaintController& paint_controller_;
  PaintChunkProperties previous_properties_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_PAINT_CHUNK_PROPERTIES_H_
