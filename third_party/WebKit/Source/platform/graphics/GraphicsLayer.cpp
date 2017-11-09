/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "platform/graphics/GraphicsLayer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include "SkMatrix44.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/layers/layer.h"
#include "platform/DragImage.h"
#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/GeometryAsJSON.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/Region.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/FirstPaintInvalidationTracking.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/LinkHighlight.h"
#include "platform/graphics/compositing/CompositedLayerRasterInvalidator.h"
#include "platform/graphics/compositing/PaintChunksToCcLayer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/json/JSONValues.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/text/TextStream.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebDisplayItemList.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebSize.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

std::unique_ptr<GraphicsLayer> GraphicsLayer::Create(
    GraphicsLayerClient* client) {
  return WTF::WrapUnique(new GraphicsLayer(client));
}

GraphicsLayer::GraphicsLayer(GraphicsLayerClient* client)
    : client_(client),
      background_color_(Color::kTransparent),
      opacity_(1),
      blend_mode_(WebBlendMode::kNormal),
      has_transform_origin_(false),
      contents_opaque_(false),
      should_flatten_transform_(true),
      backface_visibility_(true),
      draws_content_(false),
      contents_visible_(true),
      is_root_for_isolated_group_(false),
      hit_testable_without_draws_content_(false),
      has_scroll_parent_(false),
      has_clip_parent_(false),
      painted_(false),
      painting_phase_(kGraphicsLayerPaintAllWithOverflowClip),
      parent_(nullptr),
      mask_layer_(nullptr),
      contents_clipping_mask_layer_(nullptr),
      paint_count_(0),
      contents_layer_(nullptr),
      contents_layer_id_(0),
      scrollable_area_(nullptr),
      rendering_context3d_(0) {
#if DCHECK_IS_ON()
  if (client_)
    client_->VerifyNotPainting();
#endif
  layer_ = Platform::Current()->CompositorSupport()->CreateContentLayer(this);
  layer_->Layer()->SetDrawsContent(draws_content_ && contents_visible_);
  layer_->Layer()->SetLayerClient(this);

  UpdateTrackingRasterInvalidations();
}

GraphicsLayer::~GraphicsLayer() {
  for (size_t i = 0; i < link_highlights_.size(); ++i)
    link_highlights_[i]->ClearCurrentGraphicsLayer();
  link_highlights_.clear();

#if DCHECK_IS_ON()
  if (client_)
    client_->VerifyNotPainting();
#endif

  RemoveAllChildren();
  RemoveFromParent();
  DCHECK(!parent_);
}

LayoutRect GraphicsLayer::VisualRect() const {
  LayoutRect bounds = LayoutRect(FloatPoint(), Size());
  bounds.Move(OffsetFromLayoutObjectWithSubpixelAccumulation());
  return bounds;
}

void GraphicsLayer::SetHasWillChangeTransformHint(
    bool has_will_change_transform) {
  layer_->Layer()->SetHasWillChangeTransformHint(has_will_change_transform);
}

void GraphicsLayer::SetScrollBoundaryBehavior(
    const WebScrollBoundaryBehavior& behavior) {
  layer_->Layer()->SetScrollBoundaryBehavior(behavior);
}

void GraphicsLayer::SetIsResizedByBrowserControls(
    bool is_resized_by_browser_controls) {
  PlatformLayer()->SetIsResizedByBrowserControls(
      is_resized_by_browser_controls);
}

void GraphicsLayer::SetParent(GraphicsLayer* layer) {
#if DCHECK_IS_ON()
  DCHECK(!layer || !layer->HasAncestor(this));
#endif
  parent_ = layer;
}

#if DCHECK_IS_ON()

bool GraphicsLayer::HasAncestor(GraphicsLayer* ancestor) const {
  for (GraphicsLayer* curr = Parent(); curr; curr = curr->Parent()) {
    if (curr == ancestor)
      return true;
  }

  return false;
}

#endif

bool GraphicsLayer::SetChildren(const GraphicsLayerVector& new_children) {
  // If the contents of the arrays are the same, nothing to do.
  if (new_children == children_)
    return false;

  RemoveAllChildren();

  size_t list_size = new_children.size();
  for (size_t i = 0; i < list_size; ++i)
    AddChildInternal(new_children[i]);

  UpdateChildList();

  return true;
}

void GraphicsLayer::AddChildInternal(GraphicsLayer* child_layer) {
  DCHECK_NE(child_layer, this);

  if (child_layer->Parent())
    child_layer->RemoveFromParent();

  child_layer->SetParent(this);
  children_.push_back(child_layer);

  // Don't call updateChildList here, this function is used in cases where it
  // should not be called until all children are processed.
}

void GraphicsLayer::AddChild(GraphicsLayer* child_layer) {
  AddChildInternal(child_layer);
  UpdateChildList();
}

void GraphicsLayer::AddChildBelow(GraphicsLayer* child_layer,
                                  GraphicsLayer* sibling) {
  DCHECK_NE(child_layer, this);
  child_layer->RemoveFromParent();

  bool found = false;
  for (unsigned i = 0; i < children_.size(); i++) {
    if (sibling == children_[i]) {
      children_.insert(i, child_layer);
      found = true;
      break;
    }
  }

  child_layer->SetParent(this);

  if (!found)
    children_.push_back(child_layer);

  UpdateChildList();
}

void GraphicsLayer::RemoveAllChildren() {
  while (!children_.IsEmpty()) {
    GraphicsLayer* cur_layer = children_.back();
    DCHECK(cur_layer->Parent());
    cur_layer->RemoveFromParent();
  }
}

void GraphicsLayer::RemoveFromParent() {
  if (parent_) {
    // We use reverseFind so that removeAllChildren() isn't n^2.
    parent_->children_.EraseAt(parent_->children_.ReverseFind(this));
    SetParent(nullptr);
  }

  PlatformLayer()->RemoveFromParent();
}

void GraphicsLayer::SetOffsetFromLayoutObject(
    const IntSize& offset,
    ShouldSetNeedsDisplay should_set_needs_display) {
  SetOffsetDoubleFromLayoutObject(offset);
}

