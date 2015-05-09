// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/FrameSelection.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDocument.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/testing/WTFTestHelpers.h"
#include <gtest/gtest.h>

using namespace blink;

namespace {

class FrameSelectionTest : public ::testing::Test {
protected:
    virtual void SetUp() override;

    DummyPageHolder& dummyPageHolder() const { return *m_dummyPageHolder; }
    HTMLDocument& document() const;
    void setSelection(const VisibleSelection&);
    FrameSelection& selection() const;
    PassRefPtrWillBeRawPtr<Text> appendTextNode(const String& data);
    int layoutCount() const { return m_dummyPageHolder->frameView().layoutCount(); }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    RawPtr<HTMLDocument> m_document;
    RefPtrWillBePersistent<Text> m_textNode;
};

void FrameSelectionTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    ASSERT(m_document);
}

HTMLDocument& FrameSelectionTest::document() const
{
    return *m_document;
}

void FrameSelectionTest::setSelection(const VisibleSelection& newSelection)
{
    m_dummyPageHolder->frame().selection().setSelection(newSelection);
}

FrameSelection& FrameSelectionTest::selection() const
{
    return m_dummyPageHolder->frame().selection();
}

PassRefPtrWillBeRawPtr<Text> FrameSelectionTest::appendTextNode(const String& data)
{
    RefPtrWillBeRawPtr<Text> text = document().createTextNode(data);
    document().body()->appendChild(text);
    return text.release();
}

TEST_F(FrameSelectionTest, SetValidSelection)
{
    RefPtrWillBeRawPtr<Text> text = appendTextNode("Hello, World!");
    VisibleSelection validSelection(Position(text, 0), Position(text, 5));
    EXPECT_FALSE(validSelection.isNone());
    setSelection(validSelection);
    EXPECT_FALSE(selection().isNone());
}

TEST_F(FrameSelectionTest, SetInvalidSelection)
{
    // Create a new document without frame by using DOMImplementation.
    DocumentInit dummy;
    RefPtrWillBeRawPtr<Document> documentWithoutFrame = Document::create();
    RefPtrWillBeRawPtr<Element> body = documentWithoutFrame->createElement(HTMLNames::bodyTag, false);
    documentWithoutFrame->appendChild(body);
    RefPtrWillBeRawPtr<Text> anotherText = documentWithoutFrame->createTextNode("Hello, another world");
    body->appendChild(anotherText);

    // Create a new VisibleSelection for the new document without frame and
    // update FrameSelection with the selection.
    VisibleSelection invalidSelection;
    invalidSelection.setWithoutValidation(Position(anotherText, 0), Position(anotherText, 5));
    setSelection(invalidSelection);

    EXPECT_TRUE(selection().isNone());
}

TEST_F(FrameSelectionTest, InvalidateCaretRect)
{
    RefPtrWillBeRawPtr<Text> text = appendTextNode("Hello, World!");
    document().view()->updateLayoutAndStyleForPainting();

    VisibleSelection validSelection(Position(text, 0), Position(text, 0));
    setSelection(validSelection);
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());

    document().body()->removeChild(text);
    document().updateLayoutIgnorePendingStylesheets();
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());
}

TEST_F(FrameSelectionTest, PaintCaretShouldNotLayout)
{
    RefPtrWillBeRawPtr<Text> text = appendTextNode("Hello, World!");
    document().view()->updateLayoutAndStyleForPainting();

    document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
    document().body()->focus();
    EXPECT_TRUE(document().body()->focused());

    VisibleSelection validSelection(Position(text, 0), Position(text, 0));
    selection().setCaretVisible(true);
    setSelection(validSelection);
    EXPECT_TRUE(selection().isCaret());
    EXPECT_TRUE(selection().ShouldPaintCaretForTesting());

    int startCount = layoutCount();
    {
        // To force layout in next updateLayout calling, widen view.
        FrameView& frameView = dummyPageHolder().frameView();
        IntRect frameRect = frameView.frameRect();
        frameRect.setWidth(frameRect.width() + 1);
        frameRect.setHeight(frameRect.height() + 1);
        dummyPageHolder().frameView().setFrameRect(frameRect);
    }
    selection().paintCaret(nullptr, LayoutPoint(), LayoutRect());
    EXPECT_EQ(startCount, layoutCount());
}

#define EXPECT_EQ_SELECTED_TEXT(text) \
    EXPECT_EQ(text, WebString(selection().selectedText()).utf8())

