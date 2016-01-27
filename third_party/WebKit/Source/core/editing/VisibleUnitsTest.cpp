// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleUnits.h"

#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/VisiblePosition.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/line/InlineBox.h"
#include <ostream> // NOLINT

namespace blink {

namespace {

PositionWithAffinity positionWithAffinityInDOMTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return PositionWithAffinity(canonicalPositionOf(Position(&anchor, offset)), affinity);
}

VisiblePosition createVisiblePositionInDOMTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return createVisiblePosition(Position(&anchor, offset), affinity);
}

PositionInComposedTreeWithAffinity positionWithAffinityInComposedTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return PositionInComposedTreeWithAffinity(canonicalPositionOf(PositionInComposedTree(&anchor, offset)), affinity);
}

VisiblePositionInComposedTree createVisiblePositionInComposedTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return createVisiblePosition(PositionInComposedTree(&anchor, offset), affinity);
}

} // namespace

std::ostream& operator<<(std::ostream& ostream, const InlineBoxPosition& inlineBoxPosition)
{
    if (!inlineBoxPosition.inlineBox)
        return ostream << "null";
    return ostream << inlineBoxPosition.inlineBox->lineLayoutItem().node() << "@" << inlineBoxPosition.offsetInBox;
}

class VisibleUnitsTest : public EditingTestBase {
};

TEST_F(VisibleUnitsTest, absoluteCaretBoundsOf)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b><b id='two'>22</b></p>";
    const char* shadowContent = "<div><content select=#two></content><content select=#one></content></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    ASSERT_UNUSED(shadowRoot, shadowRoot);
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);

    IntRect boundsInDOMTree = absoluteCaretBoundsOf(createVisiblePosition(Position(one.get(), 0)));
    IntRect boundsInComposedTree = absoluteCaretBoundsOf(createVisiblePosition(PositionInComposedTree(one.get(), 0)));

    EXPECT_FALSE(boundsInDOMTree.isEmpty());
    EXPECT_EQ(boundsInDOMTree, boundsInComposedTree);
}

