// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Note that the new histogram names will change once the semantics stabilize;
const char* const kFeaturesHistogramName = "Blink.UseCounter.Features";
const char* const kCSSHistogramName = "Blink.UseCounter.CSSProperties";
const char* const kSVGFeaturesHistogramName =
    "Blink.UseCounter.SVGImage.Features";
const char* const kSVGCSSHistogramName =
    "Blink.UseCounter.SVGImage.CSSProperties";
const char* const kLegacyFeaturesHistogramName = "WebCore.FeatureObserver";
const char* const kLegacyCSSHistogramName =
    "WebCore.FeatureObserver.CSSProperties";
}

namespace blink {

template <typename T>
void histogramBasicTest(const std::string& histogram,
                        const std::string& legacyHistogram,
                        const std::vector<std::string>& unaffectedHistograms,
                        T item,
                        T secondItem,
                        std::function<bool(T)> counted,
                        std::function<void(T)> count,
                        std::function<int(T)> histogramMap,
                        std::function<void(KURL)> didCommitLoad,
                        const std::string& url,
                        int pageVisitBucket) {
  HistogramTester histogramTester;

  // Test recording a single (arbitrary) counter
  EXPECT_FALSE(counted(item));
  count(item);
  EXPECT_TRUE(counted(item));
  histogramTester.expectUniqueSample(histogram, histogramMap(item), 1);
  histogramTester.expectTotalCount(legacyHistogram, 0);

  // Test that repeated measurements have no effect
  count(item);
  histogramTester.expectUniqueSample(histogram, histogramMap(item), 1);
  histogramTester.expectTotalCount(legacyHistogram, 0);

  // Test recording a different sample
  EXPECT_FALSE(counted(secondItem));
  count(secondItem);
  EXPECT_TRUE(counted(secondItem));
  histogramTester.expectBucketCount(histogram, histogramMap(item), 1);
  histogramTester.expectBucketCount(histogram, histogramMap(secondItem), 1);
  histogramTester.expectTotalCount(histogram, 2);
  histogramTester.expectTotalCount(legacyHistogram, 0);

  // After a page load, the histograms will be updated, even when the URL
  // scheme is internal
  didCommitLoad(URLTestHelpers::toKURL(url));
  histogramTester.expectBucketCount(histogram, histogramMap(item), 1);
  histogramTester.expectBucketCount(histogram, histogramMap(secondItem), 1);
  histogramTester.expectBucketCount(histogram, pageVisitBucket, 1);
  histogramTester.expectTotalCount(histogram, 3);

  // And verify the legacy histogram now looks the same
  histogramTester.expectBucketCount(legacyHistogram, histogramMap(item), 1);
  histogramTester.expectBucketCount(legacyHistogram, histogramMap(secondItem),
                                    1);
  histogramTester.expectBucketCount(legacyHistogram, pageVisitBucket, 1);
  histogramTester.expectTotalCount(legacyHistogram, 3);

  // Now a repeat measurement should get recorded again, exactly once
  EXPECT_FALSE(counted(item));
  count(item);
  count(item);
  EXPECT_TRUE(counted(item));
  histogramTester.expectBucketCount(histogram, histogramMap(item), 2);
  histogramTester.expectTotalCount(histogram, 4);

  // And on the next page load, the legacy histogram will again be updated
  didCommitLoad(URLTestHelpers::toKURL(url));
  histogramTester.expectBucketCount(legacyHistogram, histogramMap(item), 2);
  histogramTester.expectBucketCount(legacyHistogram, histogramMap(secondItem),
                                    1);
  histogramTester.expectBucketCount(legacyHistogram, pageVisitBucket, 2);
  histogramTester.expectTotalCount(legacyHistogram, 5);

  for (size_t i = 0; i < unaffectedHistograms.size(); ++i) {
    histogramTester.expectTotalCount(unaffectedHistograms[i], 0);
  }
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_RecordingFeatures DISABLED_RecordingFeatures
#else
#define MAYBE_RecordingFeatures RecordingFeatures
#endif
TEST(UseCounterTest, MAYBE_RecordingFeatures) {
  UseCounter useCounter;
  histogramBasicTest<UseCounter::Feature>(
      kFeaturesHistogramName, kLegacyFeaturesHistogramName,
      {kSVGFeaturesHistogramName, kSVGCSSHistogramName}, UseCounter::Fetch,
      UseCounter::FetchBodyStream,
      [&](UseCounter::Feature feature) -> bool {
        return useCounter.hasRecordedMeasurement(feature);
      },
      [&](UseCounter::Feature feature) {
        useCounter.recordMeasurement(feature);
      },
      [](UseCounter::Feature feature) -> int { return feature; },
      [&](KURL kurl) { useCounter.didCommitLoad(kurl); },
      "https://dummysite.com/", UseCounter::PageVisits);
}

TEST(UseCounterTest, RecordingCSSProperties) {
  UseCounter useCounter;
  histogramBasicTest<CSSPropertyID>(
      kCSSHistogramName, kLegacyCSSHistogramName,
      {kSVGFeaturesHistogramName, kSVGCSSHistogramName}, CSSPropertyFont,
      CSSPropertyZoom,
      [&](CSSPropertyID property) -> bool {
        return useCounter.isCounted(property);
      },
      [&](CSSPropertyID property) {
        useCounter.count(HTMLStandardMode, property);
      },
      [](CSSPropertyID property) -> int {
        return UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(property);
      },
      [&](KURL kurl) { useCounter.didCommitLoad(kurl); },
      "https://dummysite.com/", 1 /* page visit bucket */);
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_SVGImageContextFeatures DISABLED_SVGImageContextFeatures
#else
#define MAYBE_SVGImageContextFeatures SVGImageContextFeatures
#endif
TEST(UseCounterTest, MAYBE_SVGImageContextFeatures) {
  UseCounter useCounter(UseCounter::SVGImageContext);
  histogramBasicTest<UseCounter::Feature>(
      kSVGFeaturesHistogramName, kLegacyFeaturesHistogramName,
      {kFeaturesHistogramName, kCSSHistogramName},
      UseCounter::SVGSMILAdditiveAnimation,
      UseCounter::SVGSMILAnimationElementTiming,
      [&](UseCounter::Feature feature) -> bool {
        return useCounter.hasRecordedMeasurement(feature);
      },
      [&](UseCounter::Feature feature) {
        useCounter.recordMeasurement(feature);
      },
      [](UseCounter::Feature feature) -> int { return feature; },
      [&](KURL kurl) { useCounter.didCommitLoad(kurl); }, "about:blank",
      // In practice SVGs always appear to be loaded with an about:blank URL
      UseCounter::PageVisits);
}

TEST(UseCounterTest, SVGImageContextCSSProperties) {
  UseCounter useCounter(UseCounter::SVGImageContext);
  histogramBasicTest<CSSPropertyID>(
      kSVGCSSHistogramName, kLegacyCSSHistogramName,
      {kFeaturesHistogramName, kCSSHistogramName}, CSSPropertyFont,
      CSSPropertyZoom,
      [&](CSSPropertyID property) -> bool {
        return useCounter.isCounted(property);
      },
      [&](CSSPropertyID property) {
        useCounter.count(HTMLStandardMode, property);
      },
      [](CSSPropertyID property) -> int {
        return UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(property);
      },
      [&](KURL kurl) { useCounter.didCommitLoad(kurl); }, "about:blank",
      // In practice SVGs always appear to be loaded with an about:blank URL
      1 /* page visit bucket */);
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
        m_deprecation(m_dummy->page().deprecation()),
        m_useCounter(m_dummy->page().useCounter()) {}

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
