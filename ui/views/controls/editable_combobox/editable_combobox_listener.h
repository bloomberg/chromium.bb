// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_LISTENER_H_
#define UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_LISTENER_H_

#include "ui/views/views_export.h"

namespace views {

class EditableCombobox;

// Interface used to notify consumers when something interesting happens to an
// EditableCombobox.
class VIEWS_EXPORT EditableComboboxListener {
 public:
  // Called when the content of the main textfield changes, either as the user
  // types or after selecting an option from the menu.
  virtual void OnContentChanged(EditableCombobox* editable_combobox) = 0;

 protected:
  virtual ~EditableComboboxListener() = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_LISTENER_H_
