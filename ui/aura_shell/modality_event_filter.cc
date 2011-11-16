// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/modality_event_filter.h"

#include "ui/aura/event.h"
#include "ui/aura_shell/modality_event_filter_delegate.h"

namespace aura_shell {
namespace internal {

ModalityEventFilter::ModalityEventFilter(aura::Window* container,
                                         ModalityEventFilterDelegate* delegate)
    : EventFilter(container),
      delegate_(delegate) {
}

ModalityEventFilter::~ModalityEventFilter() {
}

bool ModalityEventFilter::PreHandleKeyEvent(aura::Window* target,
                                            aura::KeyEvent* event) {
  return !delegate_->CanWindowReceiveEvents(target);
}

bool ModalityEventFilter::PreHandleMouseEvent(aura::Window* target,
                                              aura::MouseEvent* event) {
  return !delegate_->CanWindowReceiveEvents(target);
}

ui::TouchStatus ModalityEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  // TODO(sadrul): !
  return ui::TOUCH_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace aura_shell
