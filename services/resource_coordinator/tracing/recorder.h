// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_TRACING_RECORDER_H_
#define SERVICES_RESOURCE_COORDINATOR_TRACING_RECORDER_H_

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"

namespace tracing {

class Recorder : public mojom::Recorder {
 public:
  // The tracing service creates instances of the |Recorder| class and send them
  // to agents. The agents then use the recorder for sending trace data to the
  // tracing service.
  //
  // |data_is_array| tells the recorder whether the data is of type array or
  // string. Chunks of type array are concatenated using a comma as the
  // separator; chuunks of type string are concatenated without a separator.
  //
  // |on_data_change_callback| is run whenever the recorder receives data from
  // the agent or when the connection is lost to notify the tracing service of
  // the data change.
  //
  // |background_task_runner| is used so that if the tracing service is run on
  // the UI thread we do not block it. So buffering trace events and copying
  // them to data pipes are done on a background thread.
  Recorder(
      mojom::RecorderRequest request,
      bool data_is_array,
      const base::Closure& on_data_change_callback,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  ~Recorder() override;

  const std::string& data() const {
    // All access to |data_| should be done on the background thread.
    DCHECK(background_task_runner_->RunsTasksOnCurrentThread());
    return data_;
  }

  void clear_data() {
    // All access to |data_| should be done on the background thread.
    DCHECK(background_task_runner_->RunsTasksOnCurrentThread());
    data_.clear();
  }

  const base::DictionaryValue& metadata() const { return metadata_; }
  bool is_recording() const { return is_recording_; }
  bool data_is_array() const { return data_is_array_; }

 private:
  friend class RecorderTest;  // For testing.
  // mojom::Recorder
  // These are called by agents for sending trace data to the tracing service.
  void AddChunk(const std::string& chunk) override;
  void AddMetadata(std::unique_ptr<base::DictionaryValue> metadata) override;

  void OnConnectionError();

  std::string data_;
  base::DictionaryValue metadata_;
  bool is_recording_;
  bool data_is_array_;
  base::Closure on_data_change_callback_;
  // To avoid blocking the UI thread if the tracing service is run on the UI
  // thread, we make sure that buffering trace events and copying them to the
  // final stream is done on a background thread.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  mojo::Binding<mojom::Recorder> binding_;

  DISALLOW_COPY_AND_ASSIGN(Recorder);
};

}  // namespace tracing
#endif  // SERVICES_RESOURCE_COORDINATOR_TRACING_RECORDER_H_
