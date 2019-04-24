// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/callback_layer_animation_observer.h"

#include "base/bind.h"
#include "ui/compositor/layer_animation_sequence.h"

namespace ui {

void CallbackLayerAnimationObserver::DummyAnimationStartedCallback(
    const CallbackLayerAnimationObserver&) {}

bool CallbackLayerAnimationObserver::DummyAnimationEndedCallback(
    bool should_delete_observer,
    const CallbackLayerAnimationObserver&) {
  return should_delete_observer;
}

CallbackLayerAnimationObserver::CallbackLayerAnimationObserver(
    AnimationStartedCallback animation_started_callback,
    AnimationEndedCallback animation_ended_callback)
    : animation_started_callback_(animation_started_callback),
      animation_ended_callback_(animation_ended_callback) {}

CallbackLayerAnimationObserver::CallbackLayerAnimationObserver(
    AnimationStartedCallback animation_started_callback,
    bool should_delete_observer)
    : animation_started_callback_(animation_started_callback),
      animation_ended_callback_(base::Bind(
          &CallbackLayerAnimationObserver::DummyAnimationEndedCallback,
          should_delete_observer)) {}

CallbackLayerAnimationObserver::CallbackLayerAnimationObserver(
    AnimationEndedCallback animation_ended_callback)
    : animation_started_callback_(base::Bind(
          &CallbackLayerAnimationObserver::DummyAnimationStartedCallback)),
      animation_ended_callback_(animation_ended_callback) {}

CallbackLayerAnimationObserver::~CallbackLayerAnimationObserver() {
  if (destroyed_)
    *destroyed_ = true;
}

void CallbackLayerAnimationObserver::SetActive() {
  active_ = true;

  bool destroyed = false;
  destroyed_ = &destroyed;

  CheckAllSequencesStarted();

  if (destroyed)
    return;
  destroyed_ = nullptr;

  CheckAllSequencesCompleted();
}

void CallbackLayerAnimationObserver::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {
  CHECK_LT(started_count_, attached_sequence_count_);
  ++started_count_;
  CheckAllSequencesStarted();
}

void CallbackLayerAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  CHECK_LT(GetNumSequencesCompleted(), attached_sequence_count_);
  ++successful_count_;
  CheckAllSequencesCompleted();
}

void CallbackLayerAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  CHECK_LT(GetNumSequencesCompleted(), attached_sequence_count_);
  ++aborted_count_;
  CheckAllSequencesCompleted();
}

void CallbackLayerAnimationObserver::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {}

bool CallbackLayerAnimationObserver::RequiresNotificationWhenAnimatorDestroyed()
    const {
  return true;
}

void CallbackLayerAnimationObserver::OnAttachedToSequence(
    ui::LayerAnimationSequence* sequence) {
  ++attached_sequence_count_;
}

void CallbackLayerAnimationObserver::OnDetachedFromSequence(
    ui::LayerAnimationSequence* sequence) {
  CHECK_LT(detached_sequence_count_, attached_sequence_count_);
  ++detached_sequence_count_;
}

int CallbackLayerAnimationObserver::GetNumSequencesCompleted() {
  return aborted_count_ + successful_count_;
}

void CallbackLayerAnimationObserver::CheckAllSequencesStarted() {
  if (active_ && attached_sequence_count_ == started_count_)
    animation_started_callback_.Run(*this);
}

void CallbackLayerAnimationObserver::CheckAllSequencesCompleted() {
  if (active_ && GetNumSequencesCompleted() == attached_sequence_count_) {
    active_ = false;
    bool destroyed = false;
    destroyed_ = &destroyed;

    bool should_delete = animation_ended_callback_.Run(*this);

    if (destroyed) {
      if (should_delete)
        LOG(WARNING) << "CallbackLayerAnimationObserver was explicitly "
                        "destroyed AND was requested to be destroyed via the "
                        "AnimationEndedCallback's return value.";
      return;
    }
    destroyed_ = nullptr;

    if (should_delete)
      delete this;
  }
}

}  // namespace ui