TEST_F(VisibleUnitsTest, caretMinOffset)
{
    const char* bodyContent = "<p id=one>one</p>";
    setBodyContent(bodyContent);

    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");

    EXPECT_EQ(0, caretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, caretMinOffsetWithFirstLetter)
{
    const char* bodyContent = "<style>#one:first-letter { font-size: 200%; }</style><p id=one>one</p>";
    setBodyContent(bodyContent);

    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");

    EXPECT_EQ(0, caretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, characterAfter)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b><b id='two'>22</b></p><b id='three'>333</b>";
    const char* shadowContent = "<b id='four'>4444</b><content select=#two></content><content select=#one></content><b id='five'>5555</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Element> two = document().getElementById("two");

    EXPECT_EQ('2', characterAfter(createVisiblePositionInDOMTree(*one->firstChild(), 1)));
    EXPECT_EQ('5', characterAfter(createVisiblePositionInComposedTree(*one->firstChild(), 1)));

    EXPECT_EQ(0, characterAfter(createVisiblePositionInDOMTree(*two->firstChild(), 2)));
    EXPECT_EQ('1', characterAfter(createVisiblePositionInComposedTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, canonicalPositionOfWithHTMLHtmlElement)
{
    const char* bodyContent = "<html><div id=one contenteditable>1</div><span id=two contenteditable=false>22</span><span id=three contenteditable=false>333</span><span id=four contenteditable=false>333</span></html>";
    setBodyContent(bodyContent);

    RefPtrWillBeRawPtr<Node> one = document().querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Node> two = document().querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Node> three = document().querySelector("#three", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Node> four = document().querySelector("#four", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> html = document().createElement("html", ASSERT_NO_EXCEPTION);
    // Move two, three and four into second html element.
    html->appendChild(two.get());
    html->appendChild(three.get());
    html->appendChild(four.get());
    one->appendChild(html.get());
    updateLayoutAndStyleForPainting();

    EXPECT_EQ(Position(), canonicalPositionOf(Position(document().documentElement(), 0)));

    EXPECT_EQ(Position(one->firstChild(), 0), canonicalPositionOf(Position(one.get(), 0)));
    EXPECT_EQ(Position(one->firstChild(), 1), canonicalPositionOf(Position(one.get(), 1)));

    EXPECT_EQ(Position(one->firstChild(), 0), canonicalPositionOf(Position(one->firstChild(), 0)));
    EXPECT_EQ(Position(one->firstChild(), 1), canonicalPositionOf(Position(one->firstChild(), 1)));

    EXPECT_EQ(Position(html.get(), 0), canonicalPositionOf(Position(html.get(), 0)));
    EXPECT_EQ(Position(html.get(), 1), canonicalPositionOf(Position(html.get(), 1)));
    EXPECT_EQ(Position(html.get(), 2), canonicalPositionOf(Position(html.get(), 2)));

    EXPECT_EQ(Position(two->firstChild(), 0), canonicalPositionOf(Position(two.get(), 0)));
    EXPECT_EQ(Position(two->firstChild(), 2), canonicalPositionOf(Position(two.get(), 1)));
}

TEST_F(VisibleUnitsTest, characterBefore)
{
    const char* bodyContent = "<p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>5555</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(0, characterBefore(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_EQ('2', characterBefore(createVisiblePositionInComposedTree(*one, 0)));

    EXPECT_EQ('1', characterBefore(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_EQ('1', characterBefore(createVisiblePositionInComposedTree(*one, 1)));

    EXPECT_EQ('1', characterBefore(createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_EQ('4', characterBefore(createVisiblePositionInComposedTree(*two, 0)));

    EXPECT_EQ('4', characterBefore(createVisiblePositionInDOMTree(*five, 0)));
    EXPECT_EQ('1', characterBefore(createVisiblePositionInComposedTree(*five, 0)));
}

TEST_F(VisibleUnitsTest, computeInlineBoxPosition)
{
    const char* bodyContent = "<p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>5555</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(one, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*one, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(one, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*one, 1)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(two, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*two, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(two, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*two, 1)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(two, 2), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*two, 2)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(three, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*three, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(three, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*three, 1)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(four, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*four, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(four, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*four, 1)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(five, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*five, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInComposedTree(five, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInComposedTree(*five, 1)));
}

TEST_F(VisibleUnitsTest, endOfDocument)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");

    EXPECT_EQ(Position(two->firstChild(), 2), endOfDocument(createVisiblePositionInDOMTree(*one->firstChild(), 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 1), endOfDocument(createVisiblePositionInComposedTree(*one->firstChild(), 0)).deepEquivalent());

    EXPECT_EQ(Position(two->firstChild(), 2), endOfDocument(createVisiblePositionInDOMTree(*two->firstChild(), 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 1), endOfDocument(createVisiblePositionInComposedTree(*two->firstChild(), 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(seven, 7), endOfLine(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(seven, 7), endOfLine(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), endOfLine(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    // TODO(yosin) endOfLine(two, 1) -> (five, 5) is a broken result. We keep
    // it as a marker for future change.
    EXPECT_EQ(Position(five, 5), endOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), endOfLine(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), endOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfLine(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), endOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfLine(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), endOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), endOfLine(createVisiblePositionInComposedTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 6), endOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(six, 6), endOfLine(createVisiblePositionInComposedTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(seven, 7), endOfLine(createVisiblePositionInComposedTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraph)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");
    Element* three = document().getElementById("three");

    EXPECT_EQ(Position(three->firstChild(), 3), endOfParagraph(createVisiblePositionInDOMTree(*one->firstChild(), 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 1), endOfParagraph(createVisiblePositionInComposedTree(*one->firstChild(), 1)).deepEquivalent());

    EXPECT_EQ(Position(three->firstChild(), 3), endOfParagraph(createVisiblePositionInDOMTree(*two->firstChild(), 2)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two->firstChild(), 2), endOfParagraph(createVisiblePositionInComposedTree(*two->firstChild(), 2)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfSentence)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><i id=three>333</i> <content select=#two></content> <content select=#one></content> <i id=four>4444</i></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = shadowRoot->getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfSentence(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfSentence(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfSentence(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfSentence(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), endOfSentence(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfSentence(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), endOfSentence(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), endOfSentence(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfWord)
{
    const char* bodyContent = "<a id=host><b id=one>1</b> <b id=two>22</b></a><i id=three>333</i>";
    const char* shadowContent = "<p><u id=four>44444</u><content select=#two></content><span id=space> </span><content select=#one></content><u id=five>55555</u></p>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 5), endOfWord(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 5), endOfWord(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), endOfWord(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), endOfWord(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 3), endOfWord(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 5), endOfWord(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), endOfWord(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), endOfWord(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 5), endOfWord(createVisiblePositionInComposedTree(*five, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContent)
{
    const char* bodyContent = "<a id=host><b id=one contenteditable>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<content select=#two></content></p><p><content select=#one></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");

    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*one->firstChild(), 1)));
    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInComposedTree(*one->firstChild(), 1)));

    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*two->firstChild(), 2)));
    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInComposedTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContentWithInput)
{
    const char* bodyContent = "<input id=sample value=ab>cde";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();

    Node* text = toHTMLTextFormControlElement(document().getElementById("sample"))->innerEditorElement()->firstChild();

    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*text, 0)));
    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInComposedTree(*text, 0)));

    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*text, 1)));
    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInComposedTree(*text, 1)));

    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*text, 2)));
    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInComposedTree(*text, 2)));
}

