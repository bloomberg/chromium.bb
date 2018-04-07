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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COMPOSITOR_SUPPORT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COMPOSITOR_SUPPORT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/public/platform/web_scrollbar.h"
#include "third_party/blink/public/platform/web_scrollbar_theme_painter.h"

#include <memory>

namespace cc {
class Layer;
class TextureLayerClient;
}

namespace blink {

class WebContentLayer;
class WebContentLayerClient;
class WebExternalTextureLayer;
class WebImageLayer;
class WebLayer;
class WebScrollbarLayer;
class WebScrollbarThemeGeometry;

class WebCompositorSupport {
 public:
  // Layers -------------------------------------------------------

  virtual std::unique_ptr<WebLayer> CreateLayer() = 0;

  virtual std::unique_ptr<WebLayer> CreateLayerFromCCLayer(cc::Layer*) = 0;

  virtual std::unique_ptr<WebContentLayer> CreateContentLayer(
      WebContentLayerClient*) = 0;

  virtual std::unique_ptr<WebExternalTextureLayer> CreateExternalTextureLayer(
      cc::TextureLayerClient*) = 0;

  virtual std::unique_ptr<WebImageLayer> CreateImageLayer() = 0;

  virtual std::unique_ptr<WebScrollbarLayer> CreateScrollbarLayer(
      std::unique_ptr<WebScrollbar>,
      WebScrollbarThemePainter,
      std::unique_ptr<WebScrollbarThemeGeometry>) = 0;

  virtual std::unique_ptr<WebScrollbarLayer> CreateOverlayScrollbarLayer(
      std::unique_ptr<WebScrollbar>,
      WebScrollbarThemePainter,
      std::unique_ptr<WebScrollbarThemeGeometry>) = 0;

  virtual std::unique_ptr<WebScrollbarLayer> CreateSolidColorScrollbarLayer(
      WebScrollbar::Orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar) = 0;

 protected:
  virtual ~WebCompositorSupport() = default;
};
}

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COMPOSITOR_SUPPORT_H_
