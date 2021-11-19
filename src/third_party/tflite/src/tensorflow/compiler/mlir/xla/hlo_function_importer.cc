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

#include "tensorflow/compiler/mlir/xla/hlo_function_importer.h"

#include <unordered_map>

#include "absl/algorithm/container.h"
#include "absl/types/optional.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/BlockAndValueMapping.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/Identifier.h"  // from @llvm-project
#include "mlir/IR/Location.h"  // from @llvm-project
#include "mlir/IR/Region.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/error_util.h"
#include "tensorflow/compiler/mlir/xla/attribute_importer.h"
#include "tensorflow/compiler/mlir/xla/hlo_utils.h"
#include "tensorflow/compiler/xla/comparison_util.h"
#include "tensorflow/compiler/xla/protobuf_util.h"
#include "tensorflow/compiler/xla/service/hlo_casting_utils.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_instructions.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

using llvm::APInt;
using llvm::makeArrayRef;
using mlir::DenseIntElementsAttr;
using mlir::FuncOp;
using mlir::NamedAttribute;
using mlir::Operation;
using mlir::RankedTensorType;
using mlir::Type;
using mlir::Value;

namespace xla {

namespace {

// Note: This sanitization function causes an irreversible many-to-one mapping
// and any solution to mitigate this would cause issues with the reverse
// direction. Longterm solution is to add a function attribute to maintain the
// original HLO naming.
string SanitizeFunctionName(llvm::StringRef name) {
  string output(name);
  llvm::for_each(output, [](char& x) { x = x == '-' ? '_' : x; });
  return output;
}

// Returns whether the instruction is a default dot operation.
bool DotIsDefault(const HloInstruction* instruction) {
  auto dnums = instruction->dot_dimension_numbers();
  DotDimensionNumbers default_dimension_numbers;
  default_dimension_numbers.add_lhs_contracting_dimensions(
      instruction->operand(0)->shape().dimensions_size() == 1 ? 0 : 1);
  default_dimension_numbers.add_rhs_contracting_dimensions(0);
  return xla::protobuf_util::ProtobufEquals(dnums, default_dimension_numbers);
}

// Returns an MLIR Location generated from HLO Instruction. Uses instruction
// metadata if present or instruction name.
mlir::Location GenerateInstructionLocation(const HloInstruction* instruction,
                                           mlir::OpBuilder* func_builder) {
  const std::string& op_name = instruction->metadata().op_name();
  if (op_name.empty()) {
    return mlir::NameLoc::get(func_builder->getIdentifier(instruction->name()));
  }

  mlir::Location op_name_loc =
      mlir::NameLoc::get(func_builder->getIdentifier(op_name));
  const std::string& source_file = instruction->metadata().source_file();
  if (source_file.empty()) {
    return op_name_loc;
  }

  return func_builder->getFusedLoc(
      {op_name_loc,
       mlir::FileLineColLoc::get(func_builder->getContext(), source_file,
                                 instruction->metadata().source_line(), 0)});
}
}  // namespace

Status HloFunctionImporter::ImportAsFunc(
    const HloComputation& computation, mlir::ModuleOp module,
    std::unordered_map<const HloComputation*, FuncOp>* function_map,
    mlir::Builder* builder) {
  HloFunctionImporter importer(module, function_map, builder);
  return importer.ImportAsFunc(computation).status();
}

Status HloFunctionImporter::ImportAsRegion(
    const xla::HloComputation& computation, mlir::Region* region,
    mlir::Builder* builder) {
  HloFunctionImporter importer(region->getParentOfType<mlir::ModuleOp>(), {},
                               builder);
  return importer.ImportAsRegion(computation, region);
}

StatusOr<mlir::FuncOp> HloFunctionImporter::ImportAsFunc(
    const HloComputation& computation) {
  auto& imported = (*function_map_)[&computation];
  if (imported) return imported;
  llvm::SmallVector<Type, 4> args, rets;
  TF_RETURN_IF_ERROR(GetMlirTypes(computation.parameter_instructions(), &args));
  TF_RETURN_IF_ERROR(GetMlirTypes({computation.root_instruction()}, &rets));
  auto func_type = mlir::FunctionType::get(context_, args, rets);

  string computation_name =
      computation.parent()->entry_computation() == &computation
          ? "main"
          : SanitizeFunctionName(computation.name());

  // Construct the MLIR function and map arguments.
  llvm::ArrayRef<mlir::NamedAttribute> attrs;
  auto function = mlir::FuncOp::create(mlir::UnknownLoc::get(context_),
                                       computation_name, func_type, attrs);
  auto visibility = computation_name == "main" ? FuncOp::Visibility::Public
                                               : FuncOp::Visibility::Private;
  function.setVisibility(visibility);
  module_.push_back(function);

  // Add to the map right away for function calls.
  imported = function;

  mlir::Block* block = function.addEntryBlock();
  TF_RETURN_IF_ERROR(ImportInstructions(computation, block));

  return function;
}

tensorflow::Status HloFunctionImporter::ImportAsRegion(
    const HloComputation& computation, mlir::Region* region) {
  // TODO(hinsu): Store computation name as an attribute for round-trip.
  auto* block = new mlir::Block;
  region->push_back(block);

  llvm::SmallVector<Type, 4> args;
  TF_RETURN_IF_ERROR(GetMlirTypes(computation.parameter_instructions(), &args));
  block->addArguments(args);

  return ImportInstructions(computation, block);
}

StatusOr<Value> HloFunctionImporter::ImportInstructionsImpl(
    const xla::HloComputation& computation,
    const llvm::SmallVectorImpl<Value>& arguments, mlir::OpBuilder* builder) {
  // Setup the input parameters.
  const int num_parameters = computation.num_parameters();

  if (arguments.size() != num_parameters)
    return InvalidArgument("Caller vs callee argument sizes do not match");

  for (int i = 0; i < num_parameters; i++) {
    auto hlo_parameter = computation.parameter_instruction(i);
    instruction_value_map_[hlo_parameter] = arguments[i];
  }

  for (auto instruction : computation.MakeInstructionPostOrder()) {
    TF_ASSIGN_OR_RETURN(auto operands, GetOperands(instruction));
    TF_ASSIGN_OR_RETURN(
        auto new_operation,
        ImportInstructionWithLayout(instruction, operands, builder));
    if (new_operation) {
      instruction_value_map_[instruction] = new_operation->getResult(0);
    }
  }

  // Setup the return type (HLO only supports a single return value).
  return GetMlirValue(computation.root_instruction());
}

Status HloFunctionImporter::ImportInstructions(
    const HloComputation& computation, mlir::Block* block) {
  llvm::SmallVector<Value, 4> arguments(block->args_begin(), block->args_end());
  mlir::OpBuilder builder = mlir::OpBuilder::atBlockEnd(block);
  TF_ASSIGN_OR_RETURN(Value result,
                      ImportInstructionsImpl(computation, arguments, &builder));

  // TODO(suderman): Add location tracking details.
  mlir::Location loc = builder.getUnknownLoc();

  // Create terminator op depending on the parent op of this region.
  if (llvm::isa<FuncOp>(block->getParentOp())) {
    builder.create<mlir::ReturnOp>(loc, result);
  } else {
    builder.create<mlir::mhlo::ReturnOp>(loc, result);
  }
  return tensorflow::Status::OK();
}

StatusOr<Value> HloFunctionImporter::ImportInstructions(
    const xla::HloComputation& computation,
    const llvm::SmallVectorImpl<Value>& arguments, mlir::OpBuilder* builder) {
  mlir::Block* block = builder->getBlock();
  if (block == nullptr)
    return InvalidArgument(
        "ImportInstructions requires a valid block in the builder");

  HloFunctionImporter importer(
      block->getParent()->getParentOfType<mlir::ModuleOp>(), {}, builder);
  return importer.ImportInstructionsImpl(computation, arguments, builder);
}

StatusOr<mlir::Operation*> HloFunctionImporter::ImportInstruction(
    const xla::HloInstruction* instr,
    const llvm::SmallVectorImpl<mlir::Value>& operands,
    mlir::OpBuilder* builder) {
  mlir::Block* block = builder->getBlock();
  if (block == nullptr)
    return InvalidArgument(
        "ImportInstructions requires a valid block in the builder");

  HloFunctionImporter importer(
      block->getParent()->getParentOfType<mlir::ModuleOp>(), {}, builder);

  return importer.ImportInstructionWithLayout(instr, operands, builder);
}

StatusOr<mlir::Operation*> HloFunctionImporter::ImportInstructionImpl(
    const HloInstruction* instruction,
    const llvm::SmallVectorImpl<mlir::Value>& operands,
    mlir::OpBuilder* func_builder) {
  TF_ASSIGN_OR_RETURN(auto result_type, ConvertShapeToType<RankedTensorType>(
                                            instruction->shape(), *builder_));
  mlir::Location loc = GenerateInstructionLocation(instruction, func_builder);

  llvm::SmallVector<NamedAttribute, 10> attributes;
  switch (instruction->opcode()) {
    case HloOpcode::kParameter: {
      return nullptr;
    }
    case HloOpcode::kConstant: {
      const Literal& literal = instruction->literal();
      auto attr = CreateDenseElementsAttrFromLiteral(literal, *builder_);
      if (!attr.ok()) return attr.status();
      mlir::Operation* new_operation =
          func_builder->create<mlir::mhlo::ConstOp>(loc, attr.ValueOrDie());
      for (auto attr : attributes) {
        new_operation->setAttr(attr.first, attr.second);
      }
      return new_operation;
    }
    case HloOpcode::kIota: {
      return func_builder
          ->create<mlir::mhlo::IotaOp>(
              loc, result_type,
              func_builder->getI64IntegerAttr(
                  Cast<HloIotaInstruction>(instruction)->iota_dimension()))
          .getOperation();
    }
    case HloOpcode::kBroadcast: {
      // Note that the HLO broadcast is more powerful than the XLA broadcast
      // op. BroadcastInDim offers a superset of the HLO op's functionality.
      attributes.push_back(
          builder_->getNamedAttr("broadcast_dimensions",
                                 ConvertDimensions(instruction->dimensions())));
      return func_builder
          ->create<mlir::mhlo::BroadcastInDimOp>(loc, result_type, operands,
                                                 attributes)
          .getOperation();
    }

    case HloOpcode::kBatchNormGrad:
    case HloOpcode::kBatchNormInference:
    case HloOpcode::kBatchNormTraining:
      attributes.push_back(builder_->getNamedAttr(
          "epsilon", builder_->getF32FloatAttr(instruction->epsilon())));
      attributes.push_back(builder_->getNamedAttr(
          "feature_index",
          builder_->getI64IntegerAttr(instruction->feature_index())));
      if (instruction->opcode() == HloOpcode::kBatchNormGrad) {
        return func_builder
            ->create<mlir::mhlo::BatchNormGradOp>(loc, result_type, operands,
                                                  attributes)
            .getOperation();
      } else if (instruction->opcode() == HloOpcode::kBatchNormInference) {
        return func_builder
            ->create<mlir::mhlo::BatchNormInferenceOp>(loc, result_type,
                                                       operands, attributes)
            .getOperation();
      } else {
        assert(instruction->opcode() == HloOpcode::kBatchNormTraining);
        return func_builder
            ->create<mlir::mhlo::BatchNormTrainingOp>(loc, result_type,
                                                      operands, attributes)
            .getOperation();
      }

    case HloOpcode::kDot: {
      attributes.push_back(builder_->getNamedAttr(
          "precision_config",
          ConvertPrecisionConfig(&instruction->precision_config(), builder_)));

      // Consider consolidating DotOps together.
      if (DotIsDefault(instruction)) {
        return func_builder
            ->create<mlir::mhlo::DotOp>(loc, result_type, operands, attributes)
            .getOperation();
      }

      attributes.push_back(builder_->getNamedAttr(
          "dot_dimension_numbers",
          ConvertDotDimensionNumbers(instruction->dot_dimension_numbers(),
                                     builder_)));
      return func_builder
          ->create<mlir::mhlo::DotGeneralOp>(loc, result_type, operands,
                                             attributes)
          .getOperation();
    }
    case HloOpcode::kCall: {
      TF_ASSIGN_OR_RETURN(FuncOp function,
                          ImportAsFunc(*instruction->to_apply()));
      mlir::Operation* new_operation =
          func_builder->create<mlir::CallOp>(loc, function, operands);
      return new_operation;
    }
    case HloOpcode::kCollectivePermute: {
      attributes.push_back(ConvertSourceTargetPairs(
          instruction->source_target_pairs(), builder_));
      return func_builder
          ->create<mlir::mhlo::CollectivePermuteOp>(loc, result_type, operands,
                                                    attributes)
          .getOperation();
    }
    case HloOpcode::kCustomCall: {
      auto custom_call = Cast<HloCustomCallInstruction>(instruction);
      TF_ASSIGN_OR_RETURN(
          auto mlir_api_version,
          ConvertCustomCallApiVersion(custom_call->api_version()));
      attributes.push_back(builder_->getNamedAttr(
          "call_target_name",
          builder_->getStringAttr(custom_call->custom_call_target())));
      attributes.push_back(builder_->getNamedAttr(
          "has_side_effect",
          builder_->getBoolAttr(custom_call->custom_call_has_side_effect())));
      attributes.push_back(builder_->getNamedAttr(
          "backend_config",
          builder_->getStringAttr(custom_call->raw_backend_config_string())));
      attributes.push_back(builder_->getNamedAttr(
          "api_version", mlir::mhlo::CustomCallApiVersionAttr::get(
                             builder_->getContext(), mlir_api_version)));
      return func_builder
          ->create<mlir::mhlo::CustomCallOp>(loc, result_type, operands,
                                             attributes)
          .getOperation();
    }
    case HloOpcode::kCompare: {
      auto compare = Cast<HloCompareInstruction>(instruction);
      attributes.push_back(ConvertComparisonDirection(compare->direction()));
      auto default_type = Comparison::DefaultComparisonType(
          compare->operand(0)->shape().element_type());
      if (compare->type() != default_type)
        attributes.push_back(ConvertComparisonType(compare->type()));
      return func_builder
          ->create<mlir::mhlo::CompareOp>(loc, result_type, operands,
                                          attributes)
          .getOperation();
    }
    case HloOpcode::kCholesky: {
      attributes.push_back(builder_->getNamedAttr(
          "lower",
          builder_->getBoolAttr(instruction->cholesky_options().lower())));
      return func_builder
          ->create<mlir::mhlo::CholeskyOp>(loc, result_type, operands,
                                           attributes)
          .getOperation();
    }
    case HloOpcode::kGather: {
      auto gather_instruction = Cast<HloGatherInstruction>(instruction);
      attributes.push_back(builder_->getNamedAttr(
          "dimension_numbers",
          ConvertGatherDimensionNumbers(
              gather_instruction->gather_dimension_numbers(), builder_)));

      std::vector<int64_t> slice_sizes(
          gather_instruction->gather_slice_sizes().begin(),
          gather_instruction->gather_slice_sizes().end());
      attributes.push_back(
          builder_->getNamedAttr("slice_sizes", Convert(slice_sizes)));
      attributes.push_back(builder_->getNamedAttr(
          "indices_are_sorted",
          builder_->getBoolAttr(gather_instruction->indices_are_sorted())));

      return func_builder
          ->create<mlir::mhlo::GatherOp>(loc, result_type, operands, attributes)
          .getOperation();
    }
    case HloOpcode::kDynamicSlice: {
      std::vector<int64_t> slice_sizes(
          instruction->dynamic_slice_sizes().begin(),
          instruction->dynamic_slice_sizes().end());
      return func_builder
          ->create<mlir::mhlo::DynamicSliceOp>(
              loc, result_type, operands[0],
              makeArrayRef(operands).drop_front(), Convert(slice_sizes))
          .getOperation();
    }
    case HloOpcode::kDynamicUpdateSlice: {
      return func_builder
          ->create<mlir::mhlo::DynamicUpdateSliceOp>(
              loc, result_type, operands[0], operands[1],
              llvm::ArrayRef<Value>(operands.begin() + 2, operands.end()))
          .getOperation();
    }
    case HloOpcode::kInfeed: {
      attributes.push_back(builder_->getNamedAttr(
          "infeed_config",
          mlir::StringAttr::get(builder_->getContext(),
                                instruction->infeed_config())));
      TF_ASSIGN_OR_RETURN(mlir::Attribute layout,
                          ConvertShapeToMlirLayout(instruction->shape()));
      attributes.push_back(builder_->getNamedAttr("layout", layout));
      return func_builder
          ->create<mlir::mhlo::InfeedOp>(loc, result_type, operands, attributes)
          .getOperation();
    }
    case HloOpcode::kOutfeed: {
      attributes.push_back(builder_->getNamedAttr(
          "outfeed_config",
          mlir::StringAttr::get(builder_->getContext(),
                                instruction->outfeed_config())));
      return func_builder
          ->create<mlir::mhlo::OutfeedOp>(loc, result_type, operands,
                                          attributes)
          .getOperation();
    }
    case HloOpcode::kPad: {
      const auto& padding_config = instruction->padding_config();
      llvm::SmallVector<int64_t, 4> edge_padding_low;
      llvm::SmallVector<int64_t, 4> edge_padding_high;
      llvm::SmallVector<int64_t, 4> interior_padding;
      edge_padding_low.reserve(padding_config.dimensions_size());
      edge_padding_high.reserve(padding_config.dimensions_size());
      interior_padding.reserve(padding_config.dimensions_size());

      for (const auto& dimension : padding_config.dimensions()) {
        edge_padding_low.push_back(dimension.edge_padding_low());
        edge_padding_high.push_back(dimension.edge_padding_high());
        interior_padding.push_back(dimension.interior_padding());
      }

      return func_builder
          ->create<mlir::mhlo::PadOp>(loc, result_type, operands[0],
                                      operands[1], Convert(edge_padding_low),
                                      Convert(edge_padding_high),
                                      Convert(interior_padding))
          .getOperation();
    }
    case HloOpcode::kScatter: {
      auto scatter = Cast<HloScatterInstruction>(instruction);
      attributes.push_back(builder_->getNamedAttr(
          "scatter_dimension_numbers",
          ConvertScatterDimensionNumbers(scatter->scatter_dimension_numbers(),
                                         builder_)));
      attributes.push_back(builder_->getNamedAttr(
          "indices_are_sorted",
          builder_->getBoolAttr(scatter->indices_are_sorted())));
      attributes.push_back(builder_->getNamedAttr(
          "unique_indices", builder_->getBoolAttr(scatter->unique_indices())));

      auto scatter_op = func_builder->create<mlir::mhlo::ScatterOp>(
          loc, result_type, operands, attributes);
      TF_RETURN_IF_ERROR(ImportAsRegion(*scatter->to_apply(),
                                        &scatter_op.update_computation()));
      return scatter_op.getOperation();
    }
    case HloOpcode::kSelectAndScatter: {
      auto select_scatter = Cast<HloSelectAndScatterInstruction>(instruction);
      llvm::SmallVector<int64_t, 4> window_strides, window_dimensions;
      llvm::SmallVector<int64_t, 8> padding;
      for (const auto& dim : select_scatter->window().dimensions()) {
        window_strides.push_back(dim.stride());
        window_dimensions.push_back(dim.size());
        padding.push_back(dim.padding_low());
        padding.push_back(dim.padding_high());
      }
      attributes.push_back(
          builder_->getNamedAttr("window_strides", Convert(window_strides)));
      attributes.push_back(builder_->getNamedAttr("window_dimensions",
                                                  Convert(window_dimensions)));
      attributes.push_back(ConvertPadding(padding));
      auto select_scatter_op =
          func_builder->create<mlir::mhlo::SelectAndScatterOp>(
              loc, result_type, operands, attributes);
      TF_RETURN_IF_ERROR(ImportAsRegion(*select_scatter->select(),
                                        &select_scatter_op.select()));
      TF_RETURN_IF_ERROR(ImportAsRegion(*select_scatter->scatter(),
                                        &select_scatter_op.scatter()));
      return select_scatter_op.getOperation();
    }
    case HloOpcode::kSetDimensionSize: {
      attributes.push_back(builder_->getNamedAttr(
          "dimension", builder_->getI64IntegerAttr(instruction->dimension())));
      return func_builder
          ->create<mlir::mhlo::SetDimensionSizeOp>(loc, result_type, operands,
                                                   attributes)
          .getOperation();
    }
    case HloOpcode::kSlice: {
      return func_builder
          ->create<mlir::mhlo::SliceOp>(
              loc, result_type, operands[0],
              ConvertDimensions(instruction->slice_starts()),
              ConvertDimensions(instruction->slice_limits()),
              ConvertDimensions(instruction->slice_strides()))
          .getOperation();
    }
    case HloOpcode::kSort: {
      auto sort_instruction = Cast<HloSortInstruction>(instruction);

      llvm::SmallVector<Type, 4> return_types = {result_type};
      if (mlir::TupleType tuple_ty = result_type.dyn_cast<mlir::TupleType>()) {
        return_types = llvm::to_vector<6>(tuple_ty.getTypes());
      }

      auto sort_op = func_builder->create<mlir::mhlo::SortOp>(
          loc, return_types, operands,
          builder_->getI64IntegerAttr(sort_instruction->sort_dimension()),
          builder_->getBoolAttr(sort_instruction->is_stable()));
      TF_RETURN_IF_ERROR(
          ImportAsRegion(*sort_instruction->to_apply(), &sort_op.comparator()));

      // Check if the output needs to be tupled.
      if (return_types.size() == 1 && return_types.front() == result_type) {
        return sort_op.getOperation();
      }

      return func_builder
          ->create<mlir::mhlo::TupleOp>(loc, result_type, sort_op.getResults())
          .getOperation();
    }
    case HloOpcode::kConditional: {
      llvm::SmallVector<Type, 4> rets;
      mlir::Type pred_or_index_type =
          operands[0].getType().cast<mlir::TensorType>().getElementType();
      // It is a predicated conditional if first argument is a boolean and
      // should be mapped to If op.
      if (pred_or_index_type.isInteger(1)) {
        TF_RETURN_IF_ERROR(GetMlirTypes(
            {instruction->true_computation()->root_instruction()}, &rets));

        auto op = func_builder->create<mlir::mhlo::IfOp>(loc, rets, operands,
                                                         attributes);
        TF_RETURN_IF_ERROR(ImportAsRegion(*instruction->true_computation(),
                                          &op.true_branch()));
        TF_RETURN_IF_ERROR(ImportAsRegion(*instruction->false_computation(),
                                          &op.false_branch()));
        return op.getOperation();
      }

      // Otherwise, it is a indexed conditional and should be mapped to Case
      // op.
      TF_RETURN_IF_ERROR(GetMlirTypes(
          {instruction->branch_computation(0)->root_instruction()}, &rets));

      int num_branches = instruction->branch_count();
      auto op = func_builder->create<mlir::mhlo::CaseOp>(
          loc, rets, operands, attributes, num_branches);
      for (auto index_and_computation :
           llvm::enumerate(instruction->branch_computations())) {
        auto index = index_and_computation.index();
        HloComputation* computation = index_and_computation.value();
        TF_RETURN_IF_ERROR(ImportAsRegion(*computation, &op.branches()[index]));
      }
      return op.getOperation();
    }
    case HloOpcode::kConcatenate: {
      // TODO(b/132057942): Support taking an uint64_t instead of an
      // IntegerAttr for concatenate dimension.
      return func_builder
          ->create<mlir::mhlo::ConcatenateOp>(
              loc, result_type, operands,
              builder_->getI64IntegerAttr(instruction->concatenate_dimension()))
          .getOperation();
    }
    case HloOpcode::kAllGather: {
      auto all_gather = Cast<HloAllGatherInstruction>(instruction);
      attributes.push_back(builder_->getNamedAttr(
          "all_gather_dim",
          builder_->getI64IntegerAttr(all_gather->all_gather_dimension())));
      attributes.push_back(
          ConvertReplicaGroups(all_gather->replica_groups(), builder_));
      attributes.push_back(ConvertChannelHandle(all_gather->channel_id()));
      return func_builder
          ->create<mlir::mhlo::AllGatherOp>(loc, result_type, operands,
                                            attributes)
          .getOperation();
    }
    case HloOpcode::kAllReduce: {
      auto all_reduce = Cast<HloAllReduceInstruction>(instruction);
      attributes.push_back(
          ConvertReplicaGroups(all_reduce->replica_groups(), builder_));
      attributes.push_back(ConvertChannelHandle(all_reduce->channel_id()));
      auto all_reduce_op = func_builder->create<mlir::mhlo::AllReduceOp>(
          loc, result_type, operands, attributes);
      TF_RETURN_IF_ERROR(ImportAsRegion(*all_reduce->to_apply(),
                                        &all_reduce_op.computation()));
      return all_reduce_op.getOperation();
    }
    case HloOpcode::kReduce: {
      // Operands in the first half are reduction inputs and the remaining
      // operands are corresponding initial values.
      size_t num_inputs = operands.size() / 2;
      llvm::SmallVector<Type, 4> return_types = {result_type};
      if (mlir::TupleType tuple_ty = result_type.dyn_cast<mlir::TupleType>()) {
        return_types = llvm::to_vector<6>(tuple_ty.getTypes());
      }

      auto reduce = func_builder->create<mlir::mhlo::ReduceOp>(
          loc, return_types,
          llvm::makeArrayRef(operands).take_front(num_inputs),
          llvm::makeArrayRef(operands).drop_front(num_inputs),
          ConvertDimensions(instruction->dimensions()));
      TF_RETURN_IF_ERROR(
          ImportAsRegion(*instruction->to_apply(), &reduce.body()));

      // Check if the output needs to be tupled.
      if (return_types.size() == 1 && return_types.front() == result_type) {
        return reduce.getOperation();
      }

      return func_builder
          ->create<mlir::mhlo::TupleOp>(loc, result_type, reduce.getResults())
          .getOperation();
    }
    case HloOpcode::kReverse: {
      return func_builder
          ->create<mlir::mhlo::ReverseOp>(
              loc, result_type, operands[0],
              ConvertDimensions(instruction->dimensions()))
          .getOperation();
    }
    case HloOpcode::kRng: {
      auto shape = func_builder->create<mlir::arith::ConstantOp>(
          loc, Convert(result_type.cast<RankedTensorType>().getShape()));
      switch (instruction->random_distribution()) {
        case xla::RNG_UNIFORM:
          return func_builder
              ->create<mlir::mhlo::RngUniformOp>(loc, result_type, operands[0],
                                                 operands[1], shape)
              .getOperation();

        case xla::RNG_NORMAL:
          return func_builder
              ->create<mlir::mhlo::RngNormalOp>(loc, result_type, operands[0],
                                                operands[1], shape)
              .getOperation();

        default:
          return tensorflow::errors::InvalidArgument(absl::StrCat(
              "Unsupported distribution: ",
              RandomDistributionToString(instruction->random_distribution())));
      }
    }
    case HloOpcode::kRngBitGenerator: {
      auto rng_op = Cast<HloRngBitGeneratorInstruction>(instruction);
      auto op = func_builder->create<mlir::mhlo::RngBitGeneratorOp>(
          loc, result_type,
          func_builder->getI32IntegerAttr(rng_op->algorithm()), operands[0]);
      return op.getOperation();
    }
    case HloOpcode::kWhile: {
      auto op = func_builder->create<mlir::mhlo::WhileOp>(
          loc, operands[0].getType(), operands[0]);
      TF_RETURN_IF_ERROR(
          ImportAsRegion(*instruction->while_condition(), &op.cond()));
      TF_RETURN_IF_ERROR(
          ImportAsRegion(*instruction->while_body(), &op.body()));
      return op.getOperation();
    }
    case HloOpcode::kGetTupleElement: {
      attributes.push_back(builder_->getNamedAttr(
          "index", builder_->getIntegerAttr(builder_->getIntegerType(32),
                                            instruction->tuple_index())));
      return func_builder
          ->create<mlir::mhlo::GetTupleElementOp>(loc, result_type, operands,
                                                  attributes)
          .getOperation();
    };
    case HloOpcode::kGetDimensionSize: {
      attributes.push_back(builder_->getNamedAttr(
          "dimension", builder_->getI64IntegerAttr(instruction->dimension())));
      return func_builder
          ->create<mlir::mhlo::GetDimensionSizeOp>(loc, result_type, operands,
                                                   attributes)
          .getOperation();
    };
    case HloOpcode::kTranspose: {
      attributes.push_back(builder_->getNamedAttr(
          "permutation", ConvertDimensions(instruction->dimensions())));
      return func_builder
          ->create<mlir::mhlo::TransposeOp>(loc, result_type, operands,
                                            attributes)
          .getOperation();
    }
    case HloOpcode::kTriangularSolve: {
      attributes.push_back(builder_->getNamedAttr(
          "left_side",
          builder_->getBoolAttr(
              instruction->triangular_solve_options().left_side())));
      attributes.push_back(builder_->getNamedAttr(
          "lower", builder_->getBoolAttr(
                       instruction->triangular_solve_options().lower())));
      attributes.push_back(builder_->getNamedAttr(
          "unit_diagonal",
          builder_->getBoolAttr(
              instruction->triangular_solve_options().unit_diagonal())));
      auto transpose_a =
          builder_->getStringAttr(TriangularSolveOptions::Transpose_Name(
              instruction->triangular_solve_options().transpose_a()));
      attributes.push_back(builder_->getNamedAttr("transpose_a", transpose_a));
      return func_builder
          ->create<mlir::mhlo::TriangularSolveOp>(loc, result_type, operands,
                                                  attributes)
          .getOperation();
    }
    case HloOpcode::kReduceWindow: {
      llvm::SmallVector<Type, 4> return_types = {result_type};
      if (mlir::TupleType tuple_ty = result_type.dyn_cast<mlir::TupleType>()) {
        return_types = llvm::to_vector<6>(tuple_ty.getTypes());
      }
      llvm::SmallVector<int64_t, 4> sizes, strides, base_dilations,
          win_dilations;
      llvm::SmallVector<int64_t, 8> padding;
      for (const auto& dim : instruction->window().dimensions()) {
        sizes.push_back(dim.size());
        strides.push_back(dim.stride());
        base_dilations.push_back(dim.base_dilation());
        win_dilations.push_back(dim.window_dilation());
        padding.push_back(dim.padding_low());
        padding.push_back(dim.padding_high());
      }
      attributes.push_back(builder_->getNamedAttr("window_dimensions",
                                                  ConvertDimensions(sizes)));
      attributes.push_back(
          builder_->getNamedAttr("window_strides", ConvertDimensions(strides)));
      attributes.push_back(builder_->getNamedAttr(
          "base_dilations", ConvertDimensions(base_dilations)));
      attributes.push_back(builder_->getNamedAttr(
          "window_dilations", ConvertDimensions(win_dilations)));
      attributes.push_back(ConvertPadding(padding));
      auto reduce = func_builder->create<mlir::mhlo::ReduceWindowOp>(
          loc, return_types, operands, attributes);
      TF_RETURN_IF_ERROR(
          ImportAsRegion(*instruction->to_apply(), &reduce.body()));

      // Check if the output needs to be tupled.
      if (return_types.size() == 1 && return_types.front() == result_type) {
        return reduce.getOperation();
      }

      return func_builder
          ->create<mlir::mhlo::TupleOp>(loc, result_type, reduce.getResults())
          .getOperation();
    }
    case HloOpcode::kMap: {
      auto op = func_builder->create<mlir::mhlo::MapOp>(
          loc, result_type, operands,
          ConvertDimensions(instruction->dimensions()));
      TF_RETURN_IF_ERROR(
          ImportAsRegion(*instruction->to_apply(), &op.computation()));
      return op.getOperation();
    }
    case HloOpcode::kConvolution: {
      llvm::SmallVector<int64_t, 4> strides, lhs_dilations, rhs_dilations;
      llvm::SmallVector<int64_t, 8> paddings;
      for (const auto& dim : instruction->window().dimensions()) {
        strides.push_back(dim.stride());
        lhs_dilations.push_back(dim.base_dilation());
        rhs_dilations.push_back(dim.window_dilation());
        paddings.push_back(dim.padding_low());
        paddings.push_back(dim.padding_high());
      }

      attributes.push_back(
          builder_->getNamedAttr("window_strides", Convert(strides)));
      attributes.push_back(ConvertPadding(paddings));
      attributes.push_back(
          builder_->getNamedAttr("lhs_dilation", Convert(lhs_dilations)));
      attributes.push_back(
          builder_->getNamedAttr("rhs_dilation", Convert(rhs_dilations)));
      attributes.push_back(builder_->getNamedAttr(
          "dimension_numbers",
          ConvertConvDimensionNumbers(
              instruction->convolution_dimension_numbers(), builder_)));
      attributes.push_back(builder_->getNamedAttr(
          "feature_group_count",
          builder_->getI64IntegerAttr(instruction->feature_group_count())));
      attributes.push_back(builder_->getNamedAttr(
          "batch_group_count",
          builder_->getI64IntegerAttr(instruction->batch_group_count())));
      attributes.push_back(builder_->getNamedAttr(
          "precision_config",
          ConvertPrecisionConfig(&instruction->precision_config(), builder_)));

      return func_builder
          ->create<mlir::mhlo::ConvOp>(loc, result_type, operands, attributes)
          .getOperation();
    }

    case HloOpcode::kFft: {
      auto fft_type =
          builder_->getStringAttr(FftType_Name(instruction->fft_type()));

      std::vector<int64_t> fft_length(instruction->fft_length().begin(),
                                      instruction->fft_length().end());

      attributes.push_back(builder_->getNamedAttr("fft_type", fft_type));
      attributes.push_back(
          builder_->getNamedAttr("fft_length", Convert(fft_length)));
      return func_builder
          ->create<mlir::mhlo::FftOp>(loc, result_type, operands, attributes)
          .getOperation();
    }

    case HloOpcode::kAdd: {
      // HLO add ops on PRED elements are actually boolean or, but MHLO dialect
      // AddOps on i1 are just addition with overflow; so, we have to implement
      // the special behavior of HLO add ops on PRED here by creating an
      // arith::OrIOp instead.
      if (instruction->shape().element_type() == PRED) {
        return func_builder
            ->create<mlir::mhlo::OrOp>(loc, result_type, operands, attributes)
            .getOperation();
      } else {
        return func_builder
            ->create<mlir::mhlo::AddOp>(loc, result_type, operands, attributes)
            .getOperation();
      }
    }
    case HloOpcode::kAfterAll: {
      // HLO AfterAll ops without any token input are used to just create a
      // token. MHLO has a special op CreateToken for this case.
      if (instruction->operands().empty()) {
        return func_builder
            ->create<mlir::mhlo::CreateTokenOp>(loc, result_type, operands,
                                                attributes)
            .getOperation();
      } else {
        return func_builder
            ->create<mlir::mhlo::AfterAllOp>(loc, result_type, operands,
                                             attributes)
            .getOperation();
      }
    }

#define NO_ATTRIBUTE_CASE(hlo_op_code, mlir_op)                               \
  case HloOpcode::hlo_op_code: {                                              \
    return func_builder                                                       \
        ->create<mlir::mhlo::mlir_op>(loc, result_type, operands, attributes) \
        .getOperation();                                                      \
  }

      // broadcast dimensions are never added here because they don't exist as
      // part of the HLO instruction. They are only a convenience in the XLA
      // builder API.
      NO_ATTRIBUTE_CASE(kAbs, AbsOp);
      NO_ATTRIBUTE_CASE(kAnd, AndOp);
      NO_ATTRIBUTE_CASE(kAtan2, Atan2Op);
      NO_ATTRIBUTE_CASE(kBitcastConvert, BitcastConvertOp);
      NO_ATTRIBUTE_CASE(kCbrt, CbrtOp);
      NO_ATTRIBUTE_CASE(kClz, ClzOp);
      NO_ATTRIBUTE_CASE(kConvert, ConvertOp);
      NO_ATTRIBUTE_CASE(kCeil, CeilOp);
      NO_ATTRIBUTE_CASE(kClamp, ClampOp);
      NO_ATTRIBUTE_CASE(kComplex, ComplexOp);
      NO_ATTRIBUTE_CASE(kCos, CosOp);
      NO_ATTRIBUTE_CASE(kDivide, DivOp);
      NO_ATTRIBUTE_CASE(kExp, ExpOp);
      NO_ATTRIBUTE_CASE(kExpm1, Expm1Op);
      NO_ATTRIBUTE_CASE(kFloor, FloorOp);
      NO_ATTRIBUTE_CASE(kIsFinite, IsFiniteOp);
      NO_ATTRIBUTE_CASE(kImag, ImagOp);
      NO_ATTRIBUTE_CASE(kLog, LogOp);
      NO_ATTRIBUTE_CASE(kLog1p, Log1pOp);
      NO_ATTRIBUTE_CASE(kMaximum, MaxOp);
      NO_ATTRIBUTE_CASE(kMinimum, MinOp);
      NO_ATTRIBUTE_CASE(kMultiply, MulOp);
      NO_ATTRIBUTE_CASE(kNegate, NegOp);
      NO_ATTRIBUTE_CASE(kNot, NotOp);
      NO_ATTRIBUTE_CASE(kOr, OrOp);
      NO_ATTRIBUTE_CASE(kPopulationCount, PopulationCountOp);
      NO_ATTRIBUTE_CASE(kPower, PowOp);
      NO_ATTRIBUTE_CASE(kReal, RealOp);
      NO_ATTRIBUTE_CASE(kRemainder, RemOp);
      NO_ATTRIBUTE_CASE(kReplicaId, ReplicaIdOp);
      NO_ATTRIBUTE_CASE(kLogistic, LogisticOp);
      // The dimensions attribute is not present on the HLO Reshape
      // instruction. If dimensions are non-default, the XLA builder
      // implements it as a separate transpose.
      NO_ATTRIBUTE_CASE(kReshape, ReshapeOp);
      NO_ATTRIBUTE_CASE(kRoundNearestAfz, RoundOp);
      NO_ATTRIBUTE_CASE(kRsqrt, RsqrtOp);
      NO_ATTRIBUTE_CASE(kSelect, SelectOp);
      NO_ATTRIBUTE_CASE(kShiftLeft, ShiftLeftOp);
      NO_ATTRIBUTE_CASE(kShiftRightArithmetic, ShiftRightArithmeticOp);
      NO_ATTRIBUTE_CASE(kShiftRightLogical, ShiftRightLogicalOp);
      NO_ATTRIBUTE_CASE(kSign, SignOp);
      NO_ATTRIBUTE_CASE(kSin, SinOp);
      NO_ATTRIBUTE_CASE(kSqrt, SqrtOp);
      NO_ATTRIBUTE_CASE(kSubtract, SubOp);
      NO_ATTRIBUTE_CASE(kTanh, TanhOp);
      NO_ATTRIBUTE_CASE(kTuple, TupleOp);
      NO_ATTRIBUTE_CASE(kXor, XorOp);
      // TODO(b/129422361) Copy needs special handling because it is not
      // defined in tensorflow/compiler/xla/client/xla_builder.h. See
      // operation semantics in
      // g3doc/platforms/xla/g3doc/internal/hlo_semantics#copy
      NO_ATTRIBUTE_CASE(kCopy, CopyOp);

#undef NO_ATTRIBUTE_CASE

    case HloOpcode::kFusion: {
      auto fusion = func_builder->create<mlir::mhlo::FusionOp>(
          loc, result_type, operands,
          builder_->getStringAttr(xla::ToString(instruction->fusion_kind())));
      TF_RETURN_IF_ERROR(
          ImportAsRegion(*instruction->fused_instructions_computation(),
                         &fusion.fused_computation()));
      return fusion.getOperation();
    }
    case HloOpcode::kBitcast: {
      auto bitcast = func_builder->create<mlir::mhlo::BitcastOp>(
          loc, result_type, operands, attributes);
      // Store the source and result layout as attributes. Although the MHLO
      // Bitcast operates on tensors, these layouts are relevant as they define
      // the mapping between the elements of the source and result.
      SetLayoutForMlir(bitcast, instruction->shape(), "result_layout");
      SetLayoutForMlir(bitcast, instruction->operand(0)->shape(),
                       "source_layout");
      return bitcast.getOperation();
    }
    case HloOpcode::kReducePrecision: {
      auto op = func_builder->create<mlir::mhlo::ReducePrecisionOp>(
          loc, result_type, operands[0], attributes);
      op.exponent_bitsAttr(func_builder->getIntegerAttr(
          func_builder->getI32Type(), instruction->exponent_bits()));
      op.mantissa_bitsAttr(func_builder->getIntegerAttr(
          func_builder->getI32Type(), instruction->mantissa_bits()));
      return op.getOperation();
    }
    case HloOpcode::kAddDependency:
      // Arbitrary op code that I suspect we will not implement for quite a
      // while and allows testing handling of unknown ops. Selected because it
      // is not mentioned in xla client anywhere or in the hlo of our sample
      // models.
    default: {
      mlir::OperationState result(loc, "mhlo.unknown");
      result.addOperands(operands);
      result.addTypes(result_type);
      for (auto attr : attributes) {
        result.attributes.push_back(attr);
      }

      return func_builder->createOperation(result);
    }
  }
}

void SetXlaShape(mlir::Operation* op, const Shape& shape) {
  op->setAttr("xla_shape",
              mlir::Builder(op->getContext())
                  .getStringAttr(shape.ToString(/*print_layout=*/true)));
}

StatusOr<mlir::Operation*> HloFunctionImporter::ImportInstructionWithLayout(
    const HloInstruction* instruction,
    const llvm::SmallVectorImpl<mlir::Value>& operands,
    mlir::OpBuilder* func_builder) {
  TF_ASSIGN_OR_RETURN(
      mlir::Operation * op,
      ImportInstructionImpl(instruction, operands, func_builder));
  if (op == nullptr) return op;

  // See MlirToHloConversionOptions for more about layouts.
  //
  // Minor-to-major is a permutation of [0, rank), presenting tensor dimensions
  // in physical minor-to-major order.
  if (instruction->shape().IsArray()) {
    if (!instruction->shape().layout().minor_to_major().empty() &&
        instruction->shape().layout() !=
            LayoutUtil::MakeDescendingLayout(
                instruction->shape().dimensions().size())) {
      SetXlaShape(op, instruction->shape());
    }
  } else {
    SetXlaShape(op, instruction->shape());
  }
  return op;
}

StatusOr<llvm::SmallVector<mlir::Value, 4>> HloFunctionImporter::GetOperands(
    const HloInstruction* instruction) {
  llvm::SmallVector<mlir::Value, 4> operands;
  for (const auto& operand : instruction->operands()) {
    auto input_it = instruction_value_map_.find(operand);
    if (input_it == instruction_value_map_.end()) {
      return tensorflow::errors::Internal(
          absl::StrCat("Could not find input value: ", operand->name(),
                       " for instruction ", instruction->name()));
    }
    operands.push_back(input_it->second);
  }
  return operands;
}

tensorflow::Status HloFunctionImporter::GetMlirTypes(
    const std::vector<HloInstruction*>& instructions,
    llvm::SmallVectorImpl<mlir::Type>* types) {
  for (auto instruction : instructions) {
    TF_ASSIGN_OR_RETURN(auto ret_type, ConvertShapeToType<RankedTensorType>(
                                           instruction->shape(), *builder_));
    types->push_back(ret_type);
  }
  return tensorflow::Status::OK();
}

StatusOr<Value> HloFunctionImporter::GetMlirValue(
    const HloInstruction* instruction) {
  auto lookup = instruction_value_map_.find(instruction);
  if (lookup != instruction_value_map_.end()) {
    return lookup->second;
  }

  return tensorflow::errors::Internal(absl::StrCat(
      "Unable to find value for input: ", instruction->ToString()));
}

mlir::NamedAttribute HloFunctionImporter::ConvertComparisonDirection(
    ComparisonDirection direction) {
  return builder_->getNamedAttr(
      "comparison_direction",
      builder_->getStringAttr(ComparisonDirectionToString(direction)));
}

mlir::NamedAttribute HloFunctionImporter::ConvertComparisonType(
    Comparison::Type type) {
  return builder_->getNamedAttr(
      "compare_type", builder_->getStringAttr(ComparisonTypeToString(type)));
}

mlir::DenseIntElementsAttr HloFunctionImporter::ConvertDimensions(
    llvm::ArrayRef<int64_t> op_dimensions) {
  llvm::SmallVector<APInt, 8> dimensions;
  dimensions.reserve(op_dimensions.size());
  for (auto value : op_dimensions) dimensions.emplace_back(APInt(64, value));

  return DenseIntElementsAttr::get(
      RankedTensorType::get(dimensions.size(), builder_->getIntegerType(64)),
      dimensions);
}

mlir::DenseIntElementsAttr HloFunctionImporter::Convert(
    llvm::ArrayRef<int64_t> elements) {
  return DenseIntElementsAttr::get(
      RankedTensorType::get(elements.size(), builder_->getIntegerType(64)),
      elements);
}

mlir::NamedAttribute HloFunctionImporter::ConvertPadding(
    llvm::ArrayRef<int64_t> padding) {
  auto ty =
      mlir::RankedTensorType::get({static_cast<int64_t>(padding.size()) / 2, 2},
                                  builder_->getIntegerType(64));
  auto attr = DenseIntElementsAttr::get(ty, padding);
  return builder_->getNamedAttr("padding", attr);
}

mlir::NamedAttribute HloFunctionImporter::ConvertSourceTargetPairs(
    const std::vector<std::pair<int64_t, int64_t>>& source_target_pairs,
    mlir::Builder* builder) {
  std::vector<int64_t> attr(source_target_pairs.size() * 2);
  for (auto p : llvm::enumerate(source_target_pairs)) {
    attr[2 * p.index()] = p.value().first;
    attr[2 * p.index() + 1] = p.value().second;
  }
  auto type = mlir::RankedTensorType::get(
      {static_cast<int64_t>(attr.size() / 2), 2}, builder->getIntegerType(64));
  return builder->getNamedAttr("source_target_pairs",
                               DenseIntElementsAttr::get(type, attr));
}

mlir::NamedAttribute HloFunctionImporter::ConvertReplicaGroups(
    absl::Span<const ReplicaGroup> replica_groups, mlir::Builder* builder) {
  const int64_t num_groups = replica_groups.size();
  // Replica groups in HLO can be non-uniform in size, for example:
  // replica_groups={{0},{1,2},{3}}. Since we are representing them as a 2D
  // tensor, pad the smaller sized replica groups with -1.
  const int64_t group_size = absl::c_accumulate(
      replica_groups, int64_t(0), [](int64_t current, const ReplicaGroup& g) {
        return std::max<int64_t>(current, g.replica_ids_size());
      });
  // Initialize all elements to -1 to support non-uniform replica groups.
  std::vector<int64_t> attr(num_groups * group_size, -1);
  for (int i = 0; i < num_groups; ++i) {
    int index = i * group_size;
    for (const int64_t& id : replica_groups[i].replica_ids())
      attr[index++] = id;
  }
  auto type = mlir::RankedTensorType::get({num_groups, group_size},
                                          builder->getIntegerType(64));
  return builder->getNamedAttr("replica_groups",
                               DenseIntElementsAttr::get(type, attr));
}

mlir::NamedAttribute HloFunctionImporter::ConvertChannelHandle(
    absl::optional<int64_t> channel_id) {
  xla::ChannelHandle channel_handle;
  if (channel_id) channel_handle.set_handle(*channel_id);
  return ConvertChannelHandle(channel_handle);
}

mlir::NamedAttribute HloFunctionImporter::ConvertChannelHandle(
    const xla::ChannelHandle& channel) {
  return builder_->getNamedAttr(
      "channel_handle",
      mlir::mhlo::ChannelHandle::get(
          builder_->getI64IntegerAttr(channel.handle()),
          builder_->getI64IntegerAttr(channel.type()), context_));
}

void HloFunctionImporter::SetLayoutForMlir(mlir::Operation* op,
                                           const Shape& shape,
                                           llvm::StringRef attr_name) {
  llvm::SmallVector<int64_t, 4> minor_to_major(
      shape.layout().minor_to_major().begin(),
      shape.layout().minor_to_major().end());
  op->setAttr(
      attr_name,
      mlir::Builder(op->getContext()).getIndexTensorAttr(minor_to_major));
}

StatusOr<mlir::Attribute> HloFunctionImporter::ConvertShapeToMlirLayout(
    const xla::Shape& shape) {
  if (shape.IsToken()) return builder_->getUnitAttr();
  if (shape.IsTuple()) {
    std::vector<mlir::Attribute> tuple_layouts;
    for (int i = 0; i < shape.tuple_shapes_size(); i++) {
      TF_ASSIGN_OR_RETURN(mlir::Attribute layout,
                          ConvertShapeToMlirLayout(shape.tuple_shapes(i)));
      tuple_layouts.push_back(layout);
    }
    llvm::ArrayRef<mlir::Attribute> array_ref(tuple_layouts);
    return builder_->getArrayAttr(array_ref);
  }
  if (shape.IsArray()) {
    const xla::Layout l = shape.layout();
    std::vector<mlir::Attribute> minor_to_major;
    for (int64_t i : l.minor_to_major()) {
      minor_to_major.push_back(builder_->getI64IntegerAttr(i));
    }
    llvm::ArrayRef<mlir::Attribute> array_ref(minor_to_major);
    return builder_->getArrayAttr(array_ref);
  }
  return tensorflow::errors::Internal("Couldn't convert layout.");
}

}  // namespace xla
