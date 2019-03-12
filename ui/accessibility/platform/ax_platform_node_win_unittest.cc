// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <oleacc.h>
#include <wrl/client.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/win/atl.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/base/win/atl_module.h"

using Microsoft::WRL::ComPtr;
using base::win::ScopedBstr;
using base::win::ScopedVariant;

namespace ui {

namespace {

// Most IAccessible functions require a VARIANT set to CHILDID_SELF as
// the first argument.
ScopedVariant SELF(CHILDID_SELF);

}  // namespace

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

// Helper macros for testing UIAutomation property values and maintain
// correct stack tracing and failure causality.
//
// WARNING: These aren't intended to be generic EXPECT_BSTR_EQ macros
// as the logic is specific to extracting and comparing UIA property
// values.
#define EXPECT_UIA_VALUE_EQ(node, property_id, expectedVariant) \
  {                                                             \
    ScopedVariant actual;                                       \
    ASSERT_HRESULT_SUCCEEDED(                                   \
        node->GetPropertyValue(property_id, actual.Receive())); \
    EXPECT_EQ(0, actual.Compare(expectedVariant));              \
  }

#define EXPECT_UIA_BSTR_EQ(node, property_id, expected)                  \
  {                                                                      \
    ScopedVariant expectedVariant(expected);                             \
    ASSERT_EQ(VT_BSTR, expectedVariant.type());                          \
    ASSERT_NE(nullptr, expectedVariant.ptr()->bstrVal);                  \
    ScopedVariant actual;                                                \
    ASSERT_HRESULT_SUCCEEDED(                                            \
        node->GetPropertyValue(property_id, actual.Receive()));          \
    ASSERT_EQ(VT_BSTR, actual.type());                                   \
    ASSERT_NE(nullptr, actual.ptr()->bstrVal);                           \
    EXPECT_STREQ(expectedVariant.ptr()->bstrVal, actual.ptr()->bstrVal); \
  }

#define EXPECT_UIA_BOOL_EQ(node, property_id, expected)               \
  {                                                                   \
    ScopedVariant expectedVariant(expected, VT_BOOL);                 \
    ASSERT_EQ(VT_BOOL, expectedVariant.type());                       \
    ScopedVariant actual;                                             \
    ASSERT_HRESULT_SUCCEEDED(                                         \
        node->GetPropertyValue(property_id, actual.Receive()));       \
    EXPECT_EQ(expectedVariant.ptr()->boolVal, actual.ptr()->boolVal); \
  }

#define EXPECT_UIA_DOUBLE_ARRAY_EQ(node, array_property_id,                 \
                                   expected_property_values)                \
  {                                                                         \
    ScopedVariant array;                                                    \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        node->GetPropertyValue(array_property_id, array.Receive()));        \
    ASSERT_EQ(VT_ARRAY | VT_R8, array.type());                              \
    ASSERT_EQ(1u, SafeArrayGetDim(array.ptr()->parray));                    \
    LONG array_lower_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetLBound(array.ptr()->parray, 1, &array_lower_bound));    \
    LONG array_upper_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetUBound(array.ptr()->parray, 1, &array_upper_bound));    \
    double* array_data;                                                     \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                         \
        array.ptr()->parray, reinterpret_cast<void**>(&array_data)));       \
    size_t count = array_upper_bound - array_lower_bound + 1;               \
    ASSERT_EQ(expected_property_values.size(), count);                      \
    for (size_t i = 0; i < count; ++i) {                                    \
      EXPECT_EQ(array_data[i], expected_property_values[i]);                \
    }                                                                       \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(array.ptr()->parray)); \
  }

#define EXPECT_UIA_INT_EQ(node, property_id, expected)              \
  {                                                                 \
    ScopedVariant expectedVariant(expected, VT_I4);                 \
    ASSERT_EQ(VT_I4, expectedVariant.type());                       \
    ScopedVariant actual;                                           \
    ASSERT_HRESULT_SUCCEEDED(                                       \
        node->GetPropertyValue(property_id, actual.Receive()));     \
    EXPECT_EQ(expectedVariant.ptr()->intVal, actual.ptr()->intVal); \
  }

#define EXPECT_UIA_ELEMENT_ARRAY_BSTR_EQ(node, array_property_id,           \
                                         element_test_property_id,          \
                                         expected_property_values)          \
  {                                                                         \
    ScopedVariant array;                                                    \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        node->GetPropertyValue(array_property_id, array.Receive()));        \
    ASSERT_EQ(VT_ARRAY | VT_UNKNOWN, array.type());                         \
    ASSERT_EQ(1u, SafeArrayGetDim(array.ptr()->parray));                    \
    LONG array_lower_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetLBound(array.ptr()->parray, 1, &array_lower_bound));    \
    LONG array_upper_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetUBound(array.ptr()->parray, 1, &array_upper_bound));    \
    IUnknown** array_data;                                                  \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                         \
        array.ptr()->parray, reinterpret_cast<void**>(&array_data)));       \
    size_t count = array_upper_bound - array_lower_bound + 1;               \
    ASSERT_EQ(expected_property_values.size(), count);                      \
    for (size_t i = 0; i < count; ++i) {                                    \
      CComPtr<IRawElementProviderSimple> element;                           \
      ASSERT_HRESULT_SUCCEEDED(array_data[i]->QueryInterface(&element));    \
      EXPECT_UIA_BSTR_EQ(element, element_test_property_id,                 \
                         expected_property_values[i].c_str());              \
    }                                                                       \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(array.ptr()->parray)); \
  }

#define EXPECT_UIA_UNORDERED_ELEMENT_ARRAY_BSTR_EQ(node, array_property_id,    \
                                                   element_test_property_id,   \
                                                   expected_property_values)   \
  {                                                                            \
    ScopedVariant array;                                                       \
    ASSERT_HRESULT_SUCCEEDED(                                                  \
        node->GetPropertyValue(array_property_id, array.Receive()));           \
    ASSERT_EQ(VT_ARRAY | VT_UNKNOWN, array.type());                            \
    ASSERT_EQ(1u, SafeArrayGetDim(array.ptr()->parray));                       \
    LONG array_lower_bound;                                                    \
    ASSERT_HRESULT_SUCCEEDED(                                                  \
        SafeArrayGetLBound(array.ptr()->parray, 1, &array_lower_bound));       \
    LONG array_upper_bound;                                                    \
    ASSERT_HRESULT_SUCCEEDED(                                                  \
        SafeArrayGetUBound(array.ptr()->parray, 1, &array_upper_bound));       \
    IUnknown** array_data;                                                     \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                            \
        array.ptr()->parray, reinterpret_cast<void**>(&array_data)));          \
    size_t count = array_upper_bound - array_lower_bound + 1;                  \
    ASSERT_EQ(expected_property_values.size(), count);                         \
    std::vector<std::wstring> property_values;                                 \
    for (size_t i = 0; i < count; ++i) {                                       \
      CComPtr<IRawElementProviderSimple> element;                              \
      ASSERT_HRESULT_SUCCEEDED(array_data[i]->QueryInterface(&element));       \
      ScopedVariant actual;                                                    \
      ASSERT_HRESULT_SUCCEEDED(element->GetPropertyValue(                      \
          element_test_property_id, actual.Receive()));                        \
      ASSERT_EQ(VT_BSTR, actual.type());                                       \
      ASSERT_NE(nullptr, actual.ptr()->bstrVal);                               \
      property_values.push_back(std::wstring(                                  \
          V_BSTR(actual.ptr()), SysStringLen(V_BSTR(actual.ptr()))));          \
    }                                                                          \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(array.ptr()->parray));    \
    EXPECT_THAT(property_values,                                               \
                testing::UnorderedElementsAreArray(expected_property_values)); \
  }

AXPlatformNodeWinTest::AXPlatformNodeWinTest() {}
AXPlatformNodeWinTest::~AXPlatformNodeWinTest() {}

void AXPlatformNodeWinTest::SetUp() {
  win::CreateATLModuleIfNeeded();
}

void AXPlatformNodeWinTest::TearDown() {
  // Destroy the tree and make sure we're not leaking any objects.
  ax_fragment_root_.reset(nullptr);
  tree_.reset(nullptr);
  AXNodePosition::SetTreeForTesting(nullptr);
  ASSERT_EQ(0U, AXPlatformNodeBase::GetInstanceCountForTesting());
}

template <typename T>
ComPtr<T> AXPlatformNodeWinTest::QueryInterfaceFromNodeId(int32_t id) {
  const TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(tree_.get(), tree_->GetFromId(id));
  if (!wrapper)
    return ComPtr<T>();

  AXPlatformNode* ax_platform_node = wrapper->ax_platform_node();
  ComPtr<T> result;
  EXPECT_HRESULT_SUCCEEDED(
      ax_platform_node->GetNativeViewAccessible()->QueryInterface(__uuidof(T),
                                                                  &result));
  return result;
}

template <typename T>
ComPtr<T> AXPlatformNodeWinTest::QueryInterfaceFromNode(AXNode* node) {
  const TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(tree_.get(), node);
  if (!wrapper)
    return ComPtr<T>();

  AXPlatformNode* ax_platform_node = wrapper->ax_platform_node();
  ComPtr<T> result;
  EXPECT_HRESULT_SUCCEEDED(
      ax_platform_node->GetNativeViewAccessible()->QueryInterface(__uuidof(T),
                                                                  &result));
  return result;
}

ComPtr<IRawElementProviderSimple>
AXPlatformNodeWinTest::GetRootIRawElementProviderSimple() {
  return QueryInterfaceFromNode<IRawElementProviderSimple>(GetRootNode());
}

ComPtr<IRawElementProviderFragment>
AXPlatformNodeWinTest::GetRootIRawElementProviderFragment() {
  return QueryInterfaceFromNode<IRawElementProviderFragment>(GetRootNode());
}

ComPtr<IAccessible> AXPlatformNodeWinTest::IAccessibleFromNode(AXNode* node) {
  return QueryInterfaceFromNode<IAccessible>(node);
}

ComPtr<IAccessible> AXPlatformNodeWinTest::GetRootIAccessible() {
  return IAccessibleFromNode(GetRootNode());
}

ComPtr<IAccessible2> AXPlatformNodeWinTest::ToIAccessible2(
    ComPtr<IUnknown> unknown) {
  CHECK(unknown);
  ComPtr<IServiceProvider> service_provider;
  unknown.As(&service_provider);
  ComPtr<IAccessible2> result;
  CHECK(SUCCEEDED(
      service_provider->QueryService(IID_IAccessible2, IID_PPV_ARGS(&result))));
  return result;
}

ComPtr<IAccessible2> AXPlatformNodeWinTest::ToIAccessible2(
    ComPtr<IAccessible> accessible) {
  CHECK(accessible);
  ComPtr<IServiceProvider> service_provider;
  accessible.As(&service_provider);
  ComPtr<IAccessible2> result;
  CHECK(SUCCEEDED(
      service_provider->QueryService(IID_IAccessible2, IID_PPV_ARGS(&result))));
  return result;
}

ComPtr<IAccessible2_2> AXPlatformNodeWinTest::ToIAccessible2_2(
    ComPtr<IAccessible> accessible) {
  CHECK(accessible);
  ComPtr<IServiceProvider> service_provider;
  accessible.As(&service_provider);
  ComPtr<IAccessible2_2> result;
  CHECK(SUCCEEDED(service_provider->QueryService(IID_IAccessible2_2,
                                                 IID_PPV_ARGS(&result))));
  return result;
}

void AXPlatformNodeWinTest::CheckVariantHasName(ScopedVariant& variant,
                                                const wchar_t* expected_name) {
  ASSERT_NE(nullptr, variant.ptr());
  ComPtr<IAccessible> accessible;
  ASSERT_HRESULT_SUCCEEDED(
      V_DISPATCH(variant.ptr())->QueryInterface(IID_PPV_ARGS(&accessible)));
  ScopedBstr name;
  EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
  EXPECT_STREQ(expected_name, name);
}

void AXPlatformNodeWinTest::CheckIUnknownHasName(ComPtr<IUnknown> unknown,
                                                 const wchar_t* expected_name) {
  ComPtr<IAccessible2> accessible = ToIAccessible2(unknown);
  ASSERT_NE(nullptr, accessible.Get());

  ScopedBstr name;
  EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
  EXPECT_STREQ(expected_name, name);
}

ComPtr<IAccessibleTableCell> AXPlatformNodeWinTest::GetCellInTable() {
  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable2> table;
  root_obj.As(&table);
  if (!table)
    return ComPtr<IAccessibleTableCell>();

  ComPtr<IUnknown> cell;
  table->get_cellAt(1, 1, &cell);
  if (!cell)
    return ComPtr<IAccessibleTableCell>();

  ComPtr<IAccessibleTableCell> table_cell;
  cell.As(&table_cell);
  return table_cell;
}

void AXPlatformNodeWinTest::InitFragmentRoot() {
  TestAXNodeWrapper* wrapper =
      TestAXNodeWrapper::GetOrCreate(tree_.get(), GetRootNode());

  ax_fragment_root_ = std::make_unique<ui::AXFragmentRootWin>(
      gfx::kMockAcceleratedWidget,
      static_cast<ui::AXPlatformNodeWin*>(wrapper->ax_platform_node()));
}

ComPtr<IRawElementProviderFragmentRoot>
AXPlatformNodeWinTest::GetFragmentRoot() {
  ComPtr<IRawElementProviderFragmentRoot> fragment_root_provider;
  ax_fragment_root_->GetNativeViewAccessible()->QueryInterface(
      IID_PPV_ARGS(&fragment_root_provider));
  return fragment_root_provider;
}

