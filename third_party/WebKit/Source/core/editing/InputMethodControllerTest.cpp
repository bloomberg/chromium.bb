// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/InputMethodController.h"

#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLInputElement.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

using namespace blink;

namespace {

class InputMethodControllerTest : public ::testing::Test {
protected:
    InputMethodController& controller() { return frame().inputMethodController(); }
    HTMLDocument& document() const { return *m_document; }
    LocalFrame& frame() const { return m_dummyPageHolder->frame(); }
    Element* insertHTMLElement(const char* elementCode, const char* elementId);

private:
    virtual void SetUp() OVERRIDE;

    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    HTMLDocument* m_document;
};

void InputMethodControllerTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    ASSERT(m_document);
}

Element* InputMethodControllerTest::insertHTMLElement(
    const char* elementCode, const char* elementId)
{
    document().write(elementCode);
    document().updateLayout();
    Element* element = document().getElementById(elementId);
    element->focus();
    return element;
}

TEST_F(InputMethodControllerTest, BackspaceFromEndOfInput)
{
    HTMLInputElement* input = toHTMLInputElement(
        insertHTMLElement("<input id='sample'>", "sample"));

    input->setValue("fooX");
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("fooX", input->value().utf8().data());
    controller().extendSelectionAndDelete(0, 0);
    EXPECT_STREQ("fooX", input->value().utf8().data());

    input->setValue("fooX");
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("fooX", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xE2\x98\x85")); // U+2605 == "black star"
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("foo\xE2\x98\x85", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86")); // U+1F3C6 == "trophy"
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89")); // composed U+0E01 "ka kai" + U+0E49 "mai tho"
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue("fooX");
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("fooX", input->value().utf8().data());
    controller().extendSelectionAndDelete(0, 1);
    EXPECT_STREQ("fooX", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionFromExistingText)
{
    Element* div = insertHTMLElement(
        "<div id='sample' contenteditable='true'>hello world</div>", "sample");

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
    controller().setCompositionFromExistingText(underlines, 0, 5);

    RefPtrWillBeRawPtr<Range> range = controller().compositionRange();
    EXPECT_EQ(0, range->startOffset());
    EXPECT_EQ(5, range->endOffset());

    PlainTextRange plainTextRange(PlainTextRange::create(*div, *range.get()));
    EXPECT_EQ(0u, plainTextRange.start());
    EXPECT_EQ(5u, plainTextRange.end());
}

TEST_F(InputMethodControllerTest, SetCompositionFromExistingTextWithCollapsedWhiteSpace)
{
    // Creates a div with one leading new line char. The new line char is hidden
    // from the user and IME, but is visible to InputMethodController.
    Element* div = insertHTMLElement(
        "<div id='sample' contenteditable='true'>\nhello world</div>", "sample");

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
    controller().setCompositionFromExistingText(underlines, 0, 5);

    RefPtrWillBeRawPtr<Range> range = controller().compositionRange();
    EXPECT_EQ(1, range->startOffset());
    EXPECT_EQ(6, range->endOffset());

    PlainTextRange plainTextRange(PlainTextRange::create(*div, *range.get()));
    EXPECT_EQ(0u, plainTextRange.start());
    EXPECT_EQ(5u, plainTextRange.end());
}

TEST_F(InputMethodControllerTest, SetCompositionFromExistingTextWithInvalidOffsets)
{
    insertHTMLElement("<div id='sample' contenteditable='true'>test</div>", "sample");

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(7, 8, Color(255, 0, 0), false, 0));
    controller().setCompositionFromExistingText(underlines, 7, 8);

    EXPECT_FALSE(controller().compositionRange());
}

} // namespace
