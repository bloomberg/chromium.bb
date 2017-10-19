/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/DOMVisualViewport.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"

namespace blink {

DOMVisualViewport::DOMVisualViewport(LocalDOMWindow* window)
    : window_(window) {}

DOMVisualViewport::~DOMVisualViewport() {}

void DOMVisualViewport::Trace(blink::Visitor* visitor) {
  visitor->Trace(window_);
  EventTargetWithInlineData::Trace(visitor);
}

const AtomicString& DOMVisualViewport::InterfaceName() const {
  return EventTargetNames::DOMVisualViewport;
}

ExecutionContext* DOMVisualViewport::GetExecutionContext() const {
  return window_->GetExecutionContext();
}

float DOMVisualViewport::offsetLeft() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame || !frame->IsMainFrame())
    return 0;

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().OffsetLeft();

  return 0;
}

float DOMVisualViewport::offsetTop() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame || !frame->IsMainFrame())
    return 0;

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().OffsetTop();

  return 0;
}

float DOMVisualViewport::pageLeft() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  LocalFrameView* view = frame->View();
  if (!view)
    return 0;

  frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  float viewport_x = view->GetScrollableArea()->GetScrollOffset().Width();
  return AdjustScrollForAbsoluteZoom(viewport_x, frame->PageZoomFactor());
}

float DOMVisualViewport::pageTop() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  LocalFrameView* view = frame->View();
  if (!view)
    return 0;

  frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  float viewport_y = view->GetScrollableArea()->GetScrollOffset().Height();
  return AdjustScrollForAbsoluteZoom(viewport_y, frame->PageZoomFactor());
}

double DOMVisualViewport::width() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  if (!frame->IsMainFrame()) {
    FloatSize viewport_size = window_->GetViewportSize(kExcludeScrollbars);
    return AdjustForAbsoluteZoom(ExpandedIntSize(viewport_size).Width(),
                                 frame->PageZoomFactor());
  }

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().Width();

  return 0;
}

double DOMVisualViewport::height() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  if (!frame->IsMainFrame()) {
    FloatSize viewport_size = window_->GetViewportSize(kExcludeScrollbars);
    return AdjustForAbsoluteZoom(ExpandedIntSize(viewport_size).Height(),
                                 frame->PageZoomFactor());
  }

  if (Page* page = frame->GetPage())
    return page->GetVisualViewport().Height();

  return 0;
}

double DOMVisualViewport::scale() const {
  LocalFrame* frame = window_->GetFrame();
  if (!frame)
    return 0;

  if (!frame->IsMainFrame())
    return 1;

  if (Page* page = window_->GetFrame()->GetPage())
    return page->GetVisualViewport().ScaleForVisualViewport();

  return 0;
}

}  // namespace blink
