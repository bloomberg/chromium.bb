// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLSelectElement.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/forms/FormController.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLSelectElementTest : public::testing::Test {
protected:
    void SetUp() override;
    HTMLDocument& document() const { return *m_document; }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    RefPtrWillBePersistent<HTMLDocument> m_document;
};

void HTMLSelectElementTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);

    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    m_document->setMimeType("text/html");
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectSingleFormControlState)
{
    document().documentElement()->setInnerHTML(String("<!DOCTYPE HTML><select id='sel'>"
        "<option value='111' id='0'>111</option>"
        "<option value='222'>222</option>"
        "<option value='111' selected id='2'>!666</option>"
        "<option value='999'>999</option></select>"), ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("sel");
    HTMLFormControlElementWithState* select = toHTMLSelectElement(element);
    HTMLOptionElement* opt0 = toHTMLOptionElement(document().getElementById("0"));
    HTMLOptionElement* opt2 = toHTMLOptionElement(document().getElementById("2"));

    // Save the select element state, and then restore again.
    // Test passes if the restored state is not changed.
    EXPECT_EQ(2, toHTMLSelectElement(element)->selectedIndex());
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
    FormControlState selectState = select->saveFormControlState();
    EXPECT_EQ(2U, selectState.valueSize());

    // Clear the selected state, to be restored by restoreFormControlState.
    toHTMLSelectElement(select)->setSelectedIndex(-1);
    ASSERT_FALSE(opt2->selected());

    // Restore
    select->restoreFormControlState(selectState);
    EXPECT_EQ(2, toHTMLSelectElement(element)->selectedIndex());
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectMultipleFormControlState)
{
    document().documentElement()->setInnerHTML(String("<!DOCTYPE HTML><select id='sel' multiple>"
        "<option value='111' id='0'>111</option>"
        "<option value='222'>222</option>"
        "<option value='111' selected id='2'>!666</option>"
        "<option value='999' selected id='3'>999</option></select>"), ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLFormControlElementWithState* select = toHTMLSelectElement(document().getElementById("sel"));

    HTMLOptionElement* opt0 = toHTMLOptionElement(document().getElementById("0"));
    HTMLOptionElement* opt2 = toHTMLOptionElement(document().getElementById("2"));
    HTMLOptionElement* opt3 = toHTMLOptionElement(document().getElementById("3"));

    // Save the select element state, and then restore again.
    // Test passes if the selected options are not changed.
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
    EXPECT_TRUE(opt3->selected());
    FormControlState selectState = select->saveFormControlState();
    EXPECT_EQ(4U, selectState.valueSize());

    // Clear the selected state, to be restored by restoreFormControlState.
    opt2->setSelected(false);
    opt3->setSelected(false);
    ASSERT_FALSE(opt2->selected());
    ASSERT_FALSE(opt3->selected());

    // Restore
    select->restoreFormControlState(selectState);
    EXPECT_FALSE(opt0->selected());
    EXPECT_TRUE(opt2->selected());
    EXPECT_TRUE(opt3->selected());
}

TEST_F(HTMLSelectElementTest, ElementRectRelativeToViewport)
{
    document().documentElement()->setInnerHTML("<select style='position:fixed; top:12.3px; height:24px; -webkit-appearance:none;'><option>o1</select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    ASSERT(select);
    IntRect bounds = select->elementRectRelativeToViewport();
    EXPECT_EQ(24, bounds.height());
}

TEST_F(HTMLSelectElementTest, PopupIsVisible)
{
    document().documentElement()->setInnerHTML("<select><option>o1</option></select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    ASSERT(select);
    EXPECT_FALSE(select->popupIsVisible());
    select->showPopup();
    EXPECT_TRUE(select->popupIsVisible());
    document().detach();
    EXPECT_FALSE(select->popupIsVisible());
}

TEST_F(HTMLSelectElementTest, ActiveSelectionEndAfterOptionRemoval)
{
    document().documentElement()->setInnerHTML("<select><optgroup><option selected>o1</option></optgroup></select>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    HTMLSelectElement* select = toHTMLSelectElement(document().body()->firstChild());
    RefPtrWillBeRawPtr<HTMLOptionElement> option = toHTMLOptionElement(select->firstChild()->firstChild());
    EXPECT_EQ(1, select->activeSelectionEndListIndex());
    select->firstChild()->removeChild(option, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(-1, select->activeSelectionEndListIndex());
    select->appendChild(option, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(1, select->activeSelectionEndListIndex());
}

} // namespace blink
