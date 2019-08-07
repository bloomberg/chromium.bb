// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

#include "base/win/atl.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"
using Microsoft::WRL::ComPtr;

namespace ui {

// Helper macros for UIAutomation HRESULT expectations
#define EXPECT_UIA_ELEMENTNOTAVAILABLE(expr) \
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), (expr))
#define EXPECT_UIA_INVALIDOPERATION(expr) \
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_INVALIDOPERATION), (expr))
#define EXPECT_UIA_ELEMENTNOTENABLED(expr) \
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTENABLED), (expr))
#define EXPECT_UIA_NOTSUPPORTED(expr) \
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED), (expr))

#define ASSERT_UIA_ELEMENTNOTAVAILABLE(expr) \
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), (expr))
#define ASSERT_UIA_INVALIDOPERATION(expr) \
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_INVALIDOPERATION), (expr))
#define ASSERT_UIA_ELEMENTNOTENABLED(expr) \
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTENABLED), (expr))
#define ASSERT_UIA_NOTSUPPORTED(expr) \
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED), (expr))

#define EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(safearray, expected_property_values) \
  {                                                                         \
    EXPECT_EQ(sizeof(V_R8(LPVARIANT(NULL))),                                \
              ::SafeArrayGetElemsize(safearray));                           \
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

#define EXPECT_UIA_VT_UNKNOWN_SAFEARRAY_EQ(safearray,                    \
                                           expected_property_values)     \
  {                                                                      \
    EXPECT_EQ(sizeof(V_UNKNOWN(LPVARIANT(NULL))),                        \
              ::SafeArrayGetElemsize(safearray));                        \
    EXPECT_EQ(1u, SafeArrayGetDim(safearray));                           \
    LONG array_lower_bound;                                              \
    EXPECT_HRESULT_SUCCEEDED(                                            \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));           \
    LONG array_upper_bound;                                              \
    EXPECT_HRESULT_SUCCEEDED(                                            \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));           \
    ComPtr<IRawElementProviderSimple>* array_data;                       \
    EXPECT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                      \
        safearray, reinterpret_cast<void**>(&array_data)));              \
    size_t count = array_upper_bound - array_lower_bound + 1;            \
    EXPECT_EQ(expected_property_values.size(), count);                   \
    for (size_t i = 0; i < count; ++i) {                                 \
      EXPECT_EQ(array_data[i].Get(), expected_property_values[i].Get()); \
    }                                                                    \
    EXPECT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));        \
  }

#define EXPECT_UIA_TEXTATTRIBUTE_EQ(provider, attribute, variant)          \
  {                                                                        \
    base::win::ScopedVariant scoped_variant;                               \
    EXPECT_HRESULT_SUCCEEDED(                                              \
        provider->GetAttributeValue(attribute, scoped_variant.Receive())); \
    EXPECT_EQ(0, scoped_variant.Compare(variant));                         \
  }

#define EXPECT_UIA_TEXTRANGE_EQ(provider, expected_content) \
  {                                                         \
    base::win::ScopedBstr provider_content;                 \
    ASSERT_HRESULT_SUCCEEDED(                               \
        provider->GetText(-1, provider_content.Receive())); \
    EXPECT_STREQ(expected_content, provider_content);       \
  }

#define EXPECT_UIA_FIND_TEXT(text_range_provider, search_term, ignore_case) \
  {                                                                         \
    base::win::ScopedBstr find_string(search_term);                         \
    CComPtr<ITextRangeProvider> text_range_provider_found;                  \
    EXPECT_HRESULT_SUCCEEDED(text_range_provider->FindText(                 \
        find_string, false, ignore_case, &text_range_provider_found));      \
    base::win::ScopedBstr found_content;                                    \
    EXPECT_HRESULT_SUCCEEDED(                                               \
        text_range_provider_found->GetText(-1, found_content.Receive()));   \
    if (ignore_case)                                                        \
      EXPECT_EQ(0, _wcsicmp(found_content, find_string));                   \
    else                                                                    \
      EXPECT_EQ(0, wcscmp(found_content, find_string));                     \
  }

#define EXPECT_UIA_FIND_TEXT_NO_MATCH(text_range_provider, search_term, \
                                      ignore_case)                      \
  {                                                                     \
    base::win::ScopedBstr find_string(search_term);                     \
    CComPtr<ITextRangeProvider> text_range_provider_found;              \
    EXPECT_HRESULT_SUCCEEDED(text_range_provider->FindText(             \
        find_string, false, ignore_case, &text_range_provider_found));  \
    EXPECT_EQ(nullptr, text_range_provider_found);                      \
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

  void GetTextRangeProviderFromTextNode(
      ComPtr<ITextRangeProvider>& text_range_provider,
      ui::AXNode* text_node) {
    ComPtr<IRawElementProviderSimple> provider_simple =
        QueryInterfaceFromNode<IRawElementProviderSimple>(text_node);
    ASSERT_NE(nullptr, provider_simple.Get());

    ComPtr<ITextProvider> text_provider;
    EXPECT_HRESULT_SUCCEEDED(
        provider_simple->GetPatternProvider(UIA_TextPatternId, &text_provider));
    ASSERT_NE(nullptr, text_provider.Get());

    EXPECT_HRESULT_SUCCEEDED(
        text_provider->get_DocumentRange(&text_range_provider));
    ASSERT_NE(nullptr, text_range_provider.Get());
  }

  void ComputeWordBoundariesOffsets(const std::string& text,
                                    std::vector<int>& word_start_offsets,
                                    std::vector<int>& word_end_offsets) {
    char previous_char = ' ';
    word_start_offsets = std::vector<int>();
    for (size_t i = 0; i < text.size(); ++i) {
      if (previous_char == ' ' && text[i] != ' ')
        word_start_offsets.push_back(i);
      previous_char = text[i];
    }

    previous_char = ' ';
    word_end_offsets = std::vector<int>();
    for (size_t i = text.size(); i > 0; --i) {
      if (previous_char == ' ' && text[i - 1] != ' ')
        word_end_offsets.push_back(i);
      previous_char = text[i - 1];
    }
    std::reverse(word_end_offsets.begin(), word_end_offsets.end());
  }

  ui::AXTreeUpdate BuildTextDocument(
      const std::vector<std::string>& text_nodes_content,
      bool build_word_boundaries_offsets = false) {
    int current_id = 0;
    ui::AXNodeData root_data;
    root_data.id = ++current_id;
    root_data.role = ax::mojom::Role::kRootWebArea;

    ui::AXTreeUpdate update;
    update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.has_tree_data = true;

    for (const std::string& text_content : text_nodes_content) {
      ui::AXNodeData text_data;
      text_data.id = ++current_id;
      text_data.role = ax::mojom::Role::kStaticText;
      text_data.SetName(text_content);

      if (build_word_boundaries_offsets) {
        std::vector<int> word_end_offsets;
        std::vector<int> word_start_offsets;
        ComputeWordBoundariesOffsets(text_content, word_start_offsets,
                                     word_end_offsets);
        text_data.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                      word_start_offsets);
        text_data.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                      word_end_offsets);
      }

      root_data.child_ids.push_back(text_data.id);
      update.nodes.push_back(text_data);
    }

    update.nodes.insert(update.nodes.begin(), root_data);
    update.root_id = root_data.id;
    return update;
  }
};

