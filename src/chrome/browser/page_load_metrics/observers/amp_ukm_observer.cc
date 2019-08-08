// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/amp_ukm_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"

AmpUkmObserver::AmpUkmObserver() {}

AmpUkmObserver::~AmpUkmObserver() {}

void AmpUkmObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flags,
    const page_load_metrics::PageLoadExtraInfo& info) {
  ukm::builders::AmpPageLoad builder(info.source_id);
  bool should_record = false;
  if (!logged_main_frame_ &&
      (info.main_frame_metadata.behavior_flags &
       blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorAmpDocumentLoaded) !=
          0) {
    builder.SetMainFrameAmpPageLoad(true);
    logged_main_frame_ = true;
    should_record = true;
  }

  if (!logged_subframe_ &&
      (info.subframe_metadata.behavior_flags &
       blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorAmpDocumentLoaded) !=
          0) {
    builder.SetSubFrameAmpPageLoad(true);
    logged_subframe_ = true;
    should_record = true;
  }
  if (should_record)
    builder.Record(ukm::UkmRecorder::Get());
}
