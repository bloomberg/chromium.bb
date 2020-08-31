// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class FakeMediaControlElement : public HTMLDivElement {
 public:
  FakeMediaControlElement(Document& document) : HTMLDivElement(document) {}

  bool IsMediaControlElement() const override { return true; }
};

class FakeMediaControls : public HTMLDivElement {
 public:
  FakeMediaControls(Document& document) : HTMLDivElement(document) {}

  bool IsMediaControls() const override { return true; }
};

class NodeTest : public EditingTestBase {
 protected:
  LayoutObject* ReattachLayoutTreeForNode(Node& node) {
    node.SetForceReattachLayoutTree();
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
    GetDocument().GetStyleEngine().RecalcStyle();
    Node::AttachContext context;
    context.parent = LayoutTreeBuilderTraversal::ParentLayoutObject(node);
    GetDocument().GetStyleEngine().in_layout_tree_rebuild_ = true;
    node.ReattachLayoutTree(context);
    return context.previous_in_flow;
  }

  // Generate the following DOM structure and return the innermost <div>.
  //  + div#root
  //    + #shadow
  //      + test node
  //      |  + #shadow
  //      |    + div class="test"
  Node* InitializeUserAgentShadowTree(Element* test_node) {
    SetBodyContent("<div id=\"root\"></div>");
    Element* root = GetDocument().getElementById("root");
    ShadowRoot& first_shadow = root->CreateUserAgentShadowRoot();

    first_shadow.AppendChild(test_node);
    ShadowRoot& second_shadow = test_node->CreateUserAgentShadowRoot();

    auto* class_div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    class_div->setAttribute("class", "test");
    second_shadow.AppendChild(class_div);
    return class_div;
  }
};

TEST_F(NodeTest, canStartSelection) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById("one");
  Node* two = GetDocument().getElementById("two");

  EXPECT_FALSE(one->CanStartSelection());
  EXPECT_FALSE(one->firstChild()->CanStartSelection());
  EXPECT_TRUE(two->CanStartSelection());
  EXPECT_TRUE(two->firstChild()->CanStartSelection());
}

TEST_F(NodeTest, canStartSelectionWithShadowDOM) {
  const char* body_content = "<div id=host><span id=one>one</span></div>";
  const char* shadow_content =
      "<a href='http://www.msn.com'><content></content></a>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* one = GetDocument().getElementById("one");

  EXPECT_FALSE(one->CanStartSelection());
  EXPECT_FALSE(one->firstChild()->CanStartSelection());
}

