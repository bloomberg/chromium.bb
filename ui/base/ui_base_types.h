// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_TYPES_H_
#define UI_BASE_UI_BASE_TYPES_H_

#include "ui/base/ui_base_export.h"

namespace ui {

class Event;

// Window "show" state.
enum WindowShowState {
  // A default un-set state.
  SHOW_STATE_DEFAULT = 0,
  SHOW_STATE_NORMAL = 1,
  SHOW_STATE_MINIMIZED = 2,
  SHOW_STATE_MAXIMIZED = 3,
  SHOW_STATE_INACTIVE = 4,  // Views only, not persisted.
  SHOW_STATE_FULLSCREEN = 5,
  SHOW_STATE_END = 6  // The end of show state enum.
};

// Dialog button identifiers used to specify which buttons to show the user.
enum DialogButton {
  DIALOG_BUTTON_NONE   = 0,
  DIALOG_BUTTON_OK     = 1,
  DIALOG_BUTTON_CANCEL = 2,
};

// Specifies the type of modality applied to a window. Different modal
// treatments may be handled differently by the window manager.
enum ModalType {
  MODAL_TYPE_NONE   = 0,  // Window is not modal.
  MODAL_TYPE_WINDOW = 1,  // Window is modal to its transient parent.
  MODAL_TYPE_CHILD  = 2,  // Window is modal to a child of its transient parent.
  MODAL_TYPE_SYSTEM = 3   // Window is modal to all other windows.
};

// TODO(varunjain): Remove MENU_SOURCE_NONE (crbug.com/250964)
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
enum MenuSourceType {
  MENU_SOURCE_NONE = 0,
  MENU_SOURCE_MOUSE = 1,
  MENU_SOURCE_KEYBOARD = 2,
  MENU_SOURCE_TOUCH = 3,
  MENU_SOURCE_TOUCH_EDIT_MENU = 4,
  MENU_SOURCE_LONG_PRESS = 5,
  MENU_SOURCE_LONG_TAP = 6,
  MENU_SOURCE_TOUCH_HANDLE = 7,
  MENU_SOURCE_STYLUS = 8,
  MENU_SOURCE_ADJUST_SELECTION = 9,
  MENU_SOURCE_ADJUST_SELECTION_RESET = 10,
  MENU_SOURCE_TYPE_LAST = MENU_SOURCE_ADJUST_SELECTION_RESET
};

UI_BASE_EXPORT MenuSourceType GetMenuSourceTypeForEvent(const ui::Event& event);

}  // namespace ui

#endif  // UI_BASE_UI_BASE_TYPES_H_
