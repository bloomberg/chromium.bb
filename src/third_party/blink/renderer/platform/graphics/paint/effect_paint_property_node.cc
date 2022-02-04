// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

namespace {

PaintPropertyChangeType ComputeBackdropFilterChange(
    const EffectPaintPropertyNode::BackdropFilterInfo* a,
    const EffectPaintPropertyNode::BackdropFilterInfo* b,
    bool is_running_backdrop_filter_animation_on_compositor) {
  if (!a && !b)
    return PaintPropertyChangeType::kUnchanged;
  if (!a || !b || a->bounds != b->bounds ||
      a->mask_element_id != b->mask_element_id)
    return PaintPropertyChangeType::kChangedOnlyValues;
  if (a->operations != b->operations) {
    return is_running_backdrop_filter_animation_on_compositor
               ? PaintPropertyChangeType::kChangedOnlyCompositedValues
               : PaintPropertyChangeType::kChangedOnlyValues;
  }
  return PaintPropertyChangeType::kUnchanged;
}

} // anonymous namespace

PaintPropertyChangeType EffectPaintPropertyNode::State::ComputeChange(
    const State& other,
    const AnimationState& animation_state) const {
  if (local_transform_space != other.local_transform_space ||
      output_clip != other.output_clip || blend_mode != other.blend_mode ||
      document_transition_shared_element_id !=
          other.document_transition_shared_element_id ||
      shared_element_resource_id != other.shared_element_resource_id) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }
  bool opacity_changed = opacity != other.opacity;
  bool opacity_change_is_simple =
      opacity_changed &&
      // Opacity change is simple if
      // - opacity doesn't change from or to 1, or
      // - there was and is active opacity animation, or
      // TODO(crbug.com/1285498): Optimize for will-change: opacity.
      // The rule is because whether opacity is 1 affects whether the effect
      // should create a render surface if there is no active opacity animation.
      ((opacity != 1.f && other.opacity != 1.f) ||
       (has_active_opacity_animation && other.has_active_opacity_animation));
  if (opacity_changed && !opacity_change_is_simple) {
    DCHECK(!animation_state.is_running_opacity_animation_on_compositor);
    return PaintPropertyChangeType::kChangedOnlyValues;
  }
  bool filter_changed = filter != other.filter;
  if (filter_changed &&
      !animation_state.is_running_filter_animation_on_compositor) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }
  auto backdrop_filter_changed = ComputeBackdropFilterChange(
      backdrop_filter_info.get(), other.backdrop_filter_info.get(),
      animation_state.is_running_backdrop_filter_animation_on_compositor);
  if (backdrop_filter_changed == PaintPropertyChangeType::kChangedOnlyValues) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }
  bool non_reraster_values_changed =
      direct_compositing_reasons != other.direct_compositing_reasons ||
      compositor_element_id != other.compositor_element_id;
  bool simple_values_changed =
      opacity_change_is_simple &&
      !animation_state.is_running_opacity_animation_on_compositor;
  if (non_reraster_values_changed && simple_values_changed)
    return PaintPropertyChangeType::kChangedOnlyValues;
  if (non_reraster_values_changed)
    return PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  if (simple_values_changed)
    return PaintPropertyChangeType::kChangedOnlySimpleValues;

  if (opacity_changed || filter_changed ||
      backdrop_filter_changed != PaintPropertyChangeType::kUnchanged) {
    return PaintPropertyChangeType::kChangedOnlyCompositedValues;
  }
  return PaintPropertyChangeType::kUnchanged;
}

const EffectPaintPropertyNode& EffectPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(EffectPaintPropertyNode, root,
                    base::AdoptRef(new EffectPaintPropertyNode(
                        nullptr, State{&TransformPaintPropertyNode::Root(),
                                       &ClipPaintPropertyNode::Root()})));
  return *root;
}

bool EffectPaintPropertyNodeOrAlias::Changed(
    PaintPropertyChangeType change,
    const PropertyTreeState& relative_to_state,
    const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const {
  const auto& relative_effect = relative_to_state.Effect();
  const auto& relative_transform = relative_to_state.Transform();

  // Note that we can't unalias nodes in the loop conditions, since we need to
  // check NodeChanged() function on aliased nodes as well (since the parenting
  // might change).
  for (const auto* node = this; node && node != &relative_effect;
       node = node->Parent()) {
    if (node->NodeChanged() >= change)
      return true;

    // We shouldn't check state on aliased nodes, other than NodeChanged().
    if (node->IsParentAlias())
      continue;

    const auto* unaliased = static_cast<const EffectPaintPropertyNode*>(node);
    const auto& local_transform = unaliased->LocalTransformSpace();
    if (unaliased->HasFilterThatMovesPixels() &&
        &local_transform != transform_not_to_check &&
        local_transform.Changed(change, relative_transform)) {
      return true;
    }
    // We don't check for change of OutputClip here to avoid N^3 complexity.
    // The caller should check for clip change in other ways.
  }

  return false;
}

void EffectPaintPropertyNodeOrAlias::ClearChangedToRoot(
    int sequence_number) const {
  for (auto* n = this; n && n->ChangedSequenceNumber() != sequence_number;
       n = n->Parent()) {
    n->ClearChanged(sequence_number);
    if (n->IsParentAlias())
      continue;
    const auto* unaliased = static_cast<const EffectPaintPropertyNode*>(n);
    unaliased->LocalTransformSpace().ClearChangedToRoot(sequence_number);
    if (const auto* output_clip = unaliased->OutputClip())
      output_clip->ClearChangedToRoot(sequence_number);
  }
}

gfx::RectF EffectPaintPropertyNode::MapRect(const gfx::RectF& rect) const {
  if (state_.filter.IsEmpty())
    return rect;
  return state_.filter.MapRect(rect);
}

std::unique_ptr<JSONObject> EffectPaintPropertyNode::ToJSON() const {
  auto json = ToJSONBase();
  json->SetString("localTransformSpace",
                  String::Format("%p", state_.local_transform_space.get()));
  json->SetString("outputClip", String::Format("%p", state_.output_clip.get()));
  if (!state_.filter.IsEmpty())
    json->SetString("filter", state_.filter.ToString());
  if (auto* backdrop_filter = BackdropFilter())
    json->SetString("backdrop_filter", backdrop_filter->ToString());
  if (state_.opacity != 1.0f)
    json->SetDouble("opacity", state_.opacity);
  if (state_.blend_mode != SkBlendMode::kSrcOver)
    json->SetString("blendMode", SkBlendMode_Name(state_.blend_mode));
  if (state_.direct_compositing_reasons != CompositingReason::kNone) {
    json->SetString(
        "directCompositingReasons",
        CompositingReason::ToString(state_.direct_compositing_reasons));
  }
  if (state_.compositor_element_id) {
    json->SetString("compositorElementId",
                    state_.compositor_element_id.ToString().c_str());
  }
  return json;
}

}  // namespace blink
