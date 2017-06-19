/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebLayer_h
#define WebLayer_h

#include "cc/layers/layer.h"

#include "WebBlendMode.h"
#include "WebColor.h"
#include "WebCommon.h"
#include "WebFloatPoint.h"
#include "WebFloatPoint3D.h"
#include "WebFloatSize.h"
#include "WebPoint.h"
#include "WebRect.h"
#include "WebSize.h"
#include "WebString.h"
#include "WebTouchInfo.h"
#include "WebVector.h"

class SkMatrix44;

namespace cc {
class Layer;
class LayerClient;
class FilterOperations;
struct ElementId;
}

namespace blink {

class WebLayerScrollClient;
struct WebFloatPoint;
struct WebLayerPositionConstraint;
struct WebLayerStickyPositionConstraint;

class WebLayer {
 public:
  virtual ~WebLayer() {}

  static constexpr int kInvalidLayerId = cc::Layer::INVALID_ID;

  // Returns a positive ID that will be unique across all WebLayers allocated in
  // this process.
  virtual int Id() const = 0;

  // Sets a region of the layer as invalid, i.e. needs to update its content.
  virtual void InvalidateRect(const WebRect&) = 0;

  // Sets the entire layer as invalid, i.e. needs to update its content.
  virtual void Invalidate() = 0;

  // These functions do not take ownership of the WebLayer* parameter.
  virtual void AddChild(WebLayer*) = 0;
  virtual void InsertChild(WebLayer*, size_t index) = 0;
  virtual void ReplaceChild(WebLayer* reference, WebLayer* new_layer) = 0;
  virtual void RemoveFromParent() = 0;
  virtual void RemoveAllChildren() = 0;

  virtual void SetBounds(const WebSize&) = 0;
  virtual WebSize Bounds() const = 0;

  virtual void SetMasksToBounds(bool) = 0;
  virtual bool MasksToBounds() const = 0;

  virtual void SetMaskLayer(WebLayer*) = 0;

  virtual void SetOpacity(float) = 0;
  virtual float Opacity() const = 0;

  virtual void SetBlendMode(WebBlendMode) = 0;
  virtual WebBlendMode BlendMode() const = 0;

  virtual void SetIsRootForIsolatedGroup(bool) = 0;
  virtual bool IsRootForIsolatedGroup() = 0;

  virtual void SetShouldHitTest(bool) = 0;
  virtual bool ShouldHitTest() = 0;

  virtual void SetOpaque(bool) = 0;
  virtual bool Opaque() const = 0;

  virtual void SetPosition(const WebFloatPoint&) = 0;
  virtual WebFloatPoint GetPosition() const = 0;

  virtual void SetTransform(const SkMatrix44&) = 0;
  virtual SkMatrix44 Transform() const = 0;

  virtual void SetTransformOrigin(const WebFloatPoint3D&) {}
  virtual WebFloatPoint3D TransformOrigin() const { return WebFloatPoint3D(); }

  // Sets whether the layer draws its content when compositing.
  virtual void SetDrawsContent(bool) = 0;
  virtual bool DrawsContent() const = 0;

  // Set to true if the backside of this layer's contents should be visible
  // when composited. Defaults to false.
  virtual void SetDoubleSided(bool) = 0;

  // Sets whether the layer's transform should be flattened.
  virtual void SetShouldFlattenTransform(bool) = 0;

  // Sets the id of the layer's 3d rendering context. Layers in the same 3d
  // rendering context id are sorted with one another according to their 3d
  // position rather than their tree order.
  virtual void SetRenderingContext(int id) = 0;

  // Mark that this layer should use its parent's transform and double-sided
  // properties in determining this layer's backface visibility instead of
  // using its own properties. If this property is set, this layer must
  // have a parent, and the parent may not have this property set.
  // Note: This API is to work around issues with visibility the handling of
  // WebKit layers that have a contents layer (canvas, plugin, WebGL, video,
  // etc).
  virtual void SetUseParentBackfaceVisibility(bool) = 0;

  virtual void SetBackgroundColor(WebColor) = 0;
  virtual WebColor BackgroundColor() const = 0;

