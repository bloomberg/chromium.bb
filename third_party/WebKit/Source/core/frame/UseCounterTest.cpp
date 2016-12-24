// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"
#include "core/frame/FrameHost.h"
#include "core/frame/UseCounter.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Note that the new histogram names will change once the semantics stabilize;
const char* const kFeaturesHistogramName = "WebCore.UseCounter_TEST.Features";
const char* const kCSSHistogramName = "WebCore.UseCounter_TEST.CSSProperties";
const char* const kSVGFeaturesHistogramName =
    "WebCore.UseCounter_TEST.SVGImage.Features";
const char* const kSVGCSSHistogramName =
    "WebCore.UseCounter_TEST.SVGImage.CSSProperties";
const char* const kLegacyFeaturesHistogramName = "WebCore.FeatureObserver";
const char* const kLegacyCSSHistogramName =
    "WebCore.FeatureObserver.CSSProperties";
}

namespace blink {

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_RecordingFeatures DISABLED_RecordingFeatures
#else
#define MAYBE_RecordingFeatures RecordingFeatures
#endif
TEST(UseCounterTest, MAYBE_RecordingFeatures) {
  UseCounter useCounter;
  HistogramTester histogramTester;

  // Test recording a single (arbitrary) counter
  EXPECT_FALSE(useCounter.hasRecordedMeasurement(UseCounter::Fetch));
  useCounter.recordMeasurement(UseCounter::Fetch);
  EXPECT_TRUE(useCounter.hasRecordedMeasurement(UseCounter::Fetch));
  histogramTester.expectUniqueSample(kFeaturesHistogramName, UseCounter::Fetch,
                                     1);
  histogramTester.expectTotalCount(kLegacyFeaturesHistogramName, 0);

  // Test that repeated measurements have no effect
  useCounter.recordMeasurement(UseCounter::Fetch);
  histogramTester.expectUniqueSample(kFeaturesHistogramName, UseCounter::Fetch,
                                     1);
  histogramTester.expectTotalCount(kLegacyFeaturesHistogramName, 0);

  // Test recording a different sample
  EXPECT_FALSE(useCounter.hasRecordedMeasurement(UseCounter::FetchBodyStream));
  useCounter.recordMeasurement(UseCounter::FetchBodyStream);
  EXPECT_TRUE(useCounter.hasRecordedMeasurement(UseCounter::FetchBodyStream));
  histogramTester.expectBucketCount(kFeaturesHistogramName, UseCounter::Fetch,
                                    1);
  histogramTester.expectBucketCount(kFeaturesHistogramName,
                                    UseCounter::FetchBodyStream, 1);
  histogramTester.expectTotalCount(kFeaturesHistogramName, 2);
  histogramTester.expectTotalCount(kLegacyFeaturesHistogramName, 0);

  // Test the impact of page load on the new histogram
  useCounter.didCommitLoad(URLTestHelpers::toKURL("https://dummysite.com/"));
  histogramTester.expectBucketCount(kFeaturesHistogramName, UseCounter::Fetch,
                                    1);
  histogramTester.expectBucketCount(kFeaturesHistogramName,
                                    UseCounter::FetchBodyStream, 1);
  histogramTester.expectBucketCount(kFeaturesHistogramName,
                                    UseCounter::PageVisits, 1);
  histogramTester.expectTotalCount(kFeaturesHistogramName, 3);

  // And verify the legacy histogram now looks the same
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::Fetch, 1);
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::FetchBodyStream, 1);
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::PageVisits, 1);
  histogramTester.expectTotalCount(kLegacyFeaturesHistogramName, 3);

  // Now a repeat measurement should get recorded again, exactly once
  EXPECT_FALSE(useCounter.hasRecordedMeasurement(UseCounter::Fetch));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.recordMeasurement(UseCounter::Fetch);
  EXPECT_TRUE(useCounter.hasRecordedMeasurement(UseCounter::Fetch));
  histogramTester.expectBucketCount(kFeaturesHistogramName, UseCounter::Fetch,
                                    2);
  histogramTester.expectTotalCount(kFeaturesHistogramName, 4);

  // And on the next page load, the legacy histogram will again be updated
  useCounter.didCommitLoad(URLTestHelpers::toKURL("https://dummysite.com/"));
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::Fetch, 2);
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::FetchBodyStream, 1);
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::PageVisits, 2);
  histogramTester.expectTotalCount(kLegacyFeaturesHistogramName, 5);

  // None of this should update any of the SVG histograms
  histogramTester.expectTotalCount(kSVGFeaturesHistogramName, 0);
  histogramTester.expectTotalCount(kSVGCSSHistogramName, 0);
}

