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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYER_H_

#include "base/callback.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/layers/layer.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_touch_info.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
struct ElementId;
class FilterOperations;
class Layer;
class LayerClient;
class LayerPositionConstraint;
struct LayerStickyPositionConstraint;
class Region;
class TouchActionRegion;
}

namespace gfx {
class PointF;
class Point3F;
class Rect;
class ScrollOffset;
class Transform;
}

namespace blink {

class BLINK_PLATFORM_EXPORT WebLayer {
 public:
  WebLayer(cc::Layer*);
  ~WebLayer();

  // Returns a positive ID that will be unique across all WebLayers allocated in
  // this process.
  int Id() const;

  // Sets a area of the layer as invalid, i.e. needs to update its content.
  void InvalidateRect(const gfx::Rect&);

  // Sets the entire layer as invalid, i.e. needs to update its content.
  void Invalidate();

  // These functions do not take ownership of the WebLayer* parameter.
  void AddChild(WebLayer*);
  void InsertChild(WebLayer*, size_t index);
  void ReplaceChild(WebLayer* reference, WebLayer* new_layer);
  void RemoveFromParent();
  void RemoveAllChildren();

  void SetBounds(const gfx::Size&);
  const gfx::Size& Bounds() const;

  void SetMasksToBounds(bool);
  bool MasksToBounds() const;

  void SetMaskLayer(WebLayer*);

  void SetOpacity(float);
  float Opacity() const;

  void SetBlendMode(SkBlendMode);
  SkBlendMode BlendMode() const;

  void SetIsRootForIsolatedGroup(bool);
  bool IsRootForIsolatedGroup();

  void SetHitTestableWithoutDrawsContent(bool);

  void SetOpaque(bool);
  bool Opaque() const;

  void SetPosition(const gfx::PointF&);
  const gfx::PointF& GetPosition() const;

  void SetTransform(const gfx::Transform&);
  const gfx::Transform& Transform() const;

  void SetTransformOrigin(const gfx::Point3F&);
  const gfx::Point3F& TransformOrigin() const;

  // Sets whether the layer draws its content when compositing.
  void SetDrawsContent(bool);
  bool DrawsContent() const;

  // Set to true if the backside of this layer's contents should be visible
  // when composited. Defaults to false.
  void SetDoubleSided(bool);

  // Sets whether the layer's transform should be flattened.
  void SetShouldFlattenTransform(bool);

  // Sets the id of the layer's 3d rendering context. Layers in the same 3d
  // rendering context id are sorted with one another according to their 3d
  // position rather than their tree order.
  void SetRenderingContext(int id);

  // Mark that this layer should use its parent's transform and double-sided
  // properties in determining this layer's backface visibility instead of
  // using its own properties. If this property is set, this layer must
  // have a parent, and the parent may not have this property set.
  // Note: This API is to work around issues with visibility the handling of
  // WebKit layers that have a contents layer (canvas, plugin, WebGL, video,
  // etc).
  void SetUseParentBackfaceVisibility(bool);

  void SetBackgroundColor(SkColor);
  SkColor BackgroundColor() const;

  // Clear the filters in use by passing in a newly instantiated
  // FilterOperations object.
  void SetFilters(const cc::FilterOperations&);

  // The position of the original primitive inside the total bounds.
  void SetFiltersOrigin(const gfx::PointF&);

  // Clear the background filters in use by passing in a newly instantiated
  // FilterOperations object.
  // TODO(loyso): This should use CompositorFilterOperation. crbug.com/584551
  void SetBackgroundFilters(const cc::FilterOperations&);

  // Returns true if this layer has any active animations - useful for tests.
  bool HasTickingAnimationForTesting();

  // If a scroll parent is set, this layer will inherit its parent's scroll
  // delta and offset even though it will not be a descendant of the scroll
  // in the layer hierarchy.
  void SetScrollParent(WebLayer*);

  // A layer will not respect any clips established by layers between it and
  // its nearest clipping ancestor. Note, the clip parent must be an ancestor.
  // This is not a requirement of the scroll parent.
  void SetClipParent(WebLayer*);

  // Scrolling
  void SetScrollPosition(const gfx::ScrollOffset&);
  const gfx::ScrollOffset& ScrollPosition() const;

  // To set a WebLayer as scrollable we must specify the scrolling container
  // bounds.
  void SetScrollable(const gfx::Size& scroll_container_bounds);
  bool Scrollable() const;
  const gfx::Size& ScrollContainerBoundsForTesting() const;

  void SetUserScrollable(bool horizontal, bool vertical);
  bool UserScrollableHorizontal() const;
  bool UserScrollableVertical() const;

  // Indicates that this layer will always scroll on the main thread for the
  // provided reason.
  void AddMainThreadScrollingReasons(uint32_t);
  void ClearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear);
  uint32_t MainThreadScrollingReasons();
  bool ShouldScrollOnMainThread() const;

  void SetNonFastScrollableRegion(const cc::Region&);
  const cc::Region& NonFastScrollableRegion() const;

  void SetTouchEventHandlerRegion(const cc::TouchActionRegion& region);
  const cc::TouchActionRegion& TouchEventHandlerRegion() const;
  const cc::Region& TouchEventHandlerRegionForTouchActionForTesting(
      WebTouchAction) const;

  void SetIsContainerForFixedPositionLayers(bool);
  bool IsContainerForFixedPositionLayers() const;

  void SetIsResizedByBrowserControls(bool);

  // This function sets layer position constraint. The constraint will be used
  // to adjust layer position during threaded scrolling.
  void SetPositionConstraint(const cc::LayerPositionConstraint&);
  const cc::LayerPositionConstraint& PositionConstraint() const;

  // Sets the sticky position constraint. This will be used to adjust sticky
  // position objects during threaded scrolling.
  void SetStickyPositionConstraint(const cc::LayerStickyPositionConstraint&);
  const cc::LayerStickyPositionConstraint& StickyPositionConstraint() const;

  // The scroll callback is notified when the scroll position of the WebLayer
  // changes. Only a single scroll callback can be set for a WebLayer at a time.
  void SetScrollCallback(
      base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                   const cc::ElementId&)> callback);

  // Sets a synthetic impl-side scroll offset which will end up reporting this
  // call back to blink via the scroll callback set by SetScrollCallback().
  void SetScrollOffsetFromImplSideForTesting(const gfx::ScrollOffset&);

  // The scroll-boundary-behavior allows developers to specify whether the
  // scroll should be propagated to its ancestors at the beginning of the
  // scroll, and whether the overscroll should cause UI affordance such as
  // glow/bounce etc.
  void SetOverscrollBehavior(const cc::OverscrollBehavior&);

  // SnapContainerData contains the necessary information a layer needs to
  // perform snapping. The CSS scroll snap could enforce the scroll positions
  // that a scroll container's scroll port may end at after a scrolling
  // operation has completed.
  void SetSnapContainerData(base::Optional<cc::SnapContainerData>);

  // Sets the cc-side layer client.
  void SetLayerClient(base::WeakPtr<cc::LayerClient>);

  // Gets the underlying cc layer.
  const cc::Layer* CcLayer() const;
  cc::Layer* CcLayer();

  void SetElementId(const cc::ElementId&);
  cc::ElementId GetElementId() const;

  void SetHasWillChangeTransformHint(bool);

  // Called on the scroll layer to trigger showing the overlay scrollbars.
  void ShowScrollbars();

 private:
  cc::Layer* layer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYER_H_