  // Clear the filters in use by passing in a newly instantiated
  // FilterOperations object.
  virtual void SetFilters(const cc::FilterOperations&) = 0;

  // The position of the original primitive inside the total bounds.
  virtual void SetFiltersOrigin(const WebFloatPoint&) = 0;

  // Clear the background filters in use by passing in a newly instantiated
  // FilterOperations object.
  // TODO(loyso): This should use CompositorFilterOperation. crbug.com/584551
  virtual void SetBackgroundFilters(const cc::FilterOperations&) = 0;

  // Returns true if this layer has any active animations - useful for tests.
  virtual bool HasTickingAnimationForTesting() = 0;

  // If a scroll parent is set, this layer will inherit its parent's scroll
  // delta and offset even though it will not be a descendant of the scroll
  // in the layer hierarchy.
  virtual void SetScrollParent(WebLayer*) = 0;

  // A layer will not respect any clips established by layers between it and
  // its nearest clipping ancestor. Note, the clip parent must be an ancestor.
  // This is not a requirement of the scroll parent.
  virtual void SetClipParent(WebLayer*) = 0;

  // Scrolling
  virtual void SetScrollPosition(WebFloatPoint) = 0;
  virtual WebFloatPoint ScrollPosition() const = 0;

  // To set a WebLayer as scrollable we must specify the corresponding clip
  // layer.
  virtual void SetScrollClipLayer(WebLayer*) = 0;
  virtual bool Scrollable() const = 0;
  virtual void SetUserScrollable(bool horizontal, bool vertical) = 0;
  virtual bool UserScrollableHorizontal() const = 0;
  virtual bool UserScrollableVertical() const = 0;

  // Indicates that this layer will always scroll on the main thread for the
  // provided reason.
  virtual void AddMainThreadScrollingReasons(uint32_t) = 0;
  virtual void ClearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear) = 0;
  virtual uint32_t MainThreadScrollingReasons() = 0;
  virtual bool ShouldScrollOnMainThread() const = 0;

  virtual void SetNonFastScrollableRegion(const WebVector<WebRect>&) = 0;
  virtual WebVector<WebRect> NonFastScrollableRegion() const = 0;

  virtual void SetTouchEventHandlerRegion(const WebVector<WebTouchInfo>&) = 0;
  virtual WebVector<WebRect> TouchEventHandlerRegion() const = 0;

  virtual void SetIsContainerForFixedPositionLayers(bool) = 0;
  virtual bool IsContainerForFixedPositionLayers() const = 0;

  // This function sets layer position constraint. The constraint will be used
  // to adjust layer position during threaded scrolling.
  virtual void SetPositionConstraint(const WebLayerPositionConstraint&) = 0;
  virtual WebLayerPositionConstraint PositionConstraint() const = 0;

  // Sets the sticky position constraint. This will be used to adjust sticky
  // position objects during threaded scrolling.
  virtual void SetStickyPositionConstraint(
      const WebLayerStickyPositionConstraint&) = 0;
  virtual WebLayerStickyPositionConstraint StickyPositionConstraint() const = 0;

  // The scroll client is notified when the scroll position of the WebLayer
  // changes. Only a single scroll client can be set for a WebLayer at a time.
  // The WebLayer does not take ownership of the scroll client, and it is the
  // responsibility of the client to reset the layer's scroll client before
  // deleting the scroll client.
  virtual void SetScrollClient(WebLayerScrollClient*) = 0;

  // Sets the cc-side layer client.
  virtual void SetLayerClient(cc::LayerClient*) = 0;

  // Gets the underlying cc layer.
  virtual const cc::Layer* CcLayer() const = 0;
  virtual cc::Layer* CcLayer() = 0;

  virtual void SetElementId(const cc::ElementId&) = 0;
  virtual cc::ElementId GetElementId() const = 0;

  virtual void SetCompositorMutableProperties(uint32_t) = 0;
  virtual uint32_t CompositorMutableProperties() const = 0;

  virtual void SetHasWillChangeTransformHint(bool) = 0;

  // Called on the scroll layer to trigger showing the overlay scrollbars.
  virtual void ShowScrollbars() = 0;
};

}  // namespace blink

#endif  // WebLayer_h
