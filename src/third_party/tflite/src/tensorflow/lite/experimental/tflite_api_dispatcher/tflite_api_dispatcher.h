/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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
// The purpose of this file is to indirect how implementations of the TensorFlow
// Lite API are selected by providing a single namespace tflite_api_dispatcher.

#ifndef TENSORFLOW_LITE_EXPERIMENTAL_TFLITE_API_DISPATCHER_TFLITE_API_DISPATCHER_H_
#define TENSORFLOW_LITE_EXPERIMENTAL_TFLITE_API_DISPATCHER_TFLITE_API_DISPATCHER_H_

#ifndef TFLITE_EXPERIMENTAL_RUNTIME_EAGER
#define TFLITE_EXPERIMENTAL_RUNTIME_EAGER (0)
#endif

#ifndef TFLITE_EXPERIMENTAL_RUNTIME_NON_EAGER
#define TFLITE_EXPERIMENTAL_RUNTIME_NON_EAGER (0)
#endif

#if TFLITE_EXPERIMENTAL_RUNTIME_EAGER && TFLITE_EXPERIMENTAL_RUNTIME_NON_EAGER
#error \
    "TFLITE_EXPERIMENTAL_RUNTIME_EAGER and " \
    "TFLITE_EXPERIMENTAL_RUNTIME_NON_EAGER should not both be true."
#endif

// Import the relevant interpreter and model files.
#if TFLITE_EXPERIMENTAL_RUNTIME_EAGER
#include "tensorflow/lite/experimental/tf_runtime/lib/eager_model.h"
#include "tensorflow/lite/experimental/tf_runtime/public/eager_interpreter.h"
#elif TFLITE_EXPERIMENTAL_RUNTIME_NON_EAGER
#include "tensorflow/lite/experimental/tf_runtime/lib/model.h"
#include "tensorflow/lite/experimental/tf_runtime/public/interpreter.h"
#else
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"
#endif

namespace tflite_api_dispatcher {

// Use the correct interpreter.
#if TFLITE_EXPERIMENTAL_RUNTIME_EAGER
using Interpreter = tflrt::EagerInterpreter;
using InterpreterBuilder = tflrt::EagerTfLiteInterpreterBuilderAPI;
using TfLiteModel = tflite::FlatBufferModel;
using TfLiteVerifier = tflite::TfLiteVerifier;
#elif TFLITE_EXPERIMENTAL_RUNTIME_NON_EAGER
using Interpreter = tflrt::TfLiteInterpreterAPI;
using InterpreterBuilder = tflrt::TfLiteInterpreterBuilderAPI;
using TfLiteModel = tflrt::BEFModel;
using TfLiteVerifier = tflrt::TfLiteVerifier;
#else
using tflite::Interpreter;
using tflite::InterpreterBuilder;
using TfLiteModel = tflite::FlatBufferModel;
using TfLiteVerifier = tflite::TfLiteVerifier;
#endif

}  // namespace tflite_api_dispatcher

#endif  // TENSORFLOW_LITE_EXPERIMENTAL_TFLITE_API_DISPATCHER_TFLITE_API_DISPATCHER_H_
