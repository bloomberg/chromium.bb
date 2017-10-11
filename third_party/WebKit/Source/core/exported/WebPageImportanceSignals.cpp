// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebPageImportanceSignals.h"

#include "platform/Histogram.h"
#include "public/web/WebViewClient.h"

namespace blink {

void WebPageImportanceSignals::Reset() {
  had_form_interaction_ = false;
  if (observer_)
    observer_->PageImportanceSignalsChanged();
}

void WebPageImportanceSignals::SetHadFormInteraction() {
  had_form_interaction_ = true;
  if (observer_)
    observer_->PageImportanceSignalsChanged();
}

void WebPageImportanceSignals::OnCommitLoad() {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, had_form_interaction_histogram,
      ("PageImportanceSignals.HadFormInteraction.OnCommitLoad", 2));
  had_form_interaction_histogram.Count(had_form_interaction_);

  Reset();
}

}  // namespace blink
