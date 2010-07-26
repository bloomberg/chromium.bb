// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_LISTBOX_NATIVE_LISTBOX_WIN_H_
#define VIEWS_CONTROLS_LISTBOX_NATIVE_LISTBOX_WIN_H_
#pragma once

#include <windows.h>

#include "base/string16.h"
#include "views/controls/listbox/native_listbox_wrapper.h"
#include "views/controls/native_control_win.h"

namespace views {

// A View that hosts a native Windows listbox.
class NativeListboxWin : public NativeControlWin, public NativeListboxWrapper {
 public:
  NativeListboxWin(Listbox* listbox,
                   const std::vector<string16>& strings,
                   Listbox::Listener* listener);
  virtual ~NativeListboxWin();

  // NativeListboxWrapper implementation:
  virtual int GetRowCount() const;
  virtual int SelectedRow() const;
  virtual void SelectRow(int row);
  virtual View* GetView();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from NativeControlWin:
  virtual bool ProcessMessage(UINT message,
                              WPARAM w_param,
                              LPARAM l_param,
                              LRESULT* result);

 protected:
  virtual void CreateNativeControl();

 private:
  // The listbox we are bound to.
  Listbox* listbox_;

  // The strings shown in the listbox.
  std::vector<string16> strings_;

  // Listens to selection changes.
  Listbox::Listener* listener_;

  DISALLOW_COPY_AND_ASSIGN(NativeListboxWin);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_LISTBOX_NATIVE_LISTBOX_WIN_H_