TEST_F(VisibleUnitsTest, isEndOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInComposedTree(*one, 0)));

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInComposedTree(*one, 1)));

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*two, 2)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInComposedTree(*two, 2)));

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*three, 3)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInComposedTree(*three, 3)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*four, 4)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInComposedTree(*four, 4)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*five, 5)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInComposedTree(*five, 5)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*six, 6)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInComposedTree(*six, 6)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*seven, 7)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInComposedTree(*seven, 7)));
}

TEST_F(VisibleUnitsTest, isLogicalEndOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*one, 0)));

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*one, 1)));

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*two, 2)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*two, 2)));

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*three, 3)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*three, 3)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*four, 4)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*four, 4)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*five, 5)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*five, 5)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*six, 6)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*six, 6)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*seven, 7)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInComposedTree(*seven, 7)));
}

TEST_F(VisibleUnitsTest, inSameLine)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<div><span id='s4'>44</span><content select=#two></content><br><span id='s5'>55</span><br><content select=#one></content><span id='s6'>66</span></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> four = shadowRoot->querySelector("#s4", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one, 0), positionWithAffinityInDOMTree(*two, 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*five->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*two->firstChild(), 0), positionWithAffinityInDOMTree(*four->firstChild(), 0)));

    EXPECT_TRUE(inSameLine(createVisiblePositionInDOMTree(*one, 0), createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_TRUE(inSameLine(createVisiblePositionInDOMTree(*one->firstChild(), 0), createVisiblePositionInDOMTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInDOMTree(*one->firstChild(), 0), createVisiblePositionInDOMTree(*five->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInDOMTree(*two->firstChild(), 0), createVisiblePositionInDOMTree(*four->firstChild(), 0)));

    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one, 0), positionWithAffinityInComposedTree(*two, 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInComposedTree(*one->firstChild(), 0), positionWithAffinityInComposedTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInComposedTree(*two->firstChild(), 0), positionWithAffinityInComposedTree(*four->firstChild(), 0)));

    EXPECT_FALSE(inSameLine(createVisiblePositionInComposedTree(*one, 0), createVisiblePositionInComposedTree(*two, 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInComposedTree(*one->firstChild(), 0), createVisiblePositionInComposedTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInComposedTree(*one->firstChild(), 0), createVisiblePositionInComposedTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(createVisiblePositionInComposedTree(*two->firstChild(), 0), createVisiblePositionInComposedTree(*four->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, isEndOfParagraph)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInComposedTree(*one, 0)));

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInComposedTree(*one, 1)));

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*two, 2)));
    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInComposedTree(*two, 2)));

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*three, 0)));
    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInComposedTree(*three, 0)));

    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInDOMTree(*three, 3)));
    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInComposedTree(*three, 3)));
}

