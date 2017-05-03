// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FindPaintOffsetAndVisualRectNeedingUpdate_h
#define FindPaintOffsetAndVisualRectNeedingUpdate_h

#if DCHECK_IS_ON()

#include "core/layout/LayoutObject.h"
#include "core/paint/FindPropertiesNeedingUpdate.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

// This file contains scope classes for catching cases where paint offset or
// visual rect needed an update but were not marked as such. If paint offset or
// any visual rect (including visual rect of the object itself, scroll controls,
// caret, selection, etc.) will change, the object must be marked as such by
// LayoutObject::setNeedsPaintOffsetAndVisualRectUpdate() (which is a private
// function called by several public paint-invalidation-flag setting functions).

class FindPaintOffsetNeedingUpdateScope {
 public:
  FindPaintOffsetNeedingUpdateScope(const LayoutObject& object,
                                    bool& is_actually_needed)
      : object_(object),
        is_actually_needed_(is_actually_needed),
        old_paint_offset_(object.PaintOffset()) {
    if (object.PaintProperties() &&
        object.PaintProperties()->PaintOffsetTranslation()) {
      old_paint_offset_translation_ =
          object.PaintProperties()->PaintOffsetTranslation()->Clone();
    }
  }

  ~FindPaintOffsetNeedingUpdateScope() {
    if (is_actually_needed_)
      return;
    DCHECK_OBJECT_PROPERTY_EQ(object_, &old_paint_offset_,
                              &object_.PaintOffset());
    const auto* paint_offset_translation =
        object_.PaintProperties()
            ? object_.PaintProperties()->PaintOffsetTranslation()
            : nullptr;
    DCHECK_OBJECT_PROPERTY_EQ(object_, old_paint_offset_translation_.Get(),
                              paint_offset_translation);
  }

 private:
  const LayoutObject& object_;
  const bool& is_actually_needed_;
  LayoutPoint old_paint_offset_;
  RefPtr<const TransformPaintPropertyNode> old_paint_offset_translation_;
};

class FindVisualRectNeedingUpdateScopeBase {
 protected:
  FindVisualRectNeedingUpdateScopeBase(const LayoutObject& object,
                                       const PaintInvalidatorContext& context,
                                       const LayoutRect& old_visual_rect,
                                       bool is_actually_needed)
      : object_(object),
        context_(context),
        old_visual_rect_(old_visual_rect),
        needed_visual_rect_update_(context.NeedsVisualRectUpdate(object)) {
    if (needed_visual_rect_update_) {
      DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() ||
             is_actually_needed);
      return;
    }
    context.force_visual_rect_update_for_checking_ = true;
    DCHECK(context.NeedsVisualRectUpdate(object));
  }

  ~FindVisualRectNeedingUpdateScopeBase() {
    context_.force_visual_rect_update_for_checking_ = false;
    DCHECK_EQ(needed_visual_rect_update_,
              context_.NeedsVisualRectUpdate(object_));
  }

  static LayoutRect InflatedRect(const LayoutRect& r) {
    LayoutRect result = r;
    result.Inflate(1);
    return result;
  }

  void CheckVisualRect(const LayoutRect& new_visual_rect) {
    if (needed_visual_rect_update_)
      return;
    DCHECK((old_visual_rect_.IsEmpty() && new_visual_rect.IsEmpty()) ||
           object_.EnclosingLayer()->SubtreeIsInvisible() ||
           old_visual_rect_ == new_visual_rect ||
           // The following check is to tolerate the differences caused by
           // pixel snapping that may happen for one rect but not for another
           // while we need neither paint invalidation nor raster invalidation
           // for the change. This may miss some real subpixel changes of visual
           // rects. TODO(wangxianzhu): Look into whether we can tighten this
           // for SPv2.
           (InflatedRect(old_visual_rect_).Contains(new_visual_rect) &&
            InflatedRect(new_visual_rect).Contains(old_visual_rect_)))
        << "Visual rect changed without needing update"
        << " object=" << object_.DebugName()
        << " old=" << old_visual_rect_.ToString()
        << " new=" << new_visual_rect.ToString();
  }

  const LayoutObject& object_;
  const PaintInvalidatorContext& context_;
  LayoutRect old_visual_rect_;
  bool needed_visual_rect_update_;
};

// For updates of visual rects (e.g. of scroll controls, caret, selection,etc.)
// contained by an object.
class FindVisualRectNeedingUpdateScope : FindVisualRectNeedingUpdateScopeBase {
 public:
  FindVisualRectNeedingUpdateScope(const LayoutObject& object,
                                   const PaintInvalidatorContext& context,
                                   const LayoutRect& old_visual_rect,
                                   // Must be a reference to a rect that
                                   // outlives this scope.
                                   const LayoutRect& new_visual_rect)
      : FindVisualRectNeedingUpdateScopeBase(
            object,
            context,
            old_visual_rect,
            context.tree_builder_context_actually_needed_),
        new_visual_rect_ref_(new_visual_rect) {}

  ~FindVisualRectNeedingUpdateScope() { CheckVisualRect(new_visual_rect_ref_); }

 private:
  const LayoutRect& new_visual_rect_ref_;
};

// For updates of object visual rect and location.
class FindObjectVisualRectNeedingUpdateScope
    : FindVisualRectNeedingUpdateScopeBase {
 public:
  FindObjectVisualRectNeedingUpdateScope(const LayoutObject& object,
                                         const PaintInvalidatorContext& context,
                                         bool is_actually_needed)
      : FindVisualRectNeedingUpdateScopeBase(object,
                                             context,
                                             object.VisualRect(),
                                             is_actually_needed),
        old_location_(ObjectPaintInvalidator(object).LocationInBacking()) {}

  ~FindObjectVisualRectNeedingUpdateScope() {
    CheckVisualRect(object_.VisualRect());
    CheckLocation();
  }

  void CheckLocation() {
    if (needed_visual_rect_update_)
      return;
    LayoutPoint new_location =
        ObjectPaintInvalidator(object_).LocationInBacking();
    // Location of LayoutText and non-root SVG is location of the visual rect
    // which have been checked above.
    DCHECK(object_.IsText() || object_.IsSVGChild() ||
           new_location == old_location_ ||
           object_.EnclosingLayer()->SubtreeIsInvisible() ||
           // See checkVisualRect for the issue of approximation.
           LayoutRect(-1, -1, 2, 2)
               .Contains(LayoutRect(LayoutPoint(new_location - old_location_),
                                    LayoutSize())))
        << "Location changed without needing update"
        << " object=" << object_.DebugName()
        << " old=" << old_location_.ToString()
        << " new=" << new_location.ToString();
  }

 private:
  LayoutPoint old_location_;
};

}  // namespace blink

#endif  // DCHECK_IS_ON()

#endif  // FindPaintOffsetAndVisualRectNeedingUpdate_h
