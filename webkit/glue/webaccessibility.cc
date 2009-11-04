// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webaccessibility.h"

#include "webkit/api/public/WebAccessibilityCache.h"
#include "webkit/api/public/WebAccessibilityObject.h"
#include "webkit/api/public/WebAccessibilityRole.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebString.h"

using WebKit::WebAccessibilityCache;
using WebKit::WebAccessibilityRole;
using WebKit::WebAccessibilityObject;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebString;

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

long ConvertState(const WebAccessibilityObject& o) {
  long state = 0;
  if (o.isChecked())
    state |= static_cast<long>(1 << WebAccessibility::STATE_CHECKED);

  if (o.canSetFocusAttribute())
    state |= static_cast<long>(1 << WebAccessibility::STATE_FOCUSABLE);

  if (o.isFocused())
    state |= static_cast<long>(1 << WebAccessibility::STATE_FOCUSED);

  if (o.isHovered())
    state |= static_cast<long>(1 << WebAccessibility::STATE_HOTTRACKED);

  if (o.isIndeterminate())
    state |= static_cast<long>(1 << WebAccessibility::STATE_INDETERMINATE);

  if (o.isAnchor())
    state |= static_cast<long>(1 << WebAccessibility::STATE_LINKED);

  if (o.isMultiSelect())
    state |= static_cast<long>(1 << WebAccessibility::STATE_MULTISELECTABLE);

  if (o.isOffScreen())
    state |= static_cast<long>(1 << WebAccessibility::STATE_OFFSCREEN);

  if (o.isPressed())
    state |= static_cast<long>(1 << WebAccessibility::STATE_PRESSED);

  if (o.isPasswordField())
    state |= static_cast<long>(1 << WebAccessibility::STATE_PROTECTED);

  if (o.isReadOnly())
    state |= static_cast<long>(1 << WebAccessibility::STATE_READONLY);

  if (o.isVisited())
    state |= static_cast<long>(1 << WebAccessibility::STATE_TRAVERSED);

  if (!o.isEnabled())
    state |= static_cast<long>(1 << WebAccessibility::STATE_UNAVAILABLE);

  return state;
}


