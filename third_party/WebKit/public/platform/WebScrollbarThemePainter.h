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

#ifndef WebScrollbarThemePainter_h
#define WebScrollbarThemePainter_h

#include "public/platform/WebCanvas.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"

namespace blink {

class ScrollbarTheme;
class Scrollbar;
class WebScrollbar;

class WebScrollbarThemePainter {
 public:
  WebScrollbarThemePainter() : theme_(0), device_scale_factor_(1.0) {}
  WebScrollbarThemePainter(const WebScrollbarThemePainter& painter) {
    Assign(painter);
  }
  virtual ~WebScrollbarThemePainter() { Reset(); }
  WebScrollbarThemePainter& operator=(const WebScrollbarThemePainter& painter) {
    Assign(painter);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Assign(const WebScrollbarThemePainter&);
  BLINK_PLATFORM_EXPORT void Reset();

  BLINK_PLATFORM_EXPORT void PaintScrollbarBackground(WebCanvas*,
                                                      const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintTrackBackground(WebCanvas*, const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintBackTrackPart(WebCanvas*, const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintForwardTrackPart(WebCanvas*, const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintBackButtonStart(WebCanvas*, const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintBackButtonEnd(WebCanvas*, const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintForwardButtonStart(WebCanvas*,
                                                     const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintForwardButtonEnd(WebCanvas*, const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintTickmarks(WebCanvas*, const WebRect&);
  BLINK_PLATFORM_EXPORT void PaintThumb(WebCanvas*, const WebRect&);

  // This opacity is applied on top of the content that is painted for the
  // thumb.
  BLINK_PLATFORM_EXPORT float ThumbOpacity() const;

  BLINK_PLATFORM_EXPORT bool TrackNeedsRepaint() const;
  BLINK_PLATFORM_EXPORT bool ThumbNeedsRepaint() const;

  BLINK_PLATFORM_EXPORT bool UsesNinePatchThumbResource() const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebScrollbarThemePainter(ScrollbarTheme&,
                                                 Scrollbar&,
                                                 float device_scale_factor);
#endif

  BLINK_PLATFORM_EXPORT float DeviceScaleFactor() const {
    return device_scale_factor_;
  }

 private:
  // The theme is not owned by this class. It is assumed that the theme is a
  // static pointer and its lifetime is essentially infinite. The functions
  // called from the painter may not be thread-safe, so all calls must be made
  // from the same thread that it is created on.
  ScrollbarTheme* theme_;

  // It is assumed that the constructor of this paint object is responsible
  // for the lifetime of this scrollbar. The painter has to use the real
  // scrollbar (and not a WebScrollbar wrapper) due to static_casts for
  // LayoutScrollbar and pointer-based HashMap lookups for Lion scrollbars.
  WebPrivatePtr<Scrollbar> scrollbar_;

  float device_scale_factor_;
};

}  // namespace blink

#endif