TEST_F(FrameSelectionTest, SelectWordAroundPosition)
{
    // "Foo Bar  Baz,"
    RefPtrWillBeRawPtr<Text> text = appendTextNode("Foo Bar&nbsp;&nbsp;Baz,");
    // "Fo|o Bar  Baz,"
    EXPECT_TRUE(selection().selectWordAroundPosition(VisiblePosition(Position(text, 2))));
    EXPECT_EQ_SELECTED_TEXT("Foo");
    // "Foo| Bar  Baz,"
    EXPECT_TRUE(selection().selectWordAroundPosition(VisiblePosition(Position(text, 3))));
    EXPECT_EQ_SELECTED_TEXT("Foo");
    // "Foo Bar | Baz,"
    EXPECT_FALSE(selection().selectWordAroundPosition(VisiblePosition(Position(text, 13))));
    // "Foo Bar  Baz|,"
    EXPECT_TRUE(selection().selectWordAroundPosition(VisiblePosition(Position(text, 22))));
    EXPECT_EQ_SELECTED_TEXT("Baz");
}

// Test for MoveRangeSelectionExtent with the default CharacterGranularityStrategy
TEST_F(FrameSelectionTest, MoveRangeSelectionExtentCharacter)
{
    // "Foo Bar Baz,"
    RefPtrWillBeRawPtr<Text> text = appendTextNode("Foo Bar Baz,");
    // "Foo B|a>r Baz," (| means start and > means end).
    selection().setSelection(VisibleSelection(Position(text, 5), Position(text, 6)));
    EXPECT_EQ_SELECTED_TEXT("a");
    // "Foo B|ar B>az," with the Character granularity.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 1)));
    EXPECT_EQ_SELECTED_TEXT("oo B");
}

