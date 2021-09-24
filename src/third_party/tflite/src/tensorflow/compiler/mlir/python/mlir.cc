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

#include "tensorflow/compiler/mlir/python/mlir.h"

#include <string>
#include <type_traits>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/InitAllPasses.h"  // from @llvm-project
#include "mlir/Parser.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "mlir/Pass/PassRegistry.h"  // from @llvm-project
#include "tensorflow/c/eager/c_api.h"
#include "tensorflow/c/eager/tfe_context_internal.h"
#include "tensorflow/c/tf_status.h"
#include "tensorflow/c/tf_status_helper.h"
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/tf_saved_model_passes.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/import_model.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/mlir_roundtrip_flags.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/tf_mlir_translate.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/error_util.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/import_utils.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/core/common_runtime/eager/context.h"
#include "tensorflow/core/common_runtime/function_body.h"
#include "tensorflow/core/common_runtime/function_def_utils.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/framework/function.pb.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {

namespace {

// Runs pass pipeline `pass_pipeline` on `module` if `pass_pipeline` is not
// empty.
std::string RunPassPipelineOnModule(mlir::ModuleOp module,
                                    const std::string &pass_pipeline,
                                    bool show_debug_info, TF_Status *status) {
  if (!pass_pipeline.empty()) {
    mlir::PassManager pm(module.getContext());
    std::string error;
    llvm::raw_string_ostream error_stream(error);
    if (failed(mlir::parsePassPipeline(pass_pipeline, pm, error_stream))) {
      TF_SetStatus(status, TF_INVALID_ARGUMENT,
                   ("Invalid pass_pipeline: " + error_stream.str()).c_str());
      return "// error";
    }

    mlir::StatusScopedDiagnosticHandler statusHandler(module.getContext());
    if (failed(pm.run(module))) {
      Set_TF_Status_from_Status(status, statusHandler.ConsumeStatus());
      return "// error";
    }
  }
  return MlirModuleToString(module, show_debug_info);
}

}  // anonymous namespace

static std::string ImportGraphDefImpl(const std::string &proto,
                                      const std::string &pass_pipeline,
                                      bool show_debug_info,
                                      GraphDebugInfo &debug_info,
                                      GraphImportConfig &specs,
                                      TF_Status *status) {
  GraphDef graphdef;
  auto s = tensorflow::LoadProtoFromBuffer(proto, &graphdef);
  if (!s.ok()) {
    Set_TF_Status_from_Status(status, s);
    return "// error";
  }
  mlir::MLIRContext context;
  auto module = ConvertGraphdefToMlir(graphdef, debug_info, specs, &context);
  if (!module.ok()) {
    Set_TF_Status_from_Status(status, module.status());
    return "// error";
  }

  return RunPassPipelineOnModule(module->get(), pass_pipeline, show_debug_info,
                                 status);
}

std::string ImportFunction(const std::string &functiondef_proto,
                           const std::string &pass_pipeline,
                           bool show_debug_info, TFE_Context *tfe_context,
                           TF_Status *status) {
  FunctionDef functiondef;
  auto s = tensorflow::LoadProtoFromBuffer(functiondef_proto, &functiondef);
  if (!s.ok()) {
    Set_TF_Status_from_Status(status, s);
    return "// error";
  }

  const std::string &function_name = functiondef.signature().name();
  EagerContext *cpp_context = ContextFromInterface(unwrap(tfe_context));
  FunctionLibraryDefinition &flib_def = *cpp_context->FuncLibDef();
  const tensorflow::FunctionDef *fdef = flib_def.Find(function_name);
  if (fdef == nullptr) {
    s = tensorflow::errors::NotFound("Cannot find function ", function_name);
    Set_TF_Status_from_Status(status, s);
    return "// error";
  }

  std::unique_ptr<tensorflow::FunctionBody> fbody;
  s = FunctionDefToBodyHelper(*fdef, tensorflow::AttrSlice(), &flib_def,
                              &fbody);
  if (!s.ok()) {
    Set_TF_Status_from_Status(status, s);
    return "// error";
  }

  mlir::MLIRContext context;
  auto module = ConvertFunctionToMlir(fbody.get(), flib_def, &context);
  if (!module.ok()) {
    Set_TF_Status_from_Status(status, module.status());
    return "// error";
  }

  return RunPassPipelineOnModule(module->get(), pass_pipeline, show_debug_info,
                                 status);
}

std::string ImportGraphDef(const std::string &proto,
                           const std::string &pass_pipeline,
                           bool show_debug_info, TF_Status *status) {
  GraphDebugInfo debug_info;
  GraphImportConfig specs;
  return ImportGraphDefImpl(proto, pass_pipeline, show_debug_info, debug_info,
                            specs, status);
}

