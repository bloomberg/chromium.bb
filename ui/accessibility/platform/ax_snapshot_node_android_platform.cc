// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_snapshot_node_android_platform.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

bool HasFocusableChild(const AXNode* node) {
  for (auto* child : node->children()) {
    if (child->data().HasState(AX_STATE_FOCUSABLE) ||
        HasFocusableChild(child)) {
      return true;
    }
  }
  return false;
}

bool HasOnlyTextChildren(const AXNode* node) {
  for (auto* child : node->children()) {
    if (!child->IsTextNode())
      return false;
  }
  return true;
}

// TODO(muyuanli): share with BrowserAccessibility.
bool IsSimpleTextControl(const AXNode* node, uint32_t state) {
  switch (node->data().role) {
    case AX_ROLE_COMBO_BOX:
    case AX_ROLE_SEARCH_BOX:
      return true;
    case AX_ROLE_TEXT_FIELD:
      return !node->data().HasState(AX_STATE_RICHLY_EDITABLE);
    default:
      return false;
  }
}

bool IsRichTextEditable(const AXNode* node) {
  const AXNode* parent = node->parent();
  return node->data().HasState(AX_STATE_RICHLY_EDITABLE) &&
         (!parent || !parent->data().HasState(AX_STATE_RICHLY_EDITABLE));
}

bool IsNativeTextControl(const AXNode* node) {
  const std::string& html_tag =
      node->data().GetStringAttribute(AX_ATTR_HTML_TAG);
  if (html_tag == "input") {
    std::string input_type;
    if (!node->data().GetHtmlAttribute("type", &input_type))
      return true;
    return input_type.empty() || input_type == "email" ||
           input_type == "password" || input_type == "search" ||
           input_type == "tel" || input_type == "text" || input_type == "url" ||
           input_type == "number";
  }
  return html_tag == "textarea";
}

bool IsLeaf(const AXNode* node) {
  if (node->child_count() == 0)
    return true;

  if (IsNativeTextControl(node) || node->IsTextNode()) {
    return true;
  }

  switch (node->data().role) {
    case AX_ROLE_IMAGE:
    case AX_ROLE_METER:
    case AX_ROLE_SCROLL_BAR:
    case AX_ROLE_SLIDER:
    case AX_ROLE_SPLITTER:
    case AX_ROLE_PROGRESS_INDICATOR:
    case AX_ROLE_DATE:
    case AX_ROLE_DATE_TIME:
    case AX_ROLE_INPUT_TIME:
      return true;
    default:
      return false;
  }
}

base::string16 GetInnerText(const AXNode* node) {
  if (node->IsTextNode()) {
    return node->data().GetString16Attribute(AX_ATTR_NAME);
  }
  base::string16 text;
  for (auto* child : node->children()) {
    text += GetInnerText(child);
  }
  return text;
}

base::string16 GetValue(const AXNode* node, bool show_password) {
  base::string16 value = node->data().GetString16Attribute(AX_ATTR_VALUE);

  if (value.empty() &&
      (IsSimpleTextControl(node, node->data().state) ||
       IsRichTextEditable(node)) &&
      !IsNativeTextControl(node)) {
    value = GetInnerText(node);
  }

  if (node->data().HasState(AX_STATE_PROTECTED)) {
    if (!show_password) {
      value = base::string16(value.size(), kSecurePasswordBullet);
    }
  }

  return value;
}

bool HasOnlyTextAndImageChildren(const AXNode* node) {
  for (auto* child : node->children()) {
    if (child->data().role != AX_ROLE_STATIC_TEXT &&
        child->data().role != AX_ROLE_IMAGE) {
      return false;
    }
  }
  return true;
}

bool IsFocusable(const AXNode* node) {
  if (node->data().role == AX_ROLE_IFRAME ||
      node->data().role == AX_ROLE_IFRAME_PRESENTATIONAL ||
      (node->data().role == AX_ROLE_ROOT_WEB_AREA && node->parent())) {
    return node->data().HasStringAttribute(AX_ATTR_NAME);
  }
  return node->data().HasState(AX_STATE_FOCUSABLE);
}

