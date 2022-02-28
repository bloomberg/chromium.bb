// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_mac.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"
#include "ui/accessibility/platform/inspect/ax_transform_mac.h"

// This file uses the deprecated NSObject accessibility interface.
// TODO(crbug.com/948844): Migrate to the new NSAccessibility interface.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

using base::StringPrintf;
using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using content::a11y::AttributeInvoker;
using content::a11y::OptionalNSObject;
using std::string;
using ui::AXPositionOf;
using ui::AXPropertyFilter;
using ui::AXPropertyNode;
using ui::AXNSObjectToBaseValue;
using ui::AXNilToBaseValue;
using ui::AXNSPointToBaseValue;
using ui::AXTreeIndexerMac;
using ui::AXAttributeNamesOf;
using ui::AXAttributeValueOf;
using ui::AXSizeOf;
using ui::AXFormatValue;
using ui::AXMakeConst;
using ui::AXMakeOrderedKey;
using ui::AXMakeSetKey;

namespace content {

namespace {

const char kLocalPositionDictAttr[] = "LocalPosition";

const char kFailedToParseError[] = "_const_ERROR:FAILED_TO_PARSE";
const char kNotApplicable[] = "_const_n/a";

}  // namespace

AccessibilityTreeFormatterMac::AccessibilityTreeFormatterMac() = default;

AccessibilityTreeFormatterMac::~AccessibilityTreeFormatterMac() = default;

void AccessibilityTreeFormatterMac::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  static NSArray* default_attributes = [@[
    @"AXAutocompleteValue=*", @"AXDescription=*", @"AXRole=*", @"AXTitle=*",
    @"AXTitleUIElement=*", @"AXHelp=*", @"AXValue=*"
  ] retain];

  for (NSString* attribute : default_attributes) {
    AddPropertyFilter(property_filters, SysNSStringToUTF8(attribute));
  }

  if (show_ids()) {
    AddPropertyFilter(property_filters, "ChromeAXNodeId");
  }
}

base::Value AccessibilityTreeFormatterMac::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  DCHECK(root);
  BrowserAccessibility* internal_root =
      BrowserAccessibility::FromAXPlatformNodeDelegate(root);
  return BuildTree(ToBrowserAccessibilityCocoa(internal_root));
}

base::Value AccessibilityTreeFormatterMac::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  AXUIElementRef node = nil;
  std::tie(node, std::ignore) = ui::FindAXUIElement(selector);
  if (node == nil) {
    return base::Value(base::Value::Type::DICTIONARY);
  }
  return BuildTreeForAXUIElement(node);
}

base::Value AccessibilityTreeFormatterMac::BuildTreeForAXUIElement(
    AXUIElementRef node) const {
  return BuildTree(static_cast<id>(node));
}

base::Value AccessibilityTreeFormatterMac::BuildTree(const id root) const {
  DCHECK(root);

  AXTreeIndexerMac indexer(root);
  base::Value dict(base::Value::Type::DICTIONARY);

  NSPoint position = AXPositionOf(root);
  NSSize size = AXSizeOf(root);
  NSRect rect = NSMakeRect(position.x, position.y, size.width, size.height);

  RecursiveBuildTree(root, rect, &indexer, &dict);

  return dict;
}

std::string AccessibilityTreeFormatterMac::EvaluateScript(
    ui::AXPlatformNodeDelegate* root,
    const std::vector<ui::AXScriptInstruction>& instructions,
    size_t start_index,
    size_t end_index) const {
  BrowserAccessibilityCocoa* platform_root = ToBrowserAccessibilityCocoa(
      BrowserAccessibility::FromAXPlatformNodeDelegate(root));

  base::Value scripts(base::Value::Type::LIST);
  AXTreeIndexerMac indexer(platform_root);
  std::map<std::string, id> storage;
  AttributeInvoker invoker(&indexer, &storage);
  for (size_t index = start_index; index < end_index; index++) {
    if (instructions[index].IsComment()) {
      scripts.Append(instructions[index].AsComment());
      continue;
    }

    DCHECK(instructions[index].IsScript());
    const AXPropertyNode& property_node = instructions[index].AsScript();
    OptionalNSObject value = invoker.Invoke(property_node);
    if (value.IsUnsupported()) {
      continue;
    }

    base::Value result;
    if (value.IsError()) {
      result = base::Value(kFailedToParseError);
    } else if (value.IsNotApplicable()) {
      result = base::Value(kNotApplicable);
    } else {
      result = PopulateObject(*value, &indexer);
    }

    scripts.Append(property_node.ToString() + "=" + AXFormatValue(result));
  }

  std::string contents;
  for (const base::Value& script : scripts.GetList()) {
    std::string line;
    WriteAttribute(true, script.GetString(), &line);
    contents += line + "\n";
  }
  return contents;
}

