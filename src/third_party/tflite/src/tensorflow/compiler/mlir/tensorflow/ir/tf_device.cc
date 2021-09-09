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

#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SMLoc.h"
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/OpDefinition.h"  // from @llvm-project
#include "mlir/IR/OpImplementation.h"  // from @llvm-project
#include "mlir/IR/OperationSupport.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "mlir/IR/TypeUtilities.h"  // from @llvm-project
#include "mlir/IR/Types.h"  // from @llvm-project
#include "mlir/IR/UseDefLists.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/InliningUtils.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/core/platform/logging.h"

namespace mlir {
namespace tf_device {

//===----------------------------------------------------------------------===//
// TF Device Dialect Interfaces
//===----------------------------------------------------------------------===//

namespace {
struct TFInlinerInterface : public DialectInlinerInterface {
  using DialectInlinerInterface::DialectInlinerInterface;

  //===--------------------------------------------------------------------===//
  // Analysis Hooks
  //===--------------------------------------------------------------------===//

  // Defines the legality of inlining TF Device operations.
  bool isLegalToInline(Operation*, Region*, BlockAndValueMapping&) const final {
    // For now, enable inlining all operations.
    return true;
  }

  //===--------------------------------------------------------------------===//
  // Transformation Hooks
  //===--------------------------------------------------------------------===//

