// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_parser_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class HTMLParserMetricsTest : public testing::Test {
 public:
  HTMLParserMetricsTest() {
    helper_.Initialize(nullptr, nullptr, nullptr, nullptr);
  }

  ~HTMLParserMetricsTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(helper_.GetWebView()->MainFrameImpl(),
                                       html,
                                       url_test_helpers::ToKURL("about:blank"));
  }

 protected:
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(HTMLParserMetricsTest, ReportSingleChunk) {
  // Although the tests use a mock clock, the metrics recorder checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  base::HistogramTester histogram_tester;
  LoadHTML(R"HTML(
    <div></div>
  )HTML");

  // Should have one of each metric, except the yield times because with
  // a single chunk they should not report.
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ChunkCount", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMax", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMin", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeTotal", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMax", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMin", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedAverage", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMax", 0);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMin", 0);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeAverage", 0);

  // Expect specific values for the chunks and tokens counts
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.ChunkCount", 1, 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMax", 2,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMin", 2,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedAverage",
                                      2, 1);

  // Expect that the times have moved from the default and the max and min
  // and total are all the same (within the same bucket)
  std::vector<base::Bucket> parsing_time_max_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMax");
  std::vector<base::Bucket> parsing_time_min_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMin");
  std::vector<base::Bucket> parsing_time_total_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeTotal");
  EXPECT_EQ(parsing_time_max_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_min_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_total_buckets.size(), 1u);
  EXPECT_GT(parsing_time_max_buckets[0].min, 0);
  EXPECT_GT(parsing_time_min_buckets[0].min, 0);
  EXPECT_GT(parsing_time_total_buckets[0].min, 0);
}

TEST_F(HTMLParserMetricsTest, HistogramReportsTwoChunks) {
  // Although the tests use a mock clock, the metrics recorder checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  base::HistogramTester histogram_tester;

  // This content exceeds the number of tokens before a script tag used as
  // the yield threshold. If the yield threshold changes, this test will fail
  // and/or need changing. See the HTMLParserScheduler::ShouldYield method for
  // the current value of the constant.
  LoadHTML(R"HTML(
    <head></head>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <script>document.offsetTop</script>
  )HTML");

  // Should have one of each metric.
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ChunkCount", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMax", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMin", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeTotal", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMax", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMin", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedAverage", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMax", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMin", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeAverage", 1);

  // Expect specific values for the chunks and tokens counts
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.ChunkCount", 2, 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMax", 110,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMin", 0,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedAverage",
                                      55, 1);

  // For parse times, expect that the times have moved from the default.
  std::vector<base::Bucket> parsing_time_max_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMax");
  std::vector<base::Bucket> parsing_time_min_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMin");
  std::vector<base::Bucket> parsing_time_total_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeTotal");
  EXPECT_EQ(parsing_time_max_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_min_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_total_buckets.size(), 1u);
  EXPECT_GT(parsing_time_max_buckets[0].min, 0);
  EXPECT_GT(parsing_time_min_buckets[0].min, 0);
  EXPECT_GT(parsing_time_total_buckets[0].min, 0);

  // For yields, the values should be the same because there was only one yield,
  // but due to different histogram sizes we can't directly compare them.
  std::vector<base::Bucket> yield_time_max_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.YieldedTimeMax");
  std::vector<base::Bucket> yield_time_min_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.YieldedTimeMin");
  std::vector<base::Bucket> yield_time_average_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.YieldedTimeAverage");
  EXPECT_EQ(yield_time_max_buckets.size(), 1u);
  EXPECT_EQ(yield_time_min_buckets.size(), 1u);
  EXPECT_EQ(yield_time_average_buckets.size(), 1u);
  EXPECT_GT(yield_time_max_buckets[0].min, 0);
  EXPECT_GT(yield_time_min_buckets[0].min, 0);
  EXPECT_GT(yield_time_average_buckets[0].min, 0);
}

TEST_F(HTMLParserMetricsTest, UkmStoresValuesCorrectly) {
  // Although the tests use a mock clock, the metrics recorder checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  ukm::TestUkmRecorder recorder;
  HTMLParserMetrics reporter(ukm::UkmRecorder::GetNewSourceID(), &recorder);

  // Start with empty metrics
  auto entries = recorder.GetEntriesByName("Blink.HTMLParsing");
  EXPECT_EQ(entries.size(), 0u);

  // Run a fictional sequence of calls
  base::TimeDelta first_parse_time = base::TimeDelta::FromMicroseconds(20);
  base::TimeDelta second_parse_time = base::TimeDelta::FromMicroseconds(10);
  base::TimeDelta third_parse_time = base::TimeDelta::FromMicroseconds(30);
  unsigned first_tokens_parsed = 50u;
  unsigned second_tokens_parsed = 40u;
  unsigned third_tokens_parsed = 60u;
  base::TimeDelta first_yield_time = base::TimeDelta::FromMicroseconds(80);
  base::TimeDelta second_yield_time = base::TimeDelta::FromMicroseconds(70);

  reporter.AddChunk(first_parse_time, first_tokens_parsed);
  reporter.AddYieldInterval(first_yield_time);
  reporter.AddChunk(second_parse_time, second_tokens_parsed);
  reporter.AddYieldInterval(second_yield_time);
  reporter.AddChunk(third_parse_time, third_tokens_parsed);
  reporter.ReportMetricsAtParseEnd();

  // Check we have a single entry
  entries = recorder.GetEntriesByName("Blink.HTMLParsing");
  EXPECT_EQ(entries.size(), 1u);
  auto* entry = entries[0];

  // Verify all the values
  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ChunkCount"));
  const int64_t* metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "ChunkCount");
  EXPECT_EQ(*metric_value, 3);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ParsingTimeMax"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "ParsingTimeMax");
  EXPECT_EQ(*metric_value, third_parse_time.InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ParsingTimeMin"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "ParsingTimeMin");
  EXPECT_EQ(*metric_value, second_parse_time.InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ParsingTimeTotal"));
  metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "ParsingTimeTotal");
  EXPECT_EQ(*metric_value,
            (first_parse_time + second_parse_time + third_parse_time)
                .InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "TokensParsedMax"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "TokensParsedMax");
  EXPECT_EQ(*metric_value, third_tokens_parsed);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "TokensParsedMin"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "TokensParsedMin");
  EXPECT_EQ(*metric_value, second_tokens_parsed);

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, "TokensParsedAverage"));
  metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "TokensParsedAverage");
  EXPECT_EQ(
      *metric_value,
      (first_tokens_parsed + second_tokens_parsed + third_tokens_parsed) / 3);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "YieldedTimeMax"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "YieldedTimeMax");
  EXPECT_EQ(*metric_value, first_yield_time.InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "YieldedTimeMin"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "YieldedTimeMin");
  EXPECT_EQ(*metric_value, second_yield_time.InMicroseconds());

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, "YieldedTimeAverage"));
  metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "YieldedTimeAverage");
  EXPECT_EQ(*metric_value,
            ((first_yield_time + second_yield_time) / 2).InMicroseconds());
}

}  // namespace blink