base::Value AccessibilityTreeFormatterMac::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  DCHECK(node);
  BrowserAccessibility* internal_node =
      BrowserAccessibility::FromAXPlatformNodeDelegate(node);
  return BuildNode(ToBrowserAccessibilityCocoa(internal_node));
}

base::Value AccessibilityTreeFormatterMac::BuildNode(const id node) const {
  DCHECK(node);

  AXTreeIndexerMac indexer(node);
  base::Value dict(base::Value::Type::DICTIONARY);

  NSPoint position = AXPositionOf(node);
  NSSize size = AXSizeOf(node);
  NSRect rect = NSMakeRect(position.x, position.y, size.width, size.height);

  AddProperties(node, rect, &indexer, &dict);
  return dict;
}

void AccessibilityTreeFormatterMac::RecursiveBuildTree(
    const id node,
    const NSRect& root_rect,
    const AXTreeIndexerMac* indexer,
    base::Value* dict) const {
  BrowserAccessibility* platform_node =
      ui::IsNSAccessibilityElement(node)
          ? [static_cast<BrowserAccessibilityCocoa*>(node) owner]
          : nullptr;

  if (platform_node && !ShouldDumpNode(*platform_node))
    return;

  AddProperties(node, root_rect, indexer, dict);
  if (platform_node && !ShouldDumpChildren(*platform_node))
    return;

  NSArray* children = ui::AXChildrenOf(node);
  base::Value child_dict_list(base::Value::Type::LIST);
  for (id child in children) {
    base::Value child_dict(base::Value::Type::DICTIONARY);
    RecursiveBuildTree(child, root_rect, indexer, &child_dict);
    child_dict_list.Append(std::move(child_dict));
  }
  dict->SetPath(kChildrenDictAttr, std::move(child_dict_list));
}

void AccessibilityTreeFormatterMac::AddProperties(
    const id node,
    const NSRect& root_rect,
    const AXTreeIndexerMac* indexer,
    base::Value* dict) const {
  // Chromium special attributes.
  dict->SetPath(kLocalPositionDictAttr, PopulateLocalPosition(node, root_rect));

  // Dump all attributes if match-all filter is specified.
  if (HasMatchAllPropertyFilter()) {
    NSArray* attributes = AXAttributeNamesOf(node);
    for (NSString* attribute : attributes) {
      dict->SetPath(
          SysNSStringToUTF8(attribute),
          PopulateObject(AXAttributeValueOf(node, attribute), indexer));
    }
    return;
  }

  // Otherwise dump attributes matching allow filters only.
  std::string line_index = indexer->IndexBy(node);
  for (const AXPropertyNode& property_node :
       PropertyFilterNodesFor(line_index)) {
    AttributeInvoker invoker(node, indexer);
    OptionalNSObject value = invoker.Invoke(property_node);
    if (value.IsNotApplicable() || value.IsUnsupported()) {
      continue;
    }
    if (value.IsError()) {
      dict->SetPath(property_node.original_property,
                    base::Value(kFailedToParseError));
      continue;
    }
    dict->SetPath(property_node.original_property,
                  PopulateObject(*value, indexer));
  }
}

base::Value AccessibilityTreeFormatterMac::PopulateLocalPosition(
    const id node,
    const NSRect& root_rect) const {
  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and
  // less confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  int root_top = -static_cast<int>(root_rect.origin.y + root_rect.size.height);
  int root_left = static_cast<int>(root_rect.origin.x);

  NSPoint node_position = AXPositionOf(node);
  NSSize node_size = AXSizeOf(node);

  return AXNSPointToBaseValue(NSMakePoint(
      static_cast<int>(node_position.x - root_left),
      static_cast<int>(-node_position.y - node_size.height - root_top)));
}

