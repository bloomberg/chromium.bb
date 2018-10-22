// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"

namespace blink {

PictureInPictureWindow::PictureInPictureWindow(
    ExecutionContext* execution_context,
    const WebSize& size)
    : ContextClient(execution_context), size_(size) {}

void PictureInPictureWindow::OnClose() {
  size_.width = size_.height = 0;
}

void PictureInPictureWindow::OnResize(const WebSize& size) {
  if (size_ == size)
    return;

  size_ = size;
  DispatchEvent(*Event::Create(EventTypeNames::resize));
}

const AtomicString& PictureInPictureWindow::InterfaceName() const {
  return EventTargetNames::PictureInPictureWindow;
}

void PictureInPictureWindow::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (event_type == EventTypeNames::resize) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPictureInPictureWindowResizeEventListener);
  }

  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
}

bool PictureInPictureWindow::HasPendingActivity() const {
  return GetExecutionContext() && HasEventListeners();
}

void PictureInPictureWindow::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
