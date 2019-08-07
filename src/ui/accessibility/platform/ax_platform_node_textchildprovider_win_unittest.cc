// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_textchildprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

using Microsoft::WRL::ComPtr;

namespace ui {

class AXPlatformNodeTextChildProviderTest : public ui::AXPlatformNodeWinTest {
 protected:
  // Construct an accessibility tree for testing ITextChildProvider resolution
  // from various positions in the tree. The following tree configuration
  // is constructed:
  //
  // root_______________
  // |                  |
  // nontext____        text
  // |          |       |
  // nontext    text    text
  void SetUp() override {
    ui::AXNodeData root;
    root.id = 0;
    root.role = ax::mojom::Role::kRootWebArea;

    ui::AXNodeData nontext_child_of_root;
    nontext_child_of_root.id = 1;
    nontext_child_of_root.role = ax::mojom::Role::kGroup;
    root.child_ids.push_back(1);

    ui::AXNodeData text_child_of_root;
    text_child_of_root.id = 2;
    text_child_of_root.role = ax::mojom::Role::kStaticText;
    root.child_ids.push_back(2);

    ui::AXNodeData nontext_child_of_nontext;
    nontext_child_of_nontext.id = 3;
    nontext_child_of_nontext.role = ax::mojom::Role::kGroup;
    nontext_child_of_root.child_ids.push_back(3);

    ui::AXNodeData text_child_of_nontext;
    text_child_of_nontext.id = 4;
    text_child_of_nontext.role = ax::mojom::Role::kStaticText;
    nontext_child_of_root.child_ids.push_back(4);

    ui::AXNodeData text_child_of_text;
    text_child_of_text.id = 5;
    text_child_of_text.role = ax::mojom::Role::kStaticText;
    text_child_of_root.child_ids.push_back(5);

    ui::AXTreeUpdate update;
    ui::AXTreeData tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root.id;
    update.nodes.push_back(root);
    update.nodes.push_back(nontext_child_of_root);
    update.nodes.push_back(text_child_of_root);
    update.nodes.push_back(nontext_child_of_nontext);
    update.nodes.push_back(text_child_of_nontext);
    update.nodes.push_back(text_child_of_text);

    Init(update);

    AXNode* root_node = GetRootNode();
    AXNodePosition::SetTreeForTesting(tree_.get());
    AXNode* nontext_child_of_root_node = root_node->children()[0];
    AXNode* text_child_of_root_node = root_node->children()[1];
    AXNode* nontext_child_of_nontext_node =
        nontext_child_of_root_node->children()[0];
    AXNode* text_child_of_nontext_node =
        nontext_child_of_root_node->children()[1];
    AXNode* text_child_of_text_node = text_child_of_root_node->children()[0];

    InitITextChildProvider(root_node, root_provider_raw_,
                           root_text_child_provider_);
    InitITextChildProvider(nontext_child_of_root_node,
                           nontext_child_of_root_provider_raw_,
                           nontext_child_of_root_text_child_provider_);
    InitITextChildProvider(text_child_of_root_node,
                           text_child_of_root_text_provider_raw_,
                           text_child_of_root_text_child_provider_);
    InitITextChildProvider(nontext_child_of_nontext_node,
                           nontext_child_of_nontext_text_provider_raw_,
                           nontext_child_of_nontext_text_child_provider_);
    InitITextChildProvider(text_child_of_nontext_node,
                           text_child_of_nontext_text_provider_raw_,
                           text_child_of_nontext_text_child_provider_);
    InitITextChildProvider(text_child_of_text_node,
                           text_child_of_text_text_provider_raw_,
                           text_child_of_text_text_child_provider_);
  }

  void TearDown() override { AXNodePosition::SetTreeForTesting(nullptr); }

  void InitITextChildProvider(
      AXNode* node,
      ComPtr<IRawElementProviderSimple>& raw_element_provider,
      ComPtr<ITextChildProvider>& text_child_provider) {
    raw_element_provider =
        QueryInterfaceFromNode<IRawElementProviderSimple>(node);

    EXPECT_HRESULT_SUCCEEDED(raw_element_provider->GetPatternProvider(
        UIA_TextChildPatternId, &text_child_provider));

    // If the element does not support ITextChildProvider, create one anyways
    // for testing purposes.
    if (!text_child_provider) {
      ui::AXPlatformNodeWin* platform_node =
          (ui::AXPlatformNodeWin*)raw_element_provider.Get();
      ui::AXPlatformNodeTextChildProviderWin::CreateTextChildProvider(
          platform_node, &text_child_provider);
    }
  }

  ComPtr<IRawElementProviderSimple> root_provider_raw_;
  ComPtr<IRawElementProviderSimple> nontext_child_of_root_provider_raw_;
  ComPtr<IRawElementProviderSimple> text_child_of_root_text_provider_raw_;
  ComPtr<IRawElementProviderSimple> nontext_child_of_nontext_text_provider_raw_;
  ComPtr<IRawElementProviderSimple> text_child_of_nontext_text_provider_raw_;
  ComPtr<IRawElementProviderSimple> text_child_of_text_text_provider_raw_;