TEST(UseCounterTest, RecordingCSSProperties) {
  UseCounter useCounter;
  HistogramTester histogramTester;

  // Test recording a single (arbitrary) property
  EXPECT_FALSE(useCounter.isCounted(CSSPropertyFont));
  useCounter.count(HTMLStandardMode, CSSPropertyFont);
  EXPECT_TRUE(useCounter.isCounted(CSSPropertyFont));
  histogramTester.expectUniqueSample(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      1);
  histogramTester.expectTotalCount(kLegacyCSSHistogramName, 0);

  // Test that repeated measurements have no effect
  useCounter.count(HTMLStandardMode, CSSPropertyFont);
  histogramTester.expectUniqueSample(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      1);
  histogramTester.expectTotalCount(kLegacyCSSHistogramName, 0);

  // Test recording a different sample
  EXPECT_FALSE(useCounter.isCounted(CSSPropertyZoom));
  useCounter.count(HTMLStandardMode, CSSPropertyZoom);
  EXPECT_TRUE(useCounter.isCounted(CSSPropertyZoom));
  histogramTester.expectBucketCount(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      1);
  histogramTester.expectBucketCount(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyZoom),
      1);
  histogramTester.expectTotalCount(kCSSHistogramName, 2);
  histogramTester.expectTotalCount(kLegacyCSSHistogramName, 0);

  // Test the impact of page load on the new histogram
  useCounter.didCommitLoad(URLTestHelpers::toKURL("https://dummysite.com/"));
  histogramTester.expectBucketCount(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      1);
  histogramTester.expectBucketCount(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyZoom),
      1);
  histogramTester.expectBucketCount(kCSSHistogramName, 1, 1);
  histogramTester.expectTotalCount(kCSSHistogramName, 3);

  // And verify the legacy histogram now looks the same
  histogramTester.expectBucketCount(
      kLegacyCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      1);
  histogramTester.expectBucketCount(
      kLegacyCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyZoom),
      1);
  histogramTester.expectBucketCount(kLegacyCSSHistogramName, 1, 1);
  histogramTester.expectTotalCount(kLegacyCSSHistogramName, 3);

  // Now a repeat measurement should get recorded again, exactly once
  EXPECT_FALSE(useCounter.isCounted(CSSPropertyFont));
  useCounter.count(HTMLStandardMode, CSSPropertyFont);
  useCounter.count(HTMLStandardMode, CSSPropertyFont);
  EXPECT_TRUE(useCounter.isCounted(CSSPropertyFont));
  histogramTester.expectBucketCount(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      2);
  histogramTester.expectTotalCount(kCSSHistogramName, 4);

  // And on the next page load, the legacy histogram will again be updated
  useCounter.didCommitLoad(URLTestHelpers::toKURL("https://dummysite.com/"));
  histogramTester.expectBucketCount(
      kLegacyCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      2);
  histogramTester.expectBucketCount(
      kLegacyCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyZoom),
      1);
  histogramTester.expectBucketCount(kLegacyCSSHistogramName, 1, 2);
  histogramTester.expectTotalCount(kLegacyCSSHistogramName, 5);

  // None of this should update any of the SVG histograms
  histogramTester.expectTotalCount(kSVGFeaturesHistogramName, 0);
  histogramTester.expectTotalCount(kSVGCSSHistogramName, 0);
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_SVGImageContext DISABLED_SVGImageContext
#else
#define MAYBE_SVGImageContext SVGImageContext
#endif