void GraphicsLayer::SetOffsetDoubleFromLayoutObject(
    const DoubleSize& offset,
    ShouldSetNeedsDisplay should_set_needs_display) {
  if (offset == offset_from_layout_object_)
    return;

  offset_from_layout_object_ = offset;
  PlatformLayer()->SetFiltersOrigin(FloatPoint() - ToFloatSize(offset));

  // If the compositing layer offset changes, we need to repaint.
  if (should_set_needs_display == kSetNeedsDisplay)
    SetNeedsDisplay();
}

LayoutSize GraphicsLayer::OffsetFromLayoutObjectWithSubpixelAccumulation()
    const {
  return LayoutSize(OffsetFromLayoutObject()) +
         Client()->SubpixelAccumulation();
}

IntRect GraphicsLayer::InterestRect() {
  return previous_interest_rect_;
}

void GraphicsLayer::Paint(const IntRect* interest_rect,
                          GraphicsContext::DisabledMode disabled_mode) {
  if (!PaintWithoutCommit(interest_rect, disabled_mode))
    return;

  GetPaintController().CommitNewDisplayItems();

  if (layer_state_) {
    // Generate raster invalidations for SPv175 (but not SPv2).
    DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());
    IntRect layer_bounds(layer_state_->offset, ExpandedIntSize(Size()));
    EnsureRasterInvalidator().Generate(layer_bounds, AllChunkPointers(),
                                       layer_state_->state, this);
  }

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      DrawsContent()) {
    auto& tracking = EnsureRasterInvalidator().EnsureTracking();
    tracking.CheckUnderInvalidations(
        DebugName(), CapturePaintRecord(), InterestRect(),
        layer_state_ ? layer_state_->offset : IntPoint());
    if (auto record = tracking.UnderInvalidationRecord()) {
      // Add the under-invalidation overlay onto the painted result.
      GetPaintController().AppendDebugDrawingAfterCommit(
          *this, std::move(record),
          layer_state_ ? &layer_state_->state : nullptr);
      // Ensure the compositor will raster the under-invalidation overlay.
      layer_->Layer()->Invalidate();
    }
  }
}

bool GraphicsLayer::PaintWithoutCommit(
    const IntRect* interest_rect,
    GraphicsContext::DisabledMode disabled_mode) {
  DCHECK(DrawsContent());

  if (!client_)
    return false;
  if (FirstPaintInvalidationTracking::IsEnabled())
    debug_info_.ClearAnnotatedInvalidateRects();
  IncrementPaintCount();

  IntRect new_interest_rect;
  if (!interest_rect) {
    new_interest_rect =
        client_->ComputeInterestRect(this, previous_interest_rect_);
    interest_rect = &new_interest_rect;
  }

  if (!GetPaintController().SubsequenceCachingIsDisabled() &&
      !client_->NeedsRepaint(*this) &&
      !GetPaintController().CacheIsAllInvalid() &&
      previous_interest_rect_ == *interest_rect) {
    return false;
  }

  GraphicsContext context(GetPaintController(), disabled_mode, nullptr);

  previous_interest_rect_ = *interest_rect;
  client_->PaintContents(this, context, painting_phase_, *interest_rect);
  return true;
}

void GraphicsLayer::UpdateChildList() {
  WebLayer* child_host = layer_->Layer();
  child_host->RemoveAllChildren();

  ClearContentsLayerIfUnregistered();

  if (contents_layer_) {
    // FIXME: Add the contents layer in the correct order with negative z-order
    // children. This does not currently cause visible rendering issues because
    // contents layers are only used for replaced elements that don't have
    // children.
    child_host->AddChild(contents_layer_);
  }

  for (size_t i = 0; i < children_.size(); ++i)
    child_host->AddChild(children_[i]->PlatformLayer());

  for (size_t i = 0; i < link_highlights_.size(); ++i)
    child_host->AddChild(link_highlights_[i]->Layer());
}

void GraphicsLayer::UpdateLayerIsDrawable() {
  // For the rest of the accelerated compositor code, there is no reason to make
  // a distinction between drawsContent and contentsVisible. So, for
  // m_layer->layer(), these two flags are combined here. |m_contentsLayer|
  // shouldn't receive the drawsContent flag, so it is only given
  // contentsVisible.

  layer_->Layer()->SetDrawsContent(draws_content_ && contents_visible_);
  if (WebLayer* contents_layer = ContentsLayerIfRegistered())
    contents_layer->SetDrawsContent(contents_visible_);

  if (draws_content_) {
    layer_->Layer()->Invalidate();
    for (size_t i = 0; i < link_highlights_.size(); ++i)
      link_highlights_[i]->Invalidate();
  }
}

void GraphicsLayer::UpdateContentsRect() {
  WebLayer* contents_layer = ContentsLayerIfRegistered();
  if (!contents_layer)
    return;

  contents_layer->SetPosition(
      FloatPoint(contents_rect_.X(), contents_rect_.Y()));
  contents_layer->SetBounds(
      IntSize(contents_rect_.Width(), contents_rect_.Height()));

  if (contents_clipping_mask_layer_) {
    if (contents_clipping_mask_layer_->Size() != contents_rect_.Size()) {
      contents_clipping_mask_layer_->SetSize(FloatSize(contents_rect_.Size()));
      contents_clipping_mask_layer_->SetNeedsDisplay();
    }
    contents_clipping_mask_layer_->SetPosition(FloatPoint());
    contents_clipping_mask_layer_->SetOffsetFromLayoutObject(
        OffsetFromLayoutObject() +
        IntSize(contents_rect_.Location().X(), contents_rect_.Location().Y()));
  }
}

static HashSet<int>* g_registered_layer_set;

void GraphicsLayer::RegisterContentsLayer(WebLayer* layer) {
  if (!g_registered_layer_set)
    g_registered_layer_set = new HashSet<int>;
  CHECK(!g_registered_layer_set->Contains(layer->Id()));
  g_registered_layer_set->insert(layer->Id());
}

