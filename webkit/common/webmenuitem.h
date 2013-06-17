// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_WEBMENUITEM_H_
#define WEBKIT_COMMON_WEBMENUITEM_H_

#include <vector>

#include "base/strings/string16.h"
#include "third_party/WebKit/public/web/WebMenuItemInfo.h"
#include "webkit/common/webkit_common_export.h"

// Container for information about entries in an HTML select popup menu and
// custom entries of the context menu.
struct WEBKIT_COMMON_EXPORT WebMenuItem {
  enum Type {
    OPTION    = WebKit::WebMenuItemInfo::Option,
    CHECKABLE_OPTION = WebKit::WebMenuItemInfo::CheckableOption,
    GROUP     = WebKit::WebMenuItemInfo::Group,
    SEPARATOR = WebKit::WebMenuItemInfo::Separator,
    SUBMENU  // This is currently only used by Pepper, not by WebKit.
  };

  WebMenuItem();
  WebMenuItem(const WebKit::WebMenuItemInfo& item);
  WebMenuItem(const WebMenuItem& item);
  ~WebMenuItem();

  base::string16 label;
  base::string16 toolTip;
  Type type;
  unsigned action;
  bool rtl;
  bool has_directional_override;
  bool enabled;
  bool checked;
  std::vector<WebMenuItem> submenu;
};

#endif  // WEBKIT_COMMON_WEBMENUITEM_H_
