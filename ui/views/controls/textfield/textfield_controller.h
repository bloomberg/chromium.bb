// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
#pragma once

#include "base/string16.h"

namespace views {

class KeyEvent;
class Textfield;

// This defines the callback interface for other code to be notified of changes
// in the state of a text field.
class TextfieldController {
 public:
  // This method is called whenever the text in the field is changed by the
  // user. It won't be called if the text is changed by calling
  // Textfield::SetText() or Textfield::AppendText().
  virtual void ContentsChanged(Textfield* sender,
                               const string16& new_contents) = 0;

  // This method is called to get notified about keystrokes in the edit.
  // Returns true if the message was handled and should not be processed
  // further. If it returns false the processing continues.
  virtual bool HandleKeyEvent(Textfield* sender,
                              const KeyEvent& key_event) = 0;

  // Called before performing a user action that may change the textfield.
  // It's currently only supported by Views implementation.
  virtual void OnBeforeUserAction(Textfield* sender) {}

  // Called after performing a user action that may change the textfield.
  // It's currently only supported by Views implementation.
  virtual void OnAfterUserAction(Textfield* sender) {}

 protected:
  virtual ~TextfieldController() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
