// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>
#include "base/win/atl.h"
#include "base/win/scoped_bstr.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"

using Microsoft::WRL::ComPtr;

namespace ui {

class AXPlatformNodeTextRangeProviderTest : public ui::AXPlatformNodeWinTest {};

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderGetText) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData more_text_data;
  more_text_data.id = 3;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.SetName("more text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);
  root_data.child_ids.push_back(3);

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data);
  update.nodes.push_back(more_text_data);

  Init(update);

  AXNode* root_node = GetRootNode();
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* text_node = root_node->children()[0];

  ComPtr<IRawElementProviderSimple> text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text_node);

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_STREQ(text_content, L"some text");
  text_content.Reset();

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(4, text_content.Receive()));
  EXPECT_STREQ(text_content, L"some");
  text_content.Reset();

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(0, text_content.Receive()));
  EXPECT_STREQ(text_content, L"");
  text_content.Reset();

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(9, text_content.Receive()));
  EXPECT_STREQ(text_content, L"some text");
  text_content.Reset();

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(10, text_content.Receive()));
  EXPECT_STREQ(text_content, L"some text");
  text_content.Reset();

  EXPECT_HRESULT_FAILED(text_range_provider->GetText(-1, nullptr));

  EXPECT_HRESULT_FAILED(
      text_range_provider->GetText(-2, text_content.Receive()));
  text_content.Reset();

  Microsoft::WRL::ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);

  Microsoft::WRL::ComPtr<ITextProvider> document_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &document_provider));

  ComPtr<ITextRangeProvider> document_textrange;
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&document_textrange));

  EXPECT_HRESULT_SUCCEEDED(
      document_textrange->GetText(-1, text_content.Receive()));
  EXPECT_STREQ(text_content, L"some textmore text");
  text_content.Reset();

  AXNodePosition::SetTreeForTesting(nullptr);
}

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderSelection) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  Init(root_data, text_data);

  AXNode* root_node = GetRootNode();
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);

  ComPtr<ITextProvider> document_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &document_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&text_range_provider));

  ASSERT_EQ(text_range_provider->AddToSelection(),
            static_cast<HRESULT>(UIA_E_INVALIDOPERATION));
  ASSERT_EQ(text_range_provider->RemoveFromSelection(),
            static_cast<HRESULT>(UIA_E_INVALIDOPERATION));

  AXNodePosition::SetTreeForTesting(nullptr);
}

}  // namespace ui
