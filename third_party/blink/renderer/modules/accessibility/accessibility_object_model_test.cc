// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

class AccessibilityObjectModelTest
    : public SimTest,
      public ScopedAccessibilityObjectModelForTest {
 public:
  AccessibilityObjectModelTest()
      : ScopedAccessibilityObjectModelForTest(true) {}

 protected:
  AXObjectCacheImpl* AXObjectCache() {
    GetDocument().GetSettings()->SetAccessibilityEnabled(true);
    return static_cast<AXObjectCacheImpl*>(
        GetDocument().GetOrCreateAXObjectCache());
  }
};

TEST_F(AccessibilityObjectModelTest, DOMElementsHaveAnAccessibleNode) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<button id=button>Click me</button>");

  auto* button = GetDocument().getElementById("button");
  EXPECT_NE(nullptr, button->accessibleNode());
  EXPECT_TRUE(button->accessibleNode()->role().IsNull());
  EXPECT_TRUE(button->accessibleNode()->label().IsNull());
}

TEST_F(AccessibilityObjectModelTest, SetAccessibleNodeRole) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<button id=button>Click me</button>");

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);

  auto* button = GetDocument().getElementById("button");
  ASSERT_NE(nullptr, button);

  auto* axButton = cache->GetOrCreate(button);
  EXPECT_EQ(kButtonRole, axButton->RoleValue());

  button->accessibleNode()->setRole("slider");
  EXPECT_EQ("slider", button->accessibleNode()->role());

  axButton = cache->GetOrCreate(button);
  EXPECT_EQ(kSliderRole, axButton->RoleValue());
}

TEST_F(AccessibilityObjectModelTest, AOMDoesNotReflectARIA) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<input id=textbox>");

  // Set ARIA attributes.
  auto* textbox = GetDocument().getElementById("textbox");
  ASSERT_NE(nullptr, textbox);
  textbox->setAttribute("role", "combobox");
  textbox->setAttribute("aria-label", "Combo");
  textbox->setAttribute("aria-disabled", "true");

  // Assert that the ARIA attributes affect the AX object.
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* axTextBox = cache->GetOrCreate(textbox);
  EXPECT_EQ(kTextFieldWithComboBoxRole, axTextBox->RoleValue());
  AXNameFrom name_from;
  AXObject::AXObjectVector name_objects;
  EXPECT_EQ("Combo", axTextBox->GetName(name_from, &name_objects));
  EXPECT_EQ(axTextBox->Restriction(), kDisabled);

  // The AOM properties should still all be null.
  EXPECT_EQ(nullptr, textbox->accessibleNode()->role());
  EXPECT_EQ(nullptr, textbox->accessibleNode()->label());
  bool is_null = false;
  EXPECT_FALSE(textbox->accessibleNode()->disabled(is_null));
  EXPECT_TRUE(is_null);
}

TEST_F(AccessibilityObjectModelTest, AOMPropertiesCanBeCleared) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<input type=button id=button>");

  // Set ARIA attributes.
  auto* button = GetDocument().getElementById("button");
  ASSERT_NE(nullptr, button);
  button->setAttribute("role", "checkbox");
  button->setAttribute("aria-label", "Check");
  button->setAttribute("aria-disabled", "true");

  // Assert that the AX object was affected by ARIA attributes.
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* axButton = cache->GetOrCreate(button);
  EXPECT_EQ(kCheckBoxRole, axButton->RoleValue());
  AXNameFrom name_from;
  AXObject::AXObjectVector name_objects;
  EXPECT_EQ("Check", axButton->GetName(name_from, &name_objects));
  EXPECT_EQ(axButton->Restriction(), kDisabled);

  // Now set the AOM properties to override.
  button->accessibleNode()->setRole("radio");
  button->accessibleNode()->setLabel("Radio");
  button->accessibleNode()->setDisabled(false, false);

  // Assert that the AX object was affected by AOM properties.
  axButton = cache->GetOrCreate(button);
  EXPECT_EQ(kRadioButtonRole, axButton->RoleValue());
  EXPECT_EQ("Radio", axButton->GetName(name_from, &name_objects));
  EXPECT_EQ(axButton->Restriction(), kNone);

  // Null the AOM properties.
  button->accessibleNode()->setRole(g_null_atom);
  button->accessibleNode()->setLabel(g_null_atom);
  button->accessibleNode()->setDisabled(false, true);

  // The AX Object should now revert to ARIA.
  axButton = cache->GetOrCreate(button);
  EXPECT_EQ(kCheckBoxRole, axButton->RoleValue());
  EXPECT_EQ("Check", axButton->GetName(name_from, &name_objects));
  EXPECT_EQ(axButton->Restriction(), kDisabled);
}

