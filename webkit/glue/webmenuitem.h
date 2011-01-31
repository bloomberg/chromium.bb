// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBMENUITEM_H_
#define WEBMENUITEM_H_

#include <vector>

#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMenuItemInfo.h"

// Container for information about entries in an HTML select popup menu and
// custom entries of the context menu.
struct WebMenuItem {
  enum Type {
    OPTION    = WebKit::WebMenuItemInfo::Option,
    CHECKABLE_OPTION = WebKit::WebMenuItemInfo::CheckableOption,
    GROUP     = WebKit::WebMenuItemInfo::Group,
    SEPARATOR = WebKit::WebMenuItemInfo::Separator,
    SUBMENU  // This is currently only used by Pepper, not by WebKit.
  };

  string16 label;
  Type type;
  unsigned action;
  bool enabled;
  bool checked;
  std::vector<WebMenuItem> submenu;

  WebMenuItem() : type(OPTION), action(0), enabled(false), checked(false) {
  }

  WebMenuItem(const WebKit::WebMenuItemInfo& item)
      : label(item.label),
        type(static_cast<Type>(item.type)),
        action(item.action),
        enabled(item.enabled),
        checked(item.checked) {
  }
};

#endif  // WEBMENUITEM_H_
