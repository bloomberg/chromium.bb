// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTestHelper_h
#define LayoutTestHelper_h

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/Allocator.h"
#include "wtf/OwnPtr.h"
#include <gtest/gtest.h>

namespace blink {

class RenderingTest : public testing::Test {
    WTF_MAKE_FAST_ALLOCATED(RenderingTest);
public:
    virtual FrameSettingOverrideFunction settingOverrider() const { return nullptr; }

protected:
    void SetUp() override;
    void TearDown() override;

    Document& document() const { return m_pageHolder->document(); }

    // Both sets the inner html and runs the document lifecycle.
    void setBodyInnerHTML(const String& htmlContent)
    {
        document().body()->setInnerHTML(htmlContent, ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
    }

    // Both enables compositing and runs the document lifecycle.
    void enableCompositing()
    {
        m_pageHolder->page().settings().setAcceleratedCompositingEnabled(true);
        document().view()->updateAllLifecyclePhases();
    }

private:
    OwnPtr<DummyPageHolder> m_pageHolder;
};

} // namespace blink

#endif // LayoutTestHelper_h
