// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorWebPerfAgent.h"

#include "core/frame/Location.h"
#include "core/inspector/InspectedFrames.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"

#include <memory>

namespace blink {

class InspectorWebPerfAgentTest : public ::testing::Test {
protected:
    void SetUp() override;
    LocalFrame* frame() const { return m_pageHolder->document().frame(); }
    ExecutionContext* executionContext() const { return &m_pageHolder->document(); }
    LocalFrame* anotherFrame() const { return m_anotherPageHolder->document().frame(); }
    ExecutionContext* anotherExecutionContext() const { return &m_anotherPageHolder->document(); }

    String frameContextURL();
    int numUniqueFrameContextsSeen();

    String sanitizedLongTaskName(
        HeapHashSet<Member<Location>> frameContextLocations, Frame* rootFrame) {
        return m_agent->sanitizedLongTaskName(frameContextLocations, rootFrame);
    }

    Persistent<InspectorWebPerfAgent> m_agent;
    std::unique_ptr<DummyPageHolder> m_pageHolder;
    std::unique_ptr<DummyPageHolder> m_anotherPageHolder;
};

void InspectorWebPerfAgentTest::SetUp()
{
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_pageHolder->document().setURL(KURL(KURL(), "https://example.com/foo"));
    m_agent = new InspectorWebPerfAgent(InspectedFrames::create(frame()));

    // Create another dummy page holder and pretend this is the iframe.
    m_anotherPageHolder = DummyPageHolder::create(IntSize(400, 300));
    m_anotherPageHolder->document().setURL(KURL(KURL(), "https://iframed.com/bar"));
}

String InspectorWebPerfAgentTest::frameContextURL()
{
    // This is reported only if there is a single frameContext URL.
    if (m_agent->m_frameContextLocations.size() != 1)
        return "";
    Location* location = (*m_agent->m_frameContextLocations.begin()).get();
    return location->href();
}

int InspectorWebPerfAgentTest::numUniqueFrameContextsSeen()
{
    return m_agent->m_frameContextLocations.size();
}

TEST_F(InspectorWebPerfAgentTest, SingleScriptInTask)
{
    m_agent->willProcessTask();
    EXPECT_EQ(0, numUniqueFrameContextsSeen());
    m_agent->willExecuteScript(executionContext());
    EXPECT_EQ(1, numUniqueFrameContextsSeen());
    m_agent->didExecuteScript();
    m_agent->didProcessTask();
    m_agent->ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923); // Long task
    EXPECT_EQ(1, numUniqueFrameContextsSeen());
    EXPECT_EQ("https://example.com/foo", frameContextURL());
}

TEST_F(InspectorWebPerfAgentTest, MultipleScriptsInTask_SingleContext)
{
    m_agent->willProcessTask();
    EXPECT_EQ(0, numUniqueFrameContextsSeen());
    m_agent->willExecuteScript(executionContext());
    EXPECT_EQ(1, numUniqueFrameContextsSeen());
    m_agent->didExecuteScript();
    EXPECT_EQ("https://example.com/foo", frameContextURL());

    m_agent->willExecuteScript(executionContext());
    EXPECT_EQ(1, numUniqueFrameContextsSeen());
    m_agent->didExecuteScript();
    m_agent->didProcessTask();
    m_agent->ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923); // Long task
    EXPECT_EQ(1, numUniqueFrameContextsSeen());
    EXPECT_EQ("https://example.com/foo", frameContextURL());
}

TEST_F(InspectorWebPerfAgentTest, MultipleScriptsInTask_MultipleContexts)
{
    m_agent->willProcessTask();
    EXPECT_EQ(0, numUniqueFrameContextsSeen());
    m_agent->willExecuteScript(executionContext());
    EXPECT_EQ(1, numUniqueFrameContextsSeen());
    m_agent->didExecuteScript();
    EXPECT_EQ("https://example.com/foo", frameContextURL());

    m_agent->willExecuteScript(anotherExecutionContext());
    EXPECT_EQ(2, numUniqueFrameContextsSeen());
    m_agent->didExecuteScript();
    m_agent->didProcessTask();
    m_agent->ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923); // Long task
    EXPECT_EQ(2, numUniqueFrameContextsSeen());
    EXPECT_EQ("", frameContextURL());
}

TEST_F(InspectorWebPerfAgentTest, NoScriptInLongTask)
{
    m_agent->willProcessTask();
    m_agent->willExecuteScript(executionContext());
    m_agent->didExecuteScript();
    m_agent->didProcessTask();
    m_agent->ReportTaskTime(nullptr, 3719349.445172, 3719349.445182);
    m_agent->willProcessTask();
    m_agent->didProcessTask();
    m_agent->ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923); // Long task
    // Without presence of Script, FrameContext URL is not available
    EXPECT_EQ(0, numUniqueFrameContextsSeen());
}

TEST_F(InspectorWebPerfAgentTest, SanitizedLongTaskName)
{
    HeapHashSet<Member<Location>> frameContextLocations;
    // Unable to attribute, when no execution contents are available.
    EXPECT_EQ("unknown",
        sanitizedLongTaskName(frameContextLocations, frame()));

    // Attribute for same context (and same origin).
    frameContextLocations.add(Location::create(frame()));
    EXPECT_EQ("https://example.com/foo",
        sanitizedLongTaskName(frameContextLocations, frame()));

    // Unable to attribute, when multiple script execution contents are involved.
    frameContextLocations.add(Location::create(frame()));
    EXPECT_EQ("multiple-contexts",
        sanitizedLongTaskName(frameContextLocations, frame()));
}

} // namespace blink