TEST_F(VisibleUnitsTest, isStartOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_TRUE(isStartOfLine(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInComposedTree(*one, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInComposedTree(*one, 1)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInComposedTree(*two, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*three, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInComposedTree(*three, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*four, 0)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInComposedTree(*four, 0)));

    EXPECT_TRUE(isStartOfLine(createVisiblePositionInDOMTree(*five, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInComposedTree(*five, 0)));

    EXPECT_TRUE(isStartOfLine(createVisiblePositionInDOMTree(*six, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInComposedTree(*six, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*seven, 0)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInComposedTree(*seven, 0)));
}

TEST_F(VisibleUnitsTest, isStartOfParagraph)
{
    const char* bodyContent = "<b id=zero>0</b><a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* zero = document().getElementById("zero")->firstChild();
    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();

    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInDOMTree(*zero, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInComposedTree(*zero, 0)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInComposedTree(*one, 0)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInComposedTree(*one, 1)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInComposedTree(*two, 0)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*three, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInComposedTree(*three, 0)));
}

TEST_F(VisibleUnitsTest, isVisuallyEquivalentCandidateWithHTMLHtmlElement)
{
    const char* bodyContent = "<html><div id=one contenteditable>1</div><span id=two contenteditable=false>22</span><span id=three contenteditable=false>333</span><span id=four contenteditable=false>333</span></html>";
    setBodyContent(bodyContent);

    RefPtrWillBeRawPtr<Node> one = document().querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Node> two = document().querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Node> three = document().querySelector("#three", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Node> four = document().querySelector("#four", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> html = document().createElement("html", ASSERT_NO_EXCEPTION);
    // Move two, three and four into second html element.
    html->appendChild(two.get());
    html->appendChild(three.get());
    html->appendChild(four.get());
    one->appendChild(html.get());
    updateLayoutAndStyleForPainting();

    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(document().documentElement(), 0)));

    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(one.get(), 0)));
    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(one.get(), 1)));

    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(one->firstChild(), 0)));
    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(one->firstChild(), 1)));

    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(html.get(), 0)));
    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(html.get(), 1)));
    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(html.get(), 2)));

    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(two.get(), 0)));
    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(two.get(), 1)));
}

