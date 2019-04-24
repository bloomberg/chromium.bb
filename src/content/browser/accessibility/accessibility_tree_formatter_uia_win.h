// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_
#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include <ole2.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>

#include "base/win/scoped_variant.h"
namespace content {

class AccessibilityTreeFormatterUia : public AccessibilityTreeFormatter {
 public:
  AccessibilityTreeFormatterUia();

  ~AccessibilityTreeFormatterUia() override;

  static std::unique_ptr<AccessibilityTreeFormatter> CreateUia();

  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override;

  void SetUpCommandLineForTestPass(base::CommandLine* command_line) override;

  const base::FilePath::StringType GetExpectedFileSuffix() override;

  const base::FilePath::StringType GetVersionSpecificExpectedFileSuffix()
      override;

  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* start) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForProcess(
      base::ProcessId pid) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForWindow(
      gfx::AcceleratedWidget hwnd) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForPattern(
      const base::StringPiece& pattern) override;

 private:
  const long min_property_id_ = UIA_RuntimeIdPropertyId;
  const long max_property_id_ = UIA_HeadingLevelPropertyId;
  void RecursiveBuildAccessibilityTree(IUIAutomationElement* node,
                                       base::DictionaryValue* dict);
  void BuildCacheRequests();
  void AddProperties(IUIAutomationElement* node, base::DictionaryValue* dict);
  void WriteProperty(long propertyId,
                     const base::win::ScopedVariant& var,
                     base::DictionaryValue* dict);
  // UIA enums have type I4, print formatted string for these when possible
  void WriteI4Property(long propertyId, long lval, base::DictionaryValue* dict);
  void WriteUnknownProperty(long propertyId,
                            IUnknown* unk,
                            base::DictionaryValue* dict);
  void WriteElementArray(long propertyId,
                         IUIAutomationElementArray* array,
                         base::DictionaryValue* dict);
  base::string16 GetNodeName(IUIAutomationElement* node);
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
  const std::string GetDenyNodeString() override;
  base::string16 ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) override;
  Microsoft::WRL::ComPtr<IUIAutomation> uia_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> element_cache_request_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> children_cache_request_;
};
}  // namespace content
#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_
