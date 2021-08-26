// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/pending_layer.h"

#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

namespace {

const ClipPaintPropertyNode* HighestOutputClipBetween(
    const EffectPaintPropertyNode& ancestor,
    const EffectPaintPropertyNode& descendant) {
  const ClipPaintPropertyNode* result = nullptr;
  for (const auto* effect = &descendant; effect != &ancestor;
       effect = effect->UnaliasedParent()) {
    if (const auto* output_clip = effect->OutputClip())
      result = &output_clip->Unalias();
  }
  return result;
}

// When possible, provides a clip rect that limits the visibility.
absl::optional<FloatRect> VisibilityLimit(const PropertyTreeState& state) {
  if (&state.Clip().LocalTransformSpace() == &state.Transform())
    return state.Clip().PixelSnappedClipRect().Rect();
  if (const auto* scroll = state.Transform().ScrollNode()) {
    return FloatRect(
        IntRect(scroll->ContainerRect().Location(), scroll->ContentsSize()));
  }
  return absl::nullopt;
}

bool IsCompositedScrollHitTest(const PaintChunk& chunk) {
  if (!chunk.hit_test_data)
    return false;
  const auto scroll_translation = chunk.hit_test_data->scroll_translation;
  return scroll_translation &&
         scroll_translation->HasDirectCompositingReasons();
}

bool IsCompositedScrollbar(const DisplayItem& item) {
  if (const auto* scrollbar = DynamicTo<ScrollbarDisplayItem>(item)) {
    const auto* scroll_translation = scrollbar->ScrollTranslation();
    return scroll_translation &&
           scroll_translation->HasDirectCompositingReasons();
  }
  return false;
}

// Snap |bounds| if within floating-point numeric limits of an integral rect.
void PreserveNearIntegralBounds(FloatRect& bounds) {
  if (std::abs(std::round(bounds.X()) - bounds.X()) <=
          std::numeric_limits<float>::epsilon() &&
      std::abs(std::round(bounds.Y()) - bounds.Y()) <=
          std::numeric_limits<float>::epsilon() &&
      std::abs(std::round(bounds.MaxX()) - bounds.MaxX()) <=
          std::numeric_limits<float>::epsilon() &&
      std::abs(std::round(bounds.MaxY()) - bounds.MaxY()) <=
          std::numeric_limits<float>::epsilon()) {
    bounds = FloatRect(RoundedIntRect(bounds));
  }
}

}  // anonymous namespace

PendingLayer::PendingLayer(const PaintChunkSubset& chunks,
                           const PaintChunkIterator& first_chunk)
    : PendingLayer(chunks, *first_chunk, first_chunk.IndexInPaintArtifact()) {}

PendingLayer::PendingLayer(const PaintChunkSubset& chunks,
                           const PaintChunk& first_chunk,
                           wtf_size_t first_chunk_index_in_paint_artifact)
    : bounds_(first_chunk.bounds),
      rect_known_to_be_opaque_(first_chunk.rect_known_to_be_opaque),
      has_text_(first_chunk.has_text),
      text_known_to_be_on_opaque_background_(
          first_chunk.text_known_to_be_on_opaque_background),
      chunks_(&chunks.GetPaintArtifact(), first_chunk_index_in_paint_artifact),
      property_tree_state_(
          first_chunk.properties.GetPropertyTreeState().Unalias()),
      compositing_type_(kOther) {
  DCHECK(!RequiresOwnLayer() || first_chunk.size() <= 1u);
  // Though text_known_to_be_on_opaque_background is only meaningful when
  // has_text is true, we expect text_known_to_be_on_opaque_background to be
  // true when !has_text to simplify code.
  DCHECK(has_text_ || text_known_to_be_on_opaque_background_);
  if (const absl::optional<FloatRect>& visibility_limit =
          VisibilityLimit(property_tree_state_)) {
    bounds_.Intersect(*visibility_limit);
  }

  if (IsCompositedScrollHitTest(first_chunk)) {
    compositing_type_ = kScrollHitTestLayer;
  } else if (first_chunk.size()) {
    const auto& first_display_item = FirstDisplayItem();
    if (first_display_item.IsForeignLayer())
      compositing_type_ = kForeignLayer;
    else if (IsCompositedScrollbar(first_display_item))
      compositing_type_ = kScrollbarLayer;
  }
}

