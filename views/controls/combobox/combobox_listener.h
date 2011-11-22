// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_COMBOBOX_LISTENER_H_
#define VIEWS_CONTROLS_COMBOBOX_COMBOBOX_LISTENER_H_
#pragma once

namespace views {

class Combobox;

// An interface implemented by an object to let it know that the selected item
// of a combobox changed.
class ComboboxListener {
 public:
  // This is invoked once the selected item changed.
  virtual void ItemChanged(Combobox* combo_box,
                           int prev_index,
                           int new_index) = 0;

 protected:
  virtual ~ComboboxListener() {}
};

}  // namespace views

#endif  // VIEWS_CONTROLS_COMBOBOX_COMBOBOX_LISTENER_H_
