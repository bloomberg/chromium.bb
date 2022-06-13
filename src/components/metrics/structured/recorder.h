// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_RECORDER_H_

#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}

namespace metrics {
namespace structured {

// Recorder is a singleton to help communicate with the
// StructuredMetricsProvider. It serves three purposes:
// 1. Begin the initialization of the StructuredMetricsProvider (see class
// comment for more details).
// 2. Add an event for the StructuredMetricsProvider to record.
// 3. Retrieving information about project's key, specifically the day it was
// last rotated.
//
// The StructuredMetricsProvider is owned by the MetricsService, but it needs to
// be accessible to any part of the codebase, via an EventBase subclass, to
// record events. The StructuredMetricsProvider registers itself as an observer
// of this singleton when recording is enabled, and calls to Record (for
// recording) or ProfileAdded (for initialization) are then forwarded to it.
//
// Recorder is embedded within StructuredMetricsClient for Ash Chrome and should
// only be used in Ash Chrome.
class Recorder : public StructuredMetricsClient::RecordingDelegate {
 public:
  class RecorderImpl : public base::CheckedObserver {
   public:
    // Called on a call to Record.
    virtual void OnRecord(const EventBase& event) = 0;
    // Called on a call to ProfileAdded.
    virtual void OnProfileAdded(const base::FilePath& profile_path) = 0;
    // Called on a call to OnReportingStateChanged.
    virtual void OnReportingStateChanged(bool enabled) = 0;
    // Called on a call to LastKeyRotation.
    virtual absl::optional<int> LastKeyRotation(uint64_t project_name_hash) = 0;
  };

  static Recorder* GetInstance();

  // RecordingDelegate:
  void RecordEvent(Event&& event) override;
  void Record(EventBase&& event) override;
  bool IsReadyToRecord() const override;

  // Notifies the StructuredMetricsProvider that a profile has been added with
  // path |profile_path|. The first call to ProfileAdded initializes the
  // provider using the keys stored in |profile_path|, so care should be taken
  // to ensure the first call provides a |profile_path| suitable for metrics
  // collection.
  // TODO(crbug.com/1016655): When structured metrics expands beyond Chrome OS,
  // investigate whether initialization can be simplified for Chrome.
  void ProfileAdded(const base::FilePath& profile_path);

  // Returns when the key for |project_name_hash| was last rotated, in days
  // since epoch. Returns nullopt if the information is not available.
  absl::optional<int> LastKeyRotation(uint64_t project_name_hash);

  // Notifies observers that metrics reporting has been enabled or disabled.
  void OnReportingStateChanged(bool enabled);

  void SetUiTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  void AddObserver(RecorderImpl* observer);
  void RemoveObserver(RecorderImpl* observer);

 private:
  friend class base::NoDestructor<Recorder>;

  Recorder();
  ~Recorder() override;
  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::ObserverList<RecorderImpl> observers_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
