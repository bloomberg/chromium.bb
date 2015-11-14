// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/shadow/MediaControls.h"

#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

Element* getElementByShadowPseudoId(Node& rootNode, const char* shadowPseudoId)
{
    for (Element& element : ElementTraversal::descendantsOf(rootNode)) {
        if (element.fastGetAttribute(HTMLNames::pseudoAttr) == shadowPseudoId)
            return &element;
    }
    return nullptr;
}

bool isElementVisible(Element& element)
{
    const StylePropertySet* inlineStyle = element.inlineStyle();

    if (!inlineStyle)
        return true;

    if (inlineStyle->getPropertyValue(CSSPropertyDisplay) == "none")
        return false;

    if (inlineStyle->hasProperty(CSSPropertyOpacity)
        && inlineStyle->getPropertyValue(CSSPropertyOpacity).toDouble() == 0.0)
        return false;

    if (inlineStyle->getPropertyValue(CSSPropertyVisibility) == "hidden")
        return false;

    if (Element* parent = element.parentElement())
        return isElementVisible(*parent);

    return true;
}

} // namespace

class MediaControlsTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
        Document& document = this->document();
        document.write("<video controls>");
        HTMLVideoElement& video = toHTMLVideoElement(*document.querySelector("video", ASSERT_NO_EXCEPTION));
        m_mediaControls = video.mediaControls();
    }

    MediaControls& mediaControls() { return *m_mediaControls; }
    Document& document() { return m_pageHolder->document(); }

private:
    OwnPtr<DummyPageHolder> m_pageHolder;
    MediaControls* m_mediaControls;
};

TEST_F(MediaControlsTest, HideAndShow)
{
    Element* panel = getElementByShadowPseudoId(mediaControls(), "-webkit-media-controls-panel");
    ASSERT_NE(nullptr, panel);

    ASSERT_TRUE(isElementVisible(*panel));
    mediaControls().hide();
    ASSERT_FALSE(isElementVisible(*panel));
    mediaControls().show();
    ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, Reset)
{
    Element* panel = getElementByShadowPseudoId(mediaControls(), "-webkit-media-controls-panel");
    ASSERT_NE(nullptr, panel);

    ASSERT_TRUE(isElementVisible(*panel));
    mediaControls().reset();
    ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, HideAndReset)
{
    Element* panel = getElementByShadowPseudoId(mediaControls(), "-webkit-media-controls-panel");
    ASSERT_NE(nullptr, panel);

    ASSERT_TRUE(isElementVisible(*panel));
    mediaControls().hide();
    ASSERT_FALSE(isElementVisible(*panel));
    mediaControls().reset();
    ASSERT_FALSE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, ResetDoesNotTriggerInitialLayout)
{
    Document& document = this->document();
    int oldResolverCount = document.styleEngine().resolverAccessCount();
    // Also assert that there are no layouts yet.
    ASSERT_EQ(0, oldResolverCount);
    mediaControls().reset();
    int newResolverCount = document.styleEngine().resolverAccessCount();
    ASSERT_EQ(oldResolverCount, newResolverCount);
}

} // namespace blink
