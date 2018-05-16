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

#include "cc/layers/layer.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebLayer {
 public:
  WebLayer(cc::Layer*);
  ~WebLayer();

  // Returns a positive ID that will be unique across all WebLayers allocated in
  // this process.
  int id() const;

  // Marks the entire layer's bounds as being changed, which will cause a commit
  // and the compositor to submit a new frame with a damage rect that includes
  // the entire layer.
  void SetNeedsDisplay();

  // These functions do not take ownership of the WebLayer* parameter.
  void AddChild(WebLayer*);
  void InsertChild(WebLayer*, size_t index);
  void ReplaceChild(WebLayer* reference, WebLayer* new_layer);
  void RemoveFromParent();
  void RemoveAllChildren();

  void SetBounds(const gfx::Size&);
  const gfx::Size& bounds() const;

  void SetMasksToBounds(bool);
  bool masks_to_bounds() const;

  void SetMaskLayer(WebLayer*);

  void SetOpacity(float);
  float opacity() const;

  void SetBlendMode(SkBlendMode);
  SkBlendMode blend_mode() const;

  void SetIsRootForIsolatedGroup(bool);
  bool is_root_for_isolated_group();

  void SetHitTestableWithoutDrawsContent(bool);

  void SetContentsOpaque(bool);
  bool contents_opaque() const;

  void SetPosition(const gfx::PointF&);
  const gfx::PointF& position() const;

  void SetTransform(const gfx::Transform&);
  const gfx::Transform& transform() const;

  void SetTransformOrigin(const gfx::Point3F&);
  const gfx::Point3F& transform_origin() const;

  void SetMayContainVideo(bool may) { layer_->SetMayContainVideo(may); }

  // When true the layer may contribute to the compositor's output. When false,
  // it does not. This property does not apply to children of the layer, they
  // may contribute while this layer does not. The layer itself will determine
  // if it has content to contribute, but when false, this prevents it from
  // doing so.
  void SetIsDrawable(bool);
  // Is true if the layer will contribute content to the compositor's output.
  // Will be false if SetIsDrawable(false) is called. But will also be false if
  // the layer itself has no content to contribute, even though the layer was
  // given SetIsDrawable(true).
  bool DrawsContent() const;

  // Set to true if the backside of this layer's contents should be visible
  // when composited. Defaults to false.
  void SetDoubleSided(bool);

  // Sets whether the layer's transform should be flattened.
  void SetShouldFlattenTransform(bool);

  // Sets the id of the layer's 3d rendering context. Layers in the same 3d
  // rendering context id are sorted with one another according to their 3d
  // position rather than their tree order.
  void Set3dSortingContextId(int id);

  // Mark that this layer should use its parent's transform and double-sided
  // properties in determining this layer's backface visibility instead of
  // using its own properties. If this property is set, this layer must
  // have a parent, and the parent may not have this property set.
  // Note: This API is to work around issues with visibility the handling of
  // WebKit layers that have a contents layer (canvas, plugin, WebGL, video,
  // etc).
  void SetUseParentBackfaceVisibility(bool);

  void SetBackgroundColor(SkColor);
  SkColor background_color() const;

  // Clear the filters in use by passing in a newly instantiated
  // FilterOperations object.
  void SetFilters(const cc::FilterOperations&);
  const cc::FilterOperations& filters() const { return layer_->filters(); }

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
  void SetScrollOffset(const gfx::ScrollOffset&);
  const gfx::ScrollOffset& scroll_offset() const;

  // To set a WebLayer as scrollable we must specify the scrolling container
  // bounds.
  void SetScrollable(const gfx::Size& scroll_container_bounds);
  bool scrollable() const;
  const gfx::Size& scroll_container_bounds() const;

  void SetUserScrollable(bool horizontal, bool vertical);
  bool user_scrollable_horizontal() const;
  bool user_scrollable_vertical() const;

  // Indicates that this layer will always scroll on the main thread for the
  // provided reason.
  void AddMainThreadScrollingReasons(uint32_t);
  void ClearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear);
  uint32_t main_thread_scrolling_reasons();
  bool should_scroll_on_main_thread() const;

  void SetNonFastScrollableRegion(const cc::Region&);
  const cc::Region& non_fast_scrollable_region() const;

  void SetTouchActionRegion(const cc::TouchActionRegion& region);
  const cc::TouchActionRegion& touch_action_region() const;

  void SetIsContainerForFixedPositionLayers(bool);
  bool IsContainerForFixedPositionLayers() const;

  void SetIsResizedByBrowserControls(bool);

  // This function sets layer position constraint. The constraint will be used
  // to adjust layer position during threaded scrolling.
  void SetPositionConstraint(const cc::LayerPositionConstraint&);
  const cc::LayerPositionConstraint& position_constraint() const;

  // Sets the sticky position constraint. This will be used to adjust sticky
  // position objects during threaded scrolling.
  void SetStickyPositionConstraint(const cc::LayerStickyPositionConstraint&);
  const cc::LayerStickyPositionConstraint& sticky_position_constraint() const;

  // The scroll callback is notified when the scroll position of the WebLayer
  // changes. Only a single scroll callback can be set for a WebLayer at a time.
  void set_did_scroll_callback(
      base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                   const cc::ElementId&)> callback);

  // Called internally during commit to update the layer with state from the
  // compositor thread. Not to be called externally by users of this class.
  void SetScrollOffsetFromImplSide(const gfx::ScrollOffset&);

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

  void SetElementId(const cc::ElementId&);
  cc::ElementId element_id() const;

  void SetHasWillChangeTransformHint(bool);

  // Called on the scroll layer to trigger showing the overlay scrollbars.
  void ShowScrollbars();

  // Gets the underlying cc layer.
  const cc::Layer* CcLayer() const { return layer_; }
  cc::Layer* CcLayer() { return layer_; }

  cc::LayerTreeHost* GetLayerTreeHostForTesting() const {
    return layer_->GetLayerTreeHostForTesting();
  }

 private:
  cc::Layer* layer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYER_H_
