// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_layer.h"

#include "cc/layers/layer.h"

namespace blink {

WebLayer::WebLayer(cc::Layer* layer) : layer_(layer) {}

WebLayer::~WebLayer() = default;

int WebLayer::Id() const {
  return layer_->id();
}

DISABLE_CFI_PERF
void WebLayer::InvalidateRect(const gfx::Rect& rect) {
  layer_->SetNeedsDisplayRect(rect);
}

void WebLayer::Invalidate() {
  layer_->SetNeedsDisplay();
}

void WebLayer::AddChild(WebLayer* child) {
  layer_->AddChild(child->layer_);
}

void WebLayer::InsertChild(WebLayer* child, size_t index) {
  layer_->InsertChild(child->layer_, index);
}

void WebLayer::ReplaceChild(WebLayer* reference, WebLayer* new_layer) {
  layer_->ReplaceChild(reference->layer_, new_layer->layer_);
}

void WebLayer::RemoveFromParent() {
  layer_->RemoveFromParent();
}

void WebLayer::RemoveAllChildren() {
  layer_->RemoveAllChildren();
}

void WebLayer::SetBounds(const gfx::Size& size) {
  layer_->SetBounds(size);
}

const gfx::Size& WebLayer::Bounds() const {
  return layer_->bounds();
}

void WebLayer::SetMasksToBounds(bool masks_to_bounds) {
  layer_->SetMasksToBounds(masks_to_bounds);
}

bool WebLayer::MasksToBounds() const {
  return layer_->masks_to_bounds();
}

void WebLayer::SetMaskLayer(WebLayer* mask_layer) {
  layer_->SetMaskLayer(mask_layer ? mask_layer->layer_ : nullptr);
}

void WebLayer::SetOpacity(float opacity) {
  layer_->SetOpacity(opacity);
}

float WebLayer::Opacity() const {
  return layer_->opacity();
}

void WebLayer::SetBlendMode(SkBlendMode blend_mode) {
  layer_->SetBlendMode(blend_mode);
}

SkBlendMode WebLayer::BlendMode() const {
  return layer_->blend_mode();
}

void WebLayer::SetIsRootForIsolatedGroup(bool isolate) {
  layer_->SetIsRootForIsolatedGroup(isolate);
}

bool WebLayer::IsRootForIsolatedGroup() {
  return layer_->is_root_for_isolated_group();
}

void WebLayer::SetHitTestableWithoutDrawsContent(bool should_hit_test) {
  layer_->SetHitTestableWithoutDrawsContent(should_hit_test);
}

void WebLayer::SetOpaque(bool opaque) {
  layer_->SetContentsOpaque(opaque);
}

bool WebLayer::Opaque() const {
  return layer_->contents_opaque();
}

void WebLayer::SetPosition(const gfx::PointF& position) {
  layer_->SetPosition(position);
}

const gfx::PointF& WebLayer::GetPosition() const {
  return layer_->position();
}

void WebLayer::SetTransform(const gfx::Transform& transform) {
  layer_->SetTransform(transform);
}

void WebLayer::SetTransformOrigin(const gfx::Point3F& point) {
  layer_->SetTransformOrigin(point);
}

const gfx::Point3F& WebLayer::TransformOrigin() const {
  return layer_->transform_origin();
}

const gfx::Transform& WebLayer::Transform() const {
  return layer_->transform();
}

void WebLayer::SetDrawsContent(bool draws_content) {
  layer_->SetIsDrawable(draws_content);
}

bool WebLayer::DrawsContent() const {
  return layer_->DrawsContent();
}

void WebLayer::SetDoubleSided(bool double_sided) {
  layer_->SetDoubleSided(double_sided);
}

void WebLayer::SetShouldFlattenTransform(bool flatten) {
  layer_->SetShouldFlattenTransform(flatten);
}

void WebLayer::SetRenderingContext(int context) {
  layer_->Set3dSortingContextId(context);
}

void WebLayer::SetUseParentBackfaceVisibility(
    bool use_parent_backface_visibility) {
  layer_->SetUseParentBackfaceVisibility(use_parent_backface_visibility);
}

void WebLayer::SetBackgroundColor(SkColor color) {
  layer_->SetBackgroundColor(color);
}

SkColor WebLayer::BackgroundColor() const {
  return layer_->background_color();
}

void WebLayer::SetFilters(const cc::FilterOperations& filters) {
  layer_->SetFilters(filters);
}

void WebLayer::SetFiltersOrigin(const gfx::PointF& origin) {
  layer_->SetFiltersOrigin(origin);
}

void WebLayer::SetBackgroundFilters(const cc::FilterOperations& filters) {
  layer_->SetBackgroundFilters(filters);
}

bool WebLayer::HasTickingAnimationForTesting() {
  return layer_->HasTickingAnimationForTesting();
}

void WebLayer::SetScrollable(const gfx::Size& size) {
  layer_->SetScrollable(size);
}

void WebLayer::SetScrollPosition(const gfx::ScrollOffset& position) {
  layer_->SetScrollOffset(position);
}

const gfx::ScrollOffset& WebLayer::ScrollPosition() const {
  return layer_->scroll_offset();
}

bool WebLayer::Scrollable() const {
  return layer_->scrollable();
}

const gfx::Size& WebLayer::ScrollContainerBoundsForTesting() const {
  return layer_->scroll_container_bounds();
}

void WebLayer::SetUserScrollable(bool horizontal, bool vertical) {
  layer_->SetUserScrollable(horizontal, vertical);
}

bool WebLayer::UserScrollableHorizontal() const {
  return layer_->user_scrollable_horizontal();
}

bool WebLayer::UserScrollableVertical() const {
  return layer_->user_scrollable_vertical();
}

void WebLayer::AddMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons) {
  layer_->AddMainThreadScrollingReasons(main_thread_scrolling_reasons);
}