base::string16 GetText(const AXNode* node, bool show_password) {
  if (node->data().role == AX_ROLE_WEB_AREA ||
      node->data().role == AX_ROLE_IFRAME ||
      node->data().role == AX_ROLE_IFRAME_PRESENTATIONAL) {
    return base::string16();
  }

  if (node->data().role == AX_ROLE_LIST_ITEM &&
      node->data().GetIntAttribute(AX_ATTR_NAME_FROM) ==
          AX_NAME_FROM_CONTENTS) {
    if (node->child_count() > 0 && !HasOnlyTextChildren(node))
      return base::string16();
  }

  base::string16 value = GetValue(node, show_password);

  if (!value.empty()) {
    if (node->data().HasState(AX_STATE_EDITABLE))
      return value;

    switch (node->data().role) {
      case AX_ROLE_COMBO_BOX:
      case AX_ROLE_POP_UP_BUTTON:
      case AX_ROLE_TEXT_FIELD:
        return value;
      default:
        break;
    }
  }

  if (node->data().role == AX_ROLE_COLOR_WELL) {
    unsigned int color = static_cast<unsigned int>(
        node->data().GetIntAttribute(AX_ATTR_COLOR_VALUE));
    unsigned int red = color >> 16 & 0xFF;
    unsigned int green = color >> 8 & 0xFF;
    unsigned int blue = color >> 0 & 0xFF;
    return base::UTF8ToUTF16(
        base::StringPrintf("#%02X%02X%02X", red, green, blue));
  }

  base::string16 text = node->data().GetString16Attribute(AX_ATTR_NAME);
  base::string16 description =
      node->data().GetString16Attribute(AX_ATTR_DESCRIPTION);
  if (!description.empty()) {
    if (!text.empty())
      text += base::ASCIIToUTF16(" ");
    text += description;
  }

  if (text.empty())
    text = value;

  if (node->data().role == AX_ROLE_ROOT_WEB_AREA)
    return text;

  if (text.empty() &&
      (HasOnlyTextChildren(node) ||
       (IsFocusable(node) && HasOnlyTextAndImageChildren(node)))) {
    for (auto* child : node->children()) {
      text += GetText(child, show_password);
    }
  }

  if (text.empty() && (AXSnapshotNodeAndroid::AXRoleIsLink(node->data().role) ||
                       node->data().role == AX_ROLE_IMAGE)) {
    base::string16 url = node->data().GetString16Attribute(AX_ATTR_URL);
    text = AXSnapshotNodeAndroid::AXUrlBaseText(url);
  }
  return text;
}

// Get string representation of AXRole. We are not using ToString() in
// ax_enums.h since the names are subject to change in the future and
// we are only interested in a subset of the roles.
base::Optional<std::string> AXRoleToString(AXRole role) {
  switch (role) {
    case AX_ROLE_ARTICLE:
      return base::Optional<std::string>("article");
    case AX_ROLE_BANNER:
      return base::Optional<std::string>("banner");
    case AX_ROLE_CAPTION:
      return base::Optional<std::string>("caption");
    case AX_ROLE_COMPLEMENTARY:
      return base::Optional<std::string>("complementary");
    case AX_ROLE_DATE:
      return base::Optional<std::string>("date");
    case AX_ROLE_DATE_TIME:
      return base::Optional<std::string>("date_time");
    case AX_ROLE_DEFINITION:
      return base::Optional<std::string>("definition");
    case AX_ROLE_DETAILS:
      return base::Optional<std::string>("details");
    case AX_ROLE_DOCUMENT:
      return base::Optional<std::string>("document");
    case AX_ROLE_FEED:
      return base::Optional<std::string>("feed");
    case AX_ROLE_HEADING:
      return base::Optional<std::string>("heading");
    case AX_ROLE_IFRAME:
      return base::Optional<std::string>("iframe");
    case AX_ROLE_IFRAME_PRESENTATIONAL:
      return base::Optional<std::string>("iframe_presentational");
    case AX_ROLE_LIST:
      return base::Optional<std::string>("list");
    case AX_ROLE_LIST_ITEM:
      return base::Optional<std::string>("list_item");
    case AX_ROLE_MAIN:
      return base::Optional<std::string>("main");
    case AX_ROLE_PARAGRAPH:
      return base::Optional<std::string>("paragraph");
    default:
      return base::Optional<std::string>();
  }
}

}  // namespace

