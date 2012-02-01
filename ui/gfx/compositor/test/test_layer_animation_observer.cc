// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test/test_layer_animation_observer.h"

#include <cstddef>

namespace ui {

TestLayerAnimationObserver::TestLayerAnimationObserver()
    : last_ended_sequence_(NULL),
      last_scheduled_sequence_(NULL),
      last_aborted_sequence_(NULL),
      requires_notification_when_animator_destroyed_(false) {
}

TestLayerAnimationObserver::~TestLayerAnimationObserver() {
}

void TestLayerAnimationObserver::OnLayerAnimationEnded(
    const LayerAnimationSequence* sequence) {
  last_ended_sequence_ = sequence;
}

void TestLayerAnimationObserver::OnLayerAnimationAborted(
    const LayerAnimationSequence* sequence) {
  last_aborted_sequence_ = sequence;
}

void TestLayerAnimationObserver::OnLayerAnimationScheduled(
    const LayerAnimationSequence* sequence) {
  last_scheduled_sequence_ = sequence;
}

bool
TestLayerAnimationObserver::RequiresNotificationWhenAnimatorDestroyed() const {
  return requires_notification_when_animator_destroyed_;
}

}  // namespace ui
