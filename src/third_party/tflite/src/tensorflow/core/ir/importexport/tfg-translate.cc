/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "mlir/IR/AsmState.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/OwningOpRef.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Tools/mlir-translate/MlirTranslateMain.h"  // from @llvm-project
#include "mlir/Tools/mlir-translate/Translation.h"  // from @llvm-project
#include "tensorflow/core/ir/dialect.h"
#include "tensorflow/core/ir/importexport/export.h"
#include "tensorflow/core/ir/importexport/import.h"
#include "tensorflow/core/ir/importexport/load_proto.h"

namespace mlir {

TranslateToMLIRRegistration graphdef_to_mlir(
    "graphdef-to-mlir", [](StringRef proto_txt, MLIRContext *context) {
      tensorflow::GraphDebugInfo debug_info;
      tensorflow::GraphDef graphdef;
      tensorflow::Status status = tensorflow::LoadProtoFromBuffer(
          {proto_txt.data(), proto_txt.size()}, &graphdef);
      if (!status.ok()) {
        LOG(ERROR) << status.error_message();
        return OwningOpRef<mlir::ModuleOp>{};
      }
      auto errorOrModule =
          tfg::ImportGraphDefToMlir(context, debug_info, graphdef);
      if (!errorOrModule.ok()) {
        LOG(ERROR) << errorOrModule.status();
        return OwningOpRef<mlir::ModuleOp>{};
      }
      return std::move(errorOrModule.ValueOrDie());
    });

TranslateFromMLIRRegistration mlir_to_graphdef(
    "mlir-to-graphdef",
    [](ModuleOp module, raw_ostream &output) {
      tensorflow::GraphDef graphdef;
      tensorflow::Status status =
          tensorflow::ExportMlirToGraphdef(module, &graphdef);
      if (!status.ok()) {
        LOG(ERROR) << "Error exporting MLIR module to GraphDef: " << status;
        return failure();
      }
      output << graphdef.DebugString();
      return success();
    },
    [](DialectRegistry &registry) { registry.insert<tfg::TFGraphDialect>(); });
}  //  namespace mlir

int main(int argc, char **argv) {
  mlir::registerAsmPrinterCLOptions();
  return failed(
      mlir::mlirTranslateMain(argc, argv, "Graph(Def)<->TFG Translation Tool"));
}
