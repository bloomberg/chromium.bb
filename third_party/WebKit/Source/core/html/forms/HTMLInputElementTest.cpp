// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/HTMLInputElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/KeyboardEventInit.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/HTMLOptionElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLInputElementTest : public ::testing::Test {
 protected:
  Document& GetDocument() { return page_holder_->GetDocument(); }
  HTMLInputElement& TestElement() {
    Element* element = GetDocument().getElementById("test");
    DCHECK(element);
    return ToHTMLInputElement(*element);
  }

 private:
  void SetUp() override {
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }

  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(HTMLInputElementTest, FilteredDataListOptionsNoList) {
  GetDocument().documentElement()->SetInnerHTMLFromString("<input id=test>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().IsEmpty());

  GetDocument().documentElement()->SetInnerHTMLFromString(
      "<input id=test list=dl1><datalist id=dl1></datalist>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().IsEmpty());
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsContain) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      "<input id=test value=BC list=dl2>"
      "<datalist id=dl2>"
      "<option>AbC DEF</option>"
      "<option>VAX</option>"
      "<option value=ghi>abc</option>"  // Match to label, not value.
      "</datalist>");
  auto options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(2u, options.size());
  EXPECT_EQ("AbC DEF", options[0]->value().Utf8());
  EXPECT_EQ("ghi", options[1]->value().Utf8());

  GetDocument().documentElement()->SetInnerHTMLFromString(
      "<input id=test value=i list=dl2>"
      "<datalist id=dl2>"
      "<option>I</option>"
      "<option>&#x0130;</option>"  // LATIN CAPITAL LETTER I WITH DOT ABOVE
      "<option>&#xFF49;</option>"  // FULLWIDTH LATIN SMALL LETTER I
      "</datalist>");
  options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(2u, options.size());
  EXPECT_EQ("I", options[0]->value().Utf8());
  EXPECT_EQ(0x0130, options[1]->value()[0]);
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsForMultipleEmail) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      "<input id=test value='foo@example.com, tkent' list=dl3 type=email "
      "multiple>"
      "<datalist id=dl3>"
      "<option>keishi@chromium.org</option>"
      "<option>tkent@chromium.org</option>"
      "</datalist>");
  auto options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(1u, options.size());
  EXPECT_EQ("tkent@chromium.org", options[0]->value().Utf8());
}

TEST_F(HTMLInputElementTest, create) {
  HTMLInputElement* input =
      HTMLInputElement::Create(GetDocument(), /* createdByParser */ false);
  EXPECT_NE(nullptr, input->UserAgentShadowRoot());

  input = HTMLInputElement::Create(GetDocument(), /* createdByParser */ true);
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  input->ParserSetAttributes(Vector<Attribute>());
  EXPECT_NE(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, NoAssertWhenMovedInNewDocument) {
  Document* document_without_frame = Document::CreateForTest();
  EXPECT_EQ(nullptr, document_without_frame->GetPage());
  HTMLHtmlElement* html = HTMLHtmlElement::Create(*document_without_frame);
  html->AppendChild(HTMLBodyElement::Create(*document_without_frame));

  // Create an input element with type "range" inside a document without frame.
  ToHTMLBodyElement(html->firstChild())
      ->SetInnerHTMLFromString("<input type='range' />");
  document_without_frame->AppendChild(html);

  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create();
  auto& document = page_holder->GetDocument();
  EXPECT_NE(nullptr, document.GetPage());

  // Put the input element inside a document with frame.
  document.body()->AppendChild(document_without_frame->body()->firstChild());

  // Remove the input element and all refs to it so it gets deleted before the
  // document.
  // The assert in |EventHandlerRegistry::updateEventHandlerTargets()| should
  // not be triggered.
  document.body()->RemoveChild(document.body()->firstChild());
}

TEST_F(HTMLInputElementTest, DefaultToolTip) {
  HTMLInputElement* input_without_form =
      HTMLInputElement::Create(GetDocument(), false);
  input_without_form->SetBooleanAttribute(HTMLNames::requiredAttr, true);
  GetDocument().body()->AppendChild(input_without_form);
  EXPECT_EQ("<<ValidationValueMissing>>", input_without_form->DefaultToolTip());

  HTMLFormElement* form = HTMLFormElement::Create(GetDocument());
  GetDocument().body()->AppendChild(form);
  HTMLInputElement* input_with_form =
      HTMLInputElement::Create(GetDocument(), false);
  input_with_form->SetBooleanAttribute(HTMLNames::requiredAttr, true);
  form->AppendChild(input_with_form);
  EXPECT_EQ("<<ValidationValueMissing>>", input_with_form->DefaultToolTip());

  form->SetBooleanAttribute(HTMLNames::novalidateAttr, true);
  EXPECT_EQ(String(), input_with_form->DefaultToolTip());
}

// crbug.com/589838
TEST_F(HTMLInputElementTest, ImageTypeCrash) {
  HTMLInputElement* input = HTMLInputElement::Create(GetDocument(), false);
  input->setAttribute(HTMLNames::typeAttr, "image");
  input->EnsureFallbackContent();
  // Make sure ensurePrimaryContent() recreates UA shadow tree, and updating
  // |value| doesn't crash.
  input->EnsurePrimaryContent();
  input->setAttribute(HTMLNames::valueAttr, "aaa");
}

TEST_F(HTMLInputElementTest, RadioKeyDownDCHECKFailure) {
  // crbug.com/697286
  GetDocument().body()->SetInnerHTMLFromString(
      "<input type=radio name=g><input type=radio name=g>");
  HTMLInputElement& radio1 =
      ToHTMLInputElement(*GetDocument().body()->firstChild());
  HTMLInputElement& radio2 = ToHTMLInputElement(*radio1.nextSibling());
  radio1.focus();
  // Make layout-dirty.
  radio2.setAttribute(HTMLNames::styleAttr, "position:fixed");
  KeyboardEventInit init;
  init.setKey("ArrowRight");
  radio1.DefaultEventHandler(new KeyboardEvent("keydown", init));
  EXPECT_EQ(GetDocument().ActiveElement(), &radio2);
}

TEST_F(HTMLInputElementTest, DateTimeChooserSizeParamRespectsScale) {
  GetDocument().View()->GetFrame().GetPage()->GetVisualViewport().SetScale(2.f);
  GetDocument().body()->SetInnerHTMLFromString(
      "<input type='date' style='width:200px;height:50px' />");
  GetDocument().View()->UpdateAllLifecyclePhases();
  HTMLInputElement* input =
      ToHTMLInputElement(GetDocument().body()->firstChild());

  DateTimeChooserParameters params;
  bool success = input->SetupDateTimeChooserParameters(params);
  EXPECT_TRUE(success);
  EXPECT_EQ("date", params.type);
  EXPECT_EQ(IntRect(16, 16, 400, 100), params.anchor_rect_in_screen);
}

TEST_F(HTMLInputElementTest, StepDownOverflow) {
  HTMLInputElement* input = HTMLInputElement::Create(GetDocument(), false);
  input->setAttribute(HTMLNames::typeAttr, "date");
  input->setAttribute(HTMLNames::minAttr, "2010-02-10");
  input->setAttribute(HTMLNames::stepAttr, "9223372036854775556");
  // InputType::applyStep() should not pass an out-of-range value to
  // setValueAsDecimal, and WTF::msToYear() should not cause a DCHECK failure.
  input->stepDown(1, ASSERT_NO_EXCEPTION);
}

}  // namespace blink
