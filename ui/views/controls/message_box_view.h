// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_
#define UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "ui/views/view.h"

namespace views {

class Checkbox;
class ImageView;
class Label;
class Textfield;

// This class displays the contents of a message box. It is intended for use
// within a constrained window, and has options for a message, prompt, OK
// and Cancel buttons.
class VIEWS_EXPORT MessageBoxView : public View {
 public:
  MessageBoxView(int dialog_flags,
                 const string16& message,
                 const string16& default_prompt,
                 int message_width);

  MessageBoxView(int dialog_flags,
                 const string16& message,
                 const string16& default_prompt);

  virtual ~MessageBoxView();

  // Returns the text box.
  views::Textfield* text_box() { return prompt_field_; }

  // Returns user entered data in the prompt field.
  string16 GetInputText();

  // Returns true if a checkbox is selected, false otherwise. (And false if
  // the message box has no checkbox.)
  bool IsCheckBoxSelected();

  // Adds |icon| to the upper left of the message box or replaces the current
  // icon. To start out, the message box has no icon.
  void SetIcon(const SkBitmap& icon);

  // Adds a checkbox with the specified label to the message box if this is the
  // first call. Otherwise, it changes the label of the current checkbox. To
  // start, the message box has no checkbox until this function is called.
  void SetCheckBoxLabel(const string16& label);

  // Sets the state of the check-box.
  void SetCheckBoxSelected(bool selected);

  // View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  // Handles Ctrl-C and writes the message in the system clipboard.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

 private:
  // Sets up the layout manager and initializes the prompt field. This should
  // only be called once, from the constructor.
  void Init(int dialog_flags, const string16& default_prompt);

  // Sets up the layout manager based on currently initialized views. Should be
  // called when a view is initialized or changed.
  void ResetLayoutManager();

  // Message for the message box.
  Label* message_label_;

  // Input text field for the message box.
  Textfield* prompt_field_;

  // Icon displayed in the upper left corner of the message box.
  ImageView* icon_;

  // Checkbox for the message box.
  Checkbox* checkbox_;

  // Maximum width of the message label.
  int message_width_;

  DISALLOW_COPY_AND_ASSIGN(MessageBoxView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_
