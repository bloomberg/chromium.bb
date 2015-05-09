/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/editing/iterators/TextIterator.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntSize.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/testing/WTFTestHelpers.h"
#include <gtest/gtest.h>
#include <string>

using namespace blink;

namespace {

struct DOMTree : NodeTraversal {
    using PositionType = Position;
    using TextIteratorType = TextIterator;
};

struct ComposedTree : ComposedTreeTraversal {
    using PositionType = PositionInComposedTree;
    using TextIteratorType = TextIteratorInComposedTree;
};

class TextIteratorTest : public ::testing::Test {
protected:
    virtual void SetUp() override;

    HTMLDocument& document() const { return *m_document; }

    template <typename Tree>
    std::string iterate(TextIteratorBehavior = TextIteratorDefaultBehavior);

    template <typename Tree>
    std::string iteratePartial(const typename Tree::PositionType& start, const typename Tree::PositionType& end, TextIteratorBehavior = TextIteratorDefaultBehavior);

    void setBodyInnerHTML(const char*);

    PassRefPtrWillBeRawPtr<Range> getBodyRange() const;

private:
    template <typename Tree>
    std::string iterateWithIterator(typename Tree::TextIteratorType&);

    OwnPtr<DummyPageHolder> m_dummyPageHolder;

    HTMLDocument* m_document;
};

void TextIteratorTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    ASSERT(m_document);
}

template <typename Tree>
std::string TextIteratorTest::iterate(TextIteratorBehavior iteratorBehavior)
{
    RefPtrWillBeRawPtr<Element> body = document().body();
    using PositionType = typename Tree::PositionType;
    auto start = PositionType(body, 0, PositionType::PositionIsOffsetInAnchor);
    auto end = PositionType(body, Tree::countChildren(*body), PositionType::PositionIsOffsetInAnchor);
    typename Tree::TextIteratorType iterator(start, end, iteratorBehavior);
    return iterateWithIterator<Tree>(iterator);
}

template <typename Tree>
std::string TextIteratorTest::iteratePartial(const typename Tree::PositionType& start, const typename Tree::PositionType& end, TextIteratorBehavior iteratorBehavior)
{
    typename Tree::TextIteratorType iterator(start, end, iteratorBehavior);
    return iterateWithIterator<Tree>(iterator);
}

template <typename Tree>
std::string TextIteratorTest::iterateWithIterator(typename Tree::TextIteratorType& iterator)
{
    String textChunks;
    for (; !iterator.atEnd(); iterator.advance()) {
        textChunks.append('[');
        textChunks.append(iterator.text().substring(0, iterator.text().length()));
        textChunks.append(']');
    }
    return std::string(textChunks.utf8().data());
}

void TextIteratorTest::setBodyInnerHTML(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

PassRefPtrWillBeRawPtr<Range> TextIteratorTest::getBodyRange() const
{
    RefPtrWillBeRawPtr<Range> range(Range::create(document()));
    range->selectNode(document().body());
    return range.release();
}

PassRefPtrWillBeRawPtr<ShadowRoot> createShadowRootForElementWithIDAndSetInnerHTML(TreeScope& scope, const char* hostElementID, const char* shadowRootContent)
{
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = scope.getElementById(AtomicString::fromUTF8(hostElementID))->createShadowRoot(ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowRootContent), ASSERT_NO_EXCEPTION);
    return shadowRoot.release();
}

TEST_F(TextIteratorTest, BitStackOverflow)
{
    const unsigned bitsInWord = sizeof(unsigned) * 8;
    BitStack bs;

    for (unsigned i = 0; i < bitsInWord + 1u; i++)
        bs.push(true);

    bs.pop();

    EXPECT_TRUE(bs.top());
}

