// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"
#include "core/frame/FrameHost.h"
#include "core/frame/UseCounter.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class UseCounterTest : public ::testing::Test {
protected:
    bool hasRecordedMeasurement(const UseCounter& useCounter, UseCounter::Feature feature)
    {
        return useCounter.hasRecordedMeasurement(feature);
    }

    void recordMeasurement(UseCounter& useCounter, UseCounter::Feature feature)
    {
        useCounter.recordMeasurement(feature);
    }
};

TEST_F(UseCounterTest, RecordingMeasurements)
{
    UseCounter useCounter;
    for (unsigned feature = 0; feature < UseCounter::NumberOfFeatures; feature++) {
        if (feature != UseCounter::Feature::PageDestruction) {
            EXPECT_FALSE(hasRecordedMeasurement(useCounter, static_cast<UseCounter::Feature>(feature)));
            recordMeasurement(useCounter, static_cast<UseCounter::Feature>(feature));
            EXPECT_TRUE(hasRecordedMeasurement(useCounter, static_cast<UseCounter::Feature>(feature)));
        }
    }
}

TEST_F(UseCounterTest, MultipleMeasurements)
{
    UseCounter useCounter;
    for (unsigned feature = 0; feature < UseCounter::NumberOfFeatures; feature++) {
        if (feature != UseCounter::Feature::PageDestruction) {
            recordMeasurement(useCounter, static_cast<UseCounter::Feature>(feature));
            recordMeasurement(useCounter, static_cast<UseCounter::Feature>(feature));
            EXPECT_TRUE(hasRecordedMeasurement(useCounter, static_cast<UseCounter::Feature>(feature)));
        }
    }
}

TEST_F(UseCounterTest, InspectorDisablesMeasurement)
{
    UseCounter useCounter;

    // The specific feature we use here isn't important.
    UseCounter::Feature feature = UseCounter::Feature::SVGSMILElementInDocument;
    CSSPropertyID property = CSSPropertyFontWeight;
    CSSParserMode parserMode = HTMLStandardMode;

    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));

    useCounter.muteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));
    useCounter.count(parserMode, property);
    EXPECT_FALSE(useCounter.isCounted(property));

    useCounter.muteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));
    useCounter.count(parserMode, property);
    EXPECT_FALSE(useCounter.isCounted(property));

    useCounter.unmuteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_FALSE(hasRecordedMeasurement(useCounter, feature));
    useCounter.count(parserMode, property);
    EXPECT_FALSE(useCounter.isCounted(property));

    useCounter.unmuteForInspector();
    recordMeasurement(useCounter, feature);
    EXPECT_TRUE(hasRecordedMeasurement(useCounter, feature));
    useCounter.count(parserMode, property);
    EXPECT_TRUE(useCounter.isCounted(property));
}

class DeprecationTest : public ::testing::Test {
public:
    DeprecationTest()
        : m_dummy(DummyPageHolder::create())
        , m_deprecation(m_dummy->page().frameHost().deprecation())
        , m_useCounter(m_dummy->page().frameHost().useCounter())
    {
    }

protected:
    LocalFrame* frame()
    {
        return &m_dummy->frame();
    }

    std::unique_ptr<DummyPageHolder> m_dummy;
    Deprecation& m_deprecation;
    UseCounter& m_useCounter;
};

TEST_F(DeprecationTest, InspectorDisablesDeprecation)
{
    // The specific feature we use here isn't important.
    UseCounter::Feature feature = UseCounter::Feature::CSSDeepCombinator;
    CSSPropertyID property = CSSPropertyFontWeight;

    EXPECT_FALSE(m_deprecation.isSuppressed(property));

    m_deprecation.muteForInspector();
    Deprecation::warnOnDeprecatedProperties(frame(), property);
    EXPECT_FALSE(m_deprecation.isSuppressed(property));
    Deprecation::countDeprecation(frame(), feature);
    EXPECT_FALSE(m_useCounter.hasRecordedMeasurement(feature));

    m_deprecation.muteForInspector();
    Deprecation::warnOnDeprecatedProperties(frame(), property);
    EXPECT_FALSE(m_deprecation.isSuppressed(property));
    Deprecation::countDeprecation(frame(), feature);
    EXPECT_FALSE(m_useCounter.hasRecordedMeasurement(feature));

    m_deprecation.unmuteForInspector();
    Deprecation::warnOnDeprecatedProperties(frame(), property);
    EXPECT_FALSE(m_deprecation.isSuppressed(property));
    Deprecation::countDeprecation(frame(), feature);
    EXPECT_FALSE(m_useCounter.hasRecordedMeasurement(feature));

    m_deprecation.unmuteForInspector();
    Deprecation::warnOnDeprecatedProperties(frame(), property);
    // TODO: use the actually deprecated property to get a deprecation message.
    EXPECT_FALSE(m_deprecation.isSuppressed(property));
    Deprecation::countDeprecation(frame(), feature);
    EXPECT_TRUE(m_useCounter.hasRecordedMeasurement(feature));
}

} // namespace blink
