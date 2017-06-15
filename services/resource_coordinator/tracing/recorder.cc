// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/tracing/recorder.h"

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"

namespace tracing {

Recorder::Recorder(
    mojom::RecorderRequest request,
    bool data_is_array,
    const base::Closure& on_data_change_callback,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : is_recording_(true),
      data_is_array_(data_is_array),
      on_data_change_callback_(on_data_change_callback),
      background_task_runner_(background_task_runner),
      binding_(this, std::move(request)) {
  // A recorder should be deleted only if |is_recording_| is false to ensure
  // that:
  //
  // 1- |OnConnectionError| is already executed and so using Unretained(this) is
  // safe here.
  //
  // 2- The task possibly posted by |OnConnectionError| is already executed and
  // so using Unretained(this) is safe in that PostTask.
  //
  // 3- Since the connection is closed, the tasks posted by |AddChunk| are
  // already executed and so using Unretained(this) is safe in the PostTask in
  // |AddChunk|.
  //
  // We cannot use a weak pointer factory here since the weak pointers should be
  // dereferenced on the same SequencedTaskRunner. We could use two weak pointer
  // factories but that increases the complexity of the code since each factory
  // should be created on the correct thread and we should deal with cases that
  // one of the factories is not created, yet.
  //
  // The tracing coordinator deletes a recorder when |is_recording_| is false.
  binding_.set_connection_error_handler(base::BindRepeating(
      &Recorder::OnConnectionError, base::Unretained(this)));
}

Recorder::~Recorder() = default;

void Recorder::AddChunk(const std::string& chunk) {
  if (chunk.empty())
    return;
  if (!background_task_runner_->RunsTasksOnCurrentThread()) {
    background_task_runner_->PostTask(
        FROM_HERE, base::BindRepeating(&Recorder::AddChunk,
                                       base::Unretained(this), chunk));
    return;
  }
  if (data_is_array_ && !data_.empty())
    data_.append(",");
  data_.append(chunk);
  on_data_change_callback_.Run();
}

void Recorder::AddMetadata(std::unique_ptr<base::DictionaryValue> metadata) {
  metadata_.MergeDictionary(metadata.get());
}

void Recorder::OnConnectionError() {
  if (!background_task_runner_->RunsTasksOnCurrentThread()) {
    background_task_runner_->PostTask(
        FROM_HERE, base::BindRepeating(&Recorder::OnConnectionError,
                                       base::Unretained(this)));
    return;
  }
  is_recording_ = false;
  on_data_change_callback_.Run();
}

}  // namespace tracing
