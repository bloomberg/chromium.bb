// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBMENUITEM_H_
#define WEBMENUITEM_H_

#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenuInfo.h"

// Container for information about entries in an HTML select popup menu.
struct WebMenuItem {
  enum Type {
    OPTION    = WebKit::WebPopupMenuInfo::Item::Option,
    GROUP     = WebKit::WebPopupMenuInfo::Item::Group,
    SEPARATOR = WebKit::WebPopupMenuInfo::Item::Separator
  };

  string16 label;
  Type type;
  bool enabled;

  WebMenuItem() : type(OPTION), enabled(false) {
  }

  WebMenuItem(const WebKit::WebPopupMenuInfo::Item& item)
      : label(item.label),
        type(static_cast<Type>(item.type)),
        enabled(item.enabled) {
  }
};

#endif  // WEBMENUITEM_H_
