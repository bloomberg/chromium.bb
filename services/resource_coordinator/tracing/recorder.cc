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
      binding_(this, std::move(request)),
      weak_factory_(this) {
  binding_.set_connection_error_handler(base::BindRepeating(
      &Recorder::OnConnectionError, weak_factory_.GetWeakPtr()));
}

Recorder::~Recorder() = default;

void Recorder::AddChunk(const std::string& chunk) {
  if (chunk.empty())
    return;
  if (!background_task_runner_->RunsTasksOnCurrentThread()) {
    background_task_runner_->PostTask(
        FROM_HERE, base::BindRepeating(&Recorder::AddChunk,
                                       weak_factory_.GetWeakPtr(), chunk));
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
  is_recording_ = false;
  background_task_runner_->PostTask(FROM_HERE, on_data_change_callback_);
}

}  // namespace tracing
