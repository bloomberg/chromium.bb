// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_filter.h"

namespace aura {

EventFilter::EventFilter(Window* owner) : owner_(owner) {
}

EventFilter::~EventFilter() {
}

bool EventFilter::OnMouseEvent(Window* target, MouseEvent* event) {
  return false;
}

}  // namespace aura
