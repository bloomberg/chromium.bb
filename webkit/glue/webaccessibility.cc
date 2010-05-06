// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webaccessibility.h"

#include "third_party/WebKit/WebKit/chromium/public/WebAccessibilityCache.h"
#include "third_party/WebKit/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/WebKit/chromium/public/WebAccessibilityRole.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebAccessibilityCache;
using WebKit::WebAccessibilityRole;
using WebKit::WebAccessibilityObject;

namespace webkit_glue {

// Provides a conversion between the WebKit::WebAccessibilityRole and a role
// supported on the Browser side. Listed alphabetically by the
// WebAccessibilityRole (except for default role).
WebAccessibility::Role ConvertRole(WebKit::WebAccessibilityRole role) {
  switch (role) {
    case WebKit::WebAccessibilityRoleLandmarkApplication:
      return WebAccessibility::ROLE_APPLICATION;
    case WebKit::WebAccessibilityRoleCell:
      return WebAccessibility::ROLE_CELL;
    case WebKit::WebAccessibilityRoleCheckBox:
      return WebAccessibility::ROLE_CHECKBUTTON;
    case WebKit::WebAccessibilityRoleColumn:
      return WebAccessibility::ROLE_COLUMN;
    case WebKit::WebAccessibilityRoleColumnHeader:
      return WebAccessibility::ROLE_COLUMNHEADER;
    case WebKit::WebAccessibilityRoleDocumentArticle:
    case WebKit::WebAccessibilityRoleWebArea:
      return WebAccessibility::ROLE_DOCUMENT;
    case WebKit::WebAccessibilityRoleImageMap:
    case WebKit::WebAccessibilityRoleImage:
      return WebAccessibility::ROLE_GRAPHIC;
    case WebKit::WebAccessibilityRoleDocumentRegion:
    case WebKit::WebAccessibilityRoleRadioGroup:
    case WebKit::WebAccessibilityRoleGroup:
      return WebAccessibility::ROLE_GROUPING;
    case WebKit::WebAccessibilityRoleLink:
    case WebKit::WebAccessibilityRoleWebCoreLink:
      return WebAccessibility::ROLE_LINK;
    case WebKit::WebAccessibilityRoleList:
      return WebAccessibility::ROLE_LIST;
    case WebKit::WebAccessibilityRoleListBox:
      return WebAccessibility::ROLE_LISTBOX;
    case WebKit::WebAccessibilityRoleListBoxOption:
      return WebAccessibility::ROLE_LISTITEM;
    case WebKit::WebAccessibilityRoleMenuBar:
      return WebAccessibility::ROLE_MENUBAR;
    case WebKit::WebAccessibilityRoleMenuButton:
    case WebKit::WebAccessibilityRoleMenuItem:
      return WebAccessibility::ROLE_MENUITEM;
    case WebKit::WebAccessibilityRoleMenu:
      return WebAccessibility::ROLE_MENUPOPUP;
    case WebKit::WebAccessibilityRoleOutline:
      return WebAccessibility::ROLE_OUTLINE;
    case WebKit::WebAccessibilityRoleTabGroup:
      return WebAccessibility::ROLE_PAGETABLIST;
    case WebKit::WebAccessibilityRoleProgressIndicator:
      return WebAccessibility::ROLE_PROGRESSBAR;
    case WebKit::WebAccessibilityRoleButton:
      return WebAccessibility::ROLE_PUSHBUTTON;
    case WebKit::WebAccessibilityRoleRadioButton:
      return WebAccessibility::ROLE_RADIOBUTTON;
    case WebKit::WebAccessibilityRoleRow:
      return WebAccessibility::ROLE_ROW;
    case WebKit::WebAccessibilityRoleRowHeader:
      return WebAccessibility::ROLE_ROWHEADER;
    case WebKit::WebAccessibilityRoleSplitter:
      return WebAccessibility::ROLE_SEPARATOR;
    case WebKit::WebAccessibilityRoleSlider:
      return WebAccessibility::ROLE_SLIDER;
    case WebKit::WebAccessibilityRoleStaticText:
      return WebAccessibility::ROLE_STATICTEXT;
    case WebKit::WebAccessibilityRoleApplicationStatus:
      return WebAccessibility::ROLE_STATUSBAR;
    case WebKit::WebAccessibilityRoleTable:
      return WebAccessibility::ROLE_TABLE;
    case WebKit::WebAccessibilityRoleListMarker:
    case WebKit::WebAccessibilityRoleTextField:
    case WebKit::WebAccessibilityRoleTextArea:
      return WebAccessibility::ROLE_TEXT;
    case WebKit::WebAccessibilityRoleToolbar:
      return WebAccessibility::ROLE_TOOLBAR;
    case WebKit::WebAccessibilityRoleUserInterfaceTooltip:
      return WebAccessibility::ROLE_TOOLTIP;
    case WebKit::WebAccessibilityRoleDocument:
    case WebKit::WebAccessibilityRoleUnknown:
    default:
        // This is the default role.
      return WebAccessibility::ROLE_CLIENT;
  }
}

uint32 ConvertState(const WebAccessibilityObject& o) {
  uint32 state = 0;
  if (o.isChecked())
    state |= (1 << WebAccessibility::STATE_CHECKED);

  if (o.canSetFocusAttribute())
    state |= (1 << WebAccessibility::STATE_FOCUSABLE);

  if (o.isFocused())
    state |= (1 << WebAccessibility::STATE_FOCUSED);

  if (o.isHovered())
    state |= (1 << WebAccessibility::STATE_HOTTRACKED);

  if (o.isIndeterminate())
    state |= (1 << WebAccessibility::STATE_INDETERMINATE);

  if (o.isAnchor())
    state |= (1 << WebAccessibility::STATE_LINKED);

  if (o.isMultiSelectable())
    state |= (1 << WebAccessibility::STATE_MULTISELECTABLE);

  if (o.isOffScreen())
    state |= (1 << WebAccessibility::STATE_OFFSCREEN);

  if (o.isPressed())
    state |= (1 << WebAccessibility::STATE_PRESSED);

  if (o.isPasswordField())
    state |= (1 << WebAccessibility::STATE_PROTECTED);

  if (o.isReadOnly())
    state |= (1 << WebAccessibility::STATE_READONLY);

  if (o.isVisited())
    state |= (1 << WebAccessibility::STATE_TRAVERSED);

  if (!o.isEnabled())
    state |= (1 << WebAccessibility::STATE_UNAVAILABLE);

  return state;
}

WebAccessibility::WebAccessibility()
    : id(-1),
      role(ROLE_NONE),
      state(-1) {
}

WebAccessibility::WebAccessibility(const WebKit::WebAccessibilityObject& src,
                                   WebKit::WebAccessibilityCache* cache) {
  Init(src, cache);
}

void WebAccessibility::Init(const WebKit::WebAccessibilityObject& src,
                            WebKit::WebAccessibilityCache* cache) {
  name = src.title();
  value = src.stringValue();
  action = src.actionVerb();
  description = src.accessibilityDescription();
  help = src.helpText();
  shortcut = src.keyboardShortcut();
  role = ConvertRole(src.roleValue());
  state = ConvertState(src);
  location = src.boundingBoxRect();

  // Add the source object to the cache and store its id.
  id = cache->addOrGetId(src);

  // Recursively create children.
  int child_count = src.childCount();
  children.resize(child_count);
  for (int i = 0; i < child_count; i++) {
    children[i].Init(src.childAt(i), cache);
  }
}

}  // namespace webkit_glue