  ComPtr<ITextChildProvider> root_text_child_provider_;
  ComPtr<ITextChildProvider> nontext_child_of_root_text_child_provider_;
  ComPtr<ITextChildProvider> text_child_of_root_text_child_provider_;
  ComPtr<ITextChildProvider> nontext_child_of_nontext_text_child_provider_;
  ComPtr<ITextChildProvider> text_child_of_nontext_text_child_provider_;
  ComPtr<ITextChildProvider> text_child_of_text_text_child_provider_;
};

// ITextChildProvider::TextContainer Tests
//
// For each possible position in the tree verify:
// 1) A text container can/cannot be retrieved if an ancestor does/doesn't
//    support the UIA Text control pattern.
// 2) Any retrieved text container is the nearest ancestor text container.
// 3) A Text control can in fact be retrieved from any retrieved text
//    container.

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextContainerFromRoot) {
  CComPtr<IRawElementProviderSimple> text_container;
  ASSERT_HRESULT_SUCCEEDED(
      root_text_child_provider_->get_TextContainer(&text_container));
  ASSERT_EQ(nullptr, text_container);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextContainerFromNontextChildOfRoot) {
  CComPtr<IRawElementProviderSimple> text_container;
  ASSERT_HRESULT_SUCCEEDED(
      nontext_child_of_root_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container);

  ASSERT_EQ(root_provider_raw_.Get(), text_container);

  CComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(
      UIA_TextPatternId,
      reinterpret_cast<IUnknown**>(&text_container_text_provider));
  ASSERT_NE(nullptr, text_container_text_provider);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextContainerFromTextChildOfRoot) {
  CComPtr<IRawElementProviderSimple> text_container;
  ASSERT_HRESULT_SUCCEEDED(
      text_child_of_root_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container);

  ASSERT_EQ(root_provider_raw_.Get(), text_container);

  CComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(
      UIA_TextPatternId,
      reinterpret_cast<IUnknown**>(&text_container_text_provider));
  ASSERT_NE(nullptr, text_container_text_provider);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextContainerFromNontextChildOfNontext) {
  CComPtr<IRawElementProviderSimple> text_container;
  ASSERT_HRESULT_SUCCEEDED(
      nontext_child_of_nontext_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container);

  ASSERT_EQ(root_provider_raw_.Get(), text_container);

  CComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(
      UIA_TextPatternId,
      reinterpret_cast<IUnknown**>(&text_container_text_provider));
  ASSERT_NE(nullptr, text_container_text_provider);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextContainerFromTextChildOfNontext) {
  CComPtr<IRawElementProviderSimple> text_container;
  ASSERT_HRESULT_SUCCEEDED(
      text_child_of_nontext_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container);

  ASSERT_EQ(root_provider_raw_.Get(), text_container);

  CComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(
      UIA_TextPatternId,
      reinterpret_cast<IUnknown**>(&text_container_text_provider));
  ASSERT_NE(nullptr, text_container_text_provider);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextContainerFromTextChildOfText) {
  CComPtr<IRawElementProviderSimple> text_container;
  ASSERT_HRESULT_SUCCEEDED(
      text_child_of_text_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container);

  ASSERT_EQ(text_child_of_root_text_provider_raw_.Get(), text_container);

  CComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(
      UIA_TextPatternId,
      reinterpret_cast<IUnknown**>(&text_container_text_provider));
  ASSERT_NE(nullptr, text_container_text_provider);
}

// ITextChildProvider::TextRange Tests
//
// For each possible position in the tree verify:
// 1) A text range can/cannot be retrieved if an ancestor does/doesn't
//    support the UIA Text control pattern.
// 2) Any retrieved text range encloses the child element.
TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextRangeFromRoot) {
  CComPtr<ITextRangeProvider> text_range_provider;
  ASSERT_HRESULT_SUCCEEDED(
      root_text_child_provider_->get_TextRange(&text_range_provider));
  ASSERT_EQ(nullptr, text_range_provider);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextRangeFromNontextChildOfRoot) {
  CComPtr<ITextRangeProvider> text_range_provider;
  ASSERT_HRESULT_SUCCEEDED(
      nontext_child_of_root_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider);

  CComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  ASSERT_EQ(nontext_child_of_nontext_text_provider_raw_.Get(),
            enclosing_element);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextRangeFromTextChildOfRoot) {
  CComPtr<ITextRangeProvider> text_range_provider;
  ASSERT_HRESULT_SUCCEEDED(
      text_child_of_root_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider);

  CComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  ASSERT_EQ(text_child_of_text_text_provider_raw_.Get(), enclosing_element);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextRangeFromNontextChildOfNontext) {
  CComPtr<ITextRangeProvider> text_range_provider;
  ASSERT_HRESULT_SUCCEEDED(
      nontext_child_of_nontext_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider);

  CComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  ASSERT_EQ(nontext_child_of_nontext_text_provider_raw_.Get(),
            enclosing_element);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextRangeFromTextChildOfNontext) {
  CComPtr<ITextRangeProvider> text_range_provider;
  ASSERT_HRESULT_SUCCEEDED(
      text_child_of_nontext_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider);

  CComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  ASSERT_EQ(text_child_of_nontext_text_provider_raw_.Get(), enclosing_element);
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderTextRangeFromTextChildOfText) {
  CComPtr<ITextRangeProvider> text_range_provider;
  ASSERT_HRESULT_SUCCEEDED(
      text_child_of_text_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider);

  CComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  ASSERT_EQ(text_child_of_text_text_provider_raw_.Get(), enclosing_element);
}

// ITextChildProvider Tests - Inactive AX Tree
//
// Test that both ITextChildProvider::GetTextContainer and
// ITextChildProvider::GetTextContainer fail under an inactive AX tree.
TEST_F(AXPlatformNodeTextChildProviderTest,
       TestITextChildProviderInactiveAccessibilityTree) {
  tree_.reset();

  // Test that GetTextContainer fails under an inactive tree.
  CComPtr<IRawElementProviderSimple> text_container;
  HRESULT hr = nontext_child_of_root_text_child_provider_->get_TextContainer(
      &text_container);
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), hr);

  // Test that GetTextRange fails under an inactive tree.
  CComPtr<ITextRangeProvider> text_range_provider;
  hr = nontext_child_of_root_text_child_provider_->get_TextRange(
      &text_range_provider);
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), hr);
}

}  // namespace ui
