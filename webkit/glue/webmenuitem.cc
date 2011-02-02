// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webmenuitem.h"

WebMenuItem::WebMenuItem()
    : type(OPTION),
      action(0),
      enabled(false),
      checked(false) {
}

WebMenuItem::WebMenuItem(const WebKit::WebMenuItemInfo& item)
    : label(item.label),
      type(static_cast<Type>(item.type)),
      action(item.action),
      enabled(item.enabled),
      checked(item.checked) {
}

WebMenuItem::WebMenuItem(const WebMenuItem& item)
    : label(item.label),
      type(item.type),
      action(item.action),
      enabled(item.enabled),
      checked(item.checked),
      submenu(item.submenu) {
}

WebMenuItem::~WebMenuItem() {}
