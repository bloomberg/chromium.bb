// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_language_info.h"
#include "base/command_line.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(AXLanguageInfoTest, TestSwitch) {
  // TODO(crbug/889370): Remove this test once this feature is stable
  EXPECT_FALSE(
      ::switches::AreExperimentalAccessibilityLanguageDetectionEnabled());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);

  EXPECT_TRUE(
      ::switches::AreExperimentalAccessibilityLanguageDetectionEnabled());
}

// Tests that AXNode::GetLanguage() terminates when there is no lang attribute.
TEST(AXLanguageInfoTest, TestGetLanguageNoLangAttr) {
  // build tree including parenting, this is to exercise the code paths within
  // AXNode::GetLanguage() which scan up the tree to handle lang inheritance.
  //      1
  //    2   3
  //  4
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.resize(2);
  initial_state.nodes[0].child_ids[0] = 2;
  initial_state.nodes[0].child_ids[1] = 3;
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.resize(1);
  initial_state.nodes[1].child_ids[0] = 4;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[3].id = 4;
  AXTree tree(initial_state);

  // Check that tree parenting conforms to expected shape.
  AXNode* item1 = tree.GetFromId(1);
  EXPECT_EQ(item1->parent(), nullptr);

  AXNode* item2 = tree.GetFromId(2);
  EXPECT_EQ(item2->parent(), item1);
  EXPECT_EQ(item2->parent()->parent(), nullptr);

  AXNode* item3 = tree.GetFromId(3);
  EXPECT_EQ(item3->parent(), item1);
  EXPECT_EQ(item3->parent()->parent(), nullptr);

  AXNode* item4 = tree.GetFromId(4);
  EXPECT_EQ(item4->parent(), item2);
  EXPECT_EQ(item4->parent()->parent(), item1);
  EXPECT_EQ(item4->parent()->parent()->parent(), nullptr);

  std::string item1_lang = item1->GetLanguage();
  EXPECT_EQ(item1_lang, "");

  std::string item2_lang = item2->GetLanguage();
  EXPECT_EQ(item2_lang, "");

  std::string item3_lang = item3->GetLanguage();
  EXPECT_EQ(item3_lang, "");

  std::string item4_lang = item4->GetLanguage();
  EXPECT_EQ(item4_lang, "");
}

TEST(AXLanguageInfoTest, TestGetLanguageLangAttrInheritance) {
  // build tree including parenting, this is to exercise the code paths within
  // AXNode::GetLanguage() which scan up the tree to handle lang inheritance.
  //        1
  //      2   3
  //    4
  //  5
  //
  //  1 - English
  //  2 - French
  //  all other nodes are unspecified
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.resize(2);
  initial_state.nodes[0].child_ids[0] = 2;
  initial_state.nodes[0].child_ids[1] = 3;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLanguage, "en");
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.resize(1);
  initial_state.nodes[1].child_ids[0] = 4;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kLanguage, "fr");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].child_ids.resize(1);
  initial_state.nodes[3].child_ids[0] = 5;
  initial_state.nodes[4].id = 5;
  AXTree tree(initial_state);

  AXNode* item1 = tree.GetFromId(1);
  std::string item1_lang = item1->GetLanguage();
  EXPECT_EQ(item1_lang, "en");

  AXNode* item2 = tree.GetFromId(2);
  std::string item2_lang = item2->GetLanguage();
  EXPECT_EQ(item2_lang, "fr");

  AXNode* item3 = tree.GetFromId(3);
  std::string item3_lang = item3->GetLanguage();
  EXPECT_EQ(item3_lang, "en");

  AXNode* item4 = tree.GetFromId(4);
  std::string item4_lang = item4->GetLanguage();
  EXPECT_EQ(item4_lang, "fr");

  AXNode* item5 = tree.GetFromId(5);
  std::string item5_lang = item5->GetLanguage();
  EXPECT_EQ(item5_lang, "fr");
}

TEST(AXLanguageInfoTest, TestGetLanguageLangDetection) {
  // build tree including parenting, this is to exercise the code paths within
  // AXNode::GetLanguage() which scan up the tree to handle lang inheritance.
  //        1
  //      2   3
  //    4
  //  5
  //
  //  1 - English lang attribute, no text
  //  2 - French lang attribute,  no text
  //  3 - no attribute,           French text
  //  4 - no attribute,           English text
  //  5 - no attribute,           German text
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);

  {
    AXNodeData& node1 = initial_state.nodes[0];
    node1.id = 1;
    node1.child_ids.resize(2);
    node1.child_ids[0] = 2;
    node1.child_ids[1] = 3;
    node1.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en");
  }

  {
    AXNodeData& node2 = initial_state.nodes[1];
    node2.id = 2;
    node2.child_ids.resize(1);
    node2.child_ids[0] = 4;
    node2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
  }

  {
    AXNodeData& node3 = initial_state.nodes[2];
    node3.id = 3;
    node3.role = ax::mojom::Role::kStaticText;
    std::string node3_text =
        "Ce texte a été créé avec Google Translate, il est peu probable qu'il "
        "soit idiomatique dans la langue cible indiquée Ce texte est "
        "uniquement utilisé pour tester la détection de la langue.";
    node3.AddStringAttribute(ax::mojom::StringAttribute::kName, node3_text);
  }

  {
    AXNodeData& node4 = initial_state.nodes[3];
    node4.id = 4;
    node4.child_ids.resize(1);
    node4.child_ids[0] = 5;
    node4.role = ax::mojom::Role::kStaticText;
    std::string node4_text =
        "This is text created using Google Translate, it is unlikely to be "
        "idomatic in the given target langauge. This text is only used to test "
        "language detection";
    node4.AddStringAttribute(ax::mojom::StringAttribute::kName, node4_text);
  }

  {
    AXNodeData& node5 = initial_state.nodes[4];
    node5.id = 5;
    node5.role = ax::mojom::Role::kStaticText;
    std::string node5_text =
        "Dies ist ein mit Google Translate erstellter Text. Es ist "
        "unwahrscheinlich, dass er in der angegebenen Zielsprache idiomatisch "
        "ist. Dieser Text wird nur zum Testen der Spracherkennung verwendet.";
    node5.AddStringAttribute(ax::mojom::StringAttribute::kName, node5_text);
  }

  AXTree tree(initial_state);

  // TODO(crbug/889370): these expectations will be wrong once we detect
  // language from the text.
  AXNode* item1 = tree.GetFromId(1);
  std::string item1_lang = item1->GetLanguage();
  EXPECT_EQ(item1_lang, "en");

  AXNode* item2 = tree.GetFromId(2);
  std::string item2_lang = item2->GetLanguage();
  EXPECT_EQ(item2_lang, "fr");

  AXNode* item3 = tree.GetFromId(3);
  std::string item3_lang = item3->GetLanguage();
  EXPECT_EQ(item3_lang, "en");

  AXNode* item4 = tree.GetFromId(4);
  std::string item4_lang = item4->GetLanguage();
  EXPECT_EQ(item4_lang, "fr");

  AXNode* item5 = tree.GetFromId(5);
  std::string item5_lang = item5->GetLanguage();
  EXPECT_EQ(item5_lang, "fr");
}

}  // namespace ui