std::string ImportGraphDef(const std::string &proto,
                           const std::string &pass_pipeline,
                           bool show_debug_info, absl::string_view input_names,
                           absl::string_view input_data_types,
                           absl::string_view input_data_shapes,
                           absl::string_view output_names, TF_Status *status) {
  std::vector<string> node_names = absl::StrSplit(input_names, ',');
  std::vector<string> node_dtypes = absl::StrSplit(input_data_types, ',');
  std::vector<string> node_shapes_str = absl::StrSplit(input_data_shapes, ':');
  std::vector<std::vector<int>> node_shapes;

  std::vector<int> dims;
  for (const string &shape_str : node_shapes_str) {
    dims.clear();
    if (!shape_str.empty()) {
      for (const auto &dim_str : absl::StrSplit(shape_str, ',')) {
        int size;
        if (absl::SimpleAtoi(dim_str, &size)) {
          dims.push_back(size);
        } else {
          auto s = tensorflow::errors::InvalidArgument(
              "Invalid Shape Specified.", dim_str);
          Set_TF_Status_from_Status(status, s);
          return "// error";
        }
      }
    }
    node_shapes.push_back(dims);
  }
  std::vector<string> output_nodes = absl::StrSplit(output_names, ',');

  GraphDebugInfo debug_info;
  GraphImportConfig specs;

  // Set the output to the output nodes.
  specs.outputs = output_nodes;

  // Set the input values to specs.input.
  std::vector<std::string> used_node_dtypes;
  if (node_dtypes.empty() ||
      (node_dtypes.size() == 1 && node_dtypes[0].empty())) {
    // Mark all the node dtypes Invalid, so the importer can handle them by
    // using the type from the graph.
    used_node_dtypes.resize(node_names.size(), DataType_Name(DT_INVALID));
  } else if (node_names.size() == node_dtypes.size()) {
    for (const auto &dtype : node_dtypes) {
      if (dtype.empty()) {
        used_node_dtypes.push_back(DataType_Name(DT_INVALID));
      } else if (dtype != DataType_Name(DT_INVALID)) {
        used_node_dtypes.push_back(dtype);

        // Use '' if you want to use the type from graph.
      } else {
        auto s = tensorflow::errors::InvalidArgument(
            "DT_INVALID isn't a valid input data type");
        Set_TF_Status_from_Status(status, s);
        return "// error";
      }
    }

    // Unmatched input node array and data type sizes.
  } else {
    auto s = tensorflow::errors::InvalidArgument(
        "Length of input node array and data type doesn't match");
    Set_TF_Status_from_Status(status, s);
    return "// error";
  }

  // Unmatched input node array and data shapes sizes.
  if (node_names.size() != node_shapes.size()) {
    auto s = tensorflow::errors::InvalidArgument(
        "Length of input node array and data shape doesn't match");
    Set_TF_Status_from_Status(status, s);
    return "// error";
  }
  for (unsigned i = 0, e = node_names.size(); i < e; i++) {
    const string &name = node_names[i];
    if (name.empty()) continue;

    auto it_inserted_pair = specs.inputs.insert({name, {}});
    ArrayInfo &info = it_inserted_pair.first->second;
    for (auto &dim : node_shapes[i]) {
      info.shape.add_dim()->set_size(dim);
    }
  }
  return ImportGraphDefImpl(proto, pass_pipeline, show_debug_info, debug_info,
                            specs, status);
}

std::string ExperimentalConvertSavedModelToMlir(
    const std::string &saved_model_path, const std::string &exported_names_str,
    bool show_debug_info, TF_Status *status) {
  // Load the saved model into a SavedModelV2Bundle.

  tensorflow::SavedModelV2Bundle bundle;
  auto load_status =
      tensorflow::SavedModelV2Bundle::Load(saved_model_path, &bundle);
  if (!load_status.ok()) {
    Set_TF_Status_from_Status(status, load_status);
    return "// error";
  }

  // Convert the SavedModelV2Bundle to an MLIR module.

  std::vector<string> exported_names =
      absl::StrSplit(exported_names_str, ',', absl::SkipEmpty());
  mlir::MLIRContext context;
  auto module_or = ConvertSavedModelToMlir(
      &bundle, &context, absl::Span<std::string>(exported_names));
  if (!module_or.status().ok()) {
    Set_TF_Status_from_Status(status, module_or.status());
    return "// error";
  }

  return MlirModuleToString(*module_or.ConsumeValueOrDie(), show_debug_info);
}

