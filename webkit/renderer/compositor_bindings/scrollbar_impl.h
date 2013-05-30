// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_SCROLLBAR_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_SCROLLBAR_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/input/scrollbar.h"
#include "third_party/WebKit/public/platform/WebScrollbarThemePainter.h"

namespace WebKit {
class WebScrollbar;
class WebScrollbarThemeGeometry;
}

namespace webkit {

class ScrollbarImpl : public cc::Scrollbar {
 public:
  ScrollbarImpl(scoped_ptr<WebKit::WebScrollbar> scrollbar,
                WebKit::WebScrollbarThemePainter painter,
                scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry);
  virtual ~ScrollbarImpl();

  // cc::Scrollbar implementation.
  virtual cc::ScrollbarOrientation Orientation() const OVERRIDE;
  virtual bool HasThumb() const OVERRIDE;
  virtual bool IsOverlay() const OVERRIDE;
  virtual gfx::Point Location() const OVERRIDE;
  virtual int ThumbThickness() const OVERRIDE;
  virtual int ThumbLength() const OVERRIDE;
  virtual gfx::Rect TrackRect() const OVERRIDE;
  virtual void PaintPart(SkCanvas* canvas,
                         cc::ScrollbarPart part,
                         gfx::Rect content_rect) OVERRIDE;

 private:
  scoped_ptr<WebKit::WebScrollbar> scrollbar_;
  WebKit::WebScrollbarThemePainter painter_;
  scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_SCROLLBAR_IMPL_H_