AXPlatformNodeWinTest::PatternSet
AXPlatformNodeWinTest::GetSupportedPatternsFromNodeId(int32_t id) {
  ComPtr<IRawElementProviderSimple> raw_element_provider_simple =
      QueryInterfaceFromNodeId<IRawElementProviderSimple>(id);
  PatternSet supported_patterns;
  static const std::vector<LONG> all_supported_patterns_ = {
      UIA_TextChildPatternId,  UIA_TextEditPatternId,
      UIA_TextPatternId,       UIA_WindowPatternId,
      UIA_InvokePatternId,     UIA_ExpandCollapsePatternId,
      UIA_GridPatternId,       UIA_GridItemPatternId,
      UIA_RangeValuePatternId, UIA_ScrollPatternId,
      UIA_ScrollItemPatternId, UIA_TablePatternId,
      UIA_TableItemPatternId,  UIA_SelectionItemPatternId,
      UIA_SelectionPatternId,  UIA_TogglePatternId,
      UIA_ValuePatternId,
  };
  for (LONG property_id : all_supported_patterns_) {
    CComPtr<IUnknown> provider = nullptr;
    if (SUCCEEDED(raw_element_provider_simple->GetPatternProvider(property_id,
                                                                  &provider)) &&
        provider) {
      supported_patterns.insert(property_id);
    }
  }
  return supported_patterns;
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleDetachedObject) {
  AXNodeData root;
  root.id = 1;
  root.SetName("Name");
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr name;
  EXPECT_EQ(S_OK, root_obj->get_accName(SELF, name.Receive()));
  EXPECT_STREQ(L"Name", name);

  tree_.reset(new AXTree());
  ScopedBstr name2;
  EXPECT_EQ(E_FAIL, root_obj->get_accName(SELF, name2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleHitTest) {
  AXNodeData root;
  root.id = 0;
  root.child_ids.push_back(1);
  root.child_ids.push_back(2);
  root.relative_bounds.bounds = gfx::RectF(0, 0, 30, 30);

  AXNodeData node1;
  node1.id = 1;
  node1.relative_bounds.bounds = gfx::RectF(0, 0, 10, 10);
  node1.SetName("Name1");

  AXNodeData node2;
  node2.id = 2;
  node2.relative_bounds.bounds = gfx::RectF(20, 20, 10, 10);
  node2.SetName("Name2");

  Init(root, node1, node2);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedVariant obj;

  // This is way outside of the root node
  EXPECT_EQ(S_FALSE, root_obj->accHitTest(50, 50, obj.Receive()));
  EXPECT_EQ(VT_EMPTY, obj.type());

  // this is directly on node 1.
  EXPECT_EQ(S_OK, root_obj->accHitTest(5, 5, obj.Receive()));
  ASSERT_NE(nullptr, obj.ptr());

  CheckVariantHasName(obj, L"Name1");
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleName) {
  AXNodeData root;
  root.id = 1;
  root.SetName("Name");
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr name;
  EXPECT_EQ(S_OK, root_obj->get_accName(SELF, name.Receive()));
  EXPECT_STREQ(L"Name", name);

  EXPECT_EQ(E_INVALIDARG, root_obj->get_accName(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr name2;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_accName(bad_id, name2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleDescription) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                          "Description");
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr description;
  EXPECT_EQ(S_OK, root_obj->get_accDescription(SELF, description.Receive()));
  EXPECT_STREQ(L"Description", description);

  EXPECT_EQ(E_INVALIDARG, root_obj->get_accDescription(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr d2;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_accDescription(bad_id, d2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleValue) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(ax::mojom::StringAttribute::kValue, "Value");
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr value;
  EXPECT_EQ(S_OK, root_obj->get_accValue(SELF, value.Receive()));
  EXPECT_STREQ(L"Value", value);

  EXPECT_EQ(E_INVALIDARG, root_obj->get_accValue(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr v2;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_accValue(bad_id, v2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleShortcut) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                          "Shortcut");
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr shortcut;
  EXPECT_EQ(S_OK, root_obj->get_accKeyboardShortcut(SELF, shortcut.Receive()));
  EXPECT_STREQ(L"Shortcut", shortcut);

  EXPECT_EQ(E_INVALIDARG, root_obj->get_accKeyboardShortcut(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr k2;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_accKeyboardShortcut(bad_id, k2.Receive()));
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleSelectionListBoxOptionNothingSelected) {
  AXNodeData list;
  list.id = 0;
  list.role = ax::mojom::Role::kListBox;

  AXNodeData list_item_1;
  list_item_1.id = 1;
  list_item_1.role = ax::mojom::Role::kListBoxOption;
  list_item_1.SetName("Name1");

  AXNodeData list_item_2;
  list_item_2.id = 2;
  list_item_2.role = ax::mojom::Role::kListBoxOption;
  list_item_2.SetName("Name2");

  list.child_ids.push_back(list_item_1.id);
  list.child_ids.push_back(list_item_2.id);

  Init(list, list_item_1, list_item_2);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_EMPTY, selection.type());
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleSelectionListBoxOptionOneSelected) {
  AXNodeData list;
  list.id = 0;
  list.role = ax::mojom::Role::kListBox;

  AXNodeData list_item_1;
  list_item_1.id = 1;
  list_item_1.role = ax::mojom::Role::kListBoxOption;
  list_item_1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  list_item_1.SetName("Name1");

  AXNodeData list_item_2;
  list_item_2.id = 2;
  list_item_2.role = ax::mojom::Role::kListBoxOption;
  list_item_2.SetName("Name2");

  list.child_ids.push_back(list_item_1.id);
  list.child_ids.push_back(list_item_2.id);

  Init(list, list_item_1, list_item_2);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_DISPATCH, selection.type());

  CheckVariantHasName(selection, L"Name1");
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleSelectionListBoxOptionMultipleSelected) {
  AXNodeData list;
  list.id = 0;
  list.role = ax::mojom::Role::kListBox;

  AXNodeData list_item_1;
  list_item_1.id = 1;
  list_item_1.role = ax::mojom::Role::kListBoxOption;
  list_item_1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  list_item_1.SetName("Name1");

  AXNodeData list_item_2;
  list_item_2.id = 2;
  list_item_2.role = ax::mojom::Role::kListBoxOption;
  list_item_2.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  list_item_2.SetName("Name2");

  AXNodeData list_item_3;
  list_item_3.id = 3;
  list_item_3.role = ax::mojom::Role::kListBoxOption;
  list_item_3.SetName("Name3");

  list.child_ids.push_back(list_item_1.id);
  list.child_ids.push_back(list_item_2.id);
  list.child_ids.push_back(list_item_3.id);

  Init(list, list_item_1, list_item_2, list_item_3);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_UNKNOWN, selection.type());
  ASSERT_NE(nullptr, selection.ptr());

  // Loop through the selections and  make sure we have the right ones.
  ComPtr<IEnumVARIANT> accessibles;
  ASSERT_HRESULT_SUCCEEDED(
      V_UNKNOWN(selection.ptr())
          ->QueryInterface(IID_PPV_ARGS(accessibles.GetAddressOf())));
  ULONG retrieved_count;

  // Check out the first selected item.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_OK, hr);

    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(item.ptr())
            ->QueryInterface(IID_PPV_ARGS(accessible.GetAddressOf())));
    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(L"Name1", name);
  }

  // And the second selected element.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_OK, hr);

    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(item.ptr())
            ->QueryInterface(IID_PPV_ARGS(accessible.GetAddressOf())));
    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(L"Name2", name);
  }

  // There shouldn't be any more selected.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_FALSE, hr);
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleSelectionTableNothingSelected) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_EMPTY, selection.type());
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleSelectionTableRowOneSelected) {
  AXTreeUpdate update = Build3X3Table();

  // 5 == table_row_1
  update.nodes[5].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_DISPATCH, selection.type());
  ASSERT_NE(nullptr, selection.ptr());

  ComPtr<IAccessible> row;
  ASSERT_HRESULT_SUCCEEDED(
      V_DISPATCH(selection.ptr())
          ->QueryInterface(IID_PPV_ARGS(row.GetAddressOf())));

  ScopedVariant role;
  EXPECT_HRESULT_SUCCEEDED(row->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_ROW, V_I4(role.ptr()));
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleSelectionTableRowMultipleSelected) {
  AXTreeUpdate update = Build3X3Table();

  // 5 == table_row_1
  // 9 == table_row_2
  update.nodes[5].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[9].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_UNKNOWN, selection.type());
  ASSERT_NE(nullptr, selection.ptr());

  // Loop through the selections and  make sure we have the right ones.
  ComPtr<IEnumVARIANT> accessibles;
  ASSERT_HRESULT_SUCCEEDED(
      V_UNKNOWN(selection.ptr())
          ->QueryInterface(IID_PPV_ARGS(accessibles.GetAddressOf())));
  ULONG retrieved_count;

  // Check out the first selected row.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_OK, hr);

    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(item.ptr())
            ->QueryInterface(IID_PPV_ARGS(accessible.GetAddressOf())));
    ScopedVariant role;
    EXPECT_HRESULT_SUCCEEDED(accessible->get_accRole(SELF, role.Receive()));
    EXPECT_EQ(ROLE_SYSTEM_ROW, V_I4(role.ptr()));
  }

  // And the second selected element.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_OK, hr);

    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(item.ptr())
            ->QueryInterface(IID_PPV_ARGS(accessible.GetAddressOf())));
    ScopedVariant role;
    EXPECT_HRESULT_SUCCEEDED(accessible->get_accRole(SELF, role.Receive()));
    EXPECT_EQ(ROLE_SYSTEM_ROW, V_I4(role.ptr()));
  }

  // There shouldn't be any more selected.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_FALSE, hr);
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleSelectionTableCellOneSelected) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ComPtr<IDispatch> row2;
  ASSERT_HRESULT_SUCCEEDED(
      root_obj->get_accChild(ScopedVariant(2), row2.GetAddressOf()));
  ComPtr<IAccessible> row2_accessible;
  ASSERT_HRESULT_SUCCEEDED(row2.As(&row2_accessible));

  ScopedVariant selection;
  EXPECT_EQ(S_OK, row2_accessible->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_DISPATCH, selection.type());
  ASSERT_NE(nullptr, selection.ptr());

  ComPtr<IAccessible> cell;
  ASSERT_HRESULT_SUCCEEDED(
      V_DISPATCH(selection.ptr())
          ->QueryInterface(IID_PPV_ARGS(cell.GetAddressOf())));

  ScopedVariant role;
  EXPECT_HRESULT_SUCCEEDED(cell->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_CELL, V_I4(role.ptr()));

  ScopedBstr name;
  EXPECT_EQ(S_OK, cell->get_accName(SELF, name.Receive()));
  EXPECT_STREQ(L"1", name);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleSelectionTableCellMultipleSelected) {
  AXTreeUpdate update = Build3X3Table();

  // 11 == table_cell_3
  // 12 == table_cell_4
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ASSERT_NE(nullptr, root_obj.Get());

  ComPtr<IDispatch> row3;
  ASSERT_HRESULT_SUCCEEDED(
      root_obj->get_accChild(ScopedVariant(3), row3.GetAddressOf()));
  ComPtr<IAccessible> row3_accessible;
  ASSERT_HRESULT_SUCCEEDED(row3.As(&row3_accessible));

  ScopedVariant selection;
  EXPECT_EQ(S_OK, row3_accessible->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_UNKNOWN, selection.type());
  ASSERT_NE(nullptr, selection.ptr());

  // Loop through the selections and  make sure we have the right ones.
  ComPtr<IEnumVARIANT> accessibles;
  ASSERT_HRESULT_SUCCEEDED(
      V_UNKNOWN(selection.ptr())
          ->QueryInterface(IID_PPV_ARGS(accessibles.GetAddressOf())));
  ULONG retrieved_count;

  // Check out the first selected cell.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_OK, hr);

    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(item.ptr())
            ->QueryInterface(IID_PPV_ARGS(accessible.GetAddressOf())));
    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(L"3", name);
  }

  // And the second selected cell.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_OK, hr);

    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(item.ptr())
            ->QueryInterface(IID_PPV_ARGS(accessible.GetAddressOf())));
    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(L"4", name);
  }

  // There shouldn't be any more selected.
  {
    ScopedVariant item;
    HRESULT hr = accessibles->Next(1, item.Receive(), &retrieved_count);
    EXPECT_EQ(S_FALSE, hr);
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleRole) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);

  AXNodeData child;
  child.id = 2;

  Init(root, child);
  AXNode* child_node = GetRootNode()->children()[0];
  ComPtr<IAccessible> child_iaccessible(IAccessibleFromNode(child_node));

  ScopedVariant role;

  child.role = ax::mojom::Role::kAlert;
  child_node->SetData(child);
  EXPECT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_ALERT, V_I4(role.ptr()));

  child.role = ax::mojom::Role::kButton;
  child_node->SetData(child);
  EXPECT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, V_I4(role.ptr()));

  child.role = ax::mojom::Role::kPopUpButton;
  child_node->SetData(child);
  EXPECT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONMENU, V_I4(role.ptr()));

  EXPECT_EQ(E_INVALIDARG, child_iaccessible->get_accRole(SELF, nullptr));
  ScopedVariant bad_id(999);
  EXPECT_EQ(E_INVALIDARG,
            child_iaccessible->get_accRole(bad_id, role.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleLocation) {
  AXNodeData root;
  root.id = 1;
  root.relative_bounds.bounds = gfx::RectF(10, 40, 800, 600);
  Init(root);

  TestAXNodeWrapper::SetGlobalCoordinateOffset(gfx::Vector2d(100, 200));

  LONG x_left, y_top, width, height;
  EXPECT_EQ(S_OK, GetRootIAccessible()->accLocation(&x_left, &y_top, &width,
                                                    &height, SELF));
  EXPECT_EQ(110, x_left);
  EXPECT_EQ(240, y_top);
  EXPECT_EQ(800, width);
  EXPECT_EQ(600, height);

  EXPECT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
                              nullptr, &y_top, &width, &height, SELF));
  EXPECT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
                              &x_left, nullptr, &width, &height, SELF));
  EXPECT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
                              &x_left, &y_top, nullptr, &height, SELF));
  EXPECT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
                              &x_left, &y_top, &width, nullptr, SELF));
  ScopedVariant bad_id(999);
  EXPECT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
                              &x_left, &y_top, &width, &height, bad_id));

  // Un-set the global offset so that it doesn't affect subsequent tests.
  TestAXNodeWrapper::SetGlobalCoordinateOffset(gfx::Vector2d(0, 0));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleChildAndParent) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.role = ax::mojom::Role::kButton;
  button.id = 2;

  AXNodeData checkbox;
  checkbox.role = ax::mojom::Role::kCheckBox;
  checkbox.id = 3;

  Init(root, button, checkbox);
  AXNode* button_node = GetRootNode()->children()[0];
  AXNode* checkbox_node = GetRootNode()->children()[1];
  ComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ComPtr<IAccessible> button_iaccessible(IAccessibleFromNode(button_node));
  ComPtr<IAccessible> checkbox_iaccessible(IAccessibleFromNode(checkbox_node));

  LONG child_count;
  EXPECT_EQ(S_OK, root_iaccessible->get_accChildCount(&child_count));
  EXPECT_EQ(2L, child_count);
  EXPECT_EQ(S_OK, button_iaccessible->get_accChildCount(&child_count));
  EXPECT_EQ(0L, child_count);
  EXPECT_EQ(S_OK, checkbox_iaccessible->get_accChildCount(&child_count));
  EXPECT_EQ(0L, child_count);

  {
    ComPtr<IDispatch> result;
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(SELF, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), root_iaccessible.Get());
  }

  {
    ComPtr<IDispatch> result;
    ScopedVariant child1(1);
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(child1, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), button_iaccessible.Get());
  }

  {
    ComPtr<IDispatch> result;
    ScopedVariant child2(2);
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(child2, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), checkbox_iaccessible.Get());
  }

  {
    // Asking for child id 3 should fail.
    ComPtr<IDispatch> result;
    ScopedVariant child3(3);
    EXPECT_EQ(E_INVALIDARG,
              root_iaccessible->get_accChild(child3, result.GetAddressOf()));
  }

  // We should be able to ask for the button by its unique id too.
  LONG button_unique_id;
  ComPtr<IAccessible2> button_iaccessible2 = ToIAccessible2(button_iaccessible);
  button_iaccessible2->get_uniqueID(&button_unique_id);
  ASSERT_LT(button_unique_id, 0);
  {
    ComPtr<IDispatch> result;
    ScopedVariant button_id_variant(button_unique_id);
    EXPECT_EQ(S_OK, root_iaccessible->get_accChild(button_id_variant,
                                                   result.GetAddressOf()));
    EXPECT_EQ(result.Get(), button_iaccessible.Get());
  }

  // We shouldn't be able to ask for the root node by its unique ID
  // from one of its children, though.
  LONG root_unique_id;
  ComPtr<IAccessible2> root_iaccessible2 = ToIAccessible2(root_iaccessible);
  root_iaccessible2->get_uniqueID(&root_unique_id);
  ASSERT_LT(root_unique_id, 0);
  {
    ComPtr<IDispatch> result;
    ScopedVariant root_id_variant(root_unique_id);
    EXPECT_EQ(E_INVALIDARG, button_iaccessible->get_accChild(
                                root_id_variant, result.GetAddressOf()));
  }

  // Now check parents.
  {
    ComPtr<IDispatch> result;
    EXPECT_EQ(S_OK, button_iaccessible->get_accParent(result.GetAddressOf()));
    EXPECT_EQ(result.Get(), root_iaccessible.Get());
  }

  {
    ComPtr<IDispatch> result;
    EXPECT_EQ(S_OK, checkbox_iaccessible->get_accParent(result.GetAddressOf()));
    EXPECT_EQ(result.Get(), root_iaccessible.Get());
  }

  {
    ComPtr<IDispatch> result;
    EXPECT_EQ(S_FALSE, root_iaccessible->get_accParent(result.GetAddressOf()));
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2IndexInParent) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData left;
  left.id = 2;

  AXNodeData right;
  right.id = 3;

  Init(root, left, right);
  ComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ComPtr<IAccessible2> root_iaccessible2 = ToIAccessible2(root_iaccessible);
  ComPtr<IAccessible> left_iaccessible(
      IAccessibleFromNode(GetRootNode()->children()[0]));
  ComPtr<IAccessible2> left_iaccessible2 = ToIAccessible2(left_iaccessible);
  ComPtr<IAccessible> right_iaccessible(
      IAccessibleFromNode(GetRootNode()->children()[1]));
  ComPtr<IAccessible2> right_iaccessible2 = ToIAccessible2(right_iaccessible);

  LONG index;
  EXPECT_EQ(E_FAIL, root_iaccessible2->get_indexInParent(&index));

  EXPECT_EQ(S_OK, left_iaccessible2->get_indexInParent(&index));
  EXPECT_EQ(0, index);

  EXPECT_EQ(S_OK, right_iaccessible2->get_indexInParent(&index));
  EXPECT_EQ(1, index);
}

