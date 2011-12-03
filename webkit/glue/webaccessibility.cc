// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webaccessibility.h"

#include <set>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityRole.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAttribute.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocumentType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNamedNodeMap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

#ifndef NDEBUG
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityNotification.h"
#endif

using base::DoubleToString;
using base::IntToString;
using WebKit::WebAccessibilityRole;
using WebKit::WebAccessibilityObject;

#ifndef NDEBUG
using WebKit::WebAccessibilityNotification;
#endif

namespace {

std::string IntVectorToString(const std::vector<int>& items) {
  std::string str;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      str += ",";
    str += IntToString(items[i]);
  }
  return str;
}

}  // Anonymous namespace

namespace webkit_glue {

// Provides a conversion between the WebKit::WebAccessibilityRole and a role
// supported on the Browser side. Listed alphabetically by the
// WebAccessibilityRole (except for default role).
WebAccessibility::Role ConvertRole(WebKit::WebAccessibilityRole role) {
  switch (role) {
    case WebKit::WebAccessibilityRoleAnnotation:
      return WebAccessibility::ROLE_ANNOTATION;
    case WebKit::WebAccessibilityRoleApplication:
      return WebAccessibility::ROLE_APPLICATION;
    case WebKit::WebAccessibilityRoleApplicationAlert:
      return WebAccessibility::ROLE_ALERT;
    case WebKit::WebAccessibilityRoleApplicationAlertDialog:
      return WebAccessibility::ROLE_ALERT_DIALOG;
    case WebKit::WebAccessibilityRoleApplicationDialog:
      return WebAccessibility::ROLE_DIALOG;
    case WebKit::WebAccessibilityRoleApplicationLog:
      return WebAccessibility::ROLE_LOG;
    case WebKit::WebAccessibilityRoleApplicationMarquee:
      return WebAccessibility::ROLE_MARQUEE;
    case WebKit::WebAccessibilityRoleApplicationStatus:
      return WebAccessibility::ROLE_STATUS;
    case WebKit::WebAccessibilityRoleApplicationTimer:
      return WebAccessibility::ROLE_TIMER;
    case WebKit::WebAccessibilityRoleBrowser:
      return WebAccessibility::ROLE_BROWSER;
    case WebKit::WebAccessibilityRoleBusyIndicator:
      return WebAccessibility::ROLE_BUSY_INDICATOR;
    case WebKit::WebAccessibilityRoleButton:
      return WebAccessibility::ROLE_BUTTON;
    case WebKit::WebAccessibilityRoleCell:
      return WebAccessibility::ROLE_CELL;
    case WebKit::WebAccessibilityRoleCheckBox:
      return WebAccessibility::ROLE_CHECKBOX;
    case WebKit::WebAccessibilityRoleColorWell:
      return WebAccessibility::ROLE_COLOR_WELL;
    case WebKit::WebAccessibilityRoleColumn:
      return WebAccessibility::ROLE_COLUMN;
    case WebKit::WebAccessibilityRoleColumnHeader:
      return WebAccessibility::ROLE_COLUMN_HEADER;
    case WebKit::WebAccessibilityRoleComboBox:
      return WebAccessibility::ROLE_COMBO_BOX;
    case WebKit::WebAccessibilityRoleDefinitionListDefinition:
      return WebAccessibility::ROLE_DEFINITION_LIST_DEFINITION;
    case WebKit::WebAccessibilityRoleDefinitionListTerm:
      return WebAccessibility::ROLE_DEFINITION_LIST_TERM;
    case WebKit::WebAccessibilityRoleDirectory:
      return WebAccessibility::ROLE_DIRECTORY;
    case WebKit::WebAccessibilityRoleDisclosureTriangle:
      return WebAccessibility::ROLE_DISCLOSURE_TRIANGLE;
    case WebKit::WebAccessibilityRoleDocument:
      return WebAccessibility::ROLE_DOCUMENT;
    case WebKit::WebAccessibilityRoleDocumentArticle:
      return WebAccessibility::ROLE_ARTICLE;
    case WebKit::WebAccessibilityRoleDocumentMath:
      return WebAccessibility::ROLE_MATH;
    case WebKit::WebAccessibilityRoleDocumentNote:
      return WebAccessibility::ROLE_NOTE;
    case WebKit::WebAccessibilityRoleDocumentRegion:
      return WebAccessibility::ROLE_REGION;
    case WebKit::WebAccessibilityRoleDrawer:
      return WebAccessibility::ROLE_DRAWER;
    case WebKit::WebAccessibilityRoleEditableText:
      return WebAccessibility::ROLE_EDITABLE_TEXT;
    case WebKit::WebAccessibilityRoleGrid:
      return WebAccessibility::ROLE_GRID;
    case WebKit::WebAccessibilityRoleGroup:
      return WebAccessibility::ROLE_GROUP;
    case WebKit::WebAccessibilityRoleGrowArea:
      return WebAccessibility::ROLE_GROW_AREA;
    case WebKit::WebAccessibilityRoleHeading:
      return WebAccessibility::ROLE_HEADING;
    case WebKit::WebAccessibilityRoleHelpTag:
      return WebAccessibility::ROLE_HELP_TAG;
    case WebKit::WebAccessibilityRoleIgnored:
      return WebAccessibility::ROLE_IGNORED;
    case WebKit::WebAccessibilityRoleImage:
      return WebAccessibility::ROLE_IMAGE;
    case WebKit::WebAccessibilityRoleImageMap:
      return WebAccessibility::ROLE_IMAGE_MAP;
    case WebKit::WebAccessibilityRoleImageMapLink:
      return WebAccessibility::ROLE_IMAGE_MAP_LINK;
    case WebKit::WebAccessibilityRoleIncrementor:
      return WebAccessibility::ROLE_INCREMENTOR;
    case WebKit::WebAccessibilityRoleLandmarkApplication:
      return WebAccessibility::ROLE_LANDMARK_APPLICATION;
    case WebKit::WebAccessibilityRoleLandmarkBanner:
      return WebAccessibility::ROLE_LANDMARK_BANNER;
    case WebKit::WebAccessibilityRoleLandmarkComplementary:
      return WebAccessibility::ROLE_LANDMARK_COMPLEMENTARY;
    case WebKit::WebAccessibilityRoleLandmarkContentInfo:
      return WebAccessibility::ROLE_LANDMARK_CONTENTINFO;
    case WebKit::WebAccessibilityRoleLandmarkMain:
      return WebAccessibility::ROLE_LANDMARK_MAIN;
    case WebKit::WebAccessibilityRoleLandmarkNavigation:
      return WebAccessibility::ROLE_LANDMARK_NAVIGATION;
    case WebKit::WebAccessibilityRoleLandmarkSearch:
      return WebAccessibility::ROLE_LANDMARK_SEARCH;
    case WebKit::WebAccessibilityRoleLink:
      return WebAccessibility::ROLE_LINK;
    case WebKit::WebAccessibilityRoleList:
      return WebAccessibility::ROLE_LIST;
    case WebKit::WebAccessibilityRoleListBox:
      return WebAccessibility::ROLE_LISTBOX;
    case WebKit::WebAccessibilityRoleListBoxOption:
      return WebAccessibility::ROLE_LISTBOX_OPTION;
    case WebKit::WebAccessibilityRoleListItem:
      return WebAccessibility::ROLE_LIST_ITEM;
    case WebKit::WebAccessibilityRoleListMarker:
      return WebAccessibility::ROLE_LIST_MARKER;
    case WebKit::WebAccessibilityRoleMatte:
      return WebAccessibility::ROLE_MATTE;
    case WebKit::WebAccessibilityRoleMenu:
      return WebAccessibility::ROLE_MENU;
    case WebKit::WebAccessibilityRoleMenuBar:
      return WebAccessibility::ROLE_MENU_BAR;
    case WebKit::WebAccessibilityRoleMenuButton:
      return WebAccessibility::ROLE_MENU_BUTTON;
    case WebKit::WebAccessibilityRoleMenuItem:
      return WebAccessibility::ROLE_MENU_ITEM;
    case WebKit::WebAccessibilityRoleMenuListOption:
      return WebAccessibility::ROLE_MENU_LIST_OPTION;
    case WebKit::WebAccessibilityRoleMenuListPopup:
      return WebAccessibility::ROLE_MENU_LIST_POPUP;
    case WebKit::WebAccessibilityRoleOutline:
      return WebAccessibility::ROLE_OUTLINE;
    case WebKit::WebAccessibilityRolePopUpButton:
      return WebAccessibility::ROLE_POPUP_BUTTON;
    case WebKit::WebAccessibilityRoleProgressIndicator:
      return WebAccessibility::ROLE_PROGRESS_INDICATOR;
    case WebKit::WebAccessibilityRoleRadioButton:
      return WebAccessibility::ROLE_RADIO_BUTTON;
    case WebKit::WebAccessibilityRoleRadioGroup:
      return WebAccessibility::ROLE_RADIO_GROUP;
    case WebKit::WebAccessibilityRoleRow:
      return WebAccessibility::ROLE_ROW;
    case WebKit::WebAccessibilityRoleRowHeader:
      return WebAccessibility::ROLE_ROW_HEADER;
    case WebKit::WebAccessibilityRoleRuler:
      return WebAccessibility::ROLE_RULER;
    case WebKit::WebAccessibilityRoleRulerMarker:
      return WebAccessibility::ROLE_RULER_MARKER;
    case WebKit::WebAccessibilityRoleScrollArea:
      return WebAccessibility::ROLE_SCROLLAREA;
    case WebKit::WebAccessibilityRoleScrollBar:
      return WebAccessibility::ROLE_SCROLLBAR;
    case WebKit::WebAccessibilityRoleSheet:
      return WebAccessibility::ROLE_SHEET;
    case WebKit::WebAccessibilityRoleSlider:
      return WebAccessibility::ROLE_SLIDER;
    case WebKit::WebAccessibilityRoleSliderThumb:
      return WebAccessibility::ROLE_SLIDER_THUMB;
    case WebKit::WebAccessibilityRoleSplitGroup:
      return WebAccessibility::ROLE_SPLIT_GROUP;
    case WebKit::WebAccessibilityRoleSplitter:
      return WebAccessibility::ROLE_SPLITTER;
    case WebKit::WebAccessibilityRoleStaticText:
      return WebAccessibility::ROLE_STATIC_TEXT;
    case WebKit::WebAccessibilityRoleSystemWide:
      return WebAccessibility::ROLE_SYSTEM_WIDE;
    case WebKit::WebAccessibilityRoleTab:
      return WebAccessibility::ROLE_TAB;
    case WebKit::WebAccessibilityRoleTabGroup:
      return WebAccessibility::ROLE_TAB_GROUP;
    case WebKit::WebAccessibilityRoleTabList:
      return WebAccessibility::ROLE_TAB_LIST;
    case WebKit::WebAccessibilityRoleTabPanel:
      return WebAccessibility::ROLE_TAB_PANEL;
    case WebKit::WebAccessibilityRoleTable:
      return WebAccessibility::ROLE_TABLE;
    case WebKit::WebAccessibilityRoleTableHeaderContainer:
      return WebAccessibility::ROLE_TABLE_HEADER_CONTAINER;
    case WebKit::WebAccessibilityRoleTextArea:
      return WebAccessibility::ROLE_TEXTAREA;
    case WebKit::WebAccessibilityRoleTextField:
      return WebAccessibility::ROLE_TEXT_FIELD;
    case WebKit::WebAccessibilityRoleToolbar:
      return WebAccessibility::ROLE_TOOLBAR;
    case WebKit::WebAccessibilityRoleTreeGrid:
      return WebAccessibility::ROLE_TREE_GRID;
    case WebKit::WebAccessibilityRoleTreeItemRole:
      return WebAccessibility::ROLE_TREE_ITEM;
    case WebKit::WebAccessibilityRoleTreeRole:
      return WebAccessibility::ROLE_TREE;
    case WebKit::WebAccessibilityRoleUserInterfaceTooltip:
      return WebAccessibility::ROLE_TOOLTIP;
    case WebKit::WebAccessibilityRoleValueIndicator:
      return WebAccessibility::ROLE_VALUE_INDICATOR;
    case WebKit::WebAccessibilityRoleWebArea:
      return WebAccessibility::ROLE_WEB_AREA;
    case WebKit::WebAccessibilityRoleWebCoreLink:
      return WebAccessibility::ROLE_WEBCORE_LINK;
    case WebKit::WebAccessibilityRoleWindow:
      return WebAccessibility::ROLE_WINDOW;

    default:
      return WebAccessibility::ROLE_UNKNOWN;
  }
}

uint32 ConvertState(const WebAccessibilityObject& o) {
  uint32 state = 0;
  if (o.isChecked())
    state |= (1 << WebAccessibility::STATE_CHECKED);

  if (o.isCollapsed())
    state |= (1 << WebAccessibility::STATE_COLLAPSED);

  if (o.canSetFocusAttribute())
    state |= (1 << WebAccessibility::STATE_FOCUSABLE);

  if (o.isFocused())
    state |= (1 << WebAccessibility::STATE_FOCUSED);

  if (o.roleValue() == WebKit::WebAccessibilityRolePopUpButton ||
      o.ariaHasPopup()) {
    state |= (1 << WebAccessibility::STATE_HASPOPUP);
    if (!o.isCollapsed())
      state |= (1 << WebAccessibility::STATE_EXPANDED);
  }

  if (o.isHovered())
    state |= (1 << WebAccessibility::STATE_HOTTRACKED);

  if (o.isIndeterminate())
    state |= (1 << WebAccessibility::STATE_INDETERMINATE);

  if (!o.isVisible())
    state |= (1 << WebAccessibility::STATE_INVISIBLE);

  if (o.isLinked())
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

  if (o.isRequired())
    state |= (1 << WebAccessibility::STATE_REQUIRED);

  if (o.canSetSelectedAttribute())
    state |= (1 << WebAccessibility::STATE_SELECTABLE);

  if (o.isSelected())
    state |= (1 << WebAccessibility::STATE_SELECTED);

  if (o.isVisited())
    state |= (1 << WebAccessibility::STATE_TRAVERSED);

  if (!o.isEnabled())
    state |= (1 << WebAccessibility::STATE_UNAVAILABLE);

  if (o.isVertical())
    state |= (1 << WebAccessibility::STATE_VERTICAL);

  if (o.isVisited())
    state |= (1 << WebAccessibility::STATE_VISITED);

  return state;
}

WebAccessibility::WebAccessibility()
    : id(-1),
      role(ROLE_UNKNOWN),
      state(-1) {
}

WebAccessibility::WebAccessibility(const WebKit::WebAccessibilityObject& src,
                                   bool include_children) {
  Init(src, include_children);
}

WebAccessibility::~WebAccessibility() {
}

#ifndef NDEBUG
std::string WebAccessibility::DebugString(bool recursive,
                                          int render_routing_id,
                                          int notification) const {
  std::string result;
  static int indent = 0;

  if (render_routing_id != 0) {
    WebKit::WebAccessibilityNotification notification_type =
        static_cast<WebKit::WebAccessibilityNotification>(notification);
    result += "routing id=";
    result += IntToString(render_routing_id);
    result += " notification=";

    switch (notification_type) {
      case WebKit::WebAccessibilityNotificationActiveDescendantChanged:
        result += "active descendant changed";
        break;
      case WebKit::WebAccessibilityNotificationCheckedStateChanged:
        result += "check state changed";
        break;
      case WebKit::WebAccessibilityNotificationChildrenChanged:
        result += "children changed";
        break;
      case WebKit::WebAccessibilityNotificationFocusedUIElementChanged:
        result += "focus changed";
        break;
      case WebKit::WebAccessibilityNotificationLayoutComplete:
        result += "layout complete";
        break;
      case WebKit::WebAccessibilityNotificationLiveRegionChanged:
        result += "live region changed";
        break;
      case WebKit::WebAccessibilityNotificationLoadComplete:
        result += "load complete";
        break;
      case WebKit::WebAccessibilityNotificationMenuListValueChanged:
        result += "menu list changed";
        break;
      case WebKit::WebAccessibilityNotificationRowCountChanged:
        result += "row count changed";
        break;
      case WebKit::WebAccessibilityNotificationRowCollapsed:
        result += "row collapsed";
        break;
      case WebKit::WebAccessibilityNotificationRowExpanded:
        result += "row expanded";
        break;
      case WebKit::WebAccessibilityNotificationScrolledToAnchor:
        result += "scrolled to anchor";
        break;
      case WebKit::WebAccessibilityNotificationSelectedChildrenChanged:
        result += "selected children changed";
        break;
      case WebKit::WebAccessibilityNotificationSelectedTextChanged:
        result += "selected text changed";
        break;
      case WebKit::WebAccessibilityNotificationValueChanged:
        result += "value changed";
        break;
      case WebKit::WebAccessibilityNotificationInvalid:
        result += "invalid notification";
        break;
      default:
        NOTREACHED();
    }
  }

  result += "\n";
  for (int i = 0; i < indent; ++i)
    result += "  ";

  result += "id=" + IntToString(id);

  switch (role) {
    case ROLE_ALERT: result += " ALERT"; break;
    case ROLE_ALERT_DIALOG: result += " ALERT_DIALOG"; break;
    case ROLE_ANNOTATION: result += " ANNOTATION"; break;
    case ROLE_APPLICATION: result += " APPLICATION"; break;
    case ROLE_ARTICLE: result += " ARTICLE"; break;
    case ROLE_BROWSER: result += " BROWSER"; break;
    case ROLE_BUSY_INDICATOR: result += " BUSY_INDICATOR"; break;
    case ROLE_BUTTON: result += " BUTTON"; break;
    case ROLE_CELL: result += " CELL"; break;
    case ROLE_CHECKBOX: result += " CHECKBOX"; break;
    case ROLE_COLOR_WELL: result += " COLOR_WELL"; break;
    case ROLE_COLUMN: result += " COLUMN"; break;
    case ROLE_COLUMN_HEADER: result += " COLUMN_HEADER"; break;
    case ROLE_COMBO_BOX: result += " COMBO_BOX"; break;
    case ROLE_DEFINITION_LIST_DEFINITION: result += " DL_DEFINITION"; break;
    case ROLE_DEFINITION_LIST_TERM: result += " DL_TERM"; break;
    case ROLE_DIALOG: result += " DIALOG"; break;
    case ROLE_DIRECTORY: result += " DIRECTORY"; break;
    case ROLE_DISCLOSURE_TRIANGLE: result += " DISCLOSURE_TRIANGLE"; break;
    case ROLE_DOCUMENT: result += " DOCUMENT"; break;
    case ROLE_DRAWER: result += " DRAWER"; break;
    case ROLE_EDITABLE_TEXT: result += " EDITABLE_TEXT"; break;
    case ROLE_GRID: result += " GRID"; break;
    case ROLE_GROUP: result += " GROUP"; break;
    case ROLE_GROW_AREA: result += " GROW_AREA"; break;
    case ROLE_HEADING: result += " HEADING"; break;
    case ROLE_HELP_TAG: result += " HELP_TAG"; break;
    case ROLE_IGNORED: result += " IGNORED"; break;
    case ROLE_IMAGE: result += " IMAGE"; break;
    case ROLE_IMAGE_MAP: result += " IMAGE_MAP"; break;
    case ROLE_IMAGE_MAP_LINK: result += " IMAGE_MAP_LINK"; break;
    case ROLE_INCREMENTOR: result += " INCREMENTOR"; break;
    case ROLE_LANDMARK_APPLICATION: result += " L_APPLICATION"; break;
    case ROLE_LANDMARK_BANNER: result += " L_BANNER"; break;
    case ROLE_LANDMARK_COMPLEMENTARY: result += " L_COMPLEMENTARY"; break;
    case ROLE_LANDMARK_CONTENTINFO: result += " L_CONTENTINFO"; break;
    case ROLE_LANDMARK_MAIN: result += " L_MAIN"; break;
    case ROLE_LANDMARK_NAVIGATION: result += " L_NAVIGATION"; break;
    case ROLE_LANDMARK_SEARCH: result += " L_SEARCH"; break;
    case ROLE_LINK: result += " LINK"; break;
    case ROLE_LIST: result += " LIST"; break;
    case ROLE_LISTBOX: result += " LISTBOX"; break;
    case ROLE_LISTBOX_OPTION: result += " LISTBOX_OPTION"; break;
    case ROLE_LIST_ITEM: result += " LIST_ITEM"; break;
    case ROLE_LIST_MARKER: result += " LIST_MARKER"; break;
    case ROLE_LOG: result += " LOG"; break;
    case ROLE_MARQUEE: result += " MARQUEE"; break;
    case ROLE_MATH: result += " MATH"; break;
    case ROLE_MATTE: result += " MATTE"; break;
    case ROLE_MENU: result += " MENU"; break;
    case ROLE_MENU_BAR: result += " MENU_BAR"; break;
    case ROLE_MENU_BUTTON: result += " MENU_BUTTON"; break;
    case ROLE_MENU_ITEM: result += " MENU_ITEM"; break;
    case ROLE_MENU_LIST_OPTION: result += " MENU_LIST_OPTION"; break;
    case ROLE_MENU_LIST_POPUP: result += " MENU_LIST_POPUP"; break;
    case ROLE_NOTE: result += " NOTE"; break;
    case ROLE_OUTLINE: result += " OUTLINE"; break;
    case ROLE_POPUP_BUTTON: result += " POPUP_BUTTON"; break;
    case ROLE_PROGRESS_INDICATOR: result += " PROGRESS_INDICATOR"; break;
    case ROLE_RADIO_BUTTON: result += " RADIO_BUTTON"; break;
    case ROLE_RADIO_GROUP: result += " RADIO_GROUP"; break;
    case ROLE_REGION: result += " REGION"; break;
    case ROLE_ROOT_WEB_AREA: result += " ROOT_WEB_AREA"; break;
    case ROLE_ROW: result += " ROW"; break;
    case ROLE_ROW_HEADER: result += " ROW_HEADER"; break;
    case ROLE_RULER: result += " RULER"; break;
    case ROLE_RULER_MARKER: result += " RULER_MARKER"; break;
    case ROLE_SCROLLAREA: result += " SCROLLAREA"; break;
    case ROLE_SCROLLBAR: result += " SCROLLBAR"; break;
    case ROLE_SHEET: result += " SHEET"; break;
    case ROLE_SLIDER: result += " SLIDER"; break;
    case ROLE_SLIDER_THUMB: result += " SLIDER_THUMB"; break;
    case ROLE_SPLITTER: result += " SPLITTER"; break;
    case ROLE_SPLIT_GROUP: result += " SPLIT_GROUP"; break;
    case ROLE_STATIC_TEXT: result += " STATIC_TEXT"; break;
    case ROLE_STATUS: result += " STATUS"; break;
    case ROLE_SYSTEM_WIDE: result += " SYSTEM_WIDE"; break;
    case ROLE_TAB: result += " TAB"; break;
    case ROLE_TABLE: result += " TABLE"; break;
    case ROLE_TABLE_HEADER_CONTAINER: result += " TABLE_HDR_CONTAINER"; break;
    case ROLE_TAB_GROUP: result += " TAB_GROUP"; break;
    case ROLE_TAB_LIST: result += " TAB_LIST"; break;
    case ROLE_TAB_PANEL: result += " TAB_PANEL"; break;
    case ROLE_TEXTAREA: result += " TEXTAREA"; break;
    case ROLE_TEXT_FIELD: result += " TEXT_FIELD"; break;
    case ROLE_TIMER: result += " TIMER"; break;
    case ROLE_TOOLBAR: result += " TOOLBAR"; break;
    case ROLE_TOOLTIP: result += " TOOLTIP"; break;
    case ROLE_TREE: result += " TREE"; break;
    case ROLE_TREE_GRID: result += " TREE_GRID"; break;
    case ROLE_TREE_ITEM: result += " TREE_ITEM"; break;
    case ROLE_UNKNOWN: result += " UNKNOWN"; break;
    case ROLE_VALUE_INDICATOR: result += " VALUE_INDICATOR"; break;
    case ROLE_WEBCORE_LINK: result += " WEBCORE_LINK"; break;
    case ROLE_WEB_AREA: result += " WEB_AREA"; break;
    case ROLE_WINDOW: result += " WINDOW"; break;
    default:
      assert(false);
  }

  if (state & (1 << STATE_BUSY))
    result += " BUSY";
  if (state & (1 << STATE_CHECKED))
    result += " CHECKED";
  if (state & (1 << STATE_COLLAPSED))
    result += " COLLAPSED";
  if (state & (1 << STATE_EXPANDED))
    result += " EXPANDED";
  if (state & (1 << STATE_FOCUSABLE))
    result += " FOCUSABLE";
  if (state & (1 << STATE_FOCUSED))
    result += " FOCUSED";
  if (state & (1 << STATE_HASPOPUP))
    result += " HASPOPUP";
  if (state & (1 << STATE_HOTTRACKED))
    result += " HOTTRACKED";
  if (state & (1 << STATE_INDETERMINATE))
    result += " INDETERMINATE";
  if (state & (1 << STATE_INVISIBLE))
    result += " INVISIBLE";
  if (state & (1 << STATE_LINKED))
    result += " LINKED";
  if (state & (1 << STATE_MULTISELECTABLE))
    result += " MULTISELECTABLE";
  if (state & (1 << STATE_OFFSCREEN))
    result += " OFFSCREEN";
  if (state & (1 << STATE_PRESSED))
    result += " PRESSED";
  if (state & (1 << STATE_PROTECTED))
    result += " PROTECTED";
  if (state & (1 << STATE_READONLY))
    result += " READONLY";
  if (state & (1 << STATE_REQUIRED))
    result += " REQUIRED";
  if (state & (1 << STATE_SELECTABLE))
    result += " SELECTABLE";
  if (state & (1 << STATE_SELECTED))
    result += " SELECTED";
  if (state & (1 << STATE_TRAVERSED))
    result += " TRAVERSED";
  if (state & (1 << STATE_UNAVAILABLE))
    result += " UNAVAILABLE";
  if (state & (1 << STATE_VERTICAL))
    result += " VERTICAL";
  if (state & (1 << STATE_VISITED))
    result += " VISITED";

  std::string tmp = UTF16ToUTF8(name);
  RemoveChars(tmp, "\n", &tmp);
  if (!tmp.empty())
    result += " name=" + tmp;

  tmp = UTF16ToUTF8(value);
  RemoveChars(tmp, "\n", &tmp);
  if (!tmp.empty())
    result += " value=" + tmp;

  result += " (" + IntToString(location.x()) + ", " +
                   IntToString(location.y()) + ")-(" +
                   IntToString(location.width()) + ", " +
                   IntToString(location.height()) + ")";

  for (std::map<IntAttribute, int32>::const_iterator iter =
           int_attributes.begin();
       iter != int_attributes.end();
       ++iter) {
    std::string value = IntToString(iter->second);
    switch (iter->first) {
      case ATTR_SCROLL_X:
        result += " scroll_x=" + value;
        break;
      case ATTR_SCROLL_X_MIN:
        result += " scroll_x_min=" + value;
        break;
      case ATTR_SCROLL_X_MAX:
        result += " scroll_x_max=" + value;
        break;
      case ATTR_SCROLL_Y:
        result += " scroll_y=" + value;
        break;
      case ATTR_SCROLL_Y_MIN:
        result += " scroll_y_min=" + value;
        break;
      case ATTR_SCROLL_Y_MAX:
        result += " scroll_y_max=" + value;
        break;
      case ATTR_HIERARCHICAL_LEVEL:
        result += " level=" + value;
        break;
      case ATTR_TEXT_SEL_START:
        result += " sel_start=" + value;
        break;
      case ATTR_TEXT_SEL_END:
        result += " sel_end=" + value;
        break;
      case ATTR_TABLE_ROW_COUNT:
        result += " rows=" + value;
        break;
      case ATTR_TABLE_COLUMN_COUNT:
        result += " cols=" + value;
        break;
      case ATTR_TABLE_CELL_COLUMN_INDEX:
        result += " col=" + value;
        break;
      case ATTR_TABLE_CELL_ROW_INDEX:
        result += " row=" + value;
        break;
      case ATTR_TABLE_CELL_COLUMN_SPAN:
        result += " colspan=" + value;
        break;
      case ATTR_TABLE_CELL_ROW_SPAN:
        result += " rowspan=" + value;
        break;
    case ATTR_TITLE_UI_ELEMENT:
        result += " title_elem=" + value;
        break;
    }
  }

  for (std::map<StringAttribute, string16>::const_iterator iter =
           string_attributes.begin();
       iter != string_attributes.end();
       ++iter) {
    std::string value = UTF16ToUTF8(iter->second);
    switch (iter->first) {
      case ATTR_DOC_URL:
        result += " doc_url=" + value;
        break;
      case ATTR_DOC_TITLE:
        result += " doc_title=" + value;
        break;
      case ATTR_DOC_MIMETYPE:
        result += " doc_mimetype=" + value;
        break;
      case ATTR_DOC_DOCTYPE:
        result += " doc_doctype=" + value;
        break;
      case ATTR_ACCESS_KEY:
        result += " access_key=" + value;
        break;
      case ATTR_ACTION:
        result += " action=" + value;
        break;
      case ATTR_DESCRIPTION:
        result += " description=" + value;
        break;
      case ATTR_DISPLAY:
        result += " display=" + value;
        break;
      case ATTR_HELP:
        result += " help=" + value;
        break;
      case ATTR_HTML_TAG:
        result += " html_tag=" + value;
        break;
      case ATTR_LIVE_RELEVANT:
        result += " relevant=" + value;
        break;
      case ATTR_LIVE_STATUS:
        result += " live=" + value;
        break;
      case ATTR_CONTAINER_LIVE_RELEVANT:
        result += " container_relevant=" + value;
        break;
      case ATTR_CONTAINER_LIVE_STATUS:
        result += " container_live=" + value;
        break;
      case ATTR_ROLE:
        result += " role=" + value;
        break;
      case ATTR_SHORTCUT:
        result += " shortcut=" + value;
        break;
      case ATTR_URL:
        result += " url=" + value;
        break;
    }
  }

  for (std::map<FloatAttribute, float>::const_iterator iter =
           float_attributes.begin();
       iter != float_attributes.end();
       ++iter) {
    std::string value = DoubleToString(iter->second);
    switch (iter->first) {
      case ATTR_DOC_LOADING_PROGRESS:
        result += " doc_progress=" + value;
        break;
      case ATTR_VALUE_FOR_RANGE:
        result += " value_for_range=" + value;
        break;
      case ATTR_MAX_VALUE_FOR_RANGE:
        result += " max_value=" + value;
        break;
      case ATTR_MIN_VALUE_FOR_RANGE:
        result += " min_value=" + value;
        break;
    }
  }

  for (std::map<BoolAttribute, bool>::const_iterator iter =
           bool_attributes.begin();
       iter != bool_attributes.end();
       ++iter) {
    std::string value = iter->second ? "true" : "false";
    switch (iter->first) {
      case ATTR_DOC_LOADED:
        result += " doc_loaded=" + value;
        break;
      case ATTR_BUTTON_MIXED:
        result += " mixed=" + value;
        break;
      case ATTR_LIVE_ATOMIC:
        result += " atomic=" + value;
        break;
      case ATTR_LIVE_BUSY:
        result += " busy=" + value;
        break;
      case ATTR_CONTAINER_LIVE_ATOMIC:
        result += " container_atomic=" + value;
        break;
      case ATTR_CONTAINER_LIVE_BUSY:
        result += " container_busy=" + value;
        break;
      case ATTR_ARIA_READONLY:
        result += " aria_readonly=" + value;
        break;
    case ATTR_CAN_SET_VALUE:
        result += " can_set_value=" + value;
        break;
    }
  }

  if (!children.empty())
    result += " children=" + IntToString(children.size());

  if (!indirect_child_ids.empty())
    result += " indirect_child_ids=" + IntVectorToString(indirect_child_ids);

  if (!line_breaks.empty())
    result += " line_breaks=" + IntVectorToString(line_breaks);

  if (!cell_ids.empty())
    result += " cell_ids=" + IntVectorToString(cell_ids);

  if (recursive) {
    result += "\n";
    ++indent;
    for (size_t i = 0; i < children.size(); ++i)
      result += children[i].DebugString(true, 0, 0);
    --indent;
  }

  return result;
}
#endif  // ifndef NDEBUG

void WebAccessibility::Init(const WebKit::WebAccessibilityObject& src,
                            bool include_children) {
  name = src.title();
  role = ConvertRole(src.roleValue());
  state = ConvertState(src);
  location = src.boundingBoxRect();
  id = src.axID();

  if (src.valueDescription().length())
    value = src.valueDescription();
  else
    value = src.stringValue();

  if (src.accessKey().length())
    string_attributes[ATTR_ACCESS_KEY] = src.accessKey();
  if (src.actionVerb().length())
    string_attributes[ATTR_ACTION] = src.actionVerb();
  if (src.isAriaReadOnly())
    bool_attributes[ATTR_ARIA_READONLY] = true;
  if (src.isButtonStateMixed())
    bool_attributes[ATTR_BUTTON_MIXED] = true;
  if (src.canSetValueAttribute())
    bool_attributes[ATTR_CAN_SET_VALUE] = true;
  if (src.accessibilityDescription().length())
    string_attributes[ATTR_DESCRIPTION] = src.accessibilityDescription();
  if (src.hasComputedStyle())
    string_attributes[ATTR_DISPLAY] = src.computedStyleDisplay();
  if (src.helpText().length())
    string_attributes[ATTR_HELP] = src.helpText();
  if (src.keyboardShortcut().length())
    string_attributes[ATTR_SHORTCUT] = src.keyboardShortcut();
  if (src.titleUIElement().isValid())
    int_attributes[ATTR_TITLE_UI_ELEMENT] = src.titleUIElement().axID();
  if (!src.url().isEmpty())
    string_attributes[ATTR_URL] = src.url().spec().utf16();

  if (role == ROLE_TREE_ITEM)
    int_attributes[ATTR_HIERARCHICAL_LEVEL] = src.hierarchicalLevel();

  if (role == ROLE_SLIDER)
    include_children = false;

  // Treat the active list box item as focused.
  if (role == ROLE_LISTBOX_OPTION && src.isSelectedOptionActive())
    state |= (1 << WebAccessibility::STATE_FOCUSED);

  WebKit::WebNode node = src.node();
  bool is_iframe = false;

  if (!node.isNull() && node.isElementNode()) {
    WebKit::WebElement element = node.to<WebKit::WebElement>();
    is_iframe = (element.tagName() == ASCIIToUTF16("IFRAME"));

    // TODO(ctguil): The tagName in WebKit is lower cased but
    // HTMLElement::nodeName calls localNameUpper. Consider adding
    // a WebElement method that returns the original lower cased tagName.
    string_attributes[ATTR_HTML_TAG] =
        StringToLowerASCII(string16(element.tagName()));
    for (unsigned i = 0; i < element.attributes().length(); ++i) {
      string16 name = StringToLowerASCII(string16(
          element.attributes().attributeItem(i).localName()));
      string16 value = element.attributes().attributeItem(i).value();
      html_attributes.push_back(std::pair<string16, string16>(name, value));
    }

    if (role == ROLE_EDITABLE_TEXT ||
        role == ROLE_TEXTAREA ||
        role == ROLE_TEXT_FIELD) {
      // Jaws gets confused by children of text fields, so we ignore them.
      include_children = false;

      int_attributes[ATTR_TEXT_SEL_START] = src.selectionStart();
      int_attributes[ATTR_TEXT_SEL_END] = src.selectionEnd();
      WebKit::WebVector<int> src_line_breaks;
      src.lineBreaks(src_line_breaks);
      line_breaks.reserve(src_line_breaks.size());
      for (size_t i = 0; i < src_line_breaks.size(); ++i)
        line_breaks.push_back(src_line_breaks[i]);
    }

    // ARIA role.
    if (element.hasAttribute("role")) {
      string_attributes[ATTR_ROLE] = element.getAttribute("role");
    }

    // Live region attributes
    if (element.hasAttribute("aria-atomic")) {
      bool_attributes[ATTR_LIVE_ATOMIC] =
          LowerCaseEqualsASCII(element.getAttribute("aria-atomic"), "true");
    }
    if (element.hasAttribute("aria-busy")) {
      bool_attributes[ATTR_LIVE_BUSY] =
          LowerCaseEqualsASCII(element.getAttribute("aria-busy"), "true");
    }
    if (element.hasAttribute("aria-live")) {
      string_attributes[ATTR_LIVE_STATUS] = element.getAttribute("aria-live");
    }
    if (element.hasAttribute("aria-relevant")) {
      string_attributes[ATTR_LIVE_RELEVANT] =
          element.getAttribute("aria-relevant");
    }
  }

  // Walk up the parent chain to set live region attributes of containers

  WebKit::WebAccessibilityObject container_accessible = src;
  while (!container_accessible.isNull()) {
    WebKit::WebNode container_node = container_accessible.node();
    if (!container_node.isNull() && container_node.isElementNode()) {
      WebKit::WebElement container_elem =
          container_node.to<WebKit::WebElement>();
      if (container_elem.hasAttribute("aria-atomic") &&
          bool_attributes.find(ATTR_CONTAINER_LIVE_ATOMIC) ==
          bool_attributes.end()) {
        bool_attributes[ATTR_CONTAINER_LIVE_ATOMIC] =
            LowerCaseEqualsASCII(container_elem.getAttribute("aria-atomic"),
                                 "true");
      }
      if (container_elem.hasAttribute("aria-busy") &&
          bool_attributes.find(ATTR_CONTAINER_LIVE_BUSY) ==
          bool_attributes.end()) {
        bool_attributes[ATTR_CONTAINER_LIVE_BUSY] =
            LowerCaseEqualsASCII(container_elem.getAttribute("aria-busy"),
                                 "true");
      }
      if (container_elem.hasAttribute("aria-live") &&
          string_attributes.find(ATTR_CONTAINER_LIVE_STATUS) ==
          string_attributes.end()) {
        string_attributes[ATTR_CONTAINER_LIVE_STATUS] =
            container_elem.getAttribute("aria-live");
      }
      if (container_elem.hasAttribute("aria-relevant") &&
          string_attributes.find(ATTR_CONTAINER_LIVE_RELEVANT) ==
          string_attributes.end()) {
        string_attributes[ATTR_CONTAINER_LIVE_RELEVANT] =
            container_elem.getAttribute("aria-relevant");
      }
    }
    container_accessible = container_accessible.parentObject();
  }

  if (role == WebAccessibility::ROLE_PROGRESS_INDICATOR ||
      role == WebAccessibility::ROLE_SCROLLBAR ||
      role == WebAccessibility::ROLE_SLIDER) {
    float_attributes[ATTR_VALUE_FOR_RANGE] = src.valueForRange();
    float_attributes[ATTR_MAX_VALUE_FOR_RANGE] = src.minValueForRange();
    float_attributes[ATTR_MIN_VALUE_FOR_RANGE] = src.maxValueForRange();
  }

  if (role == WebAccessibility::ROLE_DOCUMENT ||
      role == WebAccessibility::ROLE_WEB_AREA) {
    const WebKit::WebDocument& document = src.document();
    if (name.empty())
      name = document.title();
    string_attributes[ATTR_DOC_TITLE] = document.title();
    string_attributes[ATTR_DOC_URL] = document.url().spec().utf16();
    if (document.isXHTMLDocument())
      string_attributes[ATTR_DOC_MIMETYPE] = WebKit::WebString("text/xhtml");
    else
      string_attributes[ATTR_DOC_MIMETYPE] = WebKit::WebString("text/html");
    bool_attributes[ATTR_DOC_LOADED] = src.isLoaded();
    float_attributes[ATTR_DOC_LOADING_PROGRESS] =
        src.estimatedLoadingProgress();

    const WebKit::WebDocumentType& doctype = document.doctype();
    if (!doctype.isNull())
      string_attributes[ATTR_DOC_DOCTYPE] = doctype.name();

    const gfx::Size& scroll_offset = document.frame()->scrollOffset();
    int_attributes[ATTR_SCROLL_X] = scroll_offset.width();
    int_attributes[ATTR_SCROLL_Y] = scroll_offset.height();

    const gfx::Size& min_offset = document.frame()->minimumScrollOffset();
    int_attributes[ATTR_SCROLL_X_MIN] = min_offset.width();
    int_attributes[ATTR_SCROLL_Y_MIN] = min_offset.height();

    const gfx::Size& max_offset = document.frame()->maximumScrollOffset();
    int_attributes[ATTR_SCROLL_X_MAX] = max_offset.width();
    int_attributes[ATTR_SCROLL_Y_MAX] = max_offset.height();
  }

  if (role == WebAccessibility::ROLE_TABLE) {
    int column_count = src.columnCount();
    int row_count = src.rowCount();
    if (column_count > 0 && row_count > 0) {
      std::set<int> unique_cell_id_set;
      int_attributes[ATTR_TABLE_COLUMN_COUNT] = column_count;
      int_attributes[ATTR_TABLE_ROW_COUNT] = row_count;
      for (int i = 0; i < column_count * row_count; ++i) {
        WebAccessibilityObject cell = src.cellForColumnAndRow(
            i % column_count, i / column_count);
        int cell_id = -1;
        if (!cell.isNull()) {
          cell_id = cell.axID();
          if (unique_cell_id_set.find(cell_id) == unique_cell_id_set.end()) {
            unique_cell_id_set.insert(cell_id);
            unique_cell_ids.push_back(cell_id);
          }
        }
        cell_ids.push_back(cell_id);
      }
    }
  }

  if (role == WebAccessibility::ROLE_CELL ||
      role == WebAccessibility::ROLE_ROW_HEADER ||
      role == WebAccessibility::ROLE_COLUMN_HEADER) {
    int_attributes[ATTR_TABLE_CELL_COLUMN_INDEX] = src.cellColumnIndex();
    int_attributes[ATTR_TABLE_CELL_COLUMN_SPAN] = src.cellColumnSpan();
    int_attributes[ATTR_TABLE_CELL_ROW_INDEX] = src.cellRowIndex();
    int_attributes[ATTR_TABLE_CELL_ROW_SPAN] = src.cellRowSpan();
  }

  if (include_children) {
    // Recursively create children.
    int child_count = src.childCount();
    std::set<int32> child_ids;
    for (int i = 0; i < child_count; ++i) {
      WebAccessibilityObject child = src.childAt(i);
      int32 child_id = child.axID();

      // The child may be invalid due to issues in webkit accessibility code.
      // Don't add children that are invalid thus preventing a crash.
      // https://bugs.webkit.org/show_bug.cgi?id=44149
      // TODO(ctguil): We may want to remove this check as webkit stabilizes.
      if (!child.isValid())
        continue;

      // Children may duplicated in the webkit accessibility tree. Only add a
      // child once for the web accessibility tree.
      // https://bugs.webkit.org/show_bug.cgi?id=58930
      if (child_ids.find(child_id) != child_ids.end())
        continue;
      child_ids.insert(child_id);

      // Some nodes appear in the tree in more than one place: for example,
      // a cell in a table appears as a child of both a row and a column.
      // Only recursively add child nodes that have this node as its
      // unignored parent. For child nodes that are actually parented to
      // somethinng else, store only the ID.
      //
      // As an exception, also add children of an iframe element.
      // https://bugs.webkit.org/show_bug.cgi?id=57066
      if (is_iframe || IsParentUnignoredOf(src, child)) {
        children.push_back(WebAccessibility(child, include_children));
      } else {
        indirect_child_ids.push_back(child_id);
      }
    }
  }
}

bool WebAccessibility::IsParentUnignoredOf(
    const WebKit::WebAccessibilityObject& ancestor,
    const WebKit::WebAccessibilityObject& child) {
  WebKit::WebAccessibilityObject parent = child.parentObject();
  while (!parent.isNull() && parent.accessibilityIsIgnored())
    parent = parent.parentObject();
  return parent.equals(ancestor);
}

}  // namespace webkit_glue