TEST_F(AccessibilityObjectModelTest, RangeProperties) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div role=slider id=slider>");

  auto* slider = GetDocument().getElementById("slider");
  ASSERT_NE(nullptr, slider);
  slider->accessibleNode()->setValueMin(-0.5, false);
  slider->accessibleNode()->setValueMax(0.5, false);
  slider->accessibleNode()->setValueNow(0.1, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* ax_slider = cache->GetOrCreate(slider);
  float value = 0.0f;
  EXPECT_TRUE(ax_slider->MinValueForRange(&value));
  EXPECT_EQ(-0.5f, value);
  EXPECT_TRUE(ax_slider->MaxValueForRange(&value));
  EXPECT_EQ(0.5f, value);
  EXPECT_TRUE(ax_slider->ValueForRange(&value));
  EXPECT_EQ(0.1f, value);
}

TEST_F(AccessibilityObjectModelTest, Level) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div role=heading id=heading>");

  auto* heading = GetDocument().getElementById("heading");
  ASSERT_NE(nullptr, heading);
  heading->accessibleNode()->setLevel(5, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* ax_heading = cache->GetOrCreate(heading);
  EXPECT_EQ(5, ax_heading->HeadingLevel());
}

TEST_F(AccessibilityObjectModelTest, ListItem) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<div role=list><div role=listitem id=listitem></div></div>");

  auto* listitem = GetDocument().getElementById("listitem");
  ASSERT_NE(nullptr, listitem);
  listitem->accessibleNode()->setPosInSet(9, false);
  listitem->accessibleNode()->setSetSize(10, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* ax_listitem = cache->GetOrCreate(listitem);
  EXPECT_EQ(9, ax_listitem->PosInSet());
  EXPECT_EQ(10, ax_listitem->SetSize());
}

TEST_F(AccessibilityObjectModelTest, Grid) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div role=grid id=grid>
      <div role=row id=row>
        <div role=gridcell id=cell></div>
        <div role=gridcell id=cell2></div>
      </div>
    </div>
  )HTML");

  auto* grid = GetDocument().getElementById("grid");
  ASSERT_NE(nullptr, grid);
  grid->accessibleNode()->setColCount(16, false);
  grid->accessibleNode()->setRowCount(9, false);

  auto* row = GetDocument().getElementById("row");
  ASSERT_NE(nullptr, row);
  row->accessibleNode()->setColIndex(8, false);
  row->accessibleNode()->setRowIndex(5, false);

  auto* cell = GetDocument().getElementById("cell");

  auto* cell2 = GetDocument().getElementById("cell2");
  ASSERT_NE(nullptr, cell2);
  cell2->accessibleNode()->setColIndex(10, false);
  cell2->accessibleNode()->setRowIndex(7, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);

  auto* ax_grid = cache->GetOrCreate(grid);
  EXPECT_EQ(16, ax_grid->AriaColumnCount());
  EXPECT_EQ(9, ax_grid->AriaRowCount());

  auto* ax_cell = cache->GetOrCreate(cell);
  EXPECT_TRUE(ax_cell->IsTableCellLikeRole());
  EXPECT_EQ(8U, ax_cell->AriaColumnIndex());
  EXPECT_EQ(5U, ax_cell->AriaRowIndex());

  auto* ax_cell2 = cache->GetOrCreate(cell2);
  EXPECT_TRUE(ax_cell2->IsTableCellLikeRole());
  EXPECT_EQ(10U, ax_cell2->AriaColumnIndex());
  EXPECT_EQ(7U, ax_cell2->AriaRowIndex());
}

class SparseAttributeAdapter : public AXSparseAttributeClient {
 public:
  SparseAttributeAdapter() = default;

  std::map<AXBoolAttribute, bool> bool_attributes;
  std::map<AXStringAttribute, String> string_attributes;
  std::map<AXObjectAttribute, Persistent<AXObject>> object_attributes;
  std::map<AXObjectVectorAttribute, HeapVector<Member<AXObject>>>
      object_vector_attributes;

 private:
  void AddBoolAttribute(AXBoolAttribute attribute, bool value) override {
    ASSERT_TRUE(bool_attributes.find(attribute) == bool_attributes.end());
    bool_attributes[attribute] = value;
  }

  void AddStringAttribute(AXStringAttribute attribute,
                          const String& value) override {
    ASSERT_TRUE(string_attributes.find(attribute) == string_attributes.end());
    string_attributes[attribute] = value;
  }

  void AddObjectAttribute(AXObjectAttribute attribute,
                          AXObject& value) override {
    ASSERT_TRUE(object_attributes.find(attribute) == object_attributes.end());
    object_attributes[attribute] = value;
  }