void GraphicsLayer::UnregisterContentsLayer(WebLayer* layer) {
  DCHECK(g_registered_layer_set);
  CHECK(g_registered_layer_set->Contains(layer->Id()));
  g_registered_layer_set->erase(layer->Id());
}

void GraphicsLayer::SetContentsTo(WebLayer* layer) {
  bool children_changed = false;
  if (layer) {
    DCHECK(g_registered_layer_set);
    CHECK(g_registered_layer_set->Contains(layer->Id()));
    if (contents_layer_id_ != layer->Id()) {
      SetupContentsLayer(layer);
      children_changed = true;
    }
    UpdateContentsRect();
  } else {
    if (contents_layer_) {
      children_changed = true;

      // The old contents layer will be removed via updateChildList.
      contents_layer_ = nullptr;
      contents_layer_id_ = 0;
    }
  }

  if (children_changed)
    UpdateChildList();
}

void GraphicsLayer::SetupContentsLayer(WebLayer* contents_layer) {
  DCHECK(contents_layer);
  contents_layer_ = contents_layer;
  contents_layer_id_ = contents_layer_->Id();

  contents_layer_->SetLayerClient(this);
  contents_layer_->SetTransformOrigin(FloatPoint3D());
  contents_layer_->SetUseParentBackfaceVisibility(true);

  // It is necessary to call setDrawsContent as soon as we receive the new
  // contentsLayer, for the correctness of early exit conditions in
  // setDrawsContent() and setContentsVisible().
  contents_layer_->SetDrawsContent(contents_visible_);

  // Insert the content layer first. Video elements require this, because they
  // have shadow content that must display in front of the video.
  layer_->Layer()->InsertChild(contents_layer_, 0);
  WebLayer* border_web_layer =
      contents_clipping_mask_layer_
          ? contents_clipping_mask_layer_->PlatformLayer()
          : nullptr;
  contents_layer_->SetMaskLayer(border_web_layer);

  contents_layer_->SetRenderingContext(rendering_context3d_);
}

void GraphicsLayer::ClearContentsLayerIfUnregistered() {
  if (!contents_layer_id_ ||
      g_registered_layer_set->Contains(contents_layer_id_))
    return;

  contents_layer_ = nullptr;
  contents_layer_id_ = 0;
}

GraphicsLayerDebugInfo& GraphicsLayer::DebugInfo() {
  return debug_info_;
}

WebLayer* GraphicsLayer::ContentsLayerIfRegistered() {
  ClearContentsLayerIfUnregistered();
  return contents_layer_;
}

CompositedLayerRasterInvalidator& GraphicsLayer::EnsureRasterInvalidator() {
  if (!raster_invalidator_) {
    raster_invalidator_ = std::make_unique<CompositedLayerRasterInvalidator>(
        [this](const IntRect& r) { SetNeedsDisplayInRectInternal(r); });
    raster_invalidator_->SetTracksRasterInvalidations(
        client_->IsTrackingRasterInvalidations());
  }
  return *raster_invalidator_;
}

void GraphicsLayer::UpdateTrackingRasterInvalidations() {
  bool should_track = client_->IsTrackingRasterInvalidations();
  if (should_track)
    EnsureRasterInvalidator().SetTracksRasterInvalidations(true);
  else if (raster_invalidator_)
    raster_invalidator_->SetTracksRasterInvalidations(false);

  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled() && paint_controller_)
    paint_controller_->SetTracksRasterInvalidations(should_track);
}

void GraphicsLayer::ResetTrackedRasterInvalidations() {
  if (auto* tracking = GetRasterInvalidationTracking())
    tracking->ClearInvalidations();
}

bool GraphicsLayer::HasTrackedRasterInvalidations() const {
  if (auto* tracking = GetRasterInvalidationTracking())
    return tracking->HasInvalidations();
  return false;
}

RasterInvalidationTracking* GraphicsLayer::GetRasterInvalidationTracking()
    const {
  return raster_invalidator_ ? raster_invalidator_->GetTracking() : nullptr;
}

void GraphicsLayer::TrackRasterInvalidation(const DisplayItemClient& client,
                                            const IntRect& rect,
                                            PaintInvalidationReason reason) {
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    EnsureRasterInvalidator().EnsureTracking();

  // For SPv175, this only tracks invalidations that the WebLayer is fully
  // invalidated directly, e.g. from SetContentsNeedsDisplay(), etc.
  if (auto* tracking = GetRasterInvalidationTracking())
    tracking->AddInvalidation(&client, client.DebugName(), rect, reason);
}

static String PointerAsString(const void* ptr) {
  TextStream ts;
  ts << ptr;
  return ts.Release();
}

class GraphicsLayer::LayersAsJSONArray {
 public:
  LayersAsJSONArray(LayerTreeFlags flags)
      : flags_(flags),
        next_transform_id_(1),
        layers_json_(JSONArray::Create()),
        transforms_json_(JSONArray::Create()) {}

  // Outputs the layer tree rooted at |layer| as a JSON array, in paint order,
  // and the transform tree also as a JSON array.
  std::unique_ptr<JSONObject> operator()(const GraphicsLayer& layer) {
    auto json = JSONObject::Create();
    Walk(layer, 0, FloatPoint());
    json->SetArray("layers", std::move(layers_json_));
    if (transforms_json_->size())
      json->SetArray("transforms", std::move(transforms_json_));
    return json;
  }

  JSONObject* AddTransformJSON(int& transform_id) {
    auto transform_json = JSONObject::Create();
    int parent_transform_id = transform_id;
    transform_id = next_transform_id_++;
    transform_json->SetInteger("id", transform_id);
    if (parent_transform_id)
      transform_json->SetInteger("parent", parent_transform_id);
    auto* result = transform_json.get();
    transforms_json_->PushObject(std::move(transform_json));
    return result;
  }