TEST_F(AXPlatformNodeWinTest, TestAccNavigate) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(2);

  AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(3);

  Init(root, child1, child2);
  ComPtr<IAccessible> ia_root(GetRootIAccessible());
  ComPtr<IDispatch> disp_root;
  ASSERT_HRESULT_SUCCEEDED(ia_root.CopyTo(disp_root.GetAddressOf()));
  ScopedVariant var_root(disp_root.Get());
  ComPtr<IAccessible> ia_child1(
      IAccessibleFromNode(GetRootNode()->children()[0]));
  ComPtr<IDispatch> disp_child1;
  ASSERT_HRESULT_SUCCEEDED(ia_child1.CopyTo(disp_child1.GetAddressOf()));
  ScopedVariant var_child1(disp_child1.Get());
  ComPtr<IAccessible> ia_child2(
      IAccessibleFromNode(GetRootNode()->children()[1]));
  ComPtr<IDispatch> disp_child2;
  ASSERT_HRESULT_SUCCEEDED(ia_child2.CopyTo(disp_child2.GetAddressOf()));
  ScopedVariant var_child2(disp_child2.Get());
  ScopedVariant end;

  // Invalid arguments.
  EXPECT_EQ(
      E_INVALIDARG,
      ia_root->accNavigate(NAVDIR_NEXT, ScopedVariant::kEmptyVariant, nullptr));
  EXPECT_EQ(E_INVALIDARG,
            ia_child1->accNavigate(NAVDIR_NEXT, ScopedVariant::kEmptyVariant,
                                   end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());

  // Navigating to first/last child should only be from self.
  EXPECT_EQ(E_INVALIDARG,
            ia_root->accNavigate(NAVDIR_FIRSTCHILD, var_root, end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());
  EXPECT_EQ(E_INVALIDARG,
            ia_root->accNavigate(NAVDIR_LASTCHILD, var_root, end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());

  // Spatial directions are not supported.
  EXPECT_EQ(E_NOTIMPL, ia_child1->accNavigate(NAVDIR_UP, SELF, end.AsInput()));
  EXPECT_EQ(E_NOTIMPL, ia_root->accNavigate(NAVDIR_DOWN, SELF, end.AsInput()));
  EXPECT_EQ(E_NOTIMPL,
            ia_child1->accNavigate(NAVDIR_RIGHT, SELF, end.AsInput()));
  EXPECT_EQ(E_NOTIMPL,
            ia_child2->accNavigate(NAVDIR_LEFT, SELF, end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());

  // Logical directions should be supported.
  EXPECT_EQ(S_OK, ia_root->accNavigate(NAVDIR_FIRSTCHILD, SELF, end.AsInput()));
  EXPECT_EQ(0, var_child1.Compare(end));
  EXPECT_EQ(S_OK, ia_root->accNavigate(NAVDIR_LASTCHILD, SELF, end.AsInput()));
  EXPECT_EQ(0, var_child2.Compare(end));

  EXPECT_EQ(S_OK, ia_child1->accNavigate(NAVDIR_NEXT, SELF, end.AsInput()));
  EXPECT_EQ(0, var_child2.Compare(end));
  EXPECT_EQ(S_OK, ia_child2->accNavigate(NAVDIR_PREVIOUS, SELF, end.AsInput()));
  EXPECT_EQ(0, var_child1.Compare(end));

  // Child indices can also be passed by variant.
  // Indices are one-based.
  EXPECT_EQ(S_OK,
            ia_root->accNavigate(NAVDIR_NEXT, ScopedVariant(1), end.AsInput()));
  EXPECT_EQ(0, var_child2.Compare(end));
  EXPECT_EQ(S_OK, ia_root->accNavigate(NAVDIR_PREVIOUS, ScopedVariant(2),
                                       end.AsInput()));
  EXPECT_EQ(0, var_child1.Compare(end));

  // Test out-of-bounds.
  EXPECT_EQ(S_FALSE,
            ia_child1->accNavigate(NAVDIR_PREVIOUS, SELF, end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());
  EXPECT_EQ(S_FALSE, ia_child2->accNavigate(NAVDIR_NEXT, SELF, end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());

  EXPECT_EQ(S_FALSE, ia_root->accNavigate(NAVDIR_PREVIOUS, ScopedVariant(1),
                                          end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());
  EXPECT_EQ(S_FALSE,
            ia_root->accNavigate(NAVDIR_NEXT, ScopedVariant(2), end.AsInput()));
  EXPECT_EQ(VT_EMPTY, end.type());
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2TextFieldSetSelection) {
  Init(BuildTextField());

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 0, 1));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 1, 0));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 2, 2));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, IA2_TEXT_OFFSET_CARET,
                                                    IA2_TEXT_OFFSET_LENGTH));

  EXPECT_HRESULT_FAILED(text_field->setSelection(1, 0, 0));
  EXPECT_HRESULT_FAILED(text_field->setSelection(0, 0, 50));
}

// This test is disabled until UpdateStep2ComputeHypertext is migrated over
// to AXPlatformNodeWin because |hypertext_| is only initialized
// on the BrowserAccessibility side.
TEST_F(AXPlatformNodeWinTest,
       DISABLED_TestIAccessible2ContentEditableSetSelection) {
  Init(BuildContentEditable());

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> content_editable;
  ia2_text_field.CopyTo(content_editable.GetAddressOf());
  ASSERT_NE(nullptr, content_editable.Get());

  EXPECT_HRESULT_SUCCEEDED(content_editable->setSelection(0, 0, 1));
  EXPECT_HRESULT_SUCCEEDED(content_editable->setSelection(0, 1, 0));
  EXPECT_HRESULT_SUCCEEDED(content_editable->setSelection(0, 2, 2));
  EXPECT_HRESULT_SUCCEEDED(content_editable->setSelection(
      0, IA2_TEXT_OFFSET_CARET, IA2_TEXT_OFFSET_LENGTH));

  EXPECT_HRESULT_FAILED(content_editable->setSelection(1, 0, 0));
  EXPECT_HRESULT_FAILED(content_editable->setSelection(0, 0, 50));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetAccessibilityAt) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  ComPtr<IUnknown> cell_1;
  EXPECT_EQ(S_OK, result->get_accessibleAt(1, 1, cell_1.GetAddressOf()));
  CheckIUnknownHasName(cell_1, L"1");

  ComPtr<IUnknown> cell_2;
  EXPECT_EQ(S_OK, result->get_accessibleAt(1, 2, cell_2.GetAddressOf()));
  CheckIUnknownHasName(cell_2, L"2");

  ComPtr<IUnknown> cell_3;
  EXPECT_EQ(S_OK, result->get_accessibleAt(2, 1, cell_3.GetAddressOf()));
  CheckIUnknownHasName(cell_3, L"3");

  ComPtr<IUnknown> cell_4;
  EXPECT_EQ(S_OK, result->get_accessibleAt(2, 2, cell_4.GetAddressOf()));
  CheckIUnknownHasName(cell_4, L"4");
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTableGetAccessibilityAtOutOfBounds) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  {
    ComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(-1, -1, cell.GetAddressOf()));
  }

  {
    ComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(0, 5, cell.GetAddressOf()));
  }

  {
    ComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(5, 0, cell.GetAddressOf()));
  }

  {
    ComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(10, 10, cell.GetAddressOf()));
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2ScrollToPoint) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 2000, 2000);

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  child1.relative_bounds.bounds = gfx::RectF(10, 10, 10, 10);
  root.child_ids.push_back(2);

  Init(root, child1);

  ComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ComPtr<IDispatch> result;
  EXPECT_EQ(S_OK, root_iaccessible->get_accChild(ScopedVariant(1),
                                                 result.GetAddressOf()));
  ComPtr<IAccessible2> ax_child1;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child1.GetAddressOf()));
  result.Reset();

  LONG x_left, y_top, width, height;
  EXPECT_EQ(S_OK,
            ax_child1->accLocation(&x_left, &y_top, &width, &height, SELF));
  EXPECT_EQ(10, x_left);
  EXPECT_EQ(10, y_top);
  EXPECT_EQ(10, width);
  EXPECT_EQ(10, height);

  ComPtr<IAccessible2> root_iaccessible2 = ToIAccessible2(root_iaccessible);
  EXPECT_EQ(S_OK, root_iaccessible2->scrollToPoint(
                      IA2_COORDTYPE_SCREEN_RELATIVE, 600, 650));

  EXPECT_EQ(S_OK,
            ax_child1->accLocation(&x_left, &y_top, &width, &height, SELF));
  EXPECT_EQ(610, x_left);
  EXPECT_EQ(660, y_top);
  EXPECT_EQ(10, width);
  EXPECT_EQ(10, height);

  EXPECT_EQ(S_OK, root_iaccessible2->scrollToPoint(
                      IA2_COORDTYPE_PARENT_RELATIVE, 0, 0));

  EXPECT_EQ(S_OK,
            ax_child1->accLocation(&x_left, &y_top, &width, &height, SELF));
  EXPECT_EQ(10, x_left);
  EXPECT_EQ(10, y_top);
  EXPECT_EQ(10, width);
  EXPECT_EQ(10, height);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2ScrollTo) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 2000, 2000);

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  child1.relative_bounds.bounds = gfx::RectF(10, 10, 10, 10);
  root.child_ids.push_back(2);

  Init(root, child1);

  ComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ComPtr<IDispatch> result;
  EXPECT_EQ(S_OK, root_iaccessible->get_accChild(ScopedVariant(1),
                                                 result.GetAddressOf()));
  ComPtr<IAccessible2> ax_child1;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child1.GetAddressOf()));
  result.Reset();

  LONG x_left, y_top, width, height;
  EXPECT_EQ(S_OK,
            ax_child1->accLocation(&x_left, &y_top, &width, &height, SELF));
  EXPECT_EQ(10, x_left);
  EXPECT_EQ(10, y_top);
  EXPECT_EQ(10, width);
  EXPECT_EQ(10, height);

  ComPtr<IAccessible2> root_iaccessible2 = ToIAccessible2(root_iaccessible);
  EXPECT_EQ(S_OK, ax_child1->scrollTo(IA2_SCROLL_TYPE_ANYWHERE));

  EXPECT_EQ(S_OK,
            ax_child1->accLocation(&x_left, &y_top, &width, &height, SELF));
  EXPECT_EQ(0, x_left);
  EXPECT_EQ(0, y_top);
  EXPECT_EQ(10, width);
  EXPECT_EQ(10, height);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetChildIndex) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG id;
  EXPECT_EQ(S_OK, result->get_childIndex(0, 0, &id));
  EXPECT_EQ(id, 0);

  EXPECT_EQ(S_OK, result->get_childIndex(0, 1, &id));
  EXPECT_EQ(id, 1);

  EXPECT_EQ(S_OK, result->get_childIndex(1, 0, &id));
  EXPECT_EQ(id, 3);

  EXPECT_EQ(S_OK, result->get_childIndex(1, 1, &id));
  EXPECT_EQ(id, 4);

  EXPECT_EQ(E_INVALIDARG, result->get_childIndex(-1, -1, &id));
  EXPECT_EQ(E_INVALIDARG, result->get_childIndex(0, 5, &id));
  EXPECT_EQ(E_INVALIDARG, result->get_childIndex(5, 0, &id));
  EXPECT_EQ(E_INVALIDARG, result->get_childIndex(5, 5, &id));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetColumnDescription) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  {
    ScopedBstr name;
    EXPECT_EQ(S_FALSE, result->get_columnDescription(0, name.Receive()));
  }
  {
    ScopedBstr name;
    EXPECT_EQ(S_OK, result->get_columnDescription(1, name.Receive()));
    EXPECT_STREQ(L"column header 1", name);
  }

  {
    ScopedBstr name;
    EXPECT_EQ(S_OK, result->get_columnDescription(2, name.Receive()));
    EXPECT_STREQ(L"column header 2", name);
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetColumnExtentAt) {
  // TODO(dougt) This table doesn't have any spanning cells. This test
  // tests get_columnExtentAt for (1) and an invalid input.
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG columns_spanned;
  EXPECT_EQ(S_OK, result->get_columnExtentAt(1, 1, &columns_spanned));
  EXPECT_EQ(columns_spanned, 1);

  EXPECT_EQ(E_INVALIDARG, result->get_columnExtentAt(-1, -1, &columns_spanned));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetColumnIndex) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG index;
  EXPECT_EQ(S_OK, result->get_columnIndex(2, &index));
  EXPECT_EQ(index, 2);
  EXPECT_EQ(S_OK, result->get_columnIndex(3, &index));
  EXPECT_EQ(index, 0);

  EXPECT_EQ(E_INVALIDARG, result->get_columnIndex(-1, &index));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNColumns) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG count;
  EXPECT_EQ(S_OK, result->get_nColumns(&count));
  EXPECT_EQ(count, 3);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNRows) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG count;
  EXPECT_EQ(S_OK, result->get_nRows(&count));
  EXPECT_EQ(count, 3);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetRowDescription) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  {
    ScopedBstr name;
    EXPECT_EQ(S_FALSE, result->get_rowDescription(0, name.Receive()));
  }
  {
    ScopedBstr name;
    EXPECT_EQ(S_OK, result->get_rowDescription(1, name.Receive()));
    EXPECT_STREQ(L"row header 1", name);
  }

  {
    ScopedBstr name;
    EXPECT_EQ(S_OK, result->get_rowDescription(2, name.Receive()));
    EXPECT_STREQ(L"row header 2", name);
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetRowExtentAt) {
  // TODO(dougt) This table doesn't have any spanning cells. This test
  // tests get_rowExtentAt for (1) and an invalid input.
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG rows_spanned;
  EXPECT_EQ(S_OK, result->get_rowExtentAt(0, 1, &rows_spanned));
  EXPECT_EQ(rows_spanned, 0);

  EXPECT_EQ(E_INVALIDARG, result->get_columnExtentAt(-1, -1, &rows_spanned));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetRowIndex) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG index;
  EXPECT_EQ(S_OK, result->get_rowIndex(2, &index));
  EXPECT_EQ(index, 0);
  EXPECT_EQ(S_OK, result->get_rowIndex(3, &index));
  EXPECT_EQ(index, 1);

  EXPECT_EQ(E_INVALIDARG, result->get_rowIndex(-1, &index));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetRowColumnExtentsAtIndex) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG row, column, row_extents, column_extents;
  boolean is_selected;
  EXPECT_EQ(S_OK,
            result->get_rowColumnExtentsAtIndex(0, &row, &column, &row_extents,
                                                &column_extents, &is_selected));

  EXPECT_EQ(row, 0);
  EXPECT_EQ(column, 0);
  EXPECT_EQ(row_extents, 0);
  EXPECT_EQ(column_extents, 0);

  EXPECT_EQ(E_INVALIDARG,
            result->get_rowColumnExtentsAtIndex(-1, &row, &column, &row_extents,
                                                &column_extents, &is_selected));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetCellAt) {
  Init(Build3X3Table());

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  ComPtr<IAccessibleTable2> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  {
    ComPtr<IUnknown> cell;
    EXPECT_EQ(S_OK, result->get_cellAt(1, 1, cell.GetAddressOf()));
    CheckIUnknownHasName(cell, L"1");
  }

  {
    ComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG, result->get_cellAt(-1, -1, cell.GetAddressOf()));
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetColumnExtent) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  LONG column_spanned;
  EXPECT_EQ(S_OK, cell->get_columnExtent(&column_spanned));
  EXPECT_EQ(column_spanned, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetColumnHeaderCells) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  IUnknown** cell_accessibles;

  LONG number_cells;
  EXPECT_EQ(S_OK,
            cell->get_columnHeaderCells(&cell_accessibles, &number_cells));
  EXPECT_EQ(number_cells, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetColumnIndex) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  LONG index;
  EXPECT_EQ(S_OK, cell->get_columnIndex(&index));
  EXPECT_EQ(index, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowExtent) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  LONG rows_spanned;
  EXPECT_EQ(S_OK, cell->get_rowExtent(&rows_spanned));
  EXPECT_EQ(rows_spanned, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowHeaderCells) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  IUnknown** cell_accessibles;

  LONG number_cells;
  EXPECT_EQ(S_OK, cell->get_rowHeaderCells(&cell_accessibles, &number_cells));
  EXPECT_EQ(number_cells, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowIndex) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  LONG index;
  EXPECT_EQ(S_OK, cell->get_rowIndex(&index));
  EXPECT_EQ(index, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowColumnExtent) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  LONG row, column, row_extents, column_extents;
  boolean is_selected;
  EXPECT_EQ(S_OK, cell->get_rowColumnExtents(&row, &column, &row_extents,
                                             &column_extents, &is_selected));
  EXPECT_EQ(row, 1);
  EXPECT_EQ(column, 1);
  EXPECT_EQ(row_extents, 1);
  EXPECT_EQ(column_extents, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetTable) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  // Check to make sure that this is the right table by checking one cell.
  ComPtr<IUnknown> cell_1;
  EXPECT_EQ(S_OK, result->get_accessibleAt(1, 1, cell_1.GetAddressOf()));
  CheckIUnknownHasName(cell_1, L"1");
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2GetNRelations) {
  // This is is a duplicated of
  // BrowserAccessibilityTest::TestIAccessible2Relations but without the
  // specific COM/BrowserAccessibility knowledge.
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  std::vector<int32_t> describedby_ids = {1, 2, 3};
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds,
                           describedby_ids);

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;

  root.child_ids.push_back(2);

  AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;

  root.child_ids.push_back(3);

  Init(root, child1, child2);
  ComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ComPtr<IAccessible2> root_iaccessible2 = ToIAccessible2(root_iaccessible);

  ComPtr<IDispatch> result;
  EXPECT_EQ(S_OK, root_iaccessible2->get_accChild(ScopedVariant(1),
                                                  result.GetAddressOf()));
  ComPtr<IAccessible2> ax_child1;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child1.GetAddressOf()));
  result.Reset();

  EXPECT_EQ(S_OK, root_iaccessible2->get_accChild(ScopedVariant(2),
                                                  result.GetAddressOf()));
  ComPtr<IAccessible2> ax_child2;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child2.GetAddressOf()));
  result.Reset();

  LONG n_relations = 0;
  LONG n_targets = 0;
  ScopedBstr relation_type;
  ComPtr<IAccessibleRelation> describedby_relation;
  ComPtr<IAccessibleRelation> description_for_relation;
  ComPtr<IUnknown> target;

  EXPECT_HRESULT_SUCCEEDED(root_iaccessible2->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(
      root_iaccessible2->get_relation(0, describedby_relation.GetAddressOf()));

  EXPECT_HRESULT_SUCCEEDED(
      describedby_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"describedBy", base::string16(relation_type));

  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(describedby_relation->get_nTargets(&n_targets));
  EXPECT_EQ(2, n_targets);

  EXPECT_HRESULT_SUCCEEDED(
      describedby_relation->get_target(0, target.GetAddressOf()));
  target.Reset();

  EXPECT_HRESULT_SUCCEEDED(
      describedby_relation->get_target(1, target.GetAddressOf()));
  target.Reset();

  describedby_relation.Reset();

  // Test the reverse relations.
  EXPECT_HRESULT_SUCCEEDED(ax_child1->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(
      ax_child1->get_relation(0, description_for_relation.GetAddressOf()));
  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"descriptionFor", base::string16(relation_type));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_nTargets(&n_targets));
  EXPECT_EQ(1, n_targets);

  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_target(0, target.GetAddressOf()));
  target.Reset();
  description_for_relation.Reset();

  EXPECT_HRESULT_SUCCEEDED(ax_child2->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(
      ax_child2->get_relation(0, description_for_relation.GetAddressOf()));
  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"descriptionFor", base::string16(relation_type));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_nTargets(&n_targets));
  EXPECT_EQ(1, n_targets);

  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_target(0, target.GetAddressOf()));
  target.Reset();

  // TODO(dougt): Try adding one more relation.
}