AXSnapshotNodeAndroid::AXSnapshotNodeAndroid() = default;
AX_EXPORT AXSnapshotNodeAndroid::~AXSnapshotNodeAndroid() = default;

// static
AX_EXPORT std::unique_ptr<AXSnapshotNodeAndroid> AXSnapshotNodeAndroid::Create(
    const AXTreeUpdate& update,
    bool show_password) {
  auto tree = std::make_unique<AXSerializableTree>();
  if (!tree->Unserialize(update)) {
    LOG(FATAL) << tree->error();
  }

  WalkAXTreeConfig config{
      false,         // should_select_leaf
      show_password  // show_password
  };
  return WalkAXTreeDepthFirst(tree->root(), gfx::Rect(), update, tree.get(),
                              config);
}

// static
AX_EXPORT bool AXSnapshotNodeAndroid::AXRoleIsLink(AXRole role) {
  return role == AX_ROLE_LINK;
}

// static
AX_EXPORT base::string16 AXSnapshotNodeAndroid::AXUrlBaseText(
    base::string16 url) {
  // Given a url like http://foo.com/bar/baz.png, just return the
  // base text, e.g., "baz".
  int trailing_slashes = 0;
  while (url.size() - trailing_slashes > 0 &&
         url[url.size() - trailing_slashes - 1] == '/') {
    trailing_slashes++;
  }
  if (trailing_slashes)
    url = url.substr(0, url.size() - trailing_slashes);
  size_t slash_index = url.rfind('/');
  if (slash_index != std::string::npos)
    url = url.substr(slash_index + 1);
  size_t dot_index = url.rfind('.');
  if (dot_index != std::string::npos)
    url = url.substr(0, dot_index);
  return url;
}

// static
AX_EXPORT const char* AXSnapshotNodeAndroid::AXRoleToAndroidClassName(
    AXRole role,
    bool has_parent) {
  switch (role) {
    case AX_ROLE_SEARCH_BOX:
    case AX_ROLE_SPIN_BUTTON:
    case AX_ROLE_TEXT_FIELD:
      return kAXEditTextClassname;
    case AX_ROLE_SLIDER:
      return kAXSeekBarClassname;
    case AX_ROLE_COLOR_WELL:
    case AX_ROLE_COMBO_BOX:
    case AX_ROLE_DATE:
    case AX_ROLE_POP_UP_BUTTON:
    case AX_ROLE_INPUT_TIME:
      return kAXSpinnerClassname;
    case AX_ROLE_BUTTON:
    case AX_ROLE_MENU_BUTTON:
      return kAXButtonClassname;
    case AX_ROLE_CHECK_BOX:
    case AX_ROLE_SWITCH:
      return kAXCheckBoxClassname;
    case AX_ROLE_RADIO_BUTTON:
      return kAXRadioButtonClassname;
    case AX_ROLE_TOGGLE_BUTTON:
      return kAXToggleButtonClassname;
    case AX_ROLE_CANVAS:
    case AX_ROLE_IMAGE:
    case AX_ROLE_SVG_ROOT:
      return kAXImageClassname;
    case AX_ROLE_METER:
    case AX_ROLE_PROGRESS_INDICATOR:
      return kAXProgressBarClassname;
    case AX_ROLE_TAB_LIST:
      return kAXTabWidgetClassname;
    case AX_ROLE_GRID:
    case AX_ROLE_TREE_GRID:
    case AX_ROLE_TABLE:
      return kAXGridViewClassname;
    case AX_ROLE_LIST:
    case AX_ROLE_LIST_BOX:
    case AX_ROLE_DESCRIPTION_LIST:
      return kAXListViewClassname;
    case AX_ROLE_DIALOG:
      return kAXDialogClassname;
    case AX_ROLE_ROOT_WEB_AREA:
      return has_parent ? kAXViewClassname : kAXWebViewClassname;
    case AX_ROLE_MENU_ITEM:
    case AX_ROLE_MENU_ITEM_CHECK_BOX:
    case AX_ROLE_MENU_ITEM_RADIO:
      return kAXMenuItemClassname;
    default:
      return kAXViewClassname;
  }
}

