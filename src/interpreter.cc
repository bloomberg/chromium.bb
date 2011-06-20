// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/interpreter.h"

#include <base/logging.h>

namespace gestures {

void SpeculativeGestures::Add(const SpeculativeGesture* gesture) {
  // TODO(adlr): fill in
}

void SpeculativeGestures::AddMovement(short first_actor,
                                      short second_actor,
                                      int dx,
                                      int dy,
                                      short confidence,
                                      const struct HardwareState* hwstate) {
  gestures_.resize(gestures_.size() + 1);
  SpeculativeGesture& elt = gestures_.back();
  elt.first_actor = first_actor;
  elt.second_actor = second_actor;
  elt.kind = kPointingKind;
  Gesture temp = { dx, dy, 0, 0, 1, 0, hwstate };
  elt.gesture = temp;
  elt.confidence = confidence;
}


const SpeculativeGesture* SpeculativeGestures::GetHighestConfidence() const {
  if (gestures_.size() != 1) {
    LOG(ERROR) << "TODO(adlr): support lookup in SpeculativeGestures";
    return NULL;
  }
  return &gestures_[0];
}
const SpeculativeGesture* SpeculativeGestures::GetOneActor(
    GestureKind kind, short actor) const {
  // TODO(adlr): fill in
  return NULL;
}
const SpeculativeGesture* SpeculativeGestures::GetTwoActors(
    GestureKind kind,
    short actor1,
    short actor2) const {
  // TODO(adlr): fill in
  return NULL;
}

}  // namespace gestures