TEST_F(TextIteratorTest, BasicIteration)
{
    static const char* input = "<p>Hello, \ntext</p><p>iterator.</p>";
    setBodyInnerHTML(input);
    EXPECT_EQ("[Hello, ][text][\n][\n][iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][text][\n][\n][iterator.]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, IgnoreAltTextInTextControls)
{
    static const char* input = "<p>Hello <input type='text' value='value'>!</p>";
    setBodyInnerHTML(input);
    EXPECT_EQ("[Hello ][][!]", iterate<DOMTree>(TextIteratorEmitsImageAltText));
    EXPECT_EQ("[Hello ][][\n][value][\n][!]", iterate<ComposedTree>(TextIteratorEmitsImageAltText));
}

TEST_F(TextIteratorTest, DisplayAltTextInImageControls)
{
    static const char* input = "<p>Hello <input type='image' alt='alt'>!</p>";
    setBodyInnerHTML(input);
    EXPECT_EQ("[Hello ][alt][!]", iterate<DOMTree>(TextIteratorEmitsImageAltText));
    EXPECT_EQ("[Hello ][alt][!]", iterate<ComposedTree>(TextIteratorEmitsImageAltText));
}

TEST_F(TextIteratorTest, NotEnteringTextControls)
{
    static const char* input = "<p>Hello <input type='text' value='input'>!</p>";
    setBodyInnerHTML(input);
    EXPECT_EQ("[Hello ][][!]", iterate<DOMTree>());
    EXPECT_EQ("[Hello ][][\n][input][\n][!]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, EnteringTextControlsWithOption)
{
    static const char* input = "<p>Hello <input type='text' value='input'>!</p>";
    setBodyInnerHTML(input);
    EXPECT_EQ("[Hello ][\n][input][!]", iterate<DOMTree>(TextIteratorEntersTextControls));
    EXPECT_EQ("[Hello ][][\n][input][\n][!]", iterate<ComposedTree>(TextIteratorEntersTextControls));
}

TEST_F(TextIteratorTest, EnteringTextControlsWithOptionComplex)
{
    static const char* input = "<input type='text' value='Beginning of range'><div><div><input type='text' value='Under DOM nodes'></div></div><input type='text' value='End of range'>";
    setBodyInnerHTML(input);
    EXPECT_EQ("[\n][Beginning of range][\n][Under DOM nodes][\n][End of range]", iterate<DOMTree>(TextIteratorEntersTextControls));
    EXPECT_EQ("[][\n][Beginning of range][\n][][\n][Under DOM nodes][\n][][\n][End of range]", iterate<ComposedTree>(TextIteratorEntersTextControls));
}

TEST_F(TextIteratorTest, NotEnteringTextControlHostingShadowTreeEvenWithOption)
{
    static const char* bodyContent = "<div>Hello, <input type='text' value='input' id='input'> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    // TextIterator doesn't emit "input" nor "shadow" since (1) the layoutObject for <input> is not created; and
    // (2) we don't (yet) recurse into shadow trees.
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "input", shadowContent);
    // FIXME: Why is an empty string emitted here?
    EXPECT_EQ("[Hello, ][][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][][shadow][ iterator.]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, NotEnteringShadowTree)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // TextIterator doesn't emit "text" since its layoutObject is not created. The shadow tree is ignored.
    EXPECT_EQ("[Hello, ][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][shadow][ iterator.]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithMultipleShadowTrees)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first shadow</span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent2);
    EXPECT_EQ("[Hello, ][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][second shadow][ iterator.]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithNestedShadowTrees)
{
    static const char* bodyContent = "<div>Hello, <span id='host-in-document'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first <span id='host-in-shadow'>shadow</span></span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyInnerHTML(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot1 = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host-in-document", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host-in-shadow", shadowContent2);
    EXPECT_EQ("[Hello, ][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithContentInsertionPoint)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow <content>content</content></span>";
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // In this case a layoutObject for "text" is created, so it shows up here.
    EXPECT_EQ("[Hello, ][text][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][shadow ][text][ iterator.]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // TextIterator emits "shadow" since TextIteratorEntersOpenShadowRoots is specified.
    EXPECT_EQ("[Hello, ][shadow][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][shadow][ iterator.]", iterate<ComposedTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithMultipleShadowTreesWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first shadow</span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent2);
    // The first isn't emitted because a layoutObject for the first is not created.
    EXPECT_EQ("[Hello, ][second shadow][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][second shadow][ iterator.]", iterate<ComposedTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithNestedShadowTreesWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host-in-document'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first <span id='host-in-shadow'>shadow</span></span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyInnerHTML(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot1 = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host-in-document", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host-in-shadow", shadowContent2);
    EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]", iterate<ComposedTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithContentInsertionPointWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span><content>content</content> shadow</span>";
    // In this case a layoutObject for "text" is created, and emitted AFTER any nodes in the shadow tree.
    // This order does not match the order of the rendered texts, but at this moment it's the expected behavior.
    // FIXME: Fix this. We probably need pure-renderer-based implementation of TextIterator to achieve this.
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    EXPECT_EQ("[Hello, ][ shadow][text][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][text][ shadow][ iterator.]", iterate<ComposedTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, StartingAtNodeInShadowRoot)
{
    static const char* bodyContent = "<div id='outer'>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span><content>content</content> shadow</span>";
    setBodyInnerHTML(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    Node* outerDiv = document().getElementById("outer");
    Node* spanInShadow = shadowRoot->firstChild();
    Position start(spanInShadow, Position::PositionIsBeforeChildren);
    Position end(outerDiv, Position::PositionIsAfterChildren);
    EXPECT_EQ("[ shadow][text][ iterator.]", iteratePartial<DOMTree>(start, end, TextIteratorEntersOpenShadowRoots));

    PositionInComposedTree startInComposedTree(spanInShadow, PositionInComposedTree::PositionIsBeforeChildren);
    PositionInComposedTree endInComposedTree(outerDiv, PositionInComposedTree::PositionIsAfterChildren);
    EXPECT_EQ("[text][ shadow][ iterator.]", iteratePartial<ComposedTree>(startInComposedTree, endInComposedTree, TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, FinishingAtNodeInShadowRoot)
{
    static const char* bodyContent = "<div id='outer'>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span><content>content</content> shadow</span>";
    setBodyInnerHTML(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    Node* outerDiv = document().getElementById("outer");
    Node* spanInShadow = shadowRoot->firstChild();
    Position start(outerDiv, Position::PositionIsBeforeChildren);
    Position end(spanInShadow, Position::PositionIsAfterChildren);
    EXPECT_EQ("[Hello, ][ shadow]", iteratePartial<DOMTree>(start, end, TextIteratorEntersOpenShadowRoots));

    PositionInComposedTree startInComposedTree(outerDiv, PositionInComposedTree::PositionIsBeforeChildren);
    PositionInComposedTree endInComposedTree(spanInShadow, PositionInComposedTree::PositionIsAfterChildren);
    EXPECT_EQ("[Hello, ][text][ shadow]", iteratePartial<ComposedTree>(startInComposedTree, endInComposedTree, TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, FullyClipsContents)
{
    static const char* bodyContent =
        "<div style='overflow: hidden; width: 200px; height: 0;'>"
        "I'm invisible"
        "</div>";
    setBodyInnerHTML(bodyContent);
    EXPECT_EQ("", iterate<DOMTree>());
    EXPECT_EQ("", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, IgnoresContainerClip)
{
    static const char* bodyContent =
        "<div style='overflow: hidden; width: 200px; height: 0;'>"
        "<div>I'm not visible</div>"
        "<div style='position: absolute; width: 200px; height: 200px; top: 0; right: 0;'>"
        "but I am!"
        "</div>"
        "</div>";
    setBodyInnerHTML(bodyContent);
    EXPECT_EQ("[but I am!]", iterate<DOMTree>());
    EXPECT_EQ("[but I am!]", iterate<ComposedTree>());
}

TEST_F(TextIteratorTest, FullyClippedContentsDistributed)
{
    static const char* bodyContent =
        "<div id='host'>"
        "<div>Am I visible?</div>"
        "</div>";
    static const char* shadowContent =
        "<div style='overflow: hidden; width: 200px; height: 0;'>"
        "<content></content>"
        "</div>";
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // FIXME: The text below is actually invisible but TextIterator currently thinks it's visible.
    EXPECT_EQ("[\n][Am I visible?]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("", iterate<ComposedTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, IgnoresContainersClipDistributed)
{
    static const char* bodyContent =
        "<div id='host' style='overflow: hidden; width: 200px; height: 0;'>"
        "<div>Nobody can find me!</div>"
        "</div>";
    static const char* shadowContent =
        "<div style='position: absolute; width: 200px; height: 200px; top: 0; right: 0;'>"
        "<content></content>"
        "</div>";
    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // FIXME: The text below is actually visible but TextIterator currently thinks it's invisible.
    // [\n][Nobody can find me!]
    EXPECT_EQ("", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Nobody can find me!]", iterate<ComposedTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, FindPlainTextInvalidTarget)
{
    static const char* bodyContent = "<div>foo bar test</div>";
    setBodyInnerHTML(bodyContent);
    RefPtrWillBeRawPtr<Range> range = getBodyRange();

    RefPtrWillBeRawPtr<Range> expectedRange = range->cloneRange();
    expectedRange->collapse(false);

    // A lone lead surrogate (0xDA0A) example taken from fuzz-58.
    static const UChar invalid1[] = {
        0x1461u, 0x2130u, 0x129bu, 0xd711u, 0xd6feu, 0xccadu, 0x7064u,
        0xd6a0u, 0x4e3bu, 0x03abu, 0x17dcu, 0xb8b7u, 0xbf55u, 0xfca0u,
        0x07fau, 0x0427u, 0xda0au, 0
    };

    // A lone trailing surrogate (U+DC01).
    static const UChar invalid2[] = {
        0x1461u, 0x2130u, 0x129bu, 0xdc01u, 0xd6feu, 0xccadu, 0
    };
    // A trailing surrogate followed by a lead surrogate (U+DC03 U+D901).
    static const UChar invalid3[] = {
        0xd800u, 0xdc00u, 0x0061u, 0xdc03u, 0xd901u, 0xccadu, 0
    };

    static const UChar* invalidUStrings[] = { invalid1, invalid2, invalid3 };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(invalidUStrings); ++i) {
        String invalidTarget(invalidUStrings[i]);
        RefPtrWillBeRawPtr<Range> actualRange = findPlainText(range.get(), invalidTarget, 0);
        EXPECT_TRUE(areRangesEqual(expectedRange.get(), actualRange.get()));
    }
}

TEST_F(TextIteratorTest, EmitsReplacementCharForInput)
{
    static const char* bodyContent =
        "<div contenteditable='true'>"
        "Before"
        "<img src='foo.png'>"
        "After"
        "</div>";
    setBodyInnerHTML(bodyContent);
    EXPECT_EQ("[Before][\xEF\xBF\xBC][After]", iterate<DOMTree>(TextIteratorEmitsObjectReplacementCharacter));
    EXPECT_EQ("[Before][\xEF\xBF\xBC][After]", iterate<ComposedTree>(TextIteratorEmitsObjectReplacementCharacter));
}

TEST_F(TextIteratorTest, RangeLengthWithReplacedElements)
{
    static const char* bodyContent =
        "<div id='div' contenteditable='true'>1<img src='foo.png'>3</div>";
    setBodyInnerHTML(bodyContent);
    document().view()->updateLayoutAndStyleForPainting();

    Node* divNode = document().getElementById("div");
    RefPtrWillBeRawPtr<Range> range = Range::create(document(), divNode, 0, divNode, 3);

    EXPECT_EQ(3, TextIterator::rangeLength(range->startPosition(), range->endPosition()));
}

TEST_F(TextIteratorTest, SubrangeWithReplacedElements)
{
    static const char* bodyContent =
        "<div id='div' contenteditable='true'>1<img src='foo.png'>345</div>";
    setBodyInnerHTML(bodyContent);
    document().view()->updateLayoutAndStyleForPainting();

    Node* divNode = document().getElementById("div");
    RefPtrWillBeRawPtr<Range> entireRange = Range::create(document(), divNode, 0, divNode, 3);

    RefPtrWillBeRawPtr<Range> subrange = TextIterator::subrange(entireRange.get(), 2, 3);
    EXPECT_EQ(0, subrange->startOffset());
    EXPECT_EQ(3, subrange->endOffset());
}

}
