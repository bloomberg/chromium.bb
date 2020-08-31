// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/file_input_type.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class HTMLInputElementTest : public PageTestBase {
 protected:
  HTMLInputElement& TestElement() {
    Element* element = GetDocument().getElementById("test");
    DCHECK(element);
    return To<HTMLInputElement>(*element);
  }
};

TEST_F(HTMLInputElementTest, FilteredDataListOptionsNoList) {
  GetDocument().documentElement()->setInnerHTML("<input id=test>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().IsEmpty());

  GetDocument().documentElement()->setInnerHTML(
      "<input id=test list=dl1><datalist id=dl1></datalist>");
  EXPECT_TRUE(TestElement().FilteredDataListOptions().IsEmpty());
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsContain) {
  GetDocument().documentElement()->setInnerHTML(
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

  GetDocument().documentElement()->setInnerHTML(
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
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <input id=test value='foo@example.com, tkent' list=dl3 type=email
    multiple>
    <datalist id=dl3>
    <option>keishi@chromium.org</option>
    <option>tkent@chromium.org</option>
    </datalist>
  )HTML");
  auto options = TestElement().FilteredDataListOptions();
  EXPECT_EQ(1u, options.size());
  EXPECT_EQ("tkent@chromium.org", options[0]->value().Utf8());
}

TEST_F(HTMLInputElementTest, create) {
  auto* input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
  EXPECT_NE(nullptr, input->UserAgentShadowRoot());

  input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByParser());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  input->ParserSetAttributes(Vector<Attribute>());
  EXPECT_NE(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, NoAssertWhenMovedInNewDocument) {
  auto* document_without_frame = MakeGarbageCollected<Document>();
  EXPECT_EQ(nullptr, document_without_frame->GetPage());
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document_without_frame);
  html->AppendChild(
      MakeGarbageCollected<HTMLBodyElement>(*document_without_frame));

  // Create an input element with type "range" inside a document without frame.
  To<HTMLBodyElement>(html->firstChild())
      ->setInnerHTML("<input type='range' />");
  document_without_frame->AppendChild(html);

  auto page_holder = std::make_unique<DummyPageHolder>();
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
  auto* input_without_form = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags());
  input_without_form->SetBooleanAttribute(html_names::kRequiredAttr, true);
  GetDocument().body()->AppendChild(input_without_form);
  EXPECT_EQ("<<ValidationValueMissing>>", input_without_form->DefaultToolTip());

  auto* form = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  GetDocument().body()->AppendChild(form);
  auto* input_with_form = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags());
  input_with_form->SetBooleanAttribute(html_names::kRequiredAttr, true);
  form->AppendChild(input_with_form);
  EXPECT_EQ("<<ValidationValueMissing>>", input_with_form->DefaultToolTip());

  form->SetBooleanAttribute(html_names::kNovalidateAttr, true);
  EXPECT_EQ(String(), input_with_form->DefaultToolTip());
}

// crbug.com/589838
TEST_F(HTMLInputElementTest, ImageTypeCrash) {
  auto* input = MakeGarbageCollected<HTMLInputElement>(GetDocument(),
                                                       CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, "image");
  input->EnsureFallbackContent();
  // Make sure ensurePrimaryContent() recreates UA shadow tree, and updating
  // |value| doesn't crash.
  input->EnsurePrimaryContent();
  input->setAttribute(html_names::kValueAttr, "aaa");
}

TEST_F(HTMLInputElementTest, RadioKeyDownDCHECKFailure) {
  // crbug.com/697286
  GetDocument().body()->setInnerHTML(
      "<input type=radio name=g><input type=radio name=g>");
  auto& radio1 = To<HTMLInputElement>(*GetDocument().body()->firstChild());
  auto& radio2 = To<HTMLInputElement>(*radio1.nextSibling());
  radio1.focus();
  // Make layout-dirty.
  radio2.setAttribute(html_names::kStyleAttr, "position:fixed");
  KeyboardEventInit* init = KeyboardEventInit::Create();
  init->setKey("ArrowRight");
  radio1.DefaultEventHandler(
      *MakeGarbageCollected<KeyboardEvent>("keydown", init));
  EXPECT_EQ(GetDocument().ActiveElement(), &radio2);
}

TEST_F(HTMLInputElementTest, DateTimeChooserSizeParamRespectsScale) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  GetDocument().View()->GetFrame().GetPage()->GetVisualViewport().SetScale(2.f);
  GetDocument().body()->setInnerHTML(
      "<input type='date' style='width:200px;height:50px' />");
  UpdateAllLifecyclePhasesForTest();
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());

  DateTimeChooserParameters params;
  bool success = input->SetupDateTimeChooserParameters(params);
  EXPECT_TRUE(success);
  EXPECT_EQ("date", params.type);
  EXPECT_EQ(IntRect(16, 16, 400, 100), params.anchor_rect_in_screen);
}

TEST_F(HTMLInputElementTest, StepDownOverflow) {
  auto* input = MakeGarbageCollected<HTMLInputElement>(GetDocument(),
                                                       CreateElementFlags());
  input->setAttribute(html_names::kTypeAttr, "date");
  input->setAttribute(html_names::kMinAttr, "2010-02-10");
  input->setAttribute(html_names::kStepAttr, "9223372036854775556");
  // InputType::applyStep() should not pass an out-of-range value to
  // setValueAsDecimal, and WTF::msToYear() should not cause a DCHECK failure.
  input->stepDown(1, ASSERT_NO_EXCEPTION);
}

TEST_F(HTMLInputElementTest, CheckboxHasNoShadowRoot) {
  GetDocument().body()->setInnerHTML("<input type='checkbox' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, ChangingInputTypeCausesShadowRootToBeCreated) {
  GetDocument().body()->setInnerHTML("<input type='checkbox' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  EXPECT_EQ(nullptr, input->UserAgentShadowRoot());
  input->setAttribute(html_names::kTypeAttr, "text");
  EXPECT_NE(nullptr, input->UserAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, RepaintAfterClearingFile) {
  GetDocument().body()->setInnerHTML("<input type='file' />");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());

  FileChooserFileInfoList files;
  files.push_back(CreateFileChooserFileInfoNative("/native/path/native-file",
                                                  "display-name"));
  FileList* list = FileInputType::CreateFileList(files, base::FilePath());
  ASSERT_TRUE(list);
  EXPECT_EQ(1u, list->length());

  input->setFiles(list);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(input->GetLayoutObject());
  EXPECT_FALSE(input->GetLayoutObject()->ShouldCheckForPaintInvalidation());

  input->setValue("");
  GetDocument().UpdateStyleAndLayoutTree();

  ASSERT_TRUE(input->GetLayoutObject());
  EXPECT_TRUE(input->GetLayoutObject()->ShouldCheckForPaintInvalidation());
}

TEST_F(HTMLInputElementTest, UpdateTypeDcheck) {
  Document& doc = GetDocument();
  // Removing <body> is required to reproduce the issue.
  doc.body()->remove();
  Element* input = doc.CreateRawElement(html_names::kInputTag);
  doc.documentElement()->appendChild(input);
  input->focus();
  input->setAttribute(html_names::kTypeAttr, AtomicString("radio"));
  // Test succeeds if the above setAttribute() didn't trigger a DCHECK failure
  // in Document::UpdateFocusAppearanceAfterLayout().
}

}  // namespace blink
