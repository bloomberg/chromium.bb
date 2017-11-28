// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_unittest.h"

namespace ui {

AXPlatformNodeTest::AXPlatformNodeTest() {}

AXPlatformNodeTest::~AXPlatformNodeTest() {}

void AXPlatformNodeTest::Init(const AXTreeUpdate& initial_state) {
  tree_.reset(new AXTree(initial_state));
}

void AXPlatformNodeTest::Init(const AXNodeData& node1) {
  AXTreeUpdate update;
  update.root_id = node1.id;
  update.nodes.push_back(node1);
  Init(update);
}

void AXPlatformNodeTest::Init(const AXNodeData& node1,
                              const AXNodeData& node2) {
  AXTreeUpdate update;
  update.root_id = node1.id;
  update.nodes.push_back(node1);
  update.nodes.push_back(node2);
  Init(update);
}

void AXPlatformNodeTest::Init(const AXNodeData& node1,
                              const AXNodeData& node2,
                              const AXNodeData& node3) {
  AXTreeUpdate update;
  update.root_id = node1.id;
  update.nodes.push_back(node1);
  update.nodes.push_back(node2);
  update.nodes.push_back(node3);
  Init(update);
}

void AXPlatformNodeTest::Init(const AXNodeData& node1,
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

AXTreeUpdate AXPlatformNodeTest::BuildTextField() {
  AXNodeData text_field_node;
  text_field_node.id = 1;
  text_field_node.role = AX_ROLE_TEXT_FIELD;
  text_field_node.AddState(AX_STATE_EDITABLE);
  text_field_node.SetValue("How now brown cow.");

  AXTreeUpdate update;
  update.root_id = text_field_node.id;
  update.nodes.push_back(text_field_node);
  return update;
}

AXTreeUpdate AXPlatformNodeTest::BuildTextFieldWithSelectionRange(
    int32_t start,
    int32_t stop) {
  AXNodeData text_field_node;
  text_field_node.id = 1;
  text_field_node.role = AX_ROLE_TEXT_FIELD;
  text_field_node.AddState(AX_STATE_EDITABLE);
  text_field_node.AddState(AX_STATE_SELECTED);
  text_field_node.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START, start);
  text_field_node.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END, stop);
  text_field_node.SetValue("How now brown cow.");

  AXTreeUpdate update;
  update.root_id = text_field_node.id;
  update.nodes.push_back(text_field_node);
  return update;
}

AXTreeUpdate AXPlatformNodeTest::BuildContentEditable() {
  AXNodeData content_editable_node;
  content_editable_node.id = 1;
  content_editable_node.role = AX_ROLE_GROUP;
  content_editable_node.AddState(AX_STATE_RICHLY_EDITABLE);
  content_editable_node.AddBoolAttribute(ui::AX_ATTR_EDITABLE_ROOT, true);
  content_editable_node.SetValue("How now brown cow.");

  AXTreeUpdate update;
  update.root_id = content_editable_node.id;
  update.nodes.push_back(content_editable_node);
  return update;
}

AXTreeUpdate AXPlatformNodeTest::BuildContentEditableWithSelectionRange(
    int32_t start,
    int32_t end) {
  AXNodeData content_editable_node;
  content_editable_node.id = 1;
  content_editable_node.role = AX_ROLE_GROUP;
  content_editable_node.AddState(AX_STATE_RICHLY_EDITABLE);
  content_editable_node.AddState(AX_STATE_SELECTED);
  content_editable_node.AddBoolAttribute(ui::AX_ATTR_EDITABLE_ROOT, true);
  content_editable_node.SetValue("How now brown cow.");

  AXTreeUpdate update;
  update.root_id = content_editable_node.id;
  update.nodes.push_back(content_editable_node);

  update.has_tree_data = true;
  update.tree_data.sel_anchor_object_id = content_editable_node.id;
  update.tree_data.sel_focus_object_id = content_editable_node.id;
  update.tree_data.sel_anchor_offset = start;
  update.tree_data.sel_focus_offset = end;

  return update;
}

AXTreeUpdate AXPlatformNodeTest::AXPlatformNodeTest::Build3X3Table() {
  /*
    Build a table that looks like:

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
  table.role = AX_ROLE_TABLE;

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
  table_row_header.role = AX_ROLE_ROW;
  table_row_header.child_ids.push_back(51);
  table_row_header.child_ids.push_back(52);
  table_row_header.child_ids.push_back(53);

  AXNodeData table_column_header_1;
  table_column_header_1.id = 51;
  table_column_header_1.role = AX_ROLE_COLUMN_HEADER;

  AXNodeData table_column_header_2;
  table_column_header_2.id = 52;
  table_column_header_2.role = AX_ROLE_COLUMN_HEADER;
  table_column_header_2.SetName("column header 1");

  AXNodeData table_column_header_3;
  table_column_header_3.id = 53;
  table_column_header_3.role = AX_ROLE_COLUMN_HEADER;
  // Either AX_ATTR_NAME -or- AX_ATTR_DESCRIPTION is acceptable for a
  // description
  table_column_header_3.AddStringAttribute(AX_ATTR_DESCRIPTION,
                                           "column header 2");

  // Row 1
  AXNodeData table_row_1;
  table_row_1.id = 1;
  table_row_1.role = AX_ROLE_ROW;
  table_row_1.child_ids.push_back(2);
  table_row_1.child_ids.push_back(3);
  table_row_1.child_ids.push_back(4);

  AXNodeData table_row_header_1;
  table_row_header_1.id = 2;
  table_row_header_1.role = AX_ROLE_ROW_HEADER;
  table_row_header_1.SetName("row header 1");

  AXNodeData table_cell_1;
  table_cell_1.id = 3;
  table_cell_1.role = AX_ROLE_CELL;
  table_cell_1.SetName("1");

  AXNodeData table_cell_2;
  table_cell_2.id = 4;
  table_cell_2.role = AX_ROLE_CELL;
  table_cell_2.SetName("2");

  // Row 2
  AXNodeData table_row_2;
  table_row_2.id = 10;
  table_row_2.role = AX_ROLE_ROW;
  table_row_2.child_ids.push_back(11);
  table_row_2.child_ids.push_back(12);
  table_row_2.child_ids.push_back(13);

  AXNodeData table_row_header_2;
  table_row_header_2.id = 11;
  table_row_header_2.role = AX_ROLE_ROW_HEADER;
  // Either AX_ATTR_NAME -or- AX_ATTR_DESCRIPTION is acceptable for a
  // description
  table_row_header_2.AddStringAttribute(AX_ATTR_DESCRIPTION, "row header 2");

  AXNodeData table_cell_3;
  table_cell_3.id = 12;
  table_cell_3.role = AX_ROLE_CELL;
  table_cell_3.SetName("3");

  AXNodeData table_cell_4;
  table_cell_4.id = 13;
  table_cell_4.role = AX_ROLE_CELL;
  table_cell_4.SetName("4");

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

}  // namespace ui