TEST(UseCounterTest, MAYBE_SVGImageContext) {
  UseCounter useCounter(UseCounter::SVGImageContext);
  HistogramTester histogramTester;

  // Verify that SVGImage related feature counters get recorded in a separate
  // histogram.
  EXPECT_FALSE(
      useCounter.hasRecordedMeasurement(UseCounter::SVGSMILAdditiveAnimation));
  useCounter.recordMeasurement(UseCounter::SVGSMILAdditiveAnimation);
  EXPECT_TRUE(
      useCounter.hasRecordedMeasurement(UseCounter::SVGSMILAdditiveAnimation));
  histogramTester.expectUniqueSample(kSVGFeaturesHistogramName,
                                     UseCounter::SVGSMILAdditiveAnimation, 1);

  // And for the CSS counters
  EXPECT_FALSE(useCounter.isCounted(CSSPropertyFont));
  useCounter.count(HTMLStandardMode, CSSPropertyFont);
  EXPECT_TRUE(useCounter.isCounted(CSSPropertyFont));
  histogramTester.expectUniqueSample(
      kSVGCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      1);

  // After a page load, the histograms will be updated, even when the URL
  // scheme is internal (in practice SVGs always appear to get loaded with
  // an about:blank URL).
  useCounter.didCommitLoad(URLTestHelpers::toKURL("about:blank"));
  histogramTester.expectBucketCount(kSVGFeaturesHistogramName,
                                    UseCounter::PageVisits, 1);
  histogramTester.expectTotalCount(kSVGFeaturesHistogramName, 2);
  histogramTester.expectBucketCount(kSVGCSSHistogramName, 1, 1);
  histogramTester.expectTotalCount(kSVGCSSHistogramName, 2);

  // And the legacy histogram will be updated to include these
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::SVGSMILAdditiveAnimation, 1);
  histogramTester.expectBucketCount(kLegacyFeaturesHistogramName,
                                    UseCounter::PageVisits, 1);
  histogramTester.expectTotalCount(kLegacyFeaturesHistogramName, 2);
  histogramTester.expectBucketCount(
      kLegacyCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyFont),
      1);
  histogramTester.expectBucketCount(kLegacyCSSHistogramName, 1, 1);
  histogramTester.expectTotalCount(kLegacyCSSHistogramName, 2);

  // None of this should update the non-legacy non-SVG histograms
  histogramTester.expectTotalCount(kCSSHistogramName, 0);
  histogramTester.expectTotalCount(kFeaturesHistogramName, 0);
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_InspectorDisablesMeasurement DISABLED_InspectorDisablesMeasurement
#else
#define MAYBE_InspectorDisablesMeasurement InspectorDisablesMeasurement
#endif

TEST(UseCounterTest, MAYBE_InspectorDisablesMeasurement) {
  UseCounter useCounter;
  HistogramTester histogramTester;

  // The specific feature we use here isn't important.
  UseCounter::Feature feature = UseCounter::Feature::SVGSMILElementInDocument;
  CSSPropertyID property = CSSPropertyFontWeight;
  CSSParserMode parserMode = HTMLStandardMode;

  EXPECT_FALSE(useCounter.hasRecordedMeasurement(feature));

  useCounter.muteForInspector();
  useCounter.recordMeasurement(feature);
  EXPECT_FALSE(useCounter.hasRecordedMeasurement(feature));
  useCounter.count(parserMode, property);
  EXPECT_FALSE(useCounter.isCounted(property));
  histogramTester.expectTotalCount(kFeaturesHistogramName, 0);
  histogramTester.expectTotalCount(kCSSHistogramName, 0);

  useCounter.muteForInspector();
  useCounter.recordMeasurement(feature);
  EXPECT_FALSE(useCounter.hasRecordedMeasurement(feature));
  useCounter.count(parserMode, property);
  EXPECT_FALSE(useCounter.isCounted(property));
  histogramTester.expectTotalCount(kFeaturesHistogramName, 0);
  histogramTester.expectTotalCount(kCSSHistogramName, 0);

  useCounter.unmuteForInspector();
  useCounter.recordMeasurement(feature);
  EXPECT_FALSE(useCounter.hasRecordedMeasurement(feature));
  useCounter.count(parserMode, property);
  EXPECT_FALSE(useCounter.isCounted(property));
  histogramTester.expectTotalCount(kFeaturesHistogramName, 0);
  histogramTester.expectTotalCount(kCSSHistogramName, 0);

  useCounter.unmuteForInspector();
  useCounter.recordMeasurement(feature);
  EXPECT_TRUE(useCounter.hasRecordedMeasurement(feature));
  useCounter.count(parserMode, property);
  EXPECT_TRUE(useCounter.isCounted(property));
  histogramTester.expectUniqueSample(kFeaturesHistogramName, feature, 1);
  histogramTester.expectUniqueSample(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(property), 1);
}