  static FloatPoint ScrollPosition(const GraphicsLayer& layer) {
    const auto* scrollable_area = layer.GetScrollableArea();
    if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      // The LayoutView layer's scrollable area is on the "Frame Scrolling
      // Layer" ancestor.
      if (layer.DebugName() == "LayoutView #document")
        scrollable_area = layer.Parent()->Parent()->GetScrollableArea();
      else if (layer.DebugName() == "Frame Scrolling Layer")
        scrollable_area = nullptr;
    }
    return scrollable_area ? scrollable_area->ScrollPosition() : FloatPoint();
  }

  void AddLayer(const GraphicsLayer& layer,
                int& transform_id,
                FloatPoint& position) {
    FloatPoint scroll_position = ScrollPosition(layer);
    if (scroll_position != FloatPoint()) {
      // Output scroll position as a transform.
      auto* scroll_translate_json = AddTransformJSON(transform_id);
      scroll_translate_json->SetArray(
          "transform", TransformAsJSONArray(TransformationMatrix().Translate(
                           -scroll_position.X(), -scroll_position.Y())));
      layer.AddFlattenInheritedTransformJSON(*scroll_translate_json);
    }

    if (!layer.transform_.IsIdentity() || layer.rendering_context3d_ ||
        layer.GetCompositingReasons() & kCompositingReason3DTransform) {
      if (position != FloatPoint()) {
        // Output position offset as a transform.
        auto* position_translate_json = AddTransformJSON(transform_id);
        position_translate_json->SetArray(
            "transform", TransformAsJSONArray(TransformationMatrix().Translate(
                             position.X(), position.Y())));
        layer.AddFlattenInheritedTransformJSON(*position_translate_json);
        if (layer.Parent() && !layer.Parent()->should_flatten_transform_) {
          position_translate_json->SetBoolean("flattenInheritedTransform",
                                              false);
        }
        position = FloatPoint();
      }

      if (!layer.transform_.IsIdentity() || layer.rendering_context3d_) {
        auto* transform_json = AddTransformJSON(transform_id);
        layer.AddTransformJSONProperties(*transform_json,
                                         rendering_context_map_);
      }
    }

    auto json =
        layer.LayerAsJSONInternal(flags_, rendering_context_map_, position);
    if (transform_id)
      json->SetInteger("transform", transform_id);
    layers_json_->PushObject(std::move(json));
  }

  void Walk(const GraphicsLayer& layer,
            int parent_transform_id,
            const FloatPoint& parent_position) {
    FloatPoint position = parent_position + layer.position_;
    int transform_id = parent_transform_id;
    AddLayer(layer, transform_id, position);
    for (auto& child : layer.children_)
      Walk(*child, transform_id, position);
  }

 private:
  LayerTreeFlags flags_;
  int next_transform_id_;
  RenderingContextMap rendering_context_map_;
  std::unique_ptr<JSONArray> layers_json_;
  std::unique_ptr<JSONArray> transforms_json_;
};

std::unique_ptr<JSONObject> GraphicsLayer::LayerTreeAsJSON(
    LayerTreeFlags flags) const {
  if (flags & kOutputAsLayerTree) {
    RenderingContextMap rendering_context_map;
    return LayerTreeAsJSONInternal(flags, rendering_context_map);
  }

  return LayersAsJSONArray(flags)(*this);
}

