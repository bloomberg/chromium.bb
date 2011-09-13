// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/compositor_observer.h"

namespace ui {

Compositor::Compositor(CompositorDelegate* delegate, const gfx::Size& size)
    : delegate_(delegate),
      size_(size) {
}

Compositor::~Compositor() {
}

void Compositor::NotifyStart(bool clear) {
  OnNotifyStart(clear);
}

void Compositor::NotifyEnd() {
  OnNotifyEnd();
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingEnded());
}

void Compositor::AddObserver(CompositorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}


}  // namespace ui
