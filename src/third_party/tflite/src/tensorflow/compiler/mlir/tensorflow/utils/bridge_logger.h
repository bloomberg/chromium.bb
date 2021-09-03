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

#ifndef TENSORFLOW_COMPILER_MLIR_TENSORFLOW_UTILS_BRIDGE_LOGGER_H_
#define TENSORFLOW_COMPILER_MLIR_TENSORFLOW_UTILS_BRIDGE_LOGGER_H_

#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project

namespace tensorflow {

// Logger for logging/dumping MLIR modules before and after passes in bridge
// targeting TPUs.
class BridgeLoggerConfig : public mlir::PassManager::IRPrinterConfig {
 public:
  explicit BridgeLoggerConfig(bool print_module_scope = false,
                              bool print_after_only_on_change = true);

  // A hook that may be overridden by a derived config that checks if the IR
  // of 'operation' should be dumped *before* the pass 'pass' has been
  // executed. If the IR should be dumped, 'print_callback' should be invoked
  // with the stream to dump into.
  void printBeforeIfEnabled(mlir::Pass *pass, mlir::Operation *operation,
                            PrintCallbackFn print_callback) override;

  // A hook that may be overridden by a derived config that checks if the IR
  // of 'operation' should be dumped *after* the pass 'pass' has been
  // executed. If the IR should be dumped, 'print_callback' should be invoked
  // with the stream to dump into.
  void printAfterIfEnabled(mlir::Pass *pass, mlir::Operation *operation,
                           PrintCallbackFn print_callback) override;
};

// Logger for logging/dumping pass pipeline timings after completion.
class BridgeTimingConfig : public mlir::PassManager::PassTimingConfig {
 public:
  // Hook that control how/where is the output produced
  void printTiming(PrintCallbackFn printCallback) override;
};

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TENSORFLOW_UTILS_BRIDGE_LOGGER_H_
