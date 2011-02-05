// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_LISTBOX_NATIVE_LISTBOX_WRAPPER_H_
#define VIEWS_CONTROLS_LISTBOX_NATIVE_LISTBOX_WRAPPER_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "views/controls/listbox/listbox.h"

namespace views {

// An interface implemented by an object that provides a platform-native
// listbox.
class NativeListboxWrapper {
 public:
  // Returns the number of rows in the table.
  virtual int GetRowCount() const = 0;

  // Returns the 0-based index of the currently selected row.
  virtual int SelectedRow() const = 0;

  // Selects the specified row, making sure it's visible.
  virtual void SelectRow(int row) = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Creates an appropriate NativeListboxWrapper for the platform.
  static NativeListboxWrapper* CreateNativeWrapper(
      Listbox* listbox,
      const std::vector<string16>& strings,
      Listbox::Listener* listener);

 protected:
  virtual ~NativeListboxWrapper() {}
};

}  // namespace views

#endif  // VIEWS_CONTROLS_LISTBOX_NATIVE_LISTBOX_WRAPPER_H_
