// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/delegating_ukm_recorder.h"

#include "base/lazy_instance.h"

namespace ukm {

namespace {

base::LazyInstance<DelegatingUkmRecorder>::Leaky g_ukm_recorder =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DelegatingUkmRecorder::DelegatingUkmRecorder() = default;
DelegatingUkmRecorder::~DelegatingUkmRecorder() = default;

// static
DelegatingUkmRecorder* DelegatingUkmRecorder::Get() {
  return &g_ukm_recorder.Get();
}

void DelegatingUkmRecorder::AddDelegate(UkmRecorder* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegates_.insert(delegate);
}

void DelegatingUkmRecorder::RemoveDelegate(UkmRecorder* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegates_.erase(delegate);
}

void DelegatingUkmRecorder::UpdateSourceURL(SourceId source_id,
                                            const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (UkmRecorder* delegate : delegates_) {
    delegate->UpdateSourceURL(source_id, url);
  }
}

void DelegatingUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there is exactly one delegate, just forward the call.
  if (delegates_.size() == 1) {
    (*delegates_.begin())->AddEntry(std::move(entry));
    return;
  }
  // Otherwise, make a copy for each delegate.
  for (UkmRecorder* delegate : delegates_)
    delegate->AddEntry(entry->Clone());
}

}  // namespace ukm
