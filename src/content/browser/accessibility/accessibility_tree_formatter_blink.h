// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace content {

class BrowserAccessibility;

class CONTENT_EXPORT AccessibilityTreeFormatterBlink
    : public ui::AXTreeFormatterBase {
 public:
  explicit AccessibilityTreeFormatterBlink();
  ~AccessibilityTreeFormatterBlink() override;

  base::Value BuildTree(ui::AXPlatformNodeDelegate* root) const override;
  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;
  base::Value BuildTreeForNode(ui::AXNode* node) const override;
  std::string DumpInternalAccessibilityTree(
      ui::AXTreeID tree_id,
      const std::vector<AXPropertyFilter>& property_filters) override;

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  void RecursiveBuildTree(const BrowserAccessibility& node,
                          base::Value* dict) const;

  void RecursiveBuildTree(const ui::AXNode& node, base::Value* dict) const;

  base::Value BuildNode(ui::AXPlatformNodeDelegate* node) const override;

  void AddProperties(const BrowserAccessibility& node,
                     base::DictionaryValue* dict) const;

  void AddProperties(const ui::AXNode& node, base::DictionaryValue* dict) const;

  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_
