// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

#include "base/win/scoped_bstr.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/base/win/accessibility_misc_utils.h"

using Microsoft::WRL::ComPtr;

namespace ui {

// Helper macros for UIAutomation HRESULT expectations
#define EXPECT_UIA_INVALIDOPERATION(expr) \
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_INVALIDOPERATION), (expr))
#define EXPECT_INVALIDARG(expr) \
  EXPECT_EQ(static_cast<HRESULT>(E_INVALIDARG), (expr))

class AXPlatformNodeTextProviderTest : public ui::AXPlatformNodeWinTest {};

class MockIRawElementProviderSimple
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IRawElementProviderSimple {
 public:
  BEGIN_COM_MAP(MockIRawElementProviderSimple)
  COM_INTERFACE_ENTRY(IRawElementProviderSimple)
  END_COM_MAP()

  MockIRawElementProviderSimple() {}
  ~MockIRawElementProviderSimple() {}

  static HRESULT CreateMockIRawElementProviderSimple(
      IRawElementProviderSimple** provider) {
    CComObject<MockIRawElementProviderSimple>* raw_element_provider = nullptr;
    HRESULT hr = CComObject<MockIRawElementProviderSimple>::CreateInstance(
        &raw_element_provider);
    if (SUCCEEDED(hr)) {
      *provider = raw_element_provider;
    }

    return hr;
  }

  //
  // IRawElementProviderSimple methods.
  //
  IFACEMETHODIMP GetPatternProvider(PATTERNID pattern_id,
                                    IUnknown** result) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP GetPropertyValue(PROPERTYID property_id,
                                  VARIANT* result) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP
  get_ProviderOptions(enum ProviderOptions* ret) override { return E_NOTIMPL; }

  IFACEMETHODIMP
  get_HostRawElementProvider(IRawElementProviderSimple** provider) override {
    return E_NOTIMPL;
  }
};

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderRangeFromChild) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData empty_text_data;
  empty_text_data.id = 3;
  empty_text_data.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
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
  update.nodes.push_back(empty_text_data);

  Init(update);

  AXNode* root_node = GetRootNode();
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* text_node = root_node->children()[0];
  AXNode* empty_text_node = root_node->children()[1];

  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);
  ComPtr<IRawElementProviderSimple> text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text_node);
  ComPtr<IRawElementProviderSimple> empty_text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(empty_text_node);

  // Call RangeFromChild on the root with the text child passed in.
  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->RangeFromChild(text_node_raw.Get(), &text_range_provider));

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(static_cast<BSTR>(text_content), L"some text"));

  // Now test that the reverse relation doesn't return a valid
  // ITextRangeProvider, and instead returns E_INVALIDARG.
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_INVALIDARG(
      text_provider->RangeFromChild(root_node_raw.Get(), &text_range_provider));

  // Now test that a child with no text returns a degenerate range.
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_HRESULT_SUCCEEDED(text_provider->RangeFromChild(
      empty_text_node_raw.Get(), &text_range_provider));

  base::win::ScopedBstr empty_text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, empty_text_content.Receive()));
  EXPECT_EQ(0, wcscmp(static_cast<BSTR>(empty_text_content), L""));

  // Test that passing in an object from a different instance of
  // IRawElementProviderSimple than that of the valid text provider
  // returns UIA_E_INVALIDOPERATION.
  ComPtr<IRawElementProviderSimple> other_root_node_raw;
  MockIRawElementProviderSimple::CreateMockIRawElementProviderSimple(
      &other_root_node_raw);

  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_UIA_INVALIDOPERATION(text_provider->RangeFromChild(
      other_root_node_raw.Get(), &text_range_provider));
}

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

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderDocumentRangeNested) {
  ui::AXNodeData text_data;
  text_data.id = 3;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData paragraph_data;
  paragraph_data.id = 2;
  paragraph_data.role = ax::mojom::Role::kParagraph;
  paragraph_data.child_ids.push_back(3);

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  Init(root_data, paragraph_data, text_data);

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
