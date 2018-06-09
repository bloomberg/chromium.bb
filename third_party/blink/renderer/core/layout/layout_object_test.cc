// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using testing::Return;

class LayoutObjectTest : public RenderingTest {
 public:
  LayoutObjectTest() : RenderingTest(EmptyLocalFrameClient::Create()) {}

 protected:
  template <bool should_have_wrapper>
  void ExpectAnonymousInlineWrapperFor(Node*);
};

template <bool should_have_wrapper>
void LayoutObjectTest::ExpectAnonymousInlineWrapperFor(Node* node) {
  ASSERT_TRUE(node);
  EXPECT_TRUE(node->IsTextNode());
  LayoutObject* text_layout = node->GetLayoutObject();
  ASSERT_TRUE(text_layout);
  LayoutObject* text_parent = text_layout->Parent();
  ASSERT_TRUE(text_parent);
  if (should_have_wrapper) {
    EXPECT_TRUE(text_parent->IsAnonymous());
    EXPECT_TRUE(text_parent->IsInline());
  } else {
    EXPECT_FALSE(text_parent->IsAnonymous());
  }
}

TEST_F(LayoutObjectTest, LayoutDecoratedNameCalledWithPositionedObject) {
  SetBodyInnerHTML("<div id='div' style='position: fixed'>test</div>");
  Element* div = GetDocument().getElementById(AtomicString("div"));
  DCHECK(div);
  LayoutObject* obj = div->GetLayoutObject();
  DCHECK(obj);
  EXPECT_STREQ("LayoutBlockFlow (positioned)",
               obj->DecoratedName().Ascii().data());
}

// Some display checks.
TEST_F(LayoutObjectTest, DisplayNoneCreateObject) {
  SetBodyInnerHTML("<div style='display:none'></div>");
  EXPECT_EQ(nullptr, GetDocument().body()->firstChild()->GetLayoutObject());
}

TEST_F(LayoutObjectTest, DisplayBlockCreateObject) {
  SetBodyInnerHTML("<foo style='display:block'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->firstChild()->GetLayoutObject();
  EXPECT_NE(nullptr, layout_object);
  EXPECT_TRUE(layout_object->IsLayoutBlockFlow());
  EXPECT_FALSE(layout_object->IsInline());
}

TEST_F(LayoutObjectTest, DisplayInlineBlockCreateObject) {
  SetBodyInnerHTML("<foo style='display:inline-block'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->firstChild()->GetLayoutObject();
  EXPECT_NE(nullptr, layout_object);
  EXPECT_TRUE(layout_object->IsLayoutBlockFlow());
  EXPECT_TRUE(layout_object->IsInline());
}

// Containing block test.
TEST_F(LayoutObjectTest, ContainingBlockLayoutViewShouldBeNull) {
  EXPECT_EQ(nullptr, GetLayoutView().ContainingBlock());
}

TEST_F(LayoutObjectTest, ContainingBlockBodyShouldBeDocumentElement) {
  EXPECT_EQ(GetDocument().body()->GetLayoutObject()->ContainingBlock(),
            GetDocument().documentElement()->GetLayoutObject());
}

TEST_F(LayoutObjectTest, ContainingBlockDocumentElementShouldBeLayoutView) {
  EXPECT_EQ(
      GetDocument().documentElement()->GetLayoutObject()->ContainingBlock(),
      GetLayoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockStaticLayoutObjectShouldBeParent) {
  SetBodyInnerHTML("<foo style='position:static'></foo>");
  LayoutObject* body_layout_object = GetDocument().body()->GetLayoutObject();
  LayoutObject* layout_object = body_layout_object->SlowFirstChild();
  EXPECT_EQ(layout_object->ContainingBlock(), body_layout_object);
}

TEST_F(LayoutObjectTest,
       ContainingBlockAbsoluteLayoutObjectShouldBeLayoutView) {
  SetBodyInnerHTML("<foo style='position:absolute'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  EXPECT_EQ(layout_object->ContainingBlock(), GetLayoutView());
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldBeNonStaticallyPositionedBlockAncestor) {
  SetBodyInnerHTML(R"HTML(
    <div style='position:relative; left:20px'>
      <bar style='position:absolute; left:2px; top:10px'></bar>
    </div>
  )HTML");
  LayoutObject* containing_blocklayout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  LayoutObject* layout_object = containing_blocklayout_object->SlowFirstChild();
  EXPECT_TRUE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kAbsolute));
  EXPECT_FALSE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlock(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
  auto offset =
      layout_object->OffsetFromContainer(containing_blocklayout_object);
  EXPECT_EQ(offset.Width().ToInt(), 2);
  EXPECT_EQ(offset.Height().ToInt(), 10);
}

TEST_F(LayoutObjectTest, ContainingBlockFixedLayoutObjectInTransformedDiv) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform:translateX(0px)'>
      <bar style='position:fixed'></bar>
    </div>
  )HTML");
  LayoutObject* containing_blocklayout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  LayoutObject* layout_object = containing_blocklayout_object->SlowFirstChild();
  EXPECT_TRUE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kAbsolute));
  EXPECT_TRUE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlock(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(),
            containing_blocklayout_object);
}

