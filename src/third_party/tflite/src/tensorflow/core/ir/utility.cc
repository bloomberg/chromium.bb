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

#include "tensorflow/core/ir/utility.h"

#include "mlir/IR/Block.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/Types.h"  // from @llvm-project
#include "tensorflow/core/ir/dialect.h"
#include "tensorflow/core/ir/interfaces.h"
#include "tensorflow/core/ir/types/dialect.h"

namespace mlir {
namespace tfg {

// For region-based loop ops, the first N block arguments are data values, with
// N control tokens afterwards.
Block::BlockArgListType GetLoopRegionDataArgs(Region &region) {
  Block::BlockArgListType args = region.getArguments();
  return args.drop_back(args.size() / 2);
}
Block::BlockArgListType GetLoopRegionControlTokens(Region &region) {
  Block::BlockArgListType args = region.getArguments();
  return args.drop_front(args.size() / 2);
}
BlockArgument GetLoopRegionControlOf(BlockArgument data) {
  Block &block = *data.getOwner();
  return block.getArgument(data.getArgNumber() + block.getNumArguments() / 2);
}
BlockArgument GetLoopRegionDataOf(BlockArgument ctl) {
  Block &block = *ctl.getOwner();
  return block.getArgument(ctl.getArgNumber() - block.getNumArguments() / 2);
}

Value LookupControlDependency(Value data) {
  assert(!data.getType().isa<ControlType>() && "expected a data type");
  // If the value is defined by an op, then the last result is the control
  // dependency.
  Value control_dep;
  if (auto result = data.dyn_cast<OpResult>()) {
    control_dep = *std::prev(result.getOwner()->result_end());
  } else {
    auto arg = data.cast<BlockArgument>();
    control_dep = cast<ControlArgumentInterface>(arg.getOwner()->getParentOp())
                      .getControlTokenOf(arg);
  }
  assert(control_dep.getType().isa<ControlType>() && "expected a control type");
  return control_dep;
}

Optional<Value> LookupDataValue(Value ctl) {
  assert(ctl.getType().isa<ControlType>() && "expected a control type");
  // If the value is defined by an op, then return the first result.
  Value data;
  if (auto result = ctl.dyn_cast<OpResult>()) {
    // If the op only has a control result, then there is no data value.
    if (result.getOwner()->getNumResults() == 1) return {};
    data = *result.getOwner()->result_begin();
  } else {
    auto arg = ctl.cast<BlockArgument>();
    data = cast<ControlArgumentInterface>(arg.getOwner()->getParentOp())
               .getDataValueOf(arg);
  }
  assert(!data.getType().isa<ControlType>() && "expected a data type");
  return data;
}

}  // namespace tfg
}  // namespace mlir
