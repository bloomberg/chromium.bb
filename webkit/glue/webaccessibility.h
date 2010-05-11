// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBACCESSIBILITY_H_
#define WEBKIT_GLUE_WEBACCESSIBILITY_H_

#include <vector>

#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/WebKit/chromium/public/WebAccessibilityRole.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"

namespace WebKit {
class WebAccessibilityCache;
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

    ROLE_APPLICATION,
    ROLE_CELL,
    ROLE_CHECKBUTTON,
    ROLE_CLIENT,
    ROLE_COLUMN,
    ROLE_COLUMNHEADER,
    ROLE_DOCUMENT,
    ROLE_GRAPHIC,
    ROLE_GROUPING,
    ROLE_LINK,
    ROLE_LIST,
    ROLE_LISTBOX,
    ROLE_LISTITEM,
    ROLE_MENUBAR,
    ROLE_MENUITEM,
    ROLE_MENUPOPUP,
    ROLE_OUTLINE,
    ROLE_PAGETABLIST,
    ROLE_PROGRESSBAR,
    ROLE_PUSHBUTTON,
    ROLE_RADIOBUTTON,
    ROLE_ROW,
    ROLE_ROWHEADER,
    ROLE_SEPARATOR,
    ROLE_SLIDER,
    ROLE_STATICTEXT,
    ROLE_STATUSBAR,
    ROLE_TABLE,
    ROLE_TEXT,
    ROLE_TOOLBAR,
    ROLE_TOOLTIP,
    NUM_ROLES
  };

  // An alphabetical enumeration of accessibility states.
  // A state bitmask is formed by shifting 1 to the left by each state,
  // for example:
  //   int mask = (1 << STATE_CHECKED) | (1 << STATE_FOCUSED);
  enum State {
    STATE_CHECKED,
    STATE_FOCUSABLE,
    STATE_FOCUSED,
    STATE_HOTTRACKED,
    STATE_INDETERMINATE,
    STATE_LINKED,
    STATE_MULTISELECTABLE,
    STATE_OFFSCREEN,
    STATE_PRESSED,
    STATE_PROTECTED,
    STATE_READONLY,
    STATE_TRAVERSED,
    STATE_UNAVAILABLE
  };

  // Empty constructor, for serialization.
  WebAccessibility();

  // Construct from a WebAccessibilityObject. Recursively creates child
  // nodes as needed to complete the tree. Adds |src| to |cache| and
  // stores its cache ID.
  WebAccessibility(const WebKit::WebAccessibilityObject& src,
                   WebKit::WebAccessibilityCache* cache);

  // Initialize an already-created struct, same as the constructor a
  void Init(const WebKit::WebAccessibilityObject& src,
            WebKit::WebAccessibilityCache* cache);

  // This is a simple serializable struct. All member variables should be
  // copyable.
  int32 id;
  string16 name;
  string16 value;
  string16 action;
  string16 description;
  string16 help;
  string16 shortcut;
  Role role;
  uint32 state;
  WebKit::WebRect location;
  std::vector<WebAccessibility> children;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBACCESSIBILITY_H_