TEST_F(LayoutObjectTest, ContainingBlockFixedLayoutObjectInBody) {
  SetBodyInnerHTML("<div style='position:fixed'></div>");
  LayoutObject* layout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  EXPECT_TRUE(layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kAbsolute));
  EXPECT_FALSE(
      layout_object->CanContainOutOfFlowPositionedElement(EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlock(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockAbsoluteLayoutObjectInBody) {
  SetBodyInnerHTML("<div style='position:absolute'></div>");
  LayoutObject* layout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  EXPECT_TRUE(layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kAbsolute));
  EXPECT_FALSE(
      layout_object->CanContainOutOfFlowPositionedElement(EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlock(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldNotBeNonStaticallyPositionedInlineAncestor) {
  // Test note: We can't use a raw string literal here, since extra whitespace
  // causes failures.
  SetBodyInnerHTML(
      "<span style='position:relative; top:1px; left:2px'><bar "
      "style='position:absolute; top:10px; left:20px;'></bar></span>");
  LayoutObject* body_layout_object = GetDocument().body()->GetLayoutObject();
  LayoutObject* span_layout_object = body_layout_object->SlowFirstChild();
  LayoutObject* layout_object = span_layout_object->SlowFirstChild();

  EXPECT_TRUE(span_layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kAbsolute));
  EXPECT_FALSE(span_layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kFixed));

  auto offset = layout_object->OffsetFromContainer(span_layout_object);
  EXPECT_EQ(offset.Width().ToInt(), 20);
  EXPECT_EQ(offset.Height().ToInt(), 10);

  // Sanity check: Make sure we don't generate anonymous objects.
  EXPECT_EQ(nullptr, body_layout_object->SlowFirstChild()->NextSibling());
  EXPECT_EQ(nullptr, layout_object->SlowFirstChild());
  EXPECT_EQ(nullptr, layout_object->NextSibling());

  EXPECT_EQ(layout_object->Container(), span_layout_object);
  EXPECT_EQ(layout_object->ContainingBlock(), body_layout_object);
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            body_layout_object);
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
}

TEST_F(LayoutObjectTest, PaintingLayerOfOverflowClipLayerUnderColumnSpanAll) {
  SetBodyInnerHTML(R"HTML(
    <div id='columns' style='columns: 3'>
      <div style='column-span: all'>
        <div id='overflow-clip-layer' style='height: 100px; overflow:
    hidden'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* overflow_clip_object =
      GetLayoutObjectByElementId("overflow-clip-layer");
  LayoutBlock* columns = ToLayoutBlock(GetLayoutObjectByElementId("columns"));
  EXPECT_EQ(columns->Layer(), overflow_clip_object->PaintingLayer());
}

TEST_F(LayoutObjectTest, FloatUnderBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id='layered-div' style='position: absolute'>
      <div id='container'>
        <div id='floating' style='float: left'>FLOAT</div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject* layered_div =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("layered-div"));
  LayoutBoxModelObject* container =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("container"));
  LayoutObject* floating = GetLayoutObjectByElementId("floating");

  EXPECT_EQ(layered_div->Layer(), layered_div->PaintingLayer());
  EXPECT_EQ(layered_div->Layer(), floating->PaintingLayer());
  EXPECT_EQ(container, floating->Container());
  EXPECT_EQ(container, floating->ContainingBlock());
}