void expectHistograms(const HistogramTester& histogramTester,
                      int visitsCount,
                      UseCounter::Feature feature,
                      int featureCount,
                      CSSPropertyID property,
                      int propertyCount) {
  histogramTester.expectBucketCount(kFeaturesHistogramName,
                                    UseCounter::PageVisits, visitsCount);
  histogramTester.expectBucketCount(kFeaturesHistogramName, feature,
                                    featureCount);
  histogramTester.expectTotalCount(kFeaturesHistogramName,
                                   visitsCount + featureCount);
  histogramTester.expectBucketCount(kCSSHistogramName, 1, visitsCount);
  histogramTester.expectBucketCount(
      kCSSHistogramName,
      UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(property),
      propertyCount);
  histogramTester.expectTotalCount(kCSSHistogramName,
                                   visitsCount + propertyCount);
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_MutedDocuments DISABLED_MutedDocuments
#else
#define MAYBE_MutedDocuments MutedDocuments
#endif
TEST(UseCounterTest, MAYBE_MutedDocuments) {
  UseCounter useCounter;
  HistogramTester histogramTester;

  // Counters triggered before any load are always reported.
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 0, UseCounter::Fetch, 1,
                   CSSPropertyFontWeight, 1);

  // Loading an internal page doesn't bump PageVisits and metrics not reported.
  useCounter.didCommitLoad(URLTestHelpers::toKURL("about:blank"));
  EXPECT_FALSE(useCounter.hasRecordedMeasurement(UseCounter::Fetch));
  EXPECT_FALSE(useCounter.isCounted(CSSPropertyFontWeight));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 0, UseCounter::Fetch, 1,
                   CSSPropertyFontWeight, 1);

  // But the fact that the features were seen is still known.
  EXPECT_TRUE(useCounter.hasRecordedMeasurement(UseCounter::Fetch));
  EXPECT_TRUE(useCounter.isCounted(CSSPropertyFontWeight));

  // Inspector muting then unmuting doesn't change the behavior.
  useCounter.muteForInspector();
  useCounter.unmuteForInspector();
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 0, UseCounter::Fetch, 1,
                   CSSPropertyFontWeight, 1);

  // If we now load a real web page, metrics are reported again.
  useCounter.didCommitLoad(URLTestHelpers::toKURL("http://foo.com/"));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 1, UseCounter::Fetch, 2,
                   CSSPropertyFontWeight, 2);

  // HTTPs URLs are the same.
  useCounter.didCommitLoad(
      URLTestHelpers::toKURL("https://baz.com:1234/blob.html"));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 2, UseCounter::Fetch, 3,
                   CSSPropertyFontWeight, 3);

  // Extensions aren't counted.
  useCounter.didCommitLoad(
      URLTestHelpers::toKURL("chrome-extension://1238ba908adf/"));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 2, UseCounter::Fetch, 3,
                   CSSPropertyFontWeight, 3);

  // Nor is devtools
  useCounter.didCommitLoad(
      URLTestHelpers::toKURL("chrome-devtools://1238ba908adf/"));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 2, UseCounter::Fetch, 3,
                   CSSPropertyFontWeight, 3);

  // Nor are data URLs
  useCounter.didCommitLoad(
      URLTestHelpers::toKURL("data:text/plain,thisisaurl"));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 2, UseCounter::Fetch, 3,
                   CSSPropertyFontWeight, 3);

  // Or empty URLs (a main frame with no Document)
  useCounter.didCommitLoad(KURL());
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 2, UseCounter::Fetch, 3,
                   CSSPropertyFontWeight, 3);

  // But file URLs are
  useCounter.didCommitLoad(URLTestHelpers::toKURL("file:///c/autoexec.bat"));
  useCounter.recordMeasurement(UseCounter::Fetch);
  useCounter.count(HTMLStandardMode, CSSPropertyFontWeight);
  expectHistograms(histogramTester, 3, UseCounter::Fetch, 4,
                   CSSPropertyFontWeight, 4);
}

class DeprecationTest : public ::testing::Test {
 public:
  DeprecationTest()
      : m_dummy(DummyPageHolder::create()),
        m_deprecation(m_dummy->page().frameHost().deprecation()),
        m_useCounter(m_dummy->page().frameHost().useCounter()) {}

 protected:
  LocalFrame* frame() { return &m_dummy->frame(); }

  std::unique_ptr<DummyPageHolder> m_dummy;
  Deprecation& m_deprecation;
  UseCounter& m_useCounter;
};

TEST_F(DeprecationTest, InspectorDisablesDeprecation) {
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

}  // namespace blink
