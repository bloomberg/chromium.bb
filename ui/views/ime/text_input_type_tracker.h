// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_TEXT_INPUT_TYPE_TRACKER_H_
#define UI_VIEWS_IME_TEXT_INPUT_TYPE_TRACKER_H_
#pragma once

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/views/views_export.h"

namespace views {

class Widget;

// This interface should be implemented by classes that want to be notified when
// the text input type of the focused widget is changed or the focused widget is
// changed.
class TextInputTypeObserver {
 public:
  // This function will be called, when the text input type of focused |widget|
  // is changed or the the widget is getting/losing focus.
  virtual void TextInputTypeChanged(ui::TextInputType type, Widget* widget) = 0;

 protected:
  virtual ~TextInputTypeObserver() {}
};

// This class is for tracking the text input type of focused widget.
class VIEWS_EXPORT TextInputTypeTracker {
 public:
  // Returns the singleton instance.
  static TextInputTypeTracker* GetInstance();

  // Adds/removes a TextInputTypeObserver |observer| to the set of
  // active observers.
  void AddTextInputTypeObserver(TextInputTypeObserver* observer);
  void RemoveTextInputTypeObserver(TextInputTypeObserver* observer);

  // views::InputMethod should call this function with new text input |type| and
  // the |widget| associated with the input method, when the text input type
  // is changed.
  // Note: The widget should have focus or is losing focus.
  void OnTextInputTypeChanged(ui::TextInputType type, Widget* widget);

 private:
  TextInputTypeTracker();
  ~TextInputTypeTracker();

  ObserverList<TextInputTypeObserver> text_input_type_observers_;

  friend struct DefaultSingletonTraits<TextInputTypeTracker>;
  DISALLOW_COPY_AND_ASSIGN(TextInputTypeTracker);
};

}  // namespace views

#endif  // UI_VIEWS_IME_TEXT_INPUT_TYPE_TRACKER_H_