  // Attempts to materialize a conversion for a type mismatch between a call
  // from this dialect, and a callable region. This method should generate an
  // operation that takes 'input' as the only operand, and produces a single
  // result of 'resultType'. If a conversion can not be generated, nullptr
  // should be returned.
  // This is just re-using the same logic as the TensorFlow dialect right now.
  Operation* materializeCallConversion(OpBuilder& builder, Value input,
                                       Type result_type,
                                       Location conversion_loc) const final {
    if (!result_type.isa<TensorType>() || !input.getType().isa<TensorType>())
      return nullptr;
    return builder.create<TF::CastOp>(conversion_loc, result_type, input,
                                      /*truncate=*/builder.getBoolAttr(false));
  }
};

// Checks if a block wraps a single operation and the single operation results
// are perfectly forwarded to the block's terminator.
bool BlockWrapsSingleOp(Block* block) {
  auto body = block->without_terminator();
  if (!hasSingleElement(body)) return false;

  Operation& wrapped_op = *body.begin();
  Operation* terminator = block->getTerminator();
  return wrapped_op.getNumResults() == terminator->getNumOperands() &&
         std::equal(wrapped_op.getResults().begin(),
                    wrapped_op.getResults().end(),
                    terminator->getOperands().begin());
}
}  // end anonymous namespace

TensorFlowDeviceDialect::TensorFlowDeviceDialect(MLIRContext* context)
    : Dialect(/*name=*/"tf_device", context) {
  addOperations<
#define GET_OP_LIST
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.cc.inc"
      >();

  addInterfaces<TFInlinerInterface>();
}

//===----------------------------------------------------------------------===//
// tf_device.launch
//===----------------------------------------------------------------------===//

// Checks if a tf_device.launch wraps a single operation and the single
// operation results are perfectly forwarded to the launch return.
bool LaunchOp::WrapsSingleOp() { return BlockWrapsSingleOp(&GetBody()); }

//===----------------------------------------------------------------------===//
// tf_device.return
//===----------------------------------------------------------------------===//

namespace {
ParseResult ParseReturnOp(OpAsmParser* parser, OperationState* state) {
  llvm::SmallVector<OpAsmParser::OperandType, 2> op_info;
  llvm::SmallVector<Type, 2> types;
  llvm::SMLoc loc = parser->getCurrentLocation();
  return failure(parser->parseOperandList(op_info) ||
                 (!op_info.empty() && parser->parseColonTypeList(types)) ||
                 parser->resolveOperands(op_info, types, loc, state->operands));
}

void Print(ReturnOp op, OpAsmPrinter* p) {
  *p << op.getOperationName();
  if (op.getNumOperands() > 0) {
    *p << ' ';
    p->printOperands(op.getOperands());
    *p << " : ";
    interleaveComma(op.getOperandTypes(), *p);
  }
}
}  // anonymous namespace

//===----------------------------------------------------------------------===//
// tf_device.parallel_execute
//===----------------------------------------------------------------------===//

namespace {

LogicalResult Verify(ParallelExecuteOp op) {
  const auto& regions = op.getOperation()->getRegions();
  if (regions.size() < 2) {
    return op.emitOpError() << "must have at least two regions.";
  }

  int output_index = 0;
  for (auto& region_and_index : llvm::enumerate(regions)) {
    auto& region = region_and_index.value();
    auto* region_terminator = region.front().getTerminator();

    // Check that output types of regions match return operand types.
    for (auto result_type : region_terminator->getOperandTypes()) {
      if (result_type !=
          op.getOperation()->getResult(output_index++).getType()) {
        return op.emitOpError() << "output types must be a concatenated "
                                << "list of output types for each regions.";
      }
    }
  }

  // Check that total number of outputs from regions match the output types of
  // the parallel_execute op.
  const int num_output_types = op.getOperation()->getNumResults();
  if (num_output_types != output_index) {
    return op.emitOpError()
           << "number of output types (" << num_output_types << ") "
           << "must match the total number of outputs from all "
           << "regions (" << output_index << ").";
  }

  return success();
}

}  // namespace

// static
void ParallelExecuteOp::build(OpBuilder& builder, OperationState& state,
                              int num_regions,
                              llvm::ArrayRef<Type> output_types) {
  DCHECK_GE(num_regions, 2);
  for (int i = 0; i < num_regions; ++i) {
    Region* region = state.addRegion();
    region->push_back(new Block);
  }
  state.addTypes(output_types);
}

Block& ParallelExecuteOp::GetRegionBlockWithIndex(unsigned index) {
  return getOperation()->getRegion(index).front();
}

Operation::result_range ParallelExecuteOp::GetRegionOutputs(
    unsigned region_index) {
  int num_region_results =
      GetRegionBlockWithIndex(region_index).getTerminator()->getNumOperands();

  int return_value_offset = 0;
  for (int region_id = 0; region_id < region_index; ++region_id)
    return_value_offset +=
        GetRegionBlockWithIndex(region_id).getTerminator()->getNumOperands();

  Operation::result_range region_results(getOperation(),
                                         /*startIndex=*/return_value_offset,
                                         /*count=*/num_region_results);
  return region_results;
}

bool ParallelExecuteOp::RegionWrapsSingleOp(unsigned index) {
  return BlockWrapsSingleOp(&GetRegionBlockWithIndex(index));
}

//===----------------------------------------------------------------------===//
// tf_device.replicate
//===----------------------------------------------------------------------===//

namespace {
ParseResult ParseReplicateOpOperands(
    OpAsmParser* parser, OperationState* state,
    llvm::SmallVectorImpl<llvm::SmallVector<OpAsmParser::OperandType, 8>>*
        operands,
    llvm::SmallVectorImpl<OpAsmParser::OperandType>* region_args,
    llvm::SmallVectorImpl<Type>* region_arg_types) {
  // No operands or empty operand list.
  bool parsed_l_paren = succeeded(parser->parseOptionalLParen());
  if (!parsed_l_paren || succeeded(parser->parseOptionalRParen()))
    return success();

  // Parse comma separated operands of the following format:
  //   [%a, ...] as %block_arg: type
  do {
    if (parser->parseOperandList(operands->emplace_back(),
                                 OpAsmParser::Delimiter::Square) ||
        parser->parseKeyword("as",
                             " between replicated inputs and block argument") ||
        parser->parseRegionArgument(region_args->emplace_back()) ||
        parser->parseColonType(region_arg_types->emplace_back()))
      return failure();
  } while (succeeded(parser->parseOptionalComma()));

  // Parse remaining `)` surrounding operands.
  return parser->parseRParen();
}

ParseResult SetOperands(
    llvm::SMLoc loc, OpAsmParser* parser, OperationState* state,
    llvm::ArrayRef<llvm::SmallVector<OpAsmParser::OperandType, 8>> operands,
    llvm::ArrayRef<Type> region_arg_types, int* n) {
  if (operands.empty()) return success();

  for (const auto& attr : state->attributes)
    if (attr.first.strref() == "n")
      if (auto n_attr = attr.second.dyn_cast<IntegerAttr>())
        *n = n_attr.getInt();

  if (*n < 2)
    return parser->emitError(loc) << "expects 'n' to be at least 2, got " << *n;

  for (int i = 0, e = operands.size(); i < e; ++i) {
    const auto& operand = operands[i];
    // Check if replicated input matches `n`.
    if (operand.size() != *n)
      return parser->emitError(loc)
             << "expects number of operands for replicated input " << i
             << " to be 'n' (" << *n << "), got " << operand.size();

    // Resolve replicated input and block argument type.
    if (parser->resolveOperands(operand, region_arg_types[i], state->operands))
      return failure();
  }

  return success();
}

ParseResult ParseReplicateOp(OpAsmParser* parser, OperationState* state) {
  llvm::SMLoc loc = parser->getCurrentLocation();

  // Parse operands, attributes, and region of op.
  llvm::SmallVector<llvm::SmallVector<OpAsmParser::OperandType, 8>, 8> operands;
  llvm::SmallVector<OpAsmParser::OperandType, 8> region_args;
  llvm::SmallVector<Type, 8> region_arg_types;
  int n = 0;
  Region& body = *state->addRegion();
  if (ParseReplicateOpOperands(parser, state, &operands, &region_args,
                               &region_arg_types) ||
      parser->parseOptionalAttrDict(state->attributes) ||
      SetOperands(loc, parser, state, operands, region_arg_types, &n) ||
      parser->parseRegion(body, region_args, region_arg_types))
    return failure();

  // Ensure that the region is well formed: it contains at least a block with
  // a ReturnOp terminator.
  ReplicateOp::ensureTerminator(body, parser->getBuilder(), state->location);

  if (!llvm::hasSingleElement(body))
    return parser->emitError(loc) << "expects a single block region";

  Operation& terminator = body.front().back();
  if (!isa<ReturnOp>(terminator))
    return parser->emitError(loc) << "expects a tf_device.return terminator";

  // Get the results type from the terminator type inside the replicate,
  // replicated each by `n`.
  state->types.reserve(terminator.getNumOperands() * n);
  for (const auto& type : terminator.getOperandTypes())
    state->types.append(n, type);

  return success();
}

void Print(ReplicateOp op, OpAsmPrinter* p) {
  *p << op.getOperationName();

  // Print comma separated operands of the following format:
  //   [%a, ...] as %block_arg: type
  int n = op.getAttrOfType<IntegerAttr>("n").getInt();
  if (op.getNumOperands()) {
    *p << '(';
    Block& block = op.body().front();
    interleaveComma(block.getArguments(), *p, [&](BlockArgument arg) {
      const int block_arg_num = arg.getArgNumber();
      *p << '[';
      p->printOperands(std::next(op.operand_begin(), block_arg_num * n),
                       std::next(op.operand_begin(), (block_arg_num + 1) * n));
      *p << "] as " << arg << ": " << arg.getType();
    });
    *p << ')';
  }

  p->printOptionalAttrDict(op.getAttrs());
  p->printRegion(op.body(), /*printEntryBlockArgs=*/false);
}

// Checks if two types are compatible (compatible shapes and same elemental
// type).
LogicalResult VerifyCompatibleTypes(Type a, Type b) {
  if (failed(verifyCompatibleShape(a, b)) ||
      getElementTypeOrSelf(a) != getElementTypeOrSelf(b))
    return failure();

  return success();
}

LogicalResult Verify(ReplicateOp op) {
  uint64_t n = op.n().getLimitedValue();
  if (n < 2)
    return op.emitOpError() << "expects 'n' to be at least 2, got " << n;

  // Check number of devices, if set, matches `n`.
  if (op.devices().hasValue()) {
    for (auto device_attr : op.devices().getValue().getValue()) {
      auto device_list = device_attr.second.dyn_cast_or_null<ArrayAttr>();
      if (!device_list)
        return op.emitError()
               << "expects 'devices' to be a map alias and device name list.";

      bool is_device_string = llvm::all_of(device_list, [](Attribute attr) {
        return attr.dyn_cast_or_null<StringAttr>();
      });
      if (!is_device_string)
        return op.emitOpError() << "expects 'devices' to be a consists of "
                                   "string list as values.";

      if (device_list.size() != n)
        return op.emitOpError()
               << "expects number of devices (" << device_list.size()
               << ") to be equal to 'n' (" << n << ")";
    }
  }

  Block& block = op.body().front();

  // Check number of operands matches `n` * number of block arguments.
  if (op.getNumOperands() != n * block.getNumArguments())
    return op.emitOpError()
           << "expects number of operands (" << op.getNumOperands()
           << ") to be equal to 'n' * number of block arguments (" << n << " * "
           << block.getNumArguments() << ")";

  // Check replicated input types match block argument types.
  for (auto block_arg : block.getArguments()) {
    Type block_arg_type = block_arg.getType();
    for (int i = n * block_arg.getArgNumber(), e = i + n; i < e; ++i)
      if (failed(VerifyCompatibleTypes(block_arg_type,
                                       op.getOperand(i).getType())))
        return op.emitOpError()
               << "incompatible types for operand " << i
               << " and block argument " << block_arg.getArgNumber();
  }

  Operation& terminator = block.back();

  // Check number of results matches `n` * number of return operands.
  if (op.getNumResults() != n * terminator.getNumOperands())
    return op.emitOpError()
           << "expects number of results (" << op.getNumResults()
           << ") to be equal to 'n' * number of terminator operands (" << n
           << " * " << terminator.getNumOperands() << ")";

  // Check replicated output types match return operand types.
  for (auto operand_type_and_idx :
       llvm::enumerate(terminator.getOperandTypes())) {
    Type operand_type = operand_type_and_idx.value();
    int operand_idx = operand_type_and_idx.index();
    for (int i = n * operand_idx, e = i + n; i < e; ++i)
      if (failed(VerifyCompatibleTypes(operand_type, op.getType(i))))
        return op.emitOpError() << "incompatible types for result " << i
                                << " and terminator operand " << operand_idx;
  }

  return success();
}

template <typename OperandsTy, typename ResultsTy>
void BuildReplicateOp(
    Builder* builder, OperationState* state, int n,
    const llvm::SmallDenseMap<StringRef, llvm::SmallVector<StringRef, 4>>&
        devices,
    llvm::ArrayRef<std::pair<OperandsTy, Type>> replicated_inputs,
    ResultsTy replica_output_types) {
  DCHECK_GE(n, 2);
  state->addAttribute("n", builder->getI32IntegerAttr(n));

  llvm::SmallVector<mlir::NamedAttribute, 1> device_list;
  device_list.reserve(devices.size());
  for (auto alias_and_devices : devices) {
    NamedAttribute device_name_attr = builder->getNamedAttr(
        alias_and_devices.getFirst(),
        builder->getStrArrayAttr(alias_and_devices.getSecond()));
    device_list.emplace_back(device_name_attr);
  }
  state->addAttribute("devices", builder->getDictionaryAttr(device_list));

  Region* region = state->addRegion();
  region->push_back(new Block);
  Block& block = region->front();

  for (auto& replicated_input : replicated_inputs) {
    DCHECK_EQ(llvm::size(replicated_input.first), n);
    for (auto input : replicated_input.first) {
      DCHECK(succeeded(
          VerifyCompatibleTypes(input.getType(), replicated_input.second)));
      state->addOperands(input);
    }
    block.addArgument(replicated_input.second);
  }

  for (const auto& output_type : replica_output_types)
    state->addTypes(llvm::SmallVector<Type, 8>(n, output_type));
}
}  // anonymous namespace

void ReplicateOp::build(
    OpBuilder& builder, OperationState& state, int n,
    const llvm::SmallDenseMap<StringRef, llvm::SmallVector<StringRef, 4>>&
        devices,
    llvm::ArrayRef<std::pair<llvm::ArrayRef<Value>, Type>> replicated_inputs,
    llvm::ArrayRef<Type> replica_output_types) {
  BuildReplicateOp(&builder, &state, n, devices, replicated_inputs,
                   replica_output_types);
}

void ReplicateOp::build(
    OpBuilder& builder, OperationState& state, int n,
    const llvm::SmallDenseMap<StringRef, llvm::SmallVector<StringRef, 4>>&
        devices,
    llvm::ArrayRef<std::pair<Operation::operand_range, Type>> replicated_inputs,
    Operation::result_type_range replica_output_types) {
  BuildReplicateOp(&builder, &state, n, devices, replicated_inputs,
                   replica_output_types);
}

//===----------------------------------------------------------------------===//
// Canonicalization patterns
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// tf_device.launch
//===----------------------------------------------------------------------===//

namespace {
// This pattern matches LaunchOps with only one ReturnOp (empty) and remaps the
// results of the LaunchOp to the operands of the ReturnOp.
struct DropEmptyLaunch : public OpRewritePattern<LaunchOp> {
  using OpRewritePattern<LaunchOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(LaunchOp op,
                                PatternRewriter& rewriter) const override {
    Block& block = op.GetBody();
    // Check if launch only has a return.
    if (&block.front() != &block.back()) return failure();

    // Map launch results to return operands.
    rewriter.replaceOp(op, block.front().getOperands());

    return success();
  }
};
}  // anonymous namespace

void LaunchOp::getCanonicalizationPatterns(OwningRewritePatternList& results,
                                           MLIRContext* context) {
  results.insert<DropEmptyLaunch>(context);
}

//===----------------------------------------------------------------------===//
// TableGen'd op method definitions
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.cc.inc"

}  // namespace tf_device
}  // namespace mlir
