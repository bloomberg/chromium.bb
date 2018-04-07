// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLSelectElementTest : public PageTestBase {
 protected:
  void SetUp() override;
};

void HTMLSelectElementTest::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  PageTestBase::SetupPageWithClients(&page_clients);
  GetDocument().SetMimeType("text/html");
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectSingleFormControlState) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML><select id='sel'>"
      "<option value='111' id='0'>111</option>"
      "<option value='222'>222</option>"
      "<option value='111' selected id='2'>!666</option>"
      "<option value='999'>999</option></select>");
  Element* element = GetElementById("sel");
  HTMLFormControlElementWithState* select = ToHTMLSelectElement(element);
  HTMLOptionElement* opt0 = ToHTMLOptionElement(GetElementById("0"));
  HTMLOptionElement* opt2 = ToHTMLOptionElement(GetElementById("2"));

  // Save the select element state, and then restore again.
  // Test passes if the restored state is not changed.
  EXPECT_EQ(2, ToHTMLSelectElement(element)->selectedIndex());
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
  FormControlState select_state = select->SaveFormControlState();
  EXPECT_EQ(2U, select_state.ValueSize());

  // Clear the selected state, to be restored by restoreFormControlState.
  ToHTMLSelectElement(select)->setSelectedIndex(-1);
  ASSERT_FALSE(opt2->Selected());

  // Restore
  select->RestoreFormControlState(select_state);
  EXPECT_EQ(2, ToHTMLSelectElement(element)->selectedIndex());
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectMultipleFormControlState) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML><select id='sel' multiple>"
      "<option value='111' id='0'>111</option>"
      "<option value='222'>222</option>"
      "<option value='111' selected id='2'>!666</option>"
      "<option value='999' selected id='3'>999</option></select>");
  HTMLFormControlElementWithState* select =
      ToHTMLSelectElement(GetElementById("sel"));

  HTMLOptionElement* opt0 = ToHTMLOptionElement(GetElementById("0"));
  HTMLOptionElement* opt2 = ToHTMLOptionElement(GetElementById("2"));
  HTMLOptionElement* opt3 = ToHTMLOptionElement(GetElementById("3"));

  // Save the select element state, and then restore again.
  // Test passes if the selected options are not changed.
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
  EXPECT_TRUE(opt3->Selected());
  FormControlState select_state = select->SaveFormControlState();
  EXPECT_EQ(4U, select_state.ValueSize());

  // Clear the selected state, to be restored by restoreFormControlState.
  opt2->SetSelected(false);
  opt3->SetSelected(false);
  ASSERT_FALSE(opt2->Selected());
  ASSERT_FALSE(opt3->Selected());

  // Restore
  select->RestoreFormControlState(select_state);
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
  EXPECT_TRUE(opt3->Selected());
}

TEST_F(HTMLSelectElementTest, RestoreUnmatchedFormControlState) {
  // We had a bug that selectedOption() and m_lastOnChangeOption were
  // mismatched in optionToBeShown(). It happened when
  // restoreFormControlState() couldn't find matched OPTIONs.
  // crbug.com/627833.

  SetHtmlInnerHTML(R"HTML(
    <select id='sel'>
    <option selected>Default</option>
    <option id='2'>222</option>
    </select>
  )HTML");
  Element* element = GetElementById("sel");
  HTMLFormControlElementWithState* select = ToHTMLSelectElement(element);
  HTMLOptionElement* opt2 = ToHTMLOptionElement(GetElementById("2"));

  ToHTMLSelectElement(element)->setSelectedIndex(1);
  // Save the current state.
  FormControlState select_state = select->SaveFormControlState();
  EXPECT_EQ(2U, select_state.ValueSize());

  // Reset the status.
  select->Reset();
  ASSERT_FALSE(opt2->Selected());
  element->RemoveChild(opt2);

  // Restore
  select->RestoreFormControlState(select_state);
  EXPECT_EQ(-1, ToHTMLSelectElement(element)->selectedIndex());
  EXPECT_EQ(nullptr, ToHTMLSelectElement(element)->OptionToBeShown());
}

TEST_F(HTMLSelectElementTest, VisibleBoundsInVisualViewport) {
  SetHtmlInnerHTML(
      "<select style='position:fixed; top:12.3px; height:24px; "
      "-webkit-appearance:none;'><option>o1</select>");
  HTMLSelectElement* select =
      ToHTMLSelectElement(GetDocument().body()->firstChild());
  ASSERT_NE(select, nullptr);
  IntRect bounds = select->VisibleBoundsInVisualViewport();
  EXPECT_EQ(24, bounds.Height());
}

TEST_F(HTMLSelectElementTest, PopupIsVisible) {
  SetHtmlInnerHTML("<select><option>o1</option></select>");
  HTMLSelectElement* select =
      ToHTMLSelectElement(GetDocument().body()->firstChild());
  ASSERT_NE(select, nullptr);
  EXPECT_FALSE(select->PopupIsVisible());
  select->ShowPopup();
  EXPECT_TRUE(select->PopupIsVisible());
  GetDocument().Shutdown();
  EXPECT_FALSE(select->PopupIsVisible());
}

TEST_F(HTMLSelectElementTest, FirstSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, select->FirstSelectableOption());
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->FirstSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 disabled></option><option "
        "id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->FirstSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 style='display:none'></option><option "
        "id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->FirstSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->FirstSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
}

