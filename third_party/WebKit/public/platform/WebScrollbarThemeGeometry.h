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

#ifndef WebScrollbarThemeGeometry_h
#define WebScrollbarThemeGeometry_h

#include "WebRect.h"
#include "WebSize.h"

namespace blink {

class WebScrollbar;

class BLINK_PLATFORM_EXPORT WebScrollbarThemeGeometry {
 public:
  virtual ~WebScrollbarThemeGeometry() = default;

  virtual bool HasButtons(WebScrollbar*) = 0;
  virtual bool HasThumb(WebScrollbar*) = 0;
  virtual WebRect TrackRect(WebScrollbar*) = 0;
  virtual WebRect ThumbRect(WebScrollbar*) = 0;
  virtual WebRect BackButtonStartRect(WebScrollbar*) = 0;
  virtual WebRect BackButtonEndRect(WebScrollbar*) = 0;
  virtual WebRect ForwardButtonStartRect(WebScrollbar*) = 0;
  virtual WebRect ForwardButtonEndRect(WebScrollbar*) = 0;
  virtual WebSize NinePatchThumbCanvasSize(WebScrollbar*) = 0;
  virtual WebRect NinePatchThumbAperture(WebScrollbar*) = 0;
};

}  // namespace blink

#endif
