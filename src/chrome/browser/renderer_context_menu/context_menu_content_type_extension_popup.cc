// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_extension_popup.h"

ContextMenuContentTypeExtensionPopup::ContextMenuContentTypeExtensionPopup(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params)
    : ContextMenuContentType(web_contents, params, false) {
}

ContextMenuContentTypeExtensionPopup::~ContextMenuContentTypeExtensionPopup() {
}

bool ContextMenuContentTypeExtensionPopup::SupportsGroup(int group) {
  switch (group) {
    case ITEM_GROUP_LINK:
    case ITEM_GROUP_MEDIA_IMAGE:
    case ITEM_GROUP_EDITABLE:
    case ITEM_GROUP_COPY:
    case ITEM_GROUP_SEARCH_PROVIDER:
    case ITEM_GROUP_DEVELOPER:
      return ContextMenuContentType::SupportsGroup(group);
    case ITEM_GROUP_ALL_EXTENSION:
      // TODO(lazyboy): Check if it's OK to use
      // ContextMenuContentType::SupportsGroup() in this case too.
      return true;
    default:
      return false;
  }
}
