// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_INSERTION_DELEGATE_H_
#define UI_VIEWS_CONTROLS_MENU_INSERTION_DELEGATE_H_

namespace views {

class MenuInsertionDelegate {
 public:
  // Returns the index to insert items into the menu at.
  virtual int GetInsertionIndex(HMENU native_menu) = 0;

 protected:
  virtual ~MenuInsertionDelegate() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_INSERTION_DELEGATE_H_