// Test for MoveRangeSelectionExtent with DirectionGranularityStrategy
TEST_F(FrameSelectionTest, MoveRangeSelectionExtentDirection)
{
    RefPtrWillBeRawPtr<Text> text = document().createTextNode("abcdef ghij kl mnopqr stuvwx yzab,");
    document().body()->appendChild(text);
    dummyPageHolder().frame().settings()->setSelectionStrategy(SelectionStrategy::Direction);

    // "abcdef ghij kl mno|p>qr stuvwx yzab," (| means start and > means end).
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 19)));
    EXPECT_EQ_SELECTED_TEXT("p");
    // "abcdef ghij kl mno|pq>r stuvwx yzab," - expand selection using character granularity
    // until the end of the word is reached.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 20)));
    EXPECT_EQ_SELECTED_TEXT("pq");
    // "abcdef ghij kl mno|pqr> stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 21)));
    EXPECT_EQ_SELECTED_TEXT("pqr");
    // "abcdef ghij kl mno|pqr >stuvwx yzab," - confirm selection doesn't
    // jump over the beginning of the word.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 22)));
    EXPECT_EQ_SELECTED_TEXT("pqr ");
    // "abcdef ghij kl mno|pqr s>tuvwx yzab," - confirm selection switches to word granularity.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 23)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx");
    // "abcdef ghij kl mno|pqr stu>vwx yzab," - selection stays the same.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 25)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx");
    // "abcdef ghij kl mno|pqr stuvwx yz>ab," - next word.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 31)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx yzab");
    // "abcdef ghij kl mno|pqr stuvwx y>zab," - move back one character -
    // confirm switch to character granularity.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 30)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx y");
    // "abcdef ghij kl mno|pqr stuvwx yz>ab," - stay in character granularity
    // if the user moves the position within the word.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 31)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx yz");
    // "abcdef ghij kl mno|pqr stuvwx >yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 29)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx ");
    // "abcdef ghij kl mno|pqr stuvwx >yzab," - it's possible to get a move when
    // position doesn't change. It shouldn't affect anything.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 29)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx ");
    // "abcdef ghij kl mno|pqr stuvwx y>zab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 30)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx y");
    // "abcdef ghij kl mno|pqr stuv>wx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 26)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuv");
    // "abcdef ghij kl mno|pqr stuvw>x yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 27)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvw");
    // "abcdef ghij kl mno|pqr stuvwx y>zab," - switch to word granularity
    // after expanding beyond the word boundary
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 30)));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwx yzab");
    // "abcdef ghij kl mn<o|pqr stuvwx yzab," - over to the other side of the base
    // - stay in character granularity until the beginning of the word is passed.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 17)));
    EXPECT_EQ_SELECTED_TEXT("o");
    // "abcdef ghij kl m<no|pqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 16)));
    EXPECT_EQ_SELECTED_TEXT("no");
    // "abcdef ghij kl <mno|pqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 15)));
    EXPECT_EQ_SELECTED_TEXT("mno");
    // "abcdef ghij kl mn<o|pqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 17)));
    EXPECT_EQ_SELECTED_TEXT("o");
    // "abcdef ghij k<l mno|pqr stuvwx yzab," - switch to word granularity
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 13)));
    EXPECT_EQ_SELECTED_TEXT("kl mno");
    // "abcd<ef ghij kl mno|pqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 4)));
    EXPECT_EQ_SELECTED_TEXT("abcdef ghij kl mno");
    // "abcde<f ghij kl mno|pqr stuvwx yzab," - decrease selection -
    // switch back to character granularity.
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 5)));
    EXPECT_EQ_SELECTED_TEXT("f ghij kl mno");
    // "abcdef ghij kl mn<o|pqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 17)));
    EXPECT_EQ_SELECTED_TEXT("o");
    // "abcdef ghij kl m<no|pqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 16)));
    EXPECT_EQ_SELECTED_TEXT("no");
    // "abcdef ghij k<l mno|pqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 13)));
    EXPECT_EQ_SELECTED_TEXT("kl mno");

    // Make sure we switch to word granularity right away when starting on a
    // word boundary and extending.
    // "abcdef ghij kl |mnopqr >stuvwx yzab," (| means start and > means end).
    selection().setSelection(VisibleSelection(Position(text, 15), Position(text, 22)));
    EXPECT_EQ_SELECTED_TEXT("mnopqr ");
    // "abcdef ghij kl |mnopqr s>tuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 23)));
    EXPECT_EQ_SELECTED_TEXT("mnopqr stuvwx");

    // Make sure we start in character granularity when moving extent over to the other
    // side of the base.
    // "abcdef| ghij> kl mnopqr stuvwx yzab," (| means start and > means end).
    selection().setSelection(VisibleSelection(Position(text, 6), Position(text, 11)));
    EXPECT_EQ_SELECTED_TEXT(" ghij");
    // "abcde<f| ghij kl mnopqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 5)));
    EXPECT_EQ_SELECTED_TEXT("f");

    // Make sure we switch to word granularity when moving over to the other
    // side of the base and then passing over the word boundary.
    // "abcdef |ghij> kl mnopqr stuvwx yzab,"
    selection().setSelection(VisibleSelection(Position(text, 7), Position(text, 11)));
    EXPECT_EQ_SELECTED_TEXT("ghij");
    // "abcdef< |ghij kl mnopqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 6)));
    EXPECT_EQ_SELECTED_TEXT(" ");
    // "abcde<f |ghij kl mnopqr stuvwx yzab,"
    selection().moveRangeSelectionExtent(VisiblePosition(Position(text, 5)));
    EXPECT_EQ_SELECTED_TEXT("abcdef ");
}

TEST_F(FrameSelectionTest, MoveRangeSelectionTest)
{
    // "Foo Bar Baz,"
    RefPtrWillBeRawPtr<Text> text = appendTextNode("Foo Bar Baz,");
    // Itinitializes with "Foo B|a>r Baz," (| means start and > means end).
    selection().setSelection(VisibleSelection(Position(text, 5), Position(text, 6)));
    EXPECT_EQ_SELECTED_TEXT("a");

    // "Foo B|ar B>az," with the Character granularity.
    selection().moveRangeSelection(VisiblePosition(Position(text, 5)), VisiblePosition(Position(text, 9)), CharacterGranularity);
    EXPECT_EQ_SELECTED_TEXT("ar B");
    // "Foo B|ar B>az," with the Word granularity.
    selection().moveRangeSelection(VisiblePosition(Position(text, 5)), VisiblePosition(Position(text, 9)), WordGranularity);
    EXPECT_EQ_SELECTED_TEXT("Bar Baz");
    // "Fo<o B|ar Baz," with the Character granularity.
    selection().moveRangeSelection(VisiblePosition(Position(text, 5)), VisiblePosition(Position(text, 2)), CharacterGranularity);
    EXPECT_EQ_SELECTED_TEXT("o B");
    // "Fo<o B|ar Baz," with the Word granularity.
    selection().moveRangeSelection(VisiblePosition(Position(text, 5)), VisiblePosition(Position(text, 2)), WordGranularity);
    EXPECT_EQ_SELECTED_TEXT("Foo Bar");
}
}