TEST_F(LayoutObjectTest, FloatUnderInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='layered-div' style='position: absolute'>
      <div id='container'>
        <span id='layered-span' style='position: relative'>
          <div id='floating' style='float: left'>FLOAT</div>
        </span>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject* layered_div =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("layered-div"));
  LayoutBoxModelObject* container =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("container"));
  LayoutBoxModelObject* layered_span =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("layered-span"));
  LayoutObject* floating = GetLayoutObjectByElementId("floating");

  EXPECT_EQ(layered_div->Layer(), layered_div->PaintingLayer());
  EXPECT_EQ(layered_span->Layer(), layered_span->PaintingLayer());
  EXPECT_EQ(layered_div->Layer(), floating->PaintingLayer());
  EXPECT_EQ(container, floating->Container());
  EXPECT_EQ(container, floating->ContainingBlock());

  LayoutObject::AncestorSkipInfo skip_info(layered_span);
  EXPECT_EQ(container, floating->Container(&skip_info));
  EXPECT_TRUE(skip_info.AncestorSkipped());

  skip_info = LayoutObject::AncestorSkipInfo(container);
  EXPECT_EQ(container, floating->Container(&skip_info));
  EXPECT_FALSE(skip_info.AncestorSkipped());
}

TEST_F(LayoutObjectTest, MutableForPaintingClearPaintFlags) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  object->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->MayNeedPaintInvalidation());
  object->SetMayNeedPaintInvalidationSubtree();
  EXPECT_TRUE(object->MayNeedPaintInvalidationSubtree());
  object->SetMayNeedPaintInvalidationAnimatedBackgroundImage();
  EXPECT_TRUE(object->MayNeedPaintInvalidationAnimatedBackgroundImage());
  object->SetShouldInvalidateSelection();
  EXPECT_TRUE(object->ShouldInvalidateSelection());
  object->SetBackgroundChangedSinceLastPaintInvalidation();
  EXPECT_TRUE(object->BackgroundChangedSinceLastPaintInvalidation());
  object->SetNeedsPaintPropertyUpdate();
  EXPECT_TRUE(object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->Parent()->DescendantNeedsPaintPropertyUpdate());
  object->bitfields_.SetDescendantNeedsPaintPropertyUpdate(true);
  EXPECT_TRUE(object->DescendantNeedsPaintPropertyUpdate());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
  object->GetMutableForPainting().ClearPaintFlags();

  EXPECT_FALSE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->MayNeedPaintInvalidationSubtree());
  EXPECT_FALSE(object->MayNeedPaintInvalidationAnimatedBackgroundImage());
  EXPECT_FALSE(object->ShouldInvalidateSelection());
  EXPECT_FALSE(object->BackgroundChangedSinceLastPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(object->DescendantNeedsPaintPropertyUpdate());
}

TEST_F(LayoutObjectTest, SubtreeNeedsPaintPropertyUpdate) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  object->SetSubtreeNeedsPaintPropertyUpdate();
  EXPECT_TRUE(object->SubtreeNeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->Parent()->DescendantNeedsPaintPropertyUpdate());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
  object->GetMutableForPainting().ClearPaintFlags();

  EXPECT_FALSE(object->SubtreeNeedsPaintPropertyUpdate());
  EXPECT_FALSE(object->NeedsPaintPropertyUpdate());
}

TEST_F(LayoutObjectTest, NeedsPaintOffsetAndVisualRectUpdate) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  LayoutObject* parent = object->Parent();

  object->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());

  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->MayNeedPaintInvalidation());
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());

  object->SetShouldDoFullPaintInvalidationWithoutGeometryChange();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());

  object->SetMayNeedPaintInvalidationWithoutGeometryChange();
  EXPECT_TRUE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());
}

TEST_F(LayoutObjectTest, AssociatedLayoutObjectOfFirstLetterPunctuations) {
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>(a)bc</p>";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_EQ(layout_object0, layout_object1)
      << "A character 'a' should be part of first letter.";

  const LayoutTextFragment* layout_object2 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object0, layout_object2)
      << "close parenthesis should be part of first letter.";

  const LayoutTextFragment* layout_object3 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 3));
  EXPECT_TRUE(layout_object3->IsRemainingTextLayoutObject());
}

