// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include <set>

#include "base/containers/hash_tables.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

using base::DoubleToString;
using base::IntToString;

namespace ui {

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

AXNodeData::AXNodeData()
    : id(-1),
      role(AX_ROLE_UNKNOWN),
      state(-1) {
}

AXNodeData::~AXNodeData() {
}

void AXNodeData::AddStringAttribute(
    AXStringAttribute attribute, const std::string& value) {
  string_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddIntAttribute(
    AXIntAttribute attribute, int value) {
  int_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddFloatAttribute(
    AXFloatAttribute attribute, float value) {
  float_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddBoolAttribute(
    AXBoolAttribute attribute, bool value) {
  bool_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddIntListAttribute(
    AXIntListAttribute attribute, const std::vector<int32>& value) {
  intlist_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::SetName(std::string name) {
  string_attributes.push_back(std::make_pair(AX_ATTR_NAME, name));
}

void AXNodeData::SetValue(std::string value) {
  string_attributes.push_back(std::make_pair(AX_ATTR_VALUE, value));
}

std::string AXNodeData::ToString() const {
  std::string result;

  result += "id=" + IntToString(id);

  switch (role) {
    case ui::AX_ROLE_ALERT: result += " ALERT"; break;
    case ui::AX_ROLE_ALERT_DIALOG: result += " ALERT_DIALOG"; break;
    case ui::AX_ROLE_ANNOTATION: result += " ANNOTATION"; break;
    case ui::AX_ROLE_APPLICATION: result += " APPLICATION"; break;
    case ui::AX_ROLE_ARTICLE: result += " ARTICLE"; break;
    case ui::AX_ROLE_BANNER: result += " BANNER"; break;
    case ui::AX_ROLE_BROWSER: result += " BROWSER"; break;
    case ui::AX_ROLE_BUSY_INDICATOR: result += " BUSY_INDICATOR"; break;
    case ui::AX_ROLE_BUTTON: result += " BUTTON"; break;
    case ui::AX_ROLE_CANVAS: result += " CANVAS"; break;
    case ui::AX_ROLE_CELL: result += " CELL"; break;
    case ui::AX_ROLE_CHECK_BOX: result += " CHECKBOX"; break;
    case ui::AX_ROLE_COLOR_WELL: result += " COLOR_WELL"; break;
    case ui::AX_ROLE_COLUMN: result += " COLUMN"; break;
    case ui::AX_ROLE_COLUMN_HEADER: result += " COLUMN_HEADER"; break;
    case ui::AX_ROLE_COMBO_BOX: result += " COMBO_BOX"; break;
    case ui::AX_ROLE_COMPLEMENTARY: result += " COMPLEMENTARY"; break;
    case ui::AX_ROLE_CONTENT_INFO: result += " CONTENTINFO"; break;
    case ui::AX_ROLE_DEFINITION: result += " DEFINITION"; break;
    case ui::AX_ROLE_DESCRIPTION_LIST_DETAIL: result += " DD"; break;
    case ui::AX_ROLE_DESCRIPTION_LIST_TERM: result += " DT"; break;
    case ui::AX_ROLE_DIALOG: result += " DIALOG"; break;
    case ui::AX_ROLE_DIRECTORY: result += " DIRECTORY"; break;
    case ui::AX_ROLE_DISCLOSURE_TRIANGLE:
        result += " DISCLOSURE_TRIANGLE"; break;
    case ui::AX_ROLE_DIV: result += " DIV"; break;
    case ui::AX_ROLE_DOCUMENT: result += " DOCUMENT"; break;
    case ui::AX_ROLE_DRAWER: result += " DRAWER"; break;
    case ui::AX_ROLE_EDITABLE_TEXT: result += " EDITABLE_TEXT"; break;
    case ui::AX_ROLE_FOOTER: result += " FOOTER"; break;
    case ui::AX_ROLE_FORM: result += " FORM"; break;
    case ui::AX_ROLE_GRID: result += " GRID"; break;
    case ui::AX_ROLE_GROUP: result += " GROUP"; break;
    case ui::AX_ROLE_GROW_AREA: result += " GROW_AREA"; break;
    case ui::AX_ROLE_HEADING: result += " HEADING"; break;
    case ui::AX_ROLE_HELP_TAG: result += " HELP_TAG"; break;
    case ui::AX_ROLE_HORIZONTAL_RULE: result += " HORIZONTAL_RULE"; break;
    case ui::AX_ROLE_IGNORED: result += " IGNORED"; break;
    case ui::AX_ROLE_IMAGE: result += " IMAGE"; break;
    case ui::AX_ROLE_IMAGE_MAP: result += " IMAGE_MAP"; break;
    case ui::AX_ROLE_IMAGE_MAP_LINK: result += " IMAGE_MAP_LINK"; break;
    case ui::AX_ROLE_INCREMENTOR: result += " INCREMENTOR"; break;
    case ui::AX_ROLE_INLINE_TEXT_BOX: result += " INLINE_TEXT_BOX"; break;
    case ui::AX_ROLE_LABEL: result += " LABEL"; break;
    case ui::AX_ROLE_LINK: result += " LINK"; break;
    case ui::AX_ROLE_LIST: result += " LIST"; break;
    case ui::AX_ROLE_LIST_BOX: result += " LISTBOX"; break;
    case ui::AX_ROLE_LIST_BOX_OPTION: result += " LISTBOX_OPTION"; break;
    case ui::AX_ROLE_LIST_ITEM: result += " LIST_ITEM"; break;
    case ui::AX_ROLE_LIST_MARKER: result += " LIST_MARKER"; break;
    case ui::AX_ROLE_LOG: result += " LOG"; break;
    case ui::AX_ROLE_MAIN: result += " MAIN"; break;
    case ui::AX_ROLE_MARQUEE: result += " MARQUEE"; break;
    case ui::AX_ROLE_MATH: result += " MATH"; break;
    case ui::AX_ROLE_MATTE: result += " MATTE"; break;
    case ui::AX_ROLE_MENU: result += " MENU"; break;
    case ui::AX_ROLE_MENU_BAR: result += " MENU_BAR"; break;
    case ui::AX_ROLE_MENU_BUTTON: result += " MENU_BUTTON"; break;
    case ui::AX_ROLE_MENU_ITEM: result += " MENU_ITEM"; break;
    case ui::AX_ROLE_MENU_LIST_OPTION: result += " MENU_LIST_OPTION"; break;
    case ui::AX_ROLE_MENU_LIST_POPUP: result += " MENU_LIST_POPUP"; break;
    case ui::AX_ROLE_NAVIGATION: result += " NAVIGATION"; break;
    case ui::AX_ROLE_NOTE: result += " NOTE"; break;
    case ui::AX_ROLE_OUTLINE: result += " OUTLINE"; break;
    case ui::AX_ROLE_PARAGRAPH: result += " PARAGRAPH"; break;
    case ui::AX_ROLE_POP_UP_BUTTON: result += " POPUP_BUTTON"; break;
    case ui::AX_ROLE_PRESENTATIONAL: result += " PRESENTATIONAL"; break;
    case ui::AX_ROLE_PROGRESS_INDICATOR:
        result += " PROGRESS_INDICATOR"; break;
    case ui::AX_ROLE_RADIO_BUTTON: result += " RADIO_BUTTON"; break;
    case ui::AX_ROLE_RADIO_GROUP: result += " RADIO_GROUP"; break;
    case ui::AX_ROLE_REGION: result += " REGION"; break;
    case ui::AX_ROLE_ROOT_WEB_AREA: result += " ROOT_WEB_AREA"; break;
    case ui::AX_ROLE_ROW: result += " ROW"; break;
    case ui::AX_ROLE_ROW_HEADER: result += " ROW_HEADER"; break;
    case ui::AX_ROLE_RULER: result += " RULER"; break;
    case ui::AX_ROLE_RULER_MARKER: result += " RULER_MARKER"; break;
    case ui::AX_ROLE_SVG_ROOT: result += " SVG_ROOT"; break;
    case ui::AX_ROLE_SCROLL_AREA: result += " SCROLLAREA"; break;
    case ui::AX_ROLE_SCROLL_BAR: result += " SCROLLBAR"; break;
    case ui::AX_ROLE_SEARCH: result += " SEARCH"; break;
    case ui::AX_ROLE_SHEET: result += " SHEET"; break;
    case ui::AX_ROLE_SLIDER: result += " SLIDER"; break;
    case ui::AX_ROLE_SLIDER_THUMB: result += " SLIDER_THUMB"; break;
    case ui::AX_ROLE_SPIN_BUTTON: result += " SPIN_BUTTON"; break;
    case ui::AX_ROLE_SPIN_BUTTON_PART: result += " SPIN_BUTTON_PART"; break;
    case ui::AX_ROLE_SPLIT_GROUP: result += " SPLIT_GROUP"; break;
    case ui::AX_ROLE_SPLITTER: result += " SPLITTER"; break;
    case ui::AX_ROLE_STATIC_TEXT: result += " STATIC_TEXT"; break;
    case ui::AX_ROLE_STATUS: result += " STATUS"; break;
    case ui::AX_ROLE_SYSTEM_WIDE: result += " SYSTEM_WIDE"; break;
    case ui::AX_ROLE_TAB: result += " TAB"; break;
    case ui::AX_ROLE_TAB_LIST: result += " TAB_LIST"; break;
    case ui::AX_ROLE_TAB_PANEL: result += " TAB_PANEL"; break;
    case ui::AX_ROLE_TABLE: result += " TABLE"; break;
    case ui::AX_ROLE_TABLE_HEADER_CONTAINER:
        result += " TABLE_HDR_CONTAINER"; break;
    case ui::AX_ROLE_TEXT_AREA: result += " TEXTAREA"; break;
    case ui::AX_ROLE_TEXT_FIELD: result += " TEXT_FIELD"; break;
    case ui::AX_ROLE_TIMER: result += " TIMER"; break;
    case ui::AX_ROLE_TOGGLE_BUTTON: result += " TOGGLE_BUTTON"; break;
    case ui::AX_ROLE_TOOLBAR: result += " TOOLBAR"; break;
    case ui::AX_ROLE_TREE: result += " TREE"; break;
    case ui::AX_ROLE_TREE_GRID: result += " TREE_GRID"; break;
    case ui::AX_ROLE_TREE_ITEM: result += " TREE_ITEM"; break;
    case ui::AX_ROLE_UNKNOWN: result += " UNKNOWN"; break;
    case ui::AX_ROLE_TOOLTIP: result += " TOOLTIP"; break;
    case ui::AX_ROLE_VALUE_INDICATOR: result += " VALUE_INDICATOR"; break;
    case ui::AX_ROLE_WEB_AREA: result += " WEB_AREA"; break;
    case ui::AX_ROLE_WINDOW: result += " WINDOW"; break;
    default:
      assert(false);
  }

  if (state & (1 << ui::AX_STATE_BUSY))
    result += " BUSY";
  if (state & (1 << ui::AX_STATE_CHECKED))
    result += " CHECKED";
  if (state & (1 << ui::AX_STATE_COLLAPSED))
    result += " COLLAPSED";
  if (state & (1 << ui::AX_STATE_EXPANDED))
    result += " EXPANDED";
  if (state & (1 << ui::AX_STATE_FOCUSABLE))
    result += " FOCUSABLE";
  if (state & (1 << ui::AX_STATE_FOCUSED))
    result += " FOCUSED";
  if (state & (1 << ui::AX_STATE_HASPOPUP))
    result += " HASPOPUP";
  if (state & (1 << ui::AX_STATE_HOVERED))
    result += " HOVERED";
  if (state & (1 << ui::AX_STATE_INDETERMINATE))
    result += " INDETERMINATE";
  if (state & (1 << ui::AX_STATE_INVISIBLE))
    result += " INVISIBLE";
  if (state & (1 << ui::AX_STATE_LINKED))
    result += " LINKED";
  if (state & (1 << ui::AX_STATE_MULTISELECTABLE))
    result += " MULTISELECTABLE";
  if (state & (1 << ui::AX_STATE_OFFSCREEN))
    result += " OFFSCREEN";
  if (state & (1 << ui::AX_STATE_PRESSED))
    result += " PRESSED";
  if (state & (1 << ui::AX_STATE_PROTECTED))
    result += " PROTECTED";
  if (state & (1 << ui::AX_STATE_READONLY))
    result += " READONLY";
  if (state & (1 << ui::AX_STATE_REQUIRED))
    result += " REQUIRED";
  if (state & (1 << ui::AX_STATE_SELECTABLE))
    result += " SELECTABLE";
  if (state & (1 << ui::AX_STATE_SELECTED))
    result += " SELECTED";
  if (state & (1 << ui::AX_STATE_VERTICAL))
    result += " VERTICAL";
  if (state & (1 << ui::AX_STATE_VISITED))
    result += " VISITED";

  result += " (" + IntToString(location.x()) + ", " +
                   IntToString(location.y()) + ")-(" +
                   IntToString(location.width()) + ", " +
                   IntToString(location.height()) + ")";

  for (size_t i = 0; i < int_attributes.size(); ++i) {
    std::string value = IntToString(int_attributes[i].second);
    switch (int_attributes[i].first) {
      case AX_ATTR_SCROLL_X:
        result += " scroll_x=" + value;
        break;
      case AX_ATTR_SCROLL_X_MIN:
        result += " scroll_x_min=" + value;
        break;
      case AX_ATTR_SCROLL_X_MAX:
        result += " scroll_x_max=" + value;
        break;
      case AX_ATTR_SCROLL_Y:
        result += " scroll_y=" + value;
        break;
      case AX_ATTR_SCROLL_Y_MIN:
        result += " scroll_y_min=" + value;
        break;
      case AX_ATTR_SCROLL_Y_MAX:
        result += " scroll_y_max=" + value;
        break;
      case AX_ATTR_HIERARCHICAL_LEVEL:
        result += " level=" + value;
        break;
      case AX_ATTR_TEXT_SEL_START:
        result += " sel_start=" + value;
        break;
      case AX_ATTR_TEXT_SEL_END:
        result += " sel_end=" + value;
        break;
      case AX_ATTR_TABLE_ROW_COUNT:
        result += " rows=" + value;
        break;
      case AX_ATTR_TABLE_COLUMN_COUNT:
        result += " cols=" + value;
        break;
      case AX_ATTR_TABLE_CELL_COLUMN_INDEX:
        result += " col=" + value;
        break;
      case AX_ATTR_TABLE_CELL_ROW_INDEX:
        result += " row=" + value;
        break;
      case AX_ATTR_TABLE_CELL_COLUMN_SPAN:
        result += " colspan=" + value;
        break;
      case AX_ATTR_TABLE_CELL_ROW_SPAN:
        result += " rowspan=" + value;
        break;
      case AX_ATTR_TABLE_COLUMN_HEADER_ID:
        result += " column_header_id=" + value;
        break;
      case AX_ATTR_TABLE_COLUMN_INDEX:
        result += " column_index=" + value;
        break;
      case AX_ATTR_TABLE_HEADER_ID:
        result += " header_id=" + value;
        break;
      case AX_ATTR_TABLE_ROW_HEADER_ID:
        result += " row_header_id=" + value;
        break;
      case AX_ATTR_TABLE_ROW_INDEX:
        result += " row_index=" + value;
        break;
      case AX_ATTR_TITLE_UI_ELEMENT:
        result += " title_elem=" + value;
        break;
      case AX_ATTR_COLOR_VALUE_RED:
        result += " color_value_red=" + value;
        break;
      case AX_ATTR_COLOR_VALUE_GREEN:
        result += " color_value_green=" + value;
        break;
      case AX_ATTR_COLOR_VALUE_BLUE:
        result += " color_value_blue=" + value;
        break;
      case AX_ATTR_TEXT_DIRECTION:
        switch (int_attributes[i].second) {
          case AX_TEXT_DIRECTION_LR:
          default:
            result += " text_direction=lr";
            break;
          case AX_TEXT_DIRECTION_RL:
            result += " text_direction=rl";
            break;
          case AX_TEXT_DIRECTION_TB:
            result += " text_direction=tb";
            break;
          case AX_TEXT_DIRECTION_BT:
            result += " text_direction=bt";
            break;
        }
        break;
    }
  }

  for (size_t i = 0; i < string_attributes.size(); ++i) {
    std::string value = string_attributes[i].second;
    switch (string_attributes[i].first) {
      case AX_ATTR_DOC_URL:
        result += " doc_url=" + value;
        break;
      case AX_ATTR_DOC_TITLE:
        result += " doc_title=" + value;
        break;
      case AX_ATTR_DOC_MIMETYPE:
        result += " doc_mimetype=" + value;
        break;
      case AX_ATTR_DOC_DOCTYPE:
        result += " doc_doctype=" + value;
        break;
      case AX_ATTR_ACCESS_KEY:
        result += " access_key=" + value;
        break;
      case AX_ATTR_ACTION:
        result += " action=" + value;
        break;
      case AX_ATTR_DESCRIPTION:
        result += " description=" + value;
        break;
      case AX_ATTR_DISPLAY:
        result += " display=" + value;
        break;
      case AX_ATTR_HELP:
        result += " help=" + value;
        break;
      case AX_ATTR_HTML_TAG:
        result += " html_tag=" + value;
        break;
      case AX_ATTR_LIVE_RELEVANT:
        result += " relevant=" + value;
        break;
      case AX_ATTR_LIVE_STATUS:
        result += " live=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_RELEVANT:
        result += " container_relevant=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_STATUS:
        result += " container_live=" + value;
        break;
      case AX_ATTR_ROLE:
        result += " role=" + value;
        break;
      case AX_ATTR_SHORTCUT:
        result += " shortcut=" + value;
        break;
      case AX_ATTR_URL:
        result += " url=" + value;
        break;
      case AX_ATTR_NAME:
        result += " name=" + value;
        break;
      case AX_ATTR_VALUE:
        result += " value=" + value;
        break;
    }
  }

  for (size_t i = 0; i < float_attributes.size(); ++i) {
    std::string value = DoubleToString(float_attributes[i].second);
    switch (float_attributes[i].first) {
      case AX_ATTR_DOC_LOADING_PROGRESS:
        result += " doc_progress=" + value;
        break;
      case AX_ATTR_VALUE_FOR_RANGE:
        result += " value_for_range=" + value;
        break;
      case AX_ATTR_MAX_VALUE_FOR_RANGE:
        result += " max_value=" + value;
        break;
      case AX_ATTR_MIN_VALUE_FOR_RANGE:
        result += " min_value=" + value;
        break;
    }
  }

  for (size_t i = 0; i < bool_attributes.size(); ++i) {
    std::string value = bool_attributes[i].second ? "true" : "false";
    switch (bool_attributes[i].first) {
      case AX_ATTR_DOC_LOADED:
        result += " doc_loaded=" + value;
        break;
      case AX_ATTR_BUTTON_MIXED:
        result += " mixed=" + value;
        break;
      case AX_ATTR_LIVE_ATOMIC:
        result += " atomic=" + value;
        break;
      case AX_ATTR_LIVE_BUSY:
        result += " busy=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_ATOMIC:
        result += " container_atomic=" + value;
        break;
      case AX_ATTR_CONTAINER_LIVE_BUSY:
        result += " container_busy=" + value;
        break;
      case AX_ATTR_ARIA_READONLY:
        result += " aria_readonly=" + value;
        break;
      case AX_ATTR_CAN_SET_VALUE:
        result += " can_set_value=" + value;
        break;
      case AX_ATTR_UPDATE_LOCATION_ONLY:
        result += " update_location_only=" + value;
        break;
      case AX_ATTR_CANVAS_HAS_FALLBACK:
        result += " has_fallback=" + value;
        break;
    }
  }

  for (size_t i = 0; i < intlist_attributes.size(); ++i) {
    const std::vector<int32>& values = intlist_attributes[i].second;
    switch (intlist_attributes[i].first) {
      case AX_ATTR_INDIRECT_CHILD_IDS:
        result += " indirect_child_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_LINE_BREAKS:
        result += " line_breaks=" + IntVectorToString(values);
        break;
      case AX_ATTR_CELL_IDS:
        result += " cell_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_UNIQUE_CELL_IDS:
        result += " unique_cell_ids=" + IntVectorToString(values);
        break;
      case AX_ATTR_CHARACTER_OFFSETS:
        result += " character_offsets=" + IntVectorToString(values);
        break;
      case AX_ATTR_WORD_STARTS:
        result += " word_starts=" + IntVectorToString(values);
        break;
      case AX_ATTR_WORD_ENDS:
        result += " word_ends=" + IntVectorToString(values);
        break;
    }
  }

  if (!child_ids.empty())
    result += " child_ids=" + IntVectorToString(child_ids);

  return result;
}

}  // namespace ui
