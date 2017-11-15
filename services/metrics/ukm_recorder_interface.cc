// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/ukm_recorder_interface.h"

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace metrics {

UkmRecorderInterface::UkmRecorderInterface(ukm::UkmRecorder* ukm_recorder,
                                           int64_t instance_id)
    : ukm_recorder_(ukm_recorder), instance_id_(instance_id) {}

UkmRecorderInterface::~UkmRecorderInterface() = default;

// static
void UkmRecorderInterface::Create(
    ukm::UkmRecorder* ukm_recorder,
    ukm::mojom::UkmRecorderInterfaceRequest request) {
  static base::AtomicSequenceNumber seq;
  mojo::MakeStrongBinding(
      std::make_unique<UkmRecorderInterface>(ukm_recorder, seq.GetNext() + 1),
      std::move(request));
}

void UkmRecorderInterface::AddEntry(ukm::mojom::UkmEntryPtr ukm_entry) {
  ukm_entry->source_id =
      ukm::ConvertSourceIdFromInstance(instance_id_, ukm_entry->source_id);
  ukm_recorder_->AddEntry(std::move(ukm_entry));
}

void UkmRecorderInterface::UpdateSourceURL(int64_t source_id,
                                           const std::string& url) {
  ukm_recorder_->UpdateSourceURL(
      ukm::ConvertSourceIdFromInstance(instance_id_, source_id), GURL(url));
}

}  // namespace metrics