TEST_F(AXPlatformNodeWinTest, TestRelationTargetsOfType) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddIntAttribute(ax::mojom::IntAttribute::kDetailsId, 2);

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;

  root.child_ids.push_back(2);

  AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  std::vector<int32_t> labelledby_ids = {1, 4};
  child2.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                             labelledby_ids);

  root.child_ids.push_back(3);

  AXNodeData child3;
  child3.id = 4;
  child3.role = ax::mojom::Role::kStaticText;
  child3.AddIntAttribute(ax::mojom::IntAttribute::kDetailsId, 2);

  root.child_ids.push_back(4);

  Init(root, child1, child2, child3);
  ComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ComPtr<IAccessible2_2> root_iaccessible2 = ToIAccessible2_2(root_iaccessible);

  ComPtr<IDispatch> result;
  EXPECT_EQ(S_OK, root_iaccessible2->get_accChild(ScopedVariant(1),
                                                  result.GetAddressOf()));
  ComPtr<IAccessible2_2> ax_child1;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child1.GetAddressOf()));
  result.Reset();

  EXPECT_EQ(S_OK, root_iaccessible2->get_accChild(ScopedVariant(2),
                                                  result.GetAddressOf()));
  ComPtr<IAccessible2_2> ax_child2;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child2.GetAddressOf()));
  result.Reset();

  EXPECT_EQ(S_OK, root_iaccessible2->get_accChild(ScopedVariant(3),
                                                  result.GetAddressOf()));
  ComPtr<IAccessible2_2> ax_child3;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child3.GetAddressOf()));
  result.Reset();

  {
    ScopedBstr type(L"details");
    IUnknown** targets;
    LONG n_targets;
    EXPECT_EQ(S_OK, root_iaccessible2->get_relationTargetsOfType(
                        type, 0, &targets, &n_targets));
    ASSERT_EQ(1, n_targets);
    EXPECT_EQ(ax_child1.Get(), targets[0]);
    CoTaskMemFree(targets);
  }

  {
    ScopedBstr type(IA2_RELATION_LABELLED_BY);
    IUnknown** targets;
    LONG n_targets;
    EXPECT_EQ(S_OK, ax_child2->get_relationTargetsOfType(type, 0, &targets,
                                                         &n_targets));
    ASSERT_EQ(2, n_targets);
    EXPECT_EQ(root_iaccessible2.Get(), targets[0]);
    EXPECT_EQ(ax_child3.Get(), targets[1]);
    CoTaskMemFree(targets);
  }

  {
    ScopedBstr type(L"detailsFor");
    IUnknown** targets;
    LONG n_targets;
    EXPECT_EQ(S_OK, ax_child1->get_relationTargetsOfType(type, 0, &targets,
                                                         &n_targets));
    ASSERT_EQ(2, n_targets);
    EXPECT_EQ(root_iaccessible2.Get(), targets[0]);
    EXPECT_EQ(ax_child3.Get(), targets[1]);
    CoTaskMemFree(targets);
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedChildrenZero) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedChildren;
  EXPECT_EQ(S_OK, result->get_nSelectedChildren(&selectedChildren));
  EXPECT_EQ(0, selectedChildren);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedChildrenOne) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedChildren;
  EXPECT_EQ(S_OK, result->get_nSelectedChildren(&selectedChildren));
  EXPECT_EQ(1, selectedChildren);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedChildrenMany) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  // 8 == table_cell_2
  // 11 == table_cell_3
  // 12 == table_cell_4
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedChildren;
  EXPECT_EQ(S_OK, result->get_nSelectedChildren(&selectedChildren));
  EXPECT_EQ(4, selectedChildren);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedColumnsZero) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedColumns;
  EXPECT_EQ(S_OK, result->get_nSelectedColumns(&selectedColumns));
  EXPECT_EQ(0, selectedColumns);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedColumnsOne) {
  AXTreeUpdate update = Build3X3Table();

  // 3 == table_column_header_2
  // 7 == table_cell_1
  // 11 == table_cell_3
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedColumns;
  EXPECT_EQ(S_OK, result->get_nSelectedColumns(&selectedColumns));
  EXPECT_EQ(1, selectedColumns);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedColumnsMany) {
  AXTreeUpdate update = Build3X3Table();

  // 3 == table_column_header_2
  // 7 == table_cell_1
  // 11 == table_cell_3
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  // 4 == table_column_header_3
  // 8 == table_cell_2
  // 12 == table_cell_4
  update.nodes[4].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedColumns;
  EXPECT_EQ(S_OK, result->get_nSelectedColumns(&selectedColumns));
  EXPECT_EQ(2, selectedColumns);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedRowsZero) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedRows;
  EXPECT_EQ(S_OK, result->get_nSelectedRows(&selectedRows));
  EXPECT_EQ(0, selectedRows);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedRowsOne) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_1
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedRows;
  EXPECT_EQ(S_OK, result->get_nSelectedRows(&selectedRows));
  EXPECT_EQ(1, selectedRows);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedRowsMany) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_3
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  // 10 == table_row_header_3
  // 11 == table_cell_1
  // 12 == table_cell_2
  update.nodes[10].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedRows;
  EXPECT_EQ(S_OK, result->get_nSelectedRows(&selectedRows));
  EXPECT_EQ(2, selectedRows);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedChildren) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  // 12 == table_cell_4
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG max = 10;
  LONG* indices;
  LONG count;
  EXPECT_EQ(S_OK, result->get_selectedChildren(max, &indices, &count));
  EXPECT_EQ(2, count);
  EXPECT_EQ(4, indices[0]);
  EXPECT_EQ(8, indices[1]);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedChildrenZeroMax) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  // 12 == table_cell_4
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG* indices;
  LONG count;
  EXPECT_EQ(E_INVALIDARG, result->get_selectedChildren(0, &indices, &count));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedColumnsZero) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  // 11 == table_cell_3
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG max_columns = 10;
  LONG* columns;
  LONG n_columns;
  EXPECT_EQ(S_OK,
            result->get_selectedColumns(max_columns, &columns, &n_columns));
  EXPECT_EQ(0, n_columns);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedColumnsOne) {
  AXTreeUpdate update = Build3X3Table();

  // 3 == table_column_header_2
  // 7 == table_cell_1
  // 11 == table_cell_3
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG max_columns = 10;
  LONG* columns;
  LONG n_columns;
  EXPECT_EQ(S_OK,
            result->get_selectedColumns(max_columns, &columns, &n_columns));
  EXPECT_EQ(1, n_columns);
  EXPECT_EQ(1, columns[0]);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedColumnsMany) {
  AXTreeUpdate update = Build3X3Table();

  // 3 == table_column_header_2
  // 7 == table_cell_1
  // 11 == table_cell_3
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  // 4 == table_column_header_3
  // 8 == table_cell_2
  // 12 == table_cell_4
  update.nodes[4].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG max_columns = 10;
  LONG* columns;
  LONG n_columns;
  EXPECT_EQ(S_OK,
            result->get_selectedColumns(max_columns, &columns, &n_columns));
  EXPECT_EQ(2, n_columns);
  EXPECT_EQ(1, columns[0]);
  EXPECT_EQ(2, columns[1]);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedRowsZero) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG max_rows = 10;
  LONG* rows;
  LONG n_rows;
  EXPECT_EQ(S_OK, result->get_selectedRows(max_rows, &rows, &n_rows));
  EXPECT_EQ(0, n_rows);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedRowsOne) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_1
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG max_rows = 10;
  LONG* rows;
  LONG n_rows;
  EXPECT_EQ(S_OK, result->get_selectedRows(max_rows, &rows, &n_rows));
  EXPECT_EQ(1, n_rows);
  EXPECT_EQ(1, rows[0]);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedRowsMany) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_3
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  // 10 == table_row_header_3
  // 11 == table_cell_1
  // 12 == table_cell_2
  update.nodes[10].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG max_rows = 10;
  LONG* rows;
  LONG n_rows;
  EXPECT_EQ(S_OK, result->get_selectedRows(max_rows, &rows, &n_rows));
  EXPECT_EQ(2, n_rows);
  EXPECT_EQ(1, rows[0]);
  EXPECT_EQ(2, rows[1]);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableIsColumnSelected) {
  AXTreeUpdate update = Build3X3Table();

  // 3 == table_column_header_2
  // 7 == table_cell_1
  // 11 == table_cell_3
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[11].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  boolean selected;
  EXPECT_EQ(S_OK, result->get_isColumnSelected(0, &selected));
  EXPECT_FALSE(selected);

  EXPECT_EQ(S_OK, result->get_isColumnSelected(1, &selected));
  EXPECT_TRUE(selected);

  EXPECT_EQ(S_OK, result->get_isColumnSelected(2, &selected));
  EXPECT_FALSE(selected);

  EXPECT_EQ(S_FALSE, result->get_isColumnSelected(3, &selected));
  EXPECT_EQ(S_FALSE, result->get_isColumnSelected(-3, &selected));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableIsRowSelected) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_3
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  boolean selected;
  EXPECT_EQ(S_OK, result->get_isRowSelected(0, &selected));
  EXPECT_FALSE(selected);

  EXPECT_EQ(S_OK, result->get_isRowSelected(1, &selected));
  EXPECT_TRUE(selected);

  EXPECT_EQ(S_OK, result->get_isRowSelected(2, &selected));
  EXPECT_FALSE(selected);

  EXPECT_EQ(S_FALSE, result->get_isRowSelected(3, &selected));
  EXPECT_EQ(S_FALSE, result->get_isRowSelected(-3, &selected));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableIsSelected) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_3
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[8].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  boolean selected;

  EXPECT_EQ(S_OK, result->get_isSelected(0, 0, &selected));
  EXPECT_FALSE(selected);

  EXPECT_EQ(S_OK, result->get_isSelected(0, 1, &selected));
  EXPECT_FALSE(selected);

  EXPECT_EQ(S_OK, result->get_isSelected(0, 2, &selected));
  EXPECT_FALSE(selected);

  EXPECT_EQ(S_FALSE, result->get_isSelected(0, 4, &selected));

  EXPECT_EQ(S_OK, result->get_isSelected(1, 0, &selected));
  EXPECT_TRUE(selected);

  EXPECT_EQ(S_OK, result->get_isSelected(1, 1, &selected));
  EXPECT_TRUE(selected);

  EXPECT_EQ(S_OK, result->get_isSelected(1, 2, &selected));
  EXPECT_TRUE(selected);

  EXPECT_EQ(S_FALSE, result->get_isSelected(1, 4, &selected));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTable2GetSelectedChildrenZero) {
  Init(Build3X3Table());

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable2> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  IUnknown** cell_accessibles;
  LONG count;
  EXPECT_EQ(S_OK, result->get_selectedCells(&cell_accessibles, &count));
  EXPECT_EQ(0, count);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTable2GetSelectedChildren) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  // 12 == table_cell_4
  update.nodes[7].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes[12].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  Init(update);

  ComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ComPtr<IAccessibleTable2> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  IUnknown** cell_accessibles;
  LONG count;
  EXPECT_EQ(S_OK, result->get_selectedCells(&cell_accessibles, &count));
  EXPECT_EQ(2, count);

  ComPtr<IUnknown> table_cell_1(cell_accessibles[0]);
  CheckIUnknownHasName(table_cell_1, L"1");

  ComPtr<IUnknown> table_cell_4(cell_accessibles[1]);
  CheckIUnknownHasName(table_cell_4, L"4");
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2GetGroupPosition) {
  AXNodeData root;
  root.id = 1;
  root.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 1);
  root.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, 1);
  root.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, 1);
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ComPtr<IAccessible2> iaccessible2 = ToIAccessible2(root_obj);
  LONG level, similar, position;
  EXPECT_EQ(S_OK, iaccessible2->get_groupPosition(&level, &similar, &position));
  EXPECT_EQ(1, level);
  EXPECT_EQ(0, similar);
  EXPECT_EQ(0, position);

  EXPECT_EQ(E_INVALIDARG,
            iaccessible2->get_groupPosition(nullptr, nullptr, nullptr));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2GetLocalizedExtendedRole) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                          "extended role");
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ComPtr<IAccessible2> iaccessible2 = ToIAccessible2(root_obj);
  ScopedBstr role;
  EXPECT_EQ(S_OK, iaccessible2->get_localizedExtendedRole(role.Receive()));
  EXPECT_STREQ(L"extended role", role);
}

TEST_F(AXPlatformNodeWinTest, TestUnlabeledImageRoleDescription) {
  AXNodeData root;
  root.id = 1;
  root.SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ComPtr<IAccessible2> iaccessible2 = ToIAccessible2(root_obj);
  ScopedBstr role_description;
  EXPECT_EQ(S_OK, iaccessible2->get_localizedExtendedRole(
                      role_description.Receive()));
  EXPECT_STREQ(L"Unlabeled image", role_description);
}

TEST_F(AXPlatformNodeWinTest, TestUnlabeledImageAttributes) {
  AXNodeData root;
  root.id = 1;
  root.SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  Init(root);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());
  ComPtr<IAccessible2> iaccessible2 = ToIAccessible2(root_obj);

  ScopedBstr attributes_bstr;
  EXPECT_EQ(S_OK, iaccessible2->get_attributes(attributes_bstr.Receive()));
  base::string16 attributes(attributes_bstr);

  std::vector<base::string16> attribute_vector = base::SplitString(
      attributes, L";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  bool found = false;
  for (base::string16 attribute : attribute_vector) {
    if (attribute == L"roledescription:Unlabeled image")
      found = true;
  }
  EXPECT_TRUE(found);
}

