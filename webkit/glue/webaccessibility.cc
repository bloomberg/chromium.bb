// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webaccessibility.h"

#include <set>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityCache.h"
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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

using WebKit::WebAccessibilityCache;
using WebKit::WebAccessibilityRole;
using WebKit::WebAccessibilityObject;

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

  if (o.roleValue() == WebKit::WebAccessibilityRolePopUpButton) {
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

  if (o.canSetSelectedAttribute())
    state |= (1 << WebAccessibility::STATE_SELECTABLE);

  if (o.isSelected())
    state |= (1 << WebAccessibility::STATE_SELECTED);

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
                                   WebKit::WebAccessibilityCache* cache,
                                   bool include_children) {
  Init(src, cache, include_children);
}

WebAccessibility::~WebAccessibility() {
}

void WebAccessibility::Init(const WebKit::WebAccessibilityObject& src,
                            WebKit::WebAccessibilityCache* cache,
                            bool include_children) {
  name = src.title();
  value = src.stringValue();
  role = ConvertRole(src.roleValue());
  state = ConvertState(src);
  location = src.boundingBoxRect();

  if (src.actionVerb().length())
    attributes[ATTR_ACTION] = src.actionVerb();
  if (src.accessibilityDescription().length())
    attributes[ATTR_DESCRIPTION] = src.accessibilityDescription();
  if (src.helpText().length())
    attributes[ATTR_HELP] = src.helpText();
  if (src.keyboardShortcut().length())
    attributes[ATTR_SHORTCUT] = src.keyboardShortcut();
  if (src.hasComputedStyle())
    attributes[ATTR_DISPLAY] = src.computedStyleDisplay();
  if (!src.url().isEmpty())
    attributes[ATTR_URL] = src.url().spec().utf16();

  WebKit::WebNode node = src.node();
  bool is_iframe = false;

  if (!node.isNull() && node.isElementNode()) {
    WebKit::WebElement element = node.to<WebKit::WebElement>();
    is_iframe = (element.tagName() == ASCIIToUTF16("IFRAME"));

    // TODO(ctguil): The tagName in WebKit is lower cased but
    // HTMLElement::nodeName calls localNameUpper. Consider adding
    // a WebElement method that returns the original lower cased tagName.
    attributes[ATTR_HTML_TAG] = StringToLowerASCII(string16(element.tagName()));
    for (unsigned i = 0; i < element.attributes().length(); i++) {
      html_attributes.push_back(
          std::pair<string16, string16>(
              element.attributes().attributeItem(i).localName(),
              element.attributes().attributeItem(i).value()));
    }

    if (element.isFormControlElement()) {
      WebKit::WebFormControlElement form_element =
          element.to<WebKit::WebFormControlElement>();
      if (form_element.formControlType() == ASCIIToUTF16("text")) {
        WebKit::WebInputElement input_element =
            form_element.to<WebKit::WebInputElement>();
        attributes[ATTR_TEXT_SEL_START] = base::IntToString16(
            input_element.selectionStart());
        attributes[ATTR_TEXT_SEL_END] = base::IntToString16(
            input_element.selectionEnd());
      }
    }
  }

  if (role == WebAccessibility::ROLE_DOCUMENT ||
      role == WebAccessibility::ROLE_WEB_AREA) {
    const WebKit::WebDocument& document = src.document();
    if (name.empty())
      name = document.title();
    attributes[ATTR_DOC_TITLE] = document.title();
    attributes[ATTR_DOC_URL] = document.frame()->url().spec().utf16();
    if (document.isXHTMLDocument())
      attributes[ATTR_DOC_MIMETYPE] = WebKit::WebString("text/xhtml");
    else
      attributes[ATTR_DOC_MIMETYPE] = WebKit::WebString("text/html");

    const WebKit::WebDocumentType& doctype = document.doctype();
    if (!doctype.isNull())
      attributes[ATTR_DOC_DOCTYPE] = doctype.name();

    const gfx::Size& scroll_offset = document.frame()->scrollOffset();
    attributes[ATTR_DOC_SCROLLX] = base::IntToString16(scroll_offset.width());
    attributes[ATTR_DOC_SCROLLY] = base::IntToString16(scroll_offset.height());
  }

  // Add the source object to the cache and store its id.
  id = cache->addOrGetId(src);

  if (include_children) {
    // Recursively create children.
    int child_count = src.childCount();
    std::set<int32> child_ids;
    for (int i = 0; i < child_count; i++) {
      WebAccessibilityObject child = src.childAt(i);
      int32 child_id = cache->addOrGetId(child);

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
        children.push_back(WebAccessibility(child, cache, include_children));
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