  void AddObjectVectorAttribute(AXObjectVectorAttribute attribute,
                                HeapVector<Member<AXObject>>& value) override {
    ASSERT_TRUE(object_vector_attributes.find(attribute) ==
                object_vector_attributes.end());
    object_vector_attributes[attribute] = value;
  }
};

TEST_F(AccessibilityObjectModelTest, SparseAttributes) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <input id=target
     aria-keyshortcuts=Ctrl+K
     aria-roledescription=Widget
     aria-activedescendant=active
     aria-details=details
     aria-errormessage=error>
    <div id=active role=option></div>
    <div id=active2 role=gridcell></div>
    <div id=details role=contentinfo></div>
    <div id=details2 role=form></div>
    <div id=error role=article>Error</div>
    <div id=error2 role=banner>Error 2</div>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* ax_target = cache->GetOrCreate(target);
  SparseAttributeAdapter sparse_attributes;
  ax_target->GetSparseAXAttributes(sparse_attributes);

  ASSERT_EQ("Ctrl+K",
            sparse_attributes
                .string_attributes[AXStringAttribute::kAriaKeyShortcuts]);
  ASSERT_EQ("Widget",
            sparse_attributes
                .string_attributes[AXStringAttribute::kAriaRoleDescription]);
  ASSERT_EQ(kListBoxOptionRole,
            sparse_attributes
                .object_attributes[AXObjectAttribute::kAriaActiveDescendant]
                ->RoleValue());
  ASSERT_EQ(
      kContentInfoRole,
      sparse_attributes.object_attributes[AXObjectAttribute::kAriaDetails]
          ->RoleValue());
  ASSERT_EQ(
      kArticleRole,
      sparse_attributes.object_attributes[AXObjectAttribute::kAriaErrorMessage]
          ->RoleValue());

  target->accessibleNode()->setKeyShortcuts("Ctrl+L");
  target->accessibleNode()->setRoleDescription("Object");
  target->accessibleNode()->setActiveDescendant(
      GetDocument().getElementById("active2")->accessibleNode());
  target->accessibleNode()->setDetails(
      GetDocument().getElementById("details2")->accessibleNode());
  target->accessibleNode()->setErrorMessage(
      GetDocument().getElementById("error2")->accessibleNode());

  SparseAttributeAdapter sparse_attributes2;
  ax_target->GetSparseAXAttributes(sparse_attributes2);

  ASSERT_EQ("Ctrl+L",
            sparse_attributes2
                .string_attributes[AXStringAttribute::kAriaKeyShortcuts]);
  ASSERT_EQ("Object",
            sparse_attributes2
                .string_attributes[AXStringAttribute::kAriaRoleDescription]);
  ASSERT_EQ(kCellRole,
            sparse_attributes2
                .object_attributes[AXObjectAttribute::kAriaActiveDescendant]
                ->RoleValue());
  ASSERT_EQ(kFormRole, sparse_attributes2
                           .object_attributes[AXObjectAttribute::kAriaDetails]
                           ->RoleValue());
  ASSERT_EQ(kBannerRole,
            sparse_attributes2
                .object_attributes[AXObjectAttribute::kAriaErrorMessage]
                ->RoleValue());
}

TEST_F(AccessibilityObjectModelTest, LabeledBy) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <input id=target aria-labelledby='l1 l2'>
    <label id=l1>Label 1</label>
    <label id=l2>Label 2</label>
    <label id=l3>Label 3</label>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  auto* l1 = GetDocument().getElementById("l1");
  auto* l2 = GetDocument().getElementById("l2");
  auto* l3 = GetDocument().getElementById("l3");

  HeapVector<Member<Element>> labeled_by;
  ASSERT_TRUE(AccessibleNode::GetPropertyOrARIAAttribute(
      target, AOMRelationListProperty::kLabeledBy, labeled_by));
  ASSERT_EQ(2U, labeled_by.size());
  ASSERT_EQ(l1, labeled_by[0]);
  ASSERT_EQ(l2, labeled_by[1]);

  AccessibleNodeList* node_list = target->accessibleNode()->labeledBy();
  ASSERT_EQ(nullptr, node_list);

  node_list = new AccessibleNodeList();
  node_list->add(l3->accessibleNode());
  target->accessibleNode()->setLabeledBy(node_list);

  labeled_by.clear();
  ASSERT_TRUE(AccessibleNode::GetPropertyOrARIAAttribute(
      target, AOMRelationListProperty::kLabeledBy, labeled_by));
  ASSERT_EQ(1U, labeled_by.size());
  ASSERT_EQ(l3, labeled_by[0]);
}

}  // namespace

}  // namespace blink
