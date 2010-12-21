// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin_delegate.h"

namespace webkit {
namespace npapi {

bool WebPluginDelegate::StartFind(const string16& search_text,
                                  bool case_sensitive,
                                  int identifier) {
  return false;
}

NPWidgetExtensions* WebPluginDelegate::GetWidgetExtensions() {
  return NULL;
}

bool WebPluginDelegate::SetCursor(NPCursorType type) {
  return false;
}

NPFontExtensions* WebPluginDelegate::GetFontExtensions() {
  return NULL;
}

bool WebPluginDelegate::HasSelection() const {
  return false;
}

string16 WebPluginDelegate::GetSelectionAsText() const {
  return string16();
}

string16 WebPluginDelegate::GetSelectionAsMarkup() const {
  return string16();
}

}  // namespace npapi
}  // namespace webkit
