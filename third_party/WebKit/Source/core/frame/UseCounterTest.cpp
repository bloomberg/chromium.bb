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
const char* const kFeaturesHistogramName = "Blink.UseCounter.Features";
const char* const kCSSHistogramName = "Blink.UseCounter.CSSProperties";
const char* const kAnimatedCSSHistogramName =
    "Blink.UseCounter.AnimatedCSSProperties";

const char* const kSVGFeaturesHistogramName =
    "Blink.UseCounter.SVGImage.Features";
const char* const kSVGCSSHistogramName =
    "Blink.UseCounter.SVGImage.CSSProperties";
const char* const kSVGAnimatedCSSHistogramName =
    "Blink.UseCounter.SVGImage.AnimatedCSSProperties";

const char* const kLegacyFeaturesHistogramName = "WebCore.FeatureObserver";
const char* const kLegacyCSSHistogramName =
    "WebCore.FeatureObserver.CSSProperties";
}

namespace blink {

template <typename T>
void HistogramBasicTest(const std::string& histogram,
                        const std::string& legacy_histogram,
                        const std::vector<std::string>& unaffected_histograms,
                        T item,
                        T second_item,
                        std::function<bool(T)> counted,
                        std::function<void(T)> count,
                        std::function<int(T)> histogram_map,
                        std::function<void(KURL)> did_commit_load,
                        const std::string& url,
                        int page_visit_bucket) {
  HistogramTester histogram_tester;

  // Test recording a single (arbitrary) counter
  EXPECT_FALSE(counted(item));
  count(item);
  EXPECT_TRUE(counted(item));
  histogram_tester.ExpectUniqueSample(histogram, histogram_map(item), 1);
  if (!legacy_histogram.empty()) {
    histogram_tester.ExpectTotalCount(legacy_histogram, 0);
  }

  // Test that repeated measurements have no effect
  count(item);
  histogram_tester.ExpectUniqueSample(histogram, histogram_map(item), 1);
  if (!legacy_histogram.empty()) {
    histogram_tester.ExpectTotalCount(legacy_histogram, 0);
  }

  // Test recording a different sample
  EXPECT_FALSE(counted(second_item));
  count(second_item);
  EXPECT_TRUE(counted(second_item));
  histogram_tester.ExpectBucketCount(histogram, histogram_map(item), 1);
  histogram_tester.ExpectBucketCount(histogram, histogram_map(second_item), 1);
  histogram_tester.ExpectTotalCount(histogram, 2);
  if (!legacy_histogram.empty()) {
    histogram_tester.ExpectTotalCount(legacy_histogram, 0);
  }

  // After a page load, the histograms will be updated, even when the URL
  // scheme is internal
  did_commit_load(URLTestHelpers::ToKURL(url));
  histogram_tester.ExpectBucketCount(histogram, histogram_map(item), 1);
  histogram_tester.ExpectBucketCount(histogram, histogram_map(second_item), 1);
  histogram_tester.ExpectBucketCount(histogram, page_visit_bucket, 1);
  histogram_tester.ExpectTotalCount(histogram, 3);

  // And verify the legacy histogram now looks the same
  if (!legacy_histogram.empty()) {
    histogram_tester.ExpectBucketCount(legacy_histogram, histogram_map(item),
                                       1);
    histogram_tester.ExpectBucketCount(legacy_histogram,
                                       histogram_map(second_item), 1);
    histogram_tester.ExpectBucketCount(legacy_histogram, page_visit_bucket, 1);
    histogram_tester.ExpectTotalCount(legacy_histogram, 3);
  }

  // Now a repeat measurement should get recorded again, exactly once
  EXPECT_FALSE(counted(item));
  count(item);
  count(item);
  EXPECT_TRUE(counted(item));
  histogram_tester.ExpectBucketCount(histogram, histogram_map(item), 2);
  histogram_tester.ExpectTotalCount(histogram, 4);

  // And on the next page load, the legacy histogram will again be updated
  did_commit_load(URLTestHelpers::ToKURL(url));
  if (!legacy_histogram.empty()) {
    histogram_tester.ExpectBucketCount(legacy_histogram, histogram_map(item),
                                       2);
    histogram_tester.ExpectBucketCount(legacy_histogram,
                                       histogram_map(second_item), 1);
    histogram_tester.ExpectBucketCount(legacy_histogram, page_visit_bucket, 2);
    histogram_tester.ExpectTotalCount(legacy_histogram, 5);
  }

  for (size_t i = 0; i < unaffected_histograms.size(); ++i) {
    histogram_tester.ExpectTotalCount(unaffected_histograms[i], 0);
  }
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_RecordingFeatures DISABLED_RecordingFeatures
#else
#define MAYBE_RecordingFeatures RecordingFeatures
#endif
TEST(UseCounterTest, MAYBE_RecordingFeatures) {
  UseCounter use_counter;
  HistogramBasicTest<UseCounter::Feature>(
      kFeaturesHistogramName, kLegacyFeaturesHistogramName,
      {kSVGFeaturesHistogramName, kSVGCSSHistogramName,
       kSVGAnimatedCSSHistogramName},
      UseCounter::kFetch, UseCounter::kFetchBodyStream,
      [&](UseCounter::Feature feature) -> bool {
        return use_counter.HasRecordedMeasurement(feature);
      },
      [&](UseCounter::Feature feature) {
        use_counter.RecordMeasurement(feature);
      },
      [](UseCounter::Feature feature) -> int { return feature; },
      [&](KURL kurl) { use_counter.DidCommitLoad(kurl); },
      "https://dummysite.com/", UseCounter::kPageVisits);
}

TEST(UseCounterTest, RecordingCSSProperties) {
  UseCounter use_counter;
  HistogramBasicTest<CSSPropertyID>(
      kCSSHistogramName, kLegacyCSSHistogramName,
      {kSVGFeaturesHistogramName, kSVGCSSHistogramName,
       kSVGAnimatedCSSHistogramName},
      CSSPropertyFont, CSSPropertyZoom,
      [&](CSSPropertyID property) -> bool {
        return use_counter.IsCounted(property);
      },
      [&](CSSPropertyID property) {
        use_counter.Count(kHTMLStandardMode, property);
      },
      [](CSSPropertyID property) -> int {
        return UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(property);
      },
      [&](KURL kurl) { use_counter.DidCommitLoad(kurl); },
      "https://dummysite.com/", 1 /* page visit bucket */);
}

TEST(UseCounterTest, RecordingAnimatedCSSProperties) {
  UseCounter use_counter;
  HistogramBasicTest<CSSPropertyID>(
      kAnimatedCSSHistogramName, "",
      {kSVGCSSHistogramName, kSVGAnimatedCSSHistogramName}, CSSPropertyOpacity,
      CSSPropertyVariable,
      [&](CSSPropertyID property) -> bool {
        return use_counter.IsCountedAnimatedCSS(property);
      },
      [&](CSSPropertyID property) { use_counter.CountAnimatedCSS(property); },
      [](CSSPropertyID property) -> int {
        return UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(property);
      },
      [&](KURL kurl) { use_counter.DidCommitLoad(kurl); },
      "https://dummysite.com/", 1 /* page visit bucket */);
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_SVGImageContextFeatures DISABLED_SVGImageContextFeatures
#else
#define MAYBE_SVGImageContextFeatures SVGImageContextFeatures
#endif
TEST(UseCounterTest, MAYBE_SVGImageContextFeatures) {
  UseCounter use_counter(UseCounter::kSVGImageContext);
  HistogramBasicTest<UseCounter::Feature>(
      kSVGFeaturesHistogramName, kLegacyFeaturesHistogramName,
      {kFeaturesHistogramName, kCSSHistogramName, kAnimatedCSSHistogramName},
      UseCounter::kSVGSMILAdditiveAnimation,
      UseCounter::kSVGSMILAnimationElementTiming,
      [&](UseCounter::Feature feature) -> bool {
        return use_counter.HasRecordedMeasurement(feature);
      },
      [&](UseCounter::Feature feature) {
        use_counter.RecordMeasurement(feature);
      },
      [](UseCounter::Feature feature) -> int { return feature; },
      [&](KURL kurl) { use_counter.DidCommitLoad(kurl); }, "about:blank",
      // In practice SVGs always appear to be loaded with an about:blank URL
      UseCounter::kPageVisits);
}

TEST(UseCounterTest, SVGImageContextCSSProperties) {
  UseCounter use_counter(UseCounter::kSVGImageContext);
  HistogramBasicTest<CSSPropertyID>(
      kSVGCSSHistogramName, kLegacyCSSHistogramName,
      {kFeaturesHistogramName, kCSSHistogramName, kAnimatedCSSHistogramName},
      CSSPropertyFont, CSSPropertyZoom,
      [&](CSSPropertyID property) -> bool {
        return use_counter.IsCounted(property);
      },
      [&](CSSPropertyID property) {
        use_counter.Count(kHTMLStandardMode, property);
      },
      [](CSSPropertyID property) -> int {
        return UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(property);
      },
      [&](KURL kurl) { use_counter.DidCommitLoad(kurl); }, "about:blank",
      // In practice SVGs always appear to be loaded with an about:blank URL
      1 /* page visit bucket */);
}

TEST(UseCounterTest, SVGImageContextAnimatedCSSProperties) {
  UseCounter use_counter(UseCounter::kSVGImageContext);
  HistogramBasicTest<CSSPropertyID>(
      kSVGAnimatedCSSHistogramName, "",
      {kCSSHistogramName, kAnimatedCSSHistogramName}, CSSPropertyOpacity,
      CSSPropertyVariable,
      [&](CSSPropertyID property) -> bool {
        return use_counter.IsCountedAnimatedCSS(property);
      },
      [&](CSSPropertyID property) { use_counter.CountAnimatedCSS(property); },
      [](CSSPropertyID property) -> int {
        return UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(property);
      },
      [&](KURL kurl) { use_counter.DidCommitLoad(kurl); }, "about:blank",
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
  UseCounter use_counter;
  HistogramTester histogram_tester;

  // The specific feature we use here isn't important.
  UseCounter::Feature feature = UseCounter::Feature::kSVGSMILElementInDocument;
  CSSPropertyID property = CSSPropertyFontWeight;
  CSSParserMode parser_mode = kHTMLStandardMode;

  EXPECT_FALSE(use_counter.HasRecordedMeasurement(feature));

  use_counter.MuteForInspector();
  use_counter.RecordMeasurement(feature);
  EXPECT_FALSE(use_counter.HasRecordedMeasurement(feature));
  use_counter.Count(parser_mode, property);
  EXPECT_FALSE(use_counter.IsCounted(property));
  histogram_tester.ExpectTotalCount(kFeaturesHistogramName, 0);
  histogram_tester.ExpectTotalCount(kCSSHistogramName, 0);

  use_counter.MuteForInspector();
  use_counter.RecordMeasurement(feature);
  EXPECT_FALSE(use_counter.HasRecordedMeasurement(feature));
  use_counter.Count(parser_mode, property);
  EXPECT_FALSE(use_counter.IsCounted(property));
  histogram_tester.ExpectTotalCount(kFeaturesHistogramName, 0);
  histogram_tester.ExpectTotalCount(kCSSHistogramName, 0);

  use_counter.UnmuteForInspector();
  use_counter.RecordMeasurement(feature);
  EXPECT_FALSE(use_counter.HasRecordedMeasurement(feature));
  use_counter.Count(parser_mode, property);
  EXPECT_FALSE(use_counter.IsCounted(property));
  histogram_tester.ExpectTotalCount(kFeaturesHistogramName, 0);
  histogram_tester.ExpectTotalCount(kCSSHistogramName, 0);

  use_counter.UnmuteForInspector();
  use_counter.RecordMeasurement(feature);
  EXPECT_TRUE(use_counter.HasRecordedMeasurement(feature));
  use_counter.Count(parser_mode, property);
  EXPECT_TRUE(use_counter.IsCounted(property));
  histogram_tester.ExpectUniqueSample(kFeaturesHistogramName, feature, 1);
  histogram_tester.ExpectUniqueSample(
      kCSSHistogramName,
      UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(property), 1);
}

void ExpectHistograms(const HistogramTester& histogram_tester,
                      int visits_count,
                      UseCounter::Feature feature,
                      int feature_count,
                      CSSPropertyID property,
                      int property_count) {
  histogram_tester.ExpectBucketCount(kFeaturesHistogramName,
                                     UseCounter::kPageVisits, visits_count);
  histogram_tester.ExpectBucketCount(kFeaturesHistogramName, feature,
                                     feature_count);
  histogram_tester.ExpectTotalCount(kFeaturesHistogramName,
                                    visits_count + feature_count);
  histogram_tester.ExpectBucketCount(kCSSHistogramName, 1, visits_count);
  histogram_tester.ExpectBucketCount(
      kCSSHistogramName,
      UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(property),
      property_count);
  histogram_tester.ExpectTotalCount(kCSSHistogramName,
                                    visits_count + property_count);
}

// Failing on Android: crbug.com/667913
#if OS(ANDROID)
#define MAYBE_MutedDocuments DISABLED_MutedDocuments
#else
#define MAYBE_MutedDocuments MutedDocuments
#endif
TEST(UseCounterTest, MAYBE_MutedDocuments) {
  UseCounter use_counter;
  HistogramTester histogram_tester;

  // Counters triggered before any load are always reported.
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 0, UseCounter::kFetch, 1,
                   CSSPropertyFontWeight, 1);

  // Loading an internal page doesn't bump PageVisits and metrics not reported.
  use_counter.DidCommitLoad(URLTestHelpers::ToKURL("about:blank"));
  EXPECT_FALSE(use_counter.HasRecordedMeasurement(UseCounter::kFetch));
  EXPECT_FALSE(use_counter.IsCounted(CSSPropertyFontWeight));
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 0, UseCounter::kFetch, 1,
                   CSSPropertyFontWeight, 1);

  // But the fact that the features were seen is still known.
  EXPECT_TRUE(use_counter.HasRecordedMeasurement(UseCounter::kFetch));
  EXPECT_TRUE(use_counter.IsCounted(CSSPropertyFontWeight));

  // Inspector muting then unmuting doesn't change the behavior.
  use_counter.MuteForInspector();
  use_counter.UnmuteForInspector();
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 0, UseCounter::kFetch, 1,
                   CSSPropertyFontWeight, 1);

  // If we now load a real web page, metrics are reported again.
  use_counter.DidCommitLoad(URLTestHelpers::ToKURL("http://foo.com/"));
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 1, UseCounter::kFetch, 2,
                   CSSPropertyFontWeight, 2);

