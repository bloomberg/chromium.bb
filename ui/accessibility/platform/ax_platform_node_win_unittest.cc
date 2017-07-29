// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node.h"

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include <memory>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/base/win/atl_module.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace ui {

namespace {

// Most IAccessible functions require a VARIANT set to CHILDID_SELF as
// the first argument.
ScopedVariant SELF(CHILDID_SELF);

}  // namespace

class AXPlatformNodeWinTest : public testing::Test {
 public:
  AXPlatformNodeWinTest() {}
  ~AXPlatformNodeWinTest() override {}

  void SetUp() override {
    win::CreateATLModuleIfNeeded();
  }

  // Initialize given an AXTreeUpdate.
  void Init(const AXTreeUpdate& initial_state) {
    tree_.reset(new AXTree(initial_state));
  }

  // Convenience functions to initialize directly from a few AXNodeData objects.
  void Init(const AXNodeData& node1) {
    AXTreeUpdate update;
    update.root_id = node1.id;
    update.nodes.push_back(node1);
    Init(update);
  }

  void Init(const AXNodeData& node1,
            const AXNodeData& node2) {
    AXTreeUpdate update;
    update.root_id = node1.id;
    update.nodes.push_back(node1);
    update.nodes.push_back(node2);
    Init(update);
  }

  void Init(const AXNodeData& node1,
            const AXNodeData& node2,
            const AXNodeData& node3) {
    AXTreeUpdate update;
    update.root_id = node1.id;
    update.nodes.push_back(node1);
    update.nodes.push_back(node2);
    update.nodes.push_back(node3);
    Init(update);
  }
  void Init(const AXNodeData& node1,
            const AXNodeData& node2,
            const AXNodeData& node3,
            const AXNodeData& node4) {
    AXTreeUpdate update;
    update.root_id = node1.id;
    update.nodes.push_back(node1);
    update.nodes.push_back(node2);
    update.nodes.push_back(node3);
    update.nodes.push_back(node4);
    Init(update);
  }

 protected:
  AXNode* GetRootNode() {
    return tree_->root();
  }

  void BuildRelationships(ScopedComPtr<IAccessible2> accessible) {
    CHECK(accessible);
    AXPlatformNodeWin* node = static_cast<AXPlatformNodeWin*>(accessible.Get());
    node->CalculateRelationships();
  }

  ScopedComPtr<IAccessible> IAccessibleFromNode(AXNode* node) {
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(tree_.get(), node);
    if (!wrapper)
      return ScopedComPtr<IAccessible>();
    AXPlatformNode* ax_platform_node = wrapper->ax_platform_node();
    IAccessible* iaccessible = ax_platform_node->GetNativeViewAccessible();
    return ScopedComPtr<IAccessible>(iaccessible);
  }

  ScopedComPtr<IAccessible> GetRootIAccessible() {
    return IAccessibleFromNode(GetRootNode());
  }

  ScopedComPtr<IAccessible2> ToIAccessible2(ScopedComPtr<IUnknown> unknown) {
    CHECK(unknown);
    ScopedComPtr<IServiceProvider> service_provider;
    unknown.CopyTo(service_provider.GetAddressOf());
    ScopedComPtr<IAccessible2> result;
    CHECK(SUCCEEDED(service_provider->QueryService(IID_IAccessible2,
                                                   result.GetAddressOf())));
    return result;
  }

  ScopedComPtr<IAccessible2> ToIAccessible2(
      ScopedComPtr<IAccessible> accessible) {
    CHECK(accessible);
    ScopedComPtr<IServiceProvider> service_provider;
    accessible.CopyTo(service_provider.GetAddressOf());
    ScopedComPtr<IAccessible2> result;
    CHECK(SUCCEEDED(service_provider->QueryService(IID_IAccessible2,
                                                   result.GetAddressOf())));
    return result;
  }

  void CheckVariantHasName(ScopedVariant& variant,
                           const wchar_t* expected_name) {
    ScopedComPtr<IAccessible> accessible;
    HRESULT hr =
        V_DISPATCH(variant.ptr())->QueryInterface(IID_PPV_ARGS(&accessible));
    EXPECT_EQ(S_OK, hr);
    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(expected_name, name);
  }

  void CheckIUnknownHasName(ScopedComPtr<IUnknown> unknown,
                            const wchar_t* expected_name) {
    ScopedComPtr<IAccessible2> accessible = ToIAccessible2(unknown);
    ASSERT_NE(nullptr, accessible.Get());

    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(expected_name, name);
  }

  AXTreeUpdate Build3X3Table() {
    /*
      Build a table the looks like:

      ----------------------        (A) Column Header
      |        | (A) | (B) |        (B) Column Header
      ----------------------        (C) Row Header
      |  (C)  |  1  |  2   |        (D) Row Header
      ----------------------
      |  (D)  |  3  |  4   |
      ----------------------
   */

    AXNodeData table;
    table.id = 0;
    table.role = ui::AX_ROLE_TABLE;

    table.AddIntAttribute(AX_ATTR_TABLE_ROW_COUNT, 3);
    table.AddIntAttribute(AX_ATTR_TABLE_COLUMN_COUNT, 3);

    // Ordering in this list matters.  It is used in the calculation
    // of where cells are by the following:
    // int position = row * GetTableColumnCount() + column;

    std::vector<int32_t> ids{51, 52, 53, 2, 3, 4, 11, 12, 13};
    table.AddIntListAttribute(AX_ATTR_CELL_IDS, ids);
    table.AddIntListAttribute(AX_ATTR_UNIQUE_CELL_IDS, ids);

    table.child_ids.push_back(50);  // Header
    table.child_ids.push_back(1);   // Row 1
    table.child_ids.push_back(10);  // Row 2

    // Table column header
    AXNodeData table_row_header;
    table_row_header.id = 50;
    table_row_header.role = ui::AX_ROLE_ROW;
    table_row_header.child_ids.push_back(51);
    table_row_header.child_ids.push_back(52);
    table_row_header.child_ids.push_back(53);

    AXNodeData table_column_header_1;
    table_column_header_1.id = 51;
    table_column_header_1.role = ui::AX_ROLE_COLUMN_HEADER;

    AXNodeData table_column_header_2;
    table_column_header_2.id = 52;
    table_column_header_2.role = ui::AX_ROLE_COLUMN_HEADER;
    table_column_header_2.AddStringAttribute(AX_ATTR_NAME, "column header 1");

    AXNodeData table_column_header_3;
    table_column_header_3.id = 53;
    table_column_header_3.role = ui::AX_ROLE_COLUMN_HEADER;
    // Either AX_ATTR_NAME -or- AX_ATTR_DESCRIPTION is acceptable for a
    // description
    table_column_header_3.AddStringAttribute(AX_ATTR_DESCRIPTION,
                                             "column header 2");

    // Row 1
    AXNodeData table_row_1;
    table_row_1.id = 1;
    table_row_1.role = ui::AX_ROLE_ROW;
    table_row_1.child_ids.push_back(2);
    table_row_1.child_ids.push_back(3);
    table_row_1.child_ids.push_back(4);

    AXNodeData table_row_header_1;
    table_row_header_1.id = 2;
    table_row_header_1.role = ui::AX_ROLE_ROW_HEADER;
    table_row_header_1.AddStringAttribute(AX_ATTR_NAME, "row header 1");

    AXNodeData table_cell_1;
    table_cell_1.id = 3;
    table_cell_1.role = ui::AX_ROLE_CELL;
    table_cell_1.AddStringAttribute(AX_ATTR_NAME, "1");

    AXNodeData table_cell_2;
    table_cell_2.id = 4;
    table_cell_2.role = ui::AX_ROLE_CELL;
    table_cell_2.AddStringAttribute(AX_ATTR_NAME, "2");

    // Row 2
    AXNodeData table_row_2;
    table_row_2.id = 10;
    table_row_2.role = ui::AX_ROLE_ROW;
    table_row_2.child_ids.push_back(11);
    table_row_2.child_ids.push_back(12);
    table_row_2.child_ids.push_back(13);

    AXNodeData table_row_header_2;
    table_row_header_2.id = 11;
    table_row_header_2.role = ui::AX_ROLE_ROW_HEADER;
    // Either AX_ATTR_NAME -or- AX_ATTR_DESCRIPTION is acceptable for a
    // description
    table_row_header_2.AddStringAttribute(AX_ATTR_DESCRIPTION, "row header 2");

    AXNodeData table_cell_3;
    table_cell_3.id = 12;
    table_cell_3.role = ui::AX_ROLE_CELL;
    table_cell_3.AddStringAttribute(AX_ATTR_NAME, "3");

    AXNodeData table_cell_4;
    table_cell_4.id = 13;
    table_cell_4.role = ui::AX_ROLE_CELL;
    table_cell_4.AddStringAttribute(AX_ATTR_NAME, "4");

    AXTreeUpdate update;
    update.root_id = table.id;

    // Some of the table testing code will index into |nodes|
    // and change the state of the given node.  If you reorder
    // these, you're going to need to update the tests.
    update.nodes.push_back(table);  // 0

    update.nodes.push_back(table_row_header);       // 1
    update.nodes.push_back(table_column_header_1);  // 2
    update.nodes.push_back(table_column_header_2);  // 3
    update.nodes.push_back(table_column_header_3);  // 4

    update.nodes.push_back(table_row_1);         // 5
    update.nodes.push_back(table_row_header_1);  // 6
    update.nodes.push_back(table_cell_1);        // 7
    update.nodes.push_back(table_cell_2);        // 8

    update.nodes.push_back(table_row_2);         // 9
    update.nodes.push_back(table_row_header_2);  // 10
    update.nodes.push_back(table_cell_3);        // 11
    update.nodes.push_back(table_cell_4);        // 12

    return update;
  }

  ScopedComPtr<IAccessibleTableCell> GetCellInTable() {
    ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

    ScopedComPtr<IAccessibleTable2> table;
    root_obj.CopyTo(table.GetAddressOf());
    if (!table)
      return ScopedComPtr<IAccessibleTableCell>();

    ScopedComPtr<IUnknown> cell;
    table->get_cellAt(1, 1, cell.GetAddressOf());
    if (!cell)
      return ScopedComPtr<IAccessibleTableCell>();

    ScopedComPtr<IAccessibleTableCell> table_cell;
    cell.CopyTo(table_cell.GetAddressOf());
    return table_cell;
  }

  std::unique_ptr<AXTree> tree_;
};

TEST_F(AXPlatformNodeWinTest, TestIAccessibleDetachedObject) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(AX_ATTR_NAME, "Name");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
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
  root.location = gfx::RectF(0, 0, 30, 30);

  AXNodeData node1;
  node1.id = 1;
  node1.location = gfx::RectF(0, 0, 10, 10);
  node1.AddStringAttribute(AX_ATTR_NAME, "Name1");

  AXNodeData node2;
  node2.id = 2;
  node2.location = gfx::RectF(20, 20, 10, 10);
  node2.AddStringAttribute(AX_ATTR_NAME, "Name2");

  Init(root, node1, node2);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

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
  root.AddStringAttribute(AX_ATTR_NAME, "Name");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
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
  root.AddStringAttribute(AX_ATTR_DESCRIPTION, "Description");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
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
  root.AddStringAttribute(AX_ATTR_VALUE, "Value");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
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
  root.AddStringAttribute(AX_ATTR_KEY_SHORTCUTS, "Shortcut");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr shortcut;
  EXPECT_EQ(S_OK, root_obj->get_accKeyboardShortcut(SELF, shortcut.Receive()));
  EXPECT_STREQ(L"Shortcut", shortcut);

  EXPECT_EQ(E_INVALIDARG, root_obj->get_accKeyboardShortcut(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr k2;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_accKeyboardShortcut(bad_id, k2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleSelectionNotListBox) {
  // We only support AX_ROLE_LIST_BOX as this point, so, this should return
  // not implemented. We're choosing AX_ROLE_ALERT, but it could be anything
  // but AX_ROLE_LIST_BOX_OPTION.

  AXNodeData not_supported;
  not_supported.id = 0;
  not_supported.role = AX_ROLE_ALERT;

  Init(not_supported);
  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedVariant selection;
  EXPECT_EQ(E_NOTIMPL, root_obj->get_accSelection(selection.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleSelectionNothingSelected) {
  // We're going to set up a AX_ROLE_LIST_BOX_OPTION with 2 options with
  // nothing selected
  AXNodeData list;
  list.id = 0;
  list.role = AX_ROLE_LIST_BOX;

  list.child_ids.push_back(2);
  list.child_ids.push_back(3);

  AXNodeData list_item_2;
  list_item_2.id = 2;
  list_item_2.role = AX_ROLE_LIST_BOX_OPTION;
  list_item_2.AddStringAttribute(AX_ATTR_NAME, "Name2");

  AXNodeData list_item_3;
  list_item_3.id = 3;
  list_item_3.role = AX_ROLE_LIST_BOX_OPTION;
  list_item_3.AddStringAttribute(AX_ATTR_NAME, "Name3");

  // Nothing is selected.  This should return S_OK and the selection should
  // be VT_EMPTY.

  Init(list, list_item_2, list_item_3);
  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  EXPECT_EQ(VT_EMPTY, selection.type());
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleSelectionOneSelected) {
  // We're going to set up a AX_ROLE_LIST_BOX_OPTION with 2 options with
  // one selected.
  AXNodeData list;
  list.id = 0;
  list.role = AX_ROLE_LIST_BOX;

  list.child_ids.push_back(2);
  list.child_ids.push_back(3);

  AXNodeData list_item_2;
  list_item_2.id = 2;
  list_item_2.role = AX_ROLE_LIST_BOX_OPTION;
  list_item_2.state = 1 << ui::AX_STATE_SELECTED;
  list_item_2.AddStringAttribute(AX_ATTR_NAME, "Name2");

  AXNodeData list_item_3;
  list_item_3.id = 3;
  list_item_3.role = AX_ROLE_LIST_BOX_OPTION;
  list_item_3.AddStringAttribute(AX_ATTR_NAME, "Name3");

  Init(list, list_item_2, list_item_3);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  ASSERT_NE(nullptr, selection.ptr());

  CheckVariantHasName(selection, L"Name2");
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleSelectionMultipleSelected) {
  // We're going to set up a AX_ROLE_LIST_BOX_OPTION with 3 options with
  // two selected.
  AXNodeData list;
  list.id = 0;
  list.role = AX_ROLE_LIST_BOX;

  list.child_ids.push_back(2);
  list.child_ids.push_back(3);
  list.child_ids.push_back(4);

  AXNodeData list_item_2;
  list_item_2.id = 2;
  list_item_2.role = AX_ROLE_LIST_BOX_OPTION;
  list_item_2.state = 1 << ui::AX_STATE_SELECTED;
  list_item_2.AddStringAttribute(AX_ATTR_NAME, "Name2");

  AXNodeData list_item_3;
  list_item_3.id = 3;
  list_item_3.role = AX_ROLE_LIST_BOX_OPTION;
  list_item_3.state = 1 << ui::AX_STATE_SELECTED;
  list_item_3.AddStringAttribute(AX_ATTR_NAME, "Name3");

  AXNodeData list_item_4;
  list_item_4.id = 4;
  list_item_4.role = AX_ROLE_LIST_BOX_OPTION;
  list_item_4.AddStringAttribute(AX_ATTR_NAME, "Name4");
  Init(list, list_item_2, list_item_3, list_item_4);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedVariant selection;
  EXPECT_EQ(S_OK, root_obj->get_accSelection(selection.Receive()));
  ASSERT_NE(nullptr, selection.ptr());

  // Loop through the selections and  make sure we have the right ones
  ScopedComPtr<IEnumVARIANT> accessibles;
  HRESULT hr =
      V_DISPATCH(selection.ptr())->QueryInterface(IID_PPV_ARGS(&accessibles));
  EXPECT_EQ(S_OK, hr);
  ULONG ignore;

  // Check out the first selected item.
  {
    ScopedVariant item;
    hr = accessibles->Next(1, item.Receive(), &ignore);
    EXPECT_EQ(S_OK, hr);

    ScopedComPtr<IAccessible> accessible;
    HRESULT hr =
        V_DISPATCH(item.ptr())->QueryInterface(IID_PPV_ARGS(&accessible));
    EXPECT_EQ(S_OK, hr);
    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(L"Name2", name);
  }

  // and the second selected element.
  {
    ScopedVariant item;
    hr = accessibles->Next(1, item.Receive(), &ignore);
    EXPECT_EQ(S_OK, hr);

    ScopedComPtr<IAccessible> accessible;
    HRESULT hr =
        V_DISPATCH(item.ptr())->QueryInterface(IID_PPV_ARGS(&accessible));
    EXPECT_EQ(S_OK, hr);
    ScopedBstr name;
    EXPECT_EQ(S_OK, accessible->get_accName(SELF, name.Receive()));
    EXPECT_STREQ(L"Name3", name);
  }

  // There shouldn't be any more selected.
  {
    ScopedVariant item;
    hr = accessibles->Next(1, item.Receive(), &ignore);
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
  ScopedComPtr<IAccessible> child_iaccessible(
      IAccessibleFromNode(child_node));

  ScopedVariant role;

  child.role = AX_ROLE_ALERT;
  child_node->SetData(child);
  EXPECT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_ALERT, V_I4(role.ptr()));

  child.role = AX_ROLE_BUTTON;
  child_node->SetData(child);
  EXPECT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, V_I4(role.ptr()));

  child.role = AX_ROLE_POP_UP_BUTTON;
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
  root.location = gfx::RectF(10, 40, 800, 600);
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
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleChildAndParent) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.role = AX_ROLE_BUTTON;
  button.id = 2;

  AXNodeData checkbox;
  checkbox.role = AX_ROLE_CHECK_BOX;
  checkbox.id = 3;

  Init(root, button, checkbox);
  AXNode* button_node = GetRootNode()->children()[0];
  AXNode* checkbox_node = GetRootNode()->children()[1];
  ScopedComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ScopedComPtr<IAccessible> button_iaccessible(
      IAccessibleFromNode(button_node));
  ScopedComPtr<IAccessible> checkbox_iaccessible(
      IAccessibleFromNode(checkbox_node));

  LONG child_count;
  EXPECT_EQ(S_OK, root_iaccessible->get_accChildCount(&child_count));
  EXPECT_EQ(2L, child_count);
  EXPECT_EQ(S_OK, button_iaccessible->get_accChildCount(&child_count));
  EXPECT_EQ(0L, child_count);
  EXPECT_EQ(S_OK, checkbox_iaccessible->get_accChildCount(&child_count));
  EXPECT_EQ(0L, child_count);

  {
    ScopedComPtr<IDispatch> result;
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(SELF, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), root_iaccessible.Get());
  }

  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant child1(1);
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(child1, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), button_iaccessible.Get());
  }

  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant child2(2);
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(child2, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), checkbox_iaccessible.Get());
  }

  {
    // Asking for child id 3 should fail.
    ScopedComPtr<IDispatch> result;
    ScopedVariant child3(3);
    EXPECT_EQ(E_INVALIDARG,
              root_iaccessible->get_accChild(child3, result.GetAddressOf()));
  }

  // We should be able to ask for the button by its unique id too.
  LONG button_unique_id;
  ScopedComPtr<IAccessible2> button_iaccessible2 =
      ToIAccessible2(button_iaccessible);
  button_iaccessible2->get_uniqueID(&button_unique_id);
  ASSERT_LT(button_unique_id, 0);
  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant button_id_variant(button_unique_id);
    EXPECT_EQ(S_OK, root_iaccessible->get_accChild(button_id_variant,
                                                   result.GetAddressOf()));
    EXPECT_EQ(result.Get(), button_iaccessible.Get());
  }

  // We shouldn't be able to ask for the root node by its unique ID
  // from one of its children, though.
  LONG root_unique_id;
  ScopedComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(root_iaccessible);
  root_iaccessible2->get_uniqueID(&root_unique_id);
  ASSERT_LT(root_unique_id, 0);
  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant root_id_variant(root_unique_id);
    EXPECT_EQ(E_INVALIDARG, button_iaccessible->get_accChild(
                                root_id_variant, result.GetAddressOf()));
  }

  // Now check parents.
  {
    ScopedComPtr<IDispatch> result;
    EXPECT_EQ(S_OK, button_iaccessible->get_accParent(result.GetAddressOf()));
    EXPECT_EQ(result.Get(), root_iaccessible.Get());
  }

  {
    ScopedComPtr<IDispatch> result;
    EXPECT_EQ(S_OK, checkbox_iaccessible->get_accParent(result.GetAddressOf()));
    EXPECT_EQ(result.Get(), root_iaccessible.Get());
  }

  {
    ScopedComPtr<IDispatch> result;
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
  ScopedComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ScopedComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(root_iaccessible);
  ScopedComPtr<IAccessible> left_iaccessible(
      IAccessibleFromNode(GetRootNode()->children()[0]));
  ScopedComPtr<IAccessible2> left_iaccessible2 =
      ToIAccessible2(left_iaccessible);
  ScopedComPtr<IAccessible> right_iaccessible(
      IAccessibleFromNode(GetRootNode()->children()[1]));
  ScopedComPtr<IAccessible2> right_iaccessible2 =
      ToIAccessible2(right_iaccessible);

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
  root.role = AX_ROLE_ROOT_WEB_AREA;

  AXNodeData child1;
  child1.id = 2;
  child1.role = AX_ROLE_STATIC_TEXT;
  root.child_ids.push_back(2);

  AXNodeData child2;
  child2.id = 3;
  child2.role = AX_ROLE_STATIC_TEXT;
  root.child_ids.push_back(3);

  Init(root, child1, child2);
  ScopedComPtr<IAccessible> ia_root(GetRootIAccessible());
  ScopedComPtr<IDispatch> disp_root;
  ASSERT_HRESULT_SUCCEEDED(ia_root.CopyTo(disp_root.GetAddressOf()));
  ScopedVariant var_root(disp_root.Get());
  ScopedComPtr<IAccessible> ia_child1(
      IAccessibleFromNode(GetRootNode()->children()[0]));
  ScopedComPtr<IDispatch> disp_child1;
  ASSERT_HRESULT_SUCCEEDED(ia_child1.CopyTo(disp_child1.GetAddressOf()));
  ScopedVariant var_child1(disp_child1.Get());
  ScopedComPtr<IAccessible> ia_child2(
      IAccessibleFromNode(GetRootNode()->children()[1]));
  ScopedComPtr<IDispatch> disp_child2;
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

TEST_F(AXPlatformNodeWinTest, TestIAccessible2SetSelection) {
  AXNodeData text_field_node;
  text_field_node.id = 1;
  text_field_node.role = ui::AX_ROLE_TEXT_FIELD;
  text_field_node.state = 1 << ui::AX_STATE_EDITABLE;
  text_field_node.SetValue("Hi");

  Init(text_field_node);
  ScopedComPtr<IAccessible2> ia2_text_field =
      ToIAccessible2(GetRootIAccessible());
  ScopedComPtr<IAccessibleText> text_field;
  ia2_text_field.CopyTo(text_field.GetAddressOf());
  ASSERT_NE(nullptr, text_field.Get());

  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 0, 1));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 1, 0));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 2, 2));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, IA2_TEXT_OFFSET_CARET,
                                                    IA2_TEXT_OFFSET_LENGTH));

  EXPECT_HRESULT_FAILED(text_field->setSelection(1, 0, 0));
  EXPECT_HRESULT_FAILED(text_field->setSelection(0, 0, 5));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetAccessibilityAt) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  ScopedComPtr<IUnknown> cell_1;
  EXPECT_EQ(S_OK, result->get_accessibleAt(1, 1, cell_1.GetAddressOf()));
  CheckIUnknownHasName(cell_1, L"1");

  ScopedComPtr<IUnknown> cell_2;
  EXPECT_EQ(S_OK, result->get_accessibleAt(1, 2, cell_2.GetAddressOf()));
  CheckIUnknownHasName(cell_2, L"2");

  ScopedComPtr<IUnknown> cell_3;
  EXPECT_EQ(S_OK, result->get_accessibleAt(2, 1, cell_3.GetAddressOf()));
  CheckIUnknownHasName(cell_3, L"3");

  ScopedComPtr<IUnknown> cell_4;
  EXPECT_EQ(S_OK, result->get_accessibleAt(2, 2, cell_4.GetAddressOf()));
  CheckIUnknownHasName(cell_4, L"4");
}

