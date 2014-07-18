// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/OwnPtr.h"
#include <gtest/gtest.h>

namespace blink {

class RenderingTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        m_pageHolder = DummyPageHolder::create(IntSize(800, 600));

        // This ensures that the minimal DOM tree gets attached
        // correctly for tests that don't call setBodyInnerHTML.
        document().view()->updateLayoutAndStyleIfNeededRecursive();
    }

    Document& document() const { return m_pageHolder->document(); }

    void setBodyInnerHTML(const char* htmlContent)
    {
        document().body()->setInnerHTML(String::fromUTF8(htmlContent), ASSERT_NO_EXCEPTION);
        document().view()->updateLayoutAndStyleIfNeededRecursive();
    }

private:
    OwnPtr<DummyPageHolder> m_pageHolder;
};

} // namespace blink
