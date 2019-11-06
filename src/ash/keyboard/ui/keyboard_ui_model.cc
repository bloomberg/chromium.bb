// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_model.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"

namespace keyboard {

namespace {

// Returns whether a given state transition is valid.
// See the design document linked in https://crbug.com/71990.
bool IsAllowedStateTransition(KeyboardUIState from, KeyboardUIState to) {
  static const base::NoDestructor<
      std::set<std::pair<KeyboardUIState, KeyboardUIState>>>
      kAllowedStateTransition({
          // The initial ShowKeyboard scenario
          // INITIAL -> LOADING_EXTENSION -> HIDDEN -> SHOWN.
          {KeyboardUIState::kInitial, KeyboardUIState::kLoading},
          {KeyboardUIState::kLoading, KeyboardUIState::kHidden},
          {KeyboardUIState::kHidden, KeyboardUIState::kShown},

          // Hide scenario
          // SHOWN -> WILL_HIDE -> HIDDEN.
          {KeyboardUIState::kShown, KeyboardUIState::kWillHide},
          {KeyboardUIState::kWillHide, KeyboardUIState::kHidden},

          // Focus transition scenario
          // SHOWN -> WILL_HIDE -> SHOWN.
          {KeyboardUIState::kWillHide, KeyboardUIState::kShown},

          // HideKeyboard can be called at anytime for example on shutdown.
          {KeyboardUIState::kShown, KeyboardUIState::kHidden},

          // Return to INITIAL when keyboard is disabled.
          {KeyboardUIState::kLoading, KeyboardUIState::kInitial},
          {KeyboardUIState::kHidden, KeyboardUIState::kInitial},
      });
  return kAllowedStateTransition->count(std::make_pair(from, to)) == 1;
}

// Records a state transition for metrics.
void RecordStateTransition(KeyboardUIState prev, KeyboardUIState next) {
  const bool valid_transition = IsAllowedStateTransition(prev, next);

  // Emit UMA
  const int transition_record =
      (valid_transition ? 1 : -1) *
      (static_cast<int>(prev) * 1000 + static_cast<int>(next));
  base::UmaHistogramSparse("VirtualKeyboard.ControllerStateTransition",
                           transition_record);
  UMA_HISTOGRAM_BOOLEAN("VirtualKeyboard.ControllerStateTransitionIsValid",
                        valid_transition);

  DCHECK(valid_transition) << "State: " << StateToStr(prev) << " -> "
                           << StateToStr(next) << " Unexpected transition";
}

}  // namespace

std::string StateToStr(KeyboardUIState state) {
  switch (state) {
    case KeyboardUIState::kUnknown:
      return "UNKNOWN";
    case KeyboardUIState::kInitial:
      return "INITIAL";
    case KeyboardUIState::kLoading:
      return "LOADING";
    case KeyboardUIState::kShown:
      return "SHOWN";
    case KeyboardUIState::kWillHide:
      return "WILL_HIDE";
    case KeyboardUIState::kHidden:
      return "HIDDEN";
  }
}

KeyboardUIModel::KeyboardUIModel() = default;

void KeyboardUIModel::ChangeState(KeyboardUIState new_state) {
  RecordStateTransition(state_, new_state);

  if (new_state == state_)
    return;

  state_ = new_state;
}

}  // namespace keyboard