class MockAXPlatformNodeTextRangeProviderWin
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ITextRangeProvider {
 public:
  BEGIN_COM_MAP(MockAXPlatformNodeTextRangeProviderWin)
  COM_INTERFACE_ENTRY(ITextRangeProvider)
  END_COM_MAP()

  MockAXPlatformNodeTextRangeProviderWin() {}
  ~MockAXPlatformNodeTextRangeProviderWin() {}

  static HRESULT CreateMockTextRangeProvider(ITextRangeProvider** provider) {
    CComObject<MockAXPlatformNodeTextRangeProviderWin>* text_range_provider =
        nullptr;
    HRESULT hr =
        CComObject<MockAXPlatformNodeTextRangeProviderWin>::CreateInstance(
            &text_range_provider);
    if (SUCCEEDED(hr)) {
      *provider = text_range_provider;
    }

    return hr;
  }

  //
  // ITextRangeProvider methods.
  //
  STDMETHODIMP Clone(ITextRangeProvider** clone) override { return E_NOTIMPL; }

  STDMETHODIMP Compare(ITextRangeProvider* other, BOOL* result) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP CompareEndpoints(TextPatternRangeEndpoint this_endpoint,
                                ITextRangeProvider* other,
                                TextPatternRangeEndpoint other_endpoint,
                                int* result) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP ExpandToEnclosingUnit(TextUnit unit) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP FindAttribute(TEXTATTRIBUTEID attribute_id,
                             VARIANT val,
                             BOOL backward,
                             ITextRangeProvider** result) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP FindText(BSTR string,
                        BOOL backwards,
                        BOOL ignore_case,
                        ITextRangeProvider** result) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP GetAttributeValue(TEXTATTRIBUTEID attribute_id,
                                 VARIANT* value) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP GetBoundingRectangles(SAFEARRAY** rectangles) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP GetEnclosingElement(
      IRawElementProviderSimple** element) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP GetText(int max_count, BSTR* text) override { return E_NOTIMPL; }

  STDMETHODIMP Move(TextUnit unit, int count, int* units_moved) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP MoveEndpointByUnit(TextPatternRangeEndpoint endpoint,
                                  TextUnit unit,
                                  int count,
                                  int* units_moved) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP MoveEndpointByRange(
      TextPatternRangeEndpoint this_endpoint,
      ITextRangeProvider* other,
      TextPatternRangeEndpoint other_endpoint) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP Select() override { return E_NOTIMPL; }

  STDMETHODIMP AddToSelection() override { return E_NOTIMPL; }

  STDMETHODIMP RemoveFromSelection() override { return E_NOTIMPL; }

  STDMETHODIMP ScrollIntoView(BOOL align_to_top) override { return E_NOTIMPL; }

  STDMETHODIMP GetChildren(SAFEARRAY** children) override { return E_NOTIMPL; }
};

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderClone) {
  Init(BuildTextDocument({"some text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   GetRootNode()->children()[0]);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"some text");

  ComPtr<ITextRangeProvider> text_range_provider_clone;
  text_range_provider->Clone(&text_range_provider_clone);

  ComPtr<AXPlatformNodeTextRangeProviderWin> original_range;
  ComPtr<AXPlatformNodeTextRangeProviderWin> clone_range;

  text_range_provider->QueryInterface(IID_PPV_ARGS(&original_range));
  text_range_provider_clone->QueryInterface(IID_PPV_ARGS(&clone_range));

  EXPECT_EQ(*GetStart(original_range.Get()), *GetStart(clone_range.Get()));
  EXPECT_EQ(*GetEnd(original_range.Get()), *GetEnd(clone_range.Get()));
  EXPECT_EQ(GetOwner(original_range.Get()), GetOwner(clone_range.Get()));

  // Clear original text range provider.
  text_range_provider.Reset();
  EXPECT_EQ(nullptr, text_range_provider.Get());

  // Ensure the clone still works correctly.
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider_clone, L"some text");
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderCompareEndpoints) {
  Init(BuildTextDocument({"some text", "more text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* root_node = GetRootNode();

  // Get the textRangeProvider for the document,
  // which contains text "some textmore text".
  ComPtr<ITextRangeProvider> document_text_range_provider;
  GetTextRangeProviderFromTextNode(document_text_range_provider, root_node);

  // Get the textRangeProvider for "some text".
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   root_node->children()[0]);

  // Get the textRangeProvider for "more text".
  ComPtr<ITextRangeProvider> more_text_range_provider;
  GetTextRangeProviderFromTextNode(more_text_range_provider,
                                   root_node->children()[1]);

  // Compare the endpoints of the document which contains "some textmore text".
  int result;
  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_Start, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_Start, &result));
  EXPECT_EQ(0, result);

  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_End, &result));
  EXPECT_EQ(0, result);

  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_Start, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_End, &result));
  EXPECT_EQ(-1, result);

  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_Start, &result));
  EXPECT_EQ(1, result);

  // Compare the endpoints of "some text" and "more text". The position at the
  // end of "some text" is logically equivalent to the position at the start of
  // "more text".
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_Start, more_text_range_provider.Get(),
      TextPatternRangeEndpoint_Start, &result));
  EXPECT_EQ(-1, result);

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, more_text_range_provider.Get(),
      TextPatternRangeEndpoint_Start, &result));
  EXPECT_EQ(0, result);

  // Compare the endpoints of "some text" with those of the entire document. The
  // position at the start of "some text" is logically equivalent to the
  // position at the start of the document.
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_Start, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_Start, &result));
  EXPECT_EQ(0, result);

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_End, &result));
  EXPECT_EQ(-1, result);

  // Compare the endpoints of "more text" with those of the entire document.
  EXPECT_HRESULT_SUCCEEDED(more_text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_Start, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_Start, &result));
  EXPECT_EQ(1, result);

  EXPECT_HRESULT_SUCCEEDED(more_text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_End, &result));
  EXPECT_EQ(0, result);
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderExpandToEnclosingCharacter) {
  Init(BuildTextDocument({"some text", "more text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, GetRootNode());
  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Character));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"s");

  int count;
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 2, &count));
  ASSERT_EQ(2, count);
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 1, &count));
  ASSERT_EQ(1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"om");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Character));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"o");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 10, &count));
  ASSERT_EQ(10, count);
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 9, &count));
  ASSERT_EQ(9, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"mo");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Character));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"m");

  // Move the start and end to the end of the document.
  // Expand to enclosing unit should never return a null position.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 9, &count));
  ASSERT_EQ(8, count);
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 9, &count));
  ASSERT_EQ(9, count);

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Character));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderExpandToEnclosingWord) {
  Init(BuildTextDocument({"some text", "definitely not text"},
                         /*build_word_boundaries_offsets*/ true));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   GetRootNode()->children()[1]);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"definitely not text");

  // Start endpoint is already on a word's start boundary.
  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Word));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"definitely");

  // Start endpoint is between a word's start and end boundaries.
  int count;
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ -2,
      &count));
  ASSERT_EQ(-2, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"xtdefinitely");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 5, &count));
  ASSERT_EQ(5, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"xtdefinitely not ");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Word));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"text");

  // Start endpoint is on a word's end boundary.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 19,
      &count));
  ASSERT_EQ(19, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 1, &count));
  ASSERT_EQ(1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L" ");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Word));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"not");
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderExpandToEnclosingLine) {
  Init(BuildTextDocument({"line #1", "maybe line #1?", "not line #1"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   GetRootNode()->children()[0]);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"line #1");

  // Start endpoint is already on a line's start boundary.
  int count;
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ -11, &count));
  ASSERT_EQ(-7, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Line));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"line #1");

  // Start endpoint is between a line's start and end boundaries.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 14,
      &count));
  ASSERT_EQ(14, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 4, &count));
  ASSERT_EQ(4, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"line");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Line));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"maybe line #1?");

  // Start endpoint is on a line's end boundary.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 29,
      &count));
  ASSERT_EQ(26, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Line));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"not line #1");
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderExpandToEnclosingDocument) {
  Init(BuildTextDocument({"some text", "more text", "even more text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  AXNode* root_node = GetRootNode();
  AXNode* text_node = root_node->children()[0];
  AXNode* more_text_node = root_node->children()[1];
  AXNode* even_more_text_node = root_node->children()[2];

  // Run the test twice, one for TextUnit_Document and once for TextUnit_Page,
  // since they should have identical behavior.
  const TextUnit textunit_types[] = {TextUnit_Document, TextUnit_Page};
  ComPtr<ITextRangeProvider> text_range_provider;

  for (auto& textunit : textunit_types) {
    GetTextRangeProviderFromTextNode(text_range_provider, text_node);
    ASSERT_HRESULT_SUCCEEDED(
        text_range_provider->ExpandToEnclosingUnit(textunit));
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                            L"some textmore texteven more text");

    GetTextRangeProviderFromTextNode(text_range_provider, more_text_node);
    ASSERT_HRESULT_SUCCEEDED(
        text_range_provider->ExpandToEnclosingUnit(textunit));
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                            L"some textmore texteven more text");

    GetTextRangeProviderFromTextNode(text_range_provider, even_more_text_node);
    ASSERT_HRESULT_SUCCEEDED(
        text_range_provider->ExpandToEnclosingUnit(textunit));
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                            L"some textmore texteven more text");
  }
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderInvalidCalls) {
  // Test for when a text range provider is invalid. Because no ax tree is
  // available, the anchor is invalid, so the text range provider fails the
  // validate call.
  {
    Init(BuildTextDocument({}));
    AXNodePosition::SetTreeForTesting(nullptr);

    ComPtr<ITextRangeProvider> text_range_provider;
    GetTextRangeProviderFromTextNode(text_range_provider, GetRootNode());

    ComPtr<ITextRangeProvider> text_range_provider_clone;
    EXPECT_UIA_ELEMENTNOTAVAILABLE(
        text_range_provider->Clone(&text_range_provider_clone));

    BOOL compare_result;
    EXPECT_UIA_ELEMENTNOTAVAILABLE(text_range_provider->Compare(
        text_range_provider.Get(), &compare_result));

    int compare_endpoints_result;
    EXPECT_UIA_ELEMENTNOTAVAILABLE(text_range_provider->CompareEndpoints(
        TextPatternRangeEndpoint_Start, text_range_provider.Get(),
        TextPatternRangeEndpoint_Start, &compare_endpoints_result));

    VARIANT attr_val;
    V_VT(&attr_val) = VT_BOOL;
    V_BOOL(&attr_val) = VARIANT_TRUE;
    ComPtr<ITextRangeProvider> matched_range_provider;
    EXPECT_UIA_ELEMENTNOTAVAILABLE(text_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, attr_val, true, &matched_range_provider));

    EXPECT_UIA_ELEMENTNOTAVAILABLE(text_range_provider->MoveEndpointByRange(
        TextPatternRangeEndpoint_Start, text_range_provider.Get(),
        TextPatternRangeEndpoint_Start));

    EXPECT_UIA_ELEMENTNOTAVAILABLE(text_range_provider->Select());
  }

  // Test for when this provider is valid, but the other provider is not an
  // instance of AXPlatformNodeTextRangeProviderWin, so no operation can be
  // performed on the other provider.
  {
    Init(BuildTextDocument({}));
    AXNodePosition::SetTreeForTesting(tree_.get());

    ComPtr<ITextRangeProvider> this_provider;
    GetTextRangeProviderFromTextNode(this_provider, GetRootNode());

    ComPtr<ITextRangeProvider> other_provider_different_type;
    MockAXPlatformNodeTextRangeProviderWin::CreateMockTextRangeProvider(
        &other_provider_different_type);

    BOOL compare_result;
    EXPECT_UIA_INVALIDOPERATION(this_provider->Compare(
        other_provider_different_type.Get(), &compare_result));

    int compare_endpoints_result;
    EXPECT_UIA_INVALIDOPERATION(this_provider->CompareEndpoints(
        TextPatternRangeEndpoint_Start, other_provider_different_type.Get(),
        TextPatternRangeEndpoint_Start, &compare_endpoints_result));

    EXPECT_UIA_INVALIDOPERATION(this_provider->MoveEndpointByRange(
        TextPatternRangeEndpoint_Start, other_provider_different_type.Get(),
        TextPatternRangeEndpoint_Start));
  }
}

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderGetText) {
  Init(BuildTextDocument({"some text", "more text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  AXNode* root_node = GetRootNode();
  AXNode* text_node = root_node->children()[0];

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, text_node);

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

  ComPtr<ITextRangeProvider> document_textrange;
  GetTextRangeProviderFromTextNode(document_textrange, root_node);

  EXPECT_HRESULT_SUCCEEDED(
      document_textrange->GetText(-1, text_content.Receive()));
  EXPECT_STREQ(text_content, L"some textmore text");
  text_content.Reset();
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderMoveEndpointByDocument) {
  Init(BuildTextDocument({"some text", "more text", "even more text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* text_node = GetRootNode()->children()[1];

  // Run the test twice, one for TextUnit_Document and once for TextUnit_Page,
  // since they should have identical behavior.
  const TextUnit textunit_types[] = {TextUnit_Document, TextUnit_Page};
  ComPtr<ITextRangeProvider> text_range_provider;

  for (auto& textunit : textunit_types) {
    GetTextRangeProviderFromTextNode(text_range_provider, text_node);

    // Verify MoveEndpointByUnit with zero count has no effect
    int count;
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
        TextPatternRangeEndpoint_End, textunit, /*count*/ 0, &count));
    ASSERT_EQ(0, count);

    // Move the endpoint to the end of the document. Verify all text content.
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
        TextPatternRangeEndpoint_End, textunit, /*count*/ 1, &count));
    ASSERT_EQ(1, count);
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more texteven more text");

    // Verify no moves occur since the end is already at the end of the document
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
        TextPatternRangeEndpoint_End, textunit, /*count*/ 5, &count));
    ASSERT_EQ(0, count);
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more texteven more text");

    // Move the end before the start
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
        TextPatternRangeEndpoint_End, textunit, /*count*/ -4, &count));
    ASSERT_EQ(-1, count);
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

    // Move the end back to the end of the document. The text content
    // should now include the entire document since end was previously
    // moved before start.
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
        TextPatternRangeEndpoint_End, textunit, /*count*/ 1, &count));
    ASSERT_EQ(1, count);
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                            L"some textmore texteven more text");

    // Move the start point to the end
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
        TextPatternRangeEndpoint_Start, textunit, /*count*/ 3, &count));
    ASSERT_EQ(1, count);
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

    // Move the start point back to the beginning
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
        TextPatternRangeEndpoint_Start, textunit, /*count*/ -3, &count));
    ASSERT_EQ(-1, count);
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                            L"some textmore texteven more text");
  }
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderMoveEndpointByCharacter) {
  Init(BuildTextDocument({"some text", "more text", "even more text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   GetRootNode()->children()[0]);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"some text");

  // Verify MoveEndpointByUnit with zero count has no effect
  int count;
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 0, &count));
  ASSERT_EQ(0, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"some text");

  // Test start and end node single-unit moves within a single node
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 1, &count));
  ASSERT_EQ(1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"ome text");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ -1, &count));
  ASSERT_EQ(-1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"ome tex");

  // Test start and end node multi-unit moves within a single node
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 2, &count));
  ASSERT_EQ(2, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"e tex");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ -3, &count));
  ASSERT_EQ(-3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"e ");

  // Move end to before start - ensure count is truncated
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ -10, &count));
  ASSERT_EQ(-5, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  // Move end back out - ensure both start and end were moved
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 4, &count));
  ASSERT_EQ(4, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"some");

  // Move start past end, ensure a degenerate range is created
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 7, &count));
  ASSERT_EQ(7, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  // Move start back to its prior position and verify that end was also moved
  // as part of moving start past end
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ -7,
      &count));
  ASSERT_EQ(-7, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"some te");

  // Move end into the adjacent node
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 3, &count));
  ASSERT_EQ(3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"some text");

  // Move end to the end of the document, ensure truncated count
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 30, &count));
  ASSERT_EQ(24, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          L"some textmore texteven more text");

  // Move start beyond end, ensure truncated count and degenerate range
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Character, /*count*/ 40,
      &count));
  ASSERT_EQ(34, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  // Move end before start, ensure both positions are moved
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ -10, &count));
  ASSERT_EQ(-10, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Character, /*count*/ 5, &count));
  ASSERT_EQ(5, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L" more");
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderMoveEndpointByWord) {
  Init(BuildTextDocument({"some text", "more text", "even more text"},
                         /*build_word_boundaries_offsets*/ true));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   GetRootNode()->children()[1]);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more text");

  // Moving with zero count does not alter the range.
  int count;
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ 0, &count));
  ASSERT_EQ(0, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more text");

  // Moving the start forward and backward.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Word, /*count*/ 1, &count));
  ASSERT_EQ(1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"text");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Word, /*count*/ -1, &count));
  ASSERT_EQ(-1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more text");

  // Moving the end backward and forward.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ -1, &count));
  ASSERT_EQ(-1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ 1, &count));
  ASSERT_EQ(1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more text");

  // Moving the start past the end, then reverting.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Word, /*count*/ 3, &count));
  ASSERT_EQ(3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Word, /*count*/ -3, &count));
  ASSERT_EQ(-3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more texteven ");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ -2, &count));
  ASSERT_EQ(-2, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more text");

  // Moving the end past the start, then reverting.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ -3, &count));
  ASSERT_EQ(-3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ 3, &count));
  ASSERT_EQ(3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L" textmore text");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Word, /*count*/ 2, &count));
  ASSERT_EQ(2, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more text");

  // Moving the endpoints further than both ends of the document.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ 5, &count));
  ASSERT_EQ(3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"more texteven more text");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Word, /*count*/ 6, &count));
  ASSERT_EQ(4, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"text");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Word, /*count*/ -7, &count));
  ASSERT_EQ(-6, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          L"some textmore texteven more text");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Word, /*count*/ -8, &count));
  ASSERT_EQ(-6, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"some");
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderMoveEndpointByLine) {
  Init(BuildTextDocument({"0", "1", "2", "3", "4", "5", "6"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   GetRootNode()->children()[3]);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"3");

  // Moving with zero count does not alter the range.
  int count;
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Line, /*count*/ 0, &count));
  ASSERT_EQ(0, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"3");

  // Moving the start backward and forward.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Line, /*count*/ -2, &count));
  ASSERT_EQ(-2, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"123");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Line, /*count*/ 1, &count));
  ASSERT_EQ(1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"23");

  // Moving the end forward and backward.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Line, /*count*/ 3, &count));
  ASSERT_EQ(3, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"23456");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Line, /*count*/ -2, &count));
  ASSERT_EQ(-2, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"234");

  // Moving the end past the start and vice versa.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Line, /*count*/ -4, &count));
  ASSERT_EQ(-4, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Line, /*count*/ -1, &count));
  ASSERT_EQ(-1, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"0");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Line, /*count*/ 6, &count));
  ASSERT_EQ(6, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Line, /*count*/ -6, &count));
  ASSERT_EQ(-6, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"012345");

  // Moving the endpoints further than both ends of the document.
  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Line, /*count*/ -13, &count));
  ASSERT_EQ(-5, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"0");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_End, TextUnit_Line, /*count*/ 11, &count));
  ASSERT_EQ(6, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"0123456");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Line, /*count*/ 9, &count));
  ASSERT_EQ(6, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"6");

  ASSERT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(
      TextPatternRangeEndpoint_Start, TextUnit_Line, /*count*/ -7, &count));
  ASSERT_EQ(-6, count);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"0123456");
}

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderCompare) {
  Init(BuildTextDocument({"some text", "some text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* root_node = GetRootNode();

  // Get the textRangeProvider for the document,
  // which contains text "some textsome text".
  ComPtr<ITextRangeProvider> document_text_range_provider;
  GetTextRangeProviderFromTextNode(document_text_range_provider, root_node);

  // Get the textRangeProvider for the first text node.
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   root_node->children()[0]);

  // Get the textRangeProvider for the second text node.
  ComPtr<ITextRangeProvider> more_text_range_provider;
  GetTextRangeProviderFromTextNode(more_text_range_provider,
                                   root_node->children()[1]);

  // Compare text range of the entire document with itself, which should return
  // that they are equal.
  BOOL result;
  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->Compare(
      document_text_range_provider.Get(), &result));
  EXPECT_TRUE(result);

  // Compare the text range of the entire document with one of its child, which
  // should return that they are not equal.
  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->Compare(
      text_range_provider.Get(), &result));
  EXPECT_FALSE(result);

  // Compare the text range of text_node which contains "some text" with
  // text range of more_text_node which also contains "some text". Those two
  // text ranges should not equal, because their endpoints are different, even
  // though their contents are the same.
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->Compare(more_text_range_provider.Get(), &result));
  EXPECT_FALSE(result);
}

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderSelection) {
  Init(BuildTextDocument({"some text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, GetRootNode());

  ASSERT_UIA_INVALIDOPERATION(text_range_provider->AddToSelection());
  ASSERT_UIA_INVALIDOPERATION(text_range_provider->RemoveFromSelection());
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
  root_data.child_ids = {2, 3};

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

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   root_node->children()[0]);

  base::win::ScopedSafearray rectangles;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));

  std::vector<double> expected_values = {100, 150, 200, 200};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  ComPtr<ITextRangeProvider> document_textrange;
  GetTextRangeProviderFromTextNode(document_textrange, root_node);

  base::win::ScopedSafearray body_rectangles;
  EXPECT_HRESULT_SUCCEEDED(
      document_textrange->GetBoundingRectangles(body_rectangles.Receive()));
  expected_values = {100, 150, 200, 200, 200, 250, 100, 100};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(body_rectangles.Get(), expected_values);

  AXTreeManagerMap::GetInstance().RemoveTreeManager(tree_data.tree_id);
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderGetEnclosingElement) {
  Init(BuildTextDocument({"some text", "more text"}));
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* root_node = GetRootNode();

  // Test GetEnclosingElement for each child text node.
  for (int i = 0; i < root_node->child_count(); ++i) {
    ComPtr<IRawElementProviderSimple> text_node_raw =
        QueryInterfaceFromNode<IRawElementProviderSimple>(
            root_node->children()[i]);

    ComPtr<ITextProvider> text_provider;
    EXPECT_HRESULT_SUCCEEDED(
        text_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

    ComPtr<ITextRangeProvider> text_range_provider;
    EXPECT_HRESULT_SUCCEEDED(
        text_provider->get_DocumentRange(&text_range_provider));

    ComPtr<IRawElementProviderSimple> enclosing_element;
    EXPECT_HRESULT_SUCCEEDED(
        text_range_provider->GetEnclosingElement(&enclosing_element));
    EXPECT_EQ(text_node_raw.Get(), enclosing_element.Get());
  }
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderMoveEndpointByRange) {
  Init(BuildTextDocument({"some text", "more text"}));

  AXNode* root_node = GetRootNode();
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* text_node = root_node->children()[0];
  AXNode* more_text_node = root_node->children()[1];

  // Text range for the document, which contains text "some textmore text".
  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);
  ComPtr<ITextProvider> document_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &document_provider));
  ComPtr<ITextRangeProvider> document_text_range_provider;
  ComPtr<AXPlatformNodeTextRangeProviderWin> document_text_range;

  // Text range related to "some text".
  ComPtr<IRawElementProviderSimple> text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text_node);
  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));
  ComPtr<ITextRangeProvider> text_range_provider;
  ComPtr<AXPlatformNodeTextRangeProviderWin> text_range;

  // Text range related to "more text".
  ComPtr<IRawElementProviderSimple> more_text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(more_text_node);
  ComPtr<ITextProvider> more_text_provider;
  EXPECT_HRESULT_SUCCEEDED(more_text_node_raw->GetPatternProvider(
      UIA_TextPatternId, &more_text_provider));
  ComPtr<ITextRangeProvider> more_text_range_provider;
  ComPtr<AXPlatformNodeTextRangeProviderWin> more_text_range;

  // Move the start of document text range "some textmore text" to the end of
  // itself.
  // The start of document text range "some textmore text" is at the end of
  // itself.
  //
  // Before:
  // |s                e|
  // "some textmore text"
  // After:
  //                  |s
  //                   e|
  // "some textmore text"

  // Get the textRangeProvider for the document, which contains text
  // "some textmore text".
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&document_text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->MoveEndpointByRange(
      TextPatternRangeEndpoint_Start, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_End));

  document_text_range_provider->QueryInterface(
      IID_PPV_ARGS(&document_text_range));
  EXPECT_EQ(*GetStart(document_text_range.Get()),
            *GetEnd(document_text_range.Get()));

  // Move the end of document text range "some textmore text" to the start of
  // itself.
  // The end of document text range "some textmore text" is at the start of
  // itself.
  //
  // Before:
  // |s                e|
  // "some textmore text"
  // After:
  // |s
  //  e|
  // "some textmore text"

  // Get the textRangeProvider for the document, which contains text
  // "some textmore text".
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&document_text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->MoveEndpointByRange(
      TextPatternRangeEndpoint_Start, document_text_range_provider.Get(),
      TextPatternRangeEndpoint_End));

  document_text_range_provider->QueryInterface(
      IID_PPV_ARGS(&document_text_range));
  EXPECT_EQ(*GetStart(document_text_range.Get()),
            *GetEnd(document_text_range.Get()));

  // Move the start of document text range "some textmore text" to the start
  // of text range "more text". The start of document text range "some
  // textmore text" is at the start of text range "more text". The end of
  // document range does not change.
  //
  // Before:
  // |s                e|
  // "some textmore text"
  // After:
  //          |s       e|
  // "some textmore text"

  // Get the textRangeProvider for the document, which contains text
  // "some textmore text".
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&document_text_range_provider));
  // Get the textRangeProvider for more_text_node which contains "more text".
  EXPECT_HRESULT_SUCCEEDED(
      more_text_provider->get_DocumentRange(&more_text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->MoveEndpointByRange(
      TextPatternRangeEndpoint_Start, more_text_range_provider.Get(),
      TextPatternRangeEndpoint_Start));

  document_text_range_provider->QueryInterface(
      IID_PPV_ARGS(&document_text_range));
  more_text_range_provider->QueryInterface(IID_PPV_ARGS(&more_text_range));
  EXPECT_EQ(*GetStart(document_text_range.Get()),
            *GetStart(more_text_range.Get()));

  // Move the end of document text range "some textmore text" to the end of
  // text range "some text".
  // The end of document text range "some textmore text" is at the end of text
  // range "some text". The start of document range does not change.
  //
  // Before:
  // |s                e|
  // "some textmore text"
  // After:
  // |s       e|
  // "some textmore text"

  // Get the textRangeProvider for the document, which contains text
  // "some textmore text".
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&document_text_range_provider));
  // Get the textRangeProvider for text_node which contains "some text".
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(document_text_range_provider->MoveEndpointByRange(
      TextPatternRangeEndpoint_End, text_range_provider.Get(),
      TextPatternRangeEndpoint_End));

  document_text_range_provider->QueryInterface(
      IID_PPV_ARGS(&document_text_range));
  text_range_provider->QueryInterface(IID_PPV_ARGS(&text_range));
  EXPECT_EQ(*GetEnd(document_text_range.Get()), *GetEnd(text_range.Get()));

  // Move the end of text range "more text" to the start of
  // text range "some text". Since the order of the endpoints being moved
  // (those of "more text") have to be ensured, both endpoints of "more text"
  // is at the start of "some text".
  //
  // Before:
  //          |s       e|
  // "some textmore text"
  // After:
  //  e|
  // |s
  // "some textmore text"

  // Get the textRangeProvider for text_node which contains "some text".
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));
  // Get the textRangeProvider for more_text_node which contains "more text".
  EXPECT_HRESULT_SUCCEEDED(
      more_text_provider->get_DocumentRange(&more_text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(more_text_range_provider->MoveEndpointByRange(
      TextPatternRangeEndpoint_End, text_range_provider.Get(),
      TextPatternRangeEndpoint_Start));

  text_range_provider->QueryInterface(IID_PPV_ARGS(&text_range));
  more_text_range_provider->QueryInterface(IID_PPV_ARGS(&more_text_range));
  EXPECT_EQ(*GetEnd(more_text_range.Get()), *GetStart(text_range.Get()));
  EXPECT_EQ(*GetStart(more_text_range.Get()), *GetStart(text_range.Get()));

  // Move the start of text range "some text" to the end of text range
  // "more text". Since the order of the endpoints being moved (those
  // of "some text") have to be ensured, both endpoints of "some text" is at
  // the end of "more text".
  //
  // Before:
  // |s       e|
  // "some textmore text"
  // After:
  //                  |s
  //                   e|
  // "some textmore text"

  // Get the textRangeProvider for text_node which contains "some text".
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));
  // Get the textRangeProvider for more_text_node which contains "more text".
  EXPECT_HRESULT_SUCCEEDED(
      more_text_provider->get_DocumentRange(&more_text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByRange(
      TextPatternRangeEndpoint_Start, more_text_range_provider.Get(),
      TextPatternRangeEndpoint_End));

  text_range_provider->QueryInterface(IID_PPV_ARGS(&text_range));
  more_text_range_provider->QueryInterface(IID_PPV_ARGS(&more_text_range));
  EXPECT_EQ(*GetStart(text_range.Get()), *GetEnd(more_text_range.Get()));
  EXPECT_EQ(*GetEnd(text_range.Get()), *GetEnd(more_text_range.Get()));
}

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderGetChildren) {
  // Set up ax tree with the following structure:
  //
  // root______________________
  // |                         |
  // text_node1___             text_node2
  // |            |
  // text_node3   text_node4
  ui::AXNodeData root_data;
  root_data.id = 0;
  root_data.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData text_node1;
  text_node1.id = 1;
  text_node1.role = ax::mojom::Role::kStaticText;
  root_data.child_ids.push_back(1);

  ui::AXNodeData text_node2;
  text_node2.id = 2;
  text_node2.role = ax::mojom::Role::kStaticText;
  root_data.child_ids.push_back(2);

  ui::AXNodeData text_node3;
  text_node3.id = 3;
  text_node3.role = ax::mojom::Role::kStaticText;
  text_node1.child_ids.push_back(3);

  ui::AXNodeData text_node4;
  text_node4.id = 4;
  text_node4.role = ax::mojom::Role::kStaticText;
  text_node1.child_ids.push_back(4);

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_node1);
  update.nodes.push_back(text_node2);
  update.nodes.push_back(text_node3);
  update.nodes.push_back(text_node4);

  Init(update);

  // Set up variables from the tree for testing.
  AXNode* rootnode = GetRootNode();
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* node1 = rootnode->children()[0];
  AXNode* node2 = rootnode->children()[1];
  AXNode* node3 = node1->children()[0];
  AXNode* node4 = node1->children()[1];

  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(rootnode);
  ComPtr<IRawElementProviderSimple> text_node_raw1 =
      QueryInterfaceFromNode<IRawElementProviderSimple>(node1);
  ComPtr<IRawElementProviderSimple> text_node_raw2 =
      QueryInterfaceFromNode<IRawElementProviderSimple>(node2);
  ComPtr<IRawElementProviderSimple> text_node_raw3 =
      QueryInterfaceFromNode<IRawElementProviderSimple>(node3);
  ComPtr<IRawElementProviderSimple> text_node_raw4 =
      QueryInterfaceFromNode<IRawElementProviderSimple>(node4);

  // Test text_node3 - leaf nodes should have no children.
  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw3->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  base::win::ScopedSafearray children;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetChildren(children.Receive()));

  std::vector<ComPtr<IRawElementProviderSimple>> expected_values = {};

  EXPECT_UIA_VT_UNKNOWN_SAFEARRAY_EQ(children.Get(), expected_values);

  // Test text_node2 - leaf nodes should have no children.
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw2->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetChildren(children.Receive()));

  EXPECT_UIA_VT_UNKNOWN_SAFEARRAY_EQ(children.Get(), expected_values);

  // Test text_node1 - children should include text_node3 and text_node4.
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw1->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetChildren(children.Receive()));

  expected_values = {text_node_raw3, text_node_raw4};

  EXPECT_UIA_VT_UNKNOWN_SAFEARRAY_EQ(children.Get(), expected_values);

  // Test root_node - children should include the entire left subtree and
  // the entire right subtree.
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetChildren(children.Receive()));

  expected_values = {text_node_raw1, text_node_raw3, text_node_raw4,
                     text_node_raw2};

  EXPECT_UIA_VT_UNKNOWN_SAFEARRAY_EQ(children.Get(), expected_values);
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderGetAttributeValue) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily, "sans");
  text_data.AddFloatAttribute(ax::mojom::FloatAttribute::kFontWeight, 300);
  text_data.AddIntAttribute(ax::mojom::IntAttribute::kTextOverlineStyle, 1);
  text_data.AddIntAttribute(ax::mojom::IntAttribute::kTextStrikethroughStyle,
                            2);
  text_data.AddIntAttribute(ax::mojom::IntAttribute::kTextUnderlineStyle, 3);
  text_data.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                            0xDEADBEEFU);
  text_data.AddIntAttribute(ax::mojom::IntAttribute::kColor, 0xDEADC0DEU);
  text_data.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr-CA");
  text_data.SetTextDirection(ax::mojom::TextDirection::kRtl);
  text_data.SetName("some text");

  ui::AXNodeData heading_data;
  heading_data.id = 3;
  heading_data.role = ax::mojom::Role::kHeading;
  heading_data.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 6);
  heading_data.child_ids = {4};

  ui::AXNodeData heading_text_data;
  heading_text_data.id = 4;
  heading_text_data.role = ax::mojom::Role::kStaticText;
  heading_text_data.AddState(ax::mojom::State::kInvisible);
  heading_text_data.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                    0xDEADBEEFU);
  heading_text_data.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                    0xDEADC0DEU);
  heading_text_data.SetTextDirection(ax::mojom::TextDirection::kRtl);
  heading_text_data.SetName("more text");

  ui::AXNodeData mark_data;
  mark_data.id = 5;
  mark_data.role = ax::mojom::Role::kMark;
  mark_data.child_ids = {6};

  ui::AXNodeData mark_text_data;
  mark_text_data.id = 6;
  mark_text_data.role = ax::mojom::Role::kStaticText;
  mark_text_data.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                 0xDEADBEEFU);
  mark_text_data.AddIntAttribute(ax::mojom::IntAttribute::kColor, 0xDEADC0DEU);
  mark_text_data.SetTextDirection(ax::mojom::TextDirection::kRtl);
  mark_text_data.SetName("marked text");

  ui::AXNodeData list_data;
  list_data.id = 7;
  list_data.role = ax::mojom::Role::kList;
  list_data.child_ids = {8, 10};

  ui::AXNodeData list_item_data;
  list_item_data.id = 8;
  list_item_data.role = ax::mojom::Role::kListItem;
  list_item_data.child_ids = {9};
  list_item_data.AddIntAttribute(
      ax::mojom::IntAttribute::kListStyle,
      static_cast<int>(ax::mojom::ListStyle::kOther));

  ui::AXNodeData list_item_text_data;
  list_item_text_data.id = 9;
  list_item_text_data.role = ax::mojom::Role::kStaticText;
  list_item_text_data.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                      0xDEADBEEFU);
  list_item_text_data.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                      0xDEADC0DEU);

  ui::AXNodeData list_item2_data;
  list_item2_data.id = 10;
  list_item2_data.role = ax::mojom::Role::kListItem;
  list_item2_data.child_ids = {11};
  list_item2_data.AddIntAttribute(
      ax::mojom::IntAttribute::kListStyle,
      static_cast<int>(ax::mojom::ListStyle::kDisc));

  ui::AXNodeData list_item2_text_data;
  list_item2_text_data.id = 11;
  list_item2_text_data.role = ax::mojom::Role::kStaticText;
  list_item2_text_data.AddIntAttribute(
      ax::mojom::IntAttribute::kBackgroundColor, 0xDEADBEEFU);
  list_item2_text_data.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                       0xDEADC0DEU);

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids = {2, 3, 5, 7};

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data);
  update.nodes.push_back(heading_data);
  update.nodes.push_back(heading_text_data);
  update.nodes.push_back(mark_data);
  update.nodes.push_back(mark_text_data);
  update.nodes.push_back(list_data);
  update.nodes.push_back(list_item_data);
  update.nodes.push_back(list_item_text_data);
  update.nodes.push_back(list_item2_data);
  update.nodes.push_back(list_item2_text_data);

  Init(update);
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXTreeManagerMap::GetInstance().AddTreeManager(tree_data.tree_id, this);

  AXNode* root_node = GetRootNode();
  AXNode* text_node = root_node->children()[0];
  AXNode* heading_node = root_node->children()[1];
  AXNode* heading_text_node = heading_node->children()[0];
  AXNode* mark_node = root_node->children()[2];
  AXNode* mark_text_node = mark_node->children()[0];
  AXNode* list_node = root_node->children()[3];
  AXNode* list_item_node = list_node->children()[0];
  AXNode* list_item_text_node = list_item_node->children()[0];
  AXNode* list_item2_node = list_node->children()[1];
  AXNode* list_item2_text_node = list_item2_node->children()[0];

  ComPtr<ITextRangeProvider> document_range_provider;
  GetTextRangeProviderFromTextNode(document_range_provider, root_node);
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, text_node);
  ComPtr<ITextRangeProvider> heading_text_range_provider;
  GetTextRangeProviderFromTextNode(heading_text_range_provider,
                                   heading_text_node);
  ComPtr<ITextRangeProvider> mark_text_range_provider;
  GetTextRangeProviderFromTextNode(mark_text_range_provider, mark_text_node);
  ComPtr<ITextRangeProvider> list_item_text_range_provider;
  GetTextRangeProviderFromTextNode(list_item_text_range_provider,
                                   list_item_text_node);
  ComPtr<ITextRangeProvider> list_item2_text_range_provider;
  GetTextRangeProviderFromTextNode(list_item2_text_range_provider,
                                   list_item2_text_node);

  base::win::ScopedVariant expected_mixed_variant;
  {
    VARIANT var;
    V_VT(&var) = VT_UNKNOWN;
    EXPECT_HRESULT_SUCCEEDED(
        ::UiaGetReservedMixedAttributeValue(&V_UNKNOWN(&var)));
    expected_mixed_variant.Reset(var);
  }

  base::win::ScopedVariant expected_variant;

  // SkColor is ARGB, COLORREF is 0BGR
  expected_variant.Set(static_cast<int32_t>(0x00EFBEADU));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider,
                              UIA_BackgroundColorAttributeId, expected_variant);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(document_range_provider,
                              UIA_BackgroundColorAttributeId, expected_variant);
  expected_variant.Reset();

  expected_variant.Set(static_cast<int32_t>(BulletStyle::BulletStyle_None));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(list_item_text_range_provider,
                              UIA_BulletStyleAttributeId, expected_variant);
  expected_variant.Reset();

  expected_variant.Set(
      static_cast<int32_t>(BulletStyle::BulletStyle_FilledRoundBullet));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(list_item2_text_range_provider,
                              UIA_BulletStyleAttributeId, expected_variant);
  expected_variant.Reset();

  {
    base::win::ScopedVariant lang_variant;
    EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
        UIA_CultureAttributeId, lang_variant.Receive()));

    EXPECT_EQ(lang_variant.type(), VT_I4);
    const LCID lcid = V_I4(lang_variant.ptr());
    EXPECT_EQ(LANG_FRENCH, PRIMARYLANGID(lcid));
    EXPECT_EQ(SUBLANG_FRENCH_CANADIAN, SUBLANGID(lcid));
    EXPECT_EQ(SORT_DEFAULT, SORTIDFROMLCID(lcid));
  }

  base::string16 font_name = base::UTF8ToUTF16("sans");
  expected_variant.Set(SysAllocString(font_name.c_str()));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider, UIA_FontNameAttributeId,
                              expected_variant);
  expected_variant.Reset();

  expected_variant.Set(300);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider, UIA_FontWeightAttributeId,
                              expected_variant);
  expected_variant.Reset();

  // SkColor is ARGB, COLORREF is 0BGR
  expected_variant.Set(static_cast<int32_t>(0x00DEC0ADU));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider,
                              UIA_ForegroundColorAttributeId, expected_variant);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(document_range_provider,
                              UIA_ForegroundColorAttributeId, expected_variant);
  expected_variant.Reset();

  expected_variant.Set(false);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider, UIA_IsHiddenAttributeId,
                              expected_variant);
  expected_variant.Reset();

  EXPECT_UIA_TEXTATTRIBUTE_EQ(document_range_provider, UIA_IsHiddenAttributeId,
                              expected_mixed_variant);

  expected_variant.Set(TextDecorationLineStyle::TextDecorationLineStyle_Dot);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider, UIA_OverlineStyleAttributeId,
                              expected_variant);
  expected_variant.Reset();

  expected_variant.Set(TextDecorationLineStyle::TextDecorationLineStyle_Dash);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(
      text_range_provider, UIA_StrikethroughStyleAttributeId, expected_variant);
  expected_variant.Reset();

  expected_variant.Set(TextDecorationLineStyle::TextDecorationLineStyle_Single);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider,
                              UIA_UnderlineStyleAttributeId, expected_variant);
  expected_variant.Reset();

  base::string16 style_name = base::UTF8ToUTF16("");
  expected_variant.Set(SysAllocString(style_name.c_str()));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(text_range_provider, UIA_StyleNameAttributeId,
                              expected_variant);
  expected_variant.Reset();

  expected_variant.Set(static_cast<int32_t>(StyleId_Heading6));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(heading_text_range_provider,
                              UIA_StyleIdAttributeId, expected_variant);
  expected_variant.Reset();

  style_name = base::UTF8ToUTF16("mark");
  expected_variant.Set(SysAllocString(style_name.c_str()));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(mark_text_range_provider,
                              UIA_StyleNameAttributeId, expected_variant);
  expected_variant.Reset();

  expected_variant.Set(static_cast<int32_t>(StyleId_NumberedList));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(list_item_text_range_provider,
                              UIA_StyleIdAttributeId, expected_variant);
  expected_variant.Reset();

  expected_variant.Set(static_cast<int32_t>(StyleId_BulletedList));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(list_item2_text_range_provider,
                              UIA_StyleIdAttributeId, expected_variant);
  expected_variant.Reset();

  expected_variant.Set(
      static_cast<int32_t>(FlowDirections::FlowDirections_RightToLeft));
  EXPECT_UIA_TEXTATTRIBUTE_EQ(
      text_range_provider, UIA_TextFlowDirectionsAttributeId, expected_variant);
  EXPECT_UIA_TEXTATTRIBUTE_EQ(document_range_provider,
                              UIA_TextFlowDirectionsAttributeId,
                              expected_variant);
  expected_variant.Reset();
}

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderSelect) {
  Init(BuildTextDocument({"some text", "more text2"}));
  AXNodePosition::SetTreeForTesting(tree_.get());
  AXNode* root_node = GetRootNode();

  // Text range for the document, which contains text "some textmore text2".
  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);
  ComPtr<ITextProvider> document_provider;
  ComPtr<ITextRangeProvider> document_text_range_provider;
  ComPtr<AXPlatformNodeTextRangeProviderWin> document_text_range;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &document_provider));
  EXPECT_HRESULT_SUCCEEDED(
      document_provider->get_DocumentRange(&document_text_range_provider));
  document_text_range_provider->QueryInterface(
      IID_PPV_ARGS(&document_text_range));

  // Text range related to "some text".
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider,
                                   root_node->children()[0]);
  ComPtr<AXPlatformNodeTextRangeProviderWin> text_range;
  text_range_provider->QueryInterface(IID_PPV_ARGS(&text_range));

  // Text range related to "more text2".
  ComPtr<ITextRangeProvider> more_text_range_provider;
  GetTextRangeProviderFromTextNode(more_text_range_provider,
                                   root_node->children()[1]);
  ComPtr<AXPlatformNodeTextRangeProviderWin> more_text_range;
  more_text_range_provider->QueryInterface(IID_PPV_ARGS(&more_text_range));

  AXPlatformNodeDelegate* delegate =
      GetOwner(document_text_range.Get())->GetDelegate();

  CComPtr<ITextRangeProvider> selected_text_range_provider;
  base::win::ScopedSafearray selection;
  LONG index = 0;
  LONG ubound;
  LONG lbound;

  // Text range "some text" performs select.
  {
    text_range_provider->Select();

    // Verify selection.
    EXPECT_EQ(2, delegate->GetTreeData().sel_anchor_object_id);
    EXPECT_EQ(2, delegate->GetTreeData().sel_focus_object_id);
    EXPECT_EQ(0, delegate->GetTreeData().sel_anchor_offset);
    EXPECT_EQ(9, delegate->GetTreeData().sel_focus_offset);

    // Verify the content of the selection.
    document_provider->GetSelection(selection.Receive());
    ASSERT_NE(nullptr, selection.Get());

    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selection.Get(), 1, &ubound));
    EXPECT_EQ(0, ubound);
    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selection.Get(), 1, &lbound));
    EXPECT_EQ(0, lbound);
    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
        selection.Get(), &index, &selected_text_range_provider));
    EXPECT_UIA_TEXTRANGE_EQ(selected_text_range_provider, L"some text");

    selected_text_range_provider.Release();
    selection.Reset();
  }

  // Text range "more text2" performs select.
  {
    more_text_range_provider->Select();

    // Verify selection
    EXPECT_EQ(3, delegate->GetTreeData().sel_anchor_object_id);
    EXPECT_EQ(3, delegate->GetTreeData().sel_focus_object_id);
    EXPECT_EQ(0, delegate->GetTreeData().sel_anchor_offset);
    EXPECT_EQ(10, delegate->GetTreeData().sel_focus_offset);

    // Verify the content of the selection.
    document_provider->GetSelection(selection.Receive());
    ASSERT_NE(nullptr, selection.Get());

    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selection.Get(), 1, &ubound));
    EXPECT_EQ(0, ubound);
    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selection.Get(), 1, &lbound));
    EXPECT_EQ(0, lbound);
    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
        selection.Get(), &index, &selected_text_range_provider));
    EXPECT_UIA_TEXTRANGE_EQ(selected_text_range_provider, L"more text2");

    selected_text_range_provider.Release();
    selection.Reset();
  }

  // Document text range "some textmore text2" performs select.
  {
    document_text_range_provider->Select();

    // Verify selection.
    EXPECT_EQ(2, delegate->GetTreeData().sel_anchor_object_id);
    EXPECT_EQ(3, delegate->GetTreeData().sel_focus_object_id);
    EXPECT_EQ(0, delegate->GetTreeData().sel_anchor_offset);
    EXPECT_EQ(10, delegate->GetTreeData().sel_focus_offset);

    // When selection spans multiple nodes ITextProvider::GetSelection is not
    // supported. But the text range is still selected.
    document_provider->GetSelection(selection.Receive());
    ASSERT_EQ(nullptr, selection.Get());
  }

  // A degenerate text range performs select.
  {
    // Move the endpoint of text range so it becomes degenerate, then select.
    text_range_provider->MoveEndpointByRange(TextPatternRangeEndpoint_Start,
                                             text_range_provider.Get(),
                                             TextPatternRangeEndpoint_End);
    text_range_provider->Select();

    // Verify selection.
    EXPECT_EQ(2, delegate->GetTreeData().sel_anchor_object_id);
    EXPECT_EQ(2, delegate->GetTreeData().sel_focus_object_id);
    EXPECT_EQ(9, delegate->GetTreeData().sel_anchor_offset);
    EXPECT_EQ(9, delegate->GetTreeData().sel_focus_offset);

    // Verify the content of the selection.
    document_provider->GetSelection(selection.Receive());
    ASSERT_NE(nullptr, selection.Get());

    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selection.Get(), 1, &ubound));
    EXPECT_EQ(0, ubound);
    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selection.Get(), 1, &lbound));
    EXPECT_EQ(0, lbound);
    EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
        selection.Get(), &index, &selected_text_range_provider));
    EXPECT_UIA_TEXTRANGE_EQ(selected_text_range_provider, L"");

    selected_text_range_provider.Release();
    selection.Reset();
  }
}

