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

#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/Dialect/SCF/SCF.h"  // from @llvm-project
#include "mlir/Dialect/Shape/IR/Shape.h"  // from @llvm-project
#include "mlir/Dialect/StandardOps/IR/Ops.h"  // from @llvm-project
#include "mlir/IR/AsmState.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Verifier.h"  // from @llvm-project
#include "mlir/Parser.h"  // from @llvm-project
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_executor.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tfr/ir/tfr_ops.h"
#include "tensorflow/python/lib/core/pybind11_lib.h"
#include "tensorflow/python/lib/core/pybind11_status.h"

PYBIND11_MODULE(tfr_wrapper, m) {
  m.def("verify", [](std::string input) {
    mlir::DialectRegistry registry;
    registry.insert<mlir::scf::SCFDialect, mlir::TF::TensorFlowDialect,
                    mlir::StandardOpsDialect, mlir::shape::ShapeDialect,
                    mlir::TFR::TFRDialect>();
    mlir::MLIRContext ctx(registry);
    ctx.loadAllAvailableDialects();

    llvm::SourceMgr source_mgr = llvm::SourceMgr();
    source_mgr.AddNewSourceBuffer(llvm::MemoryBuffer::getMemBuffer(input),
                                  llvm::SMLoc());
    auto module = mlir::parseSourceFile(source_mgr, &ctx);
    if (!module) {
      return false;
    }

    mlir::SourceMgrDiagnosticHandler sourceMgrHandler(source_mgr, &ctx);
    if (failed(mlir::verify(*module))) {
      module->emitError("Invalid MLIR module: failed verification.");
      return false;
    }
    return true;
  });
}