// This is the SPv1 version of ContentLayerClientImpl::LayerAsJSON().
std::unique_ptr<JSONObject> GraphicsLayer::LayerAsJSONInternal(
    LayerTreeFlags flags,
    RenderingContextMap& rendering_context_map,
    const FloatPoint& position) const {
  std::unique_ptr<JSONObject> json = JSONObject::Create();

  if (flags & kLayerTreeIncludesDebugInfo)
    json->SetString("this", PointerAsString(this));

  json->SetString("name", DebugName());

  if (position != FloatPoint())
    json->SetArray("position", PointAsJSONArray(position));

  if (flags & kLayerTreeIncludesDebugInfo &&
      offset_from_layout_object_ != DoubleSize()) {
    json->SetArray("offsetFromLayoutObject",
                   SizeAsJSONArray(offset_from_layout_object_));
  }

  if (size_ != IntSize())
    json->SetArray("bounds", SizeAsJSONArray(size_));

  if (opacity_ != 1)
    json->SetDouble("opacity", opacity_);

  if (blend_mode_ != WebBlendMode::kNormal) {
    json->SetString("blendMode",
                    CompositeOperatorName(kCompositeSourceOver, blend_mode_));
  }

  if (is_root_for_isolated_group_)
    json->SetBoolean("isolate", true);

  if (contents_opaque_)
    json->SetBoolean("contentsOpaque", true);

  if (!draws_content_)
    json->SetBoolean("drawsContent", false);

  if (!contents_visible_)
    json->SetBoolean("contentsVisible", false);

  if (!backface_visibility_)
    json->SetString("backfaceVisibility", "hidden");

  if (flags & kLayerTreeIncludesDebugInfo)
    json->SetString("client", PointerAsString(client_));

  if (background_color_.Alpha()) {
    json->SetString("backgroundColor",
                    background_color_.NameForLayoutTreeAsText());
  }

  if (flags & kOutputAsLayerTree) {
    AddTransformJSONProperties(*json, rendering_context_map);
    if (!should_flatten_transform_)
      json->SetBoolean("shouldFlattenTransform", false);
    if (scrollable_area_ &&
        scrollable_area_->ScrollPosition() != FloatPoint()) {
      json->SetArray("scrollPosition",
                     PointAsJSONArray(scrollable_area_->ScrollPosition()));
    }
  }

  if ((flags & kLayerTreeIncludesPaintInvalidations) &&
      client_->IsTrackingRasterInvalidations() &&
      GetRasterInvalidationTracking())
    GetRasterInvalidationTracking()->AsJSON(json.get());

  if ((flags & kLayerTreeIncludesPaintingPhases) && painting_phase_) {
    std::unique_ptr<JSONArray> painting_phases_json = JSONArray::Create();
    if (painting_phase_ & kGraphicsLayerPaintBackground)
      painting_phases_json->PushString("GraphicsLayerPaintBackground");
    if (painting_phase_ & kGraphicsLayerPaintForeground)
      painting_phases_json->PushString("GraphicsLayerPaintForeground");
    if (painting_phase_ & kGraphicsLayerPaintMask)
      painting_phases_json->PushString("GraphicsLayerPaintMask");
    if (painting_phase_ & kGraphicsLayerPaintChildClippingMask)
      painting_phases_json->PushString("GraphicsLayerPaintChildClippingMask");
    if (painting_phase_ & kGraphicsLayerPaintAncestorClippingMask)
      painting_phases_json->PushString(
          "GraphicsLayerPaintAncestorClippingMask");
    if (painting_phase_ & kGraphicsLayerPaintOverflowContents)
      painting_phases_json->PushString("GraphicsLayerPaintOverflowContents");
    if (painting_phase_ & kGraphicsLayerPaintCompositedScroll)
      painting_phases_json->PushString("GraphicsLayerPaintCompositedScroll");
    if (painting_phase_ & kGraphicsLayerPaintDecoration)
      painting_phases_json->PushString("GraphicsLayerPaintDecoration");
    json->SetArray("paintingPhases", std::move(painting_phases_json));
  }

  if (flags & kLayerTreeIncludesClipAndScrollParents) {
    if (has_scroll_parent_)
      json->SetBoolean("hasScrollParent", true);
    if (has_clip_parent_)
      json->SetBoolean("hasClipParent", true);
  }

  if (flags &
      (kLayerTreeIncludesDebugInfo | kLayerTreeIncludesCompositingReasons)) {
    bool debug = flags & kLayerTreeIncludesDebugInfo;
    std::unique_ptr<JSONArray> compositing_reasons_json = JSONArray::Create();
    for (size_t i = 0; i < kNumberOfCompositingReasons; ++i) {
      if (debug_info_.GetCompositingReasons() &
          kCompositingReasonStringMap[i].reason) {
        compositing_reasons_json->PushString(
            debug ? kCompositingReasonStringMap[i].description
                  : kCompositingReasonStringMap[i].short_name);
      }
    }
    json->SetArray("compositingReasons", std::move(compositing_reasons_json));

    std::unique_ptr<JSONArray> squashing_disallowed_reasons_json =
        JSONArray::Create();
    for (size_t i = 0; i < kNumberOfSquashingDisallowedReasons; ++i) {
      if (debug_info_.GetSquashingDisallowedReasons() &
          kSquashingDisallowedReasonStringMap[i].reason) {
        squashing_disallowed_reasons_json->PushString(
            debug ? kSquashingDisallowedReasonStringMap[i].description
                  : kSquashingDisallowedReasonStringMap[i].short_name);
      }
    }
    json->SetArray("squashingDisallowedReasons",
                   std::move(squashing_disallowed_reasons_json));
  }

  if (mask_layer_) {
    std::unique_ptr<JSONArray> mask_layer_json = JSONArray::Create();
    mask_layer_json->PushObject(mask_layer_->LayerAsJSONInternal(
        flags, rendering_context_map, mask_layer_->position_));
    json->SetArray("maskLayer", std::move(mask_layer_json));
  }

  if (contents_clipping_mask_layer_) {
    std::unique_ptr<JSONArray> contents_clipping_mask_layer_json =
        JSONArray::Create();
    contents_clipping_mask_layer_json->PushObject(
        contents_clipping_mask_layer_->LayerAsJSONInternal(
            flags, rendering_context_map,
            contents_clipping_mask_layer_->position_));
    json->SetArray("contentsClippingMaskLayer",
                   std::move(contents_clipping_mask_layer_json));
  }

  return json;
}

std::unique_ptr<JSONObject> GraphicsLayer::LayerTreeAsJSONInternal(
    LayerTreeFlags flags,
    RenderingContextMap& rendering_context_map) const {
  std::unique_ptr<JSONObject> json =
      LayerAsJSONInternal(flags, rendering_context_map, position_);

  if (children_.size()) {
    std::unique_ptr<JSONArray> children_json = JSONArray::Create();
    for (size_t i = 0; i < children_.size(); i++) {
      children_json->PushObject(
          children_[i]->LayerTreeAsJSONInternal(flags, rendering_context_map));
    }
    json->SetArray("children", std::move(children_json));
  }

  return json;
}

void GraphicsLayer::AddTransformJSONProperties(
    JSONObject& json,
    RenderingContextMap& rendering_context_map) const {
  if (!transform_.IsIdentity())
    json.SetArray("transform", TransformAsJSONArray(transform_));

  if (!transform_.IsIdentityOrTranslation())
    json.SetArray("origin", PointAsJSONArray(transform_origin_));

  AddFlattenInheritedTransformJSON(json);

  if (rendering_context3d_) {
    auto it = rendering_context_map.find(rendering_context3d_);
    int context_id = rendering_context_map.size() + 1;
    if (it == rendering_context_map.end())
      rendering_context_map.Set(rendering_context3d_, context_id);
    else
      context_id = it->value;

    json.SetInteger("renderingContext", context_id);
  }
}

void GraphicsLayer::AddFlattenInheritedTransformJSON(JSONObject& json) const {
  if (Parent() && !Parent()->should_flatten_transform_)
    json.SetBoolean("flattenInheritedTransform", false);
}

String GraphicsLayer::GetLayerTreeAsTextForTesting(LayerTreeFlags flags) const {
  return LayerTreeAsJSON(flags)->ToPrettyJSONString();
}

static const cc::Layer* CcLayerForWebLayer(const WebLayer* web_layer) {
  return web_layer ? web_layer->CcLayer() : nullptr;
}

String GraphicsLayer::DebugName(cc::Layer* layer) const {
  String name;
  if (!client_)
    return name;

  String highlight_debug_name;
  for (size_t i = 0; i < link_highlights_.size(); ++i) {
    if (layer == CcLayerForWebLayer(link_highlights_[i]->Layer())) {
      highlight_debug_name = "LinkHighlight[" + String::Number(i) + "] for " +
                             client_->DebugName(this);
      break;
    }
  }

  if (layer->id() == contents_layer_id_) {
    name = "ContentsLayer for " + client_->DebugName(this);
  } else if (!highlight_debug_name.IsEmpty()) {
    name = highlight_debug_name;
  } else if (layer == CcLayerForWebLayer(layer_->Layer())) {
    name = client_->DebugName(this);
  } else {
    NOTREACHED();
  }
  return name;
}

