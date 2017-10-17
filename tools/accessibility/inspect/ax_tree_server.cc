// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_tree_server.h"

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

namespace content {

AXTreeServer::AXTreeServer(base::ProcessId pid) {
  std::unique_ptr<AccessibilityTreeFormatter> formatter(
      AccessibilityTreeFormatter::Create());

  // Get accessibility tree as nested dictionary.
  base::string16 accessibility_contents_utf16;
  std::unique_ptr<base::DictionaryValue> dict =
      formatter->BuildAccessibilityTreeForProcess(pid);

  if (!dict) {
    std::cout << "Failed to get accessibility tree";
    return;
  }

  Format(*formatter, *dict);
}

AXTreeServer::AXTreeServer(gfx::AcceleratedWidget widget) {
  std::unique_ptr<AccessibilityTreeFormatter> formatter(
      AccessibilityTreeFormatter::Create());

  // Get accessibility tree as nested dictionary.
  std::unique_ptr<base::DictionaryValue> dict =
      formatter->BuildAccessibilityTreeForWindow(widget);

  if (!dict) {
    std::cout << "Failed to get accessibility tree";
    return;
  }

  Format(*formatter, *dict);
}

void AXTreeServer::Format(AccessibilityTreeFormatter& formatter,
                          base::DictionaryValue& dict) {
  // Set filters.
  std::vector<AccessibilityTreeFormatter::Filter> filters;
  filters.push_back(AccessibilityTreeFormatter::Filter(
      base::ASCIIToUTF16("*"), AccessibilityTreeFormatter::Filter::ALLOW));
  formatter.SetFilters(filters);

  // Format accessibility tree as text.
  base::string16 accessibility_contents_utf16;
  formatter.FormatAccessibilityTree(dict, &accessibility_contents_utf16);

  // Write to console.
  std::cout << accessibility_contents_utf16;
}

}  // namespace content