TEST_F(VisibleUnitsTest, leftPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>55555</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");
    Element* three = document().getElementById("three");
    Element* four = shadowRoot->getElementById("four");
    Element* five = shadowRoot->getElementById("five");

    EXPECT_EQ(Position(two->firstChild(), 1), leftPositionOf(createVisiblePosition(Position(one, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two->firstChild(), 1), leftPositionOf(createVisiblePosition(PositionInComposedTree(one, 0))).deepEquivalent());

    EXPECT_EQ(Position(one->firstChild(), 0), leftPositionOf(createVisiblePosition(Position(two, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four->firstChild(), 3), leftPositionOf(createVisiblePosition(PositionInComposedTree(two, 0))).deepEquivalent());

    EXPECT_EQ(Position(two->firstChild(), 2), leftPositionOf(createVisiblePosition(Position(three, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five->firstChild(), 5), leftPositionOf(createVisiblePosition(PositionInComposedTree(three, 0))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, localCaretRectOfPosition)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");

    LayoutObject* layoutObjectFromDOMTree;
    LayoutRect layoutRectFromDOMTree = localCaretRectOfPosition(Position(one->firstChild(), 0), layoutObjectFromDOMTree);

    LayoutObject* layoutObjectFromComposedTree;
    LayoutRect layoutRectFromComposedTree = localCaretRectOfPosition(PositionInComposedTree(one->firstChild(), 0), layoutObjectFromComposedTree);

    EXPECT_TRUE(layoutObjectFromDOMTree);
    EXPECT_FALSE(layoutRectFromDOMTree.isEmpty());
    EXPECT_EQ(layoutObjectFromDOMTree, layoutObjectFromComposedTree);
    EXPECT_EQ(layoutRectFromDOMTree, layoutRectFromComposedTree);
}

TEST_F(VisibleUnitsTest, logicalEndOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(seven, 7), logicalEndOfLine(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(seven, 7), logicalEndOfLine(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), logicalEndOfLine(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    // TODO(yosin) logicalEndOfLine(two, 1) -> (five, 5) is a broken result. We keep
    // it as a marker for future change.
    EXPECT_EQ(Position(five, 5), logicalEndOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), logicalEndOfLine(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), logicalEndOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), logicalEndOfLine(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), logicalEndOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 4), logicalEndOfLine(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), logicalEndOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), logicalEndOfLine(createVisiblePositionInComposedTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 6), logicalEndOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(six, 6), logicalEndOfLine(createVisiblePositionInComposedTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(seven, 7), logicalEndOfLine(createVisiblePositionInComposedTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, logicalStartOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    // TODO(yosin) logicalStartOfLine(four, 1) -> (two, 2) is a broken result.
    // We keep it as a marker for future change.
    EXPECT_EQ(Position(two, 2), logicalStartOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(six, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 0), logicalStartOfLine(createVisiblePositionInComposedTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");

    EXPECT_EQ(Position::lastPositionInNode(host.get()), mostForwardCaretPosition(Position::afterNode(host.get())));
    EXPECT_EQ(PositionInComposedTree::lastPositionInNode(host.get()), mostForwardCaretPosition(PositionInComposedTree::afterNode(host.get())));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionFirstLetter)
{
    // Note: first-letter pseudo element contains letter and punctuations.
    const char* bodyContent = "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();

    Node* sample = document().getElementById("sample")->firstChild();

    EXPECT_EQ(Position(sample->parentNode(), 0), mostBackwardCaretPosition(Position(sample, 0)));
    EXPECT_EQ(Position(sample->parentNode(), 0), mostBackwardCaretPosition(Position(sample, 1)));
    EXPECT_EQ(Position(sample, 2), mostBackwardCaretPosition(Position(sample, 2)));
    EXPECT_EQ(Position(sample, 3), mostBackwardCaretPosition(Position(sample, 3)));
    EXPECT_EQ(Position(sample, 4), mostBackwardCaretPosition(Position(sample, 4)));
    EXPECT_EQ(Position(sample, 5), mostBackwardCaretPosition(Position(sample, 5)));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position(sample, 6)));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position(sample, 7)));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position::lastPositionInNode(sample->parentNode())));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position::afterNode(sample->parentNode())));
    EXPECT_EQ(Position::lastPositionInNode(document().body()), mostBackwardCaretPosition(Position::lastPositionInNode(document().body())));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> host = document().getElementById("host");
    RefPtrWillBeRawPtr<Element> one = document().getElementById("one");
    RefPtrWillBeRawPtr<Element> three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one->firstChild(), 1), mostBackwardCaretPosition(Position::afterNode(host.get())));
    EXPECT_EQ(PositionInComposedTree(three->firstChild(), 3), mostBackwardCaretPosition(PositionInComposedTree::afterNode(host.get())));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionFirstLetter)
{
    // Note: first-letter pseudo element contains letter and punctuations.
    const char* bodyContent = "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();

    Node* sample = document().getElementById("sample")->firstChild();

    EXPECT_EQ(Position(document().body(), 0), mostForwardCaretPosition(Position::firstPositionInNode(document().body())));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position::beforeNode(sample->parentNode())));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position::firstPositionInNode(sample->parentNode())));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position(sample, 0)));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position(sample, 1)));
    EXPECT_EQ(Position(sample, 2), mostForwardCaretPosition(Position(sample, 2)));
    EXPECT_EQ(Position(sample, 3), mostForwardCaretPosition(Position(sample, 3)));
    EXPECT_EQ(Position(sample, 4), mostForwardCaretPosition(Position(sample, 4)));
    EXPECT_EQ(Position(sample, 5), mostForwardCaretPosition(Position(sample, 5)));
    EXPECT_EQ(Position(sample, 7), mostForwardCaretPosition(Position(sample, 6)));
    EXPECT_EQ(Position(sample, 7), mostForwardCaretPosition(Position(sample, 7)));
}

