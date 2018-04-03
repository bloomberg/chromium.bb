// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/PictureInPictureWindow.h"

#include "core/execution_context/ExecutionContext.h"

namespace blink {

PictureInPictureWindow::PictureInPictureWindow(
    ExecutionContext* execution_context,
    int width,
    int height)
    : ContextClient(execution_context), width_(width), height_(height) {}

void PictureInPictureWindow::OnClose() {
  width_ = 0;
  height_ = 0;
}

const AtomicString& PictureInPictureWindow::InterfaceName() const {
  return EventTargetNames::PictureInPictureWindow;
}

void PictureInPictureWindow::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
