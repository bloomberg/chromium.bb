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
    virtual void SetUp() OVERRIDE;

    DummyPageHolder& dummyPageHolder() const { return *m_dummyPageHolder; }
    HTMLDocument& document() const;
    void setSelection(const VisibleSelection&);
    FrameSelection& selection() const;
    Text* textNode() { return m_textNode.get(); }
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
    m_textNode = m_document->createTextNode("Hello, World!");
    m_document->body()->appendChild(m_textNode.get());
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

TEST_F(FrameSelectionTest, SetValidSelection)
{
    VisibleSelection validSelection(Position(textNode(), 0), Position(textNode(), 5));
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
    document().view()->updateLayoutAndStyleIfNeededRecursive();

    VisibleSelection validSelection(Position(textNode(), 0), Position(textNode(), 0));
    setSelection(validSelection);
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());

    document().body()->removeChild(textNode());
    document().updateLayoutIgnorePendingStylesheets();
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());
}

TEST_F(FrameSelectionTest, PaintCaretShouldNotLayout)
{
    document().view()->updateLayoutAndStyleIfNeededRecursive();

    document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
    document().body()->focus();
    EXPECT_TRUE(document().body()->focused());

    VisibleSelection validSelection(Position(textNode(), 0), Position(textNode(), 0));
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

}
