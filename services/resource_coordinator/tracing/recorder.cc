// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/tracing/recorder.h"

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"

namespace tracing {

Recorder::Recorder(mojom::RecorderRequest request,
                   mojom::TraceDataType data_type,
                   const base::RepeatingClosure& on_data_change_callback)
    : is_recording_(true),
      data_type_(data_type),
      on_data_change_callback_(on_data_change_callback),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  binding_.set_connection_error_handler(base::BindOnce(
      &Recorder::OnConnectionError, weak_ptr_factory_.GetWeakPtr()));
}

Recorder::~Recorder() = default;

void Recorder::AddChunk(const std::string& chunk) {
  if (chunk.empty())
    return;
  if (data_type_ != mojom::TraceDataType::STRING && !data_.empty())
    data_.append(",");
  data_.append(chunk);
  on_data_change_callback_.Run();
}

void Recorder::AddMetadata(std::unique_ptr<base::DictionaryValue> metadata) {
  metadata_.MergeDictionary(metadata.get());
}

void Recorder::OnConnectionError() {
  is_recording_ = false;
  on_data_change_callback_.Run();
}

}  // namespace tracing