PendingLayer::PendingLayer(const PreCompositedLayerInfo& pre_composited_layer)
    : chunks_(pre_composited_layer.chunks),
      property_tree_state_(
          pre_composited_layer.graphics_layer->GetPropertyTreeState()
              .Unalias()),
      graphics_layer_(pre_composited_layer.graphics_layer),
      compositing_type_(kPreCompositedLayer) {
  DCHECK(graphics_layer_);
  DCHECK(!graphics_layer_->ShouldCreateLayersAfterPaint());
}

FloatPoint PendingLayer::LayerOffset() const {
  // The solid color layer optimization is important for performance. Snapping
  // the location could make the solid color drawings not cover the entire
  // cc::Layer which would make the layer non-solid-color.
  if (IsSolidColor())
    return bounds_.Location();
  // Otherwise return integral offset to reduce chance of additional blurriness.
  return FloatPoint(FlooredIntPoint(bounds_.Location()));
}

IntSize PendingLayer::LayerBounds() const {
  // Because solid color layers do not adjust their location (see:
  // |PendingLayer::LayerOffset()|), we only expand their size here.
  if (IsSolidColor())
    return ExpandedIntSize(bounds_.Size());
  return EnclosingIntRect(bounds_).Size();
}

FloatRect PendingLayer::MapRectKnownToBeOpaque(
    const PropertyTreeState& new_state) const {
  if (rect_known_to_be_opaque_.IsEmpty())
    return FloatRect();

  FloatClipRect float_clip_rect(rect_known_to_be_opaque_);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state_, new_state,
                                            float_clip_rect);
  return float_clip_rect.IsTight() ? float_clip_rect.Rect() : FloatRect();
}

std::unique_ptr<JSONObject> PendingLayer::ToJSON() const {
  std::unique_ptr<JSONObject> result = std::make_unique<JSONObject>();
  result->SetArray("bounds", RectAsJSONArray(bounds_));
  result->SetArray("rect_known_to_be_opaque",
                   RectAsJSONArray(rect_known_to_be_opaque_));
  result->SetObject("property_tree_state", property_tree_state_.ToJSON());
  result->SetArray("offset_of_decomposited_transforms",
                   PointAsJSONArray(offset_of_decomposited_transforms_));
  std::unique_ptr<JSONArray> json_chunks = std::make_unique<JSONArray>();
  for (auto it = chunks_.begin(); it != chunks_.end(); ++it) {
    StringBuilder sb;
    sb.Append("index=");
    sb.AppendNumber(it.IndexInPaintArtifact());
    sb.Append(" ");
    sb.Append(it->ToString());
    json_chunks->PushString(sb.ToString());
  }
  result->SetArray("paint_chunks", std::move(json_chunks));
  return result;
}

