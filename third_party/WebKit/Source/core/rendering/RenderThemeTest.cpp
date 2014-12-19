// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderTheme.h"

#include "core/dom/NodeRenderStyle.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/Color.h"
#include <gtest/gtest.h>

using namespace blink;

namespace {

class RenderThemeTest : public ::testing::Test {

protected:
    virtual void SetUp() override;
    HTMLDocument& document() const { return *m_document; }
    void setHtmlInnerHTML(const char* htmlContent);

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    HTMLDocument* m_document;
};

void RenderThemeTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    ASSERT(m_document);
}

void RenderThemeTest::setHtmlInnerHTML(const char* htmlContent)
{
    document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent), ASSERT_NO_EXCEPTION);
    document().view()->updateLayoutAndStyleIfNeededRecursive();
}

inline Color outlineColor(Element* element)
{
    return element->renderStyle()->visitedDependentColor(CSSPropertyOutlineColor);
}

inline EBorderStyle outlineStyle(Element* element)
{
    return element->renderStyle()->outlineStyle();
}

TEST_F(RenderThemeTest, ChangeFocusRingColor)
{
    setHtmlInnerHTML("<span id=span tabIndex=0>Span</span>");

    Element* span = document().getElementById(AtomicString("span"));
    EXPECT_NE(nullptr, span);
    EXPECT_NE(nullptr, span->renderer());

    Color customColor = makeRGB(123, 145, 167);

    // Checking unfocused style.
    EXPECT_EQ(BNONE, outlineStyle(span));
    EXPECT_NE(customColor, outlineColor(span));

    // Do focus.
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    span->focus();
    document().view()->updateLayoutAndStyleIfNeededRecursive();

    // Checking focused style.
    EXPECT_NE(BNONE, outlineStyle(span));
    EXPECT_NE(customColor, outlineColor(span));

    // Change focus ring color.
    RenderTheme::theme().setCustomFocusRingColor(customColor);
    Page::platformColorsChanged();
    document().view()->updateLayoutAndStyleIfNeededRecursive();

    // Check that the focus ring color is updated.
    EXPECT_NE(BNONE, outlineStyle(span));
    EXPECT_EQ(customColor, outlineColor(span));
}

}