TEST_F(HTMLSelectElementTest, LastSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, select->LastSelectableOption());
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->LastSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "disabled></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->LastSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "style='display:none'></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->LastSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->LastSelectableOption()->FastGetAttribute(
                        HTMLNames::idAttr));
  }
}

TEST_F(HTMLSelectElementTest, NextSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, select->NextSelectableOption(nullptr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->NextSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 disabled></option><option "
        "id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->NextSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 style='display:none'></option><option "
        "id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->NextSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->NextSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    HTMLOptionElement* option = ToHTMLOptionElement(GetElementById("o1"));
    EXPECT_EQ("o2", select->NextSelectableOption(option)->FastGetAttribute(
                        HTMLNames::idAttr));

    EXPECT_EQ(nullptr, select->NextSelectableOption(
                           ToHTMLOptionElement(GetElementById("o2"))));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><optgroup><option "
        "id=o2></option></optgroup></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    HTMLOptionElement* option = ToHTMLOptionElement(GetElementById("o1"));
    EXPECT_EQ("o2", select->NextSelectableOption(option)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
}

TEST_F(HTMLSelectElementTest, PreviousSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, select->PreviousSelectableOption(nullptr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->PreviousSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "disabled></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->PreviousSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "style='display:none'></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", select->PreviousSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", select->PreviousSelectableOption(nullptr)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    HTMLOptionElement* option = ToHTMLOptionElement(GetElementById("o2"));
    EXPECT_EQ("o1", select->PreviousSelectableOption(option)->FastGetAttribute(
                        HTMLNames::idAttr));

    EXPECT_EQ(nullptr, select->PreviousSelectableOption(
                           ToHTMLOptionElement(GetElementById("o1"))));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><optgroup><option "
        "id=o2></option></optgroup></select>");
    HTMLSelectElement* select =
        ToHTMLSelectElement(GetDocument().body()->firstChild());
    HTMLOptionElement* option = ToHTMLOptionElement(GetElementById("o2"));
    EXPECT_EQ("o1", select->PreviousSelectableOption(option)->FastGetAttribute(
                        HTMLNames::idAttr));
  }
}

TEST_F(HTMLSelectElementTest, ActiveSelectionEndAfterOptionRemoval) {
  SetHtmlInnerHTML(
      "<select><optgroup><option selected>o1</option></optgroup></select>");
  HTMLSelectElement* select =
      ToHTMLSelectElement(GetDocument().body()->firstChild());
  HTMLOptionElement* option =
      ToHTMLOptionElement(select->firstChild()->firstChild());
  EXPECT_EQ(1, select->ActiveSelectionEndListIndex());
  select->firstChild()->removeChild(option);
  EXPECT_EQ(-1, select->ActiveSelectionEndListIndex());
  select->AppendChild(option);
  EXPECT_EQ(1, select->ActiveSelectionEndListIndex());
}

TEST_F(HTMLSelectElementTest, DefaultToolTip) {
  SetHtmlInnerHTML(
      "<select size=4><option value="
      ">Placeholder</option><optgroup><option>o2</option></optgroup></select>");
  HTMLSelectElement* select =
      ToHTMLSelectElement(GetDocument().body()->firstChild());
  Element* option = ToElement(select->firstChild());
  Element* optgroup = ToElement(option->nextSibling());

  EXPECT_EQ(String(), select->DefaultToolTip())
      << "defaultToolTip for SELECT without FORM and without required "
         "attribute should return null string.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  select->SetBooleanAttribute(HTMLNames::requiredAttr, true);
  EXPECT_EQ("<<ValidationValueMissingForSelect>>", select->DefaultToolTip())
      << "defaultToolTip for SELECT without FORM and with required attribute "
         "should return a valueMissing message.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  HTMLFormElement* form = HTMLFormElement::Create(GetDocument());
  GetDocument().body()->AppendChild(form);
  form->AppendChild(select);
  EXPECT_EQ("<<ValidationValueMissingForSelect>>", select->DefaultToolTip())
      << "defaultToolTip for SELECT with FORM and required attribute should "
         "return a valueMissing message.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  form->SetBooleanAttribute(HTMLNames::novalidateAttr, true);
  EXPECT_EQ(String(), select->DefaultToolTip())
      << "defaultToolTip for SELECT with FORM[novalidate] and required "
         "attribute should return null string.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  option->remove();
  optgroup->remove();
  EXPECT_EQ(String(), option->DefaultToolTip());
  EXPECT_EQ(String(), optgroup->DefaultToolTip());
}

TEST_F(HTMLSelectElementTest, SetRecalcListItemsByOptgroupRemoval) {
  SetHtmlInnerHTML(
      "<select><optgroup><option>sub1</option><option>sub2</option></"
      "optgroup></select>");
  HTMLSelectElement* select =
      ToHTMLSelectElement(GetDocument().body()->firstChild());
  select->SetInnerHTMLFromString("");
  // PASS if setInnerHTML didn't have a check failure.
}

TEST_F(HTMLSelectElementTest, ScrollToOptionAfterLayoutCrash) {
  // crbug.com/737447
  // This test passes if no crash.
  SetHtmlInnerHTML(R"HTML(
    <style>*:checked { position:fixed; }</style>
    <select multiple><<option>o1</option><option
    selected>o2</option></select>
  )HTML");
}

}  // namespace blink