// static
std::unique_ptr<AXSnapshotNodeAndroid>
AXSnapshotNodeAndroid::WalkAXTreeDepthFirst(
    const AXNode* node,
    gfx::Rect rect,
    const AXTreeUpdate& update,
    const AXTree* tree,
    AXSnapshotNodeAndroid::WalkAXTreeConfig& config) {
  auto result =
      std::unique_ptr<AXSnapshotNodeAndroid>(new AXSnapshotNodeAndroid());
  result->text = GetText(node, config.show_password);
  result->class_name = AXSnapshotNodeAndroid::AXRoleToAndroidClassName(
      node->data().role, node->parent() != nullptr);
  result->role = AXRoleToString(node->data().role);

  result->text_size = -1.0;
  result->bgcolor = 0;
  result->color = 0;
  result->bold = 0;
  result->italic = 0;
  result->line_through = 0;
  result->underline = 0;

  if (node->data().HasFloatAttribute(AX_ATTR_FONT_SIZE)) {
    gfx::RectF text_size_rect(
        0, 0, 1, node->data().GetFloatAttribute(AX_ATTR_FONT_SIZE));
    gfx::Rect scaled_text_size_rect =
        gfx::ToEnclosingRect(tree->RelativeToTreeBounds(node, text_size_rect));
    result->text_size = scaled_text_size_rect.height();

    const int text_style = node->data().GetIntAttribute(AX_ATTR_TEXT_STYLE);
    result->color = node->data().GetIntAttribute(AX_ATTR_COLOR);
    result->bgcolor = node->data().GetIntAttribute(AX_ATTR_BACKGROUND_COLOR);
    result->bold = (text_style & AX_TEXT_STYLE_BOLD) != 0;
    result->italic = (text_style & AX_TEXT_STYLE_ITALIC) != 0;
    result->line_through = (text_style & AX_TEXT_STYLE_LINE_THROUGH) != 0;
    result->underline = (text_style & AX_TEXT_STYLE_UNDERLINE) != 0;
  }

  const gfx::Rect& absolute_rect =
      gfx::ToEnclosingRect(tree->GetTreeBounds(node));
  gfx::Rect parent_relative_rect = absolute_rect;
  bool is_root = node->parent() == nullptr;
  if (!is_root) {
    parent_relative_rect.Offset(-rect.OffsetFromOrigin());
  }
  result->rect = gfx::Rect(parent_relative_rect.x(), parent_relative_rect.y(),
                           absolute_rect.width(), absolute_rect.height());
  result->has_selection = false;

  if (IsLeaf(node) && update.has_tree_data) {
    int start_selection = 0;
    int end_selection = 0;
    if (update.tree_data.sel_anchor_object_id == node->id()) {
      start_selection = update.tree_data.sel_anchor_offset;
      config.should_select_leaf = true;
    }

    if (config.should_select_leaf) {
      end_selection =
          static_cast<int32_t>(GetText(node, config.show_password).length());
    }

    if (update.tree_data.sel_focus_object_id == node->id()) {
      end_selection = update.tree_data.sel_focus_offset;
      config.should_select_leaf = false;
    }
    if (end_selection > 0) {
      result->has_selection = true;
      result->start_selection = start_selection;
      result->end_selection = end_selection;
    }
  }

  for (auto* child : node->children()) {
    result->children.push_back(
        WalkAXTreeDepthFirst(child, absolute_rect, update, tree, config));
  }

  return result;
}

}  // namespace ui
