// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/text_input_type_tracker.h"

namespace views {

void TextInputTypeTracker::AddTextInputTypeObserver(
    TextInputTypeObserver* observer) {
  text_input_type_observers_.AddObserver(observer);
}

void TextInputTypeTracker::RemoveTextInputTypeObserver(
    TextInputTypeObserver* observer) {
  text_input_type_observers_.RemoveObserver(observer);
}

void TextInputTypeTracker::OnTextInputTypeChanged(
    ui::TextInputType type, Widget* widget) {
  FOR_EACH_OBSERVER(TextInputTypeObserver,
                    text_input_type_observers_,
                    TextInputTypeChanged(type, widget));
}

TextInputTypeTracker::TextInputTypeTracker() {
}

TextInputTypeTracker::~TextInputTypeTracker() {
}

// static
TextInputTypeTracker* TextInputTypeTracker::GetInstance() {
  return Singleton<TextInputTypeTracker>::get();
}

}  // namespace views