void WebLayer::ClearMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons_to_clear) {
  layer_->ClearMainThreadScrollingReasons(
      main_thread_scrolling_reasons_to_clear);
}

uint32_t WebLayer::MainThreadScrollingReasons() {
  return layer_->main_thread_scrolling_reasons();
}

bool WebLayer::ShouldScrollOnMainThread() const {
  return layer_->should_scroll_on_main_thread();
}

void WebLayer::SetNonFastScrollableRegion(const cc::Region& region) {
  layer_->SetNonFastScrollableRegion(region);
}

const cc::Region& WebLayer::NonFastScrollableRegion() const {
  return layer_->non_fast_scrollable_region();
}

void WebLayer::SetTouchEventHandlerRegion(const cc::TouchActionRegion& region) {
  layer_->SetTouchActionRegion(region);
}

const cc::TouchActionRegion& WebLayer::TouchEventHandlerRegion() const {
  return layer_->touch_action_region();
}

const cc::Region& WebLayer::TouchEventHandlerRegionForTouchActionForTesting(
    cc::TouchAction touch_action) const {
  return layer_->touch_action_region().GetRegionForTouchAction(touch_action);
}

void WebLayer::SetIsContainerForFixedPositionLayers(bool enable) {
  layer_->SetIsContainerForFixedPositionLayers(enable);
}

bool WebLayer::IsContainerForFixedPositionLayers() const {
  return layer_->IsContainerForFixedPositionLayers();
}

void WebLayer::SetIsResizedByBrowserControls(bool enable) {
  layer_->SetIsResizedByBrowserControls(enable);
}

void WebLayer::SetPositionConstraint(
    const cc::LayerPositionConstraint& constraint) {
  layer_->SetPositionConstraint(constraint);
}

const cc::LayerPositionConstraint& WebLayer::PositionConstraint() const {
  return layer_->position_constraint();
}

void WebLayer::SetStickyPositionConstraint(
    const cc::LayerStickyPositionConstraint& constraint) {
  layer_->SetStickyPositionConstraint(constraint);
}

const cc::LayerStickyPositionConstraint& WebLayer::StickyPositionConstraint()
    const {
  return layer_->sticky_position_constraint();
}

void WebLayer::SetScrollCallback(
    base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                 const cc::ElementId&)> callback) {
  layer_->set_did_scroll_callback(std::move(callback));
}

void WebLayer::SetScrollOffsetFromImplSideForTesting(
    const gfx::ScrollOffset& offset) {
  layer_->SetScrollOffsetFromImplSide(offset);
}

void WebLayer::SetLayerClient(base::WeakPtr<cc::LayerClient> client) {
  layer_->SetLayerClient(std::move(client));
}

const cc::Layer* WebLayer::CcLayer() const {
  return layer_;
}

cc::Layer* WebLayer::CcLayer() {
  return layer_;
}

void WebLayer::SetElementId(const cc::ElementId& id) {
  layer_->SetElementId(id);
}

cc::ElementId WebLayer::GetElementId() const {
  return layer_->element_id();
}

void WebLayer::SetScrollParent(WebLayer* parent) {
  layer_->SetScrollParent(parent ? parent->layer_ : nullptr);
}

void WebLayer::SetClipParent(WebLayer* parent) {
  layer_->SetClipParent(parent ? parent->layer_ : nullptr);
}

void WebLayer::SetHasWillChangeTransformHint(bool has_will_change) {
  layer_->SetHasWillChangeTransformHint(has_will_change);
}

void WebLayer::ShowScrollbars() {
  layer_->ShowScrollbars();
}

void WebLayer::SetOverscrollBehavior(const cc::OverscrollBehavior& behavior) {
  layer_->SetOverscrollBehavior(behavior);
}

void WebLayer::SetSnapContainerData(
    base::Optional<cc::SnapContainerData> data) {
  layer_->SetSnapContainerData(std::move(data));
}

}  // namespace blink