TEST_F(AXPlatformNodeWinTest, TestAnnotatedImageName) {
  std::vector<const wchar_t*> expected_names;

  AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(10);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3, 4, 5, 6, 7, 8, 9, 10};

  // If the status is EligibleForAnnotation and there's no existing label,
  // the name should be the discoverability string.
  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  expected_names.push_back(
      L"To get missing image descriptions, open the context menu.");

  // If the status is EligibleForAnnotation, the discoverability string
  // should be appended to the existing name.
  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[2].SetName("ExistingLabel");
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  expected_names.push_back(
      L"ExistingLabel. To get missing image descriptions, open the context "
      L"menu.");

  // If the status is IneligibleForAnnotation, nothing should be appended.
  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kImage;
  tree.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[3].SetName("ExistingLabel");
  tree.nodes[3].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
  expected_names.push_back(L"ExistingLabel");

  // If the status is AnnotationPending, pending text should be appended
  // to the name.
  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kImage;
  tree.nodes[4].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[4].SetName("ExistingLabel");
  tree.nodes[4].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  expected_names.push_back(L"ExistingLabel. Getting description...");

  // If the status is AnnotationSucceeded, and there's no annotation,
  // nothing should be appended. (Ideally this shouldn't happen.)
  tree.nodes[5].id = 6;
  tree.nodes[5].role = ax::mojom::Role::kImage;
  tree.nodes[5].SetName("ExistingLabel");
  tree.nodes[5].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  expected_names.push_back(L"ExistingLabel");

  // If the status is AnnotationSucceeded, the annotation should be appended
  // to the existing label.
  tree.nodes[6].id = 7;
  tree.nodes[6].role = ax::mojom::Role::kImage;
  tree.nodes[6].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[6].SetName("ExistingLabel");
  tree.nodes[6].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  expected_names.push_back(L"ExistingLabel. Annotation");

  // If the status is AnnotationEmpty, no failure text should be added to the
  // name.
  tree.nodes[7].id = 8;
  tree.nodes[7].role = ax::mojom::Role::kImage;
  tree.nodes[7].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[7].SetName("ExistingLabel");
  tree.nodes[7].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);
  expected_names.push_back(L"ExistingLabel");

  // If the status is AnnotationAdult, appropriate text should be appended
  // to the name.
  tree.nodes[8].id = 9;
  tree.nodes[8].role = ax::mojom::Role::kImage;
  tree.nodes[8].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[8].SetName("ExistingLabel");
  tree.nodes[8].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationAdult);
  expected_names.push_back(
      L"ExistingLabel. Appears to contain adult content. No description "
      L"available.");

  // If the status is AnnotationProcessFailed, no failure text should be added
  // to the name.
  tree.nodes[9].id = 10;
  tree.nodes[9].role = ax::mojom::Role::kImage;
  tree.nodes[9].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[9].SetName("ExistingLabel");
  tree.nodes[9].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed);
  expected_names.push_back(L"ExistingLabel");

  // We should have one expected name per child of the root.
  ASSERT_EQ(expected_names.size(), tree.nodes[0].child_ids.size());
  int child_count = static_cast<int>(expected_names.size());

  Init(tree);

  ComPtr<IAccessible> root_obj(GetRootIAccessible());

  for (int child_index = 0; child_index < child_count; child_index++) {
    ComPtr<IDispatch> child_dispatch;
    ASSERT_HRESULT_SUCCEEDED(root_obj->get_accChild(
        ScopedVariant(child_index + 1), &child_dispatch));
    ComPtr<IAccessible> child;
    ASSERT_HRESULT_SUCCEEDED(child_dispatch.As(&child));

    ScopedBstr name;
    EXPECT_EQ(S_OK, child->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(expected_names[child_index], name);
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTextGetNCharacters) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(1);

  AXNodeData node;
  node.id = 1;
  node.role = ax::mojom::Role::kStaticText;
  node.SetName("Name");

  Init(root, node);

  AXNode* child_node = GetRootNode()->children()[0];
  ComPtr<IAccessible> child_iaccessible(IAccessibleFromNode(child_node));
  ASSERT_NE(nullptr, child_iaccessible.Get());

  ComPtr<IAccessibleText> text;
  child_iaccessible.CopyTo(text.GetAddressOf());
  ASSERT_NE(nullptr, text.Get());

  LONG count;
  EXPECT_HRESULT_SUCCEEDED(text->get_nCharacters(&count));
  EXPECT_EQ(4, count);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTextTextFieldRemoveSelection) {
  Init(BuildTextFieldWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;
  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);

  EXPECT_HRESULT_SUCCEEDED(text_field->removeSelection(0));

  // There is no selection, just a caret.
  EXPECT_EQ(E_INVALIDARG,
            text_field->get_selection(0, &start_offset, &end_offset));
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTextContentEditableRemoveSelection) {
  Init(BuildTextFieldWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;
  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);

  EXPECT_HRESULT_SUCCEEDED(text_field->removeSelection(0));

  // There is no selection, just a caret.
  EXPECT_EQ(E_INVALIDARG,
            text_field->get_selection(0, &start_offset, &end_offset));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTextTextFieldGetSelected) {
  Init(BuildTextFieldWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;

  // We only care about selection_index of zero, so passing anything but 0 as
  // the first parameter should fail.
  EXPECT_EQ(E_INVALIDARG,
            text_field->get_selection(1, &start_offset, &end_offset));

  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTextTextFieldGetSelectedBackward) {
  Init(BuildTextFieldWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;
  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleContentEditabledGetSelected) {
  Init(BuildContentEditableWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;

  // We only care about selection_index of zero, so passing anything but 0 as
  // the first parameter should fail.
  EXPECT_EQ(E_INVALIDARG,
            text_field->get_selection(1, &start_offset, &end_offset));

  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleContentEditabledGetSelectedBackward) {
  Init(BuildContentEditableWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;
  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTextTextFieldAddSelection) {
  Init(BuildTextField());
  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;
  // There is no selection, just a caret.
  EXPECT_EQ(E_INVALIDARG,
            text_field->get_selection(0, &start_offset, &end_offset));

  EXPECT_HRESULT_SUCCEEDED(text_field->addSelection(1, 2));

  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);
}

// This test is disabled until UpdateStep2ComputeHypertext is migrated over
// to AXPlatformNodeWin because |hypertext_| is only initialized
// on the BrowserAccessibility side.
TEST_F(AXPlatformNodeWinTest,
       DISABLED_TestIAccessibleTextContentEditableAddSelection) {
  Init(BuildContentEditable());

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG start_offset, end_offset;
  // There is no selection, just a caret.
  EXPECT_EQ(E_INVALIDARG,
            text_field->get_selection(0, &start_offset, &end_offset));

  EXPECT_HRESULT_SUCCEEDED(text_field->addSelection(1, 2));

  EXPECT_HRESULT_SUCCEEDED(
      text_field->get_selection(0, &start_offset, &end_offset));
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(2, end_offset);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTextTextFieldGetNSelectionsZero) {
  Init(BuildTextField());

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG selections;
  EXPECT_HRESULT_SUCCEEDED(text_field->get_nSelections(&selections));
  EXPECT_EQ(0, selections);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTextContentEditableGetNSelectionsZero) {
  Init(BuildContentEditable());

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG selections;
  EXPECT_HRESULT_SUCCEEDED(text_field->get_nSelections(&selections));
  EXPECT_EQ(0, selections);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTextContentEditableGetNSelections) {
  Init(BuildContentEditableWithSelectionRange(1, 2));
  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG selections;
  EXPECT_HRESULT_SUCCEEDED(text_field->get_nSelections(&selections));
  EXPECT_EQ(1, selections);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTextTextFieldGetCaretOffsetNoCaret) {
  Init(BuildTextField());

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG offset;
  EXPECT_EQ(S_FALSE, text_field->get_caretOffset(&offset));
  EXPECT_EQ(0, offset);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTextTextFieldGetCaretOffsetHasCaret) {
  Init(BuildTextFieldWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG offset;
  EXPECT_HRESULT_SUCCEEDED(text_field->get_caretOffset(&offset));
  EXPECT_EQ(2, offset);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTextContextEditableGetCaretOffsetNoCaret) {
  Init(BuildContentEditable());

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG offset;
  EXPECT_EQ(S_FALSE, text_field->get_caretOffset(&offset));
  EXPECT_EQ(0, offset);
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTextContentEditableGetCaretOffsetHasCaret) {
  Init(BuildContentEditableWithSelectionRange(1, 2));

  ComPtr<IAccessible2> ia2_text_field = ToIAccessible2(GetRootIAccessible());
  ComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  LONG offset;
  EXPECT_HRESULT_SUCCEEDED(text_field->get_caretOffset(&offset));
  EXPECT_EQ(2, offset);
}

TEST_F(AXPlatformNodeWinTest, TestIGridProviderGetRowCount) {
  Init(BuildAriaColumnAndRowCountGrids());

  // Empty Grid
  ComPtr<IGridProvider> grid1_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[0]);

  // Grid with a cell that defines aria-rowindex (4) and aria-colindex (5)
  ComPtr<IGridProvider> grid2_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[1]);

  // Grid that specifies aria-rowcount (2) and aria-colcount (3)
  ComPtr<IGridProvider> grid3_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[2]);

  // Grid that specifies aria-rowcount and aria-colcount are both (-1)
  ComPtr<IGridProvider> grid4_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[3]);

  int row_count;

  EXPECT_HRESULT_SUCCEEDED(grid1_provider->get_RowCount(&row_count));
  EXPECT_EQ(row_count, 0);

  EXPECT_HRESULT_SUCCEEDED(grid2_provider->get_RowCount(&row_count));
  EXPECT_EQ(row_count, 4);

  EXPECT_HRESULT_SUCCEEDED(grid3_provider->get_RowCount(&row_count));
  EXPECT_EQ(row_count, 2);

  EXPECT_EQ(E_UNEXPECTED, grid4_provider->get_RowCount(&row_count));
}

TEST_F(AXPlatformNodeWinTest, TestIGridProviderGetColumnCount) {
  Init(BuildAriaColumnAndRowCountGrids());

  // Empty Grid
  ComPtr<IGridProvider> grid1_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[0]);

  // Grid with a cell that defines aria-rowindex (4) and aria-colindex (5)
  ComPtr<IGridProvider> grid2_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[1]);

  // Grid that specifies aria-rowcount (2) and aria-colcount (3)
  ComPtr<IGridProvider> grid3_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[2]);

  // Grid that specifies aria-rowcount and aria-colcount are both (-1)
  ComPtr<IGridProvider> grid4_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()->children()[3]);

  int column_count;

  EXPECT_HRESULT_SUCCEEDED(grid1_provider->get_ColumnCount(&column_count));
  EXPECT_EQ(column_count, 0);

  EXPECT_HRESULT_SUCCEEDED(grid2_provider->get_ColumnCount(&column_count));
  EXPECT_EQ(column_count, 5);

  EXPECT_HRESULT_SUCCEEDED(grid3_provider->get_ColumnCount(&column_count));
  EXPECT_EQ(column_count, 3);

  EXPECT_EQ(E_UNEXPECTED, grid4_provider->get_ColumnCount(&column_count));
}

TEST_F(AXPlatformNodeWinTest, TestIGridProviderGetItem) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kGrid;
  root.AddIntAttribute(ax::mojom::IntAttribute::kAriaRowCount, 1);
  root.AddIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount, 1);

  AXNodeData row1;
  row1.id = 2;
  row1.role = ax::mojom::Role::kRow;
  root.child_ids.push_back(row1.id);

  AXNodeData cell1;
  cell1.id = 3;
  cell1.role = ax::mojom::Role::kCell;
  row1.child_ids.push_back(cell1.id);

  Init(root, row1, cell1);

  ComPtr<IGridProvider> root_igridprovider(
      QueryInterfaceFromNode<IGridProvider>(GetRootNode()));

  ComPtr<IRawElementProviderSimple> cell1_irawelementprovidersimple(
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[0]->children()[0]));

  IRawElementProviderSimple* grid_item = nullptr;
  EXPECT_HRESULT_SUCCEEDED(root_igridprovider->GetItem(0, 0, &grid_item));
  EXPECT_NE(nullptr, grid_item);
  EXPECT_EQ(cell1_irawelementprovidersimple.Get(), grid_item);
}

TEST_F(AXPlatformNodeWinTest, TestITableProviderGetColumnHeaders) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kTable;

  AXNodeData row1;
  row1.id = 2;
  row1.role = ax::mojom::Role::kRow;
  root.child_ids.push_back(row1.id);

  AXNodeData column_header;
  column_header.id = 3;
  column_header.role = ax::mojom::Role::kColumnHeader;
  row1.child_ids.push_back(column_header.id);

  AXNodeData row_header;
  row_header.id = 4;
  row_header.role = ax::mojom::Role::kRowHeader;
  row1.child_ids.push_back(row_header.id);

  Init(root, row1, column_header, row_header);

  ComPtr<ITableProvider> root_itableprovider(
      QueryInterfaceFromNode<ITableProvider>(GetRootNode()));

  ComPtr<IRawElementProviderSimple> column_header_irawelementprovidersimple(
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[0]->children()[0]));

  SAFEARRAY* safearray = nullptr;
  EXPECT_HRESULT_SUCCEEDED(root_itableprovider->GetColumnHeaders(&safearray));
  EXPECT_NE(nullptr, safearray);
  EXPECT_EQ(1U, ::SafeArrayGetDim(safearray));
  EXPECT_EQ(sizeof(IRawElementProviderSimple*),
            ::SafeArrayGetElemsize(safearray));

  LONG array_lbound;
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayGetLBound(safearray, 1, &array_lbound));
  EXPECT_EQ(0, array_lbound);

  LONG array_ubound;
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayGetUBound(safearray, 1, &array_ubound));
  EXPECT_EQ(0, array_ubound);

  LONG index = 0;
  CComPtr<IRawElementProviderSimple> array_element;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetElement(safearray, &index, &array_element));
  EXPECT_NE(nullptr, array_element);
  EXPECT_EQ(array_element, column_header_irawelementprovidersimple.Get());
}

TEST_F(AXPlatformNodeWinTest, TestITableProviderGetRowHeaders) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kTable;

  AXNodeData row1;
  row1.id = 2;
  row1.role = ax::mojom::Role::kRow;
  root.child_ids.push_back(row1.id);

  AXNodeData column_header;
  column_header.id = 3;
  column_header.role = ax::mojom::Role::kColumnHeader;
  row1.child_ids.push_back(column_header.id);

  AXNodeData row_header;
  row_header.id = 4;
  row_header.role = ax::mojom::Role::kRowHeader;
  row1.child_ids.push_back(row_header.id);

  Init(root, row1, column_header, row_header);

  ComPtr<ITableProvider> root_itableprovider(
      QueryInterfaceFromNode<ITableProvider>(GetRootNode()));

  ComPtr<IRawElementProviderSimple> row_header_irawelementprovidersimple(
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[0]->children()[1]));

  SAFEARRAY* safearray = nullptr;
  EXPECT_HRESULT_SUCCEEDED(root_itableprovider->GetRowHeaders(&safearray));
  EXPECT_NE(nullptr, safearray);
  EXPECT_EQ(1U, ::SafeArrayGetDim(safearray));
  EXPECT_EQ(sizeof(IRawElementProviderSimple*),
            ::SafeArrayGetElemsize(safearray));

  LONG array_lbound;
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayGetLBound(safearray, 1, &array_lbound));
  EXPECT_EQ(0, array_lbound);

  LONG array_ubound;
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayGetUBound(safearray, 1, &array_ubound));
  EXPECT_EQ(0, array_ubound);

  LONG index = 0;
  CComPtr<IRawElementProviderSimple> array_element;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetElement(safearray, &index, &array_element));
  EXPECT_NE(nullptr, array_element);
  EXPECT_EQ(array_element, row_header_irawelementprovidersimple.Get());
}