void GraphicsLayer::SetCompositingReasons(CompositingReasons reasons) {
  debug_info_.SetCompositingReasons(reasons);
}

void GraphicsLayer::SetSquashingDisallowedReasons(
    SquashingDisallowedReasons reasons) {
  debug_info_.SetSquashingDisallowedReasons(reasons);
}

void GraphicsLayer::SetOwnerNodeId(int node_id) {
  debug_info_.SetOwnerNodeId(node_id);
}

void GraphicsLayer::SetPosition(const FloatPoint& point) {
  position_ = point;
  PlatformLayer()->SetPosition(position_);
}

void GraphicsLayer::SetSize(const FloatSize& size) {
  // We are receiving negative sizes here that cause assertions to fail in the
  // compositor. Clamp them to 0 to avoid those assertions.
  // FIXME: This should be an DCHECK instead, as negative sizes should not exist
  // in WebCore.
  FloatSize clamped_size = size;
  if (clamped_size.Width() < 0 || clamped_size.Height() < 0)
    clamped_size = FloatSize();

  if (clamped_size == size_)
    return;

  size_ = clamped_size;

  layer_->Layer()->SetBounds(FlooredIntSize(size_));
  // Note that we don't resize m_contentsLayer. It's up the caller to do that.
}

void GraphicsLayer::SetTransform(const TransformationMatrix& transform) {
  transform_ = transform;
  PlatformLayer()->SetTransform(TransformationMatrix::ToSkMatrix44(transform_));
}

void GraphicsLayer::SetTransformOrigin(const FloatPoint3D& transform_origin) {
  has_transform_origin_ = true;
  transform_origin_ = transform_origin;
  PlatformLayer()->SetTransformOrigin(transform_origin);
}

void GraphicsLayer::SetShouldFlattenTransform(bool should_flatten) {
  if (should_flatten == should_flatten_transform_)
    return;

  should_flatten_transform_ = should_flatten;

  layer_->Layer()->SetShouldFlattenTransform(should_flatten);
}

void GraphicsLayer::SetRenderingContext(int context) {
  if (rendering_context3d_ == context)
    return;

  rendering_context3d_ = context;
  layer_->Layer()->SetRenderingContext(context);

  if (contents_layer_)
    contents_layer_->SetRenderingContext(rendering_context3d_);
}

bool GraphicsLayer::MasksToBounds() const {
  return layer_->Layer()->MasksToBounds();
}

void GraphicsLayer::SetMasksToBounds(bool masks_to_bounds) {
  layer_->Layer()->SetMasksToBounds(masks_to_bounds);
}

void GraphicsLayer::SetDrawsContent(bool draws_content) {
  // NOTE: This early-exit is only correct because we also properly call
  // WebLayer::setDrawsContent() whenever |m_contentsLayer| is set to a new
  // layer in setupContentsLayer().
  if (draws_content == draws_content_)
    return;

  draws_content_ = draws_content;
  UpdateLayerIsDrawable();

  if (!draws_content && paint_controller_)
    paint_controller_.reset();
}

void GraphicsLayer::SetContentsVisible(bool contents_visible) {
  // NOTE: This early-exit is only correct because we also properly call
  // WebLayer::setDrawsContent() whenever |m_contentsLayer| is set to a new
  // layer in setupContentsLayer().
  if (contents_visible == contents_visible_)
    return;

  contents_visible_ = contents_visible;
  UpdateLayerIsDrawable();
}

void GraphicsLayer::SetClipParent(WebLayer* parent) {
  has_clip_parent_ = !!parent;
  layer_->Layer()->SetClipParent(parent);
}

void GraphicsLayer::SetScrollParent(WebLayer* parent) {
  has_scroll_parent_ = !!parent;
  layer_->Layer()->SetScrollParent(parent);
}

void GraphicsLayer::SetBackgroundColor(const Color& color) {
  if (color == background_color_)
    return;

  background_color_ = color;
  layer_->Layer()->SetBackgroundColor(background_color_.Rgb());
}

void GraphicsLayer::SetContentsOpaque(bool opaque) {
  contents_opaque_ = opaque;
  layer_->Layer()->SetOpaque(contents_opaque_);
  ClearContentsLayerIfUnregistered();
  if (contents_layer_)
    contents_layer_->SetOpaque(opaque);
}

void GraphicsLayer::SetMaskLayer(GraphicsLayer* mask_layer) {
  if (mask_layer == mask_layer_)
    return;

  mask_layer_ = mask_layer;
  WebLayer* mask_web_layer =
      mask_layer_ ? mask_layer_->PlatformLayer() : nullptr;
  layer_->Layer()->SetMaskLayer(mask_web_layer);
}

void GraphicsLayer::SetContentsClippingMaskLayer(
    GraphicsLayer* contents_clipping_mask_layer) {
  if (contents_clipping_mask_layer == contents_clipping_mask_layer_)
    return;

  contents_clipping_mask_layer_ = contents_clipping_mask_layer;
  WebLayer* contents_layer = ContentsLayerIfRegistered();
  if (!contents_layer)
    return;
  WebLayer* contents_clipping_mask_web_layer =
      contents_clipping_mask_layer_
          ? contents_clipping_mask_layer_->PlatformLayer()
          : nullptr;
  contents_layer->SetMaskLayer(contents_clipping_mask_web_layer);
  UpdateContentsRect();
}

void GraphicsLayer::SetBackfaceVisibility(bool visible) {
  backface_visibility_ = visible;
  PlatformLayer()->SetDoubleSided(backface_visibility_);
}

void GraphicsLayer::SetOpacity(float opacity) {
  float clamped_opacity = clampTo(opacity, 0.0f, 1.0f);
  opacity_ = clamped_opacity;
  PlatformLayer()->SetOpacity(opacity);
}

void GraphicsLayer::SetBlendMode(WebBlendMode blend_mode) {
  if (blend_mode_ == blend_mode)
    return;
  blend_mode_ = blend_mode;
  PlatformLayer()->SetBlendMode(blend_mode);
}