TEST_F(NodeTest, customElementState) {
  const char* body_content = "<div id=div></div>";
  SetBodyContent(body_content);
  Element* div = GetDocument().getElementById("div");
  EXPECT_EQ(CustomElementState::kUncustomized, div->GetCustomElementState());
  EXPECT_TRUE(div->IsDefined());
  EXPECT_EQ(Node::kV0NotCustomElement, div->GetV0CustomElementState());

  div->SetCustomElementState(CustomElementState::kUndefined);
  EXPECT_EQ(CustomElementState::kUndefined, div->GetCustomElementState());
  EXPECT_FALSE(div->IsDefined());
  EXPECT_EQ(Node::kV0NotCustomElement, div->GetV0CustomElementState());

  div->SetCustomElementState(CustomElementState::kCustom);
  EXPECT_EQ(CustomElementState::kCustom, div->GetCustomElementState());
  EXPECT_TRUE(div->IsDefined());
  EXPECT_EQ(Node::kV0NotCustomElement, div->GetV0CustomElementState());
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_TextRoot) {
  SetBodyContent("Text");
  Node* root = GetDocument().body()->firstChild();
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_InlineRoot) {
  SetBodyContent("<span id=root>Text <span></span></span>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_BlockRoot) {
  SetBodyContent("<div id=root>Text <span></span></div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_FloatRoot) {
  SetBodyContent("<div id=root style='float:left'><span></span></div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_AbsoluteRoot) {
  SetBodyContent("<div id=root style='position:absolute'><span></span></div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Text) {
  SetBodyContent("<div id=root style='display:contents'>Text</div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Inline) {
  SetBodyContent("<div id=root style='display:contents'><span></span></div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Block) {
  SetBodyContent("<div id=root style='display:contents'><div></div></div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(root->firstChild()->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Float) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .float { float:left }"
      "</style>"
      "<div id=root><div class=float></div></div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_AbsolutePositioned) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .abs { position:absolute }"
      "</style>"
      "<div id=root><div class=abs></div></div>");
  Element* root = GetDocument().getElementById("root");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_FALSE(previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_SkipAbsolute) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .abs { position:absolute }"
      "</style>"
      "<div id=root>"
      "<div class=abs></div><span id=inline></span><div class=abs></div>"
      "</div>");
  Element* root = GetDocument().getElementById("root");
  Element* span = GetDocument().getElementById("inline");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_SkipFloats) {
  SetBodyContent(
      "<style>"
      "  #root { display:contents }"
      "  .float { float:left }"
      "</style>"
      "<div id=root>"
      "<div class=float></div>"
      "<span id=inline></span>"
      "<div class=float></div>"
      "</div>");
  Element* root = GetDocument().getElementById("root");
  Element* span = GetDocument().getElementById("inline");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_InsideDisplayContents) {
  SetBodyContent(
      "<style>"
      "  #root, .contents { display:contents }"
      "  .float { float:left }"
      "</style>"
      "<div id=root>"
      "<span></span><div class=contents><span id=inline></span></div>"
      "</div>");
  Element* root = GetDocument().getElementById("root");
  Element* span = GetDocument().getElementById("inline");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_Slotted) {
  SetBodyContent("<div id=host><span id=inline></span></div>");
  ShadowRoot& shadow_root =
      GetDocument().getElementById("host")->AttachShadowRootInternal(
          ShadowRootType::kOpen);
  shadow_root.setInnerHTML(
      "<div id=root style='display:contents'><span></span><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  Element* root = shadow_root.getElementById("root");
  Element* span = GetDocument().getElementById("inline");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, AttachContext_PreviousInFlow_V0Content) {
  SetBodyContent("<div id=host><span id=inline></span></div>");
  ShadowRoot* shadow_root = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), "host",
      "<div id=root style='display:contents'><span></span><content /></div>");
  Element* root = shadow_root->getElementById("root");
  Element* span = GetDocument().getElementById("inline");
  LayoutObject* previous_in_flow = ReattachLayoutTreeForNode(*root);

  EXPECT_TRUE(previous_in_flow);
  EXPECT_EQ(span->GetLayoutObject(), previous_in_flow);
}

TEST_F(NodeTest, HasMediaControlAncestor_Fail) {
  auto* node = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  EXPECT_FALSE(node->HasMediaControlAncestor());
  EXPECT_FALSE(InitializeUserAgentShadowTree(node)->HasMediaControlAncestor());
}

TEST_F(NodeTest, HasMediaControlAncestor_MediaControlElement) {
  FakeMediaControlElement* node =
      MakeGarbageCollected<FakeMediaControlElement>(GetDocument());
  EXPECT_TRUE(node->HasMediaControlAncestor());
  EXPECT_TRUE(InitializeUserAgentShadowTree(node)->HasMediaControlAncestor());
}

TEST_F(NodeTest, HasMediaControlAncestor_MediaControls) {
  FakeMediaControls* node =
      MakeGarbageCollected<FakeMediaControls>(GetDocument());
  EXPECT_TRUE(node->HasMediaControlAncestor());
  EXPECT_TRUE(InitializeUserAgentShadowTree(node)->HasMediaControlAncestor());
}

TEST_F(NodeTest, appendChildProcessingInstructionNoStyleRecalc) {
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  auto* pi =
      MakeGarbageCollected<ProcessingInstruction>(GetDocument(), "A", "B");
  GetDocument().body()->appendChild(pi, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
}

TEST_F(NodeTest, appendChildCommentNoStyleRecalc) {
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  Comment* comment = Comment::Create(GetDocument(), "comment");
  GetDocument().body()->appendChild(comment, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
}

TEST_F(NodeTest, MutationOutsideFlatTreeStyleDirty) {
  SetBodyContent("<div id=host><span id=nonslotted></span></div>");
  GetDocument().getElementById("host")->AttachShadowRootInternal(
      ShadowRootType::kOpen);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument()
      .getElementById("nonslotted")
      ->setAttribute("style", "color:green");
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(NodeTest, SkipStyleDirtyHostChild) {
  SetBodyContent("<div id=host><span></span></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<div style='display:none'><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Check that we do not mark an element for style recalc when the element and
  // its flat tree parent are display:none.
  To<Element>(host->firstChild())->setAttribute("style", "color:green");
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(NodeTest, ContainsChild) {
  SetBodyContent("<div id=a><div id=b></div></div>");
  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");
  EXPECT_TRUE(a->contains(b));
}

TEST_F(NodeTest, ContainsNoSibling) {
  SetBodyContent("<div id=a></div><div id=b></div>");
  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");
  EXPECT_FALSE(a->contains(b));
}

TEST_F(NodeTest, ContainsPseudo) {
  SetBodyContent(
      "<style>#a::before{content:'aaa';}</style>"
      "<div id=a></div>");
  Element* a = GetDocument().getElementById("a");
  PseudoElement* pseudo = a->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(pseudo);
  EXPECT_TRUE(a->contains(pseudo));
}

TEST_F(NodeTest, SkipForceReattachDisplayNone) {
  SetBodyContent("<div id=host><span style='display:none'></span></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<slot name='target'></slot>");
  UpdateAllLifecyclePhasesForTest();

  Element* span = To<Element>(host->firstChild());
  span->setAttribute(html_names::kSlotAttr, "target");
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();

  // Node::FlatTreeParentChanged for a display:none could trigger style recalc,
  // but we should skip a forced re-attach for nodes with a null ComputedStyle.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(span->NeedsStyleRecalc());
  EXPECT_FALSE(span->GetForceReattachLayoutTree());
}

TEST_F(NodeTest, UpdateChildDirtyAncestorsOnSlotAssignment) {
  SetBodyContent("<div id=host><span></span></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(
      "<div><slot></slot></div><div id='child-dirty'><slot "
      "name='target'></slot></div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  auto* span = To<Element>(host->firstChild());
  auto* ancestor = shadow_root.getElementById("child-dirty");

  // Make sure the span is dirty before the re-assignment.
  span->setAttribute("style", "color:green");
  EXPECT_FALSE(ancestor->ChildNeedsStyleRecalc());

  // Re-assign to second slot.
  span->setAttribute(html_names::kSlotAttr, "target");
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_TRUE(ancestor->ChildNeedsStyleRecalc());
}

TEST_F(NodeTest, UpdateChildDirtySlotAfterRemoval) {
  SetBodyContent("<div id=host><span></span></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<slot></slot>");
  UpdateAllLifecyclePhasesForTest();

  auto* span = To<Element>(host->firstChild());
  auto* slot = shadow_root.firstChild();

  // Make sure the span is dirty, and the slot marked child-dirty before the
  // removal.
  span->setAttribute("style", "color:green");
  EXPECT_TRUE(span->NeedsStyleRecalc());
  EXPECT_TRUE(slot->ChildNeedsStyleRecalc());
  EXPECT_TRUE(host->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsStyleRecalc());

  // The StyleRecalcRoot is now the span. Removing the span should clear the
  // root and the child-dirty bits on the ancestors.
  span->remove();

  EXPECT_FALSE(slot->ChildNeedsStyleRecalc());
  EXPECT_FALSE(host->ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().GetStyleEngine().NeedsStyleRecalc());
}

TEST_F(NodeTest, UpdateChildDirtyAfterSlotRemoval) {
  SetBodyContent("<div id=host><span></span></div>");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<div><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  auto* span = To<Element>(host->firstChild());
  auto* div = shadow_root.firstChild();
  auto* slot = div->firstChild();

  // Make sure the span is dirty, and the slot marked child-dirty before the
  // removal.
  span->setAttribute("style", "color:green");
  EXPECT_TRUE(span->NeedsStyleRecalc());
  EXPECT_TRUE(slot->ChildNeedsStyleRecalc());
  EXPECT_TRUE(div->ChildNeedsStyleRecalc());
  EXPECT_TRUE(host->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsStyleRecalc());

  // The StyleRecalcRoot is now the span. Removing the slot would break the flat
  // tree ancestor chain so that when removing the span we would no longer be
  // able to clear the dirty bits for all of the previous ancestor chain. Thus,
  // we fall back to use the host as the style recalc root to be able to
  // traverse and clear the dirty bit of the shadow tree div element on the next
  // style recalc.
  slot->remove();
  span->remove();

  EXPECT_TRUE(div->ChildNeedsStyleRecalc());
  EXPECT_TRUE(host->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsStyleRecalc());
}

TEST_F(NodeTest, UpdateChildDirtyAfterSlottingDirtyNode) {
  SetBodyContent("<div id=host><span></span></div>");

  auto* host = GetDocument().getElementById("host");
  auto* span = To<Element>(host->firstChild());

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<div><slot name=x></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  // Make sure the span is style dirty.
  span->setAttribute("style", "color:green");

  // Assign span to slot.
  span->setAttribute("slot", "x");

  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();

  // Make sure shadow tree div and slot are marked with ChildNeedsStyleRecalc
  // when the dirty span is slotted in.
  EXPECT_TRUE(shadow_root.firstChild()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(shadow_root.firstChild()->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(span->NeedsStyleRecalc());

  // This used to call a DCHECK failure. Make sure we don't regress.
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(NodeTest, ChildDirtyNeedsV0Distribution) {
  SetBodyContent("<div id=host><span></span> </div>");
  ShadowRoot* shadow_root = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), "host", "<content />");
  UpdateAllLifecyclePhasesForTest();

#if DCHECK_IS_ON()
  GetDocument().SetAllowDirtyShadowV0Traversal(true);
#endif

  auto* host = GetDocument().getElementById("host");
  auto* span = To<Element>(host->firstChild());
  auto* content = shadow_root->firstChild();

  host->lastChild()->remove();

  EXPECT_FALSE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().documentElement()->ChildNeedsDistributionRecalc());
  EXPECT_EQ(content, host->firstChild()->GetStyleRecalcParent());
  EXPECT_FALSE(content->ChildNeedsStyleRecalc());

  // Make the span style dirty.
  span->setAttribute("style", "color:green");

  // Check that the flat tree ancestor chain is child-dirty while the
  // shadow distribution is still dirty.
  EXPECT_TRUE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(host->ChildNeedsStyleRecalc());
  EXPECT_TRUE(content->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().documentElement()->ChildNeedsDistributionRecalc());

#if DCHECK_IS_ON()
  GetDocument().SetAllowDirtyShadowV0Traversal(false);
#endif
}

}  // namespace blink
