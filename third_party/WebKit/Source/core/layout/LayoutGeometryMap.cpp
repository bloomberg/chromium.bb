/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutGeometryMap.h"

#include "core/frame/LocalFrame.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/TransformState.h"
#include "platform/wtf/AutoReset.h"

#define LAYOUT_GEOMETRY_MAP_LOGGING 0

namespace blink {

LayoutGeometryMap::LayoutGeometryMap(MapCoordinatesFlags flags)
    : insertion_position_(kNotFound),
      non_uniform_steps_count_(0),
      transformed_steps_count_(0),
      fixed_steps_count_(0),
      map_coordinates_flags_(flags) {}

LayoutGeometryMap::~LayoutGeometryMap() = default;

void LayoutGeometryMap::MapToAncestor(
    TransformState& transform_state,
    const LayoutBoxModelObject* ancestor) const {
  // If the mapping includes something like columns, we have to go via
  // layoutObjects.
  if (HasNonUniformStep()) {
    mapping_.back().layout_object_->MapLocalToAncestor(
        ancestor, transform_state,
        kApplyContainerFlip | map_coordinates_flags_);
    transform_state.Flatten();
    return;
  }

  bool in_fixed = false;
#if DCHECK_IS_ON()
  bool found_ancestor =
      !ancestor || (mapping_.size() && mapping_[0].layout_object_ == ancestor);
#endif

  int i = mapping_.size() - 1;
  for (; i >= 0; --i) {
    const LayoutGeometryMapStep& current_step = mapping_[i];

    // If container is the root LayoutView (step 0) we want to apply its fixed
    // position offset.
    if (i > 0 && current_step.layout_object_ == ancestor) {
#if DCHECK_IS_ON()
      found_ancestor = true;
#endif
      break;
    }

    // If this box has a transform, it acts as a fixed position container
    // for fixed descendants, which prevents the propagation of 'fixed'
    // unless the layer itself is also fixed position.
    if (i && current_step.flags_ & kContainsFixedPosition &&
        !(current_step.flags_ & kIsFixedPosition))
      in_fixed = false;
    else if (current_step.flags_ & kIsFixedPosition)
      in_fixed = true;

#if DCHECK_IS_ON()
    DCHECK_EQ(!i, IsTopmostLayoutView(current_step.layout_object_));
#endif

    if (!i) {
      // A null container indicates mapping through the root LayoutView, so
      // including its transform (the page scale).
      if (!ancestor && current_step.transform_)
        transform_state.ApplyTransform(*current_step.transform_.get());
    } else {
      TransformState::TransformAccumulation accumulate =
          current_step.flags_ & kAccumulatingTransform
              ? TransformState::kAccumulateTransform
              : TransformState::kFlattenTransform;
      if (current_step.transform_)
        transform_state.ApplyTransform(*current_step.transform_.get(),
                                       accumulate);
      else
        transform_state.Move(current_step.offset_.Width(),
                             current_step.offset_.Height(), accumulate);
    }

    if (in_fixed && current_step.layout_object_->IsLayoutView()) {
      transform_state.Move(current_step.offset_for_fixed_position_);
      in_fixed = false;
    }
  }

  if (in_fixed) {
    // In case we've not reached top ('ancestor' isn't top level view) either
    // assure that 'ancestor' and object both fixed or apply fixed offset of
    // the nearest containing view.
    for (; i >= 0; --i) {
      const LayoutGeometryMapStep& current_step = mapping_[i];
      if (current_step.flags_ & (kContainsFixedPosition | kIsFixedPosition))
        break;
      if (current_step.layout_object_->IsLayoutView()) {
        transform_state.Move(current_step.offset_for_fixed_position_);
        break;
      }
    }
  }

#if DCHECK_IS_ON()
  DCHECK(found_ancestor);
#endif
  transform_state.Flatten();
}

#ifndef NDEBUG
// Handy function to call from gdb while debugging mismatched point/rect errors.
void LayoutGeometryMap::DumpSteps() const {
  fprintf(stderr, "LayoutGeometryMap::dumpSteps accumulatedOffset=%d,%d\n",
          accumulated_offset_.Width().ToInt(),
          accumulated_offset_.Height().ToInt());
  for (int i = mapping_.size() - 1; i >= 0; --i) {
    fprintf(stderr, " [%d] %s: offset=%d,%d", i,
            mapping_[i].layout_object_->DebugName().Ascii().data(),
            mapping_[i].offset_.Width().ToInt(),
            mapping_[i].offset_.Height().ToInt());
    if (mapping_[i].flags_ & kContainsFixedPosition)
      fprintf(stderr, " containsFixedPosition");
    fprintf(stderr, "\n");
  }
}
#endif

FloatQuad LayoutGeometryMap::MapToAncestor(
    const FloatRect& rect,
    const LayoutBoxModelObject* ancestor) const {
  FloatQuad result;

  if (!HasFixedPositionStep() && !HasTransformStep() && !HasNonUniformStep() &&
      (!ancestor ||
       (mapping_.size() && ancestor == mapping_[0].layout_object_))) {
    result = rect;
    result.Move(accumulated_offset_);
  } else {
    TransformState transform_state(TransformState::kApplyTransformDirection,
                                   rect.Center(), rect);
    MapToAncestor(transform_state, ancestor);
    result = transform_state.LastPlanarQuad();
  }

#if DCHECK_IS_ON()
  if (mapping_.size() > 0) {
    const LayoutObject* last_layout_object = mapping_.back().layout_object_;

    FloatRect layout_object_mapped_result =
        last_layout_object
            ->LocalToAncestorQuad(rect, ancestor, map_coordinates_flags_)
            .BoundingBox();

    // Inspector creates layoutObjects with negative width
    // <https://bugs.webkit.org/show_bug.cgi?id=87194>.
    // Taking FloatQuad bounds avoids spurious assertions because of that.
    DCHECK(EnclosingIntRect(layout_object_mapped_result) ==
               EnclosingIntRect(result.BoundingBox()) ||
           layout_object_mapped_result.MayNotHaveExactIntRectRepresentation() ||
           result.BoundingBox().MayNotHaveExactIntRectRepresentation());
  }
#endif

  return result;
}

void LayoutGeometryMap::PushMappingsToAncestor(
    const LayoutObject* layout_object,
    const LayoutBoxModelObject* ancestor_layout_object) {
  // We need to push mappings in reverse order here, so do insertions rather
  // than appends.
  AutoReset<size_t> position_change(&insertion_position_, mapping_.size());
  do {
    layout_object =
        layout_object->PushMappingToContainer(ancestor_layout_object, *this);
  } while (layout_object && layout_object != ancestor_layout_object);

#if DCHECK_IS_ON()
  DCHECK(mapping_.IsEmpty() || IsTopmostLayoutView(mapping_[0].layout_object_));
#endif
}

static bool CanMapBetweenLayoutObjects(const LayoutObject& layout_object,
                                       const LayoutObject& ancestor) {
  for (const LayoutObject* current = &layout_object;;
       current = current->Parent()) {
    const ComputedStyle& style = current->StyleRef();
    if (style.GetPosition() == EPosition::kFixed ||
        style.IsFlippedBlocksWritingMode() ||
        style.HasTransformRelatedProperty())
      return false;

    if (current->CanContainFixedPositionObjects() ||
        current->IsLayoutFlowThread() || current->IsSVGRoot())
      return false;

    if (current == &ancestor)
      break;

    if (current->IsFloatingWithNonContainingBlockParent())
      return false;
  }

  return true;
}

void LayoutGeometryMap::PushMappingsToAncestor(
    const PaintLayer* layer,
    const PaintLayer* ancestor_layer) {
  const LayoutObject& layout_object = layer->GetLayoutObject();

  bool cross_document =
      ancestor_layer &&
      layout_object.GetFrame() != ancestor_layer->GetLayoutObject().GetFrame();
  DCHECK(!cross_document ||
         map_coordinates_flags_ & kTraverseDocumentBoundaries);

  // We have to visit all the layoutObjects to detect flipped blocks. This might
  // defeat the gains from mapping via layers.
  bool can_convert_in_layer_tree =
      (ancestor_layer && !cross_document)
          ? CanMapBetweenLayoutObjects(layout_object,
                                       ancestor_layer->GetLayoutObject())
          : false;
#if LAYOUT_GEOMETRY_MAP_LOGGING
  DLOG(INFO) << "LayoutGeometryMap::pushMappingsToAncestor from layer " << layer
             << " to layer " << ancestor_layer
             << ", canConvertInLayerTree=" << can_convert_in_layer_tree;
#endif

  if (can_convert_in_layer_tree) {
    LayoutPoint layer_offset;
    layer->ConvertToLayerCoords(ancestor_layer, layer_offset);

    // The LayoutView must be pushed first.
    if (!mapping_.size()) {
      DCHECK(ancestor_layer->GetLayoutObject().IsLayoutView());
      PushMappingsToAncestor(&ancestor_layer->GetLayoutObject(), nullptr);
    }

    AutoReset<size_t> position_change(&insertion_position_, mapping_.size());
    bool accumulating_transform =
        layout_object.Style()->Preserves3D() ||
        ancestor_layer->GetLayoutObject().Style()->Preserves3D();
    Push(&layout_object, ToLayoutSize(layer_offset),
         accumulating_transform ? kAccumulatingTransform : 0);
    return;
  }
  const LayoutBoxModelObject* ancestor_layout_object =
      ancestor_layer ? &ancestor_layer->GetLayoutObject() : nullptr;
  PushMappingsToAncestor(&layout_object, ancestor_layout_object);
}

void LayoutGeometryMap::Push(const LayoutObject* layout_object,
                             const LayoutSize& offset_from_container,
                             GeometryInfoFlags flags,
                             LayoutSize offset_for_fixed_position) {
#if LAYOUT_GEOMETRY_MAP_LOGGING
  DLOG(INFO) << "LayoutGeometryMap::push" << layout_object << " "
             << offset_from_container.Width().ToInt() << ","
             << offset_from_container.Height().ToInt()
             << " isNonUniform=" << kIsNonUniform;
#endif

  DCHECK_NE(insertion_position_, kNotFound);
  DCHECK(!layout_object->IsLayoutView() || !insertion_position_ ||
         map_coordinates_flags_ & kTraverseDocumentBoundaries);
  DCHECK(offset_for_fixed_position.IsZero() || layout_object->IsLayoutView());

  mapping_.insert(insertion_position_,
                  LayoutGeometryMapStep(layout_object, flags));

  LayoutGeometryMapStep& step = mapping_[insertion_position_];
  step.offset_ = offset_from_container;
  step.offset_for_fixed_position_ = offset_for_fixed_position;

  StepInserted(step);
}

void LayoutGeometryMap::Push(const LayoutObject* layout_object,
                             const TransformationMatrix& t,
                             GeometryInfoFlags flags,
                             LayoutSize offset_for_fixed_position) {
  DCHECK_NE(insertion_position_, kNotFound);
  DCHECK(!layout_object->IsLayoutView() || !insertion_position_ ||
         map_coordinates_flags_ & kTraverseDocumentBoundaries);
  DCHECK(offset_for_fixed_position.IsZero() || layout_object->IsLayoutView());

  mapping_.insert(insertion_position_,
                  LayoutGeometryMapStep(layout_object, flags));

  LayoutGeometryMapStep& step = mapping_[insertion_position_];
  step.offset_for_fixed_position_ = offset_for_fixed_position;

  if (!t.IsIntegerTranslation())
    step.transform_ = TransformationMatrix::Create(t);
  else
    step.offset_ = LayoutSize(LayoutUnit(t.E()), LayoutUnit(t.F()));

  StepInserted(step);
}

void LayoutGeometryMap::PopMappingsToAncestor(
    const LayoutBoxModelObject* ancestor_layout_object) {
  DCHECK(mapping_.size());

  bool might_be_saturated = false;
  while (mapping_.size() &&
         mapping_.back().layout_object_ != ancestor_layout_object) {
    might_be_saturated =
        might_be_saturated || accumulated_offset_.Width().MightBeSaturated();
    might_be_saturated =
        might_be_saturated || accumulated_offset_.Height().MightBeSaturated();
    StepRemoved(mapping_.back());
    mapping_.pop_back();
  }
  if (UNLIKELY(might_be_saturated)) {
    accumulated_offset_ = LayoutSize();
    for (const auto& step : mapping_)
      accumulated_offset_ += step.offset_;
  }
}

void LayoutGeometryMap::PopMappingsToAncestor(
    const PaintLayer* ancestor_layer) {
  const LayoutBoxModelObject* ancestor_layout_object =
      ancestor_layer ? &ancestor_layer->GetLayoutObject() : nullptr;
  PopMappingsToAncestor(ancestor_layout_object);
}

void LayoutGeometryMap::StepInserted(const LayoutGeometryMapStep& step) {
  accumulated_offset_ += step.offset_;

  if (step.flags_ & kIsNonUniform)
    ++non_uniform_steps_count_;

  if (step.transform_)
    ++transformed_steps_count_;

  if (step.flags_ & kIsFixedPosition)
    ++fixed_steps_count_;
}

void LayoutGeometryMap::StepRemoved(const LayoutGeometryMapStep& step) {
  accumulated_offset_ -= step.offset_;

  if (step.flags_ & kIsNonUniform) {
    DCHECK(non_uniform_steps_count_);
    --non_uniform_steps_count_;
  }

  if (step.transform_) {
    DCHECK(transformed_steps_count_);
    --transformed_steps_count_;
  }

  if (step.flags_ & kIsFixedPosition) {
    DCHECK(fixed_steps_count_);
    --fixed_steps_count_;
  }
}

#if DCHECK_IS_ON()
bool LayoutGeometryMap::IsTopmostLayoutView(
    const LayoutObject* layout_object) const {
  if (!layout_object->IsLayoutView())
    return false;

  // If we're not working with multiple LayoutViews, then any view is considered
  // "topmost" (to preserve original behavior).
  if (!(map_coordinates_flags_ & kTraverseDocumentBoundaries))
    return true;

  return layout_object->GetFrame()->IsMainFrame();
}
#endif

}  // namespace blink
