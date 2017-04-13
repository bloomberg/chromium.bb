/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/exported/WebScrollbarImpl.h"

#include "platform/geometry/IntRect.h"
#include "platform/scroll/Scrollbar.h"

namespace blink {

WebScrollbarImpl::WebScrollbarImpl(Scrollbar* scrollbar)
    : scrollbar_(scrollbar) {}

bool WebScrollbarImpl::IsOverlay() const {
  return scrollbar_->IsOverlayScrollbar();
}

int WebScrollbarImpl::Value() const {
  return scrollbar_->Value();
}

WebPoint WebScrollbarImpl::Location() const {
  return scrollbar_->Location();
}

WebSize WebScrollbarImpl::Size() const {
  return scrollbar_->Size();
}

bool WebScrollbarImpl::Enabled() const {
  return scrollbar_->Enabled();
}

int WebScrollbarImpl::Maximum() const {
  return scrollbar_->Maximum();
}

int WebScrollbarImpl::TotalSize() const {
  return scrollbar_->TotalSize();
}

bool WebScrollbarImpl::IsScrollableAreaActive() const {
  return scrollbar_->IsScrollableAreaActive();
}

void WebScrollbarImpl::GetTickmarks(WebVector<WebRect>& web_tickmarks) const {
  Vector<IntRect> tickmarks;
  scrollbar_->GetTickmarks(tickmarks);

  WebVector<WebRect> result(tickmarks.size());
  for (size_t i = 0; i < tickmarks.size(); ++i)
    result[i] = tickmarks[i];

  web_tickmarks.Swap(result);
}

WebScrollbar::ScrollbarControlSize WebScrollbarImpl::GetControlSize() const {
  return static_cast<WebScrollbar::ScrollbarControlSize>(
      scrollbar_->GetControlSize());
}

WebScrollbar::ScrollbarPart WebScrollbarImpl::PressedPart() const {
  return static_cast<WebScrollbar::ScrollbarPart>(scrollbar_->PressedPart());
}

WebScrollbar::ScrollbarPart WebScrollbarImpl::HoveredPart() const {
  return static_cast<WebScrollbar::ScrollbarPart>(scrollbar_->HoveredPart());
}

WebScrollbarOverlayColorTheme WebScrollbarImpl::ScrollbarOverlayColorTheme()
    const {
  return static_cast<WebScrollbarOverlayColorTheme>(
      scrollbar_->GetScrollbarOverlayColorTheme());
}

WebScrollbar::Orientation WebScrollbarImpl::GetOrientation() const {
  return static_cast<WebScrollbar::Orientation>(scrollbar_->Orientation());
}

bool WebScrollbarImpl::IsLeftSideVerticalScrollbar() const {
  return scrollbar_->IsLeftSideVerticalScrollbar();
}

bool WebScrollbarImpl::IsCustomScrollbar() const {
  return scrollbar_->IsCustomScrollbar();
}

float WebScrollbarImpl::ElasticOverscroll() const {
  return scrollbar_->ElasticOverscroll();
}

void WebScrollbarImpl::SetElasticOverscroll(float elastic_overscroll) {
  scrollbar_->SetElasticOverscroll(elastic_overscroll);
}

}  // namespace blink
