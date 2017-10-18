// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_

#include <set>

#include "base/sequence_checker.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

namespace ukm {

/**
 * This is UkmRecorder which forwards its calls to some number of other
 * UkmRecorders. This primarily provides a way for TestUkmRecorders to
 * receive copies of recorded metrics.
 */
class METRICS_EXPORT DelegatingUkmRecorder : public UkmRecorder {
 public:
  DelegatingUkmRecorder();
  ~DelegatingUkmRecorder() override;

  // Lazy global instance getter.
  static DelegatingUkmRecorder* Get();

  // Adds a recorder this one should send its calls to.
  // The caller is responsible for removing the delegate before it is destroyed.
  void AddDelegate(UkmRecorder* delegate);

  // Removes a delegate added with AddDelegate.
  void RemoveDelegate(UkmRecorder* delegate);

 private:
  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void AddEntry(mojom::UkmEntryPtr entry) override;

  std::set<UkmRecorder*> delegates_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DelegatingUkmRecorder);
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_
