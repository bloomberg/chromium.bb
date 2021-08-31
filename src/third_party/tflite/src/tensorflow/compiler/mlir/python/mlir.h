/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

// Functions for getting information about kernels registered in the binary.
// Migrated from previous SWIG file (mlir.i) authored by aminim@.
#ifndef TENSORFLOW_COMPILER_MLIR_PYTHON_MLIR_H_
#define TENSORFLOW_COMPILER_MLIR_PYTHON_MLIR_H_

#include <string>

#include "tensorflow/c/tf_status.h"

namespace tensorflow {

// Simple wrapper to support tf.mlir.experimental.convert_graph_def.
// Load a .pbptx, convert to MLIR, and (optionally) optimize the module before
// returning it as a string.
// This is an early experimental API, ideally we should return a wrapper object
// around a Python binding to the MLIR module.
std::string ImportGraphDef(const std::string &proto,
                           const std::string &pass_pipeline, TF_Status *status);

// Load a SavedModel and return a textual MLIR string corresponding to it.
//
// Args:
//   saved_model_path: File path from which to load the SavedModel.
//   exported_names_str: Comma-separated list of names to export.
//                       Empty means "export all".
//
// Returns:
//   A string of textual MLIR representing the raw imported SavedModel.
std::string ExperimentalConvertSavedModelToMlir(
    const std::string &saved_model_path, const std::string &exported_names_str,
    bool show_debug_info, TF_Status *status);

// Load a SavedModel V1 and return a textual MLIR string corresponding to it.
//
// Args:
//   saved_model_path: File path from which to load the SavedModel.
//   tags: Tags to identify MetaGraphDef that need to be loaded.
//
// Returns:
//   A string of textual MLIR representing the raw imported SavedModel.
std::string ExperimentalConvertSavedModelV1ToMlir(
    const std::string &saved_model_path, const std::string &tags,
    bool show_debug_info, TF_Status *status);

std::string ExperimentalRunPassPipeline(const std::string &mlir_txt,
                                        const std::string &pass_pipeline,
                                        bool show_debug_info,
                                        TF_Status *status);

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_PYTHON_MLIR_H_
