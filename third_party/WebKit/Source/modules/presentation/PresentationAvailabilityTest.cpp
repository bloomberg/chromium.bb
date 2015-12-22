// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailability.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

namespace blink {
namespace {

class PresentationAvailabilityTest : public ::testing::Test {
public:
    PresentationAvailabilityTest()
        : m_scope(v8::Isolate::GetCurrent())
        , m_page(DummyPageHolder::create())
    {
    }

    void SetUp() override
    {
        m_scope.scriptState()->setExecutionContext(&m_page->document());
    }

    Page& page() { return m_page->page(); }
    LocalFrame& frame() { return m_page->frame(); }
    ScriptState* scriptState() { return m_scope.scriptState(); }

private:
    V8TestingScope m_scope;
    OwnPtr<DummyPageHolder> m_page;
};

TEST_F(PresentationAvailabilityTest, NoPageVisibilityChangeAfterDetach)
{
    const KURL url = URLTestHelpers::toKURL("https://example.com");
    Persistent<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState());
    Persistent<PresentationAvailability> availability = PresentationAvailability::take(resolver, url, false);

    // These two calls should not crash.
    frame().detach(FrameDetachType::Remove);
    page().setVisibilityState(PageVisibilityStateHidden, false);
}

} // anonymous namespace
} // namespace blink
