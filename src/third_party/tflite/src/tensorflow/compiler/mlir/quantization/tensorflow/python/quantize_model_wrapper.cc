/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/compiler/mlir/quantization/tensorflow/python/quantize_model_wrapper.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "llvm/Support/Debug.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "tensorflow/cc/saved_model/loader.h"
#include "tensorflow/compiler/mlir/quantization/tensorflow/calibrator/calibrator_singleton.h"
#include "tensorflow/compiler/mlir/quantization/tensorflow/python/quantize_model.h"
#include "tensorflow/core/common_runtime/graph_constructor.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/platform/statusor.h"
#include "tensorflow/core/platform/stringpiece.h"
#include "tensorflow/lite/python/interpreter_wrapper/python_utils.h"

using tensorflow::FunctionDefLibrary;
using tensorflow::Graph;
using tensorflow::GraphDef;
using tensorflow::ImportGraphDefOptions;
using tensorflow::OpRegistry;

namespace tensorflow {
namespace quantization {

PyObject* QuantizeQATModel(absl::string_view saved_model_path,
                           absl::string_view exported_names_str,
                           absl::string_view tags) {
  absl::StatusOr<tensorflow::GraphDef> graph_def =
      internal::QuantizeQATModel(saved_model_path, exported_names_str, tags);
  if (!graph_def.ok()) {
    PyErr_Format(PyExc_ValueError, "failed to quantize QAT model: %s",
                 std::string(graph_def.status().message()).c_str());
    return nullptr;
  }

  std::string ret_str = graph_def.value().SerializeAsString();

  return tflite::python_utils::ConvertToPyString(ret_str.c_str(),
                                                 ret_str.size());
}

PyObject* QuantizePTQDynamicRange(absl::string_view saved_model_path,
                                  absl::string_view exported_names_str,
                                  absl::string_view tags) {
  absl::StatusOr<tensorflow::GraphDef> graph_def =
      internal::QuantizePTQDynamicRange(saved_model_path, exported_names_str,
                                        tags);
  if (!graph_def.ok()) {
    PyErr_Format(PyExc_ValueError,
                 "failed to apply post-training dynamic range quantization to "
                 "the model: %s",
                 std::string(graph_def.status().message()).c_str());
    return nullptr;
  }

  std::string ret_str = graph_def.value().SerializeAsString();

  return tflite::python_utils::ConvertToPyString(ret_str.c_str(),
                                                 ret_str.size());
}

PyObject* QuantizePTQModelPreCalibration(absl::string_view saved_model_path,
                                         absl::string_view exported_names_str,
                                         absl::string_view tags) {
  absl::StatusOr<tensorflow::GraphDef> graph_def =
      internal::QuantizePTQModelPreCalibration(saved_model_path,
                                               exported_names_str, tags);
  if (!graph_def.ok()) {
    PyErr_Format(PyExc_ValueError,
                 "failed to quantize PTQ model at the precalibration stage: %s",
                 std::string(graph_def.status().message()).c_str());
    return nullptr;
  }

  std::string ret_str = graph_def.value().SerializeAsString();

  return tflite::python_utils::ConvertToPyString(ret_str.c_str(),
                                                 ret_str.size());
}

PyObject* QuantizePTQModelPostCalibration(absl::string_view saved_model_path,
                                          absl::string_view exported_names_str,
                                          absl::string_view tags) {
  absl::StatusOr<tensorflow::GraphDef> graph_def =
      internal::QuantizePTQModelPostCalibration(saved_model_path,
                                                exported_names_str, tags);
  if (!graph_def.ok()) {
    PyErr_Format(
        PyExc_ValueError,
        "failed to quantize PTQ model at the postcalibration stage: %s",
        std::string(graph_def.status().message()).c_str());
    return nullptr;
  }

  std::string ret_str = graph_def.value().SerializeAsString();

  return tflite::python_utils::ConvertToPyString(ret_str.c_str(),
                                                 ret_str.size());
}

void ClearCollectedInformationFromCalibrator() {
  calibrator::CalibratorSingleton::ClearCollectedInformation();
}

void ClearDataFromCalibrator(absl::string_view id) {
  calibrator::CalibratorSingleton::ClearData(id);
}

float GetMinFromCalibrator(absl::string_view id) {
  absl::optional<std::pair<float, float>> min_max =
      calibrator::CalibratorSingleton::GetMinMax(id);
  if (!min_max.has_value()) {
    PyErr_Format(PyExc_ValueError, "No calibrated data for '%s'",
                 std::string{id}.c_str());
    throw py::error_already_set();
  }

  return min_max->first;
}

float GetMaxFromCalibrator(absl::string_view id) {
  absl::optional<std::pair<float, float>> min_max =
      calibrator::CalibratorSingleton::GetMinMax(id);
  if (!min_max.has_value()) {
    PyErr_Format(PyExc_ValueError, "No calibrated data for '%s'",
                 std::string{id}.c_str());
    throw py::error_already_set();
  }

  return min_max->second;
}

}  // namespace quantization
}  // namespace tensorflow
