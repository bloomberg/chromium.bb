// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_ENDED_REASON_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_ENDED_REASON_H_

#include <string>

namespace views {

// Enumeration of the different reasons why an ink drop animation has finished.
enum class InkDropAnimationEndedReason {
  // The animation was completed successfully.
  SUCCESS,
  // The animation was stopped prematurely before reaching its final state.
  PRE_EMPTED
};

// Returns a human readable string for |reason|.  Useful for logging.
std::string ToString(InkDropAnimationEndedReason reason);

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_ENDED_REASON_H_