TEST_F(LayoutObjectTest, AssociatedLayoutObjectOfFirstLetterSplit) {
  V8TestingScope scope;

  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* first_letter = sample->firstChild();
  // Split "abc" into "a" "bc"
  ToText(first_letter)->splitText(1, ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  const LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*first_letter, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*first_letter, 1));
  EXPECT_EQ(layout_object0, layout_object1);
}

TEST_F(LayoutObjectTest,
       AssociatedLayoutObjectOfFirstLetterWithTrailingWhitespace) {
  const char* body_content = R"HTML(
    <style>
      div:first-letter {
        color:red;
      }
    </style>
    <div id=sample>a
      <div></div>
    </div>
  )HTML";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_TRUE(layout_object1->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object2 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object1, layout_object2);
}

TEST_F(LayoutObjectTest, VisualRect) {
  class MockLayoutObject : public LayoutObject {
   public:
    MockLayoutObject() : LayoutObject(nullptr) {}
    MOCK_CONST_METHOD0(VisualRectRespectsVisibility, bool());

   private:
    LayoutRect LocalVisualRectIgnoringVisibility() const override {
      return LayoutRect(10, 10, 20, 20);
    }
    const char* GetName() const final { return "MockLayoutObject"; }
    void UpdateLayout() final {}
    FloatRect LocalBoundingBoxRectForAccessibility() const final {
      return FloatRect();
    }
  };

  MockLayoutObject mock_object;
  auto style = ComputedStyle::Create();
  mock_object.SetStyle(style.get());
  EXPECT_EQ(LayoutRect(10, 10, 20, 20), mock_object.LocalVisualRect());
  EXPECT_EQ(LayoutRect(10, 10, 20, 20), mock_object.LocalVisualRect());

  style->SetVisibility(EVisibility::kHidden);
  EXPECT_CALL(mock_object, VisualRectRespectsVisibility())
      .WillOnce(Return(true));
  EXPECT_EQ(LayoutRect(), mock_object.LocalVisualRect());
  EXPECT_CALL(mock_object, VisualRectRespectsVisibility())
      .WillOnce(Return(false));
  EXPECT_EQ(LayoutRect(10, 10, 20, 20), mock_object.LocalVisualRect());
}

