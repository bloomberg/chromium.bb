// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_ACCESSIBILITY_ACCESSIBILITY_TYPES_H_
#define VIEWS_ACCESSIBILITY_ACCESSIBILITY_TYPES_H_

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
    STATE_HASPOPUP    = 1 << 1,
    STATE_LINKED      = 1 << 2,
    STATE_PROTECTED   = 1 << 3,
    STATE_READONLY    = 1 << 4
  };

  // This defines an enumeration of the supported accessibility roles in our
  // Views (e.g. used in View::GetAccessibleRole). Any interface using roles
  // must provide a conversion to its own roles (see e.g.
  // ViewAccessibility::get_accRole and ViewAccessibility::MSAARole).
  enum Role {
    ROLE_APPLICATION,
    ROLE_BUTTONDROPDOWN,
    ROLE_BUTTONMENU,
    ROLE_CHECKBUTTON,
    ROLE_CLIENT,
    ROLE_COMBOBOX,
    ROLE_GRAPHIC,
    ROLE_GROUPING,
    ROLE_LINK,
    ROLE_MENUITEM,
    ROLE_MENUPOPUP,
    ROLE_OUTLINE,
    ROLE_OUTLINEITEM,
    ROLE_PAGETAB,
    ROLE_PAGETABLIST,
    ROLE_PANE,
    ROLE_PROGRESSBAR,
    ROLE_PUSHBUTTON,
    ROLE_SCROLLBAR,
    ROLE_SEPARATOR,
    ROLE_STATICTEXT,
    ROLE_TEXT,
    ROLE_TITLEBAR,
    ROLE_TOOLBAR,
    ROLE_WINDOW
  };

 private:
  // Do not instantiate this class.
  AccessibilityTypes() {}
  ~AccessibilityTypes() {}
};

#endif  // VIEWS_ACCESSIBILITY_ACCESSIBILITY_TYPES_H_
