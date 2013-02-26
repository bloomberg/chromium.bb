// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_layer_impl.h"

#include "base/string_util.h"
#include "cc/animation.h"
#include "cc/layer.h"
#include "cc/region.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "webkit/compositor_bindings/web_animation_impl.h"
#include "webkit/compositor_bindings/web_transformation_matrix_util.h"

using cc::Animation;
using cc::Layer;
using webkit::WebTransformationMatrixUtil;

namespace WebKit {

WebLayerImpl::WebLayerImpl() : layer_(Layer::create()) {}

WebLayerImpl::WebLayerImpl(scoped_refptr<Layer> layer) : layer_(layer) {}

WebLayerImpl::~WebLayerImpl() {
  layer_->clearRenderSurface();
  layer_->setLayerAnimationDelegate(0);
}

int WebLayerImpl::id() const { return layer_->id(); }

void WebLayerImpl::invalidateRect(const WebFloatRect& rect) {
  layer_->setNeedsDisplayRect(rect);
}

void WebLayerImpl::invalidate() { layer_->setNeedsDisplay(); }

void WebLayerImpl::addChild(WebLayer* child) {
  layer_->addChild(static_cast<WebLayerImpl*>(child)->layer());
}

void WebLayerImpl::insertChild(WebLayer* child, size_t index) {
  layer_->insertChild(static_cast<WebLayerImpl*>(child)->layer(), index);
}

void WebLayerImpl::replaceChild(WebLayer* reference, WebLayer* new_layer) {
  layer_->replaceChild(static_cast<WebLayerImpl*>(reference)->layer(),
                       static_cast<WebLayerImpl*>(new_layer)->layer());
}

void WebLayerImpl::removeFromParent() { layer_->removeFromParent(); }

void WebLayerImpl::removeAllChildren() { layer_->removeAllChildren(); }

void WebLayerImpl::setAnchorPoint(const WebFloatPoint& anchor_point) {
  layer_->setAnchorPoint(anchor_point);
}

WebFloatPoint WebLayerImpl::anchorPoint() const {
  return layer_->anchorPoint();
}

void WebLayerImpl::setAnchorPointZ(float anchor_point_z) {
  layer_->setAnchorPointZ(anchor_point_z);
}

float WebLayerImpl::anchorPointZ() const { return layer_->anchorPointZ(); }

void WebLayerImpl::setBounds(const WebSize& size) { layer_->setBounds(size); }

WebSize WebLayerImpl::bounds() const { return layer_->bounds(); }

void WebLayerImpl::setMasksToBounds(bool masks_to_bounds) {
  layer_->setMasksToBounds(masks_to_bounds);
}

bool WebLayerImpl::masksToBounds() const { return layer_->masksToBounds(); }

void WebLayerImpl::setMaskLayer(WebLayer* maskLayer) {
  layer_->setMaskLayer(
      maskLayer ? static_cast<WebLayerImpl*>(maskLayer)->layer() : 0);
}

void WebLayerImpl::setReplicaLayer(WebLayer* replica_layer) {
  layer_->setReplicaLayer(
      replica_layer ? static_cast<WebLayerImpl*>(replica_layer)->layer() : 0);
}

void WebLayerImpl::setOpacity(float opacity) { layer_->setOpacity(opacity); }

float WebLayerImpl::opacity() const { return layer_->opacity(); }

void WebLayerImpl::setOpaque(bool opaque) { layer_->setContentsOpaque(opaque); }

bool WebLayerImpl::opaque() const { return layer_->contentsOpaque(); }

void WebLayerImpl::setPosition(const WebFloatPoint& position) {
  layer_->setPosition(position);
}

WebFloatPoint WebLayerImpl::position() const { return layer_->position(); }

void WebLayerImpl::setSublayerTransform(const SkMatrix44& matrix) {
  gfx::Transform sub_layer_transform;
  sub_layer_transform.matrix() = matrix;
  layer_->setSublayerTransform(sub_layer_transform);
}

void WebLayerImpl::setSublayerTransform(const WebTransformationMatrix& matrix) {
  layer_->setSublayerTransform(
      WebTransformationMatrixUtil::ToTransform(matrix));
}

SkMatrix44 WebLayerImpl::sublayerTransform() const {
  return layer_->sublayerTransform().matrix();
}

void WebLayerImpl::setTransform(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  layer_->setTransform(transform);
}

void WebLayerImpl::setTransform(const WebTransformationMatrix& matrix) {
  layer_->setTransform(WebTransformationMatrixUtil::ToTransform(matrix));
}

SkMatrix44 WebLayerImpl::transform() const {
  return layer_->transform().matrix();
}

void WebLayerImpl::setDrawsContent(bool draws_content) {
  layer_->setIsDrawable(draws_content);
}

bool WebLayerImpl::drawsContent() const { return layer_->drawsContent(); }

void WebLayerImpl::setPreserves3D(bool preserve3D) {
  layer_->setPreserves3D(preserve3D);
}

void WebLayerImpl::setUseParentBackfaceVisibility(
    bool use_parent_backface_visibility) {
  layer_->setUseParentBackfaceVisibility(use_parent_backface_visibility);
}

void WebLayerImpl::setBackgroundColor(WebColor color) {
  layer_->setBackgroundColor(color);
}

WebColor WebLayerImpl::backgroundColor() const {
  return layer_->backgroundColor();
}

void WebLayerImpl::setFilters(const WebFilterOperations& filters) {
  layer_->setFilters(filters);
}

void WebLayerImpl::setBackgroundFilters(const WebFilterOperations& filters) {
  layer_->setBackgroundFilters(filters);
}

void WebLayerImpl::setFilter(SkImageFilter* filter) {
  SkSafeRef(filter);  // Claim a reference for the compositor.
  layer_->setFilter(skia::AdoptRef(filter));
}

void WebLayerImpl::setDebugName(WebString name) {
  layer_->setDebugName(UTF16ToASCII(string16(name.data(), name.length())));
}

void WebLayerImpl::setAnimationDelegate(WebAnimationDelegate* delegate) {
  layer_->setLayerAnimationDelegate(delegate);
}

bool WebLayerImpl::addAnimation(WebAnimation* animation) {
  return layer_->addAnimation(static_cast<WebAnimationImpl*>(animation)
                                  ->cloneToAnimation());
}

void WebLayerImpl::removeAnimation(int animation_id) {
  layer_->removeAnimation(animation_id);
}

void WebLayerImpl::removeAnimation(
    int animation_id,
    WebAnimation::TargetProperty target_property) {
  layer_->layerAnimationController()
      ->removeAnimation(
          animation_id,
          static_cast<Animation::TargetProperty>(target_property));
}

void WebLayerImpl::pauseAnimation(int animation_id, double time_offset) {
  layer_->pauseAnimation(animation_id, time_offset);
}

void WebLayerImpl::suspendAnimations(double monotonic_time) {
  layer_->suspendAnimations(monotonic_time);
}

void WebLayerImpl::resumeAnimations(double monotonic_time) {
  layer_->resumeAnimations(monotonic_time);
}

bool WebLayerImpl::hasActiveAnimation() { return layer_->hasActiveAnimation(); }

void WebLayerImpl::transferAnimationsTo(WebLayer* other) {
  DCHECK(other);
  static_cast<WebLayerImpl*>(other)->layer_
      ->setLayerAnimationController(layer_->releaseLayerAnimationController());
}

void WebLayerImpl::setForceRenderSurface(bool force_render_surface) {
  layer_->setForceRenderSurface(force_render_surface);
}

void WebLayerImpl::setScrollPosition(WebPoint position) {
  layer_->setScrollOffset(gfx::Point(position).OffsetFromOrigin());
}

WebPoint WebLayerImpl::scrollPosition() const {
  return gfx::PointAtOffsetFromOrigin(layer_->scrollOffset());
}

void WebLayerImpl::setMaxScrollPosition(WebSize max_scroll_position) {
  layer_->setMaxScrollOffset(max_scroll_position);
}

WebSize WebLayerImpl::maxScrollPosition() const {
  return layer_->maxScrollOffset();
}

void WebLayerImpl::setScrollable(bool scrollable) {
  layer_->setScrollable(scrollable);
}

bool WebLayerImpl::scrollable() const { return layer_->scrollable(); }

void WebLayerImpl::setHaveWheelEventHandlers(bool have_wheel_event_handlers) {
  layer_->setHaveWheelEventHandlers(have_wheel_event_handlers);
}

bool WebLayerImpl::haveWheelEventHandlers() const {
  return layer_->haveWheelEventHandlers();
}

void WebLayerImpl::setShouldScrollOnMainThread(
    bool should_scroll_on_main_thread) {
  layer_->setShouldScrollOnMainThread(should_scroll_on_main_thread);
}

bool WebLayerImpl::shouldScrollOnMainThread() const {
  return layer_->shouldScrollOnMainThread();
}

void WebLayerImpl::setNonFastScrollableRegion(const WebVector<WebRect>& rects) {
  cc::Region region;
  for (size_t i = 0; i < rects.size(); ++i)
    region.Union(rects[i]);
  layer_->setNonFastScrollableRegion(region);
}

WebVector<WebRect> WebLayerImpl::nonFastScrollableRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(layer_->nonFastScrollableRegion());
       region_rects.has_rect();
       region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(layer_->nonFastScrollableRegion());
       region_rects.has_rect();
       region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

void WebLayerImpl::setTouchEventHandlerRegion(const WebVector<WebRect>& rects) {
  cc::Region region;
  for (size_t i = 0; i < rects.size(); ++i)
    region.Union(rects[i]);
  layer_->setTouchEventHandlerRegion(region);
}

WebVector<WebRect> WebLayerImpl::touchEventHandlerRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(layer_->touchEventHandlerRegion());
       region_rects.has_rect();
       region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(layer_->touchEventHandlerRegion());
       region_rects.has_rect();
       region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

void WebLayerImpl::setIsContainerForFixedPositionLayers(bool enable) {
  layer_->setIsContainerForFixedPositionLayers(enable);
}

bool WebLayerImpl::isContainerForFixedPositionLayers() const {
  return layer_->isContainerForFixedPositionLayers();
}

void WebLayerImpl::setFixedToContainerLayer(bool enable) {
  layer_->setFixedToContainerLayer(enable);
}

bool WebLayerImpl::fixedToContainerLayer() const {
  return layer_->fixedToContainerLayer();
}

void WebLayerImpl::setScrollClient(WebLayerScrollClient* scroll_client) {
  layer_->setLayerScrollClient(scroll_client);
}

Layer* WebLayerImpl::layer() const { return layer_.get(); }

}  // namespace WebKit
