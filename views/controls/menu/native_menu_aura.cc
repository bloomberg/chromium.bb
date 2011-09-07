// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/native_menu_linux.h"

#include "base/logging.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/menu_wrapper.h"

namespace views {

class NativeMenuAura : public MenuWrapper {
public:
  explicit NativeMenuAura(Menu2* menu) {}
  virtual ~NativeMenuAura() {}

  // Overridden from MenuWrapper:
  virtual void RunMenuAt(const gfx::Point& point, int alignment) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void CancelMenu() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void Rebuild() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void UpdateStates() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual gfx::NativeMenu GetNativeMenu() const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual MenuWrapper::MenuAction GetMenuAction() const OVERRIDE {
    NOTIMPLEMENTED();
    return MENU_ACTION_NONE;
  }

  virtual void AddMenuListener(MenuListener* listener) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void RemoveMenuListener(MenuListener* listener) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void SetMinimumWidth(int width) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeMenuAura);
};

// MenuWrapper, public:

// static
MenuWrapper* MenuWrapper::CreateWrapper(Menu2* menu) {
  return new NativeMenuAura(menu);
}

}  // namespace views
