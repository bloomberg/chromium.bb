/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINK_HIGHLIGHT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINK_HIGHLIGHT_IMPL_H_

#include <memory>
#include "third_party/blink/public/platform/web_content_layer.h"
#include "third_party/blink/public/platform/web_content_layer_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/link_highlight.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class GraphicsLayer;
class LayoutBoxModelObject;
class Node;
class WebContentLayer;
class WebLayer;
class WebViewImpl;

class CORE_EXPORT LinkHighlightImpl final : public LinkHighlight,
                                            public WebContentLayerClient,
                                            public CompositorAnimationDelegate,
                                            public CompositorAnimationClient {
 public:
  static std::unique_ptr<LinkHighlightImpl> Create(Node*, WebViewImpl*);
  ~LinkHighlightImpl() override;

  WebContentLayer* ContentLayer();
  WebLayer* ClipLayer();
  void StartHighlightAnimationIfNeeded();
  void UpdateGeometry();

  // WebContentLayerClient implementation.
  gfx::Rect PaintableRegion() override;
  void PaintContents(WebDisplayItemList*,
                     WebContentLayerClient::PaintingControlSetting) override;

  // CompositorAnimationDelegate implementation.
  void NotifyAnimationStarted(double monotonic_time, int group) override;
  void NotifyAnimationFinished(double monotonic_time, int group) override;
  void NotifyAnimationAborted(double monotonic_time, int group) override {}

  // LinkHighlight implementation.
  void Invalidate() override;
  WebLayer* Layer() override;
  void ClearCurrentGraphicsLayer() override;

  // CompositorAnimationClient implementation.
  CompositorAnimation* GetCompositorAnimation() const override;

  GraphicsLayer* CurrentGraphicsLayerForTesting() const {
    return current_graphics_layer_;
  }

 private:
  LinkHighlightImpl(Node*, WebViewImpl*);

  void ReleaseResources();
  void ComputeQuads(const Node&, Vector<FloatQuad>&) const;

  void AttachLinkHighlightToCompositingLayer(
      const LayoutBoxModelObject& paint_invalidation_container);
  void ClearGraphicsLayerLinkHighlightPointer();
  // This function computes the highlight path, and returns true if it has
  // changed size since the last call to this function.
  bool ComputeHighlightLayerPathAndPosition(const LayoutBoxModelObject&);

  std::unique_ptr<WebContentLayer> content_layer_;
  std::unique_ptr<WebLayer> clip_layer_;
  Path path_;

  Persistent<Node> node_;
  WebViewImpl* owning_web_view_;
  GraphicsLayer* current_graphics_layer_;
  bool is_scrolling_graphics_layer_;
  std::unique_ptr<CompositorAnimation> compositor_animation_;

  bool geometry_needs_update_;
  bool is_animating_;
  double start_time_;
  UniqueObjectId unique_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINK_HIGHLIGHT_IMPL_H_
