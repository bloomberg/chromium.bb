// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/mock_braille_controller.h"

namespace extensions {
namespace api {
namespace braille_display_private {

MockBrailleController::MockBrailleController()
    : available_(false), observer_(NULL) {}

std::unique_ptr<DisplayState> MockBrailleController::GetDisplayState() {
  std::unique_ptr<DisplayState> state(new DisplayState());
  state->available = available_;
  if (available_) {
    state->text_column_count.reset(new int(18));
    state->text_row_count.reset(new int(18));
  }
  return state;
}

void MockBrailleController::AddObserver(BrailleObserver* observer) {
  CHECK(observer_ == NULL);
  observer_ = observer;
}

void MockBrailleController::RemoveObserver(BrailleObserver* observer) {
  CHECK(observer == observer_);
  observer_ = NULL;
}

void MockBrailleController::SetAvailable(bool available) {
  available_ = available;
}

BrailleObserver* MockBrailleController::GetObserver() const {
  return observer_;
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
