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

 protected:
  AXNode* GetRootNode() {
    return tree_->root();
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
  root.AddStringAttribute(AX_ATTR_SHORTCUT, "Shortcut");
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
    EXPECT_EQ(result.Get(), root_iaccessible);
  }

  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant child1(1);
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(child1, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), button_iaccessible);
  }

  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant child2(2);
    EXPECT_EQ(S_OK,
              root_iaccessible->get_accChild(child2, result.GetAddressOf()));
    EXPECT_EQ(result.Get(), checkbox_iaccessible);
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
    EXPECT_EQ(result.Get(), button_iaccessible);
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
    EXPECT_EQ(result.Get(), root_iaccessible);
  }

  {
    ScopedComPtr<IDispatch> result;
    EXPECT_EQ(S_OK, checkbox_iaccessible->get_accParent(result.GetAddressOf()));
    EXPECT_EQ(result.Get(), root_iaccessible);
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
  ASSERT_NE(nullptr, text_field);

  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 0, 1));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 1, 0));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, 2, 2));
  EXPECT_HRESULT_SUCCEEDED(text_field->setSelection(0, IA2_TEXT_OFFSET_CARET,
                                                    IA2_TEXT_OFFSET_LENGTH));

  EXPECT_HRESULT_FAILED(text_field->setSelection(1, 0, 0));
  EXPECT_HRESULT_FAILED(text_field->setSelection(0, 0, 5));
}

}  // namespace ui
