// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLTextFormControlElement.h"

#include "core/dom/Position.h"
#include "core/dom/Text.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SpellChecker.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/loader/EmptyClients.h"
#include "core/page/SpellCheckerClient.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/OwnPtr.h"
#include <gtest/gtest.h>

using namespace blink;

namespace {

class HTMLTextFormControlElementTest : public ::testing::Test {
protected:
    virtual void SetUp() OVERRIDE;

    DummyPageHolder& page() const { return *m_dummyPageHolder; }
    HTMLDocument& document() const { return *m_document; }
    HTMLTextFormControlElement& textControl() const { return *m_textControl; }

private:
    OwnPtr<SpellCheckerClient> m_spellCheckerClient;
    OwnPtr<DummyPageHolder> m_dummyPageHolder;

    RefPtrWillBePersistent<HTMLDocument> m_document;
    RefPtrWillBePersistent<HTMLTextFormControlElement> m_textControl;
};

class DummyTextCheckerClient : public EmptyTextCheckerClient {
public:
    ~DummyTextCheckerClient() { }

    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const OVERRIDE { return false; }
};

class DummySpellCheckerClient : public EmptySpellCheckerClient {
public:
    virtual ~DummySpellCheckerClient() { }

    virtual bool isContinuousSpellCheckingEnabled() OVERRIDE { return true; }
    virtual bool isGrammarCheckingEnabled() OVERRIDE { return true; }

    virtual TextCheckerClient& textChecker() OVERRIDE { return m_dummyTextCheckerClient; }

private:
    DummyTextCheckerClient m_dummyTextCheckerClient;
};

void HTMLTextFormControlElementTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_spellCheckerClient = adoptPtr(new DummySpellCheckerClient);
    pageClients.spellCheckerClient = m_spellCheckerClient.get();
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);

    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    m_document->documentElement()->setInnerHTML("<body><textarea id=textarea></textarea><input id=input /></body>", ASSERT_NO_EXCEPTION);
    m_document->view()->updateLayoutAndStyleIfNeededRecursive();
    m_textControl = toHTMLTextFormControlElement(m_document->getElementById("textarea"));
    m_textControl->focus();
}

TEST_F(HTMLTextFormControlElementTest, SetSelectionRange)
{
    EXPECT_EQ(0, textControl().selectionStart());
    EXPECT_EQ(0, textControl().selectionEnd());

    textControl().setInnerEditorValue("Hello, text form.");
    EXPECT_EQ(0, textControl().selectionStart());
    EXPECT_EQ(0, textControl().selectionEnd());

    textControl().setSelectionRange(1, 3);
    EXPECT_EQ(1, textControl().selectionStart());
    EXPECT_EQ(3, textControl().selectionEnd());
}

typedef Position (*PositionFunction)(const Position&);
typedef VisiblePosition(*VisblePositionFunction)(const VisiblePosition&);

void testFunctionEquivalence(const Position& position, PositionFunction positionFunction, VisblePositionFunction visibleFunction)
{
    VisiblePosition visiblePosition(position);
    VisiblePosition expected = visibleFunction(visiblePosition);
    VisiblePosition actual = VisiblePosition(positionFunction(position));
    EXPECT_EQ(expected, actual);
}

static VisiblePosition startOfWord(const VisiblePosition& position)
{
    return startOfWord(position, LeftWordIfOnBoundary);
}

static VisiblePosition endOfWord(const VisiblePosition& position)
{
    return endOfWord(position, RightWordIfOnBoundary);
}

void testBoundary(HTMLDocument& document, HTMLTextFormControlElement& textControl)
{
    for (unsigned i = 0; i < textControl.innerEditorValue().length(); i++) {
        textControl.setSelectionRange(i, i);
        Position position = document.frame()->selection().start();
        SCOPED_TRACE(testing::Message() << "offset " << position.deprecatedEditingOffset() << " of " << nodePositionAsStringForTesting(position.deprecatedNode()).ascii().data());
        {
            SCOPED_TRACE("HTMLTextFormControlElement::startOfWord");
            testFunctionEquivalence(position, HTMLTextFormControlElement::startOfWord, startOfWord);
        }
        {
            SCOPED_TRACE("HTMLTextFormControlElement::endOfWord");
            testFunctionEquivalence(position, HTMLTextFormControlElement::endOfWord, endOfWord);
        }
        {
            SCOPED_TRACE("HTMLTextFormControlElement::startOfSentence");
            testFunctionEquivalence(position, HTMLTextFormControlElement::startOfSentence, startOfSentence);
        }
        {
            SCOPED_TRACE("HTMLTextFormControlElement::endOfSentence");
            testFunctionEquivalence(position, HTMLTextFormControlElement::endOfSentence, endOfSentence);
        }
    }
}

TEST_F(HTMLTextFormControlElementTest, WordAndSentenceBoundary)
{
    HTMLElement* innerText = textControl().innerEditorElement();
    {
        SCOPED_TRACE("String is value.");
        innerText->removeChildren();
        innerText->setNodeValue("Hel\nlo, text form.\n");
        testBoundary(document(), textControl());
    }
    {
        SCOPED_TRACE("A Text node and a BR element");
        innerText->removeChildren();
        innerText->setNodeValue("");
        innerText->appendChild(Text::create(document(), "Hello, text form."));
        innerText->appendChild(HTMLBRElement::create(document()));
        testBoundary(document(), textControl());
    }
    {
        SCOPED_TRACE("Text nodes.");
        innerText->removeChildren();
        innerText->setNodeValue("");
        innerText->appendChild(Text::create(document(), "Hel\nlo, te"));
        innerText->appendChild(Text::create(document(), "xt form."));
        testBoundary(document(), textControl());
    }
}

TEST_F(HTMLTextFormControlElementTest, SpellCheckDoesNotCauseUpdateLayout)
{
    HTMLInputElement* input = toHTMLInputElement(document().getElementById("input"));
    input->focus();
    input->setValue("Hello, input field");
    VisibleSelection oldSelection = document().frame()->selection().selection();

    Position newPosition(input->innerEditorElement()->firstChild(), 3, Position::PositionIsOffsetInAnchor);
    VisibleSelection newSelection(newPosition, DOWNSTREAM);
    document().frame()->selection().setSelection(newSelection, FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle | FrameSelection::DoNotUpdateAppearance);
    ASSERT_EQ(3, input->selectionStart());

    OwnPtr<SpellChecker> spellChecker(SpellChecker::create(page().frame()));
    page().frameView().setFrameRect(IntRect(0, 0, 801, 601));
    int startCount = page().frameView().layoutCount();
    spellChecker->respondToChangedSelection(oldSelection, FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle);
    EXPECT_EQ(startCount, page().frameView().layoutCount());
}

}