TEST_F(AXPlatformNodeWinTest, TestITableProviderGetRowOrColumnMajor) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kTable;

  Init(root);

  ComPtr<ITableProvider> root_itableprovider(
      QueryInterfaceFromNode<ITableProvider>(GetRootNode()));

  RowOrColumnMajor row_or_column_major;
  EXPECT_HRESULT_SUCCEEDED(
      root_itableprovider->get_RowOrColumnMajor(&row_or_column_major));
  EXPECT_EQ(row_or_column_major, RowOrColumnMajor_RowMajor);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetPropertySimple) {
  AXNodeData root;
  root.SetName("fake name");
  root.AddStringAttribute(ax::mojom::StringAttribute::kAccessKey, "Ctrl+Q");
  root.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en-us");
  root.AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts, "Alt+F4");
  root.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                          "fake description");
  root.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                          "role description");
  root.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, 2);
  root.AddIntAttribute(ax::mojom::IntAttribute::kInvalidState, 1);
  root.id = 0;
  root.role = ax::mojom::Role::kList;

  AXNodeData child1;
  child1.id = 1;
  child1.role = ax::mojom::Role::kListItem;
  child1.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, 1);
  child1.SetName("child1");
  root.child_ids.push_back(1);

  Init(root, child1);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();
  ScopedVariant uia_id;
  EXPECT_UIA_BSTR_EQ(root_node, UIA_AccessKeyPropertyId, L"Ctrl+Q");
  EXPECT_UIA_BSTR_EQ(root_node, UIA_AcceleratorKeyPropertyId, L"Alt+F4");
  ASSERT_HRESULT_SUCCEEDED(root_node->GetPropertyValue(
      UIA_AutomationIdPropertyId, uia_id.Receive()));
  EXPECT_UIA_BSTR_EQ(root_node, UIA_AutomationIdPropertyId,
                     uia_id.ptr()->bstrVal);
  EXPECT_UIA_BSTR_EQ(root_node, UIA_FullDescriptionPropertyId,
                     L"fake description");
  EXPECT_UIA_BSTR_EQ(root_node, UIA_LocalizedControlTypePropertyId,
                     L"role description");
  EXPECT_UIA_BSTR_EQ(root_node, UIA_AriaRolePropertyId, L"list");
  EXPECT_UIA_BSTR_EQ(root_node, UIA_AriaPropertiesPropertyId,
                     L"readonly=true;expanded=false;multiline=false;"
                     L"multiselectable=false;required=false;setsize=2");
  EXPECT_UIA_BSTR_EQ(root_node, UIA_ClassNamePropertyId, L"fake name");
  EXPECT_UIA_BSTR_EQ(root_node, UIA_CulturePropertyId, L"en-us");
  EXPECT_UIA_BSTR_EQ(root_node, UIA_NamePropertyId, L"fake name");
  EXPECT_UIA_INT_EQ(root_node, UIA_ControlTypePropertyId,
                    int{UIA_ListControlTypeId});
  EXPECT_UIA_INT_EQ(root_node, UIA_OrientationPropertyId,
                    int{OrientationType_None});
  EXPECT_UIA_INT_EQ(root_node, UIA_SizeOfSetPropertyId, 2);
  EXPECT_UIA_INT_EQ(root_node, UIA_ToggleToggleStatePropertyId,
                    int{ToggleState_Off});
  EXPECT_UIA_BOOL_EQ(root_node, UIA_IsPasswordPropertyId, false);
  EXPECT_UIA_BOOL_EQ(root_node, UIA_IsEnabledPropertyId, true);
  EXPECT_UIA_BOOL_EQ(root_node, UIA_HasKeyboardFocusPropertyId, false);
  EXPECT_UIA_BOOL_EQ(root_node, UIA_IsRequiredForFormPropertyId, false);
  EXPECT_UIA_BOOL_EQ(root_node, UIA_IsDataValidForFormPropertyId, true);
  EXPECT_UIA_BOOL_EQ(root_node, UIA_IsKeyboardFocusablePropertyId, false);
  EXPECT_UIA_BOOL_EQ(root_node, UIA_IsOffscreenPropertyId, false);
  ComPtr<IRawElementProviderSimple> child_node1 =
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[0]);
  EXPECT_UIA_INT_EQ(child_node1, UIA_PositionInSetPropertyId, 1);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetPropertyValueClickablePoint) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kButton;
  root.relative_bounds.bounds = gfx::RectF(20, 30, 100, 200);
  Init(root);

  ComPtr<IRawElementProviderSimple> raw_element_provider_simple =
      GetRootIRawElementProviderSimple();

  // The clickable point of a rectangle {20, 30, 100, 200} is the rectangle's
  // center, with coordinates {x: 70, y: 130}.
  std::vector<double> expected_values = {70, 130};
  EXPECT_UIA_DOUBLE_ARRAY_EQ(raw_element_provider_simple,
                             UIA_ClickablePointPropertyId, expected_values);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetPropertyValue_Histogram) {
  AXNodeData root;
  root.id = 0;
  Init(root);
  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  // Log a known property ID
  {
    base::HistogramTester histogram_tester;
    ScopedVariant property_value;
    root_node->GetPropertyValue(UIA_NamePropertyId, property_value.Receive());
    histogram_tester.ExpectUniqueSample(
        "Accessibility.WinAPIs.GetPropertyValue", UIA_NamePropertyId, 1);
  }

  // Collapse unknown property IDs to zero
  {
    base::HistogramTester histogram_tester;
    ScopedVariant property_value;
    root_node->GetPropertyValue(42, property_value.Receive());
    histogram_tester.ExpectUniqueSample(
        "Accessibility.WinAPIs.GetPropertyValue", 0, 1);
  }
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetDescribedByPropertyId) {
  AXNodeData root;
  std::vector<int32_t> describedby_ids = {1, 2, 3};
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds,
                           describedby_ids);
  root.id = 0;
  root.role = ax::mojom::Role::kMarquee;
  root.SetName("root");

  AXNodeData child1;
  child1.id = 1;
  child1.role = ax::mojom::Role::kStaticText;
  child1.SetName("child1");

  root.child_ids.push_back(1);

  AXNodeData child2;
  child2.id = 2;
  child2.role = ax::mojom::Role::kStaticText;
  child2.SetName("child2");

  root.child_ids.push_back(2);

  Init(root, child1, child2);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  std::vector<std::wstring> expected_names = {L"child1", L"child2"};
  EXPECT_UIA_ELEMENT_ARRAY_BSTR_EQ(root_node, UIA_DescribedByPropertyId,
                                   UIA_NamePropertyId, expected_names);
}

TEST_F(AXPlatformNodeWinTest, TestUIAItemStatusPropertyId) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kTable;

  AXNodeData row1;
  row1.id = 2;
  row1.role = ax::mojom::Role::kRow;
  row1.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                       static_cast<int>(ax::mojom::SortDirection::kAscending));
  root.child_ids.push_back(row1.id);

  AXNodeData header1;
  header1.id = 3;
  header1.role = ax::mojom::Role::kRowHeader;
  header1.AddIntAttribute(
      ax::mojom::IntAttribute::kSortDirection,
      static_cast<int>(ax::mojom::SortDirection::kAscending));
  row1.child_ids.push_back(header1.id);

  AXNodeData header2;
  header2.id = 4;
  header2.role = ax::mojom::Role::kColumnHeader;
  header2.AddIntAttribute(
      ax::mojom::IntAttribute::kSortDirection,
      static_cast<int>(ax::mojom::SortDirection::kDescending));
  row1.child_ids.push_back(header2.id);

  AXNodeData header3;
  header3.id = 5;
  header3.role = ax::mojom::Role::kColumnHeader;
  header3.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                          static_cast<int>(ax::mojom::SortDirection::kOther));
  row1.child_ids.push_back(header3.id);

  AXNodeData header4;
  header4.id = 6;
  header4.role = ax::mojom::Role::kColumnHeader;
  header4.AddIntAttribute(
      ax::mojom::IntAttribute::kSortDirection,
      static_cast<int>(ax::mojom::SortDirection::kUnsorted));
  row1.child_ids.push_back(header4.id);

  Init(root, row1, header1, header2, header3, header4);

  auto* row_node = GetRootNode()->children()[0];

  EXPECT_UIA_BSTR_EQ(QueryInterfaceFromNode<IRawElementProviderSimple>(
                         row_node->children()[0]),
                     UIA_ItemStatusPropertyId, L"ascending");

  EXPECT_UIA_BSTR_EQ(QueryInterfaceFromNode<IRawElementProviderSimple>(
                         row_node->children()[1]),
                     UIA_ItemStatusPropertyId, L"descending");

  EXPECT_UIA_BSTR_EQ(QueryInterfaceFromNode<IRawElementProviderSimple>(
                         row_node->children()[2]),
                     UIA_ItemStatusPropertyId, L"other");

  EXPECT_UIA_VALUE_EQ(QueryInterfaceFromNode<IRawElementProviderSimple>(
                          row_node->children()[3]),
                      UIA_ItemStatusPropertyId, ScopedVariant::kEmptyVariant);

  EXPECT_UIA_VALUE_EQ(
      QueryInterfaceFromNode<IRawElementProviderSimple>(row_node),
      UIA_ItemStatusPropertyId, ScopedVariant::kEmptyVariant);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetFlowsToPropertyId) {
  AXNodeData root;
  std::vector<int32_t> flowto_ids = {1, 2, 3};
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds, flowto_ids);
  root.id = 0;
  root.role = ax::mojom::Role::kMarquee;
  root.SetName("root");

  AXNodeData child1;
  child1.id = 1;
  child1.role = ax::mojom::Role::kStaticText;
  child1.SetName("child1");

  root.child_ids.push_back(1);

  AXNodeData child2;
  child2.id = 2;
  child2.role = ax::mojom::Role::kStaticText;
  child2.SetName("child2");

  root.child_ids.push_back(2);

  Init(root, child1, child2);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();
  std::vector<std::wstring> expected_names = {L"child1", L"child2"};
  EXPECT_UIA_ELEMENT_ARRAY_BSTR_EQ(root_node, UIA_FlowsToPropertyId,
                                   UIA_NamePropertyId, expected_names);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetPropertyValueFlowsFromNone) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  root.SetName("root");

  Init(root);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();
  std::vector<std::wstring> expected_names = {};
  EXPECT_UIA_ELEMENT_ARRAY_BSTR_EQ(root_node, UIA_FlowsFromPropertyId,
                                   UIA_NamePropertyId, expected_names);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetPropertyValueFlowsFromSingle) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  root.SetName("root");
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds, {1});

  AXNodeData child1;
  child1.id = 1;
  child1.role = ax::mojom::Role::kGenericContainer;
  child1.SetName("child1");
  root.child_ids.push_back(1);

  Init(root, child1);
  ASSERT_NE(nullptr,
            TestAXNodeWrapper::GetOrCreate(tree_.get(), GetRootNode()));

  ComPtr<IRawElementProviderSimple> child_node1 =
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[0]);
  std::vector<std::wstring> expected_names = {L"root"};
  EXPECT_UIA_ELEMENT_ARRAY_BSTR_EQ(child_node1, UIA_FlowsFromPropertyId,
                                   UIA_NamePropertyId, expected_names);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetPropertyValueFlowsFromMultiple) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  root.SetName("root");
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds, {1, 2});

  AXNodeData child1;
  child1.id = 1;
  child1.role = ax::mojom::Role::kGenericContainer;
  child1.SetName("child1");
  child1.AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds, {2});
  root.child_ids.push_back(1);

  AXNodeData child2;
  child2.id = 2;
  child2.role = ax::mojom::Role::kGenericContainer;
  child2.SetName("child2");
  root.child_ids.push_back(2);

  Init(root, child1, child2);
  ASSERT_NE(nullptr,
            TestAXNodeWrapper::GetOrCreate(tree_.get(), GetRootNode()));
  ASSERT_NE(nullptr, TestAXNodeWrapper::GetOrCreate(
                         tree_.get(), GetRootNode()->children()[0]));

  ComPtr<IRawElementProviderSimple> child_node2 =
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[1]);
  std::vector<std::wstring> expected_names = {L"root", L"child1"};
  EXPECT_UIA_UNORDERED_ELEMENT_ARRAY_BSTR_EQ(
      child_node2, UIA_FlowsFromPropertyId, UIA_NamePropertyId, expected_names);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetPropertyValueFrameworkId) {
  AXNodeData root_ax_node_data;
  root_ax_node_data.id = 0;
  root_ax_node_data.role = ax::mojom::Role::kRootWebArea;
  Init(root_ax_node_data);

  ComPtr<IRawElementProviderSimple> root_raw_element_provider_simple =
      GetRootIRawElementProviderSimple();
  EXPECT_UIA_BSTR_EQ(root_raw_element_provider_simple,
                     UIA_FrameworkIdPropertyId, L"Chrome");
}

TEST_F(AXPlatformNodeWinTest, TestGetPropertyValue_LabeledByTest) {
  AXNodeData root;
  root.role = ax::mojom::Role::kListItem;
  root.id = 1;
  std::vector<int32_t> labeledby_ids = {2};
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                           labeledby_ids);

  AXNodeData referenced_node;
  referenced_node.SetName("Name");
  referenced_node.role = ax::mojom::Role::kNone;
  referenced_node.id = 2;

  root.child_ids.push_back(referenced_node.id);
  Init(root, referenced_node);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();
  ScopedVariant propertyValue;
  EXPECT_EQ(S_OK, root_node->GetPropertyValue(UIA_LabeledByPropertyId,
                                              propertyValue.Receive()));
  ASSERT_EQ(propertyValue.type(), VT_UNKNOWN);
  ComPtr<IRawElementProviderSimple> referenced_element;
  EXPECT_EQ(S_OK, propertyValue.ptr()->punkVal->QueryInterface(
                      IID_PPV_ARGS(&referenced_element)));
  EXPECT_UIA_BSTR_EQ(referenced_element, UIA_NamePropertyId, L"Name");
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetProviderOptions) {
  AXNodeData root_data;
  Init(root_data);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ProviderOptions provider_options = static_cast<ProviderOptions>(0);
  EXPECT_HRESULT_SUCCEEDED(root_node->get_ProviderOptions(&provider_options));
  EXPECT_EQ(
      ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading,
      provider_options);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetHostRawElementProvider) {
  AXNodeData root_data;
  Init(root_data);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<IRawElementProviderSimple> host_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->get_HostRawElementProvider(&host_provider));
  EXPECT_EQ(nullptr, host_provider.Get());
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetBoundingRectangle) {
  AXNodeData root_data;
  root_data.id = 0;
  root_data.relative_bounds.bounds = gfx::RectF(10, 20, 30, 50);
  Init(root_data);

  ComPtr<IRawElementProviderFragment> root_node =
      GetRootIRawElementProviderFragment();

  UiaRect bounding_rectangle;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->get_BoundingRectangle(&bounding_rectangle));
  EXPECT_EQ(10, bounding_rectangle.left);
  EXPECT_EQ(20, bounding_rectangle.top);
  EXPECT_EQ(30, bounding_rectangle.width);
  EXPECT_EQ(50, bounding_rectangle.height);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetFragmentRoot) {
  // This test needs to be run on a child node since AXPlatformRootNodeWin
  // overrides the method.
  AXNodeData root_data;
  root_data.id = 0;
  root_data.child_ids.push_back(1);

  AXNodeData element1_data;
  element1_data.id = 1;

  Init(root_data, element1_data);
  InitFragmentRoot();

  AXNode* root_node = GetRootNode();
  AXNode* element1_node = root_node->children()[0];

  ComPtr<IRawElementProviderFragment> element1_provider =
      QueryInterfaceFromNode<IRawElementProviderFragment>(element1_node);
  ComPtr<IRawElementProviderFragmentRoot> expected_fragment_root =
      GetFragmentRoot();

  ComPtr<IRawElementProviderFragmentRoot> actual_fragment_root;
  EXPECT_HRESULT_SUCCEEDED(
      element1_provider->get_FragmentRoot(&actual_fragment_root));
  EXPECT_EQ(expected_fragment_root.Get(), actual_fragment_root.Get());
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetEmbeddedFragmentRoots) {
  AXNodeData root_data;
  root_data.id = 0;
  Init(root_data);

  ComPtr<IRawElementProviderFragment> root_provider =
      GetRootIRawElementProviderFragment();

  SAFEARRAY* embedded_fragment_roots;
  EXPECT_HRESULT_SUCCEEDED(
      root_provider->GetEmbeddedFragmentRoots(&embedded_fragment_roots));
  EXPECT_EQ(nullptr, embedded_fragment_roots);
}

TEST_F(AXPlatformNodeWinTest, TestUIAGetRuntimeId) {
  AXNodeData root_data;
  root_data.id = 0;
  Init(root_data);

  ComPtr<IRawElementProviderFragment> root_provider =
      GetRootIRawElementProviderFragment();

  SAFEARRAY* runtime_id;
  EXPECT_HRESULT_SUCCEEDED(root_provider->GetRuntimeId(&runtime_id));

  LONG array_lower_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetLBound(runtime_id, 1, &array_lower_bound));
  EXPECT_EQ(0, array_lower_bound);

  LONG array_upper_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetUBound(runtime_id, 1, &array_upper_bound));
  EXPECT_EQ(1, array_upper_bound);

  int* array_data;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayAccessData(runtime_id, reinterpret_cast<void**>(&array_data)));
  EXPECT_EQ(UiaAppendRuntimeId, array_data[0]);
  EXPECT_NE(-1, array_data[1]);

  EXPECT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(runtime_id));
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayDestroy(runtime_id));
}

TEST_F(AXPlatformNodeWinTest, TestUIAIWindowProviderGetIsModalUnset) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  Init(root);

  ComPtr<IRawElementProviderSimple> raw_element_provider_simple =
      GetRootIRawElementProviderSimple();
  ComPtr<IWindowProvider> window_provider;
  EXPECT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPatternProvider(
      UIA_WindowPatternId, &window_provider));
  ASSERT_EQ(nullptr, window_provider.Get());
}

TEST_F(AXPlatformNodeWinTest, TestUIAIWindowProviderGetIsModalFalse) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, false);
  Init(root);

  ComPtr<IRawElementProviderSimple> raw_element_provider_simple =
      GetRootIRawElementProviderSimple();
  ComPtr<IWindowProvider> window_provider;
  EXPECT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPatternProvider(
      UIA_WindowPatternId, &window_provider));
  ASSERT_NE(nullptr, window_provider.Get());

  BOOL is_modal;
  EXPECT_HRESULT_SUCCEEDED(window_provider->get_IsModal(&is_modal));
  ASSERT_FALSE(is_modal);
}