TEST_F(VisibleUnitsTest, nextPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>55555</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Element* zero = document().getElementById("zero");
    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");
    Element* three = document().getElementById("three");
    Element* four = shadowRoot->getElementById("four");
    Element* five = shadowRoot->getElementById("five");

    EXPECT_EQ(Position(one->firstChild(), 0), nextPositionOf(createVisiblePosition(Position(zero, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four->firstChild(), 0), nextPositionOf(createVisiblePosition(PositionInComposedTree(zero, 1))).deepEquivalent());

    EXPECT_EQ(Position(one->firstChild(), 1), nextPositionOf(createVisiblePosition(Position(one, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInComposedTree(one, 0))).deepEquivalent());

    EXPECT_EQ(Position(two->firstChild(), 1), nextPositionOf(createVisiblePosition(Position(one, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInComposedTree(one, 1))).deepEquivalent());

    EXPECT_EQ(Position(three->firstChild(), 0), nextPositionOf(createVisiblePosition(Position(two, 2))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInComposedTree(two, 2))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, previousPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>55555</b>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* zero = document().getElementById("zero")->firstChild();
    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(Position(zero, 0), previousPositionOf(createVisiblePosition(Position(zero, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(zero, 0), previousPositionOf(createVisiblePosition(PositionInComposedTree(zero, 1))).deepEquivalent());

    EXPECT_EQ(Position(zero, 1), previousPositionOf(createVisiblePosition(Position(one, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 1), previousPositionOf(createVisiblePosition(PositionInComposedTree(one, 0))).deepEquivalent());

    EXPECT_EQ(Position(one, 0), previousPositionOf(createVisiblePosition(Position(one, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 2), previousPositionOf(createVisiblePosition(PositionInComposedTree(one, 1))).deepEquivalent());

    EXPECT_EQ(Position(one, 0), previousPositionOf(createVisiblePosition(Position(two, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 3), previousPositionOf(createVisiblePosition(PositionInComposedTree(two, 0))).deepEquivalent());

    // DOM tree to shadow tree
    EXPECT_EQ(Position(two, 2), previousPositionOf(createVisiblePosition(Position(three, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 5), previousPositionOf(createVisiblePosition(PositionInComposedTree(three, 0))).deepEquivalent());

    // Shadow tree to DOM tree
    EXPECT_EQ(Position(), previousPositionOf(createVisiblePosition(Position(four, 0))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(zero, 1), previousPositionOf(createVisiblePosition(PositionInComposedTree(four, 0))).deepEquivalent());

    // Note: Canonicalization maps (five, 0) to (four, 4) in DOM tree and
    // (one, 1) in composed tree.
    EXPECT_EQ(Position(four, 4), previousPositionOf(createVisiblePosition(Position(five, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 1), previousPositionOf(createVisiblePosition(PositionInComposedTree(five, 1))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, previousPositionOfOneCharPerLine)
{
    const char* bodyContent = "<div id=sample style='font-size: 500px'>A&#x714a;&#xfa67;</div>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample")->firstChild();

    // In case of each line has one character, VisiblePosition are:
    // [C,Dn]   [C,Up]  [B, Dn]   [B, Up]
    //  A        A       A         A|
    //  B        B|     |B         B
    // |C        C       C         C
    EXPECT_EQ(PositionWithAffinity(Position(sample, 1)), previousPositionOf(createVisiblePosition(Position(sample, 2))).toPositionWithAffinity());
    EXPECT_EQ(PositionWithAffinity(Position(sample, 1)), previousPositionOf(createVisiblePosition(Position(sample, 2), TextAffinity::Upstream)).toPositionWithAffinity());
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchor)
{
    const char* bodyContent = "<p id='sample'>00</p>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();
    RefPtrWillBeRawPtr<Element> sample = document().getElementById("sample");

    EXPECT_FALSE(rendersInDifferentPosition(Position::afterNode(sample.get()), Position(sample.get(), 1)));
    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(sample.get()), Position(sample.get(), 1)));
}

TEST_F(VisibleUnitsTest, renderedOffset)
{
    const char* bodyContent = "<div contenteditable><span id='sample1'>1</span><span id='sample2'>22</span></div>";
    setBodyContent(bodyContent);
    updateLayoutAndStyleForPainting();
    RefPtrWillBeRawPtr<Element> sample1 = document().getElementById("sample1");
    RefPtrWillBeRawPtr<Element> sample2 = document().getElementById("sample2");

    EXPECT_FALSE(rendersInDifferentPosition(Position::afterNode(sample1->firstChild()), Position(sample2->firstChild(), 0)));
    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(sample1->firstChild()), Position(sample2->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, rightPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<p id=four>4444</p><content select=#two></content><content select=#one></content><p id=five>55555</p>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(Position(), rightPositionOf(createVisiblePosition(Position(one, 1))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 0), rightPositionOf(createVisiblePosition(PositionInComposedTree(one, 1))).deepEquivalent());

    EXPECT_EQ(Position(one, 1), rightPositionOf(createVisiblePosition(Position(two, 2))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 1), rightPositionOf(createVisiblePosition(PositionInComposedTree(two, 2))).deepEquivalent());

    EXPECT_EQ(Position(five, 0), rightPositionOf(createVisiblePosition(Position(four, 4))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 0), rightPositionOf(createVisiblePosition(PositionInComposedTree(four, 4))).deepEquivalent());

    EXPECT_EQ(Position(), rightPositionOf(createVisiblePosition(Position(five, 5))).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), rightPositionOf(createVisiblePosition(PositionInComposedTree(five, 5))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfDocument)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfDocument(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 0), startOfDocument(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfDocument(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 0), startOfDocument(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 0), startOfLine(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 0), startOfLine(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 0), startOfLine(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), startOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 0), startOfLine(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), startOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfLine(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    // TODO(yosin) startOfLine(four, 1) -> (two, 2) is a broken result. We keep
    // it as a marker for future change.
    EXPECT_EQ(Position(two, 2), startOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfLine(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), startOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(five, 0), startOfLine(createVisiblePositionInComposedTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 0), startOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(six, 0), startOfLine(createVisiblePositionInComposedTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 0), startOfLine(createVisiblePositionInComposedTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfParagraph)
{
    const char* bodyContent = "<b id=zero>0</b><a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* zero = document().getElementById("zero")->firstChild();
    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();

    EXPECT_EQ(Position(zero, 0), startOfParagraph(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(one, 0), startOfParagraph(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(zero, 0), startOfParagraph(createVisiblePositionInDOMTree(*two, 2)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(two, 0), startOfParagraph(createVisiblePositionInComposedTree(*two, 2)).deepEquivalent());

    // DOM version of |startOfParagraph()| doesn't take account contents in
    // shadow tree.
    EXPECT_EQ(Position(zero, 0), startOfParagraph(createVisiblePositionInDOMTree(*three, 2)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfParagraph(createVisiblePositionInComposedTree(*three, 2)).deepEquivalent());

    // crbug.com/563777. startOfParagraph() unexpectedly returned a null
    // position with nested editable <BODY>s.
    Element* root = document().documentElement();
    root->setInnerHTML("<style>* { display:inline-table; }</style><body contenteditable=true><svg><svg><foreignObject>abc<svg></svg></foreignObject></svg></svg></body>", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> oldBody = document().body();
    root->setInnerHTML("<body contenteditable=true><svg><foreignObject><style>def</style>", ASSERT_NO_EXCEPTION);
    ASSERT(oldBody != document().body());
    Node* foreignObject = document().body()->firstChild()->firstChild();
    foreignObject->insertBefore(oldBody.get(), foreignObject->firstChild());
    Node* styleText = foreignObject->lastChild()->firstChild();
    ASSERT(styleText->isTextNode());
    updateLayoutAndStyleForPainting();

    EXPECT_FALSE(startOfParagraph(createVisiblePosition(Position(styleText, 0))).isNull());
}

TEST_F(VisibleUnitsTest, startOfSentence)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><i id=three>333</i> <content select=#two></content> <content select=#one></content> <i id=four>4444</i></p>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = shadowRoot->getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfSentence(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfSentence(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfSentence(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfSentence(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 0), startOfSentence(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfSentence(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 0), startOfSentence(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(three, 0), startOfSentence(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfWord)
{
    const char* bodyContent = "<a id=host><b id=one>1</b> <b id=two>22</b></a><i id=three>333</i>";
    const char* shadowContent = "<p><u id=four>44444</u><content select=#two></content><span id=space> </span><content select=#one></content><u id=five>55555</u></p>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent, "host");
    updateLayoutAndStyleForPainting();

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* space = shadowRoot->getElementById("space")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(space, 1), startOfWord(createVisiblePositionInComposedTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(space, 1), startOfWord(createVisiblePositionInComposedTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 0), startOfWord(createVisiblePositionInComposedTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 0), startOfWord(createVisiblePositionInComposedTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(space, 1), startOfWord(createVisiblePositionInComposedTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 0), startOfWord(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(four, 0), startOfWord(createVisiblePositionInComposedTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(space, 1), startOfWord(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInComposedTree(space, 1), startOfWord(createVisiblePositionInComposedTree(*five, 1)).deepEquivalent());
}

} // namespace blink
