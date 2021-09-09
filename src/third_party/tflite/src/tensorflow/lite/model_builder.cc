/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/lite/model_builder.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/core/api/error_reporter.h"
#include "tensorflow/lite/core/api/flatbuffer_conversions.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/util.h"
#include "tensorflow/lite/version.h"

#if defined(TFLITE_ENABLE_DEFAULT_PROFILER)
#include "tensorflow/lite/profiling/platform_profiler.h"
#endif

namespace tflite {

namespace {

// Ensure that ErrorReporter is non-null.
ErrorReporter* ValidateErrorReporter(ErrorReporter* e) {
  return e ? e : DefaultErrorReporter();
}

}  // namespace

#ifndef TFLITE_MCU
// Loads a model from `filename`. If `mmap_file` is true then use mmap,
// otherwise make a copy of the model in a buffer.
std::unique_ptr<Allocation> GetAllocationFromFile(const char* filename,
                                                  bool mmap_file,
                                                  ErrorReporter* error_reporter,
                                                  bool use_nnapi) {
  std::unique_ptr<Allocation> allocation;
  if (mmap_file && MMAPAllocation::IsSupported()) {
    allocation.reset(new MMAPAllocation(filename, error_reporter));
  } else {
    allocation.reset(new FileCopyAllocation(filename, error_reporter));
  }
  return allocation;
}

std::unique_ptr<FlatBufferModel> FlatBufferModel::BuildFromFile(
    const char* filename, ErrorReporter* error_reporter) {
  error_reporter = ValidateErrorReporter(error_reporter);

  std::unique_ptr<FlatBufferModel> model;
  auto allocation = GetAllocationFromFile(filename, /*mmap_file=*/true,
                                          error_reporter, /*use_nnapi=*/true);
  model.reset(new FlatBufferModel(std::move(allocation), error_reporter));
  if (!model->initialized()) model.reset();
  return model;
}

std::unique_ptr<FlatBufferModel> FlatBufferModel::VerifyAndBuildFromFile(
    const char* filename, TfLiteVerifier* extra_verifier,
    ErrorReporter* error_reporter) {
  error_reporter = ValidateErrorReporter(error_reporter);

  std::unique_ptr<FlatBufferModel> model;
  auto allocation = GetAllocationFromFile(filename, /*mmap_file=*/true,
                                          error_reporter, /*use_nnapi=*/true);

  flatbuffers::Verifier base_verifier(
      reinterpret_cast<const uint8_t*>(allocation->base()),
      allocation->bytes());
  if (!VerifyModelBuffer(base_verifier)) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "The model is not a valid Flatbuffer file");
    return nullptr;
  }

  if (extra_verifier &&
      !extra_verifier->Verify(static_cast<const char*>(allocation->base()),
                              allocation->bytes(), error_reporter)) {
    return model;
  }
  model.reset(new FlatBufferModel(std::move(allocation), error_reporter));
  if (!model->initialized()) model.reset();
  return model;
}
#endif

std::unique_ptr<FlatBufferModel> FlatBufferModel::BuildFromBuffer(
    const char* caller_owned_buffer, size_t buffer_size,
    ErrorReporter* error_reporter) {
  error_reporter = ValidateErrorReporter(error_reporter);

  std::unique_ptr<FlatBufferModel> model;
  std::unique_ptr<Allocation> allocation(
      new MemoryAllocation(caller_owned_buffer, buffer_size, error_reporter));
  model.reset(new FlatBufferModel(std::move(allocation), error_reporter));
  if (!model->initialized()) model.reset();
  return model;
}

std::unique_ptr<FlatBufferModel> FlatBufferModel::VerifyAndBuildFromBuffer(
    const char* caller_owned_buffer, size_t buffer_size,
    TfLiteVerifier* extra_verifier, ErrorReporter* error_reporter) {
  error_reporter = ValidateErrorReporter(error_reporter);

  flatbuffers::Verifier base_verifier(
      reinterpret_cast<const uint8_t*>(caller_owned_buffer), buffer_size);
  if (!VerifyModelBuffer(base_verifier)) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "The model is not a valid Flatbuffer buffer");
    return nullptr;
  }

  if (extra_verifier && !extra_verifier->Verify(caller_owned_buffer,
                                                buffer_size, error_reporter)) {
    return nullptr;
  }

  return BuildFromBuffer(caller_owned_buffer, buffer_size, error_reporter);
}

std::unique_ptr<FlatBufferModel> FlatBufferModel::BuildFromModel(
    const tflite::Model* caller_owned_model_spec,
    ErrorReporter* error_reporter) {
  error_reporter = ValidateErrorReporter(error_reporter);

  std::unique_ptr<FlatBufferModel> model;
  model.reset(new FlatBufferModel(caller_owned_model_spec, error_reporter));
  if (!model->initialized()) model.reset();
  return model;
}

string FlatBufferModel::GetMinimumRuntime() const {
  if (!model_ || !model_->metadata()) return "";

  for (int i = 0; i < model_->metadata()->size(); ++i) {
    auto metadata = model_->metadata()->Get(i);
    if (metadata->name()->str() == "min_runtime_version") {
      auto buf = metadata->buffer();
      auto* buffer = (*model_->buffers())[buf];
      auto* array = buffer->data();
      // Get the real length of the runtime string, since there might be
      // trailing
      // '\0's in the buffer.
      for (int len = 0; len < array->size(); ++len) {
        if (array->data()[len] == '\0') {
          return string(reinterpret_cast<const char*>(array->data()), len);
        }
      }
      // If there is no '\0' in the buffer, this indicates that the flatbuffer
      // is malformed.
      TF_LITE_REPORT_ERROR(
          error_reporter_,
          "Min_runtime_version in model metadata is malformed");
      break;
    }
  }
  return "";
}

bool FlatBufferModel::CheckModelIdentifier() const {
  if (!tflite::ModelBufferHasIdentifier(allocation_->base())) {
    const char* ident = flatbuffers::GetBufferIdentifier(allocation_->base());
    error_reporter_->Report(
        "Model provided has model identifier '%c%c%c%c', should be '%s'\n",
        ident[0], ident[1], ident[2], ident[3], tflite::ModelIdentifier());
    return false;
  }
  return true;
}

FlatBufferModel::FlatBufferModel(const Model* model,
                                 ErrorReporter* error_reporter)
    : model_(model), error_reporter_(ValidateErrorReporter(error_reporter)) {}

FlatBufferModel::FlatBufferModel(std::unique_ptr<Allocation> allocation,
                                 ErrorReporter* error_reporter)
    : error_reporter_(ValidateErrorReporter(error_reporter)),
      allocation_(std::move(allocation)) {
  if (!allocation_->valid() || !CheckModelIdentifier()) return;

  model_ = ::tflite::GetModel(allocation_->base());
}

FlatBufferModel::~FlatBufferModel() {}

}  // namespace tflite
