// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"

namespace ukm {

#if defined(OS_IOS)
const base::Feature kUkmFeature = {"Ukm", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kUkmFeature = {"Ukm", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

UkmRecorder::UkmRecorder() = default;

UkmRecorder::~UkmRecorder() = default;

// static
UkmRecorder* UkmRecorder::Get() {
  return DelegatingUkmRecorder::Get();
}

// static
ukm::SourceId UkmRecorder::GetNewSourceID() {
  return AssignNewSourceId();
}

std::unique_ptr<UkmEntryBuilder> UkmRecorder::GetEntryBuilder(
    ukm::SourceId source_id,
    const char* event_name) {
  return base::MakeUnique<UkmEntryBuilder>(
      base::Bind(&UkmRecorder::AddEntry, base::Unretained(this)), source_id,
      event_name);
}

}  // namespace ukm
