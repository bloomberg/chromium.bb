// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include "base/win/scoped_bstr.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/base/win/accessibility_misc_utils.h"

using Microsoft::WRL::ComPtr;

namespace ui {

class AXPlatformNodeTextProviderTest : public ui::AXPlatformNodeWinTest {};

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderDocumentRange) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  Init(root_data, text_data);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));
}

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderSupportedSelection) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  Init(root_data, text_data);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  SupportedTextSelection text_selection_mode;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_SupportedTextSelection(&text_selection_mode));
  EXPECT_EQ(text_selection_mode, SupportedTextSelection_Single);
}

}  // namespace ui
