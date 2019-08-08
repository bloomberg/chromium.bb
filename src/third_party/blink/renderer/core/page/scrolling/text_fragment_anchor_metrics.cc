// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"

namespace blink {

TextFragmentAnchorMetrics::TextFragmentAnchorMetrics(Document* document)
    : document_(document) {}

TextFragmentAnchorMetrics::~TextFragmentAnchorMetrics() {
#ifndef NDEBUG
  DCHECK(metrics_reported_);
#endif
}

void TextFragmentAnchorMetrics::DidCreateAnchor(int selector_count) {
  UseCounter::Count(document_, WebFeature::kTextFragmentAnchor);
  create_time_ = WTF::CurrentTimeTicks();
  selector_count_ = selector_count;
}

void TextFragmentAnchorMetrics::DidFindMatch() {
  match_count_++;
}

void TextFragmentAnchorMetrics::ResetMatchCount() {
  match_count_ = 0;
}

void TextFragmentAnchorMetrics::DidFindAmbiguousMatch() {
  ambiguous_match_ = true;
}

void TextFragmentAnchorMetrics::ScrollCancelled() {
  scroll_cancelled_ = true;
}

void TextFragmentAnchorMetrics::DidScroll() {
  if (first_scroll_into_view_time_.is_null())
    first_scroll_into_view_time_ = WTF::CurrentTimeTicks();
}

void TextFragmentAnchorMetrics::DidNonZeroScroll() {
  did_non_zero_scroll_ = true;
}

void TextFragmentAnchorMetrics::ReportMetrics() {
#ifndef NDEBUG
  DCHECK(!metrics_reported_);
#endif
  DCHECK(selector_count_);
  DCHECK(match_count_ <= selector_count_);

  if (match_count_ > 0) {
    UseCounter::Count(document_, WebFeature::kTextFragmentAnchorMatchFound);
  }

  UMA_HISTOGRAM_COUNTS_100("TextFragmentAnchor.SelectorCount", selector_count_);

  const int match_rate_percent =
      static_cast<int>(100 * ((match_count_ + 0.0) / selector_count_));
  UMA_HISTOGRAM_PERCENTAGE("TextFragmentAnchor.MatchRate", match_rate_percent);

  UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.AmbiguousMatch", ambiguous_match_);

  UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.ScrollCancelled",
                        scroll_cancelled_);

  if (first_scroll_into_view_time_ > create_time_) {
    UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.DidScrollIntoView",
                          did_non_zero_scroll_);

    UMA_HISTOGRAM_TIMES("TextFragmentAnchor.TimeToScrollIntoView",
                        first_scroll_into_view_time_ - create_time_);
  }

#ifndef NDEBUG
  metrics_reported_ = true;
#endif
}

void TextFragmentAnchorMetrics::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
}

}  // namespace blink
