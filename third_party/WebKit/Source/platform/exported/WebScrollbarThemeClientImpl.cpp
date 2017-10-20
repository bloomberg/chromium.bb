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

#include "platform/exported/WebScrollbarThemeClientImpl.h"

#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

WebScrollbarThemeClientImpl::WebScrollbarThemeClientImpl(
    WebScrollbar& scrollbar)
    : scrollbar_(scrollbar) {
  ScrollbarTheme::DeprecatedStaticGetTheme().RegisterScrollbar(*this);
}

WebScrollbarThemeClientImpl::~WebScrollbarThemeClientImpl() {
  ScrollbarTheme::DeprecatedStaticGetTheme().UnregisterScrollbar(*this);
}

int WebScrollbarThemeClientImpl::X() const {
  return Location().X();
}

int WebScrollbarThemeClientImpl::Y() const {
  return Location().Y();
}

int WebScrollbarThemeClientImpl::Width() const {
  return Size().Width();
}

int WebScrollbarThemeClientImpl::Height() const {
  return Size().Height();
}

IntSize WebScrollbarThemeClientImpl::Size() const {
  return scrollbar_.Size();
}

IntPoint WebScrollbarThemeClientImpl::Location() const {
  return scrollbar_.Location();
}

void WebScrollbarThemeClientImpl::SetFrameRect(const IntRect&) {
  // Unused by Chromium scrollbar themes.
  NOTREACHED();
}

IntRect WebScrollbarThemeClientImpl::FrameRect() const {
  return IntRect(Location(), Size());
}

void WebScrollbarThemeClientImpl::Invalidate() {
  // Unused by Chromium scrollbar themes.
  NOTREACHED();
}

void WebScrollbarThemeClientImpl::InvalidateRect(const IntRect&) {
  // Unused by Chromium scrollbar themes.
  NOTREACHED();
}

ScrollbarOverlayColorTheme
WebScrollbarThemeClientImpl::GetScrollbarOverlayColorTheme() const {
  return static_cast<ScrollbarOverlayColorTheme>(
      scrollbar_.ScrollbarOverlayColorTheme());
}

void WebScrollbarThemeClientImpl::GetTickmarks(
    Vector<IntRect>& tickmarks) const {
  WebVector<WebRect> web_tickmarks;
  scrollbar_.GetTickmarks(web_tickmarks);
  tickmarks.resize(web_tickmarks.size());
  for (size_t i = 0; i < web_tickmarks.size(); ++i)
    tickmarks[i] = web_tickmarks[i];
}

bool WebScrollbarThemeClientImpl::IsScrollableAreaActive() const {
  return scrollbar_.IsScrollableAreaActive();
}

IntPoint WebScrollbarThemeClientImpl::ConvertFromRootFrame(
    const IntPoint& point_in_root_frame) const {
  // Unused by Chromium scrollbar themes.
  NOTREACHED();
  return point_in_root_frame;
}

bool WebScrollbarThemeClientImpl::IsCustomScrollbar() const {
  return scrollbar_.IsCustomScrollbar();
}

ScrollbarOrientation WebScrollbarThemeClientImpl::Orientation() const {
  return static_cast<ScrollbarOrientation>(scrollbar_.GetOrientation());
}

bool WebScrollbarThemeClientImpl::IsLeftSideVerticalScrollbar() const {
  return scrollbar_.IsLeftSideVerticalScrollbar();
}

int WebScrollbarThemeClientImpl::Value() const {
  return scrollbar_.Value();
}

float WebScrollbarThemeClientImpl::CurrentPos() const {
  return Value();
}

int WebScrollbarThemeClientImpl::VisibleSize() const {
  return TotalSize() - Maximum();
}

int WebScrollbarThemeClientImpl::TotalSize() const {
  return scrollbar_.TotalSize();
}

int WebScrollbarThemeClientImpl::Maximum() const {
  return scrollbar_.Maximum();
}

ScrollbarControlSize WebScrollbarThemeClientImpl::GetControlSize() const {
  return static_cast<ScrollbarControlSize>(scrollbar_.GetControlSize());
}

ScrollbarPart WebScrollbarThemeClientImpl::PressedPart() const {
  return static_cast<ScrollbarPart>(scrollbar_.PressedPart());
}

ScrollbarPart WebScrollbarThemeClientImpl::HoveredPart() const {
  return static_cast<ScrollbarPart>(scrollbar_.HoveredPart());
}

void WebScrollbarThemeClientImpl::StyleChanged() {
  NOTREACHED();
}

void WebScrollbarThemeClientImpl::SetScrollbarsHidden(bool hidden) {
  NOTREACHED();
}

bool WebScrollbarThemeClientImpl::Enabled() const {
  return scrollbar_.Enabled();
}

void WebScrollbarThemeClientImpl::SetEnabled(bool) {
  NOTREACHED();
}

bool WebScrollbarThemeClientImpl::IsOverlayScrollbar() const {
  return scrollbar_.IsOverlay();
}

float WebScrollbarThemeClientImpl::ElasticOverscroll() const {
  return scrollbar_.ElasticOverscroll();
}

void WebScrollbarThemeClientImpl::SetElasticOverscroll(
    float elastic_overscroll) {
  return scrollbar_.SetElasticOverscroll(elastic_overscroll);
}

}  // namespace blink