TEST_F(AXPlatformNodeTextRangeProviderTest, TestITextRangeProviderFindText) {
  ui::AXNodeData some_text_data;
  some_text_data.id = 2;
  some_text_data.role = ax::mojom::Role::kStaticText;
  some_text_data.SetName("some text");

  ui::AXNodeData more_text_data;
  more_text_data.id = 3;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.SetName("more text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.SetName("root");
  root_data.child_ids = {2, 3};

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(some_text_data);
  update.nodes.push_back(more_text_data);

  Init(update);
  AXNodePosition::SetTreeForTesting(tree_.get());

  AXNode* root_node = GetRootNode();
  AXNode* some_text_node = root_node->children()[0];
  AXNode* more_text_node = root_node->children()[1];

  ComPtr<ITextRangeProvider> range;

  // Test Leaf kStaticText search
  GetTextRangeProviderFromTextNode(range, some_text_node);
  EXPECT_UIA_FIND_TEXT(range, L"some text", false);
  EXPECT_UIA_FIND_TEXT(range, L"SoMe TeXt", true);
  GetTextRangeProviderFromTextNode(range, more_text_node);
  EXPECT_UIA_FIND_TEXT(range, L"more", false);
  EXPECT_UIA_FIND_TEXT(range, L"MoRe", true);

  // Test searching for leaf content from ancestor
  GetTextRangeProviderFromTextNode(range, root_node);
  EXPECT_UIA_FIND_TEXT(range, L"some text", false);
  EXPECT_UIA_FIND_TEXT(range, L"SoMe TeXt", true);
  EXPECT_UIA_FIND_TEXT(range, L"more text", false);
  EXPECT_UIA_FIND_TEXT(range, L"MoRe TeXt", true);
  EXPECT_UIA_FIND_TEXT(range, L"more", false);
  // Test finding text that crosses a node boundary
  EXPECT_UIA_FIND_TEXT(range, L"textmore", false);
  // Test no match
  EXPECT_UIA_FIND_TEXT_NO_MATCH(range, L"no match", false);
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderFindTextBackwards) {
  ui::AXNodeData text_data1;
  text_data1.id = 2;
  text_data1.role = ax::mojom::Role::kStaticText;
  text_data1.SetName("text");

  ui::AXNodeData text_data2;
  text_data2.id = 3;
  text_data2.role = ax::mojom::Role::kStaticText;
  text_data2.SetName("some");

  ui::AXNodeData text_data3;
  text_data3.id = 4;
  text_data3.role = ax::mojom::Role::kStaticText;
  text_data3.SetName("text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.SetName("root");
  root_data.child_ids = {2, 3, 4};

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data1);
  update.nodes.push_back(text_data2);
  update.nodes.push_back(text_data3);

  Init(update);
  AXNodePosition::SetTreeForTesting(tree_.get());

  AXNode* root_node = GetRootNode();
  AXNode* text_node1 = root_node->children()[0];
  AXNode* text_node3 = root_node->children()[2];

  ComPtr<ITextRangeProvider> root_range_provider;
  GetTextRangeProviderFromTextNode(root_range_provider, root_node);
  ComPtr<ITextRangeProvider> text_node1_range;
  GetTextRangeProviderFromTextNode(text_node1_range, text_node1);
  ComPtr<ITextRangeProvider> text_node3_range;
  GetTextRangeProviderFromTextNode(text_node3_range, text_node3);

  CComPtr<ITextRangeProvider> text_range_provider_found;
  base::win::ScopedBstr find_string(L"text");
  base::win::ScopedBstr found_content;
  BOOL range_equal;

  // Forward search finds the text_node1
  EXPECT_HRESULT_SUCCEEDED(root_range_provider->FindText(
      find_string, false, false, &text_range_provider_found));
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider_found->GetText(-1, found_content.Receive()));
  EXPECT_STREQ(found_content, find_string);
  found_content.Reset();

  range_equal = false;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider_found->Compare(text_node1_range.Get(), &range_equal));
  EXPECT_TRUE(range_equal);

  // Backwards search finds the text_node3
  EXPECT_HRESULT_SUCCEEDED(root_range_provider->FindText(
      find_string, true, false, &text_range_provider_found));
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider_found->GetText(-1, found_content.Receive()));
  EXPECT_STREQ(found_content, find_string);
  found_content.Reset();

  range_equal = false;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider_found->Compare(text_node3_range.Get(), &range_equal));
  EXPECT_TRUE(range_equal);
}

