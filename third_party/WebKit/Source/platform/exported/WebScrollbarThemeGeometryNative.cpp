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

#include "platform/exported/WebScrollbarThemeGeometryNative.h"

#include <memory>
#include "platform/exported/WebScrollbarThemeClientImpl.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebScrollbar.h"

namespace blink {

std::unique_ptr<WebScrollbarThemeGeometryNative>
WebScrollbarThemeGeometryNative::Create(ScrollbarTheme& theme) {
  return WTF::WrapUnique(new WebScrollbarThemeGeometryNative(theme));
}

WebScrollbarThemeGeometryNative::WebScrollbarThemeGeometryNative(
    ScrollbarTheme& theme)
    : theme_(theme) {}

bool WebScrollbarThemeGeometryNative::HasButtons(WebScrollbar* scrollbar) {
  return theme_.HasButtons(WebScrollbarThemeClientImpl(*scrollbar));
}

bool WebScrollbarThemeGeometryNative::HasThumb(WebScrollbar* scrollbar) {
  return theme_.HasThumb(WebScrollbarThemeClientImpl(*scrollbar));
}

WebRect WebScrollbarThemeGeometryNative::TrackRect(WebScrollbar* scrollbar) {
  return theme_.TrackRect(WebScrollbarThemeClientImpl(*scrollbar));
}

WebRect WebScrollbarThemeGeometryNative::ThumbRect(WebScrollbar* scrollbar) {
  return theme_.ThumbRect(WebScrollbarThemeClientImpl(*scrollbar));
}

WebRect WebScrollbarThemeGeometryNative::BackButtonStartRect(
    WebScrollbar* scrollbar) {
  return theme_.BackButtonRect(WebScrollbarThemeClientImpl(*scrollbar),
                               kBackButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometryNative::BackButtonEndRect(
    WebScrollbar* scrollbar) {
  return theme_.BackButtonRect(WebScrollbarThemeClientImpl(*scrollbar),
                               kBackButtonEndPart, false);
}

WebRect WebScrollbarThemeGeometryNative::ForwardButtonStartRect(
    WebScrollbar* scrollbar) {
  return theme_.ForwardButtonRect(WebScrollbarThemeClientImpl(*scrollbar),
                                  kForwardButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometryNative::ForwardButtonEndRect(
    WebScrollbar* scrollbar) {
  return theme_.ForwardButtonRect(WebScrollbarThemeClientImpl(*scrollbar),
                                  kForwardButtonEndPart, false);
}

WebSize WebScrollbarThemeGeometryNative::NinePatchThumbCanvasSize(
    WebScrollbar* scrollbar) {
  DCHECK(theme_.UsesNinePatchThumbResource());
  return theme_.NinePatchThumbCanvasSize(
      WebScrollbarThemeClientImpl(*scrollbar));
}

WebRect WebScrollbarThemeGeometryNative::NinePatchThumbAperture(
    WebScrollbar* scrollbar) {
  DCHECK(theme_.UsesNinePatchThumbResource());
  return theme_.NinePatchThumbAperture(WebScrollbarThemeClientImpl(*scrollbar));
}

}  // namespace blink
