// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_fuchsia.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility_fuchsia.h"

namespace content {
namespace {

using FuchsiaAction = fuchsia::accessibility::semantics::Action;
using FuchsiaCheckedState = fuchsia::accessibility::semantics::CheckedState;
using FuchsiaRole = fuchsia::accessibility::semantics::Role;
using FuchsiaToggledState = fuchsia::accessibility::semantics::ToggledState;

constexpr const char* const kBoolAttributes[] = {
    "hidden", "focusable", "has_input_focus", "is_keyboard_key", "selected",
};

constexpr const char* const kStringAttributes[] = {
    "label",           "actions",       "secondary_label",
    "value",           "checked_state", "toggled_state",
    "viewport_offset", "location",      "transform",
};

constexpr const char* const kIntAttributes[] = {
    "number_of_rows",   "number_of_columns", "row_index",
    "cell_row_index",   "cell_column_index", "cell_row_span",
    "cell_column_span",
};

constexpr const char* const kDoubleAttributes[] = {
    "min_value",
    "max_value",
    "step_delta",
};

std::string FuchsiaRoleToString(const FuchsiaRole role) {
  switch (role) {
    case FuchsiaRole::BUTTON:
      return "BUTTON";
    case FuchsiaRole::CELL:
      return "CELL";
    case FuchsiaRole::CHECK_BOX:
      return "CHECK_BOX";
    case FuchsiaRole::COLUMN_HEADER:
      return "COLUMN_HEADER";
    case FuchsiaRole::GRID:
      return "GRID";
    case FuchsiaRole::HEADER:
      return "HEADER";
    case FuchsiaRole::IMAGE:
      return "IMAGE";
    case FuchsiaRole::LINK:
      return "LINK";
    case FuchsiaRole::LIST:
      return "LIST";
    case FuchsiaRole::LIST_ELEMENT_MARKER:
      return "LIST_ELEMENT_MARKER";
    case FuchsiaRole::PARAGRAPH:
      return "PARAGRAPH";
    case FuchsiaRole::RADIO_BUTTON:
      return "RADIO_BUTTON";
    case FuchsiaRole::ROW_GROUP:
      return "ROW_GROUP";
    case FuchsiaRole::ROW_HEADER:
      return "ROW_HEADER";
    case FuchsiaRole::SEARCH_BOX:
      return "SEARCH_BOX";
    case FuchsiaRole::SLIDER:
      return "SLIDER";
    case FuchsiaRole::STATIC_TEXT:
      return "STATIC_TEXT";
    case FuchsiaRole::TABLE:
      return "TABLE";
    case FuchsiaRole::TABLE_ROW:
      return "TABLE_ROW";
    case FuchsiaRole::TEXT_FIELD:
      return "TEXT_FIELD";
    case FuchsiaRole::TEXT_FIELD_WITH_COMBO_BOX:
      return "TEXT_FIELD_WITH_COMBO_BOX";
    case FuchsiaRole::TOGGLE_SWITCH:
      return "TOGGLE_SWITCH";
    case FuchsiaRole::UNKNOWN:
      return "UNKNOWN";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string FuchsiaActionToString(FuchsiaAction action) {
  switch (action) {
    case FuchsiaAction::DEFAULT:
      return "DEFAULT";
    case FuchsiaAction::DECREMENT:
      return "DECREMENT";
    case FuchsiaAction::INCREMENT:
      return "INCREMENT";
    case FuchsiaAction::SECONDARY:
      return "SECONDARY";
    case FuchsiaAction::SET_FOCUS:
      return "SET_FOCUS";
    case FuchsiaAction::SET_VALUE:
      return "SET_VALUE";
    case FuchsiaAction::SHOW_ON_SCREEN:
      return "SHOW_ON_SCREEN";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string FuchsiaActionsToString(const std::vector<FuchsiaAction>& actions) {
  std::vector<std::string> fuchsia_actions;
  for (const auto& action : actions) {
    fuchsia_actions.push_back(FuchsiaActionToString(action));
  }

  if (fuchsia_actions.empty())
    return std::string();

  return "{" + base::JoinString(fuchsia_actions, ", ") + "}";
}

std::string CheckedStateToString(const FuchsiaCheckedState checked_state) {
  switch (checked_state) {
    case FuchsiaCheckedState::NONE:
      return "NONE";
    case FuchsiaCheckedState::CHECKED:
      return "CHECKED";
    case FuchsiaCheckedState::UNCHECKED:
      return "UNCHECKED";
    case FuchsiaCheckedState::MIXED:
      return "MIXED";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string ToggledStateToString(const FuchsiaToggledState toggled_state) {
  switch (toggled_state) {
    case FuchsiaToggledState::ON:
      return "ON";
    case FuchsiaToggledState::OFF:
      return "OFF";
    case FuchsiaToggledState::INDETERMINATE:
      return "INDETERMINATE";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string ViewportOffsetToString(
    const fuchsia::ui::gfx::vec2& viewport_offset) {
  return base::StringPrintf("(%.1f, %.1f)", viewport_offset.x,
                            viewport_offset.y);
}

std::string Vec3ToString(const fuchsia::ui::gfx::vec3& vec) {
  return base::StringPrintf("(%.1f, %.1f, %.1f)", vec.x, vec.y, vec.z);
}

std::string Mat4ToString(const fuchsia::ui::gfx::mat4& mat) {
  std::string retval = "{ ";
  for (int i = 0; i < 4; i++) {
    retval.append(base::StringPrintf(
        "col%d: (%.1f,%.1f,%.1f,%.1f), ", i, mat.matrix[i * 4],
        mat.matrix[i * 4 + 1], mat.matrix[i * 4 + 2], mat.matrix[i * 4 + 3]));
  }
  return retval.append(" }");
}

std::string LocationToString(const fuchsia::ui::gfx::BoundingBox& location) {
  return base::StringPrintf("{ min: %s, max: %s }",
                            Vec3ToString(location.min).c_str(),
                            Vec3ToString(location.max).c_str());
}

}  // namespace

AccessibilityTreeFormatterFuchsia::AccessibilityTreeFormatterFuchsia() =
    default;
AccessibilityTreeFormatterFuchsia::~AccessibilityTreeFormatterFuchsia() =
    default;

void AccessibilityTreeFormatterFuchsia::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  // Exclude spatial semantics by default to avoid flakiness.
  AddPropertyFilter(property_filters, "location", AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "transform", AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "viewport_offset",
                    AXPropertyFilter::DENY);
}

base::Value AccessibilityTreeFormatterFuchsia::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  CHECK(root);

  BrowserAccessibility* root_internal =
      BrowserAccessibility::FromAXPlatformNodeDelegate(root);

  base::DictionaryValue dict;
  RecursiveBuildTree(*root_internal, &dict);
  return dict;
}

void AccessibilityTreeFormatterFuchsia::RecursiveBuildTree(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) const {
  if (!ShouldDumpNode(node))
    return;

  AddProperties(node, dict);
  if (!ShouldDumpChildren(node))
    return;

  base::ListValue children;

  fuchsia::accessibility::semantics::Node fuchsia_node =
      static_cast<const BrowserAccessibilityFuchsia&>(node).ToFuchsiaNodeData();

  for (uint32_t child_id : fuchsia_node.child_ids()) {
    ui::AXPlatformNodeFuchsia* child_node =
        static_cast<ui::AXPlatformNodeFuchsia*>(
            ui::AXPlatformNodeBase::GetFromUniqueId(child_id));
    DCHECK(child_node);

    BrowserAccessibilityFuchsia* child_browser_accessibility =
        static_cast<BrowserAccessibilityFuchsia*>(child_node->GetDelegate());

    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);
    RecursiveBuildTree(*child_browser_accessibility, child_dict.get());
    children.Append(std::move(child_dict));
  }
  dict->SetKey(kChildrenDictAttr, std::move(children));
}

base::Value AccessibilityTreeFormatterFuchsia::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  CHECK(node);
  base::DictionaryValue dict;
  AddProperties(*BrowserAccessibility::FromAXPlatformNodeDelegate(node), &dict);
  return std::move(dict);
}

void AccessibilityTreeFormatterFuchsia::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) const {
  dict->SetIntKey("id", node.GetId());

  const BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      static_cast<const BrowserAccessibilityFuchsia*>(&node);

  CHECK(browser_accessibility_fuchsia);

  const fuchsia::accessibility::semantics::Node& fuchsia_node =
      browser_accessibility_fuchsia->ToFuchsiaNodeData();

  // Add fuchsia node attributes.
  dict->SetStringKey("role", FuchsiaRoleToString(fuchsia_node.role()));

  dict->SetStringKey("actions", FuchsiaActionsToString(fuchsia_node.actions()));

  if (fuchsia_node.has_attributes()) {
    const fuchsia::accessibility::semantics::Attributes& attributes =
        fuchsia_node.attributes();

    if (attributes.has_label() && !attributes.label().empty())
      dict->SetStringKey("label", attributes.label());

    if (attributes.has_secondary_label() &&
        !attributes.secondary_label().empty()) {
      dict->SetStringKey("secondary_label", attributes.secondary_label());
    }

    if (attributes.has_range()) {
      const auto& range_attributes = attributes.range();

      if (range_attributes.has_min_value())
        dict->SetDoubleKey("min_value", range_attributes.min_value());

      if (range_attributes.has_max_value())
        dict->SetDoubleKey("max_value", range_attributes.max_value());

      if (range_attributes.has_step_delta())
        dict->SetDoubleKey("step_delta", range_attributes.step_delta());
    }

    if (attributes.has_table_attributes()) {
      const auto& table_attributes = attributes.table_attributes();

      if (table_attributes.has_number_of_rows())
        dict->SetIntKey("number_of_rows", table_attributes.number_of_rows());

      if (table_attributes.has_number_of_columns()) {
        dict->SetIntKey("number_of_columns",
                        table_attributes.number_of_columns());
      }
    }

    if (attributes.has_table_row_attributes()) {
      const auto& table_row_attributes = attributes.table_row_attributes();

      if (table_row_attributes.has_row_index())
        dict->SetIntKey("row_index", table_row_attributes.row_index());
    }

    if (attributes.has_table_cell_attributes()) {
      const auto& table_cell_attributes = attributes.table_cell_attributes();

      if (table_cell_attributes.has_row_index())
        dict->SetIntKey("cell_row_index", table_cell_attributes.row_index());

      if (table_cell_attributes.has_column_index()) {
        dict->SetIntKey("cell_column_index",
                        table_cell_attributes.column_index());
      }

      if (table_cell_attributes.has_row_span())
        dict->SetIntKey("cell_row_span", table_cell_attributes.row_span());

      if (table_cell_attributes.has_column_span()) {
        dict->SetIntKey("cell_column_span",
                        table_cell_attributes.column_span());
      }
    }

    if (attributes.has_is_keyboard_key())
      dict->SetBoolKey("is_keyboard_key", attributes.is_keyboard_key());
  }

  if (fuchsia_node.has_states()) {
    const fuchsia::accessibility::semantics::States& states =
        fuchsia_node.states();

    if (states.has_selected())
      dict->SetBoolKey("selected", states.selected());

    if (states.has_checked_state()) {
      dict->SetStringKey("checked_state",
                         CheckedStateToString(states.checked_state()));
    }

    if (states.has_hidden())
      dict->SetBoolKey("hidden", states.hidden());

    if (states.has_value() && !states.value().empty())
      dict->SetStringKey("value", states.value());

    if (states.has_viewport_offset()) {
      dict->SetStringKey("viewport_offset",
                         ViewportOffsetToString(states.viewport_offset()));
    }

    if (states.has_toggled_state()) {
      dict->SetStringKey("toggled_state",
                         ToggledStateToString(states.toggled_state()));
    }

    if (states.has_focusable())
      dict->SetBoolKey("focusable", states.focusable());

    if (states.has_has_input_focus())
      dict->SetBoolKey("has_input_focus", states.has_input_focus());
  }

  if (fuchsia_node.has_location())
    dict->SetStringKey("location", LocationToString(fuchsia_node.location()));

  if (fuchsia_node.has_transform()) {
    dict->SetStringKey(
        "transform", Mat4ToString(fuchsia_node.node_to_container_transform()));
  }
}

std::string AccessibilityTreeFormatterFuchsia::ProcessTreeForOutput(
    const base::DictionaryValue& node) const {
  std::string error_value;
  if (node.GetString("error", &error_value))
    return error_value;

  std::string line;

  if (show_ids()) {
    int id_value = node.FindIntKey("id").value_or(0);
    WriteAttribute(true, base::NumberToString(id_value), &line);
  }

  std::string role_value;
  node.GetString("role", &role_value);
  WriteAttribute(true, role_value, &line);

  for (unsigned i = 0; i < base::size(kBoolAttributes); i++) {
    const char* bool_attribute = kBoolAttributes[i];
    absl::optional<bool> value = node.FindBoolPath(bool_attribute);
    if (value && *value)
      WriteAttribute(/*include_by_default=*/true, bool_attribute, &line);
  }

  for (unsigned i = 0; i < base::size(kStringAttributes); i++) {
    const char* string_attribute = kStringAttributes[i];
    std::string value;
    if (!node.GetString(string_attribute, &value) || value.empty())
      continue;

    WriteAttribute(
        /*include_by_default=*/true,
        base::StringPrintf("%s='%s'", string_attribute, value.c_str()), &line);
  }

  for (unsigned i = 0; i < base::size(kIntAttributes); i++) {
    const char* attribute_name = kIntAttributes[i];
    int value = node.FindIntKey(attribute_name).value_or(0);
    if (value == 0)
      continue;
    WriteAttribute(true, base::StringPrintf("%s=%d", attribute_name, value),
                   &line);
  }

  for (unsigned i = 0; i < base::size(kDoubleAttributes); i++) {
    const char* attribute_name = kDoubleAttributes[i];
    int value = node.FindIntKey(attribute_name).value_or(0);
    if (value == 0)
      continue;
    WriteAttribute(true, base::StringPrintf("%s=%d", attribute_name, value),
                   &line);
  }

  return line;
}

base::Value AccessibilityTreeFormatterFuchsia::BuildTreeForSelector(
    const AXTreeSelector&) const {
  NOTIMPLEMENTED();
  return base::Value(base::Value::Type::DICTIONARY);
}

}  // namespace content
