// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_LISTBOX_LISTBOX_H_
#define VIEWS_CONTROLS_LISTBOX_LISTBOX_H_
#pragma once

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/string16.h"
#include "views/view.h"

namespace views {

class NativeListboxWrapper;

// A Listbox is a view that displays multiple rows of fixed strings.
// Exactly one of these strings is shown as selected at all times.
class Listbox : public View {
 public:
  // An interface implemented by an object to let it know that a listbox
  // selection has changed.
  class Listener {
   public:
    // This is called if the user changes the current selection of the
    // listbox.
    virtual void ListboxSelectionChanged(Listbox* sender) = 0;
  };

  // Creates a new listbox, given the list of strings. |listener| can be NULL.
  // Listbox does not take ownership of |listener|.
  Listbox(const std::vector<string16>& strings, Listbox::Listener* listener);
  virtual ~Listbox();

  // Returns the number of rows in the table.
  int GetRowCount() const;

  // Returns the 0-based index of the currently selected row, or -1 if nothing
  // is selected. Note that as soon as a row has been selected once, there will
  // always be a selected row.
  int SelectedRow() const;

  // Selects the specified row. Note that this does NOT call the listener's
  // |ListboxSelectionChanged()| method.
  void SelectRow(int row);

 protected:
  virtual NativeListboxWrapper* CreateWrapper();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

 private:
  // Data stored in the listbox.
  std::vector<string16> strings_;

  // Listens to selection changes.
  Listbox::Listener* listener_;

  // The object that actually implements the table.
  NativeListboxWrapper* native_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(Listbox);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_LISTBOX_LISTBOX_H_
