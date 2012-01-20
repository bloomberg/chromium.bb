// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
#pragma once

#include "base/string16.h"
#include "ui/views/views_export.h"

namespace ui {
class OSExchangeData;
class SimpleMenuModel;
}  // namespace ui

namespace views {

class KeyEvent;
class Textfield;

// This defines the callback interface for other code to be notified of changes
// in the state of a text field.
class VIEWS_EXPORT TextfieldController {
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

  // Called after performing a Cut or Copy operation.
  virtual void OnAfterCutOrCopy() {}

  // Called after the textfield has written drag data to give the controller a
  // chance to modify the drag data.
  virtual void OnWriteDragData(ui::OSExchangeData* data) {}

  // Gives the controller a chance to modify the context menu contents.
  virtual void UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {}

  // Returns true if the |command_id| should be enabled in the context menu.
  virtual bool IsCommandIdEnabled(int command_id) const;

  // Execute context menu command specified by |command_id|.
  virtual void ExecuteCommand(int command_id) {}

 protected:
  virtual ~TextfieldController() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
