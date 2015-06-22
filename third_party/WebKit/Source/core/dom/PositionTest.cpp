// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/Position.h"

#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/testing/CoreTestHelpers.h"
#include "wtf/RefPtr.h"
#include <gtest/gtest.h>

namespace blink {

class PositionTest : public ::testing::Test {
protected:
    void SetUp() override;

    HTMLDocument& document() const;
    void setBodyContent(const char*);
    PassRefPtrWillBeRawPtr<ShadowRoot> setShadowContent(const char*);

private:
    RefPtrWillBePersistent<HTMLDocument> m_document;
};

void PositionTest::SetUp()
{
    m_document = HTMLDocument::create();
    RefPtrWillBeRawPtr<HTMLHtmlElement> html = HTMLHtmlElement::create(*m_document);
    html->appendChild(HTMLBodyElement::create(*m_document));
    m_document->appendChild(html.release());
}

HTMLDocument& PositionTest::document() const
{
    return *m_document;
}

void PositionTest::setBodyContent(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

static PassRefPtrWillBeRawPtr<ShadowRoot> createShadowRootForElementWithIDAndSetInnerHTML(TreeScope& scope, const char* hostElementID, const char* shadowRootContent)
{
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = scope.getElementById(AtomicString::fromUTF8(hostElementID))->createShadowRoot(ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowRootContent), ASSERT_NO_EXCEPTION);
    return shadowRoot.release();
}

PassRefPtrWillBeRawPtr<ShadowRoot> PositionTest::setShadowContent(const char* shadowContent)
{
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    document().recalcDistribution();
    return shadowRoot;
}

TEST_F(PositionTest, NodeAsRangeLastNodeNull)
{
    EXPECT_EQ(nullptr, Position().nodeAsRangeLastNode());
    EXPECT_EQ(nullptr, PositionInComposedTree().nodeAsRangeLastNode());
}

TEST_F(PositionTest, NodeAsRangeLastNode)
{
    const char* bodyContent = "<p id='p1'>11</p><p id='p2'></p><p id='p3'>33</p>";
    setBodyContent(bodyContent);
    Node* p1 = document().getElementById("p1");
    Node* p2 = document().getElementById("p2");
    Node* p3 = document().getElementById("p3");
    Node* body = EditingStrategy::parent(*p1);
    Node* t1 = EditingStrategy::firstChild(*p1);
    Node* t3 = EditingStrategy::firstChild(*p3);

    EXPECT_EQ(body, Position::inParentBeforeNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentBeforeNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(p2, Position::inParentBeforeNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentAfterNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(p2, Position::inParentAfterNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(t3, Position::inParentAfterNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t3, Position::afterNode(p3).nodeAsRangeLastNode());

    EXPECT_EQ(body, PositionInComposedTree::inParentBeforeNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::inParentBeforeNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(p2, PositionInComposedTree::inParentBeforeNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::inParentAfterNode(*p1).nodeAsRangeLastNode());
    EXPECT_EQ(p2, PositionInComposedTree::inParentAfterNode(*p2).nodeAsRangeLastNode());
    EXPECT_EQ(t3, PositionInComposedTree::inParentAfterNode(*p3).nodeAsRangeLastNode());
    EXPECT_EQ(t3, PositionInComposedTree::afterNode(p3).nodeAsRangeLastNode());
}

TEST_F(PositionTest, NodeAsRangeLastNodeShadow)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a id='a'><content select=#two></content><content select=#one></content></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    Node* host = document().getElementById("host");
    Node* n1 = document().getElementById("one");
    Node* n2 = document().getElementById("two");
    Node* t0 = EditingStrategy::firstChild(*host);
    Node* t1 = EditingStrategy::firstChild(*n1);
    Node* t2 = EditingStrategy::firstChild(*n2);
    Node* t3 = EditingStrategy::lastChild(*host);
    Node* a = shadowRoot->getElementById("a");

    EXPECT_EQ(t0, Position::inParentBeforeNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentBeforeNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t1, Position::inParentAfterNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(t2, Position::inParentAfterNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t3, Position::afterNode(host).nodeAsRangeLastNode());

    EXPECT_EQ(t2, PositionInComposedTree::inParentBeforeNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(a, PositionInComposedTree::inParentBeforeNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::inParentAfterNode(*n1).nodeAsRangeLastNode());
    EXPECT_EQ(t2, PositionInComposedTree::inParentAfterNode(*n2).nodeAsRangeLastNode());
    EXPECT_EQ(t1, PositionInComposedTree::afterNode(host).nodeAsRangeLastNode());
}

} // namespace blink