bool WebAccessibility::GetAccObjInfo(WebAccessibilityCache* cache,
  const WebAccessibility::InParams& in_params,
  WebAccessibility::OutParams* out_params) {

  // Find object requested by |object_id|.
  WebAccessibilityObject active_acc_obj;

  // Since ids assigned by Chrome starts at 1000, whereas platform-specific ids
  // used to reference a child will be in a wholly different range, we know
  // that any id that high should be treated as a non-direct descendant.
  bool local_child = false;
  if (cache->isValidId(in_params.child_id)) {
    // Object is not a direct child, re-map the input parameters accordingly.
    // The object to be retrieved is referred to by the |in_params.child_id|, as
    // a result of e.g. a focus event.
    active_acc_obj = cache->getObjectById(in_params.child_id);
  } else {
    local_child = true;

    active_acc_obj = cache->getObjectById(in_params.object_id);
    if (active_acc_obj.isNull())
      return false;

    // child_id == 0 means self. Otherwise, it's a local child - 1.
    if (in_params.child_id > 0) {
      unsigned index = in_params.child_id - 1;
      if (index >= active_acc_obj.childCount())
        return false;

      active_acc_obj = active_acc_obj.childAt(index);
    }
  }

  if (active_acc_obj.isNull())
    return false;

  // Temp paramters for holding output information.
  WebAccessibilityObject out_acc_obj;
  string16 out_string;

  switch (in_params.function_id) {
    case WebAccessibility::FUNCTION_DODEFAULTACTION: {
      if (!active_acc_obj.performDefaultAction())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_HITTEST: {
      WebPoint point(in_params.input_long1, in_params.input_long2);
      out_acc_obj = active_acc_obj.hitTest(point);
      if (out_acc_obj.isNull())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_LOCATION: {
      WebRect rect = active_acc_obj.boundingBoxRect();
      out_params->output_long1 = rect.x;
      out_params->output_long2 = rect.y;
      out_params->output_long3 = rect.width;
      out_params->output_long4 = rect.height;
      break;
    }
    case WebAccessibility::FUNCTION_NAVIGATE: {
      WebAccessibility::Direction dir =
        static_cast<WebAccessibility::Direction>(in_params.input_long1);
      switch (dir) {
        case WebAccessibility::DIRECTION_DOWN:
        case WebAccessibility::DIRECTION_UP:
        case WebAccessibility::DIRECTION_LEFT:
        case WebAccessibility::DIRECTION_RIGHT:
          // These directions are not implemented, matching Mozilla and IE.
          return false;
        case WebAccessibility::DIRECTION_LASTCHILD:
        case WebAccessibility::DIRECTION_FIRSTCHILD:
          // MSDN states that navigating to first/last child can only be from
          // self.
          if (!local_child)
            return false;

          if (dir == WebAccessibility::DIRECTION_FIRSTCHILD) {
            out_acc_obj = active_acc_obj.firstChild();
          } else {
            out_acc_obj = active_acc_obj.lastChild();
          }
          break;
        case WebAccessibility::DIRECTION_NEXT:
        case WebAccessibility::DIRECTION_PREVIOUS: {
          if (dir == WebAccessibility::DIRECTION_NEXT) {
            out_acc_obj = active_acc_obj.nextSibling();
          } else {
            out_acc_obj = active_acc_obj.previousSibling();
          }
          break;
        }
        default:
          return false;
      }
      if (out_acc_obj.isNull())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_GETCHILD: {
      out_params->object_id = in_params.object_id;
      out_acc_obj = active_acc_obj;
      break;
    }
    case WebAccessibility::FUNCTION_CHILDCOUNT: {
      out_params->output_long1 = active_acc_obj.childCount();
      break;
    }
    case WebAccessibility::FUNCTION_DEFAULTACTION: {
      out_string = active_acc_obj.actionVerb();
      if (out_string.empty())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_DESCRIPTION: {
      out_string = active_acc_obj.accessibilityDescription();
      if (out_string.empty())
        return false;
      // From the Mozilla MSAA implementation:
      // "Signal to screen readers that this description is speakable and is not
      // a formatted positional information description. Don't localize the
      // 'Description: ' part of this string, it will be parsed out by assistive
      // technologies."
      out_string = L"Description: " + out_string;
      break;
    }
    case WebAccessibility::FUNCTION_GETFOCUSEDCHILD: {
      out_acc_obj = active_acc_obj.focusedChild();
      if (out_acc_obj.isNull())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_HELPTEXT: {
      out_string = active_acc_obj.helpText();
      if (out_string.empty())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_KEYBOARDSHORTCUT: {
      out_string = active_acc_obj.keyboardShortcut();
      if (out_string.empty())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_NAME: {
      out_string = active_acc_obj.title();
      if (out_string.empty())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_GETPARENT: {
      out_acc_obj = active_acc_obj.parentObject();
      if (out_acc_obj.isNull())
        return false;
      break;
    }
    case WebAccessibility::FUNCTION_ROLE: {
      out_params->output_long1 = ConvertRole(active_acc_obj.roleValue());
      break;
    }
    case WebAccessibility::FUNCTION_STATE: {
      out_params->output_long1 = ConvertState(active_acc_obj);
      break;
    }
    case WebAccessibility::FUNCTION_VALUE: {
      out_string = active_acc_obj.stringValue();
      if (out_string.empty())
        return false;
      break;
    }
    default:
      // Non-supported function id.
      return false;
  }

  // Output and hashmap assignments, as appropriate.
  if (!out_string.empty())
    out_params->output_string = out_string;

  if (out_acc_obj.isNull())
    return true;

  int id = cache->addOrGetId(out_acc_obj);
  out_params->object_id = id;
  out_params->output_long1 = -1;

  // TODO(klink): Handle simple objects returned.
  return true;
}

}  // namespace webkit_glue