void GraphicsLayer::SetIsRootForIsolatedGroup(bool isolated) {
  if (is_root_for_isolated_group_ == isolated)
    return;
  is_root_for_isolated_group_ = isolated;
  PlatformLayer()->SetIsRootForIsolatedGroup(isolated);
}

void GraphicsLayer::SetHitTestableWithoutDrawsContent(bool should_hit_test) {
  if (hit_testable_without_draws_content_ == should_hit_test)
    return;
  hit_testable_without_draws_content_ = should_hit_test;
  PlatformLayer()->SetHitTestableWithoutDrawsContent(should_hit_test);
}

void GraphicsLayer::SetContentsNeedsDisplay() {
  if (WebLayer* contents_layer = ContentsLayerIfRegistered()) {
    contents_layer->Invalidate();
    TrackRasterInvalidation(*this, contents_rect_,
                            PaintInvalidationReason::kFull);
  }
}

void GraphicsLayer::SetNeedsDisplay() {
  if (!DrawsContent())
    return;

  // TODO(chrishtr): Stop invalidating the rects once
  // FrameView::paintRecursively() does so.
  layer_->Layer()->Invalidate();
  for (size_t i = 0; i < link_highlights_.size(); ++i)
    link_highlights_[i]->Invalidate();
  GetPaintController().InvalidateAll();

  TrackRasterInvalidation(*this, IntRect(IntPoint(), ExpandedIntSize(size_)),
                          PaintInvalidationReason::kFull);
}

DISABLE_CFI_PERF
void GraphicsLayer::SetNeedsDisplayInRect(
    const IntRect& rect,
    PaintInvalidationReason invalidation_reason,
    const DisplayItemClient& client) {
  DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV175Enabled());
  if (!DrawsContent())
    return;

  if (!ScopedSetNeedsDisplayInRectForTrackingOnly::s_enabled_) {
    SetNeedsDisplayInRectInternal(rect);
    // TODO(wangxianzhu): Need equivalence for SPv175/SPv2.
    if (FirstPaintInvalidationTracking::IsEnabled())
      debug_info_.AppendAnnotatedInvalidateRect(rect, invalidation_reason);
  }

  TrackRasterInvalidation(client, rect, invalidation_reason);
}

void GraphicsLayer::SetNeedsDisplayInRectInternal(const IntRect& rect) {
  DCHECK(DrawsContent());

  layer_->Layer()->InvalidateRect(rect);
  for (auto* link_highlight : link_highlights_)
    link_highlight->Invalidate();
}

void GraphicsLayer::SetContentsRect(const IntRect& rect) {
  if (rect == contents_rect_)
    return;

  contents_rect_ = rect;
  UpdateContentsRect();
}

void GraphicsLayer::SetContentsToImage(
    Image* image,
    Image::ImageDecodingMode decode_mode,
    RespectImageOrientationEnum respect_image_orientation) {
  PaintImage paint_image;
  if (image)
    paint_image = image->PaintImageForCurrentFrame();

  if (paint_image && image->IsBitmapImage() &&
      respect_image_orientation == kRespectImageOrientation) {
    ImageOrientation image_orientation =
        ToBitmapImage(image)->CurrentFrameOrientation();
    paint_image =
        DragImage::ResizeAndOrientImage(paint_image, image_orientation);
  }

  if (paint_image) {
    paint_image =
        PaintImageBuilder::WithCopy(std::move(paint_image))
            .set_decoding_mode(Image::ToPaintImageDecodingMode(decode_mode))
            .TakePaintImage();
    if (!image_layer_) {
      image_layer_ =
          Platform::Current()->CompositorSupport()->CreateImageLayer();
      RegisterContentsLayer(image_layer_->Layer());
    }
    image_layer_->SetImage(std::move(paint_image));
    image_layer_->Layer()->SetOpaque(image->CurrentFrameKnownToBeOpaque());
    UpdateContentsRect();
  } else if (image_layer_) {
    UnregisterContentsLayer(image_layer_->Layer());
    image_layer_.reset();
  }

  SetContentsTo(image_layer_ ? image_layer_->Layer() : nullptr);
}

WebLayer* GraphicsLayer::PlatformLayer() const {
  return layer_->Layer();
}

void GraphicsLayer::SetFilters(CompositorFilterOperations filters) {
  PlatformLayer()->SetFilters(filters.ReleaseCcFilterOperations());
}

void GraphicsLayer::SetBackdropFilters(CompositorFilterOperations filters) {
  PlatformLayer()->SetBackgroundFilters(filters.ReleaseCcFilterOperations());
}

void GraphicsLayer::SetStickyPositionConstraint(
    const WebLayerStickyPositionConstraint& sticky_constraint) {
  layer_->Layer()->SetStickyPositionConstraint(sticky_constraint);
}

void GraphicsLayer::SetFilterQuality(SkFilterQuality filter_quality) {
  if (image_layer_)
    image_layer_->SetNearestNeighbor(filter_quality == kNone_SkFilterQuality);
}

void GraphicsLayer::SetPaintingPhase(GraphicsLayerPaintingPhase phase) {
  if (painting_phase_ == phase)
    return;
  painting_phase_ = phase;
  SetNeedsDisplay();
}

void GraphicsLayer::AddLinkHighlight(LinkHighlight* link_highlight) {
  DCHECK(link_highlight && !link_highlights_.Contains(link_highlight));
  link_highlights_.push_back(link_highlight);
  link_highlight->Layer()->SetLayerClient(this);
  UpdateChildList();
}

void GraphicsLayer::RemoveLinkHighlight(LinkHighlight* link_highlight) {
  link_highlights_.EraseAt(link_highlights_.Find(link_highlight));
  UpdateChildList();
}

