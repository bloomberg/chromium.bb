// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/FocusController.h"

#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FocusControllerTest : public testing::Test {
public:
    Document& document() const { return m_pageHolder->document(); }
    FocusController& focusController() const { return document().page()->focusController(); }

private:
    void SetUp() override { m_pageHolder = DummyPageHolder::create(); }

    OwnPtr<DummyPageHolder> m_pageHolder;
};

TEST_F(FocusControllerTest, SetInitialFocus)
{
    document().body()->setInnerHTML("<input><textarea>", ASSERT_NO_EXCEPTION);
    Element* input = toElement(document().body()->firstChild());
    // Set sequential focus navigation point before the initial focus.
    input->focus();
    input->blur();
    focusController().setInitialFocus(WebFocusTypeForward);
    EXPECT_EQ(input, document().focusedElement()) << "We should ignore sequential focus navigation starting point in setInitialFocus().";
}

} // namespace blink