TEST_F(AXPlatformNodeWinTest,
       TestIAccessibleTableGetAccessibilityAtOutOfBounds) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  {
    ScopedComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(-1, -1, cell.GetAddressOf()));
  }

  {
    ScopedComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(0, 5, cell.GetAddressOf()));
  }

  {
    ScopedComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(5, 0, cell.GetAddressOf()));
  }

  {
    ScopedComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG,
              result->get_accessibleAt(10, 10, cell.GetAddressOf()));
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetChildIndex) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long id;
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

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
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

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long columns_spanned;
  EXPECT_EQ(S_OK, result->get_columnExtentAt(1, 1, &columns_spanned));
  EXPECT_EQ(columns_spanned, 1);

  EXPECT_EQ(E_INVALIDARG, result->get_columnExtentAt(-1, -1, &columns_spanned));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetColumnIndex) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long index;
  EXPECT_EQ(S_OK, result->get_columnIndex(1, &index));
  EXPECT_EQ(index, 0);

  EXPECT_EQ(E_INVALIDARG, result->get_columnIndex(-1, &index));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNColumns) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long count;
  EXPECT_EQ(S_OK, result->get_nColumns(&count));
  EXPECT_EQ(count, 3);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNRows) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long count;
  EXPECT_EQ(S_OK, result->get_nRows(&count));
  EXPECT_EQ(count, 3);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetRowDescription) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
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

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long rows_spanned;
  EXPECT_EQ(S_OK, result->get_rowExtentAt(0, 1, &rows_spanned));
  EXPECT_EQ(rows_spanned, 0);

  EXPECT_EQ(E_INVALIDARG, result->get_columnExtentAt(-1, -1, &rows_spanned));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetRowIndex) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long index;
  EXPECT_EQ(S_OK, result->get_rowIndex(1, &index));
  EXPECT_EQ(index, 0);

  EXPECT_EQ(E_INVALIDARG, result->get_rowIndex(-1, &index));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetRowColumnExtentsAtIndex) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long row, column, row_extents, column_extents;
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

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());

  ScopedComPtr<IAccessibleTable2> result;
  root_obj.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  {
    ScopedComPtr<IUnknown> cell;
    EXPECT_EQ(S_OK, result->get_cellAt(1, 1, cell.GetAddressOf()));
    CheckIUnknownHasName(cell, L"1");
  }

  {
    ScopedComPtr<IUnknown> cell;
    EXPECT_EQ(E_INVALIDARG, result->get_cellAt(-1, -1, cell.GetAddressOf()));
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetColumnExtent) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  long column_spanned;
  EXPECT_EQ(S_OK, cell->get_columnExtent(&column_spanned));
  EXPECT_EQ(column_spanned, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetColumnHeaderCells) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  IUnknown** cell_accessibles;

  long number_cells;
  EXPECT_EQ(S_OK,
            cell->get_columnHeaderCells(&cell_accessibles, &number_cells));
  EXPECT_EQ(number_cells, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetColumnIndex) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  long index;
  EXPECT_EQ(S_OK, cell->get_columnIndex(&index));
  EXPECT_EQ(index, 0);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowExtent) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  long rows_spanned;
  EXPECT_EQ(S_OK, cell->get_rowExtent(&rows_spanned));
  EXPECT_EQ(rows_spanned, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowHeaderCells) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  IUnknown** cell_accessibles;

  long number_cells;
  EXPECT_EQ(S_OK, cell->get_rowHeaderCells(&cell_accessibles, &number_cells));

  // Since we do not have AX_ATTR_TABLE_CELL_ROW_INDEX set, the evaluated row
  // will be 0.  In this case, we do not expect any row headers.
  EXPECT_EQ(number_cells, 0);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowIndex) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  long index;
  EXPECT_EQ(S_OK, cell->get_rowIndex(&index));
  EXPECT_EQ(index, 0);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetRowColumnExtent) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  long row, column, row_extents, column_extents;
  boolean is_selected;
  EXPECT_EQ(S_OK, cell->get_rowColumnExtents(&row, &column, &row_extents,
                                             &column_extents, &is_selected));
  EXPECT_EQ(row, 0);
  EXPECT_EQ(column, 0);
  EXPECT_EQ(row_extents, 1);
  EXPECT_EQ(column_extents, 1);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableCellGetTable) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  // Check to make sure that this is the right table by checking one cell.
  ScopedComPtr<IUnknown> cell_1;
  EXPECT_EQ(S_OK, result->get_accessibleAt(1, 1, cell_1.GetAddressOf()));
  CheckIUnknownHasName(cell_1, L"1");
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2GetNRelations) {
  // This is is a duplicated of
  // BrowserAccessibilityTest::TestIAccessible2Relations but without the
  // specific COM/BrowserAccessibility knowledge.
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;

  std::vector<int32_t> describedby_ids = {1, 2, 3};
  root.AddIntListAttribute(ui::AX_ATTR_DESCRIBEDBY_IDS, describedby_ids);

  ui::AXNodeData child1;
  child1.id = 2;
  child1.role = ui::AX_ROLE_STATIC_TEXT;

  root.child_ids.push_back(2);

  ui::AXNodeData child2;
  child2.id = 3;
  child2.role = ui::AX_ROLE_STATIC_TEXT;

  root.child_ids.push_back(3);

  Init(root, child1, child2);
  ScopedComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ScopedComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(root_iaccessible);

  ScopedComPtr<IDispatch> result;
  EXPECT_EQ(S_OK, root_iaccessible2->get_accChild(ScopedVariant(1),
                                                  result.GetAddressOf()));
  ScopedComPtr<IAccessible2> ax_child1;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child1.GetAddressOf()));
  result.Reset();

  EXPECT_EQ(S_OK, root_iaccessible2->get_accChild(ScopedVariant(2),
                                                  result.GetAddressOf()));
  ScopedComPtr<IAccessible2> ax_child2;
  EXPECT_EQ(S_OK, result.CopyTo(ax_child2.GetAddressOf()));
  result.Reset();

  BuildRelationships(root_iaccessible2);
  BuildRelationships(ax_child1);
  BuildRelationships(ax_child2);

  LONG n_relations = 0;
  LONG n_targets = 0;
  ScopedBstr relation_type;
  ScopedComPtr<IAccessibleRelation> describedby_relation;
  ScopedComPtr<IAccessibleRelation> description_for_relation;
  ScopedComPtr<IUnknown> target;

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

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedChildrenZero) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedChildren;
  EXPECT_EQ(S_OK, result->get_nSelectedChildren(&selectedChildren));
  EXPECT_EQ(0, selectedChildren);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedChildrenOne) {
  AXTreeUpdate update = Build3X3Table();

  // 7 == table_cell_1
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[12].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedChildren;
  EXPECT_EQ(S_OK, result->get_nSelectedChildren(&selectedChildren));
  EXPECT_EQ(4, selectedChildren);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedColumnsZero) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[3].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[3].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;

  // 4 == table_column_header_3
  // 8 == table_cell_2
  // 12 == table_cell_4
  update.nodes[4].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[12].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  LONG selectedColumns;
  EXPECT_EQ(S_OK, result->get_nSelectedColumns(&selectedColumns));
  EXPECT_EQ(2, selectedColumns);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetNSelectedRowsZero) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[6].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[6].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;

  // 10 == table_row_header_3
  // 11 == table_cell_1
  // 12 == table_cell_2
  update.nodes[10].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[12].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[12].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[12].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long max_columns = 10;
  long* columns;
  long n_columns;
  EXPECT_EQ(S_OK,
            result->get_selectedColumns(max_columns, &columns, &n_columns));
  EXPECT_EQ(0, n_columns);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedColumnsOne) {
  AXTreeUpdate update = Build3X3Table();

  // 3 == table_column_header_2
  // 7 == table_cell_1
  // 11 == table_cell_3
  update.nodes[3].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long max_columns = 10;
  long* columns;
  long n_columns;
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
  update.nodes[3].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;

  // 4 == table_column_header_3
  // 8 == table_cell_2
  // 12 == table_cell_4
  update.nodes[4].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[12].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long max_columns = 10;
  long* columns;
  long n_columns;
  EXPECT_EQ(S_OK,
            result->get_selectedColumns(max_columns, &columns, &n_columns));
  EXPECT_EQ(2, n_columns);
  EXPECT_EQ(1, columns[0]);
  EXPECT_EQ(2, columns[1]);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedRowsZero) {
  Init(Build3X3Table());

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long max_rows = 10;
  long* rows;
  long n_rows;
  EXPECT_EQ(S_OK, result->get_selectedRows(max_rows, &rows, &n_rows));
  EXPECT_EQ(0, n_rows);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedRowsOne) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_1
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long max_rows = 10;
  long* rows;
  long n_rows;
  EXPECT_EQ(S_OK, result->get_selectedRows(max_rows, &rows, &n_rows));
  EXPECT_EQ(1, n_rows);
  EXPECT_EQ(1, rows[0]);
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleTableGetSelectedRowsMany) {
  AXTreeUpdate update = Build3X3Table();

  // 6 == table_row_header_3
  // 7 == table_cell_1
  // 8 == table_cell_2
  update.nodes[6].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;

  // 10 == table_row_header_3
  // 11 == table_cell_1
  // 12 == table_cell_2
  update.nodes[10].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[12].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
  table.CopyTo(result.GetAddressOf());
  ASSERT_NE(nullptr, result.Get());

  long max_rows = 10;
  long* rows;
  long n_rows;
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
  update.nodes[3].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[11].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[6].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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
  update.nodes[6].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[7].state = 1 << ui::AX_STATE_SELECTED;
  update.nodes[8].state = 1 << ui::AX_STATE_SELECTED;

  Init(update);

  ScopedComPtr<IAccessibleTableCell> cell = GetCellInTable();
  ASSERT_NE(nullptr, cell.Get());

  ScopedComPtr<IUnknown> table;
  EXPECT_EQ(S_OK, cell->get_table(table.GetAddressOf()));

  ScopedComPtr<IAccessibleTable> result;
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

}  // namespace ui