TEST_F(LayoutObjectTest, DisplayContentsInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents;color:pink'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<true>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsNoInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<false>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsAddInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<false>(text);

  div->SetInlineStyleProperty(CSSPropertyColor, "pink");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ExpectAnonymousInlineWrapperFor<true>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsRemoveInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents;color:pink'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<true>(text);

  div->RemoveInlineStyleProperty(CSSPropertyColor);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ExpectAnonymousInlineWrapperFor<false>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperPerTextNode) {
  // This test checks the current implementation; that text node siblings do not
  // share inline wrappers. Doing so requires code to handle all situations
  // where text nodes are no longer layout tree siblings by splitting wrappers,
  // and merge wrappers when text nodes become layout tree siblings.
  SetBodyInnerHTML(
      "<div id='div' style='display:contents;color:pink'>A<!-- -->B</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text1 = div->firstChild();
  ASSERT_TRUE(text1);
  Node* text2 = div->lastChild();
  ASSERT_TRUE(text2);
  EXPECT_NE(text1, text2);

  ExpectAnonymousInlineWrapperFor<true>(text1);
  ExpectAnonymousInlineWrapperFor<true>(text2);

  EXPECT_NE(text1->GetLayoutObject()->Parent(),
            text2->GetLayoutObject()->Parent());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTable) {
  SetBodyInnerHTML(R"HTML(
    <div id='table' style='display:table'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableSection) {
  SetBodyInnerHTML(R"HTML(
    <div id='section' style='display:table-row-group'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableRow) {
  SetBodyInnerHTML(R"HTML(
    <div id='row' style='display:table-row'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableCell) {
  SetBodyInnerHTML(R"HTML(
    <div id='cell' style='display:table-cell'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* cell = GetDocument().getElementById("cell");
  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  EXPECT_EQ(cell->GetLayoutObject(), none->GetLayoutObject()->Parent());
}

TEST_F(LayoutObjectTest, DumpLayoutObject) {
  // Test dumping for debugging, in particular that newlines and non-ASCII
  // characters are escaped as expected.
  SetBodyInnerHTML(String::FromUTF8(R"HTML(
    <div id='block' style='background:
lime'>
      testing Среќен роденден
</div>
  )HTML"));

  LayoutObject* block = GetLayoutObjectByElementId("block");
  ASSERT_TRUE(block);
  LayoutObject* text = block->SlowFirstChild();
  ASSERT_TRUE(text);

  StringBuilder result;
  block->DumpLayoutObject(result, false, 0);
  EXPECT_EQ(
      result.ToString(),
      String("LayoutBlockFlow\tDIV id=\"block\" style=\"background:\\nlime\""));

  result.Clear();
  text->DumpLayoutObject(result, false, 0);
  EXPECT_EQ(
      result.ToString(),
      String("LayoutText\t#text \"\\n      testing "
             "\\u0421\\u0440\\u0435\\u045C\\u0435\\u043D "
             "\\u0440\\u043E\\u0434\\u0435\\u043D\\u0434\\u0435\\u043D\\n\""));
}

TEST_F(LayoutObjectTest, DisplayContentsSVGGElementInHTML) {
  SetBodyInnerHTML(R"HTML(
    <style>*|g { display:contents}</style>
    <span id=span></span>
  )HTML");

  Element* span = GetDocument().getElementById("span");
  SVGGElement* svg_element = SVGGElement::Create(GetDocument());
  Text* text = Text::Create(GetDocument(), "text");
  svg_element->appendChild(text);
  span->appendChild(svg_element);

  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_FALSE(svg_element->GetLayoutObject());
  ASSERT_FALSE(text->GetLayoutObject());
}

TEST_F(LayoutObjectTest, HasDistortingVisualEffects) {
  SetBodyInnerHTML(R"HTML(
    <div id=opaque style='opacity:1'><div class=inner></div></div>
    <div id=transparent style='opacity:0.99'><div class=inner></div></div>
    <div id=blurred style='filter:blur(5px)'><div class=inner></div></div>
    <div id=blended style='mix-blend-mode:hue'><div class=inner></div></div>
    <div id=good-transform style='transform:translateX(10px) scale(1.6)'>
      <div class=inner></div>
    </div>
    <div id=bad-transform style='transform:rotate(45deg)'>
      <div class=inner></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* outer = GetDocument().getElementById("opaque");
  Element* inner = outer->QuerySelector(".inner");
  ASSERT_FALSE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("transparent");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("blurred");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("blended");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("good-transform");
  inner = outer->QuerySelector(".inner");
  ASSERT_FALSE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("bad-transform");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());
}

class LayoutObjectSimTest : public SimTest {
 public:
  bool DocumentHasTouchActionRegion(const EventHandlerRegistry& registry) {
    GetDocument().View()->UpdateAllLifecyclePhases();
    return registry.HasEventHandlers(
        EventHandlerRegistry::EventHandlerClass::kTouchAction);
  }
};

TEST_F(LayoutObjectSimTest, TouchActionUpdatesSubframeEventHandler) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<div id='container'>"
      "<iframe src=frame.html></iframe>"
      "</div>");
  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<html><body>"
      "<div id='inner'></div>"
      "</body></html>");

  Element* iframe_element = GetDocument().QuerySelector("iframe");
  HTMLFrameOwnerElement* frame_owner_element =
      ToHTMLFrameOwnerElement(iframe_element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  Element* inner = iframe_doc->getElementById("inner");
  Element* iframe_doc_element = iframe_doc->documentElement();
  Element* container = GetDocument().getElementById("container");

  EventHandlerRegistry& registry =
      iframe_doc->GetFrame()->GetEventHandlerRegistry();

  // We should add event handler if touch action is set on subframe.
  inner->setAttribute("style", "touch-action: none");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should remove event handler if touch action is removed on subframe.
  inner->setAttribute("style", "touch-action: auto");
  EXPECT_FALSE(DocumentHasTouchActionRegion(registry));

  // We should add event handler if touch action is set on main frame.
  container->setAttribute("style", "touch-action: none");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should keep event handler if touch action is set on subframe document
  // element.
  iframe_doc_element->setAttribute("style", "touch-action: none");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should keep the event handler if touch action is removed on subframe
  // document element.
  iframe_doc_element->setAttribute("style", "touch-action: auto");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should remove the handler if touch action is removed on main frame.
  container->setAttribute("style", "touch-action: auto");
  EXPECT_FALSE(DocumentHasTouchActionRegion(registry));
}

}  // namespace blink
