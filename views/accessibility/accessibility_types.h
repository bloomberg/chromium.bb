// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_ACCESSIBILITY_ACCESSIBILITY_TYPES_H_
#define VIEWS_ACCESSIBILITY_ACCESSIBILITY_TYPES_H_
#pragma once

#include "base/basictypes.h"

////////////////////////////////////////////////////////////////////////////////
//
// AccessibilityTypes
//
// Provides enumerations used to preserve platform-independence in accessibility
// functions used in various Views, both in Browser\Views and Views.
//
////////////////////////////////////////////////////////////////////////////////
class AccessibilityTypes {
 public:

  // This defines states of the supported accessibility roles in our
  // Views (e.g. used in View::GetAccessibleState). Any interface using roles
  // must provide a conversion to its own roles (see e.g.
  // ViewAccessibility::get_accState and ViewAccessibility::MSAAState).
  typedef uint32 State;
  enum StateFlag {
    STATE_CHECKED     = 1 << 0,
    STATE_COLLAPSED   = 1 << 1,
    STATE_DEFAULT     = 1 << 2,
    STATE_EXPANDED    = 1 << 3,
    STATE_HASPOPUP    = 1 << 4,
    STATE_HOTTRACKED  = 1 << 5,
    STATE_INVISIBLE   = 1 << 6,
    STATE_LINKED      = 1 << 7,
    STATE_OFFSCREEN   = 1 << 8,
    STATE_PRESSED     = 1 << 9,
    STATE_PROTECTED   = 1 << 10,
    STATE_READONLY    = 1 << 11,
    STATE_SELECTED    = 1 << 12,
    STATE_FOCUSED     = 1 << 13,
    STATE_UNAVAILABLE = 1 << 14
  };

  // This defines an enumeration of the supported accessibility roles in our
  // Views (e.g. used in View::GetAccessibleRole). Any interface using roles
  // must provide a conversion to its own roles (see e.g.
  // ViewAccessibility::get_accRole and ViewAccessibility::MSAARole).
  enum Role {
    ROLE_ALERT,
    ROLE_APPLICATION,
    ROLE_BUTTONDROPDOWN,
    ROLE_BUTTONMENU,
    ROLE_CHECKBUTTON,
    ROLE_CLIENT,
    ROLE_COMBOBOX,
    ROLE_DIALOG,
    ROLE_GRAPHIC,
    ROLE_GROUPING,
    ROLE_LINK,
    ROLE_MENUBAR,
    ROLE_MENUITEM,
    ROLE_MENUPOPUP,
    ROLE_OUTLINE,
    ROLE_OUTLINEITEM,
    ROLE_PAGETAB,
    ROLE_PAGETABLIST,
    ROLE_PANE,
    ROLE_PROGRESSBAR,
    ROLE_PUSHBUTTON,
    ROLE_RADIOBUTTON,
    ROLE_SCROLLBAR,
    ROLE_SEPARATOR,
    ROLE_STATICTEXT,
    ROLE_TEXT,
    ROLE_TITLEBAR,
    ROLE_TOOLBAR,
    ROLE_WINDOW
  };

  // This defines an enumeration of the supported accessibility events in our
  // Views (e.g. used in View::NotifyAccessibilityEvent). Any interface using
  // events must provide a conversion to its own events (see e.g.
  // ViewAccessibility::MSAAEvent).
  enum Event {
    EVENT_ALERT,
    EVENT_FOCUS,
    EVENT_MENUSTART,
    EVENT_MENUEND,
    EVENT_MENUPOPUPSTART,
    EVENT_MENUPOPUPEND,
    EVENT_NAME_CHANGED,
    EVENT_TEXT_CHANGED,
    EVENT_SELECTION_CHANGED,
    EVENT_VALUE_CHANGED
  };

 private:
  // Do not instantiate this class.
  AccessibilityTypes() {}
  ~AccessibilityTypes() {}
};

#endif  // VIEWS_ACCESSIBILITY_ACCESSIBILITY_TYPES_H_