  // HTTPs URLs are the same.
  use_counter.DidCommitLoad(
      URLTestHelpers::ToKURL("https://baz.com:1234/blob.html"));
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 2, UseCounter::kFetch, 3,
                   CSSPropertyFontWeight, 3);

  // Extensions aren't counted.
  use_counter.DidCommitLoad(
      URLTestHelpers::ToKURL("chrome-extension://1238ba908adf/"));
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 2, UseCounter::kFetch, 3,
                   CSSPropertyFontWeight, 3);

  // Nor is devtools
  use_counter.DidCommitLoad(
      URLTestHelpers::ToKURL("chrome-devtools://1238ba908adf/"));
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 2, UseCounter::kFetch, 3,
                   CSSPropertyFontWeight, 3);

  // Nor are data URLs
  use_counter.DidCommitLoad(
      URLTestHelpers::ToKURL("data:text/plain,thisisaurl"));
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 2, UseCounter::kFetch, 3,
                   CSSPropertyFontWeight, 3);

  // Or empty URLs (a main frame with no Document)
  use_counter.DidCommitLoad(KURL());
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 2, UseCounter::kFetch, 3,
                   CSSPropertyFontWeight, 3);

  // But file URLs are
  use_counter.DidCommitLoad(URLTestHelpers::ToKURL("file:///c/autoexec.bat"));
  use_counter.RecordMeasurement(UseCounter::kFetch);
  use_counter.Count(kHTMLStandardMode, CSSPropertyFontWeight);
  ExpectHistograms(histogram_tester, 3, UseCounter::kFetch, 4,
                   CSSPropertyFontWeight, 4);
}

class DeprecationTest : public ::testing::Test {
 public:
  DeprecationTest()
      : dummy_(DummyPageHolder::Create()),
        deprecation_(dummy_->GetPage().GetDeprecation()),
        use_counter_(dummy_->GetPage().GetUseCounter()) {}

 protected:
  LocalFrame* GetFrame() { return &dummy_->GetFrame(); }

  std::unique_ptr<DummyPageHolder> dummy_;
  Deprecation& deprecation_;
  UseCounter& use_counter_;
};

TEST_F(DeprecationTest, InspectorDisablesDeprecation) {
  // The specific feature we use here isn't important.
  UseCounter::Feature feature = UseCounter::Feature::kCSSDeepCombinator;
  CSSPropertyID property = CSSPropertyFontWeight;

  EXPECT_FALSE(deprecation_.IsSuppressed(property));

  deprecation_.MuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.MuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  // TODO: use the actually deprecated property to get a deprecation message.
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_TRUE(use_counter_.HasRecordedMeasurement(feature));
}

}  // namespace blink