void GraphicsLayer::SetScrollableArea(ScrollableArea* scrollable_area) {
  if (scrollable_area_ == scrollable_area)
    return;
  scrollable_area_ = scrollable_area;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
GraphicsLayer::TakeDebugInfo(cc::Layer* layer) {
  std::unique_ptr<base::trace_event::TracedValue> traced_value(
      debug_info_.AsTracedValue());
  traced_value->SetString(
      "layer_name", WTF::StringUTF8Adaptor(DebugName(layer)).AsStringPiece());
  return std::move(traced_value);
}

void GraphicsLayer::didUpdateMainThreadScrollingReasons() {
  debug_info_.SetMainThreadScrollingReasons(
      PlatformLayer()->MainThreadScrollingReasons());
}

void GraphicsLayer::didChangeScrollbarsHidden(bool hidden) {
  if (scrollable_area_)
    scrollable_area_->SetScrollbarsHidden(hidden);
}

PaintController& GraphicsLayer::GetPaintController() const {
  CHECK(DrawsContent());
  if (!paint_controller_) {
    paint_controller_ = PaintController::Create();
    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
      paint_controller_->SetTracksRasterInvalidations(
          client_->IsTrackingRasterInvalidations());
    }
  }
  return *paint_controller_;
}

void GraphicsLayer::SetElementId(const CompositorElementId& id) {
  if (WebLayer* layer = PlatformLayer())
    layer->SetElementId(id);
}

CompositorElementId GraphicsLayer::GetElementId() const {
  if (WebLayer* layer = PlatformLayer())
    return layer->GetElementId();
  return CompositorElementId();
}

sk_sp<PaintRecord> GraphicsLayer::CapturePaintRecord() const {
  if (!DrawsContent())
    return nullptr;

  FloatRect bounds(IntRect(IntPoint(0, 0), ExpandedIntSize(Size())));
  GraphicsContext graphics_context(GetPaintController());
  graphics_context.BeginRecording(bounds);
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled() && !layer_state_) {
    // TODO(wangxianzhu): Remove this condition when all drawable layers have
    // layer_state_ for SPv175.
    for (const auto& display_item :
         GetPaintController().GetPaintArtifact().GetDisplayItemList())
      display_item.Replay(graphics_context);
  } else {
    GetPaintController().GetPaintArtifact().Replay(
        graphics_context,
        layer_state_ ? layer_state_->state : PropertyTreeState::Root());
  }
  return graphics_context.EndRecording();
}

void GraphicsLayer::SetLayerState(PropertyTreeState&& layer_state,
                                  const IntPoint& layer_offset) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());

  if (!layer_state_) {
    layer_state_ = std::make_unique<LayerState>(
        LayerState{std::move(layer_state), layer_offset});
    return;
  }
  layer_state_->state = std::move(layer_state);
  layer_state_->offset = layer_offset;
}

Vector<const PaintChunk*> GraphicsLayer::AllChunkPointers() const {
  const auto& chunks = GetPaintController().GetPaintArtifact().PaintChunks();
  Vector<const PaintChunk*> result;
  result.ReserveInitialCapacity(chunks.size());
  for (const auto& chunk : chunks)
    result.push_back(&chunk);
  return result;
}

void GraphicsLayer::PaintContents(WebDisplayItemList* web_display_item_list,
                                  PaintingControlSetting painting_control) {
  TRACE_EVENT0("blink,benchmark", "GraphicsLayer::PaintContents");

  PaintController& paint_controller = GetPaintController();
  paint_controller.SetDisplayItemConstructionIsDisabled(
      painting_control == kDisplayListConstructionDisabled);
  paint_controller.SetSubsequenceCachingIsDisabled(painting_control ==
                                                   kSubsequenceCachingDisabled);

  if (painting_control == kPartialInvalidation)
    client_->InvalidateTargetElementForTesting();

  // We also disable caching when Painting or Construction are disabled. In both
  // cases we would like to compare assuming the full cost of recording, not the
  // cost of re-using cached content.
  if (painting_control == kDisplayListCachingDisabled ||
      painting_control == kDisplayListPaintingDisabled ||
      painting_control == kDisplayListConstructionDisabled)
    paint_controller.InvalidateAll();

  GraphicsContext::DisabledMode disabled_mode =
      GraphicsContext::kNothingDisabled;
  if (painting_control == kDisplayListPaintingDisabled ||
      painting_control == kDisplayListConstructionDisabled)
    disabled_mode = GraphicsContext::kFullyDisabled;

  // Anything other than PaintDefaultBehavior is for testing. In non-testing
  // scenarios, it is an error to call GraphicsLayer::Paint. Actual painting
  // occurs in LocalFrameView::PaintTree() which calls GraphicsLayer::Paint();
  // this method merely copies the painted output to the WebDisplayItemList.
  if (painting_control != kPaintDefaultBehavior)
    Paint(nullptr, disabled_mode);

  if (layer_state_) {
    DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());
    PaintChunksToCcLayer::ConvertInto(
        AllChunkPointers(), layer_state_->state,
        gfx::Vector2dF(layer_state_->offset.X(), layer_state_->offset.Y()),
        paint_controller.GetPaintArtifact().GetDisplayItemList(),
        *web_display_item_list->GetCcDisplayItemList());
  } else {
    paint_controller.GetPaintArtifact().AppendToWebDisplayItemList(
        OffsetFromLayoutObjectWithSubpixelAccumulation(),
        web_display_item_list);
  }

  paint_controller.SetDisplayItemConstructionIsDisabled(false);
  paint_controller.SetSubsequenceCachingIsDisabled(false);
}

size_t GraphicsLayer::ApproximateUnsharedMemoryUsage() const {
  return GetPaintController().ApproximateUnsharedMemoryUsage();
}

bool ScopedSetNeedsDisplayInRectForTrackingOnly::s_enabled_ = false;

}  // namespace blink

#ifndef NDEBUG
void showGraphicsLayerTree(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(INFO) << "Cannot showGraphicsLayerTree for (nil).";
    return;
  }

  String output = layer->GetLayerTreeAsTextForTesting(0xffffffff);
  LOG(INFO) << output.Utf8().data();
}

void showGraphicsLayers(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(INFO) << "Cannot showGraphicsLayers for (nil).";
    return;
  }

  String output = layer->GetLayerTreeAsTextForTesting(
      0xffffffff & ~blink::kOutputAsLayerTree);
  LOG(INFO) << output.Utf8().data();
}
#endif
