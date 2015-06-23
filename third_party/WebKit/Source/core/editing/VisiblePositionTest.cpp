// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/VisiblePosition.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/html/HTMLElement.h"
#include "core/testing/CoreTestHelpers.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

Position positionInDOMTree(Node& anchor, int offset)
{
    return Position(&anchor, offset, Position::PositionIsOffsetInAnchor);
}

PositionInComposedTree positionInComposedTree(Node& anchor, int offset)
{
    return PositionInComposedTree(&anchor, offset, PositionInComposedTree::PositionIsOffsetInAnchor);
}

} // namespace

class VisiblePositionTest : public ::testing::Test {
protected:
    void SetUp() override;

    Document& document() const { return m_dummyPageHolder->document(); }

    static PassRefPtrWillBeRawPtr<ShadowRoot> createShadowRootForElementWithIDAndSetInnerHTML(TreeScope&, const char* hostElementID, const char* shadowRootContent);

    void setBodyContent(const char*);
    PassRefPtrWillBeRawPtr<ShadowRoot> setShadowContent(const char*);

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
};

void VisiblePositionTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

PassRefPtrWillBeRawPtr<ShadowRoot> VisiblePositionTest::createShadowRootForElementWithIDAndSetInnerHTML(TreeScope& scope, const char* hostElementID, const char* shadowRootContent)
{
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = scope.getElementById(AtomicString::fromUTF8(hostElementID))->createShadowRoot(ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowRootContent), ASSERT_NO_EXCEPTION);
    return shadowRoot.release();
}

void VisiblePositionTest::setBodyContent(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

PassRefPtrWillBeRawPtr<ShadowRoot> VisiblePositionTest::setShadowContent(const char* shadowContent)
{
    return createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
}

TEST_F(VisiblePositionTest, ShadowDistributedNodes)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    setBodyContent(bodyContent);
    RefPtrWillBeRawPtr<ShadowRoot> shadowRoot = setShadowContent(shadowContent);

    RefPtrWillBeRawPtr<Element> body = document().body();
    RefPtrWillBeRawPtr<Element> one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> four = shadowRoot->querySelector("#s4", ASSERT_NO_EXCEPTION);
    RefPtrWillBeRawPtr<Element> five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(positionInDOMTree(*one->firstChild(), 0), canonicalPositionOf(positionInDOMTree(*one, 0)));
    EXPECT_EQ(positionInDOMTree(*one->firstChild(), 2), canonicalPositionOf(positionInDOMTree(*two, 0)));

    EXPECT_EQ(positionInComposedTree(*five->firstChild(), 2), canonicalPositionOf(positionInComposedTree(*one, 0)));
    EXPECT_EQ(positionInComposedTree(*four->firstChild(), 2), canonicalPositionOf(positionInComposedTree(*two, 0)));
}

} // namespace blink