TEST_F(AXPlatformNodeTextRangeProviderTest,
       TestITextRangeProviderFindAttribute) {
  // document - visible
  //  [empty]
  //
  // Search forward, look for IsHidden=true.
  // Expected: nullptr
  // Search forward, look for IsHidden=false.
  // Expected: ""
  // Note: returns "" rather than nullptr here because document root web area by
  //       default set to visible. So the text range represents document matches
  //       our searching criteria. And we return a degenerate range.
  //
  // Search backward, look for IsHidden=true.
  // Expected: nullptr
  // Search backward, look for IsHidden=false.
  // Expected: ""
  // Note: returns "" rather than nullptr here because document root web area by
  //       default set to visible. So the text range represents document matches
  //       our searching criteria. And we return a degenerate range.
  {
    ui::AXNodeData root_data;
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kRootWebArea;

    ui::AXTreeUpdate update;
    ui::AXTreeData tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root_data.id;
    update.nodes = {root_data};

    Init(update);

    AXNodePosition::SetTreeForTesting(tree_.get());

    ComPtr<ITextProvider> document_provider;
    ComPtr<ITextRangeProvider> document_range_provider;
    ComPtr<ITextRangeProvider> matched_range_provider;
    base::win::ScopedBstr matched_text_content;
    VARIANT is_hidden_attr_val;
    V_VT(&is_hidden_attr_val) = VT_BOOL;
    bool is_search_backward;

    EXPECT_HRESULT_SUCCEEDED(
        GetRootIRawElementProviderSimple()->GetPatternProvider(
            UIA_TextPatternId, &document_provider));
    EXPECT_HRESULT_SUCCEEDED(
        document_provider->get_DocumentRange(&document_range_provider));

    // Search forward, look for IsHidden=true.
    // Expected: nullptr
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_EQ(nullptr, matched_range_provider.Get());

    // Search forward, look for IsHidden=false.
    // Expected: ""
    // Note: returns "" rather than nullptr here because document root web area
    //       by default set to visible. So the text range represents document
    //       matches our searching criteria. And we return a degenerate range.
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search backward, look for IsHidden=true.
    // Expected: nullptr
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_EQ(nullptr, matched_range_provider.Get());

    // Search backward, look for IsHidden=false.
    // Expected: ""
    // Note: returns "" rather than nullptr here because document root web area
    //       by default set to visible. So the text range represents document
    //       matches our searching criteria. And we return a degenerate range.
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"");

    AXNodePosition::SetTreeForTesting(nullptr);
  }

  // document - visible
  //  text1 - invisible
  //
  // Search forward, look for IsHidden=true.
  // Expected: "text1"
  // Search forward, look for IsHidden=false.
  // Expected: nullptr
  // Search backward, look for IsHidden=true.
  // Expected: "text1"
  // Search backward, look for IsHidden=false.
  // Expected: nullptr
  {
    ui::AXNodeData text_data1;
    text_data1.id = 2;
    text_data1.role = ax::mojom::Role::kStaticText;
    text_data1.AddState(ax::mojom::State::kInvisible);
    text_data1.SetName("text1");

    ui::AXNodeData root_data;
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kRootWebArea;
    root_data.child_ids = {2};

    ui::AXTreeUpdate update;
    ui::AXTreeData tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root_data.id;
    update.nodes = {root_data, text_data1};

    Init(update);

    AXNodePosition::SetTreeForTesting(tree_.get());

    ComPtr<ITextProvider> document_provider;
    ComPtr<ITextRangeProvider> document_range_provider;
    ComPtr<ITextRangeProvider> matched_range_provider;
    base::win::ScopedBstr matched_text_content;
    VARIANT is_hidden_attr_val;
    V_VT(&is_hidden_attr_val) = VT_BOOL;
    bool is_search_backward;

    EXPECT_HRESULT_SUCCEEDED(
        GetRootIRawElementProviderSimple()->GetPatternProvider(
            UIA_TextPatternId, &document_provider));
    EXPECT_HRESULT_SUCCEEDED(
        document_provider->get_DocumentRange(&document_range_provider));

    // Search forward, look for IsHidden=true.
    // Expected: "text1"
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text1");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search forward, look for IsHidden=false.
    // Expected: nullptr
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_EQ(nullptr, matched_range_provider.Get());

    // Search backward, look for IsHidden=true.
    // Expected: "text1"
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text1");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search backward, look for IsHidden=false.
    // Expected: nullptr
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_EQ(nullptr, matched_range_provider.Get());

    AXNodePosition::SetTreeForTesting(nullptr);
  }

  // document - visible
  //  text1 - visible
  //  text2 - visible
  //
  // Search forward, look for IsHidden=true.
  // Expected: nullptr
  // Search forward, look for IsHidden=false.
  // Expected: "text1text2"
  // Search backward, look for IsHidden=true.
  // Expected: nullptr
  // Search backward, look for IsHidden=false.
  // Expected: "text1text2"
  {
    ui::AXNodeData text_data1;
    text_data1.id = 2;
    text_data1.role = ax::mojom::Role::kStaticText;
    text_data1.SetName("text1");

    ui::AXNodeData text_data2;
    text_data2.id = 3;
    text_data2.role = ax::mojom::Role::kStaticText;
    text_data2.SetName("text2");

    ui::AXNodeData root_data;
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kRootWebArea;
    root_data.child_ids = {2, 3};

    ui::AXTreeUpdate update;
    ui::AXTreeData tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root_data.id;
    update.nodes = {root_data, text_data1, text_data2};

    Init(update);

    AXNodePosition::SetTreeForTesting(tree_.get());

    ComPtr<ITextProvider> document_provider;
    ComPtr<ITextRangeProvider> document_range_provider;
    ComPtr<ITextRangeProvider> matched_range_provider;
    base::win::ScopedBstr matched_text_content;
    VARIANT is_hidden_attr_val;
    V_VT(&is_hidden_attr_val) = VT_BOOL;
    bool is_search_backward;

    EXPECT_HRESULT_SUCCEEDED(
        GetRootIRawElementProviderSimple()->GetPatternProvider(
            UIA_TextPatternId, &document_provider));
    EXPECT_HRESULT_SUCCEEDED(
        document_provider->get_DocumentRange(&document_range_provider));

    // Search forward, look for IsHidden=true.
    // Expected: nullptr
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_EQ(nullptr, matched_range_provider.Get());

    // Search forward, look for IsHidden=false.
    // Expected: "text1text2"
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text1text2");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search backward, look for IsHidden=true.
    // Expected: nullptr
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_EQ(nullptr, matched_range_provider.Get());

    // Search backward, look for IsHidden=false.
    // Expected: "text1text2"
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text1text2");

    AXNodePosition::SetTreeForTesting(nullptr);
  }

  // document - visible
  //  text1 - visible
  //  text2 - invisible
  //  text3 - invisible
  //  text4 - visible
  //  text5 - invisible
  //
  // Search forward, look for IsHidden=true.
  // Expected: "text2text3"
  // Search forward, look for IsHidden=false.
  // Expected: "text1"
  // Search backward, look for IsHidden=true.
  // Expected: "text5"
  // Search backward, look for IsHidden=false.
  // Expected: "text4"
  {
    ui::AXNodeData text_data1;
    text_data1.id = 2;
    text_data1.role = ax::mojom::Role::kStaticText;
    text_data1.SetName("text1");

    ui::AXNodeData text_data2;
    text_data2.id = 3;
    text_data2.role = ax::mojom::Role::kStaticText;
    text_data2.AddState(ax::mojom::State::kInvisible);
    text_data2.SetName("text2");

    ui::AXNodeData text_data3;
    text_data3.id = 4;
    text_data3.role = ax::mojom::Role::kStaticText;
    text_data3.AddState(ax::mojom::State::kInvisible);
    text_data3.SetName("text3");

    ui::AXNodeData text_data4;
    text_data4.id = 5;
    text_data4.role = ax::mojom::Role::kStaticText;
    text_data4.SetName("text4");

    ui::AXNodeData text_data5;
    text_data5.id = 6;
    text_data5.role = ax::mojom::Role::kStaticText;
    text_data5.AddState(ax::mojom::State::kInvisible);
    text_data5.SetName("text5");

    ui::AXNodeData root_data;
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kRootWebArea;
    root_data.child_ids = {2, 3, 4, 5, 6};

    ui::AXTreeUpdate update;
    ui::AXTreeData tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root_data.id;
    update.nodes = {root_data,  text_data1, text_data2,
                    text_data3, text_data4, text_data5};

    Init(update);

    AXNodePosition::SetTreeForTesting(tree_.get());

    ComPtr<ITextProvider> document_provider;
    ComPtr<ITextRangeProvider> document_range_provider;
    ComPtr<ITextRangeProvider> matched_range_provider;
    base::win::ScopedBstr matched_text_content;
    VARIANT is_hidden_attr_val;
    V_VT(&is_hidden_attr_val) = VT_BOOL;
    bool is_search_backward;

    EXPECT_HRESULT_SUCCEEDED(
        GetRootIRawElementProviderSimple()->GetPatternProvider(
            UIA_TextPatternId, &document_provider));
    EXPECT_HRESULT_SUCCEEDED(
        document_provider->get_DocumentRange(&document_range_provider));

    // Search forward, look for IsHidden=true.
    // Expected: "text2text3"
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text2text3");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search forward, look for IsHidden=false.
    // Expected: "text1"
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text1");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search backward, look for IsHidden=true.
    // Expected: "text5"
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text5");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search backward, look for IsHidden=false.
    // Expected: "text4"
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text4");

    AXNodePosition::SetTreeForTesting(nullptr);
  }

  // document - visible
  //  text1 - visible
  //  text2 - invisible
  //  text3 - invisible
  //  text4 - invisible
  //  text5 - visible
  //
  // Search forward, look for IsHidden=true.
  // Expected: "text2text3text4"
  // Search forward, look for IsHidden=false.
  // Expected: "text1"
  // Search backward, look for IsHidden=true.
  // Expected: "text2text3text4"
  // Search backward, look for IsHidden=false.
  // Expected: "text5"
  {
    ui::AXNodeData text_data1;
    text_data1.id = 2;
    text_data1.role = ax::mojom::Role::kStaticText;
    text_data1.SetName("text1");

    ui::AXNodeData text_data2;
    text_data2.id = 3;
    text_data2.role = ax::mojom::Role::kStaticText;
    text_data2.AddState(ax::mojom::State::kInvisible);
    text_data2.SetName("text2");

    ui::AXNodeData text_data3;
    text_data3.id = 4;
    text_data3.role = ax::mojom::Role::kStaticText;
    text_data3.AddState(ax::mojom::State::kInvisible);
    text_data3.SetName("text3");

    ui::AXNodeData text_data4;
    text_data4.id = 5;
    text_data4.role = ax::mojom::Role::kStaticText;
    text_data4.AddState(ax::mojom::State::kInvisible);
    text_data4.SetName("text4");

    ui::AXNodeData text_data5;
    text_data5.id = 6;
    text_data5.role = ax::mojom::Role::kStaticText;
    text_data5.SetName("text5");

    ui::AXNodeData root_data;
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kRootWebArea;
    root_data.child_ids = {2, 3, 4, 5, 6};

    ui::AXTreeUpdate update;
    ui::AXTreeData tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root_data.id;
    update.nodes = {root_data,  text_data1, text_data2,
                    text_data3, text_data4, text_data5};

    Init(update);

    AXNodePosition::SetTreeForTesting(tree_.get());

    ComPtr<ITextProvider> document_provider;
    ComPtr<ITextRangeProvider> document_range_provider;
    ComPtr<ITextRangeProvider> matched_range_provider;
    base::win::ScopedBstr matched_text_content;
    VARIANT is_hidden_attr_val;
    V_VT(&is_hidden_attr_val) = VT_BOOL;
    bool is_search_backward;

    EXPECT_HRESULT_SUCCEEDED(
        GetRootIRawElementProviderSimple()->GetPatternProvider(
            UIA_TextPatternId, &document_provider));
    EXPECT_HRESULT_SUCCEEDED(
        document_provider->get_DocumentRange(&document_range_provider));

    // Search forward, look for IsHidden=true.
    // Expected: "text2text3text4"
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text2text3text4");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search forward, look for IsHidden=false.
    // Expected: "text1"
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = false;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text1");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search backward, look for IsHidden=true.
    // Expected: "text2text3text4"
    V_BOOL(&is_hidden_attr_val) = VARIANT_TRUE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text2text3text4");
    matched_range_provider.Reset();
    matched_text_content.Reset();

    // Search backward, look for IsHidden=false.
    // Expected: "text5"
    V_BOOL(&is_hidden_attr_val) = VARIANT_FALSE;
    is_search_backward = true;
    document_range_provider->FindAttribute(
        UIA_IsHiddenAttributeId, is_hidden_attr_val, is_search_backward,
        &matched_range_provider);
    ASSERT_NE(nullptr, matched_range_provider.Get());
    EXPECT_HRESULT_SUCCEEDED(
        matched_range_provider->GetText(-1, matched_text_content.Receive()));
    EXPECT_STREQ(matched_text_content, L"text5");

    AXNodePosition::SetTreeForTesting(nullptr);
  }
}

}  // namespace ui
