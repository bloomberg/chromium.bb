// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_

#include <set>

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
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
  void AddDelegate(base::WeakPtr<UkmRecorder> delegate);

  // Removes a delegate added with AddDelegate.
  // The pointer is only used as a key.
  void RemoveDelegate(UkmRecorder* delegate);

 private:
  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void AddEntry(mojom::UkmEntryPtr entry) override;

  class Delegate final {
   public:
    Delegate(scoped_refptr<base::SequencedTaskRunner> task_runner,
             base::WeakPtr<UkmRecorder> ptr);
    Delegate(const Delegate& other);
    ~Delegate();

    void UpdateSourceURL(SourceId source_id, const GURL& url);
    void AddEntry(mojom::UkmEntryPtr entry);

   private:
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    base::WeakPtr<UkmRecorder> ptr_;
  };

  // Synchronizes access to |delegates_|.
  // Not using ObserverListThreadSafe since we need to make copies of call
  // arguments.
  mutable base::Lock lock_;

  std::unordered_map<UkmRecorder*, Delegate> delegates_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingUkmRecorder);
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_
