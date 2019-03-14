// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>
#include <atlsafe.h>

#include "base/win/atl.h"
#include "base/win/scoped_bstr.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"
using Microsoft::WRL::ComPtr;

namespace ui {

#define EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(safearray, expected_property_values) \
  {                                                                         \
    EXPECT_EQ(8U, ::SafeArrayGetElemsize(rectangles));                      \
    ASSERT_EQ(1u, SafeArrayGetDim(safearray));                              \
    LONG array_lower_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));              \
    LONG array_upper_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));              \
    double* array_data;                                                     \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                         \
        safearray, reinterpret_cast<void**>(&array_data)));                 \
    size_t count = array_upper_bound - array_lower_bound + 1;               \
    ASSERT_EQ(expected_property_values.size(), count);                      \
    for (size_t i = 0; i < count; ++i) {                                    \
      EXPECT_EQ(array_data[i], expected_property_values[i]);                \
    }                                                                       \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));           \
  }

class AXPlatformNodeTextRangeProviderTest : public ui::AXPlatformNodeWinTest {
 public:
  const AXNodePosition::AXPositionInstance& GetStart(
      const AXPlatformNodeTextRangeProviderWin* text_range) {
    return text_range->start_;
  }
  const AXNodePosition::AXPositionInstance& GetEnd(
      const AXPlatformNodeTextRangeProviderWin* text_range) {
    return text_range->end_;
  }
  ui::AXPlatformNodeWin* GetOwner(
      const AXPlatformNodeTextRangeProviderWin* text_range) {
    return text_range->owner_;
  }
};

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderClone) {
  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");
  root_data.child_ids.push_back(2);

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data);
  Init(update);

  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<IRawElementProviderSimple> text_node_raw =
      QueryInterfaceFromNodeId<IRawElementProviderSimple>(text_data.id);

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

  ComPtr<ITextRangeProvider> text_range_provider_clone;
  text_range_provider->Clone(&text_range_provider_clone);

  ComPtr<AXPlatformNodeTextRangeProviderWin> original_range;
  ComPtr<AXPlatformNodeTextRangeProviderWin> clone_range;

  text_range_provider->QueryInterface(IID_PPV_ARGS(&original_range));
  text_range_provider->QueryInterface(IID_PPV_ARGS(&clone_range));

  EXPECT_EQ(GetStart(original_range.Get()), GetStart(clone_range.Get()));
  EXPECT_EQ(GetEnd(original_range.Get()), GetEnd(clone_range.Get()));
  EXPECT_EQ(GetOwner(original_range.Get()), GetOwner(clone_range.Get()));

  // Clear original text range provider.
  text_range_provider.Reset();
  EXPECT_EQ(nullptr, text_range_provider.Get());

  // Ensure the clone still works correctly.
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider_clone->GetText(-1, text_content.Receive()));
  EXPECT_STREQ(text_content, L"some text");
  text_content.Reset();
}

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

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderGetBoundingRectangles) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.relative_bounds.bounds = gfx::RectF(100, 150, 200, 200);
  text_data.SetName("some text");

  ui::AXNodeData more_text_data;
  more_text_data.id = 3;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.relative_bounds.bounds = gfx::RectF(200, 250, 100, 100);
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
  AXTreeManagerMap::GetInstance().AddTreeManager(tree_data.tree_id, this);
  AXNode* text_node = root_node->children()[0];

  ComPtr<IRawElementProviderSimple> text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text_node);

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  CComSafeArray<LONG> rectangles;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.GetSafeArrayPtr()));

  std::vector<double> expected_values = {100, 150, 200, 200};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles, expected_values);

  Microsoft::WRL::ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);

  Microsoft::WRL::ComPtr<ITextProvider> document_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &document_provider));

  ComPtr<ITextRangeProvider> document_textrange;
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&document_textrange));

  CComSafeArray<LONG> body_rectangles;
  EXPECT_HRESULT_SUCCEEDED(document_textrange->GetBoundingRectangles(
      body_rectangles.GetSafeArrayPtr()));
  expected_values = {100, 150, 200, 200, 200, 250, 100, 100};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(body_rectangles, expected_values);

  AXTreeManagerMap::GetInstance().RemoveTreeManager(tree_data.tree_id);
  AXNodePosition::SetTreeForTesting(nullptr);
}

}  // namespace ui