std::string ExperimentalConvertSavedModelV1ToMlirLite(
    const std::string &saved_model_path, const std::string &exported_names_str,
    const std::string &tags, bool upgrade_legacy, bool show_debug_info,
    TF_Status *status) {
  std::unordered_set<string> tag_set =
      absl::StrSplit(tags, ',', absl::SkipEmpty());

  std::vector<string> exported_names =
      absl::StrSplit(exported_names_str, ',', absl::SkipEmpty());
  mlir::MLIRContext context;

  tensorflow::MLIRImportOptions import_options;
  import_options.upgrade_legacy = upgrade_legacy;
  auto module_or = SavedModelSignatureDefsToMlirImportLite(
      saved_model_path, tag_set, absl::Span<std::string>(exported_names),
      &context, import_options);
  if (!module_or.status().ok()) {
    Set_TF_Status_from_Status(status, module_or.status());
    return "// error";
  }

  return MlirModuleToString(*module_or.ValueOrDie(), show_debug_info);
}

std::string ExperimentalConvertSavedModelV1ToMlir(
    const std::string &saved_model_path, const std::string &exported_names_str,
    const std::string &tags, bool lift_variables, bool upgrade_legacy,
    bool show_debug_info, TF_Status *status) {
  // Load the saved model into a SavedModelBundle.

  std::unordered_set<string> tag_set =
      absl::StrSplit(tags, ',', absl::SkipEmpty());

  tensorflow::SavedModelBundle bundle;
  auto load_status =
      tensorflow::LoadSavedModel({}, {}, saved_model_path, tag_set, &bundle);
  if (!load_status.ok()) {
    Set_TF_Status_from_Status(status, load_status);
    return "// error";
  }

  // Convert the SavedModelBundle to an MLIR module.
  std::vector<string> exported_names =
      absl::StrSplit(exported_names_str, ',', absl::SkipEmpty());
  mlir::MLIRContext context;
  tensorflow::MLIRImportOptions import_options;
  import_options.upgrade_legacy = upgrade_legacy;
  auto module_or =
      ConvertSavedModelV1ToMlir(bundle, absl::Span<std::string>(exported_names),
                                &context, import_options, lift_variables);
  if (!module_or.status().ok()) {
    Set_TF_Status_from_Status(status, module_or.status());
    return "// error";
  }

  // Run the tf standard pipeline by default and then, run passes that lift
  // variables if the flag is set on the module.
  mlir::OwningModuleRef module = module_or.ConsumeValueOrDie();
  mlir::PassManager pm(&context);
  std::string error;
  llvm::raw_string_ostream error_stream(error);

  mlir::TF::StandardPipelineOptions tf_options;
  mlir::TF::CreateTFStandardPipeline(pm, tf_options);

  mlir::StatusScopedDiagnosticHandler diagnostic_handler(&context);
  if (failed(pm.run(*module))) {
    Set_TF_Status_from_Status(status, diagnostic_handler.ConsumeStatus());
    return "// error";
  }
  return MlirModuleToString(*module, show_debug_info);
}

std::string ExperimentalRunPassPipeline(const std::string &mlir_txt,
                                        const std::string &pass_pipeline,
                                        bool show_debug_info,
                                        TF_Status *status) {
  mlir::DialectRegistry registry;
  mlir::RegisterAllTensorFlowDialects(registry);
  mlir::MLIRContext context(registry);
  mlir::OwningModuleRef module;
  {
    mlir::StatusScopedDiagnosticHandler diagnostic_handler(&context);
    module = mlir::parseSourceString(mlir_txt, &context);
    if (!module) {
      Set_TF_Status_from_Status(status, diagnostic_handler.ConsumeStatus());
      return "// error";
    }
  }

  // Run the pass_pipeline on the module.
  mlir::PassManager pm(&context);
  std::string error;
  llvm::raw_string_ostream error_stream(error);
  mlir::registerAllPasses();
  if (failed(mlir::parsePassPipeline(pass_pipeline, pm, error_stream))) {
    TF_SetStatus(status, TF_INVALID_ARGUMENT,
                 ("Invalid pass_pipeline: " + error_stream.str()).c_str());
    return "// error";
  }

  mlir::StatusScopedDiagnosticHandler diagnostic_handler(&context);
  if (failed(pm.run(*module))) {
    Set_TF_Status_from_Status(status, diagnostic_handler.ConsumeStatus());
    return "// error";
  }
  return MlirModuleToString(*module, show_debug_info);
}

}  // namespace tensorflow
