/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 Zoltan Herczeg <zherczeg@webkit.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGPaintContext_h
#define SVGPaintContext_h

#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "core/paint/ClipPathClipper.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGFilterPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/transforms/AffineTransform.h"
#include <memory>

namespace blink {

class LayoutObject;
class LayoutSVGResourceFilter;
class LayoutSVGResourceMasker;
class SVGResources;

// This class hooks up the correct paint property transform node when spv2 is
// enabled, and otherwise works like a TransformRecorder which emits Transform
// display items for spv1.
class SVGTransformContext : public TransformRecorder {
  STACK_ALLOCATED();

 public:
  SVGTransformContext(GraphicsContext& context,
                      const LayoutObject& object,
                      const AffineTransform& transform)
      : TransformRecorder(context, object, transform) {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      const auto* fragment_data = object.FirstFragment();
      if (!fragment_data)
        return;
      const auto* object_properties = fragment_data->PaintProperties();
      if (!object_properties)
        return;
      if (object.IsSVGRoot()) {
        // If a transform exists, we can rely on a layer existing to apply it.
        DCHECK(!object_properties || !object_properties->Transform() ||
               object.HasLayer());
        if (object_properties->SvgLocalToBorderBoxTransform()) {
          DCHECK(object_properties->SvgLocalToBorderBoxTransform()->Matrix() ==
                 transform.ToTransformationMatrix());
          auto& paint_controller = context.GetPaintController();
          PaintChunkProperties properties(
              paint_controller.CurrentPaintChunkProperties());
          properties.property_tree_state.SetTransform(
              object_properties->SvgLocalToBorderBoxTransform());
          transform_property_scope_.emplace(paint_controller, object,
                                            properties);
        }
      } else {
        DCHECK(object.IsSVG());
        // Should only be used by LayoutSVGRoot.
        DCHECK(!object_properties->SvgLocalToBorderBoxTransform());

        if (object_properties->Transform()) {
          DCHECK(object_properties->Transform()->Matrix() ==
                 transform.ToTransformationMatrix());
          auto& paint_controller = context.GetPaintController();
          PaintChunkProperties properties(
              paint_controller.CurrentPaintChunkProperties());
          properties.property_tree_state.SetTransform(
              object_properties->Transform());
          transform_property_scope_.emplace(paint_controller, object,
                                            properties);
        }
      }
    }
  }

 private:
  Optional<ScopedPaintChunkProperties> transform_property_scope_;
};

class SVGPaintContext {
  STACK_ALLOCATED();

 public:
  SVGPaintContext(const LayoutObject& object, const PaintInfo& paint_info)
      : object_(object),
        paint_info_(paint_info),
        filter_(nullptr),
        masker_(nullptr) {}

  ~SVGPaintContext();

  PaintInfo& GetPaintInfo() {
    return filter_paint_info_ ? *filter_paint_info_ : paint_info_;
  }

  // Return true if these operations aren't necessary or if they are
  // successfully applied.
  bool ApplyClipMaskAndFilterIfNecessary();

  static void PaintResourceSubtree(GraphicsContext&, const LayoutObject*);

  // TODO(fs): This functions feels a bit misplaced (we don't want this to
  // turn into the new kitchen sink). Move it if a better location surfaces.
  static bool PaintForLayoutObject(
      const PaintInfo&,
      const ComputedStyle&,
      const LayoutObject&,
      LayoutSVGResourceMode,
      PaintFlags&,
      const AffineTransform* additional_paint_server_transform = nullptr);

 private:
  void ApplyCompositingIfNecessary();
  void ApplyPaintPropertyState();
  void ApplyClipIfNecessary();

  // Return true if no masking is necessary or if the mask is successfully
  // applied.
  bool ApplyMaskIfNecessary(SVGResources*);

  // Return true if no filtering is necessary or if the filter is successfully
  // applied.
  bool ApplyFilterIfNecessary(SVGResources*);

  bool IsIsolationInstalled() const;

  const LayoutObject& object_;
  PaintInfo paint_info_;
  std::unique_ptr<PaintInfo> filter_paint_info_;
  LayoutSVGResourceFilter* filter_;
  LayoutSVGResourceMasker* masker_;
  std::unique_ptr<CompositingRecorder> compositing_recorder_;
  Optional<ClipPathClipper> clip_path_clipper_;
  std::unique_ptr<SVGFilterRecordingContext> filter_recording_context_;
  Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties_;
#if DCHECK_IS_ON()
  bool apply_clip_mask_and_filter_if_necessary_called_ = false;
#endif
};

}  // namespace blink

#endif  // SVGPaintContext_h