base::Value AccessibilityTreeFormatterMac::PopulateObject(
    id value,
    const AXTreeIndexerMac* indexer) const {
  // NSArray
  if ([value isKindOfClass:[NSArray class]]) {
    return PopulateArray((NSArray*)value, indexer);
  }

  // AXTextMarker
  if (content::IsAXTextMarker(value)) {
    return PopulateTextPosition(content::AXTextMarkerToAXPosition(value),
                                indexer);
  }

  // AXTextMarkerRange
  if (content::IsAXTextMarkerRange(value))
    return PopulateTextMarkerRange(value, indexer);

  return AXNSObjectToBaseValue(value, indexer);
}

base::Value AccessibilityTreeFormatterMac::PopulateTextPosition(
    const BrowserAccessibility::AXPosition& position,
    const AXTreeIndexerMac* indexer) const {
  if (position->IsNullPosition())
    return AXNilToBaseValue();

  auto* manager = BrowserAccessibilityManager::FromID(position->tree_id());
  DCHECK(manager) << "A non-null position should have an associated AX tree.";
  BrowserAccessibility* anchor = manager->GetFromID(position->anchor_id());
  DCHECK(anchor) << "A non-null position should have a non-null anchor node.";
  BrowserAccessibilityCocoa* cocoa_anchor = ToBrowserAccessibilityCocoa(anchor);

  std::string affinity;
  switch (position->affinity()) {
    case ax::mojom::TextAffinity::kNone:
      affinity = "none";
      break;
    case ax::mojom::TextAffinity::kDownstream:
      affinity = "down";
      break;
    case ax::mojom::TextAffinity::kUpstream:
      affinity = "up";
      break;
  }

  base::Value set(base::Value::Type::DICTIONARY);
  set.SetPath(AXMakeSetKey(AXMakeOrderedKey("anchor", 0)),
              AXElementToBaseValue(cocoa_anchor, indexer));
  set.SetIntPath(AXMakeSetKey(AXMakeOrderedKey("offset", 1)),
                 position->text_offset());
  set.SetStringPath(AXMakeSetKey(AXMakeOrderedKey("affinity", 2)),
                    AXMakeConst(affinity));
  return set;
}

base::Value AccessibilityTreeFormatterMac::PopulateTextMarkerRange(
    id marker_range,
    const AXTreeIndexerMac* indexer) const {
  BrowserAccessibility::AXRange ax_range =
      content::AXTextMarkerRangeToAXRange(marker_range);
  if (ax_range.IsNull())
    return AXNilToBaseValue();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetPath("anchor",
               PopulateTextPosition(ax_range.anchor()->Clone(), indexer));
  dict.SetPath("focus",
               PopulateTextPosition(ax_range.focus()->Clone(), indexer));
  return dict;
}

base::Value AccessibilityTreeFormatterMac::PopulateArray(
    NSArray* node_array,
    const AXTreeIndexerMac* indexer) const {
  base::Value list(base::Value::Type::LIST);
  for (NSUInteger i = 0; i < [node_array count]; i++)
    list.Append(PopulateObject([node_array objectAtIndex:i], indexer));
  return list;
}

std::string AccessibilityTreeFormatterMac::ProcessTreeForOutput(
    const base::DictionaryValue& dict) const {
  std::string error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  std::string line;

  // AXRole and AXSubrole have own formatting and should be listed upfront.
  std::string role_attr = SysNSStringToUTF8(NSAccessibilityRoleAttribute);
  const std::string* value = dict.FindStringPath(role_attr);
  if (value) {
    WriteAttribute(true, *value, &line);
  }
  std::string subrole_attr = SysNSStringToUTF8(NSAccessibilitySubroleAttribute);
  value = dict.FindStringPath(subrole_attr);
  if (value) {
    WriteAttribute(false,
                   StringPrintf("%s=%s", subrole_attr.c_str(), value->c_str()),
                   &line);
  }

  // Expose all other attributes.
  for (auto item : dict.DictItems()) {
    if (item.second.is_string() &&
        (item.first == role_attr || item.first == subrole_attr)) {
      continue;
    }

    // Special case: children.
    // Children are used to generate the tree
    // itself, thus no sense to expose them on each node.
    if (item.first == kChildrenDictAttr) {
      continue;
    }

    // Write formatted value.
    std::string formatted_value = AXFormatValue(item.second);
    WriteAttribute(
        false,
        StringPrintf("%s=%s", item.first.c_str(), formatted_value.c_str()),
        &line);
  }

  return line;
}

}  // namespace content

#pragma clang diagnostic pop