TEST_F(AXPlatformNodeWinTest, TestUIAIWindowProviderGetIsModalTrue) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  Init(root);

  ComPtr<IRawElementProviderSimple> raw_element_provider_simple =
      GetRootIRawElementProviderSimple();
  ComPtr<IWindowProvider> window_provider;
  EXPECT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPatternProvider(
      UIA_WindowPatternId, &window_provider));
  ASSERT_NE(nullptr, window_provider.Get());

  BOOL is_modal;
  EXPECT_HRESULT_SUCCEEDED(window_provider->get_IsModal(&is_modal));
  ASSERT_TRUE(is_modal);
}

TEST_F(AXPlatformNodeWinTest, TestUIAIWindowProviderInvalidArgument) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  Init(root);

  ComPtr<IRawElementProviderSimple> raw_element_provider_simple =
      GetRootIRawElementProviderSimple();
  ComPtr<IWindowProvider> window_provider;
  EXPECT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPatternProvider(
      UIA_WindowPatternId, &window_provider));
  ASSERT_NE(nullptr, window_provider.Get());

  ASSERT_EQ(E_INVALIDARG, window_provider->WaitForInputIdle(0, nullptr));
  ASSERT_EQ(E_INVALIDARG, window_provider->get_CanMaximize(nullptr));
  ASSERT_EQ(E_INVALIDARG, window_provider->get_CanMinimize(nullptr));
  ASSERT_EQ(E_INVALIDARG, window_provider->get_IsModal(nullptr));
  ASSERT_EQ(E_INVALIDARG, window_provider->get_WindowVisualState(nullptr));
  ASSERT_EQ(E_INVALIDARG, window_provider->get_WindowInteractionState(nullptr));
  ASSERT_EQ(E_INVALIDARG, window_provider->get_IsTopmost(nullptr));
}

TEST_F(AXPlatformNodeWinTest, TestUIAIWindowProviderNotSupported) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  Init(root);

  ComPtr<IRawElementProviderSimple> raw_element_provider_simple =
      GetRootIRawElementProviderSimple();
  ComPtr<IWindowProvider> window_provider;
  EXPECT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPatternProvider(
      UIA_WindowPatternId, &window_provider));
  ASSERT_NE(nullptr, window_provider.Get());

  BOOL bool_result;
  WindowVisualState window_visual_state_result;
  WindowInteractionState window_interaction_state_result;

  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED),
            window_provider->SetVisualState(
                WindowVisualState::WindowVisualState_Normal));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED), window_provider->Close());
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED),
            window_provider->WaitForInputIdle(0, &bool_result));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED),
            window_provider->get_CanMaximize(&bool_result));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED),
            window_provider->get_CanMinimize(&bool_result));
  ASSERT_EQ(
      static_cast<HRESULT>(UIA_E_NOTSUPPORTED),
      window_provider->get_WindowVisualState(&window_visual_state_result));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED),
            window_provider->get_WindowInteractionState(
                &window_interaction_state_result));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_NOTSUPPORTED),
            window_provider->get_IsTopmost(&bool_result));
}

TEST_F(AXPlatformNodeWinTest, TestUIANavigate) {
  AXNodeData root_data;
  root_data.id = 0;
  root_data.child_ids.push_back(1);
  root_data.child_ids.push_back(2);

  AXNodeData element1_data;
  element1_data.id = 1;
  element1_data.child_ids.push_back(3);

  AXNodeData element2_data;
  element2_data.id = 2;

  AXNodeData element3_data;
  element3_data.id = 3;

  Init(root_data, element1_data, element2_data, element3_data);

  AXNode* root_node = GetRootNode();
  AXNode* element1_node = root_node->children()[0];
  AXNode* element2_node = root_node->children()[1];
  AXNode* element3_node = element1_node->children()[0];

  auto TestNavigate = [this](AXNode* element_node, AXNode* parent,
                             AXNode* next_sibling, AXNode* prev_sibling,
                             AXNode* first_child, AXNode* last_child) {
    ComPtr<IRawElementProviderFragment> element_provider =
        QueryInterfaceFromNode<IRawElementProviderFragment>(element_node);

    auto TestNavigateSingle = [&](NavigateDirection direction,
                                  AXNode* expected_node) {
      ComPtr<IRawElementProviderFragment> expected_provider =
          QueryInterfaceFromNode<IRawElementProviderFragment>(expected_node);

      ComPtr<IRawElementProviderFragment> navigated_to_fragment;
      EXPECT_HRESULT_SUCCEEDED(
          element_provider->Navigate(direction, &navigated_to_fragment));
      EXPECT_EQ(expected_provider.Get(), navigated_to_fragment.Get());
    };

    TestNavigateSingle(NavigateDirection_Parent, parent);
    TestNavigateSingle(NavigateDirection_NextSibling, next_sibling);
    TestNavigateSingle(NavigateDirection_PreviousSibling, prev_sibling);
    TestNavigateSingle(NavigateDirection_FirstChild, first_child);
    TestNavigateSingle(NavigateDirection_LastChild, last_child);
  };

  TestNavigate(root_node,
               nullptr,         // Parent
               nullptr,         // NextSibling
               nullptr,         // PreviousSibling
               element1_node,   // FirstChild
               element2_node);  // LastChild

  TestNavigate(element1_node, root_node, element2_node, nullptr, element3_node,
               element3_node);

  TestNavigate(element2_node, root_node, nullptr, element1_node, nullptr,
               nullptr);

  TestNavigate(element3_node, element1_node, nullptr, nullptr, nullptr,
               nullptr);
}

TEST_F(AXPlatformNodeWinTest, TestISelectionProviderCanSelectMultipleDefault) {
  Init(BuildListBox(/*option_1_is_selected*/ false,
                    /*option_2_is_selected*/ false,
                    /*option_3_is_selected*/ false));

  ComPtr<ISelectionProvider> selection_provider(
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode()));

  BOOL multiple = TRUE;
  EXPECT_HRESULT_SUCCEEDED(
      selection_provider->get_CanSelectMultiple(&multiple));
  EXPECT_FALSE(multiple);
}

TEST_F(AXPlatformNodeWinTest, TestISelectionProviderCanSelectMultipleTrue) {
  Init(BuildListBox(/*option_1_is_selected*/ false,
                    /*option_2_is_selected*/ false,
                    /*option_3_is_selected*/ false,
                    /*additional_state*/ ax::mojom::State::kMultiselectable));

  ComPtr<ISelectionProvider> selection_provider(
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode()));

  BOOL multiple = FALSE;
  EXPECT_HRESULT_SUCCEEDED(
      selection_provider->get_CanSelectMultiple(&multiple));
  EXPECT_TRUE(multiple);
}

TEST_F(AXPlatformNodeWinTest,
       TestISelectionProviderIsSelectionRequiredDefault) {
  Init(BuildListBox(/*option_1_is_selected*/ false,
                    /*option_2_is_selected*/ false,
                    /*option_3_is_selected*/ false));

  ComPtr<ISelectionProvider> selection_provider(
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode()));

  BOOL selection_required = TRUE;
  EXPECT_HRESULT_SUCCEEDED(
      selection_provider->get_IsSelectionRequired(&selection_required));
  EXPECT_FALSE(selection_required);
}

TEST_F(AXPlatformNodeWinTest, TestISelectionProviderIsSelectionRequiredTrue) {
  Init(BuildListBox(/*option_1_is_selected*/ false,
                    /*option_2_is_selected*/ false,
                    /*option_3_is_selected*/ false,
                    /*additional_state*/ ax::mojom::State::kRequired));

  ComPtr<ISelectionProvider> selection_provider(
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode()));

  BOOL selection_required = FALSE;
  EXPECT_HRESULT_SUCCEEDED(
      selection_provider->get_IsSelectionRequired(&selection_required));
  EXPECT_TRUE(selection_required);
}

TEST_F(AXPlatformNodeWinTest, TestISelectionProviderGetSelectionNoneSelected) {
  Init(BuildListBox(/*option_1_is_selected*/ false,
                    /*option_2_is_selected*/ false,
                    /*option_3_is_selected*/ false));

  ComPtr<ISelectionProvider> selection_provider(
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode()));

  SAFEARRAY* selected_items;
  EXPECT_HRESULT_SUCCEEDED(selection_provider->GetSelection(&selected_items));
  EXPECT_NE(nullptr, selected_items);

  LONG array_lower_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetLBound(selected_items, 1, &array_lower_bound));
  EXPECT_EQ(0, array_lower_bound);

  LONG array_upper_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetUBound(selected_items, 1, &array_upper_bound));
  EXPECT_EQ(-1, array_upper_bound);

  EXPECT_HRESULT_SUCCEEDED(::SafeArrayDestroy(selected_items));
}

TEST_F(AXPlatformNodeWinTest,
       TestISelectionProviderGetSelectionSingleItemSelected) {
  Init(BuildListBox(/*option_1_is_selected*/ false,
                    /*option_2_is_selected*/ true,
                    /*option_3_is_selected*/ false));

  ComPtr<ISelectionProvider> selection_provider(
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode()));
  ComPtr<IRawElementProviderSimple> option2_provider(
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[1]));

  SAFEARRAY* selected_items;
  EXPECT_HRESULT_SUCCEEDED(selection_provider->GetSelection(&selected_items));
  EXPECT_NE(nullptr, selected_items);

  LONG array_lower_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetLBound(selected_items, 1, &array_lower_bound));
  EXPECT_EQ(0, array_lower_bound);

  LONG array_upper_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetUBound(selected_items, 1, &array_upper_bound));
  EXPECT_EQ(0, array_upper_bound);

  IRawElementProviderSimple** array_data;
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
      selected_items, reinterpret_cast<void**>(&array_data)));
  EXPECT_EQ(option2_provider.Get(), array_data[0]);

  EXPECT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(selected_items));
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayDestroy(selected_items));
}

TEST_F(AXPlatformNodeWinTest,
       TestISelectionProviderGetSelectionMultipleItemsSelected) {
  Init(BuildListBox(/*option_1_is_selected*/ true,
                    /*option_2_is_selected*/ true,
                    /*option_3_is_selected*/ true));

  ComPtr<ISelectionProvider> selection_provider(
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode()));
  ComPtr<IRawElementProviderSimple> option1_provider(
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[0]));
  ComPtr<IRawElementProviderSimple> option2_provider(
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[1]));
  ComPtr<IRawElementProviderSimple> option3_provider(
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          GetRootNode()->children()[2]));

  SAFEARRAY* selected_items;
  EXPECT_HRESULT_SUCCEEDED(selection_provider->GetSelection(&selected_items));
  EXPECT_NE(nullptr, selected_items);

  LONG array_lower_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetLBound(selected_items, 1, &array_lower_bound));
  EXPECT_EQ(0, array_lower_bound);

  LONG array_upper_bound;
  EXPECT_HRESULT_SUCCEEDED(
      ::SafeArrayGetUBound(selected_items, 1, &array_upper_bound));
  EXPECT_EQ(2, array_upper_bound);

  IRawElementProviderSimple** array_data;
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
      selected_items, reinterpret_cast<void**>(&array_data)));
  EXPECT_EQ(option1_provider.Get(), array_data[0]);
  EXPECT_EQ(option2_provider.Get(), array_data[1]);
  EXPECT_EQ(option3_provider.Get(), array_data[2]);

  EXPECT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(selected_items));
  EXPECT_HRESULT_SUCCEEDED(::SafeArrayDestroy(selected_items));
}

TEST_F(AXPlatformNodeWinTest, TestComputeUIAControlType) {
  AXNodeData root;
  root.id = 0;
  root.role = ax::mojom::Role::kRootWebArea;

  AXNodeData child1;
  int32_t child1_id = 1;
  child1.id = child1_id;
  child1.role = ax::mojom::Role::kTable;
  root.child_ids.push_back(child1_id);

  AXNodeData child2;
  int32_t child2_id = 2;
  child2.id = child2_id;
  child2.role = ax::mojom::Role::kLayoutTable;
  root.child_ids.push_back(child2_id);

  Init(root, child1, child2);

  EXPECT_UIA_INT_EQ(
      QueryInterfaceFromNodeId<IRawElementProviderSimple>(child1_id),
      UIA_ControlTypePropertyId, int{UIA_TableControlTypeId});
  EXPECT_UIA_INT_EQ(
      QueryInterfaceFromNodeId<IRawElementProviderSimple>(child2_id),
      UIA_ControlTypePropertyId, int{UIA_TableControlTypeId});
}

TEST_F(AXPlatformNodeWinTest, TestIRawElementProviderSimple2ShowContextMenu) {
  AXNodeData root_data;
  root_data.id = 0;

  AXNodeData element1_data;
  element1_data.id = 1;
  root_data.child_ids.push_back(1);

  AXNodeData element2_data;
  element2_data.id = 2;
  root_data.child_ids.push_back(2);

  Init(root_data, element1_data, element2_data);

  AXNode* root_node = GetRootNode();
  AXNode* element1_node = root_node->children()[0];
  AXNode* element2_node = root_node->children()[1];

  ComPtr<IRawElementProviderSimple2> root_provider =
      QueryInterfaceFromNode<IRawElementProviderSimple2>(root_node);
  ComPtr<IRawElementProviderSimple2> element1_provider =
      QueryInterfaceFromNode<IRawElementProviderSimple2>(element1_node);
  ComPtr<IRawElementProviderSimple2> element2_provider =
      QueryInterfaceFromNode<IRawElementProviderSimple2>(element2_node);

  EXPECT_HRESULT_SUCCEEDED(element1_provider->ShowContextMenu());
  EXPECT_EQ(element1_node, TestAXNodeWrapper::GetNodeFromLastShowContextMenu());
  EXPECT_HRESULT_SUCCEEDED(element2_provider->ShowContextMenu());
  EXPECT_EQ(element2_node, TestAXNodeWrapper::GetNodeFromLastShowContextMenu());
  EXPECT_HRESULT_SUCCEEDED(root_provider->ShowContextMenu());
  EXPECT_EQ(root_node, TestAXNodeWrapper::GetNodeFromLastShowContextMenu());
}