FloatRect PendingLayer::VisualRectForOverlapTesting(
    const PropertyTreeState& ancestor_state) const {
  FloatClipRect visual_rect(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(
      property_tree_state_, ancestor_state, visual_rect,
      kIgnoreOverlayScrollbarSize, kNonInclusiveIntersect,
      kExpandVisualRectForCompositingOverlap);
  return visual_rect.Rect();
}

void PendingLayer::Upcast(const PropertyTreeState& new_state) {
  DCHECK(!RequiresOwnLayer());
  if (property_tree_state_ == new_state)
    return;

  FloatClipRect float_clip_rect(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state_, new_state,
                                            float_clip_rect);
  bounds_ = float_clip_rect.Rect();

  rect_known_to_be_opaque_ = MapRectKnownToBeOpaque(new_state);
  property_tree_state_ = new_state;
}

const PaintChunk& PendingLayer::FirstPaintChunk() const {
  DCHECK(!RequiresOwnLayer() || chunks_.size() == 1);
  return *chunks_.begin();
}

const DisplayItem& PendingLayer::FirstDisplayItem() const {
#if DCHECK_IS_ON()
  // This method should never be called if the first paint chunk is empty.
  if (RequiresOwnLayer())
    DCHECK_EQ(FirstPaintChunk().size(), 1u);
  else
    DCHECK_GE(FirstPaintChunk().size(), 1u);
#endif
  return *chunks_.begin().DisplayItems().begin();
}

bool PendingLayer::MayDrawContent() const {
  return Chunks().size() > 1 || FirstPaintChunk().size() > 0;
}

// We will only allow merging if
// merged_area - (home_area + guest_area) <= kMergeSparsityAreaTolerance
static constexpr float kMergeSparsityAreaTolerance = 10000;

bool PendingLayer::MergeInternal(const PendingLayer& guest,
                                 const PropertyTreeState& guest_state,
                                 bool prefers_lcd_text,
                                 bool dry_run) {
  if (&Chunks().GetPaintArtifact() != &guest.Chunks().GetPaintArtifact())
    return false;
  if (RequiresOwnLayer() || guest.RequiresOwnLayer())
    return false;
  if (&GetPropertyTreeState().Effect() != &guest_state.Effect())
    return false;

  const absl::optional<PropertyTreeState>& merged_state =
      GetPropertyTreeState().CanUpcastWith(guest_state);
  if (!merged_state)
    return false;

  const absl::optional<FloatRect>& merged_visibility_limit =
      VisibilityLimit(*merged_state);

  // If the current bounds and known-to-be-opaque area already cover the entire
  // visible area of the merged state, and the current state is already equal
  // to the merged state, we can merge the guest immediately without needing to
  // update any bounds at all. This simple merge fast-path avoids the cost of
  // mapping the visual rects, below.
  if (merged_visibility_limit && *merged_visibility_limit == bounds_ &&
      merged_state == property_tree_state_ &&
      rect_known_to_be_opaque_.Contains(bounds_)) {
    if (!dry_run) {
      chunks_.Merge(guest.Chunks());
      text_known_to_be_on_opaque_background_ = true;
      has_text_ |= guest.has_text_;
      change_of_decomposited_transforms_ =
          std::max(ChangeOfDecompositedTransforms(),
                   guest.ChangeOfDecompositedTransforms());
    }
    return true;
  }

  FloatClipRect new_home_bounds(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(GetPropertyTreeState(),
                                            *merged_state, new_home_bounds);
  if (merged_visibility_limit)
    new_home_bounds.Rect().Intersect(*merged_visibility_limit);

  FloatClipRect new_guest_bounds(guest.bounds_);
  GeometryMapper::LocalToAncestorVisualRect(guest_state, *merged_state,
                                            new_guest_bounds);
  if (merged_visibility_limit)
    new_guest_bounds.Rect().Intersect(*merged_visibility_limit);

  FloatRect merged_bounds =
      UnionRect(new_home_bounds.Rect(), new_guest_bounds.Rect());
  float sum_area = new_home_bounds.Rect().Size().Area() +
                   new_guest_bounds.Rect().Size().Area();
  if (merged_bounds.Size().Area() - sum_area > kMergeSparsityAreaTolerance)
    return false;

  FloatRect merged_rect_known_to_be_opaque =
      MaximumCoveredRect(MapRectKnownToBeOpaque(*merged_state),
                         guest.MapRectKnownToBeOpaque(*merged_state));
  bool merged_text_known_to_be_on_opaque_background =
      text_known_to_be_on_opaque_background_;
  if (text_known_to_be_on_opaque_background_ !=
      guest.text_known_to_be_on_opaque_background_) {
    if (!text_known_to_be_on_opaque_background_) {
      if (merged_rect_known_to_be_opaque.Contains(new_home_bounds.Rect()))
        merged_text_known_to_be_on_opaque_background = true;
    } else if (!guest.text_known_to_be_on_opaque_background_) {
      if (!merged_rect_known_to_be_opaque.Contains(new_guest_bounds.Rect()))
        merged_text_known_to_be_on_opaque_background = false;
    }
  }
  if (prefers_lcd_text && !merged_text_known_to_be_on_opaque_background) {
    if (has_text_ && text_known_to_be_on_opaque_background_)
      return false;
    if (guest.has_text_ && guest.text_known_to_be_on_opaque_background_)
      return false;
  }

  if (!dry_run) {
    chunks_.Merge(guest.Chunks());
    bounds_ = merged_bounds;
    property_tree_state_ = *merged_state;
    rect_known_to_be_opaque_ = merged_rect_known_to_be_opaque;
    text_known_to_be_on_opaque_background_ =
        merged_text_known_to_be_on_opaque_background;
    has_text_ |= guest.has_text_;
    change_of_decomposited_transforms_ =
        std::max(ChangeOfDecompositedTransforms(),
                 guest.ChangeOfDecompositedTransforms());
    // GeometryMapper::LocalToAncestorVisualRect can introduce floating-point
    // error to the bounds. Integral bounds are important for reducing
    // blurriness (see: PendingLayer::LayerOffset) so preserve that here.
    PreserveNearIntegralBounds(bounds_);
    PreserveNearIntegralBounds(rect_known_to_be_opaque_);
  }
  return true;
}

const TransformPaintPropertyNode&
PendingLayer::ScrollTranslationForScrollHitTestLayer() const {
  DCHECK_EQ(GetCompositingType(), kScrollHitTestLayer);
  DCHECK_EQ(1u, Chunks().size());
  const auto& paint_chunk = FirstPaintChunk();
  DCHECK(paint_chunk.hit_test_data);
  DCHECK(paint_chunk.hit_test_data->scroll_translation);
  DCHECK(paint_chunk.hit_test_data->scroll_translation->ScrollNode());
  return *paint_chunk.hit_test_data->scroll_translation;
}

bool PendingLayer::PropertyTreeStateChanged() const {
  auto change = PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  if (change_of_decomposited_transforms_ >= change)
    return true;

  return property_tree_state_.ChangedToRoot(change);
}

bool PendingLayer::MightOverlap(const PendingLayer& other) const {
  PropertyTreeState common_ancestor_state(
      property_tree_state_.Transform()
          .LowestCommonAncestor(other.property_tree_state_.Transform())
          .Unalias(),
      property_tree_state_.Clip()
          .LowestCommonAncestor(other.property_tree_state_.Clip())
          .Unalias(),
      property_tree_state_.Effect()
          .LowestCommonAncestor(other.property_tree_state_.Effect())
          .Unalias());
  // Move the common clip up if some effect nodes have OutputClip escaping the
  // common clip.
  if (const auto* clip_a = HighestOutputClipBetween(
          common_ancestor_state.Effect(), property_tree_state_.Effect())) {
    common_ancestor_state.SetClip(
        clip_a->LowestCommonAncestor(common_ancestor_state.Clip()).Unalias());
  }
  if (const auto* clip_b =
          HighestOutputClipBetween(common_ancestor_state.Effect(),
                                   other.property_tree_state_.Effect())) {
    common_ancestor_state.SetClip(
        clip_b->LowestCommonAncestor(common_ancestor_state.Clip()).Unalias());
  }
  return VisualRectForOverlapTesting(common_ancestor_state)
      .Intersects(other.VisualRectForOverlapTesting(common_ancestor_state));
}

// Walk the pending layer list and build up a table of transform nodes that
// can be de-composited (replaced with offset_to_transform_parent). A
// transform node can be de-composited if:
//  1. It is not the root transform node.
//  2. It is a 2d translation only.
//  3. The transform is not used for scrolling - its ScrollNode() is nullptr.
//  4. The transform is not a StickyTranslation node.
//  5. It has no direct compositing reasons, other than k3DTransform. Note
//     that if it has a k3DTransform reason, check #2 above ensures that it
//     isn't really 3D.
//  6. It has FlattensInheritedTransform matching that of its direct parent.
//  7. It has backface visibility matching its direct parent.
//  8. No clips have local_transform_space referring to this transform node.
//  9. No effects have local_transform_space referring to this transform node.
//  10. All child transform nodes are also able to be de-composited.
// This algorithm should be O(t+c+e) where t,c,e are the number of transform,
// clip, and effect nodes in the full tree.
void PendingLayer::DecompositeTransforms(Vector<PendingLayer>& pending_layers) {
  HashMap<const TransformPaintPropertyNode*, bool> can_be_decomposited;
  HashSet<const void*> clips_and_effects_seen;
  for (const PendingLayer& pending_layer : pending_layers) {
    const auto& property_state = pending_layer.GetPropertyTreeState();

    // Lambda to handle marking a transform node false, and walking up all
    // true parents and marking them false as well. This also handles
    // inserting transform_node if it isn't in the map, and keeps track of
    // clips or effects.
    auto mark_not_decompositable =
        [&can_be_decomposited](
            const TransformPaintPropertyNode& transform_node) {
          for (const auto* node = &transform_node; node && !node->IsRoot();
               node = node->UnaliasedParent()) {
            auto result = can_be_decomposited.insert(node, false);
            if (!result.is_new_entry) {
              if (!result.stored_value->value)
                break;
              result.stored_value->value = false;
            }
          }
        };

    // Add the transform and all transform parents to the map.
    for (const auto* node = &property_state.Transform();
         !node->IsRoot() && !can_be_decomposited.Contains(node);
         node = &node->Parent()->Unalias()) {
      if (!node->IsIdentityOr2DTranslation() || node->ScrollNode() ||
          node->GetStickyConstraint() ||
          node->IsAffectedByOuterViewportBoundsDelta() ||
          node->HasDirectCompositingReasonsOtherThan3dTransform() ||
          !node->FlattensInheritedTransformSameAsParent() ||
          !node->BackfaceVisibilitySameAsParent()) {
        mark_not_decompositable(*node);
        break;
      }
      can_be_decomposited.insert(node, true);
    }

    // Add clips and effects, and their parents, that we haven't already seen.
    for (const auto* node = &property_state.Clip();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(node->LocalTransformSpace().Unalias());
    }
    for (const auto* node = &property_state.Effect();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(node->LocalTransformSpace().Unalias());
    }

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        pending_layer.GetCompositingType() == kScrollHitTestLayer) {
      // The scroll translation node of a scroll hit test layer may not be
      // referenced by any pending layer's property tree state. Disallow
      // decomposition of it (and its ancestors).
      mark_not_decompositable(
          pending_layer.ScrollTranslationForScrollHitTestLayer());
    }
  }

  // Now, for any transform nodes that can be de-composited, re-map their
  // transform to point to the correct parent, and set the
  // offset_to_transform_parent.
  for (PendingLayer& pending_layer : pending_layers) {
    const auto* transform = &pending_layer.GetPropertyTreeState().Transform();
    while (!transform->IsRoot() && can_be_decomposited.at(transform)) {
      pending_layer.offset_of_decomposited_transforms_ +=
          transform->Translation2D();
      pending_layer.change_of_decomposited_transforms_ =
          std::max(pending_layer.ChangeOfDecompositedTransforms(),
                   transform->NodeChanged());
      transform = &transform->Parent()->Unalias();
    }
    pending_layer.property_tree_state_.SetTransform(*transform);
    pending_layer.bounds_.MoveBy(
        pending_layer.OffsetOfDecompositedTransforms());
    pending_layer.rect_known_to_be_opaque_.MoveBy(
        pending_layer.OffsetOfDecompositedTransforms());
  }
}

bool PendingLayer::IsSolidColor() const {
  if (Chunks().size() != 1)
    return false;
  const auto& items = chunks_.begin().DisplayItems();
  if (items.size() != 1)
    return false;
  auto* drawing = DynamicTo<DrawingDisplayItem>(*items.begin());
  return drawing && drawing->IsSolidColor();
}

}  // namespace blink
