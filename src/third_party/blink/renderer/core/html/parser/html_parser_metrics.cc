// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_parser_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

HTMLParserMetrics::HTMLParserMetrics(int64_t source_id,
                                     ukm::UkmRecorder* recorder)
    : source_id_(source_id), recorder_(recorder) {}

void HTMLParserMetrics::AddChunk(base::TimeDelta elapsed_time,
                                 unsigned tokens_parsed) {
  DCHECK(base::TimeTicks::IsHighResolution());

  ++chunk_count_;

  accumulated_parsing_time_ += elapsed_time;
  if (elapsed_time < min_parsing_time_)
    min_parsing_time_ = elapsed_time;
  if (elapsed_time > max_parsing_time_)
    max_parsing_time_ = elapsed_time;

  total_tokens_parsed_ += tokens_parsed;
  if (tokens_parsed < min_tokens_parsed_)
    min_tokens_parsed_ = tokens_parsed;
  if (tokens_parsed > max_tokens_parsed_)
    max_tokens_parsed_ = tokens_parsed;
}

void HTMLParserMetrics::AddYieldInterval(base::TimeDelta elapsed_time) {
  DCHECK(base::TimeTicks::IsHighResolution());

  yield_count_++;

  accumulated_yield_intervals_ += elapsed_time;
  if (elapsed_time < min_yield_interval_)
    min_yield_interval_ = elapsed_time;
  if (elapsed_time > max_yield_interval_)
    max_yield_interval_ = elapsed_time;
}

void HTMLParserMetrics::ReportMetricsAtParseEnd() {
  // The various histogram limits were chosen based on initial UKM data.
  UMA_HISTOGRAM_COUNTS_1000("Blink.HTMLParsing.ChunkCount", chunk_count_);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Blink.HTMLParsing.ParsingTimeMax", max_parsing_time_,
      base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(100),
      1000);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Blink.HTMLParsing.ParsingTimeMin",
                                          min_parsing_time_,
                                          base::TimeDelta::FromMicroseconds(1),
                                          base::TimeDelta::FromSeconds(1), 100);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Blink.HTMLParsing.ParsingTimeTotal", accumulated_parsing_time_,
      base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(100),
      1000);
  UMA_HISTOGRAM_COUNTS_1M("Blink.HTMLParsing.TokensParsedMax",
                          max_tokens_parsed_);
  UMA_HISTOGRAM_COUNTS_10000("Blink.HTMLParsing.TokensParsedMin",
                             min_tokens_parsed_);
  UMA_HISTOGRAM_COUNTS_1M("Blink.HTMLParsing.TokensParsedAverage",
                          total_tokens_parsed_ / chunk_count_);

  // Only report yield data if we actually yielded.
  if (max_yield_interval_ != base::TimeDelta()) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLParsing.YieldedTimeMax", max_yield_interval_,
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(100),
        1000);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLParsing.YieldedTimeMin", min_yield_interval_,
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
        100);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLParsing.YieldedTimeAverage",
        accumulated_yield_intervals_ / yield_count_,
        base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
        100);
  }

  // Build and report UKM
  ukm::builders::Blink_HTMLParsing builder(source_id_);
  builder.SetChunkCount(chunk_count_);
  builder.SetParsingTimeMax(max_parsing_time_.InMicroseconds());
  builder.SetParsingTimeMin(min_parsing_time_.InMicroseconds());
  builder.SetParsingTimeTotal(accumulated_parsing_time_.InMicroseconds());
  builder.SetTokensParsedMax(max_tokens_parsed_);
  builder.SetTokensParsedMin(min_tokens_parsed_);
  builder.SetTokensParsedAverage(total_tokens_parsed_ / chunk_count_);
  if (accumulated_yield_intervals_ != base::TimeDelta()) {
    builder.SetYieldedTimeMax(max_yield_interval_.InMicroseconds());
    builder.SetYieldedTimeMin(min_yield_interval_.InMicroseconds());
    builder.SetYieldedTimeAverage(
        accumulated_yield_intervals_.InMicroseconds() / yield_count_);
  }
  builder.Record(recorder_);
}

}  // namespace blink