TEST_F(AXPlatformNodeWinTest, TestUIAErrorHandling) {
  AXNodeData root;
  Init(root);

  ComPtr<IRawElementProviderSimple> simple_provider =
      GetRootIRawElementProviderSimple();
  ComPtr<IRawElementProviderSimple2> simple2_provider =
      QueryInterfaceFromNode<IRawElementProviderSimple2>(GetRootNode());
  ComPtr<IRawElementProviderFragment> fragment_provider =
      GetRootIRawElementProviderFragment();
  ComPtr<IGridItemProvider> grid_item_provider =
      QueryInterfaceFromNode<IGridItemProvider>(GetRootNode());
  ComPtr<IGridProvider> grid_provider =
      QueryInterfaceFromNode<IGridProvider>(GetRootNode());
  ComPtr<IScrollItemProvider> scroll_item_provider =
      QueryInterfaceFromNode<IScrollItemProvider>(GetRootNode());
  ComPtr<IScrollProvider> scroll_provider =
      QueryInterfaceFromNode<IScrollProvider>(GetRootNode());
  ComPtr<ISelectionItemProvider> selection_item_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(GetRootNode());
  ComPtr<ISelectionProvider> selection_provider =
      QueryInterfaceFromNode<ISelectionProvider>(GetRootNode());
  ComPtr<ITableItemProvider> table_item_provider =
      QueryInterfaceFromNode<ITableItemProvider>(GetRootNode());
  ComPtr<ITableProvider> table_provider =
      QueryInterfaceFromNode<ITableProvider>(GetRootNode());
  ComPtr<IExpandCollapseProvider> expand_collapse_provider =
      QueryInterfaceFromNode<IExpandCollapseProvider>(GetRootNode());
  ComPtr<IToggleProvider> toggle_provider =
      QueryInterfaceFromNode<IToggleProvider>(GetRootNode());
  ComPtr<IValueProvider> value_provider =
      QueryInterfaceFromNode<IValueProvider>(GetRootNode());
  ComPtr<IRangeValueProvider> range_value_provider =
      QueryInterfaceFromNode<IRangeValueProvider>(GetRootNode());
  ComPtr<IWindowProvider> window_provider =
      QueryInterfaceFromNode<IWindowProvider>(GetRootNode());

  tree_.reset(new AXTree());

  // IGridItemProvider
  int int_result = 0;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            grid_item_provider->get_Column(&int_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            grid_item_provider->get_ColumnSpan(&int_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            grid_item_provider->get_Row(&int_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            grid_item_provider->get_RowSpan(&int_result));

  // IExpandCollapseProvider
  ExpandCollapseState expand_collapse_state;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            expand_collapse_provider->Collapse());
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            expand_collapse_provider->Expand());
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            expand_collapse_provider->get_ExpandCollapseState(
                &expand_collapse_state));

  // IGridProvider
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            grid_provider->GetItem(0, 0, simple_provider.GetAddressOf()));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            grid_provider->get_RowCount(&int_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            grid_provider->get_ColumnCount(&int_result));

  // IScrollItemProvider
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_item_provider->ScrollIntoView());

  // IScrollProvider
  BOOL bool_result = TRUE;
  double double_result = 3.14;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_provider->SetScrollPercent(0, 0));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_provider->get_HorizontallyScrollable(&bool_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_provider->get_HorizontalScrollPercent(&double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_provider->get_HorizontalViewSize(&double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_provider->get_VerticallyScrollable(&bool_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_provider->get_VerticalScrollPercent(&double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            scroll_provider->get_VerticalViewSize(&double_result));

  // ISelectionItemProvider
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_item_provider->AddToSelection());
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_item_provider->RemoveFromSelection());
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_item_provider->Select());
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_item_provider->get_IsSelected(&bool_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_item_provider->get_SelectionContainer(
                simple_provider.GetAddressOf()));

  // ISelectionProvider
  SAFEARRAY* array_result = nullptr;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_provider->GetSelection(&array_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_provider->get_CanSelectMultiple(&bool_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            selection_provider->get_IsSelectionRequired(&bool_result));

  // ITableItemProvider
  RowOrColumnMajor row_or_column_major_result;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            table_item_provider->GetColumnHeaderItems(&array_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            table_item_provider->GetRowHeaderItems(&array_result));

  // ITableProvider
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            table_provider->GetColumnHeaders(&array_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            table_provider->GetRowHeaders(&array_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            table_provider->get_RowOrColumnMajor(&row_or_column_major_result));

  // IRawElementProviderSimple
  ScopedVariant variant;
  ComPtr<IUnknown> unknown;
  ComPtr<IRawElementProviderSimple> host_provider;
  ProviderOptions options;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            simple_provider->GetPatternProvider(UIA_WindowPatternId,
                                                unknown.GetAddressOf()));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            simple_provider->GetPropertyValue(UIA_FrameworkIdPropertyId,
                                              variant.Receive()));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            simple_provider->get_ProviderOptions(&options));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            simple_provider->get_HostRawElementProvider(&host_provider));

  // IRawElementProviderSimple2
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            simple2_provider->ShowContextMenu());

  // IRawElementProviderFragment
  ComPtr<IRawElementProviderFragment> navigated_to_fragment;
  SAFEARRAY* safearray = nullptr;
  UiaRect bounding_rectangle;
  ComPtr<IRawElementProviderFragmentRoot> fragment_root;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_provider->Navigate(NavigateDirection_Parent,
                                        &navigated_to_fragment));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_provider->GetRuntimeId(&safearray));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_provider->get_BoundingRectangle(&bounding_rectangle));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_provider->GetEmbeddedFragmentRoots(&safearray));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_provider->get_FragmentRoot(&fragment_root));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_provider->SetFocus());

  // IValueProvider
  ScopedBstr bstr_value;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            value_provider->SetValue(L"3.14"));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            value_provider->get_Value(bstr_value.Receive()));

  // IRangeValueProvider
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            range_value_provider->SetValue(double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            range_value_provider->get_LargeChange(&double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            range_value_provider->get_Maximum(&double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            range_value_provider->get_Minimum(&double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            range_value_provider->get_SmallChange(&double_result));
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            range_value_provider->get_Value(&double_result));

  // IToggleProvider
  ToggleState toggle_state;
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            toggle_provider->Toggle());
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            toggle_provider->get_ToggleState(&toggle_state));

  // IWindowProvider
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->SetVisualState(
                WindowVisualState::WindowVisualState_Normal));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->Close());
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->WaitForInputIdle(0, nullptr));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->get_CanMaximize(nullptr));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->get_CanMinimize(nullptr));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->get_IsModal(nullptr));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->get_WindowVisualState(nullptr));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->get_WindowInteractionState(nullptr));
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            window_provider->get_IsTopmost(nullptr));
}

TEST_F(AXPlatformNodeWinTest, TestGetPatternProviderSupportedPatterns) {
  ui::AXNodeData root;
  int32_t root_id = 0;
  root.id = root_id;
  root.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData text_field_with_combo_box;
  int32_t text_field_with_combo_box_id = 1;
  text_field_with_combo_box.id = text_field_with_combo_box_id;
  text_field_with_combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  root.child_ids.push_back(text_field_with_combo_box_id);

  ui::AXNodeData table;
  int32_t table_id = 2;
  table.id = table_id;
  table.role = ax::mojom::Role::kTable;
  root.child_ids.push_back(table_id);

  ui::AXNodeData table_cell;
  int32_t table_cell_id = 3;
  table_cell.id = table_cell_id;
  table_cell.role = ax::mojom::Role::kCell;
  table.child_ids.push_back(table_cell_id);

  ui::AXNodeData meter;
  int32_t meter_id = 4;
  meter.id = meter_id;
  meter.role = ax::mojom::Role::kMeter;
  root.child_ids.push_back(meter_id);

  ui::AXNodeData group_with_scroll;
  int32_t group_with_scroll_id = 5;
  group_with_scroll.id = group_with_scroll_id;
  group_with_scroll.role = ax::mojom::Role::kGroup;
  group_with_scroll.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin, 10);
  group_with_scroll.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMax, 10);
  group_with_scroll.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 10);
  root.child_ids.push_back(group_with_scroll_id);

  ui::AXNodeData grid;
  int32_t grid_id = 6;
  grid.id = grid_id;
  grid.role = ax::mojom::Role::kGrid;
  root.child_ids.push_back(grid_id);

  ui::AXNodeData checkbox;
  int32_t checkbox_id = 7;
  checkbox.id = checkbox_id;
  checkbox.role = ax::mojom::Role::kCheckBox;
  root.child_ids.push_back(checkbox_id);

  Init(root, text_field_with_combo_box, table, table_cell, meter,
       group_with_scroll, grid, checkbox);

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_TextEditPatternId, UIA_TextPatternId}),
            GetSupportedPatternsFromNodeId(root_id));

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_ExpandCollapsePatternId}),
            GetSupportedPatternsFromNodeId(text_field_with_combo_box_id));

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_GridPatternId, UIA_TablePatternId}),
            GetSupportedPatternsFromNodeId(table_id));

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_GridItemPatternId, UIA_TableItemPatternId}),
            GetSupportedPatternsFromNodeId(table_cell_id));

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_RangeValuePatternId}),
            GetSupportedPatternsFromNodeId(meter_id));

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_ScrollPatternId}),
            GetSupportedPatternsFromNodeId(group_with_scroll_id));

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_SelectionPatternId, UIA_GridPatternId,
                        UIA_TablePatternId}),
            GetSupportedPatternsFromNodeId(grid_id));

  EXPECT_EQ(PatternSet({UIA_ScrollItemPatternId, UIA_ValuePatternId,
                        UIA_TogglePatternId}),
            GetSupportedPatternsFromNodeId(checkbox_id));
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderNotSupported) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kNone;

  Init(root);

  ComPtr<ISelectionItemProvider> provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(GetRootNode());

  BOOL selected;

  EXPECT_UIA_INVALIDOPERATION(provider->AddToSelection());
  EXPECT_UIA_INVALIDOPERATION(provider->RemoveFromSelection());
  EXPECT_UIA_INVALIDOPERATION(provider->Select());
  EXPECT_UIA_INVALIDOPERATION(provider->get_IsSelected(&selected));
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderDisabled) {
  AXNodeData root;
  root.id = 1;
  root.AddIntAttribute(ax::mojom::IntAttribute::kRestriction,
                       static_cast<int>(ax::mojom::Restriction::kDisabled));
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  root.role = ax::mojom::Role::kTab;

  Init(root);

  ComPtr<ISelectionItemProvider> provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(GetRootNode());

  BOOL selected;

  EXPECT_UIA_ELEMENTNOTENABLED(provider->AddToSelection());
  EXPECT_UIA_ELEMENTNOTENABLED(provider->RemoveFromSelection());
  EXPECT_UIA_ELEMENTNOTENABLED(provider->Select());
  EXPECT_HRESULT_SUCCEEDED(provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderNotSelectable) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kTab;

  Init(root);

  ComPtr<ISelectionItemProvider> item_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(GetRootNode());

  BOOL selected;

  EXPECT_UIA_INVALIDOPERATION(item_provider->AddToSelection());
  EXPECT_UIA_INVALIDOPERATION(item_provider->RemoveFromSelection());
  EXPECT_UIA_INVALIDOPERATION(item_provider->Select());
  EXPECT_UIA_INVALIDOPERATION(item_provider->get_IsSelected(&selected));
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderSimple) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kListBox;

  AXNodeData option1;
  option1.id = 2;
  option1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  option1.role = ax::mojom::Role::kListBoxOption;
  root.child_ids.push_back(option1.id);

  Init(root, option1);

  const auto* root_node = GetRootNode();
  ComPtr<ISelectionItemProvider> option1_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(root_node->children()[0]);

  BOOL selected;

  // Note: TestAXNodeWrapper::AccessibilityPerformAction will
  // flip kSelected for kListBoxOption when the kDoDefault action is fired.

  // Initial State
  EXPECT_HRESULT_SUCCEEDED(option1_provider->get_IsSelected(&selected));
  EXPECT_FALSE(selected);

  // AddToSelection should fire event when not selected
  EXPECT_HRESULT_SUCCEEDED(option1_provider->AddToSelection());
  EXPECT_HRESULT_SUCCEEDED(option1_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);

  // AddToSelection should not fire event when selected
  EXPECT_HRESULT_SUCCEEDED(option1_provider->AddToSelection());
  EXPECT_HRESULT_SUCCEEDED(option1_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);

  // RemoveFromSelection should fire event when selected
  EXPECT_HRESULT_SUCCEEDED(option1_provider->RemoveFromSelection());
  EXPECT_HRESULT_SUCCEEDED(option1_provider->get_IsSelected(&selected));
  EXPECT_FALSE(selected);

  // RemoveFromSelection should not fire event when not selected
  EXPECT_HRESULT_SUCCEEDED(option1_provider->RemoveFromSelection());
  EXPECT_HRESULT_SUCCEEDED(option1_provider->get_IsSelected(&selected));
  EXPECT_FALSE(selected);

  // Select should fire event when not selected
  EXPECT_HRESULT_SUCCEEDED(option1_provider->Select());
  EXPECT_HRESULT_SUCCEEDED(option1_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);

  // Select should not fire event when selected
  EXPECT_HRESULT_SUCCEEDED(option1_provider->Select());
  EXPECT_HRESULT_SUCCEEDED(option1_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderRadioButtonIsSelected) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRadioGroup;

  // CheckedState::kNone
  AXNodeData option1;
  option1.id = 2;
  option1.AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                          static_cast<int>(ax::mojom::CheckedState::kNone));
  option1.role = ax::mojom::Role::kRadioButton;
  root.child_ids.push_back(option1.id);

  // CheckedState::kFalse
  AXNodeData option2;
  option2.id = 3;
  option2.AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                          static_cast<int>(ax::mojom::CheckedState::kFalse));
  option2.role = ax::mojom::Role::kRadioButton;
  root.child_ids.push_back(option2.id);

  // CheckedState::kTrue
  AXNodeData option3;
  option3.id = 4;
  option3.AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                          static_cast<int>(ax::mojom::CheckedState::kTrue));
  option3.role = ax::mojom::Role::kRadioButton;
  root.child_ids.push_back(option3.id);

  // CheckedState::kMixed
  AXNodeData option4;
  option4.id = 5;
  option4.AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                          static_cast<int>(ax::mojom::CheckedState::kMixed));
  option4.role = ax::mojom::Role::kRadioButton;
  root.child_ids.push_back(option4.id);

  Init(root, option1, option2, option3, option4);

  const auto* root_node = GetRootNode();
  // CheckedState::kNone
  ComPtr<ISelectionItemProvider> option1_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(root_node->children()[0]);
  // CheckedState::kFalse
  ComPtr<ISelectionItemProvider> option2_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(root_node->children()[1]);
  // CheckedState::kTrue
  ComPtr<ISelectionItemProvider> option3_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(root_node->children()[2]);
  // CheckedState::kMixed
  ComPtr<ISelectionItemProvider> option4_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(root_node->children()[3]);

  BOOL selected;

  EXPECT_UIA_INVALIDOPERATION(option1_provider->get_IsSelected(&selected));

  EXPECT_HRESULT_SUCCEEDED(option2_provider->get_IsSelected(&selected));
  EXPECT_FALSE(selected);

  EXPECT_HRESULT_SUCCEEDED(option3_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);

  EXPECT_UIA_INVALIDOPERATION(option4_provider->get_IsSelected(&selected));
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderTable) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kTable;

  AXNodeData row1;
  row1.id = 2;
  row1.role = ax::mojom::Role::kRow;
  root.child_ids.push_back(row1.id);

  AXNodeData cell1;
  cell1.id = 3;
  cell1.role = ax::mojom::Role::kCell;
  cell1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  row1.child_ids.push_back(cell1.id);

  Init(root, row1, cell1);

  const auto* row = GetRootNode()->children()[0];
  ComPtr<ISelectionItemProvider> item_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(row->children()[0]);

  BOOL selected;

  EXPECT_UIA_INVALIDOPERATION(item_provider->AddToSelection());
  EXPECT_UIA_INVALIDOPERATION(item_provider->RemoveFromSelection());
  EXPECT_UIA_INVALIDOPERATION(item_provider->Select());
  EXPECT_UIA_INVALIDOPERATION(item_provider->get_IsSelected(&selected));
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderGrid) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kGrid;

  AXNodeData row1;
  row1.id = 2;
  row1.role = ax::mojom::Role::kRow;
  root.child_ids.push_back(row1.id);

  AXNodeData cell1;
  cell1.id = 3;
  cell1.role = ax::mojom::Role::kCell;
  cell1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  row1.child_ids.push_back(cell1.id);

  Init(root, row1, cell1);

  const auto* row = GetRootNode()->children()[0];
  ComPtr<ISelectionItemProvider> item_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(row->children()[0]);

  BOOL selected;

  // Note: TestAXNodeWrapper::AccessibilityPerformAction will
  // flip kSelected for kCell when the kDoDefault action is fired.

  // Initial State
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_IsSelected(&selected));
  EXPECT_FALSE(selected);

  // AddToSelection should fire event when not selected
  EXPECT_HRESULT_SUCCEEDED(item_provider->AddToSelection());
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);

  // AddToSelection should not fire event when selected
  EXPECT_HRESULT_SUCCEEDED(item_provider->AddToSelection());
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);

  // RemoveFromSelection should fire event when selected
  EXPECT_HRESULT_SUCCEEDED(item_provider->RemoveFromSelection());
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_IsSelected(&selected));
  EXPECT_FALSE(selected);

  // RemoveFromSelection should not fire event when not selected
  EXPECT_HRESULT_SUCCEEDED(item_provider->RemoveFromSelection());
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_IsSelected(&selected));
  EXPECT_FALSE(selected);

  // Select should fire event when not selected
  EXPECT_HRESULT_SUCCEEDED(item_provider->Select());
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);

  // Select should not fire event when selected
  EXPECT_HRESULT_SUCCEEDED(item_provider->Select());
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_IsSelected(&selected));
  EXPECT_TRUE(selected);
}

TEST_F(AXPlatformNodeWinTest, TestISelectionItemProviderGetSelectionContainer) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kGrid;

  AXNodeData row1;
  row1.id = 2;
  row1.role = ax::mojom::Role::kRow;
  root.child_ids.push_back(row1.id);

  AXNodeData cell1;
  cell1.id = 3;
  cell1.role = ax::mojom::Role::kCell;
  cell1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  row1.child_ids.push_back(cell1.id);

  Init(root, row1, cell1);

  ComPtr<IRawElementProviderSimple> container_provider =
      GetRootIRawElementProviderSimple();

  const auto* row = GetRootNode()->children()[0];
  ComPtr<ISelectionItemProvider> item_provider =
      QueryInterfaceFromNode<ISelectionItemProvider>(row->children()[0]);

  CComPtr<IRawElementProviderSimple> container;
  EXPECT_HRESULT_SUCCEEDED(item_provider->get_SelectionContainer(&container));
  EXPECT_NE(nullptr, container);
  EXPECT_EQ(container, container_provider.Get());
}

}  // namespace ui
