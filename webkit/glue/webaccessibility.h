// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBACCESSIBILITY_H_
#define WEBKIT_GLUE_WEBACCESSIBILITY_H_

#include <map>
#include <vector>

#include "base/string16.h"
#include "ui/gfx/rect.h"

namespace WebKit {
class WebAccessibilityCache;
class WebAccessibilityObject;
}

namespace webkit_glue {

// A compact representation of the accessibility information for a
// single web object, in a form that can be serialized and sent from
// the renderer process to the browser process.
struct WebAccessibility {
 public:
  // An alphabetical enumeration of accessibility roles.
  enum Role {
    ROLE_NONE = 0,

    ROLE_UNKNOWN,

    ROLE_ALERT,
    ROLE_ALERT_DIALOG,
    ROLE_ANNOTATION,
    ROLE_APPLICATION,
    ROLE_ARTICLE,
    ROLE_BROWSER,
    ROLE_BUSY_INDICATOR,
    ROLE_BUTTON,
    ROLE_CELL,
    ROLE_CHECKBOX,
    ROLE_COLOR_WELL,
    ROLE_COLUMN,
    ROLE_COLUMN_HEADER,
    ROLE_COMBO_BOX,
    ROLE_DEFINITION_LIST_DEFINITION,
    ROLE_DEFINITION_LIST_TERM,
    ROLE_DIALOG,
    ROLE_DIRECTORY,
    ROLE_DISCLOSURE_TRIANGLE,
    ROLE_DOCUMENT,
    ROLE_DRAWER,
    ROLE_EDITABLE_TEXT,
    ROLE_GRID,
    ROLE_GROUP,
    ROLE_GROW_AREA,
    ROLE_HEADING,
    ROLE_HELP_TAG,
    ROLE_IGNORED,
    ROLE_IMAGE,
    ROLE_IMAGE_MAP,
    ROLE_IMAGE_MAP_LINK,
    ROLE_INCREMENTOR,
    ROLE_LANDMARK_APPLICATION,
    ROLE_LANDMARK_BANNER,
    ROLE_LANDMARK_COMPLEMENTARY,
    ROLE_LANDMARK_CONTENTINFO,
    ROLE_LANDMARK_MAIN,
    ROLE_LANDMARK_NAVIGATION,
    ROLE_LANDMARK_SEARCH,
    ROLE_LINK,
    ROLE_LIST,
    ROLE_LISTBOX,
    ROLE_LISTBOX_OPTION,
    ROLE_LIST_ITEM,
    ROLE_LIST_MARKER,
    ROLE_LOG,
    ROLE_MARQUEE,
    ROLE_MATH,
    ROLE_MATTE,
    ROLE_MENU,
    ROLE_MENU_BAR,
    ROLE_MENU_ITEM,
    ROLE_MENU_BUTTON,
    ROLE_MENU_LIST_OPTION,
    ROLE_MENU_LIST_POPUP,
    ROLE_NOTE,
    ROLE_OUTLINE,
    ROLE_POPUP_BUTTON,
    ROLE_PROGRESS_INDICATOR,
    ROLE_RADIO_BUTTON,
    ROLE_RADIO_GROUP,
    ROLE_REGION,
    ROLE_ROW,
    ROLE_ROW_HEADER,
    ROLE_RULER,
    ROLE_RULER_MARKER,
    ROLE_SCROLLAREA,
    ROLE_SCROLLBAR,
    ROLE_SHEET,
    ROLE_SLIDER,
    ROLE_SLIDER_THUMB,
    ROLE_SPLITTER,
    ROLE_SPLIT_GROUP,
    ROLE_STATIC_TEXT,
    ROLE_STATUS,
    ROLE_SYSTEM_WIDE,
    ROLE_TAB,
    ROLE_TABLE,
    ROLE_TABLE_HEADER_CONTAINER,
    ROLE_TAB_GROUP,
    ROLE_TAB_LIST,
    ROLE_TAB_PANEL,
    ROLE_TEXTAREA,
    ROLE_TEXT_FIELD,
    ROLE_TIMER,
    ROLE_TOOLBAR,
    ROLE_TOOLTIP,
    ROLE_TREE,
    ROLE_TREE_GRID,
    ROLE_TREE_ITEM,
    ROLE_VALUE_INDICATOR,
    ROLE_WEBCORE_LINK,
    ROLE_WEB_AREA,
    ROLE_WINDOW,
    NUM_ROLES
  };

  // An alphabetical enumeration of accessibility states.
  // A state bitmask is formed by shifting 1 to the left by each state,
  // for example:
  //   int mask = (1 << STATE_CHECKED) | (1 << STATE_FOCUSED);
  enum State {
    STATE_CHECKED,
    STATE_COLLAPSED,
    STATE_EXPANDED,
    STATE_FOCUSABLE,
    STATE_FOCUSED,
    STATE_HASPOPUP,
    STATE_HOTTRACKED,
    STATE_INDETERMINATE,
    STATE_INVISIBLE,
    STATE_LINKED,
    STATE_MULTISELECTABLE,
    STATE_OFFSCREEN,
    STATE_PRESSED,
    STATE_PROTECTED,
    STATE_READONLY,
    STATE_SELECTABLE,
    STATE_SELECTED,
    STATE_TRAVERSED,
    STATE_BUSY,
    STATE_UNAVAILABLE
  };

  // Additional optional attributes that can be optionally attached to
  // a node.
  enum Attribute {
    // Doc attributes: only make sense when applied to the top-level
    // Document node.
    ATTR_DOC_URL,
    ATTR_DOC_TITLE,
    ATTR_DOC_MIMETYPE,
    ATTR_DOC_DOCTYPE,
    ATTR_DOC_SCROLLX,
    ATTR_DOC_SCROLLY,

    // Editable text attributes
    ATTR_TEXT_SEL_START,
    ATTR_TEXT_SEL_END,

    // Attributes that could apply to any node.
    ATTR_ACTION,
    ATTR_DESCRIPTION,
    ATTR_DISPLAY,
    ATTR_HELP,
    ATTR_HTML_TAG,
    ATTR_SHORTCUT,
    ATTR_URL,
    NUM_ATTRIBUTES
  };

  // Empty constructor, for serialization.
  WebAccessibility();

  // Construct from a WebAccessibilityObject. Recursively creates child
  // nodes as needed to complete the tree. Adds |src| to |cache| and
  // stores its cache ID.
  WebAccessibility(const WebKit::WebAccessibilityObject& src,
                   WebKit::WebAccessibilityCache* cache,
                   bool include_children);

  ~WebAccessibility();

 private:
  // Initialize an already-created struct, same as the constructor above.
  void Init(const WebKit::WebAccessibilityObject& src,
            WebKit::WebAccessibilityCache* cache,
            bool include_children);

  // Returns true if |ancestor| is the first unignored parent of |child|,
  // which means that when walking up the parent chain from |child|,
  // |ancestor| is the *first* ancestor that isn't marked as
  // accessibilityIsIgnored().
  bool IsParentUnignoredOf(const WebKit::WebAccessibilityObject& ancestor,
                           const WebKit::WebAccessibilityObject& child);

 public:
  // This is a simple serializable struct. All member variables should be
  // copyable.
  int32 id;
  string16 name;
  string16 value;
  Role role;
  uint32 state;
  gfx::Rect location;
  std::map<int32, string16> attributes;
  std::vector<WebAccessibility> children;
  std::vector<int32> indirect_child_ids;
  std::vector<std::pair<string16, string16> > html_attributes;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBACCESSIBILITY_H_
