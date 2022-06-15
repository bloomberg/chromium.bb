/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/compiler/xla/client/xla_builder.h"

#include <functional>
#include <numeric>
#include <queue>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"
#include "tensorflow/compiler/xla/client/sharding_builder.h"
#include "tensorflow/compiler/xla/client/xla_computation.h"
#include "tensorflow/compiler/xla/comparison_util.h"
#include "tensorflow/compiler/xla/execution_options_util.h"
#include "tensorflow/compiler/xla/layout_util.h"
#include "tensorflow/compiler/xla/permutation_util.h"
#include "tensorflow/compiler/xla/primitive_util.h"
#include "tensorflow/compiler/xla/service/hlo.pb.h"
#include "tensorflow/compiler/xla/service/hlo_input_output_alias_config.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_instructions.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/service/shape_inference.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/window_util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/stream_executor/lib/statusor.h"

namespace xla {

using absl::StrCat;

namespace {

static const char kNameSeparator = '.';

// Retrieves the base name of an instruction or computation fully qualified
// name, using separator as boundary between the initial base name part, and
// the numeric identification.
std::string GetBaseName(const std::string& name, char separator) {
  auto pos = name.rfind(separator);
  CHECK_NE(pos, std::string::npos) << name;
  return name.substr(0, pos);
}

// Generates a fully qualified computation/instruction name.
std::string GetFullName(const std::string& base_name, char separator,
                        int64_t id) {
  const char separator_str[] = {separator, '\0'};
  return StrCat(base_name, separator_str, id);
}

// Common function to standardize setting name and IDs on computation and
// instruction proto entities.
template <typename T>
void SetProtoIdAndName(T* entry, const std::string& base_name, char separator,
                       int64_t id) {
  entry->set_id(id);
  entry->set_name(GetFullName(base_name, separator, id));
}

bool InstrIsSetBound(const HloInstructionProto* instr_proto) {
  HloOpcode opcode = StringToHloOpcode(instr_proto->opcode()).ValueOrDie();
  if (opcode == HloOpcode::kCustomCall &&
      instr_proto->custom_call_target() == "SetBound") {
    return true;
  }
  return false;
}

}  // namespace

namespace internal {

XlaOp XlaBuilderFriend::BuildAddDependency(XlaBuilder* builder, XlaOp operand,
                                           XlaOp token, const Shape& shape) {
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();
    return builder->AddInstruction(std::move(instr), HloOpcode::kAddDependency,
                                   {operand, token});
  });
}

XlaOp XlaBuilderFriend::BuildFusion(XlaBuilder* builder,
                                    absl::Span<const XlaOp> operands,
                                    absl::string_view fusion_kind,
                                    const XlaComputation& fused_computation) {
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    instr.set_fusion_kind(std::string(fusion_kind));
    std::vector<const Shape*> operand_shape_ptrs;
    TF_ASSIGN_OR_RETURN(auto program_shape,
                        fused_computation.GetProgramShape());
    *instr.mutable_shape() = program_shape.result().ToProto();
    builder->AddCalledComputation(fused_computation, &instr);
    return builder->AddInstruction(std::move(instr), HloOpcode::kFusion,
                                   operands);
  });
}

XlaOp XlaBuilderFriend::BuildBitcast(XlaBuilder* builder, XlaOp operand,
                                     const Shape& shape) {
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();
    return builder->AddInstruction(std::move(instr), HloOpcode::kBitcast,
                                   {operand});
  });
}

XlaOp XlaBuilderFriend::BuildPartitionId(XlaBuilder* builder,
                                         const Shape& shape) {
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();
    return builder->AddInstruction(std::move(instr), HloOpcode::kPartitionId);
  });
}

XlaOp XlaBuilderFriend::BuildRngGetAndUpdateState(XlaBuilder* builder,

                                                  int64_t delta,
                                                  const Shape& shape) {
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    instr.set_delta(delta);
    *instr.mutable_shape() = shape.ToProto();
    return builder->AddInstruction(std::move(instr),
                                   HloOpcode::kRngGetAndUpdateState);
  });
}

HloInstructionProto* XlaBuilderFriend::GetInstruction(XlaOp op) {
  return &op.builder()
              ->instructions_[op.builder()->handle_to_index_[op.handle_]];
}

HloInstructionProto* XlaBuilderFriend::GetInstructionByHandle(
    XlaBuilder* builder, int64_t handle) {
  return &builder->instructions_[builder->handle_to_index_[handle]];
}

}  // namespace internal

XlaOp operator-(XlaOp x) { return Neg(x); }
XlaOp operator+(XlaOp x, XlaOp y) { return Add(x, y); }
XlaOp operator-(XlaOp x, XlaOp y) { return Sub(x, y); }
XlaOp operator*(XlaOp x, XlaOp y) { return Mul(x, y); }
XlaOp operator/(XlaOp x, XlaOp y) { return Div(x, y); }
XlaOp operator%(XlaOp x, XlaOp y) { return Rem(x, y); }

XlaOp operator~(XlaOp x) { return Not(x); }
XlaOp operator&(XlaOp x, XlaOp y) { return And(x, y); }
XlaOp operator|(XlaOp x, XlaOp y) { return Or(x, y); }
XlaOp operator^(XlaOp x, XlaOp y) { return Xor(x, y); }
XlaOp operator<<(XlaOp x, XlaOp y) { return ShiftLeft(x, y); }

XlaOp operator>>(XlaOp x, XlaOp y) {
  XlaBuilder* builder = x.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const xla::Shape* shape, builder->GetShapePtr(x));
    if (!ShapeUtil::ElementIsIntegral(*shape)) {
      return InvalidArgument(
          "Argument to >> operator does not have an integral type (%s).",
          ShapeUtil::HumanString(*shape));
    }
    if (ShapeUtil::ElementIsSigned(*shape)) {
      return ShiftRightArithmetic(x, y);
    } else {
      return ShiftRightLogical(x, y);
    }
  });
}

StatusOr<const Shape*> XlaBuilder::GetShapePtr(XlaOp op) const {
  TF_RETURN_IF_ERROR(first_error_);
  TF_RETURN_IF_ERROR(CheckOpBuilder(op));
  auto it = handle_to_index_.find(op.handle());
  if (it == handle_to_index_.end()) {
    return InvalidArgument("No XlaOp with handle %d", op.handle());
  }
  return instruction_shapes_.at(it->second).get();
}

StatusOr<Shape> XlaBuilder::GetShape(XlaOp op) const {
  TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(op));
  return *shape;
}

StatusOr<std::vector<Shape>> XlaBuilder::GetOperandShapes(
    absl::Span<const XlaOp> operands) const {
  std::vector<Shape> operand_shapes;
  operand_shapes.reserve(operands.size());
  for (XlaOp operand : operands) {
    TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(operand));
    operand_shapes.push_back(*shape);
  }
  return operand_shapes;
}

std::string XlaBuilder::OpToString(XlaOp op) const {
  std::string s;
  ToStringHelper(&s, /*ident=*/0, op.handle());
  return s;
}

static std::string ShapeToString(const xla::ShapeProto& shape) {
  if (shape.tuple_shapes_size() > 1) {
    return absl::StrCat(
        "(",
        absl::StrJoin(shape.tuple_shapes(), ", ",
                      [&](std::string* s, const xla::ShapeProto& subshape) {
                        absl::StrAppend(s, ShapeToString(subshape));
                      }),
        ")");
  }
  return absl::StrCat("[", absl::StrJoin(shape.dimensions(), ", "), "]");
}

void XlaBuilder::ToStringHelper(std::string* out, int ident,
                                int64_t op_handle) const {
  const HloInstructionProto& instr =
      *(LookUpInstructionByHandle(op_handle).ValueOrDie());
  absl::StrAppend(out, std::string(ident, ' '), instr.opcode(),
                  ", shape=", ShapeToString(instr.shape()));
  if (instr.has_metadata()) {
    absl::StrAppend(out, ", metadata={", instr.metadata().source_file(), ":",
                    instr.metadata().source_line(), "}");
  }
  if (instr.operand_ids_size()) {
    absl::StrAppend(out, "\n");
  }
  absl::StrAppend(out, absl::StrJoin(instr.operand_ids(), "\n",
                                     [&](std::string* s, int64_t subop) {
                                       ToStringHelper(s, ident + 2, subop);
                                     }));
}

XlaBuilder::XlaBuilder(const std::string& computation_name)
    : name_(computation_name) {}

XlaBuilder::~XlaBuilder() {}

XlaOp XlaBuilder::ReportError(const Status& error) {
  CHECK(!error.ok());
  if (die_immediately_on_error_) {
    LOG(FATAL) << "error building computation: " << error;
  }

  if (first_error_.ok()) {
    first_error_ = error;
    first_error_backtrace_.CreateCurrent(/*skip_count=*/1);
  }
  return XlaOp(this);
}

XlaOp XlaBuilder::ReportErrorOrReturn(const StatusOr<XlaOp>& op) {
  if (!first_error_.ok()) {
    return XlaOp(this);
  }
  if (!op.ok()) {
    return ReportError(op.status());
  }
  return op.ValueOrDie();
}

XlaOp XlaBuilder::ReportErrorOrReturn(
    const std::function<StatusOr<XlaOp>()>& op_creator) {
  return ReportErrorOrReturn(op_creator());
}

StatusOr<ProgramShape> XlaBuilder::GetProgramShape(int64_t root_id) const {
  TF_RETURN_IF_ERROR(first_error_);
  TF_ASSIGN_OR_RETURN(const HloInstructionProto* root_proto,
                      LookUpInstructionByHandle(root_id));

  ProgramShape program_shape;

  *program_shape.mutable_result() = Shape(root_proto->shape());

  // Check that the parameter numbers are continuous from 0, and add parameter
  // shapes and names to the program shape.
  const int64_t param_count = parameter_numbers_.size();
  for (int64_t i = 0; i < param_count; i++) {
    program_shape.add_parameters();
    program_shape.add_parameter_names();
  }
  for (const HloInstructionProto& instr : instructions_) {
    // Parameter number uniqueness is guaranteed in XlaBuilder::Parameter(). So
    // to verify continuity, we just need to verify that every parameter is in
    // the right range.
    if (instr.opcode() == HloOpcodeString(HloOpcode::kParameter)) {
      const int64_t index = instr.parameter_number();
      TF_RET_CHECK(index >= 0 && index < param_count)
          << "invalid parameter number: " << index;
      *program_shape.mutable_parameters(index) = Shape(instr.shape());
      *program_shape.mutable_parameter_names(index) = instr.name();
    }
  }
  return program_shape;
}

StatusOr<ProgramShape> XlaBuilder::GetProgramShape() const {
  TF_RET_CHECK(!instructions_.empty());
  return GetProgramShape(instructions_.back().id());
}

StatusOr<ProgramShape> XlaBuilder::GetProgramShape(XlaOp root) const {
  if (root.builder_ != this) {
    return InvalidArgument("Given root operation is not in this computation.");
  }
  return GetProgramShape(root.handle());
}

void XlaBuilder::IsConstantVisitor(const int64_t op_handle, int depth,
                                   absl::flat_hash_set<int64_t>* visited,
                                   bool* is_constant) const {
  if (visited->contains(op_handle) || !*is_constant) {
    return;
  }

  const HloInstructionProto& instr =
      *(LookUpInstructionByHandle(op_handle).ValueOrDie());
  HloInstructionProto to_print(instr);
  to_print.clear_shape();
  const HloOpcode opcode = StringToHloOpcode(instr.opcode()).ValueOrDie();
  const std::string indent =
      absl::StrJoin(std::vector<absl::string_view>(depth, "  "), "");
  if (VLOG_IS_ON(2)) {
    VLOG(2) << indent << "Visiting:";
    for (const auto& l : absl::StrSplit(to_print.DebugString(), '\n')) {
      VLOG(2) << indent << l;
    }
  }
  switch (opcode) {
    default:
      for (const int64_t operand_id : instr.operand_ids()) {
        IsConstantVisitor(operand_id, depth + 1, visited, is_constant);
      }
      // TODO(b/32495713): We aren't checking the called computations.
      break;

    case HloOpcode::kGetDimensionSize:
      // GetDimensionSize is always considered constant in XLA -- If a dynamic
      // dimension is presented, -1 is returned.
      break;
    // Non functional ops.
    case HloOpcode::kRng:
    case HloOpcode::kAllReduce:
    case HloOpcode::kReduceScatter:
      // TODO(b/33009255): Implement constant folding for cross replica sum.
    case HloOpcode::kInfeed:
    case HloOpcode::kOutfeed:
    case HloOpcode::kCall:
      // TODO(b/32495713): We aren't checking the to_apply computation itself,
      // so we conservatively say that computations containing the Call op
      // cannot be constant.  We cannot set is_functional=false in other similar
      // cases since we're already relying on IsConstant to return true.
    case HloOpcode::kCustomCall:
      if (instr.custom_call_target() == "SetBound") {
        // Set bound is considered constant -- the bound is used as the value.
        break;
      }
      ABSL_FALLTHROUGH_INTENDED;
    case HloOpcode::kWhile:
      // TODO(b/32495713): We aren't checking the condition and body
      // computations themselves.
    case HloOpcode::kScatter:
      // TODO(b/32495713): We aren't checking the embedded computation in
      // Scatter.
    case HloOpcode::kSend:
    case HloOpcode::kRecv:
    case HloOpcode::kParameter:
      *is_constant = false;
      break;
    case HloOpcode::kGetTupleElement: {
      const HloInstructionProto& operand_instr =
          *(LookUpInstructionByHandle(instr.operand_ids(0)).ValueOrDie());
      if (HloOpcodeString(HloOpcode::kTuple) == operand_instr.opcode()) {
        IsConstantVisitor(operand_instr.operand_ids(instr.tuple_index()),
                          depth + 1, visited, is_constant);
      } else {
        for (const int64_t operand_id : instr.operand_ids()) {
          IsConstantVisitor(operand_id, depth + 1, visited, is_constant);
        }
      }
    }
  }
  if (VLOG_IS_ON(1) && !*is_constant) {
    VLOG(1) << indent << "Non-constant: ";
    for (const auto& l : absl::StrSplit(to_print.DebugString(), '\n')) {
      VLOG(1) << indent << l;
    }
  }
  visited->insert(op_handle);
}

Status XlaBuilder::SetDynamicBinding(int64_t dynamic_size_param_num,
                                     ShapeIndex dynamic_size_param_index,
                                     int64_t target_param_num,
                                     ShapeIndex target_param_index,
                                     int64_t target_dim_num) {
  bool param_exists = false;
  for (size_t index = 0; index < instructions_.size(); ++index) {
    HloInstructionProto& instr = instructions_[index];
    if (instr.opcode() == HloOpcodeString(HloOpcode::kParameter) &&
        instr.parameter_number() == target_param_num) {
      param_exists = true;
      Shape param_shape(instr.shape());
      Shape* param_shape_ptr = &param_shape;
      for (int64_t index : target_param_index) {
        param_shape_ptr = param_shape_ptr->mutable_tuple_shapes(index);
      }
      param_shape_ptr->set_dynamic_dimension(target_dim_num,
                                             /*is_dynamic=*/true);
      *instr.mutable_shape() = param_shape.ToProto();
      instruction_shapes_[index] =
          absl::make_unique<Shape>(std::move(param_shape));
    }
  }
  if (!param_exists) {
    return InvalidArgument(
        "Asked to mark parameter %lld as dynamic sized parameter, but the "
        "doesn't exists",
        target_param_num);
  }

  TF_RETURN_IF_ERROR(dynamic_parameter_binding_.Bind(
      DynamicParameterBinding::DynamicParameter{dynamic_size_param_num,
                                                dynamic_size_param_index},
      DynamicParameterBinding::DynamicDimension{
          target_param_num, target_param_index, target_dim_num}));
  return Status::OK();
}

Status XlaBuilder::SetInstructionFrontendAttribute(const XlaOp op,
                                                   std::string attribute,
                                                   std::string value) {
  TF_ASSIGN_OR_RETURN(auto instr_proto, LookUpMutableInstruction(op));
  auto* frontend_attributes = instr_proto->mutable_frontend_attributes();
  (*frontend_attributes->mutable_map())[attribute] = std::move(value);
  return Status::OK();
}

XlaComputation XlaBuilder::BuildAndNoteError() {
  DCHECK(parent_builder_ != nullptr);
  auto build_status = Build();
  if (!build_status.ok()) {
    parent_builder_->ReportError(
        AddStatus(build_status.status(), absl::StrCat("error from: ", name_)));
    return {};
  }
  return build_status.ConsumeValueOrDie();
}

Status XlaBuilder::GetCurrentStatus() const {
  if (!first_error_.ok()) {
    std::string backtrace;
    first_error_backtrace_.Dump(tensorflow::DebugWriteToString, &backtrace);
    return AppendStatus(first_error_, backtrace);
  }
  return Status::OK();
}

StatusOr<XlaComputation> XlaBuilder::Build(bool remove_dynamic_dimensions) {
  TF_RETURN_IF_ERROR(GetCurrentStatus());
  return Build(instructions_.back().id(), remove_dynamic_dimensions);
}

StatusOr<XlaComputation> XlaBuilder::Build(XlaOp root,
                                           bool remove_dynamic_dimensions) {
  if (root.builder_ != this) {
    return InvalidArgument("Given root operation is not in this computation.");
  }
  return Build(root.handle(), remove_dynamic_dimensions);
}

StatusOr<XlaComputation> XlaBuilder::Build(int64_t root_id,
                                           bool remove_dynamic_dimensions) {
  TF_RETURN_IF_ERROR(GetCurrentStatus());

  // TODO(b/121223198): XLA backend cannot handle dynamic dimensions yet, remove
  // all dynamic dimensions before building xla program until we have support in
  // the backend.
  if (remove_dynamic_dimensions) {
    std::function<void(Shape*)> remove_dynamic_dimension = [&](Shape* shape) {
      if (shape->tuple_shapes_size() != 0) {
        for (int i = 0; i < shape->tuple_shapes_size(); ++i) {
          remove_dynamic_dimension(shape->mutable_tuple_shapes(i));
        }
      }
      for (int64_t i = 0; i < shape->dimensions_size(); ++i) {
        shape->set_dynamic_dimension(i, false);
      }
    };
    for (size_t index = 0; index < instructions_.size(); ++index) {
      remove_dynamic_dimension(instruction_shapes_[index].get());
      *instructions_[index].mutable_shape() =
          instruction_shapes_[index]->ToProto();
    }
  }

  HloComputationProto entry;
  SetProtoIdAndName(&entry, name_, kNameSeparator, GetNextId());
  TF_ASSIGN_OR_RETURN(ProgramShape program_shape, GetProgramShape(root_id));
  *entry.mutable_program_shape() = program_shape.ToProto();
  entry.set_root_id(root_id);

  for (auto& instruction : instructions_) {
    // Ensures that the instruction names are unique among the whole graph.
    instruction.set_name(
        GetFullName(instruction.name(), kNameSeparator, instruction.id()));
    entry.add_instructions()->Swap(&instruction);
  }

  XlaComputation computation(entry.id());
  HloModuleProto* module = computation.mutable_proto();
  module->set_name(entry.name());
  module->set_id(entry.id());
  module->set_entry_computation_name(entry.name());
  module->set_entry_computation_id(entry.id());
  *module->mutable_host_program_shape() = entry.program_shape();
  for (auto& e : embedded_) {
    module->add_computations()->Swap(&e.second);
  }
  module->add_computations()->Swap(&entry);
  if (!input_output_aliases_.empty()) {
    TF_RETURN_IF_ERROR(
        PopulateInputOutputAlias(module, program_shape, input_output_aliases_));
  }
  *(module->mutable_dynamic_parameter_binding()) =
      dynamic_parameter_binding_.ToProto();

  // Clear data held by this builder.
  this->instructions_.clear();
  this->instruction_shapes_.clear();
  this->handle_to_index_.clear();
  this->embedded_.clear();
  this->parameter_numbers_.clear();

  return std::move(computation);
}

/* static */ Status XlaBuilder::PopulateInputOutputAlias(
    HloModuleProto* module, const ProgramShape& program_shape,
    const std::vector<InputOutputAlias>& input_output_aliases) {
  HloInputOutputAliasConfig config(program_shape.result());
  for (auto& alias : input_output_aliases) {
    // The HloInputOutputAliasConfig does not do parameter validation as it only
    // carries the result shape. Maybe it should be constructed with a
    // ProgramShape to allow full validation. We will still get an error when
    // trying to compile the HLO module, but would be better to have validation
    // at this stage.
    if (alias.param_number >= program_shape.parameters_size()) {
      return InvalidArgument("Invalid parameter number %ld (total %ld)",
                             alias.param_number,
                             program_shape.parameters_size());
    }
    const Shape& parameter_shape = program_shape.parameters(alias.param_number);
    if (!ShapeUtil::IndexIsValid(parameter_shape, alias.param_index)) {
      return InvalidArgument("Invalid parameter %ld index: %s",
                             alias.param_number,
                             alias.param_index.ToString().c_str());
    }
    TF_RETURN_IF_ERROR(config.SetUpAlias(alias.output_index, alias.param_number,
                                         alias.param_index, alias.kind));
  }
  *module->mutable_input_output_alias() = config.ToProto();
  return Status::OK();
}

StatusOr<XlaOp> XlaBuilder::InDimBroadcast(
    const Shape& shape, XlaOp operand,
    absl::Span<const int64_t> broadcast_dimensions) {
  TF_RETURN_IF_ERROR(first_error_);

  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  for (int64_t dim : broadcast_dimensions) {
    instr.add_dimensions(dim);
  }

  return AddInstruction(std::move(instr), HloOpcode::kBroadcast, {operand});
}

StatusOr<XlaOp> XlaBuilder::AddBroadcastSequence(const Shape& output_shape,
                                                 XlaOp operand) {
  TF_RETURN_IF_ERROR(first_error_);

  TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));

  CHECK(ShapeUtil::IsScalar(*operand_shape) ||
        operand_shape->rank() == output_shape.rank());
  Shape broadcast_shape =
      ShapeUtil::ChangeElementType(output_shape, operand_shape->element_type());

  // Do explicit broadcast for scalar.
  if (ShapeUtil::IsScalar(*operand_shape)) {
    return InDimBroadcast(broadcast_shape, operand, {});
  }

  // Do explicit broadcast for degenerate broadcast.
  std::vector<int64_t> broadcast_dimensions;
  std::vector<int64_t> reshaped_dimensions;
  for (int i = 0; i < operand_shape->rank(); i++) {
    if (operand_shape->dimensions(i) == output_shape.dimensions(i)) {
      broadcast_dimensions.push_back(i);
      reshaped_dimensions.push_back(operand_shape->dimensions(i));
    } else {
      TF_RET_CHECK(operand_shape->dimensions(i) == 1)
          << "An explicit broadcast sequence requires the broadcasted "
             "dimensions to be trivial; operand shape: "
          << *operand_shape << "; output_shape: " << output_shape;
    }
  }

  Shape reshaped_shape =
      ShapeUtil::MakeShape(operand_shape->element_type(), reshaped_dimensions);

  std::vector<std::pair<int64_t, int64_t>> unmodified_dims =
      ShapeUtil::DimensionsUnmodifiedByReshape(*operand_shape, reshaped_shape);

  for (auto& unmodified : unmodified_dims) {
    if (operand_shape->is_dynamic_dimension(unmodified.first)) {
      reshaped_shape.set_dynamic_dimension(unmodified.second, true);
    }
  }

  // Eliminate the size one dimensions.
  TF_ASSIGN_OR_RETURN(
      XlaOp reshaped_operand,
      ReshapeInternal(reshaped_shape, operand, /*inferred_dimension=*/-1));
  // Broadcast 'reshape' up to the larger size.
  return InDimBroadcast(broadcast_shape, reshaped_operand,
                        broadcast_dimensions);
}

XlaOp XlaBuilder::UnaryOp(HloOpcode unop, XlaOp operand) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(
        Shape shape, ShapeInference::InferUnaryOpShape(unop, *operand_shape));
    return AddOpWithShape(unop, shape, {operand});
  });
}

XlaOp XlaBuilder::BinaryOp(HloOpcode binop, XlaOp lhs, XlaOp rhs,
                           absl::Span<const int64_t> broadcast_dimensions,
                           absl::optional<ComparisonDirection> direction,
                           absl::optional<Comparison::Type> type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(lhs));
    TF_ASSIGN_OR_RETURN(const Shape* rhs_shape, GetShapePtr(rhs));
    TF_ASSIGN_OR_RETURN(
        Shape shape, ShapeInference::InferBinaryOpShape(
                         binop, *lhs_shape, *rhs_shape, broadcast_dimensions));

    const int64_t lhs_rank = lhs_shape->rank();
    const int64_t rhs_rank = rhs_shape->rank();

    XlaOp updated_lhs = lhs;
    XlaOp updated_rhs = rhs;
    if (!broadcast_dimensions.empty() && lhs_rank != rhs_rank) {
      const bool should_broadcast_lhs = lhs_rank < rhs_rank;
      XlaOp from = should_broadcast_lhs ? lhs : rhs;
      const Shape& from_shape = should_broadcast_lhs ? *lhs_shape : *rhs_shape;

      std::vector<int64_t> to_size;
      std::vector<bool> to_size_is_dynamic;
      const auto rank = shape.rank();
      to_size.reserve(rank);
      to_size_is_dynamic.reserve(rank);
      for (int i = 0; i < rank; i++) {
        to_size.push_back(shape.dimensions(i));
        to_size_is_dynamic.push_back(shape.is_dynamic_dimension(i));
      }
      for (int64_t from_dim = 0; from_dim < from_shape.rank(); from_dim++) {
        int64_t to_dim = broadcast_dimensions[from_dim];
        to_size[to_dim] = from_shape.dimensions(from_dim);
        to_size_is_dynamic[to_dim] = from_shape.is_dynamic_dimension(from_dim);
      }

      const Shape& broadcasted_shape = ShapeUtil::MakeShape(
          from_shape.element_type(), to_size, to_size_is_dynamic);
      TF_ASSIGN_OR_RETURN(
          XlaOp broadcasted_operand,
          InDimBroadcast(broadcasted_shape, from, broadcast_dimensions));

      updated_lhs = should_broadcast_lhs ? broadcasted_operand : lhs;
      updated_rhs = !should_broadcast_lhs ? broadcasted_operand : rhs;
    }

    TF_ASSIGN_OR_RETURN(const Shape* updated_lhs_shape,
                        GetShapePtr(updated_lhs));
    if (!ShapeUtil::SameDimensions(shape, *updated_lhs_shape)) {
      TF_ASSIGN_OR_RETURN(updated_lhs,
                          AddBroadcastSequence(shape, updated_lhs));
    }
    TF_ASSIGN_OR_RETURN(const Shape* updated_rhs_shape,
                        GetShapePtr(updated_rhs));
    if (!ShapeUtil::SameDimensions(shape, *updated_rhs_shape)) {
      TF_ASSIGN_OR_RETURN(updated_rhs,
                          AddBroadcastSequence(shape, updated_rhs));
    }

    if (binop == HloOpcode::kCompare) {
      if (!direction.has_value()) {
        return InvalidArgument(
            "kCompare expects a ComparisonDirection, but none provided.");
      }
      if (type == absl::nullopt) {
        return Compare(shape, updated_lhs, updated_rhs, *direction);
      } else {
        return Compare(shape, updated_lhs, updated_rhs, *direction, *type);
      }
    }

    if (direction.has_value()) {
      return InvalidArgument(
          "A comparison direction is provided for a non-compare opcode: %s.",
          HloOpcodeString(binop));
    }
    return BinaryOpNoBroadcast(binop, shape, updated_lhs, updated_rhs);
  });
}

XlaOp XlaBuilder::BinaryOpNoBroadcast(HloOpcode binop, const Shape& shape,
                                      XlaOp lhs, XlaOp rhs) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();
    return AddInstruction(std::move(instr), binop, {lhs, rhs});
  });
}

StatusOr<XlaOp> XlaBuilder::Compare(const Shape& shape, XlaOp lhs, XlaOp rhs,
                                    ComparisonDirection direction) {
  TF_ASSIGN_OR_RETURN(auto operand_shape, GetShape(lhs));
  return Compare(
      shape, lhs, rhs, direction,
      Comparison::DefaultComparisonType(operand_shape.element_type()));
}

StatusOr<XlaOp> XlaBuilder::Compare(const Shape& shape, XlaOp lhs, XlaOp rhs,
                                    ComparisonDirection direction,
                                    Comparison::Type type) {
  HloInstructionProto instr;
  instr.set_comparison_direction(ComparisonDirectionToString(direction));
  instr.set_comparison_type(ComparisonTypeToString(type));
  *instr.mutable_shape() = shape.ToProto();
  return AddInstruction(std::move(instr), HloOpcode::kCompare, {lhs, rhs});
}

XlaOp XlaBuilder::TernaryOp(HloOpcode triop, XlaOp lhs, XlaOp rhs, XlaOp ehs) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    XlaOp updated_lhs = lhs;
    XlaOp updated_rhs = rhs;
    XlaOp updated_ehs = ehs;
    // The client API supports implicit broadcast for kSelect and kClamp, but
    // XLA does not support implicit broadcast. Make implicit broadcast explicit
    // and update the operands.
    if (triop == HloOpcode::kSelect || triop == HloOpcode::kClamp) {
      TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(lhs));
      TF_ASSIGN_OR_RETURN(const Shape* rhs_shape, GetShapePtr(rhs));
      TF_ASSIGN_OR_RETURN(const Shape* ehs_shape, GetShapePtr(ehs));

      absl::optional<Shape> non_scalar_shape;
      for (const Shape* shape : {lhs_shape, rhs_shape, ehs_shape}) {
        if (shape->IsArray() && shape->rank() != 0) {
          if (non_scalar_shape.has_value()) {
            // TODO(jpienaar): The case where we need to compute the broadcasted
            // shape by considering multiple of the shapes is not implemented.
            // Consider reusing getBroadcastedType from mlir/Dialect/Traits.h.
            TF_RET_CHECK(non_scalar_shape.value().dimensions() ==
                         shape->dimensions())
                << "Unimplemented implicit broadcast.";
          } else {
            non_scalar_shape = *shape;
          }
        }
      }
      if (non_scalar_shape.has_value()) {
        if (ShapeUtil::IsScalar(*lhs_shape)) {
          TF_ASSIGN_OR_RETURN(updated_lhs,
                              AddBroadcastSequence(*non_scalar_shape, lhs));
        }
        if (ShapeUtil::IsScalar(*rhs_shape)) {
          TF_ASSIGN_OR_RETURN(updated_rhs,
                              AddBroadcastSequence(*non_scalar_shape, rhs));
        }
        if (ShapeUtil::IsScalar(*ehs_shape)) {
          TF_ASSIGN_OR_RETURN(updated_ehs,
                              AddBroadcastSequence(*non_scalar_shape, ehs));
        }
      }
    }

    TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(updated_lhs));
    TF_ASSIGN_OR_RETURN(const Shape* rhs_shape, GetShapePtr(updated_rhs));
    TF_ASSIGN_OR_RETURN(const Shape* ehs_shape, GetShapePtr(updated_ehs));
    StatusOr<const Shape> status_or_shape = ShapeInference::InferTernaryOpShape(
        triop, *lhs_shape, *rhs_shape, *ehs_shape);
    if (!status_or_shape.status().ok()) {
      return InvalidArgument(
          "%s Input scalar shapes may have been changed to non-scalar shapes.",
          status_or_shape.status().error_message());
    }

    return AddOpWithShape(triop, status_or_shape.ValueOrDie(),
                          {updated_lhs, updated_rhs, updated_ehs});
  });
}

XlaOp XlaBuilder::ConstantLiteral(const LiteralSlice& literal) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (literal.shape().IsArray() && literal.element_count() > 1 &&
        literal.IsAllFirst()) {
      Literal scalar = LiteralUtil::GetFirstScalarLiteral(literal);
      HloInstructionProto instr;
      *instr.mutable_shape() = scalar.shape().ToProto();
      *instr.mutable_literal() = scalar.ToProto();
      TF_ASSIGN_OR_RETURN(
          XlaOp scalar_op,
          AddInstruction(std::move(instr), HloOpcode::kConstant));
      return Broadcast(scalar_op, literal.shape().dimensions());
    } else {
      HloInstructionProto instr;
      *instr.mutable_shape() = literal.shape().ToProto();
      *instr.mutable_literal() = literal.ToProto();
      return AddInstruction(std::move(instr), HloOpcode::kConstant);
    }
  });
}

XlaOp XlaBuilder::Iota(const Shape& shape, int64_t iota_dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();
    instr.add_dimensions(iota_dimension);
    return AddInstruction(std::move(instr), HloOpcode::kIota);
  });
}

XlaOp XlaBuilder::Iota(PrimitiveType type, int64_t size) {
  return Iota(ShapeUtil::MakeShape(type, {size}), /*iota_dimension=*/0);
}

XlaOp XlaBuilder::Call(const XlaComputation& computation,
                       absl::Span<const XlaOp> operands) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    std::vector<const Shape*> operand_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& operand_shapes, GetOperandShapes(operands));
    absl::c_transform(operand_shapes, std::back_inserter(operand_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(const ProgramShape& called_program_shape,
                        computation.GetProgramShape());
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferCallShape(
                                         operand_shape_ptrs,
                                         /*to_apply=*/called_program_shape));
    *instr.mutable_shape() = shape.ToProto();

    AddCalledComputation(computation, &instr);

    return AddInstruction(std::move(instr), HloOpcode::kCall, operands);
  });
}

XlaOp XlaBuilder::Parameter(
    int64_t parameter_number, const Shape& shape, const std::string& name,
    const std::vector<bool>& replicated_at_leaf_buffers) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    if (!parameter_numbers_.insert(parameter_number).second) {
      return InvalidArgument("parameter %d already registered",
                             parameter_number);
    }
    instr.set_parameter_number(parameter_number);
    instr.set_name(name);
    *instr.mutable_shape() = shape.ToProto();
    if (!replicated_at_leaf_buffers.empty()) {
      auto replication = instr.mutable_parameter_replication();
      for (bool replicated : replicated_at_leaf_buffers) {
        replication->add_replicated_at_leaf_buffers(replicated);
      }
    }
    return AddInstruction(std::move(instr), HloOpcode::kParameter);
  });
}

XlaOp XlaBuilder::Broadcast(XlaOp operand,
                            absl::Span<const int64_t> broadcast_sizes) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(
        const Shape& shape,
        ShapeInference::InferBroadcastShape(*operand_shape, broadcast_sizes));

    // The client-level broadcast op just appends dimensions on the left (adds
    // lowest numbered dimensions). The HLO broadcast instruction is more
    // flexible and can add new dimensions anywhere. The instruction's
    // dimensions field maps operand dimensions to dimensions in the broadcast
    // output, so to append dimensions on the left the instruction's dimensions
    // should just be the n highest dimension numbers of the output shape where
    // n is the number of input dimensions.
    const int64_t operand_rank = operand_shape->rank();
    std::vector<int64_t> dimensions(operand_rank);
    for (int i = 0; i < operand_rank; ++i) {
      dimensions[i] = i + shape.rank() - operand_rank;
    }
    return InDimBroadcast(shape, operand, dimensions);
  });
}

XlaOp XlaBuilder::BroadcastInDim(
    XlaOp operand, const absl::Span<const int64_t> out_dim_size,
    const absl::Span<const int64_t> broadcast_dimensions) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    // Output shape, in the case of degenerate broadcast, the out_dim_size is
    // not necessarily the same as the dimension sizes of the output shape.
    TF_ASSIGN_OR_RETURN(auto output_shape,
                        ShapeUtil::MakeValidatedShape(
                            operand_shape->element_type(), out_dim_size));
    int64_t broadcast_rank = broadcast_dimensions.size();
    if (operand_shape->rank() != broadcast_rank) {
      return InvalidArgument(
          "Size of broadcast_dimensions has to match operand's rank; operand "
          "rank: %lld, size of broadcast_dimensions %u.",
          operand_shape->rank(), broadcast_dimensions.size());
    }
    for (int i = 0; i < broadcast_rank; i++) {
      const int64_t num_dims = out_dim_size.size();
      if (broadcast_dimensions[i] < 0 || broadcast_dimensions[i] > num_dims) {
        return InvalidArgument("Broadcast dimension %lld is out of bound",
                               broadcast_dimensions[i]);
      }
      output_shape.set_dynamic_dimension(
          broadcast_dimensions[i], operand_shape->is_dynamic_dimension(i));
    }

    TF_RETURN_IF_ERROR(ShapeInference::InferBroadcastShape(
                           *operand_shape, output_shape, broadcast_dimensions)
                           .status());
    std::vector<int64_t> in_dim_size(out_dim_size.begin(), out_dim_size.end());
    for (int i = 0; i < broadcast_rank; i++) {
      in_dim_size[broadcast_dimensions[i]] = operand_shape->dimensions(i);
    }
    const auto& in_dim_shape =
        ShapeUtil::MakeShape(operand_shape->element_type(), in_dim_size);
    TF_ASSIGN_OR_RETURN(
        XlaOp in_dim_broadcast,
        InDimBroadcast(in_dim_shape, operand, broadcast_dimensions));

    // If broadcast is not degenerate, return broadcasted result.
    if (ShapeUtil::Equal(in_dim_shape, output_shape)) {
      return in_dim_broadcast;
    }

    // Otherwise handle degenerate broadcast case.
    return AddBroadcastSequence(output_shape, in_dim_broadcast);
  });
}

StatusOr<XlaOp> XlaBuilder::ReshapeInternal(const Shape& shape, XlaOp operand,
                                            int64_t inferred_dimension) {
  TF_RETURN_IF_ERROR(first_error_);

  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  if (inferred_dimension != -1) {
    instr.add_dimensions(inferred_dimension);
  }
  return AddInstruction(std::move(instr), HloOpcode::kReshape, {operand});
}

XlaOp XlaBuilder::Slice(XlaOp operand, absl::Span<const int64_t> start_indices,
                        absl::Span<const int64_t> limit_indices,
                        absl::Span<const int64_t> strides) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferSliceShape(
                                         *operand_shape, start_indices,
                                         limit_indices, strides));
    return SliceInternal(shape, operand, start_indices, limit_indices, strides);
  });
}

StatusOr<XlaOp> XlaBuilder::SliceInternal(
    const Shape& shape, XlaOp operand, absl::Span<const int64_t> start_indices,
    absl::Span<const int64_t> limit_indices,
    absl::Span<const int64_t> strides) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  for (int i = 0, end = start_indices.size(); i < end; i++) {
    auto* slice_config = instr.add_slice_dimensions();
    slice_config->set_start(start_indices[i]);
    slice_config->set_limit(limit_indices[i]);
    slice_config->set_stride(strides[i]);
  }
  return AddInstruction(std::move(instr), HloOpcode::kSlice, {operand});
}

XlaOp XlaBuilder::SliceInDim(XlaOp operand, int64_t start_index,
                             int64_t limit_index, int64_t stride,
                             int64_t dimno) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(operand));
    std::vector<int64_t> starts(shape->rank(), 0);
    std::vector<int64_t> limits(shape->dimensions().begin(),
                                shape->dimensions().end());
    std::vector<int64_t> strides(shape->rank(), 1);
    starts[dimno] = start_index;
    limits[dimno] = limit_index;
    strides[dimno] = stride;
    return Slice(operand, starts, limits, strides);
  });
}

XlaOp XlaBuilder::DynamicSlice(XlaOp operand,
                               absl::Span<const XlaOp> start_indices,
                               absl::Span<const int64_t> slice_sizes) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    std::vector<const Shape*> start_indices_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& start_indices_shapes,
                        GetOperandShapes(start_indices));
    absl::c_transform(start_indices_shapes,
                      std::back_inserter(start_indices_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(Shape shape,
                        ShapeInference::InferDynamicSliceShape(
                            *operand_shape, start_indices_shapes, slice_sizes));
    return DynamicSliceInternal(shape, operand, start_indices, slice_sizes);
  });
}

StatusOr<XlaOp> XlaBuilder::DynamicSliceInternal(
    const Shape& shape, XlaOp operand, absl::Span<const XlaOp> start_indices,
    absl::Span<const int64_t> slice_sizes) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();

  for (int64_t size : slice_sizes) {
    instr.add_dynamic_slice_sizes(size);
  }

  std::vector<XlaOp> operands = {operand};
  operands.insert(operands.end(), start_indices.begin(), start_indices.end());
  return AddInstruction(std::move(instr), HloOpcode::kDynamicSlice, operands);
}

XlaOp XlaBuilder::DynamicUpdateSlice(XlaOp operand, XlaOp update,
                                     absl::Span<const XlaOp> start_indices) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(const Shape* update_shape, GetShapePtr(update));
    std::vector<const Shape*> start_indices_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& start_indices_shapes,
                        GetOperandShapes(start_indices));
    absl::c_transform(start_indices_shapes,
                      std::back_inserter(start_indices_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(
        Shape shape, ShapeInference::InferDynamicUpdateSliceShape(
                         *operand_shape, *update_shape, start_indices_shapes));
    return DynamicUpdateSliceInternal(shape, operand, update, start_indices);
  });
}

StatusOr<XlaOp> XlaBuilder::DynamicUpdateSliceInternal(
    const Shape& shape, XlaOp operand, XlaOp update,
    absl::Span<const XlaOp> start_indices) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();

  std::vector<XlaOp> operands = {operand, update};
  operands.insert(operands.end(), start_indices.begin(), start_indices.end());
  return AddInstruction(std::move(instr), HloOpcode::kDynamicUpdateSlice,
                        operands);
}

XlaOp XlaBuilder::ConcatInDim(absl::Span<const XlaOp> operands,
                              int64_t dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    std::vector<const Shape*> operand_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& operand_shapes, GetOperandShapes(operands));
    absl::c_transform(operand_shapes, std::back_inserter(operand_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferConcatOpShape(
                                         operand_shape_ptrs, dimension));
    return ConcatInDimInternal(shape, operands, dimension);
  });
}

StatusOr<XlaOp> XlaBuilder::ConcatInDimInternal(
    const Shape& shape, absl::Span<const XlaOp> operands, int64_t dimension) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();

  instr.add_dimensions(dimension);

  return AddInstruction(std::move(instr), HloOpcode::kConcatenate, operands);
}

XlaOp XlaBuilder::Pad(XlaOp operand, XlaOp padding_value,
                      const PaddingConfig& padding_config) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(const Shape* padding_value_shape,
                        GetShapePtr(padding_value));
    TF_ASSIGN_OR_RETURN(
        Shape shape, ShapeInference::InferPadShape(
                         *operand_shape, *padding_value_shape, padding_config));
    return PadInternal(shape, operand, padding_value, padding_config);
  });
}

XlaOp XlaBuilder::PadInDim(XlaOp operand, XlaOp padding_value, int64_t dimno,
                           int64_t pad_lo, int64_t pad_hi) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(operand));
    PaddingConfig padding_config = MakeNoPaddingConfig(shape->rank());
    auto* dims = padding_config.mutable_dimensions(dimno);
    dims->set_edge_padding_low(pad_lo);
    dims->set_edge_padding_high(pad_hi);
    return Pad(operand, padding_value, padding_config);
  });
}

StatusOr<XlaOp> XlaBuilder::PadInternal(const Shape& shape, XlaOp operand,
                                        XlaOp padding_value,
                                        const PaddingConfig& padding_config) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  *instr.mutable_padding_config() = padding_config;
  return AddInstruction(std::move(instr), HloOpcode::kPad,
                        {operand, padding_value});
}

XlaOp XlaBuilder::Reshape(XlaOp operand, absl::Span<const int64_t> dimensions,
                          absl::Span<const int64_t> new_sizes,
                          int64_t inferred_dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(const Shape shape, ShapeInference::InferReshapeShape(
                                               *operand_shape, dimensions,
                                               new_sizes, inferred_dimension));
    XlaOp transposed = IsIdentityPermutation(dimensions)
                           ? operand
                           : Transpose(operand, dimensions);
    return ReshapeInternal(shape, transposed, inferred_dimension);
  });
}

XlaOp XlaBuilder::Reshape(XlaOp operand, absl::Span<const int64_t> new_sizes,
                          int64_t inferred_dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(operand));
    std::vector<int64_t> dimensions(shape->dimensions_size());
    std::iota(dimensions.begin(), dimensions.end(), 0);
    return Reshape(operand, dimensions, new_sizes, inferred_dimension);
  });
}

XlaOp XlaBuilder::Reshape(const Shape& shape, XlaOp operand,
                          int64_t inferred_dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    return ReshapeInternal(shape, operand, inferred_dimension);
  });
}

XlaOp XlaBuilder::DynamicReshape(XlaOp operand,
                                 absl::Span<const XlaOp> dim_sizes,
                                 absl::Span<const int64_t> new_size_bounds,
                                 const std::vector<bool>& dims_are_dynamic) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    std::vector<const Shape*> dim_size_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& dim_size_shapes,
                        GetOperandShapes(dim_sizes));

    absl::c_transform(dim_size_shapes, std::back_inserter(dim_size_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(const Shape shape,
                        ShapeInference::InferDynamicReshapeShape(
                            *operand_shape, dim_size_shape_ptrs,
                            new_size_bounds, dims_are_dynamic));
    TF_RETURN_IF_ERROR(first_error_);
    std::vector<XlaOp> operands;
    operands.reserve(1 + dim_sizes.size());
    operands.push_back(operand);
    for (const XlaOp& dim_size : dim_sizes) {
      operands.push_back(dim_size);
    }
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();
    return AddInstruction(std::move(instr), HloOpcode::kDynamicReshape,
                          operands);
  });
}

XlaOp XlaBuilder::Collapse(XlaOp operand,
                           absl::Span<const int64_t> dimensions) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (dimensions.size() <= 1) {
      // Not collapsing anything, trivially we can return the operand versus
      // enqueueing a trivial reshape.
      return operand;
    }

    // Out-of-order collapse is not supported.
    // Checks that the collapsed dimensions are in order and consecutive.
    for (absl::Span<const int64_t>::size_type i = 1; i < dimensions.size();
         ++i) {
      if (dimensions[i] - 1 != dimensions[i - 1]) {
        return InvalidArgument(
            "Collapsed dimensions are not in consecutive order.");
      }
    }

    // Create a new sizes vector from the old shape, replacing the collapsed
    // dimensions by the product of their sizes.
    TF_ASSIGN_OR_RETURN(const Shape* original_shape, GetShapePtr(operand));

    VLOG(3) << "original shape: " << ShapeUtil::HumanString(*original_shape);
    VLOG(3) << "dims to collapse: " << absl::StrJoin(dimensions, ",");

    std::vector<int64_t> new_sizes;
    for (int i = 0; i < original_shape->rank(); ++i) {
      if (i <= dimensions.front() || i > dimensions.back()) {
        new_sizes.push_back(original_shape->dimensions(i));
      } else {
        new_sizes.back() *= original_shape->dimensions(i);
      }
    }

    VLOG(3) << "new sizes: [" << absl::StrJoin(new_sizes, ",") << "]";

    return Reshape(operand, new_sizes);
  });
}

// Dummy pass-through computation returning it's parameter of shape `shape`.
static StatusOr<XlaComputation> PassthroughComputation(
    const xla::Shape& shape) {
  XlaBuilder builder("dummy");
  XlaOp out = Parameter(&builder, 0, shape, "p");
  return builder.Build(out);
}

XlaOp XlaBuilder::Select(XlaOp pred, XlaOp on_true, XlaOp on_false) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* true_shape, GetShapePtr(on_true));
    TF_ASSIGN_OR_RETURN(const Shape* false_shape, GetShapePtr(on_false));
    TF_RET_CHECK(true_shape->IsTuple() == false_shape->IsTuple());
    if (true_shape->IsTuple()) {
      TF_ASSIGN_OR_RETURN(XlaComputation passthrough_true,
                          PassthroughComputation(*true_shape));
      TF_ASSIGN_OR_RETURN(XlaComputation passthrough_false,
                          PassthroughComputation(*false_shape));
      return Conditional(pred, on_true, passthrough_true, on_false,
                         passthrough_false);
    }
    return TernaryOp(HloOpcode::kSelect, pred, on_true, on_false);
  });
}

XlaOp XlaBuilder::Tuple(absl::Span<const XlaOp> elements) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    std::vector<const Shape*> operand_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& operand_shapes, GetOperandShapes(elements));
    absl::c_transform(operand_shapes, std::back_inserter(operand_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(const Shape shape,
                        ShapeInference::InferVariadicOpShape(
                            HloOpcode::kTuple, operand_shape_ptrs));
    return TupleInternal(shape, elements);
  });
}

StatusOr<XlaOp> XlaBuilder::TupleInternal(const Shape& shape,
                                          absl::Span<const XlaOp> elements) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  return AddInstruction(std::move(instr), HloOpcode::kTuple, elements);
}

XlaOp XlaBuilder::GetTupleElement(XlaOp tuple_data, int64_t index) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* tuple_shape, GetShapePtr(tuple_data));
    if (!tuple_shape->IsTuple()) {
      return InvalidArgument(
          "Operand to GetTupleElement() is not a tuple; got %s",
          ShapeUtil::HumanString(*tuple_shape));
    }
    if (index < 0 || index >= ShapeUtil::TupleElementCount(*tuple_shape)) {
      return InvalidArgument(
          "GetTupleElement() index (%d) out of range for tuple shape %s", index,
          ShapeUtil::HumanString(*tuple_shape));
    }
    return GetTupleElementInternal(
        ShapeUtil::GetTupleElementShape(*tuple_shape, index), tuple_data,
        index);
  });
}

StatusOr<XlaOp> XlaBuilder::GetTupleElementInternal(const Shape& shape,
                                                    XlaOp tuple_data,
                                                    int64_t index) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  instr.set_tuple_index(index);
  return AddInstruction(std::move(instr), HloOpcode::kGetTupleElement,
                        {tuple_data});
}

XlaOp XlaBuilder::Dot(XlaOp lhs, XlaOp rhs,
                      const PrecisionConfig* precision_config,
                      absl::optional<PrimitiveType> preferred_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(lhs));

    DotDimensionNumbers dimension_numbers;
    dimension_numbers.add_lhs_contracting_dimensions(
        lhs_shape->dimensions_size() == 1 ? 0 : 1);
    dimension_numbers.add_rhs_contracting_dimensions(0);
    return DotGeneral(lhs, rhs, dimension_numbers, precision_config,
                      preferred_element_type);
  });
}

XlaOp XlaBuilder::DotGeneral(
    XlaOp lhs, XlaOp rhs, const DotDimensionNumbers& dimension_numbers,
    const PrecisionConfig* precision_config,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(lhs));
    TF_ASSIGN_OR_RETURN(const Shape* rhs_shape, GetShapePtr(rhs));
    TF_ASSIGN_OR_RETURN(
        Shape shape,
        ShapeInference::InferDotOpShape(
            *lhs_shape, *rhs_shape, dimension_numbers, preferred_element_type));
    return DotGeneralInternal(shape, lhs, rhs, dimension_numbers,
                              precision_config);
  });
}

StatusOr<XlaOp> XlaBuilder::DotGeneralInternal(
    const Shape& shape, XlaOp lhs, XlaOp rhs,
    const DotDimensionNumbers& dimension_numbers,
    const PrecisionConfig* precision_config) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  *instr.mutable_dot_dimension_numbers() = dimension_numbers;
  if (precision_config != nullptr) {
    *instr.mutable_precision_config() = *precision_config;
  }
  return AddInstruction(std::move(instr), HloOpcode::kDot, {lhs, rhs});
}

Status XlaBuilder::VerifyConvolution(
    const Shape& lhs_shape, const Shape& rhs_shape,
    const ConvolutionDimensionNumbers& dimension_numbers) const {
  if (lhs_shape.rank() != rhs_shape.rank()) {
    return InvalidArgument(
        "Convolution arguments must have same number of "
        "dimensions. Got: %s and %s",
        ShapeUtil::HumanString(lhs_shape), ShapeUtil::HumanString(rhs_shape));
  }
  int num_dims = lhs_shape.rank();
  if (num_dims < 2) {
    return InvalidArgument(
        "Convolution expects argument arrays with >= 3 dimensions. "
        "Got: %s and %s",
        ShapeUtil::HumanString(lhs_shape), ShapeUtil::HumanString(rhs_shape));
  }
  int num_spatial_dims = num_dims - 2;

  const auto check_spatial_dimensions = [&](absl::string_view field_name,
                                            absl::Span<const int64_t> numbers) {
    if (numbers.size() != num_spatial_dims) {
      return InvalidArgument("Expected %d elements for %s, but got %d.",
                             num_spatial_dims, field_name, numbers.size());
    }
    for (int i = 0; i < numbers.size(); ++i) {
      if (numbers[i] < 0 || numbers[i] >= num_dims) {
        return InvalidArgument("Convolution %s[%d] is out of bounds: %d",
                               field_name, i, numbers[i]);
      }
    }
    return Status::OK();
  };
  TF_RETURN_IF_ERROR(
      check_spatial_dimensions("input_spatial_dimensions",
                               dimension_numbers.input_spatial_dimensions()));
  TF_RETURN_IF_ERROR(
      check_spatial_dimensions("kernel_spatial_dimensions",
                               dimension_numbers.kernel_spatial_dimensions()));
  return check_spatial_dimensions(
      "output_spatial_dimensions",
      dimension_numbers.output_spatial_dimensions());
}

XlaOp XlaBuilder::Conv(XlaOp lhs, XlaOp rhs,
                       absl::Span<const int64_t> window_strides,
                       Padding padding, int64_t feature_group_count,
                       int64_t batch_group_count,
                       const PrecisionConfig* precision_config,
                       absl::optional<PrimitiveType> preferred_element_type) {
  return ConvWithGeneralDimensions(
      lhs, rhs, window_strides, padding,
      CreateDefaultConvDimensionNumbers(window_strides.size()),
      feature_group_count, batch_group_count, precision_config,
      preferred_element_type);
}

XlaOp XlaBuilder::ConvWithGeneralPadding(
    XlaOp lhs, XlaOp rhs, absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ConvGeneral(lhs, rhs, window_strides, padding,
                     CreateDefaultConvDimensionNumbers(window_strides.size()),
                     feature_group_count, batch_group_count, precision_config,
                     preferred_element_type);
}

XlaOp XlaBuilder::ConvWithGeneralDimensions(
    XlaOp lhs, XlaOp rhs, absl::Span<const int64_t> window_strides,
    Padding padding, const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(lhs));
    TF_ASSIGN_OR_RETURN(const Shape* rhs_shape, GetShapePtr(rhs));

    TF_RETURN_IF_ERROR(
        VerifyConvolution(*lhs_shape, *rhs_shape, dimension_numbers));

    std::vector<int64_t> base_area_dimensions(
        dimension_numbers.input_spatial_dimensions_size());
    for (std::vector<int64_t>::size_type i = 0; i < base_area_dimensions.size();
         ++i) {
      base_area_dimensions[i] =
          lhs_shape->dimensions(dimension_numbers.input_spatial_dimensions(i));
    }

    std::vector<int64_t> window_dimensions(
        dimension_numbers.kernel_spatial_dimensions_size());
    for (std::vector<int64_t>::size_type i = 0; i < window_dimensions.size();
         ++i) {
      window_dimensions[i] =
          rhs_shape->dimensions(dimension_numbers.kernel_spatial_dimensions(i));
    }

    return ConvGeneral(lhs, rhs, window_strides,
                       MakePadding(base_area_dimensions, window_dimensions,
                                   window_strides, padding),
                       dimension_numbers, feature_group_count,
                       batch_group_count, precision_config,
                       preferred_element_type);
  });
}

XlaOp XlaBuilder::ConvGeneral(
    XlaOp lhs, XlaOp rhs, absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ConvGeneralDilated(lhs, rhs, window_strides, padding, {}, {},
                            dimension_numbers, feature_group_count,
                            batch_group_count, precision_config,
                            preferred_element_type);
}

XlaOp XlaBuilder::ConvGeneralDilated(
    XlaOp lhs, XlaOp rhs, absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(lhs));
    TF_ASSIGN_OR_RETURN(const Shape* rhs_shape, GetShapePtr(rhs));
    TF_RETURN_IF_ERROR(
        VerifyConvolution(*lhs_shape, *rhs_shape, dimension_numbers));

    std::vector<int64_t> window_dimensions(
        dimension_numbers.kernel_spatial_dimensions_size());
    for (std::vector<int64_t>::size_type i = 0; i < window_dimensions.size();
         ++i) {
      window_dimensions[i] =
          rhs_shape->dimensions(dimension_numbers.kernel_spatial_dimensions(i));
    }

    TF_ASSIGN_OR_RETURN(Window window,
                        ShapeInference::InferWindowFromDimensions(
                            window_dimensions, window_strides, padding,
                            lhs_dilation, rhs_dilation));
    TF_ASSIGN_OR_RETURN(
        Shape shape,
        ShapeInference::InferConvolveShape(
            *lhs_shape, *rhs_shape, feature_group_count, batch_group_count,
            window, dimension_numbers, preferred_element_type));
    return ConvGeneralDilatedInternal(shape, lhs, rhs, window, window_strides,
                                      padding, lhs_dilation, rhs_dilation,
                                      dimension_numbers, feature_group_count,
                                      batch_group_count, precision_config);
  });
}

StatusOr<HloInstructionProto> XlaBuilder::DynamicConvInstruction(
    XlaOp lhs, XlaOp rhs, absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config, PaddingType padding_type,
    absl::optional<PrimitiveType> preferred_element_type) {
  TF_ASSIGN_OR_RETURN(const Shape* lhs_shape, GetShapePtr(lhs));
  TF_ASSIGN_OR_RETURN(const Shape* rhs_shape, GetShapePtr(rhs));
  std::vector<int64_t> window_dimensions(
      dimension_numbers.kernel_spatial_dimensions_size());
  for (std::vector<int64_t>::size_type i = 0; i < window_dimensions.size();
       ++i) {
    window_dimensions[i] =
        rhs_shape->dimensions(dimension_numbers.kernel_spatial_dimensions(i));
  }

  TF_ASSIGN_OR_RETURN(Window window, ShapeInference::InferWindowFromDimensions(
                                         window_dimensions, window_strides,
                                         padding, lhs_dilation, rhs_dilation));
  TF_ASSIGN_OR_RETURN(
      Shape shape,
      ShapeInference::InferConvolveShape(
          *lhs_shape, *rhs_shape, feature_group_count, batch_group_count,
          window, dimension_numbers, preferred_element_type));

  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();

  *instr.mutable_window() = window;
  *instr.mutable_convolution_dimension_numbers() = dimension_numbers;
  instr.set_feature_group_count(feature_group_count);
  instr.set_batch_group_count(batch_group_count);
  instr.set_padding_type(padding_type);

  if (precision_config != nullptr) {
    *instr.mutable_precision_config() = *precision_config;
  }
  return std::move(instr);
}

XlaOp XlaBuilder::DynamicConvInputGrad(
    XlaOp input_sizes, XlaOp lhs, XlaOp rhs,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config, PaddingType padding_type,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(
        HloInstructionProto instr,
        DynamicConvInstruction(
            lhs, rhs, window_strides, padding, lhs_dilation, rhs_dilation,
            dimension_numbers, feature_group_count, batch_group_count,
            precision_config, padding_type, preferred_element_type));

    instr.set_custom_call_target("DynamicConvolutionInputGrad");

    return AddInstruction(std::move(instr), HloOpcode::kCustomCall,
                          {input_sizes, lhs, rhs});
  });
}

XlaOp XlaBuilder::DynamicConvKernelGrad(
    XlaOp activations, XlaOp gradients,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config, PaddingType padding_type,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(
        HloInstructionProto instr,
        DynamicConvInstruction(activations, gradients, window_strides, padding,
                               lhs_dilation, rhs_dilation, dimension_numbers,
                               feature_group_count, batch_group_count,
                               precision_config, padding_type,
                               preferred_element_type));

    instr.set_custom_call_target("DynamicConvolutionKernelGrad");
    // The gradient of kernel has kernel shape and shouldn't have any dynamic
    // sizes.
    instr.mutable_shape()->clear_is_dynamic_dimension();
    return AddInstruction(std::move(instr), HloOpcode::kCustomCall,
                          {activations, gradients});
  });
}

XlaOp XlaBuilder::DynamicConvForward(
    XlaOp lhs, XlaOp rhs, absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config, PaddingType padding_type,
    absl::optional<PrimitiveType> preferred_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(
        HloInstructionProto instr,
        DynamicConvInstruction(
            lhs, rhs, window_strides, padding, lhs_dilation, rhs_dilation,
            dimension_numbers, feature_group_count, batch_group_count,
            precision_config, padding_type, preferred_element_type));
    instr.set_custom_call_target("DynamicConvolutionForward");

    return AddInstruction(std::move(instr), HloOpcode::kCustomCall, {lhs, rhs});
  });
}

StatusOr<XlaOp> XlaBuilder::ConvGeneralDilatedInternal(
    const Shape& shape, XlaOp lhs, XlaOp rhs, const Window& window,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();

  *instr.mutable_window() = window;
  *instr.mutable_convolution_dimension_numbers() = dimension_numbers;
  instr.set_feature_group_count(feature_group_count);
  instr.set_batch_group_count(batch_group_count);

  if (precision_config != nullptr) {
    *instr.mutable_precision_config() = *precision_config;
  }

  return AddInstruction(std::move(instr), HloOpcode::kConvolution, {lhs, rhs});
}

XlaOp XlaBuilder::Fft(XlaOp operand, const FftType fft_type,
                      const absl::Span<const int64_t> fft_length) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferFftShape(
                                         *operand_shape, fft_type, fft_length));
    return FftInternal(shape, operand, fft_type, fft_length);
  });
}

StatusOr<XlaOp> XlaBuilder::FftInternal(
    const Shape& shape, XlaOp operand, const FftType fft_type,
    const absl::Span<const int64_t> fft_length) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  instr.set_fft_type(fft_type);
  for (int64_t i : fft_length) {
    instr.add_fft_length(i);
  }

  return AddInstruction(std::move(instr), HloOpcode::kFft, {operand});
}

StatusOr<XlaOp> XlaBuilder::TriangularSolveInternal(
    const Shape& shape, XlaOp a, XlaOp b, TriangularSolveOptions options) {
  HloInstructionProto instr;
  *instr.mutable_triangular_solve_options() = std::move(options);
  *instr.mutable_shape() = shape.ToProto();

  return AddInstruction(std::move(instr), HloOpcode::kTriangularSolve, {a, b});
}

StatusOr<XlaOp> XlaBuilder::CholeskyInternal(const Shape& shape, XlaOp a,
                                             bool lower) {
  HloInstructionProto instr;
  xla::CholeskyOptions& options = *instr.mutable_cholesky_options();
  options.set_lower(lower);
  *instr.mutable_shape() = shape.ToProto();

  return AddInstruction(std::move(instr), HloOpcode::kCholesky, {a});
}

XlaOp XlaBuilder::Infeed(const Shape& shape, const std::string& config) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    if (!LayoutUtil::HasLayout(shape)) {
      return InvalidArgument("Given shape to Infeed must have a layout");
    }
    const Shape infeed_instruction_shape =
        ShapeUtil::MakeTupleShape({shape, ShapeUtil::MakeTokenShape()});
    *instr.mutable_shape() = infeed_instruction_shape.ToProto();
    instr.set_infeed_config(config);

    if (shape.IsArray() && sharding() &&
        sharding()->type() == OpSharding::OTHER) {
      // TODO(b/110793772): Support tiled array-shaped infeeds.
      return InvalidArgument(
          "Tiled sharding is not yet supported for array-shaped infeeds");
    }

    if (sharding() && sharding()->type() == OpSharding::REPLICATED) {
      return InvalidArgument(
          "Replicated sharding is not yet supported for infeeds");
    }

    // Infeed takes a single token operand. Generate the token to pass to the
    // infeed.
    XlaOp token;
    auto make_token = [&]() {
      HloInstructionProto token_instr;
      *token_instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
      return AddInstruction(std::move(token_instr), HloOpcode::kAfterAll, {});
    };
    if (sharding()) {
      // Arbitrarily assign token to device 0.
      OpSharding sharding = sharding_builder::AssignDevice(0);
      XlaScopedShardingAssignment scoped_sharding(this, sharding);
      TF_ASSIGN_OR_RETURN(token, make_token());
    } else {
      TF_ASSIGN_OR_RETURN(token, make_token());
    }

    // The sharding is set by the client according to the data tuple shape.
    // However, the shape of the infeed instruction is a tuple containing the
    // data and a token. For tuple sharding type, the sharding must be changed
    // to accommodate the token.
    XlaOp infeed;
    if (sharding() && sharding()->type() == OpSharding::TUPLE) {
      // TODO(b/80000000): Remove this when clients have been updated to handle
      // tokens.
      OpSharding infeed_instruction_sharding = *sharding();
      // Arbitrarily assign the token to device 0.
      *infeed_instruction_sharding.add_tuple_shardings() =
          sharding_builder::AssignDevice(0);
      XlaScopedShardingAssignment scoped_sharding(this,
                                                  infeed_instruction_sharding);
      TF_ASSIGN_OR_RETURN(infeed, AddInstruction(std::move(instr),
                                                 HloOpcode::kInfeed, {token}));
    } else {
      TF_ASSIGN_OR_RETURN(infeed, AddInstruction(std::move(instr),
                                                 HloOpcode::kInfeed, {token}));
    }

    // The infeed instruction produces a tuple of the infed data and a token
    // type. Return XLA op containing the data.
    // TODO(b/80000000): Remove this when clients have been updated to handle
    // tokens.
    HloInstructionProto infeed_data;
    *infeed_data.mutable_shape() = shape.ToProto();
    infeed_data.set_tuple_index(0);
    return AddInstruction(std::move(infeed_data), HloOpcode::kGetTupleElement,
                          {infeed});
  });
}

XlaOp XlaBuilder::InfeedWithToken(XlaOp token, const Shape& shape,
                                  const std::string& config) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (!LayoutUtil::HasLayout(shape)) {
      return InvalidArgument("Given shape to Infeed must have a layout");
    }
    const Shape infeed_instruction_shape =
        ShapeUtil::MakeTupleShape({shape, ShapeUtil::MakeTokenShape()});

    if (shape.IsArray() && sharding() &&
        sharding()->type() == OpSharding::OTHER) {
      // TODO(b/110793772): Support tiled array-shaped infeeds.
      return InvalidArgument(
          "Tiled sharding is not yet supported for array-shaped infeeds");
    }

    if (sharding() && sharding()->type() == OpSharding::REPLICATED) {
      return InvalidArgument(
          "Replicated sharding is not yet supported for infeeds");
    }
    return InfeedWithTokenInternal(infeed_instruction_shape, token, config);
  });
}

StatusOr<XlaOp> XlaBuilder::InfeedWithTokenInternal(
    const Shape& infeed_instruction_shape, XlaOp token,
    const std::string& config) {
  HloInstructionProto instr;
  *instr.mutable_shape() = infeed_instruction_shape.ToProto();
  instr.set_infeed_config(config);
  return AddInstruction(std::move(instr), HloOpcode::kInfeed, {token});
}

void XlaBuilder::Outfeed(XlaOp operand, const Shape& shape_with_layout,
                         const std::string& outfeed_config) {
  ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;

    *instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();

    // Check and set outfeed shape.
    if (!LayoutUtil::HasLayout(shape_with_layout)) {
      return InvalidArgument("Given shape to Outfeed must have a layout");
    }
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    if (!ShapeUtil::Compatible(*operand_shape, shape_with_layout)) {
      return InvalidArgument(
          "Outfeed shape %s must be compatible with operand shape %s",
          ShapeUtil::HumanStringWithLayout(shape_with_layout),
          ShapeUtil::HumanStringWithLayout(*operand_shape));
    }
    *instr.mutable_outfeed_shape() = shape_with_layout.ToProto();

    instr.set_outfeed_config(outfeed_config);

    // Outfeed takes a token as its second operand. Generate the token to pass
    // to the outfeed.
    HloInstructionProto token_instr;
    *token_instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
    TF_ASSIGN_OR_RETURN(XlaOp token, AddInstruction(std::move(token_instr),
                                                    HloOpcode::kAfterAll, {}));

    TF_RETURN_IF_ERROR(
        AddInstruction(std::move(instr), HloOpcode::kOutfeed, {operand, token})
            .status());

    // The outfeed instruction produces a token. However, existing users expect
    // a nil shape (empty tuple). This should only be relevant if the outfeed is
    // the root of a computation.
    // TODO(b/80000000): Remove this when clients have been updated to handle
    // tokens.
    HloInstructionProto tuple_instr;
    *tuple_instr.mutable_shape() = ShapeUtil::MakeNil().ToProto();

    // The dummy tuple should have no sharding.
    {
      XlaScopedShardingAssignment scoped_sharding(this, OpSharding());
      TF_ASSIGN_OR_RETURN(
          XlaOp empty_tuple,
          AddInstruction(std::move(tuple_instr), HloOpcode::kTuple, {}));
      return empty_tuple;
    }
  });
}

XlaOp XlaBuilder::OutfeedWithToken(XlaOp operand, XlaOp token,
                                   const Shape& shape_with_layout,
                                   const std::string& outfeed_config) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    // Check and set outfeed shape.
    if (!LayoutUtil::HasLayout(shape_with_layout)) {
      return InvalidArgument("Given shape to Outfeed must have a layout");
    }
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    if (!ShapeUtil::Compatible(*operand_shape, shape_with_layout)) {
      return InvalidArgument(
          "Outfeed shape %s must be compatible with operand shape %s",
          ShapeUtil::HumanStringWithLayout(shape_with_layout),
          ShapeUtil::HumanStringWithLayout(*operand_shape));
    }
    return OutfeedWithTokenInternal(operand, token, shape_with_layout,
                                    outfeed_config);
  });
}

StatusOr<XlaOp> XlaBuilder::OutfeedWithTokenInternal(
    XlaOp operand, XlaOp token, const Shape& shape_with_layout,
    const std::string& outfeed_config) {
  HloInstructionProto instr;
  *instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
  *instr.mutable_outfeed_shape() = shape_with_layout.ToProto();
  instr.set_outfeed_config(outfeed_config);
  return AddInstruction(std::move(instr), HloOpcode::kOutfeed,
                        {operand, token});
}

XlaOp XlaBuilder::CreateToken() {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
    return AddInstruction(std::move(instr), HloOpcode::kAfterAll);
  });
}

XlaOp XlaBuilder::AfterAll(absl::Span<const XlaOp> tokens) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (tokens.empty()) {
      return InvalidArgument("AfterAll requires at least one operand");
    }
    for (int i = 0, end = tokens.size(); i < end; ++i) {
      XlaOp operand = tokens[i];
      TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
      if (!operand_shape->IsToken()) {
        return InvalidArgument(
            "All operands to AfterAll must be tokens; operand %d has shape %s",
            i, ShapeUtil::HumanString(*operand_shape));
      }
    }
    HloInstructionProto instr;
    *instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
    return AddInstruction(std::move(instr), HloOpcode::kAfterAll, tokens);
  });
}

XlaOp XlaBuilder::CustomCall(
    const std::string& call_target_name, absl::Span<const XlaOp> operands,
    const Shape& shape, const std::string& opaque,
    absl::optional<absl::Span<const Shape>> operand_shapes_with_layout,
    bool has_side_effect,
    absl::Span<const std::pair<ShapeIndex, std::pair<int64_t, ShapeIndex>>>
        output_operand_aliasing,
    const Literal* literal, absl::optional<Window> window,
    absl::optional<ConvolutionDimensionNumbers> dnums,
    CustomCallSchedule schedule, CustomCallApiVersion api_version) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (absl::StartsWith(call_target_name, "$")) {
      return InvalidArgument(
          "Invalid custom_call_target \"%s\": Call targets that start with '$' "
          "are reserved for internal use.",
          call_target_name);
    }
    if (operand_shapes_with_layout.has_value()) {
      if (!LayoutUtil::HasLayout(shape)) {
        return InvalidArgument(
            "Result shape must have layout for custom call with constrained "
            "layout.");
      }
      if (operands.size() != operand_shapes_with_layout->size()) {
        return InvalidArgument(
            "Must specify a shape with layout for each operand for custom call "
            "with constrained layout; given %d shapes, expected %d",
            operand_shapes_with_layout->size(), operands.size());
      }
      int64_t operand_num = 0;
      for (const Shape& operand_shape : *operand_shapes_with_layout) {
        if (!LayoutUtil::HasLayout(operand_shape)) {
          return InvalidArgument(
              "No layout specified for operand %d for custom call with "
              "constrained layout.",
              operand_num);
        }
        ++operand_num;
      }
    }
    return CustomCallInternal(call_target_name, operands, shape, opaque,
                              operand_shapes_with_layout, has_side_effect,
                              output_operand_aliasing, literal, window, dnums,
                              schedule, api_version);
  });
}

StatusOr<XlaOp> XlaBuilder::CustomCallInternal(
    const std::string& call_target_name, absl::Span<const XlaOp> operands,
    const Shape& shape, const std::string& opaque,
    absl::optional<absl::Span<const Shape>> operand_shapes_with_layout,
    bool has_side_effect,
    absl::Span<const std::pair<ShapeIndex, std::pair<int64_t, ShapeIndex>>>
        output_operand_aliasing,
    const Literal* literal, absl::optional<Window> window,
    absl::optional<ConvolutionDimensionNumbers> dnums,
    CustomCallSchedule schedule, CustomCallApiVersion api_version) {
  HloInstructionProto instr;
  // Bit of a hack: cudnn conv custom-calls are created through this API. Give
  // them a user-friendly name. (This has no effect on correctness, it's just
  // cosmetic.)
  if (call_target_name == "__cudnn$convForward") {
    instr.set_name("cudnn-conv");
  } else if (call_target_name == "__cudnn$convBackwardInput") {
    instr.set_name("cudnn-conv-bw-input");
  } else if (call_target_name == "__cudnn$convBackwardFilter") {
    instr.set_name("cudnn-conv-bw-filter");
  } else if (call_target_name == "__cudnn$convBiasActivationForward") {
    instr.set_name("cudnn-conv-bias-activation");
  }
  *instr.mutable_shape() = shape.ToProto();
  instr.set_custom_call_target(call_target_name);
  instr.set_backend_config(opaque);
  if (operand_shapes_with_layout.has_value()) {
    instr.set_constrain_layout(true);
    for (const Shape& operand_shape : *operand_shapes_with_layout) {
      *instr.add_operand_shapes_with_layout() = operand_shape.ToProto();
    }
  }
  if (literal != nullptr) {
    *instr.mutable_literal() = literal->ToProto();
  }
  instr.set_custom_call_has_side_effect(has_side_effect);
  for (const auto& pair : output_operand_aliasing) {
    auto aliasing = instr.add_custom_call_output_operand_aliasing();
    aliasing->set_operand_index(pair.second.first);
    for (int64_t index : pair.second.second) {
      aliasing->add_operand_shape_index(index);
    }
    for (int64_t index : pair.first) {
      aliasing->add_output_shape_index(index);
    }
  }
  if (window.has_value()) {
    *instr.mutable_window() = *window;
  }
  if (dnums.has_value()) {
    *instr.mutable_convolution_dimension_numbers() = *dnums;
  }
  instr.set_custom_call_schedule(schedule);
  instr.set_custom_call_api_version(api_version);
  return AddInstruction(std::move(instr), HloOpcode::kCustomCall, operands);
}

XlaOp XlaBuilder::CustomCall(
    const std::string& call_target_name, absl::Span<const XlaOp> operands,
    const XlaComputation& computation, const Shape& shape,
    const std::string& opaque,
    absl::optional<absl::Span<const Shape>> operand_shapes_with_layout,
    bool has_side_effect,
    absl::Span<const std::pair<ShapeIndex, std::pair<int64_t, ShapeIndex>>>
        output_operand_aliasing,
    const Literal* literal, CustomCallSchedule schedule,
    CustomCallApiVersion api_version) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    if (absl::StartsWith(call_target_name, "$")) {
      return InvalidArgument(
          "Invalid custom_call_target \"%s\": Call targets that start with '$' "
          "are reserved for internal use.",
          call_target_name);
    }
    *instr.mutable_shape() = shape.ToProto();
    instr.set_custom_call_target(call_target_name);
    instr.set_backend_config(opaque);
    if (literal != nullptr) {
      *instr.mutable_literal() = literal->ToProto();
    }
    if (operand_shapes_with_layout.has_value()) {
      if (!LayoutUtil::HasLayout(shape)) {
        return InvalidArgument(
            "Result shape must have layout for custom call with constrained "
            "layout.");
      }
      if (operands.size() != operand_shapes_with_layout->size()) {
        return InvalidArgument(
            "Must specify a shape with layout for each operand for custom call "
            "with constrained layout; given %d shapes, expected %d",
            operand_shapes_with_layout->size(), operands.size());
      }
      instr.set_constrain_layout(true);
      int64_t operand_num = 0;
      for (const Shape& operand_shape : *operand_shapes_with_layout) {
        if (!LayoutUtil::HasLayout(operand_shape)) {
          return InvalidArgument(
              "No layout specified for operand %d for custom call with "
              "constrained layout.",
              operand_num);
        }
        *instr.add_operand_shapes_with_layout() = operand_shape.ToProto();
        ++operand_num;
      }
    }
    AddCalledComputation(computation, &instr);
    for (const auto& pair : output_operand_aliasing) {
      auto aliasing = instr.add_custom_call_output_operand_aliasing();
      aliasing->set_operand_index(pair.second.first);
      for (int64_t index : pair.second.second) {
        aliasing->add_operand_shape_index(index);
      }
      for (int64_t index : pair.first) {
        aliasing->add_output_shape_index(index);
      }
    }
    instr.set_custom_call_schedule(schedule);
    instr.set_custom_call_api_version(api_version);
    return AddInstruction(std::move(instr), HloOpcode::kCustomCall, operands);
  });
}

XlaOp XlaBuilder::OptimizationBarrier(XlaOp operand) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    Shape shape = *operand_shape;
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();
    return AddInstruction(std::move(instr), HloOpcode::kOptimizationBarrier,
                          {operand});
  });
}

XlaOp XlaBuilder::Transpose(XlaOp operand,
                            absl::Span<const int64_t> permutation) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferTransposeShape(
                                         *operand_shape, permutation));
    return TransposeInternal(shape, operand, permutation);
  });
}

StatusOr<XlaOp> XlaBuilder::TransposeInternal(
    const Shape& shape, XlaOp operand, absl::Span<const int64_t> permutation) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  for (int64_t dim : permutation) {
    instr.add_dimensions(dim);
  }
  return AddInstruction(std::move(instr), HloOpcode::kTranspose, {operand});
}

XlaOp XlaBuilder::Rev(XlaOp operand, absl::Span<const int64_t> dimensions) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferReverseShape(
                                         *operand_shape, dimensions));
    return RevInternal(shape, operand, dimensions);
  });
}

StatusOr<XlaOp> XlaBuilder::RevInternal(const Shape& shape, XlaOp operand,
                                        absl::Span<const int64_t> dimensions) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  for (int64_t dim : dimensions) {
    instr.add_dimensions(dim);
  }
  return AddInstruction(std::move(instr), HloOpcode::kReverse, {operand});
}

XlaOp XlaBuilder::Sort(absl::Span<const XlaOp> operands,
                       const XlaComputation& comparator, int64_t dimension,
                       bool is_stable) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    std::vector<const Shape*> operand_shape_ptrs;
    TF_ASSIGN_OR_RETURN(std::vector<Shape> operand_shapes,
                        GetOperandShapes(operands));
    absl::c_transform(operand_shapes, std::back_inserter(operand_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferVariadicOpShape(
                                         HloOpcode::kSort, operand_shape_ptrs));
    return SortInternal(shape, operands, comparator, dimension, is_stable);
  });
}

StatusOr<XlaOp> XlaBuilder::SortInternal(const Shape& shape,
                                         absl::Span<const XlaOp> operands,
                                         const XlaComputation& comparator,
                                         int64_t dimension, bool is_stable) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  instr.set_is_stable(is_stable);
  if (dimension == -1) {
    TF_ASSIGN_OR_RETURN(const Shape* keys_shape, GetShapePtr(operands[0]));
    dimension = keys_shape->rank() - 1;
  }
  instr.add_dimensions(dimension);
  AddCalledComputation(comparator, &instr);
  return AddInstruction(std::move(instr), HloOpcode::kSort, operands);
}

XlaOp XlaBuilder::ConvertElementType(XlaOp operand,
                                     PrimitiveType new_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferConvertShape(
                                         *operand_shape, new_element_type));
    return AddOpWithShape(HloOpcode::kConvert, shape, {operand});
  });
}

XlaOp XlaBuilder::BitcastConvertType(XlaOp operand,
                                     PrimitiveType new_element_type) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferBitcastConvertShape(
                                         *operand_shape, new_element_type));
    return BitcastConvertTypeInternal(shape, operand);
  });
}

StatusOr<XlaOp> XlaBuilder::BitcastConvertTypeInternal(const Shape& shape,
                                                       XlaOp operand) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  return AddInstruction(std::move(instr), HloOpcode::kBitcastConvert,
                        {operand});
}

XlaOp XlaBuilder::Clamp(XlaOp min, XlaOp operand, XlaOp max) {
  return TernaryOp(HloOpcode::kClamp, min, operand, max);
}

XlaOp XlaBuilder::Map(absl::Span<const XlaOp> operands,
                      const XlaComputation& computation,
                      absl::Span<const int64_t> dimensions,
                      absl::Span<const XlaOp> static_operands) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (!static_operands.empty()) {
      return Unimplemented("static_operands is not supported in Map");
    }

    HloInstructionProto instr;
    std::vector<const Shape*> operand_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& operand_shapes, GetOperandShapes(operands));
    absl::c_transform(operand_shapes, std::back_inserter(operand_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(const ProgramShape& called_program_shape,
                        computation.GetProgramShape());
    TF_ASSIGN_OR_RETURN(
        Shape shape, ShapeInference::InferMapShape(
                         operand_shape_ptrs, called_program_shape, dimensions));
    *instr.mutable_shape() = shape.ToProto();

    Shape output_shape(instr.shape());
    const int64_t output_rank = output_shape.rank();
    AddCalledComputation(computation, &instr);
    std::vector<XlaOp> new_operands(operands.begin(), operands.end());
    for (XlaOp& new_operand : new_operands) {
      TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(new_operand));
      const int64_t rank = shape->rank();
      if (rank != output_rank) {
        TF_ASSIGN_OR_RETURN(new_operand,
                            InDimBroadcast(output_shape, new_operand, {}));
        TF_ASSIGN_OR_RETURN(shape, GetShapePtr(new_operand));
      }
      if (!ShapeUtil::SameDimensions(output_shape, *shape)) {
        TF_ASSIGN_OR_RETURN(new_operand,
                            AddBroadcastSequence(output_shape, new_operand));
      }
    }

    return AddInstruction(std::move(instr), HloOpcode::kMap, new_operands);
  });
}

XlaOp XlaBuilder::RngOp(RandomDistribution distribution,
                        absl::Span<const XlaOp> parameters,
                        const Shape& shape) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    // Check the number of parameters per RNG distribution.
    switch (distribution) {
      case RandomDistribution::RNG_NORMAL:
      case RandomDistribution::RNG_UNIFORM:
        if (parameters.size() != 2) {
          return InvalidArgument(
              "RNG distribution (%s) expects 2 parameters, but got %ld",
              RandomDistribution_Name(distribution), parameters.size());
        }
        break;
      default:
        LOG(FATAL) << "unhandled distribution " << distribution;
    }

    TF_RETURN_IF_ERROR(ShapeUtil::ValidateShapeWithOptionalLayout(shape));
    return RngOpInternal(distribution, parameters, shape);
  });
}

StatusOr<XlaOp> XlaBuilder::RngOpInternal(RandomDistribution distribution,
                                          absl::Span<const XlaOp> parameters,
                                          const Shape& shape) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  instr.set_distribution(distribution);

  return AddInstruction(std::move(instr), HloOpcode::kRng, parameters);
}

XlaOp XlaBuilder::RngNormal(XlaOp mu, XlaOp sigma, const Shape& shape) {
  return RngOp(RandomDistribution::RNG_NORMAL, {mu, sigma}, shape);
}

XlaOp XlaBuilder::RngUniform(XlaOp a, XlaOp b, const Shape& shape) {
  return RngOp(RandomDistribution::RNG_UNIFORM, {a, b}, shape);
}

XlaOp XlaBuilder::RngBitGenerator(RandomAlgorithm algorithm,
                                  XlaOp initial_state, const Shape& shape) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_RETURN_IF_ERROR(ShapeUtil::ValidateShapeWithOptionalLayout(shape));
    TF_ASSIGN_OR_RETURN(Shape state_shape, GetShape(initial_state));
    Shape output_shape = shape;
    switch (output_shape.element_type()) {
      case PrimitiveType::F32:
      case PrimitiveType::S32:
      case PrimitiveType::U32:
        output_shape.set_element_type(PrimitiveType::U32);
        break;
      case PrimitiveType::F64:
      case PrimitiveType::S64:
      case PrimitiveType::U64:
        output_shape.set_element_type(PrimitiveType::U64);
        break;
      default:
        return InvalidArgument("Unsupported shape for RngBitGenerator: %s",
                               PrimitiveType_Name(output_shape.element_type()));
    }
    return RngBitGeneratorInternal(
        ShapeUtil::MakeTupleShapeWithPtrs({&state_shape, &output_shape}),
        algorithm, initial_state);
  });
}

StatusOr<XlaOp> XlaBuilder::RngBitGeneratorInternal(
    const Shape& full_result_shape, RandomAlgorithm algorithm,
    XlaOp initial_state) {
  HloInstructionProto instr;
  *instr.mutable_shape() = full_result_shape.ToProto();
  instr.set_rng_algorithm(algorithm);
  return AddInstruction(std::move(instr), HloOpcode::kRngBitGenerator,
                        {initial_state});
}

XlaOp XlaBuilder::While(const XlaComputation& condition,
                        const XlaComputation& body, XlaOp init) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    // Infer shape.
    TF_ASSIGN_OR_RETURN(const auto& body_program_shape, body.GetProgramShape());
    TF_ASSIGN_OR_RETURN(const auto& condition_program_shape,
                        condition.GetProgramShape());
    TF_ASSIGN_OR_RETURN(const Shape* init_shape, GetShapePtr(init));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferWhileShape(
                                         condition_program_shape,
                                         body_program_shape, *init_shape));
    return WhileInternal(shape, condition, body, init);
  });
}

StatusOr<XlaOp> XlaBuilder::WhileInternal(const Shape& shape,
                                          const XlaComputation& condition,
                                          const XlaComputation& body,
                                          XlaOp init) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  // Body comes before condition computation in the vector.
  AddCalledComputation(body, &instr);
  AddCalledComputation(condition, &instr);
  return AddInstruction(std::move(instr), HloOpcode::kWhile, {init});
}

XlaOp XlaBuilder::Gather(XlaOp input, XlaOp start_indices,
                         const GatherDimensionNumbers& dimension_numbers,
                         absl::Span<const int64_t> slice_sizes,
                         bool indices_are_sorted) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* input_shape, GetShapePtr(input));
    TF_ASSIGN_OR_RETURN(const Shape* start_indices_shape,
                        GetShapePtr(start_indices));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferGatherShape(
                                         *input_shape, *start_indices_shape,
                                         dimension_numbers, slice_sizes));
    return GatherInternal(shape, input, start_indices, dimension_numbers,
                          slice_sizes, indices_are_sorted);
  });
}

StatusOr<XlaOp> XlaBuilder::GatherInternal(
    const Shape& shape, XlaOp input, XlaOp start_indices,
    const GatherDimensionNumbers& dimension_numbers,
    absl::Span<const int64_t> slice_sizes, bool indices_are_sorted) {
  HloInstructionProto instr;
  instr.set_indices_are_sorted(indices_are_sorted);
  *instr.mutable_shape() = shape.ToProto();
  *instr.mutable_gather_dimension_numbers() = dimension_numbers;
  for (int64_t bound : slice_sizes) {
    instr.add_gather_slice_sizes(bound);
  }

  return AddInstruction(std::move(instr), HloOpcode::kGather,
                        {input, start_indices});
}

XlaOp XlaBuilder::Scatter(XlaOp input, XlaOp scatter_indices, XlaOp updates,
                          const XlaComputation& update_computation,
                          const ScatterDimensionNumbers& dimension_numbers,
                          bool indices_are_sorted, bool unique_indices) {
  return Scatter(absl::MakeConstSpan(&input, 1), scatter_indices,
                 absl::MakeConstSpan(&updates, 1), update_computation,
                 dimension_numbers, indices_are_sorted, unique_indices);
}

XlaOp XlaBuilder::Scatter(absl::Span<const XlaOp> inputs, XlaOp scatter_indices,
                          absl::Span<const XlaOp> updates,
                          const XlaComputation& update_computation,
                          const ScatterDimensionNumbers& dimension_numbers,
                          bool indices_are_sorted, bool unique_indices) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (inputs.empty()) {
      return InvalidArgument("Scatter inputs cannot be empty.");
    }
    if (inputs.size() != updates.size()) {
      return InvalidArgument(
          "Scatter should have same number of inputs and updates: %d vs %d.",
          inputs.size(), updates.size());
    }
    absl::InlinedVector<const Shape*, 3> operand_shapes;
    operand_shapes.reserve(inputs.size() + 1 + updates.size());
    for (const XlaOp& input : inputs) {
      TF_ASSIGN_OR_RETURN(const Shape* input_shape, GetShapePtr(input));
      operand_shapes.push_back(input_shape);
    }
    TF_ASSIGN_OR_RETURN(const Shape* scatter_indices_shape,
                        GetShapePtr(scatter_indices));
    operand_shapes.push_back(scatter_indices_shape);
    for (const XlaOp& update : updates) {
      TF_ASSIGN_OR_RETURN(const Shape* update_shape, GetShapePtr(update));
      operand_shapes.push_back(update_shape);
    }
    TF_ASSIGN_OR_RETURN(const ProgramShape& to_apply_shape,
                        update_computation.GetProgramShape());
    TF_ASSIGN_OR_RETURN(Shape shape,
                        ShapeInference::InferScatterShape(
                            operand_shapes, to_apply_shape, dimension_numbers));
    return ScatterInternal(shape, inputs, scatter_indices, updates,
                           update_computation, dimension_numbers,
                           indices_are_sorted, unique_indices);
  });
}

StatusOr<XlaOp> XlaBuilder::ScatterInternal(
    const Shape& shape, absl::Span<const XlaOp> inputs, XlaOp scatter_indices,
    absl::Span<const XlaOp> updates, const XlaComputation& update_computation,
    const ScatterDimensionNumbers& dimension_numbers, bool indices_are_sorted,
    bool unique_indices) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    instr.set_indices_are_sorted(indices_are_sorted);
    instr.set_unique_indices(unique_indices);
    *instr.mutable_shape() = shape.ToProto();
    *instr.mutable_scatter_dimension_numbers() = dimension_numbers;

    AddCalledComputation(update_computation, &instr);
    absl::InlinedVector<XlaOp, 3> operands;
    operands.reserve(inputs.size() + 1 + updates.size());
    absl::c_copy(inputs, std::back_inserter(operands));
    operands.push_back(scatter_indices);
    absl::c_copy(updates, std::back_inserter(operands));
    return AddInstruction(std::move(instr), HloOpcode::kScatter, operands);
  });
}

XlaOp XlaBuilder::Conditional(XlaOp predicate, XlaOp true_operand,
                              const XlaComputation& true_computation,
                              XlaOp false_operand,
                              const XlaComputation& false_computation) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const xla::Shape* shape, GetShapePtr(predicate));

    if (!ShapeUtil::IsScalar(*shape) || shape->element_type() != PRED) {
      return InvalidArgument(
          "Argument to predicated-Conditional is not a scalar of PRED type "
          "(%s).",
          ShapeUtil::HumanString(*shape));
    }
    // The index of true_computation must be 0 and that of false computation
    // must be 1.
    return ConditionalImpl(predicate, {&true_computation, &false_computation},
                           {true_operand, false_operand});
  });
}

XlaOp XlaBuilder::Conditional(
    XlaOp branch_index,
    absl::Span<const XlaComputation* const> branch_computations,
    absl::Span<const XlaOp> branch_operands) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const xla::Shape* shape, GetShapePtr(branch_index));

    if (!ShapeUtil::IsScalar(*shape) || shape->element_type() != S32) {
      return InvalidArgument(
          "Argument to indexed-Conditional is not a scalar of S32 type (%s).",
          ShapeUtil::HumanString(*shape));
    }
    return ConditionalImpl(branch_index, branch_computations, branch_operands);
  });
}

XlaOp XlaBuilder::ConditionalImpl(
    XlaOp branch_index,
    absl::Span<const XlaComputation* const> branch_computations,
    absl::Span<const XlaOp> branch_operands) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;

    TF_ASSIGN_OR_RETURN(const Shape* branch_index_shape,
                        GetShapePtr(branch_index));
    std::vector<Shape> branch_operand_shapes(branch_operands.size());
    std::vector<ProgramShape> branch_computation_shapes(
        branch_computations.size());
    for (int j = 0, end = branch_operands.size(); j < end; ++j) {
      TF_ASSIGN_OR_RETURN(branch_operand_shapes[j],
                          GetShape(branch_operands[j]));
      TF_ASSIGN_OR_RETURN(branch_computation_shapes[j],
                          branch_computations[j]->GetProgramShape());
    }
    TF_ASSIGN_OR_RETURN(const Shape shape,
                        ShapeInference::InferConditionalShape(
                            *branch_index_shape, branch_computation_shapes,
                            branch_operand_shapes));
    *instr.mutable_shape() = shape.ToProto();

    for (const XlaComputation* branch_computation : branch_computations) {
      AddCalledComputation(*branch_computation, &instr);
    }

    std::vector<XlaOp> operands(1, branch_index);
    for (const XlaOp branch_operand : branch_operands) {
      operands.emplace_back(branch_operand);
    }
    return AddInstruction(std::move(instr), HloOpcode::kConditional,
                          absl::MakeSpan(operands));
  });
}

Status XlaBuilder::CheckOpBuilder(XlaOp op) const {
  if (this != op.builder()) {
    return InvalidArgument(
        "XlaOp with handle %d is built by builder '%s', but is trying to use "
        "it in builder '%s'",
        op.handle(), op.builder()->name(), name());
  }
  return Status::OK();
}

XlaOp XlaBuilder::Reduce(XlaOp operand, XlaOp init_value,
                         const XlaComputation& computation,
                         absl::Span<const int64_t> dimensions_to_reduce) {
  return Reduce(absl::Span<const XlaOp>({operand}),
                absl::Span<const XlaOp>({init_value}), computation,
                dimensions_to_reduce);
}

XlaOp XlaBuilder::Reduce(absl::Span<const XlaOp> operands,
                         absl::Span<const XlaOp> init_values,
                         const XlaComputation& computation,
                         absl::Span<const int64_t> dimensions_to_reduce) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const ProgramShape& called_program_shape,
                        computation.GetProgramShape());

    std::vector<XlaOp> all_operands;
    all_operands.insert(all_operands.end(), operands.begin(), operands.end());
    all_operands.insert(all_operands.end(), init_values.begin(),
                        init_values.end());

    std::vector<const Shape*> operand_shape_ptrs;
    TF_ASSIGN_OR_RETURN(const auto& operand_shapes,
                        GetOperandShapes(all_operands));
    absl::c_transform(operand_shapes, std::back_inserter(operand_shape_ptrs),
                      [](const Shape& shape) { return &shape; });

    TF_ASSIGN_OR_RETURN(
        Shape shape,
        ShapeInference::InferReduceShape(
            operand_shape_ptrs, dimensions_to_reduce, called_program_shape));
    return ReduceInternal(shape, all_operands, computation,
                          dimensions_to_reduce);
  });
}

StatusOr<XlaOp> XlaBuilder::ReduceInternal(
    const Shape& shape, absl::Span<const XlaOp> all_operands,
    const XlaComputation& computation,
    absl::Span<const int64_t> dimensions_to_reduce) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = shape.ToProto();

    for (int64_t dim : dimensions_to_reduce) {
      instr.add_dimensions(dim);
    }

    AddCalledComputation(computation, &instr);
    return AddInstruction(std::move(instr), HloOpcode::kReduce, all_operands);
  });
}

XlaOp XlaBuilder::ReduceAll(XlaOp operand, XlaOp init_value,
                            const XlaComputation& computation) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    std::vector<int64_t> all_dimnos(operand_shape->rank());
    std::iota(all_dimnos.begin(), all_dimnos.end(), 0);
    return Reduce(operand, init_value, computation, all_dimnos);
  });
}

XlaOp XlaBuilder::ReduceWindow(XlaOp operand, XlaOp init_value,
                               const XlaComputation& computation,
                               absl::Span<const int64_t> window_dimensions,
                               absl::Span<const int64_t> window_strides,
                               Padding padding) {
  return ReduceWindow(absl::MakeSpan(&operand, 1),
                      absl::MakeSpan(&init_value, 1), computation,
                      window_dimensions, window_strides, padding);
}

XlaOp XlaBuilder::ReduceWindow(absl::Span<const XlaOp> operands,
                               absl::Span<const XlaOp> init_values,
                               const XlaComputation& computation,
                               absl::Span<const int64_t> window_dimensions,
                               absl::Span<const int64_t> window_strides,
                               Padding padding) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    const Shape* operand_shape = nullptr;
    for (const auto& operand : operands) {
      TF_ASSIGN_OR_RETURN(operand_shape, GetShapePtr(operand));
      TF_RETURN_IF_ERROR(ValidatePaddingValues(
          operand_shape->dimensions(), window_dimensions, window_strides));
    }
    CHECK(operand_shape != nullptr);
    std::vector<std::pair<int64_t, int64_t>> padding_values =
        MakePadding(operand_shape->dimensions(), window_dimensions,
                    window_strides, padding);
    TF_ASSIGN_OR_RETURN(auto window,
                        ShapeInference::InferWindowFromDimensions(
                            window_dimensions, window_strides, padding_values,
                            /*lhs_dilation=*/{},
                            /*rhs_dilation=*/{}));
    PaddingType padding_type = PADDING_INVALID;
    for (int64_t i = 0; i < operand_shape->rank(); ++i) {
      if (operand_shape->is_dynamic_dimension(i) &&
          !window_util::IsTrivialWindowDimension(window.dimensions(i)) &&
          padding == Padding::kSame) {
        // SAME padding can create dynamic padding sizes. The padding size
        // need to be rewritten by dynamic padder using HloInstructions. We
        // create a CustomCall to handle this.
        padding_type = PADDING_SAME;
      }
    }
    if (padding_type == PADDING_SAME) {
      TF_ASSIGN_OR_RETURN(
          HloInstructionProto instr,
          ReduceWindowInternal(operands, init_values, computation,
                               window_dimensions, window_strides, {}, {},
                               padding_values));
      instr.set_custom_call_target("DynamicReduceWindowSamePadding");
      std::vector<XlaOp> args;
      args.insert(args.end(), operands.begin(), operands.end());
      args.insert(args.end(), init_values.begin(), init_values.end());
      return AddInstruction(std::move(instr), HloOpcode::kCustomCall, args);
    }
    return ReduceWindowWithGeneralPadding(
        operands, init_values, computation, window_dimensions, window_strides,
        /*base_dilations=*/{}, /*window_dilations=*/{}, padding_values);
  });
}

XlaOp XlaBuilder::ReduceWindowWithGeneralPadding(
    absl::Span<const XlaOp> operands, absl::Span<const XlaOp> init_values,
    const XlaComputation& computation,
    absl::Span<const int64_t> window_dimensions,
    absl::Span<const int64_t> window_strides,
    absl::Span<const int64_t> base_dilations,
    absl::Span<const int64_t> window_dilations,
    absl::Span<const std::pair<int64_t, int64_t>> padding) {
  std::vector<const Shape*> operand_shapes, init_shapes;
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (operands.size() == 1) {
      const auto& operand = operands[0];
      const auto& init_value = init_values[0];
      TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
      operand_shapes.push_back(operand_shape);
      TF_ASSIGN_OR_RETURN(const Shape* init_shape, GetShapePtr(init_value));
      init_shapes.push_back(init_shape);

      TF_ASSIGN_OR_RETURN(const ProgramShape& to_apply_shape,
                          computation.GetProgramShape());
      TF_ASSIGN_OR_RETURN(auto window,
                          ShapeInference::InferWindowFromDimensions(
                              window_dimensions, window_strides, padding,
                              /*lhs_dilation=*/base_dilations,
                              /*rhs_dilation=*/window_dilations));
      TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferReduceWindowShape(
                                           absl::MakeSpan(operand_shapes),
                                           absl::MakeSpan(init_shapes), window,
                                           to_apply_shape));
      return ReduceWindowInternal(shape, operands[0], init_values[0],
                                  computation, window);
    }

    TF_ASSIGN_OR_RETURN(
        HloInstructionProto instr,
        ReduceWindowInternal(operands, init_values, computation,
                             window_dimensions, window_strides, base_dilations,
                             window_dilations, padding));
    std::vector<XlaOp> args;
    args.insert(args.end(), operands.begin(), operands.end());
    args.insert(args.end(), init_values.begin(), init_values.end());
    return AddInstruction(std::move(instr), HloOpcode::kReduceWindow, args);
  });
}

StatusOr<HloInstructionProto> XlaBuilder::ReduceWindowInternal(
    absl::Span<const XlaOp> operands, absl::Span<const XlaOp> init_values,
    const XlaComputation& computation,
    absl::Span<const int64_t> window_dimensions,
    absl::Span<const int64_t> window_strides,
    absl::Span<const int64_t> base_dilations,
    absl::Span<const int64_t> window_dilations,
    absl::Span<const std::pair<int64_t, int64_t>> padding) {
  std::vector<const Shape*> operand_shapes, init_shapes;
  for (int i = 0; i < operands.size(); ++i) {
    const auto& operand = operands[i];
    const auto& init_value = init_values[i];
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    operand_shapes.push_back(operand_shape);
    TF_ASSIGN_OR_RETURN(const Shape* init_shape, GetShapePtr(init_value));
    init_shapes.push_back(init_shape);
  }
  TF_ASSIGN_OR_RETURN(const ProgramShape& to_apply_shape,
                      computation.GetProgramShape());
  TF_ASSIGN_OR_RETURN(auto window,
                      ShapeInference::InferWindowFromDimensions(
                          window_dimensions, window_strides, padding,
                          /*lhs_dilation=*/base_dilations,
                          /*rhs_dilation=*/window_dilations));
  TF_ASSIGN_OR_RETURN(Shape shape,
                      ShapeInference::InferReduceWindowShape(
                          absl::MakeSpan(operand_shapes),
                          absl::MakeSpan(init_shapes), window, to_apply_shape));
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  *instr.mutable_window() = std::move(window);
  AddCalledComputation(computation, &instr);
  return instr;
}

StatusOr<XlaOp> XlaBuilder::ReduceWindowInternal(
    const Shape& shape, XlaOp operand, XlaOp init_value,
    const XlaComputation& computation, Window window) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  *instr.mutable_window() = std::move(window);

  AddCalledComputation(computation, &instr);
  return AddInstruction(std::move(instr), HloOpcode::kReduceWindow,
                        {operand, init_value});
}

XlaOp XlaBuilder::BatchNormTraining(XlaOp operand, XlaOp scale, XlaOp offset,
                                    float epsilon, int64_t feature_index) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;

    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(const Shape* scale_shape, GetShapePtr(scale));
    TF_ASSIGN_OR_RETURN(const Shape* offset_shape, GetShapePtr(offset));
    TF_ASSIGN_OR_RETURN(
        Shape shape,
        ShapeInference::InferBatchNormTrainingShape(
            *operand_shape, *scale_shape, *offset_shape, feature_index));
    *instr.mutable_shape() = shape.ToProto();

    instr.set_epsilon(epsilon);
    instr.set_feature_index(feature_index);

    return AddInstruction(std::move(instr), HloOpcode::kBatchNormTraining,
                          {operand, scale, offset});
  });
}

XlaOp XlaBuilder::BatchNormInference(XlaOp operand, XlaOp scale, XlaOp offset,
                                     XlaOp mean, XlaOp variance, float epsilon,
                                     int64_t feature_index) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;

    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(const Shape* scale_shape, GetShapePtr(scale));
    TF_ASSIGN_OR_RETURN(const Shape* offset_shape, GetShapePtr(offset));
    TF_ASSIGN_OR_RETURN(const Shape* mean_shape, GetShapePtr(mean));
    TF_ASSIGN_OR_RETURN(const Shape* variance_shape, GetShapePtr(variance));
    TF_ASSIGN_OR_RETURN(Shape shape,
                        ShapeInference::InferBatchNormInferenceShape(
                            *operand_shape, *scale_shape, *offset_shape,
                            *mean_shape, *variance_shape, feature_index));
    *instr.mutable_shape() = shape.ToProto();

    instr.set_epsilon(epsilon);
    instr.set_feature_index(feature_index);

    return AddInstruction(std::move(instr), HloOpcode::kBatchNormInference,
                          {operand, scale, offset, mean, variance});
  });
}

XlaOp XlaBuilder::BatchNormGrad(XlaOp operand, XlaOp scale, XlaOp batch_mean,
                                XlaOp batch_var, XlaOp grad_output,
                                float epsilon, int64_t feature_index) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;

    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(const Shape* scale_shape, GetShapePtr(scale));
    TF_ASSIGN_OR_RETURN(const Shape* batch_mean_shape, GetShapePtr(batch_mean));
    TF_ASSIGN_OR_RETURN(const Shape* batch_var_shape, GetShapePtr(batch_var));
    TF_ASSIGN_OR_RETURN(const Shape* grad_output_shape,
                        GetShapePtr(grad_output));
    TF_ASSIGN_OR_RETURN(
        Shape shape, ShapeInference::InferBatchNormGradShape(
                         *operand_shape, *scale_shape, *batch_mean_shape,
                         *batch_var_shape, *grad_output_shape, feature_index));
    *instr.mutable_shape() = shape.ToProto();

    instr.set_epsilon(epsilon);
    instr.set_feature_index(feature_index);

    return AddInstruction(std::move(instr), HloOpcode::kBatchNormGrad,
                          {operand, scale, batch_mean, batch_var, grad_output});
  });
}

XlaOp XlaBuilder::AllGather(XlaOp operand, int64_t all_gather_dimension,
                            int64_t shard_count,
                            absl::Span<const ReplicaGroup> replica_groups,
                            const absl::optional<ChannelHandle>& channel_id,
                            const absl::optional<Layout>& layout,
                            const absl::optional<bool> use_global_device_ids) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));

    TF_ASSIGN_OR_RETURN(
        Shape inferred_shape,
        ShapeInference::InferAllGatherShape({operand_shape},
                                            all_gather_dimension, shard_count));
    if (layout) {
      *inferred_shape.mutable_layout() = *layout;
      instr.set_constrain_layout(true);
    }
    *instr.mutable_shape() = inferred_shape.ToProto();

    instr.add_dimensions(all_gather_dimension);
    for (const ReplicaGroup& group : replica_groups) {
      *instr.add_replica_groups() = group;
    }
    if (channel_id.has_value()) {
      instr.set_channel_id(channel_id->handle());
    }
    if (use_global_device_ids.has_value()) {
      instr.set_use_global_device_ids(use_global_device_ids.value());
    }

    TF_ASSIGN_OR_RETURN(
        auto all_gather,
        AddInstruction(std::move(instr), HloOpcode::kAllGather, {operand}));
    return all_gather;
  });
}

XlaOp XlaBuilder::CrossReplicaSum(
    XlaOp operand, absl::Span<const ReplicaGroup> replica_groups) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(operand));
    const Shape* element_shape;
    if (shape->IsTuple()) {
      if (shape->tuple_shapes_size() == 0) {
        return Unimplemented(
            "0 element tuple CrossReplicaSum is not supported");
      }
      element_shape = &shape->tuple_shapes(0);
    } else {
      element_shape = shape;
    }
    const Shape scalar_shape =
        ShapeUtil::MakeShape(element_shape->element_type(), {});
    auto b = CreateSubBuilder("sum");
    auto x = b->Parameter(/*parameter_number=*/0, scalar_shape, "x");
    auto y = b->Parameter(/*parameter_number=*/1, scalar_shape, "y");
    if (scalar_shape.element_type() == PRED) {
      Or(x, y);
    } else {
      Add(x, y);
    }
    TF_ASSIGN_OR_RETURN(auto computation, b->Build());
    return AllReduce(operand, computation, replica_groups,
                     /*channel_id=*/absl::nullopt);
  });
}

XlaOp XlaBuilder::AllReduce(XlaOp operand, const XlaComputation& computation,
                            absl::Span<const ReplicaGroup> replica_groups,
                            const absl::optional<ChannelHandle>& channel_id,
                            const absl::optional<Shape>& shape_with_layout) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    std::vector<const Shape*> operand_shapes;
    std::vector<XlaOp> operands;
    if (operand_shape->IsTuple()) {
      if (operand_shape->tuple_shapes_size() == 0) {
        return Unimplemented("0 element tuple AllReduce is not supported");
      }
      for (int i = 0; i < operand_shape->tuple_shapes_size(); ++i) {
        if (operand_shape->tuple_shapes(i).element_type() !=
            operand_shape->tuple_shapes(0).element_type()) {
          return Unimplemented(
              "All the shapes of a tuple input of AllReduce must have the same "
              "element type");
        }
        operand_shapes.push_back(&operand_shape->tuple_shapes(i));
        operands.push_back(GetTupleElement(operand, i));
      }
    } else {
      operand_shapes.push_back(operand_shape);
      operands.push_back(operand);
    }

    TF_ASSIGN_OR_RETURN(Shape inferred_shape,
                        ShapeInference::InferAllReduceShape(operand_shapes));
    if (shape_with_layout) {
      if (!LayoutUtil::HasLayout(*shape_with_layout)) {
        return InvalidArgument("shape_with_layout must have the layout set: %s",
                               shape_with_layout->ToString());
      }
      if (!ShapeUtil::Compatible(*shape_with_layout, *operand_shape)) {
        return InvalidArgument(
            "Provided shape_with_layout must be compatible with the "
            "operand shape: %s vs %s",
            shape_with_layout->ToString(), operand_shape->ToString());
      }
      instr.set_constrain_layout(true);
      if (operand_shape->IsTuple() && !inferred_shape.IsTuple()) {
        // For a single-element tuple, take the tuple element shape.
        TF_RET_CHECK(shape_with_layout->tuple_shapes_size() == 1);
        *instr.mutable_shape() = shape_with_layout->tuple_shapes(0).ToProto();
      } else {
        *instr.mutable_shape() = shape_with_layout->ToProto();
      }
    } else {
      *instr.mutable_shape() = inferred_shape.ToProto();
    }

    for (const ReplicaGroup& group : replica_groups) {
      *instr.add_replica_groups() = group;
    }

    if (channel_id.has_value()) {
      instr.set_channel_id(channel_id->handle());
    }

    AddCalledComputation(computation, &instr);

    TF_ASSIGN_OR_RETURN(
        auto all_reduce,
        AddInstruction(std::move(instr), HloOpcode::kAllReduce, operands));
    if (operand_shape->IsTuple() && !inferred_shape.IsTuple()) {
      // For a single-element tuple, wrap the result into a tuple.
      TF_RET_CHECK(operand_shapes.size() == 1);
      TF_RET_CHECK(ShapeUtil::Compatible(*operand_shapes[0], inferred_shape));
      return Tuple({all_reduce});
    }
    return all_reduce;
  });
}

XlaOp XlaBuilder::ReduceScatter(
    XlaOp operand, const XlaComputation& computation, int64_t scatter_dimension,
    int64_t shard_count, absl::Span<const ReplicaGroup> replica_groups,
    const absl::optional<ChannelHandle>& channel_id,
    const absl::optional<Layout>& layout,
    const absl::optional<bool> use_global_device_ids) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    std::vector<const Shape*> operand_shapes;
    std::vector<XlaOp> operands;
    if (operand_shape->IsTuple()) {
      if (operand_shape->tuple_shapes_size() == 0) {
        return Unimplemented("0 element tuple ReduceScatter is not supported");
      }
      for (int i = 0; i < operand_shape->tuple_shapes_size(); ++i) {
        if (operand_shape->tuple_shapes(i).element_type() !=
            operand_shape->tuple_shapes(0).element_type()) {
          return Unimplemented(
              "All the shapes of a tuple input of ReduceScatter must have "
              "the same "
              "element type");
        }
        operand_shapes.push_back(&operand_shape->tuple_shapes(i));
        operands.push_back(GetTupleElement(operand, i));
      }
    } else {
      operand_shapes.push_back(operand_shape);
      operands.push_back(operand);
    }

    TF_ASSIGN_OR_RETURN(Shape inferred_shape,
                        ShapeInference::InferReduceScatterShape(
                            operand_shapes, scatter_dimension, shard_count));
    if (layout) {
      *inferred_shape.mutable_layout() = *layout;
      instr.set_constrain_layout(true);
    }
    *instr.mutable_shape() = inferred_shape.ToProto();

    AddCalledComputation(computation, &instr);

    instr.add_dimensions(scatter_dimension);
    for (const ReplicaGroup& group : replica_groups) {
      *instr.add_replica_groups() = group;
    }
    if (channel_id.has_value()) {
      instr.set_channel_id(channel_id->handle());
    }
    if (use_global_device_ids.has_value()) {
      instr.set_use_global_device_ids(use_global_device_ids.value());
    }

    TF_ASSIGN_OR_RETURN(
        auto reduce_scatter,
        AddInstruction(std::move(instr), HloOpcode::kReduceScatter, {operand}));
    return reduce_scatter;
  });
}

XlaOp XlaBuilder::AllToAll(XlaOp operand, int64_t split_dimension,
                           int64_t concat_dimension, int64_t split_count,
                           absl::Span<const ReplicaGroup> replica_groups,
                           const absl::optional<Layout>& layout) {
  // Array all_to_all may need to violate layout constraint to be legal so use
  // the tuple version.
  if (layout.has_value()) {
    return AllToAllTuple(operand, split_dimension, concat_dimension,
                         split_count, replica_groups, layout);
  }
  return AllToAllArray(operand, split_dimension, concat_dimension, split_count,
                       replica_groups);
}

XlaOp XlaBuilder::AllToAllArray(XlaOp operand, int64_t split_dimension,
                                int64_t concat_dimension, int64_t split_count,
                                absl::Span<const ReplicaGroup> replica_groups) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(
        const Shape all_to_all_shape,
        ShapeInference::InferAllToAllShape(*operand_shape, split_dimension,
                                           concat_dimension, split_count));
    HloInstructionProto instr;
    *instr.mutable_shape() = operand_shape->ToProto();
    if (replica_groups.empty()) {
      auto* group = instr.add_replica_groups();
      for (int64_t i = 0; i < split_count; ++i) {
        group->add_replica_ids(i);
      }
    } else {
      for (const ReplicaGroup& group : replica_groups) {
        *instr.add_replica_groups() = group;
      }
    }
    instr.add_dimensions(split_dimension);
    TF_ASSIGN_OR_RETURN(
        XlaOp all_to_all,
        AddInstruction(std::move(instr), HloOpcode::kAllToAll, {operand}));
    if (split_dimension == concat_dimension) {
      return all_to_all;
    }
    DimensionVector sizes;
    for (int64_t i = 0; i < operand_shape->rank(); ++i) {
      if (i != split_dimension) {
        sizes.push_back(operand_shape->dimensions(i));
        continue;
      }
      sizes.push_back(split_count);
      sizes.push_back(operand_shape->dimensions(i) / split_count);
    }
    all_to_all = Reshape(all_to_all, sizes);

    std::vector<int64_t> permutation;
    const auto rank = operand_shape->rank();
    permutation.reserve(rank + 1);
    for (int64_t i = 0; i < rank; ++i) {
      int64_t dim_after_reshape = i >= split_dimension ? i + 1 : i;
      if (i == concat_dimension) {
        permutation.push_back(split_dimension);
      }
      permutation.push_back(dim_after_reshape);
    }
    all_to_all = Transpose(all_to_all, permutation);
    return Reshape(all_to_all_shape, all_to_all);
  });
}

XlaOp XlaBuilder::AllToAllTuple(absl::Span<const XlaOp> operands,
                                absl::Span<const ReplicaGroup> replica_groups,
                                const absl::optional<Layout>& layout) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    TF_ASSIGN_OR_RETURN(auto operand_shapes, this->GetOperandShapes(operands));
    std::vector<const Shape*> operand_shape_ptrs;
    operand_shape_ptrs.reserve(operand_shapes.size());
    absl::c_transform(operand_shapes, std::back_inserter(operand_shape_ptrs),
                      [](const Shape& shape) { return &shape; });
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferAllToAllTupleShape(
                                         operand_shape_ptrs));

    if (layout) {
      TF_RET_CHECK(shape.IsTuple() && !ShapeUtil::IsNestedTuple(shape));
      for (int64_t i = 0; i < ShapeUtil::TupleElementCount(shape); ++i) {
        const int64_t layout_minor_to_major_size =
            layout->minor_to_major().size();
        if (layout_minor_to_major_size != shape.tuple_shapes(i).rank()) {
          return InvalidArgument(
              "Provided layout must be compatible with the operands' shape. "
              "The layout is %s, but operand %d has shape %s.",
              layout->ToString(), i, shape.tuple_shapes(i).ToString());
        }
        *(shape.mutable_tuple_shapes(i)->mutable_layout()) = *layout;
      }
      instr.set_constrain_layout(true);
    }
    *instr.mutable_shape() = shape.ToProto();

    for (const ReplicaGroup& group : replica_groups) {
      *instr.add_replica_groups() = group;
    }

    return AddInstruction(std::move(instr), HloOpcode::kAllToAll, operands);
  });
}

XlaOp XlaBuilder::AllToAllTuple(XlaOp operand, int64_t split_dimension,
                                int64_t concat_dimension, int64_t split_count,
                                absl::Span<const ReplicaGroup> replica_groups,
                                const absl::optional<Layout>& layout) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));

    // The HloInstruction for Alltoall currently only handles the data
    // communication: it accepts N already split parts and scatters them to N
    // cores, and each core gathers the N received parts into a tuple as the
    // output. So here we explicitly split the operand before the hlo alltoall,
    // and concat the tuple elements.
    //
    // First, run shape inference to make sure the shapes are valid.
    TF_RETURN_IF_ERROR(
        ShapeInference::InferAllToAllShape(*operand_shape, split_dimension,
                                           concat_dimension, split_count)
            .status());

    // Split into N parts.
    std::vector<XlaOp> slices;
    slices.reserve(split_count);
    const int64_t block_size =
        operand_shape->dimensions(split_dimension) / split_count;
    for (int i = 0; i < split_count; i++) {
      slices.push_back(SliceInDim(operand, /*start_index=*/i * block_size,
                                  /*limit_index=*/(i + 1) * block_size,
                                  /*stride=*/1, /*dimno=*/split_dimension));
    }

    // Handle data communication.
    XlaOp alltoall = this->AllToAllTuple(slices, replica_groups, layout);

    // Concat the N received parts.
    std::vector<XlaOp> received;
    received.reserve(split_count);
    for (int i = 0; i < split_count; i++) {
      received.push_back(this->GetTupleElement(alltoall, i));
    }
    return this->ConcatInDim(received, concat_dimension);
  });
}

XlaOp XlaBuilder::CollectivePermute(
    XlaOp operand,
    const std::vector<std::pair<int64_t, int64_t>>& source_target_pairs) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    HloInstructionProto instr;
    TF_ASSIGN_OR_RETURN(
        Shape shape,
        ShapeInference::InferCollectivePermuteShape({operand_shape}));
    *instr.mutable_shape() = shape.ToProto();

    for (const auto& pair : source_target_pairs) {
      auto* proto_pair = instr.add_source_target_pairs();
      proto_pair->set_source(pair.first);
      proto_pair->set_target(pair.second);
    }

    return AddInstruction(std::move(instr), HloOpcode::kCollectivePermute,
                          {operand});
  });
}

XlaOp XlaBuilder::ReplicaId() {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    *instr.mutable_shape() = ShapeUtil::MakeShape(U32, {}).ToProto();
    return AddInstruction(std::move(instr), HloOpcode::kReplicaId, {});
  });
}

XlaOp XlaBuilder::SelectAndScatter(XlaOp operand, const XlaComputation& select,
                                   absl::Span<const int64_t> window_dimensions,
                                   absl::Span<const int64_t> window_strides,
                                   Padding padding, XlaOp source,
                                   XlaOp init_value,
                                   const XlaComputation& scatter) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));

    std::vector<std::pair<int64_t, int64_t>> padding_values =
        MakePadding(operand_shape->dimensions(), window_dimensions,
                    window_strides, padding);

    TF_ASSIGN_OR_RETURN(auto window,
                        ShapeInference::InferWindowFromDimensions(
                            window_dimensions, window_strides, padding_values,
                            /*lhs_dilation=*/{},
                            /*rhs_dilation=*/{}));
    PaddingType padding_type = PADDING_INVALID;
    for (int64_t i = 0; i < operand_shape->rank(); ++i) {
      if (operand_shape->is_dynamic_dimension(i) &&
          !window_util::IsTrivialWindowDimension(window.dimensions(i)) &&
          padding == Padding::kSame) {
        // SAME padding can create dynamic padding sizes. The padding size
        // need to be rewritten by dynamic padder using HloInstructions. We
        // create a CustomCall to handle this.
        padding_type = PADDING_SAME;
      }
    }
    if (padding_type == PADDING_SAME) {
      TF_ASSIGN_OR_RETURN(
          HloInstructionProto instr,
          SelectAndScatterInternal(operand, select, window_dimensions,
                                   window_strides, padding_values, source,
                                   init_value, scatter));
      instr.set_custom_call_target("DynamicSelectAndScatterSamePadding");
      return AddInstruction(std::move(instr), HloOpcode::kCustomCall,
                            {operand, source, init_value});
    }
    return SelectAndScatterWithGeneralPadding(
        operand, select, window_dimensions, window_strides, padding_values,
        source, init_value, scatter);
  });
}

StatusOr<HloInstructionProto> XlaBuilder::SelectAndScatterInternal(
    XlaOp operand, const XlaComputation& select,
    absl::Span<const int64_t> window_dimensions,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding, XlaOp source,
    XlaOp init_value, const XlaComputation& scatter) {
  HloInstructionProto instr;

  TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
  TF_ASSIGN_OR_RETURN(const Shape* source_shape, GetShapePtr(source));
  TF_ASSIGN_OR_RETURN(const Shape* init_shape, GetShapePtr(init_value));
  TF_ASSIGN_OR_RETURN(const ProgramShape& select_shape,
                      select.GetProgramShape());
  TF_ASSIGN_OR_RETURN(const ProgramShape& scatter_shape,
                      scatter.GetProgramShape());
  TF_ASSIGN_OR_RETURN(*instr.mutable_window(),
                      ShapeInference::InferWindowFromDimensions(
                          window_dimensions, window_strides, padding,
                          /*lhs_dilation=*/{}, /*rhs_dilation=*/{}));
  TF_ASSIGN_OR_RETURN(Shape shape,
                      ShapeInference::InferSelectAndScatterShape(
                          *operand_shape, select_shape, instr.window(),
                          *source_shape, *init_shape, scatter_shape));
  *instr.mutable_shape() = shape.ToProto();

  AddCalledComputation(select, &instr);
  AddCalledComputation(scatter, &instr);
  return instr;
}

XlaOp XlaBuilder::SelectAndScatterWithGeneralPadding(
    XlaOp operand, const XlaComputation& select,
    absl::Span<const int64_t> window_dimensions,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding, XlaOp source,
    XlaOp init_value, const XlaComputation& scatter) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(HloInstructionProto instr,
                        SelectAndScatterInternal(
                            operand, select, window_dimensions, window_strides,
                            padding, source, init_value, scatter));

    return AddInstruction(std::move(instr), HloOpcode::kSelectAndScatter,
                          {operand, source, init_value});
  });
}

XlaOp XlaBuilder::ReducePrecision(XlaOp operand, const int exponent_bits,
                                  const int mantissa_bits) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape,
                        ShapeInference::InferReducePrecisionShape(
                            *operand_shape, exponent_bits, mantissa_bits));
    return ReducePrecisionInternal(shape, operand, exponent_bits,
                                   mantissa_bits);
  });
}

StatusOr<XlaOp> XlaBuilder::ReducePrecisionInternal(const Shape& shape,
                                                    XlaOp operand,
                                                    const int exponent_bits,
                                                    const int mantissa_bits) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  instr.set_exponent_bits(exponent_bits);
  instr.set_mantissa_bits(mantissa_bits);
  return AddInstruction(std::move(instr), HloOpcode::kReducePrecision,
                        {operand});
}

void XlaBuilder::Send(XlaOp operand, const ChannelHandle& handle) {
  ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    // Send HLO takes two operands: a data operand and a token. Generate the
    // token to pass into the send.
    // TODO(b/80000000): Remove this when clients have been updated to handle
    // tokens.
    HloInstructionProto token_instr;
    *token_instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
    TF_ASSIGN_OR_RETURN(XlaOp token, AddInstruction(std::move(token_instr),
                                                    HloOpcode::kAfterAll, {}));

    return SendWithToken(operand, token, handle);
  });
}

XlaOp XlaBuilder::SendWithToken(XlaOp operand, XlaOp token,
                                const ChannelHandle& handle) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (handle.type() != ChannelHandle::DEVICE_TO_DEVICE) {
      return InvalidArgument("Send must use a device-to-device channel");
    }

    // Send instruction produces a tuple of {aliased operand, U32 context,
    // token}.
    HloInstructionProto send_instr;
    TF_ASSIGN_OR_RETURN(const Shape* shape, GetShapePtr(operand));
    *send_instr.mutable_shape() =
        ShapeUtil::MakeTupleShape({*shape, ShapeUtil::MakeShape(U32, {}),
                                   ShapeUtil::MakeTokenShape()})
            .ToProto();
    send_instr.set_channel_id(handle.handle());
    TF_ASSIGN_OR_RETURN(XlaOp send,
                        AddInstruction(std::move(send_instr), HloOpcode::kSend,
                                       {operand, token}));

    HloInstructionProto send_done_instr;
    *send_done_instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
    send_done_instr.set_channel_id(handle.handle());
    return AddInstruction(std::move(send_done_instr), HloOpcode::kSendDone,
                          {send});
  });
}

XlaOp XlaBuilder::Recv(const Shape& shape, const ChannelHandle& handle) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    // Recv HLO takes a single token operand. Generate the token to pass into
    // the Recv and RecvDone instructions.
    // TODO(b/80000000): Remove this when clients have been updated to handle
    // tokens.
    HloInstructionProto token_instr;
    *token_instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
    TF_ASSIGN_OR_RETURN(XlaOp token, AddInstruction(std::move(token_instr),
                                                    HloOpcode::kAfterAll, {}));

    XlaOp recv = RecvWithToken(token, shape, handle);

    // The RecvDone instruction produces a tuple of the data and a token
    // type. Return XLA op containing the data.
    // TODO(b/80000000): Remove this when clients have been updated to handle
    // tokens.
    HloInstructionProto recv_data;
    *recv_data.mutable_shape() = shape.ToProto();
    recv_data.set_tuple_index(0);
    return AddInstruction(std::move(recv_data), HloOpcode::kGetTupleElement,
                          {recv});
  });
}

XlaOp XlaBuilder::RecvWithToken(XlaOp token, const Shape& shape,
                                const ChannelHandle& handle) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (handle.type() != ChannelHandle::DEVICE_TO_DEVICE) {
      return InvalidArgument("Recv must use a device-to-device channel");
    }

    // Recv instruction produces a tuple of {receive buffer, U32 context,
    // token}.
    HloInstructionProto recv_instr;
    *recv_instr.mutable_shape() =
        ShapeUtil::MakeTupleShape(
            {shape, ShapeUtil::MakeShape(U32, {}), ShapeUtil::MakeTokenShape()})
            .ToProto();
    recv_instr.set_channel_id(handle.handle());
    TF_ASSIGN_OR_RETURN(XlaOp recv, AddInstruction(std::move(recv_instr),
                                                   HloOpcode::kRecv, {token}));

    HloInstructionProto recv_done_instr;
    *recv_done_instr.mutable_shape() =
        ShapeUtil::MakeTupleShape({shape, ShapeUtil::MakeTokenShape()})
            .ToProto();
    recv_done_instr.set_channel_id(handle.handle());
    return AddInstruction(std::move(recv_done_instr), HloOpcode::kRecvDone,
                          {recv});
  });
}

XlaOp XlaBuilder::SendToHost(XlaOp operand, XlaOp token,
                             const Shape& shape_with_layout,
                             const ChannelHandle& handle) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (!LayoutUtil::HasLayout(shape_with_layout)) {
      return InvalidArgument("Shape passed to SendToHost must have a layout");
    }
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    if (!ShapeUtil::Compatible(*operand_shape, shape_with_layout)) {
      return InvalidArgument(
          "SendToHost shape %s must be compatible with operand shape %s",
          ShapeUtil::HumanStringWithLayout(shape_with_layout),
          ShapeUtil::HumanStringWithLayout(*operand_shape));
    }
    // TODO(b/111544877): Support tuple shapes.
    if (!operand_shape->IsArray()) {
      return InvalidArgument("SendToHost only supports array shapes, shape: %s",
                             ShapeUtil::HumanString(*operand_shape));
    }

    if (handle.type() != ChannelHandle::DEVICE_TO_HOST) {
      return InvalidArgument("SendToHost must use a device-to-host channel");
    }

    // Send instruction produces a tuple of {aliased operand, U32 context,
    // token}.
    HloInstructionProto send_instr;
    *send_instr.mutable_shape() =
        ShapeUtil::MakeTupleShape({shape_with_layout,
                                   ShapeUtil::MakeShape(U32, {}),
                                   ShapeUtil::MakeTokenShape()})
            .ToProto();
    send_instr.set_channel_id(handle.handle());
    send_instr.set_is_host_transfer(true);
    TF_ASSIGN_OR_RETURN(XlaOp send,
                        AddInstruction(std::move(send_instr), HloOpcode::kSend,
                                       {operand, token}));

    HloInstructionProto send_done_instr;
    *send_done_instr.mutable_shape() = ShapeUtil::MakeTokenShape().ToProto();
    send_done_instr.set_channel_id(handle.handle());
    send_done_instr.set_is_host_transfer(true);
    TF_ASSIGN_OR_RETURN(XlaOp send_done,
                        AddInstruction(std::move(send_done_instr),
                                       HloOpcode::kSendDone, {send}));
    return send_done;
  });
}

XlaOp XlaBuilder::RecvFromHost(XlaOp token, const Shape& shape,
                               const ChannelHandle& handle) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    if (!LayoutUtil::HasLayout(shape)) {
      return InvalidArgument("Shape passed to RecvFromHost must have a layout");
    }

    // TODO(b/111544877): Support tuple shapes.
    if (!shape.IsArray()) {
      return InvalidArgument(
          "RecvFromHost only supports array shapes, shape: %s",
          ShapeUtil::HumanString(shape));
    }

    if (handle.type() != ChannelHandle::HOST_TO_DEVICE) {
      return InvalidArgument("RecvFromHost must use a host-to-device channel");
    }

    // Recv instruction produces a tuple of {receive buffer, U32 context,
    // token}.
    HloInstructionProto recv_instr;
    *recv_instr.mutable_shape() =
        ShapeUtil::MakeTupleShape(
            {shape, ShapeUtil::MakeShape(U32, {}), ShapeUtil::MakeTokenShape()})
            .ToProto();
    recv_instr.set_channel_id(handle.handle());
    recv_instr.set_is_host_transfer(true);
    TF_ASSIGN_OR_RETURN(XlaOp recv, AddInstruction(std::move(recv_instr),
                                                   HloOpcode::kRecv, {token}));

    HloInstructionProto recv_done_instr;
    *recv_done_instr.mutable_shape() =
        ShapeUtil::MakeTupleShape({shape, ShapeUtil::MakeTokenShape()})
            .ToProto();
    recv_done_instr.set_channel_id(handle.handle());
    recv_done_instr.set_is_host_transfer(true);
    return AddInstruction(std::move(recv_done_instr), HloOpcode::kRecvDone,
                          {recv});
  });
}

XlaOp XlaBuilder::GetDimensionSize(XlaOp operand, int64_t dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferGetDimensionSizeShape(
                                         *operand_shape, dimension));
    // Calling GetDimensionSize on a static dimension returns a constant
    // instruction.
    if (!operand_shape->is_dynamic_dimension(dimension)) {
      return ConstantR0<int32_t>(this, operand_shape->dimensions(dimension));
    }
    *instr.mutable_shape() = shape.ToProto();
    instr.add_dimensions(dimension);
    return AddInstruction(std::move(instr), HloOpcode::kGetDimensionSize,
                          {operand});
  });
}

XlaOp XlaBuilder::RemoveDynamicDimension(XlaOp operand, int64_t dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    HloInstructionProto instr;
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));

    Shape shape = *operand_shape;
    shape.set_dynamic_dimension(dimension, false);
    // Setting an op's dynamic dimension to its static size removes the dynamic
    // dimension.
    XlaOp static_size =
        ConstantR0<int32_t>(this, operand_shape->dimensions(dimension));
    return SetDimensionSizeInternal(shape, operand, static_size, dimension);
  });
}

XlaOp XlaBuilder::SetDimensionSize(XlaOp operand, XlaOp val,
                                   int64_t dimension) {
  return ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* operand_shape, GetShapePtr(operand));
    TF_ASSIGN_OR_RETURN(const Shape* val_shape, GetShapePtr(val));

    TF_ASSIGN_OR_RETURN(Shape shape,
                        ShapeInference::InferSetDimensionSizeShape(
                            *operand_shape, *val_shape, dimension));
    return SetDimensionSizeInternal(shape, operand, val, dimension);
  });
}

StatusOr<XlaOp> XlaBuilder::SetDimensionSizeInternal(const Shape& shape,
                                                     XlaOp operand, XlaOp val,
                                                     int64_t dimension) {
  TF_ASSIGN_OR_RETURN(const HloInstructionProto* val_proto,
                      LookUpInstruction(val));
  if (StringToHloOpcode(val_proto->opcode()).ValueOrDie() ==
          HloOpcode::kConstant &&
      shape.is_dynamic_dimension(dimension)) {
    TF_ASSIGN_OR_RETURN(auto constant_size,
                        Literal::CreateFromProto(val_proto->literal(), true));
    if (constant_size.Get<int32_t>({}) == shape.dimensions(dimension)) {
      return operand;
    }
  }

  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  instr.add_dimensions(dimension);
  return AddInstruction(std::move(instr), HloOpcode::kSetDimensionSize,
                        {operand, val});
}

StatusOr<bool> XlaBuilder::IsConstant(XlaOp operand) const {
  TF_RETURN_IF_ERROR(first_error_);

  // Verify that the handle is valid.
  TF_RETURN_IF_ERROR(LookUpInstruction(operand).status());

  bool is_constant = true;
  absl::flat_hash_set<int64_t> visited;
  IsConstantVisitor(operand.handle(), /*depth=*/0, &visited, &is_constant);
  return is_constant;
}

StatusOr<XlaComputation> XlaBuilder::BuildConstantSubGraph(
    XlaOp root_op, bool dynamic_dimension_is_minus_one) {
  TF_ASSIGN_OR_RETURN(bool is_constant, IsConstant(root_op));
  if (!is_constant) {
    auto op_status = LookUpInstruction(root_op);
    std::string op_string =
        op_status.ok() ? op_status.ValueOrDie()->name() : "<unknown operation>";
    return InvalidArgument(
        "Operand to BuildConstantSubGraph depends on a parameter.\n\n"
        "  op requested for constant subgraph: %s\n\n"
        "This is an internal error that typically happens when the XLA user "
        "(e.g. TensorFlow) is attempting to determine a value that must be a "
        "compile-time constant (e.g. an array dimension) but it is not capable "
        "of being evaluated at XLA compile time.\n\n"
        "Please file a usability bug with the framework being used (e.g. "
        "TensorFlow).",
        op_string);
  }

  TF_ASSIGN_OR_RETURN(const HloInstructionProto* root,
                      LookUpInstruction(root_op));
  if (VLOG_IS_ON(4)) {
    VLOG(4) << "Build constant subgraph for:\n" << OpToString(root_op);
  }

  HloComputationProto entry;
  SetProtoIdAndName(&entry, StrCat(name_, "_compute_constant"), kNameSeparator,
                    GetNextId());
  ProgramShapeProto* program_shape = entry.mutable_program_shape();
  *program_shape->mutable_result() = root->shape();

  // We use std::set to keep the instruction ids in ascending order (which is
  // also a valid dependency order). The related ops will be added to the
  // subgraph in the same order.
  std::set<int64_t> related_ops;
  absl::flat_hash_map<int64_t, int64_t> substitutions;
  absl::flat_hash_set<int64_t> related_calls;  // Related computations.
  std::queue<int64_t> worklist;
  worklist.push(root->id());
  related_ops.insert(root->id());

  while (!worklist.empty()) {
    int64_t handle = worklist.front();
    worklist.pop();
    TF_ASSIGN_OR_RETURN(const HloInstructionProto* instr_proto,
                        LookUpInstructionByHandle(handle));

    auto default_behavior = [&related_ops, &worklist, &related_calls,
                             instr_proto]() {
      for (int64_t id : instr_proto->operand_ids()) {
        if (related_ops.insert(id).second) {
          worklist.push(id);
        }
      }
      for (int64_t called_id : instr_proto->called_computation_ids()) {
        related_calls.insert(called_id);
      }
    };

    if (instr_proto->opcode() ==
            HloOpcodeString(HloOpcode::kGetDimensionSize) ||
        InstrIsSetBound(instr_proto)) {
      int32_t constant_value = -1;
      HloInstructionProto const_instr;

      if (instr_proto->opcode() ==
          HloOpcodeString(HloOpcode::kGetDimensionSize)) {
        // At this point, BuildConstantSubGraph should never encounter a
        // GetDimensionSize with a dynamic dimension. IsConstant check would
        // have failed at the beginning of this function.
        //
        // Replace GetDimensionSize with a Constant representing the static
        // bound of the shape.
        int64_t dimension = instr_proto->dimensions(0);
        int64_t operand_handle = instr_proto->operand_ids(0);
        TF_ASSIGN_OR_RETURN(const HloInstructionProto* operand_proto,
                            LookUpInstructionByHandle(operand_handle));

        if (!(operand_proto->shape().is_dynamic_dimension(dimension) &&
              dynamic_dimension_is_minus_one)) {
          constant_value = static_cast<int32_t>(
              operand_proto->shape().dimensions(dimension));
        }
        Literal literal = LiteralUtil::CreateR0(constant_value);
        *const_instr.mutable_literal() = literal.ToProto();
        *const_instr.mutable_shape() = literal.shape().ToProto();
      } else {
        if (instr_proto->literal().shape().element_type() == TUPLE) {
          *const_instr.mutable_literal() =
              // First literal of SetBound contains bounds, second literal
              // contains dynamism indicators.
              instr_proto->literal().tuple_literals(0);
        } else {
          *const_instr.mutable_literal() = instr_proto->literal();
        }

        *const_instr.mutable_shape() = instr_proto->shape();
      }
      *const_instr.mutable_opcode() = HloOpcodeString(HloOpcode::kConstant);
      const_instr.set_id(handle);
      *const_instr.mutable_name() =
          GetFullName(const_instr.opcode(), kNameSeparator, const_instr.id());
      *entry.add_instructions() =
          const_instr;  // Add to the result constant graph.

    } else if (instr_proto->opcode() ==
               HloOpcodeString(HloOpcode::kGetTupleElement)) {
      // Look through GTE(Tuple(..), i).
      TF_ASSIGN_OR_RETURN(
          const HloInstructionProto* maybe_tuple_instr,
          LookUpInstructionByHandle(instr_proto->operand_ids(0)));

      if (maybe_tuple_instr->opcode() == HloOpcodeString(HloOpcode::kTuple)) {
        int64_t id = maybe_tuple_instr->operand_ids(instr_proto->tuple_index());
        // Enqueue any dependencies of `id`.
        if (related_ops.insert(id).second) {
          worklist.push(id);
        }
        substitutions[handle] = id;

      } else {
        default_behavior();
      }

    } else {
      default_behavior();
    }
  }

  // Resolve any substitutions for the root id.
  int64_t root_id = root->id();
  auto it = substitutions.find(root_id);
  while (it != substitutions.end()) {
    root_id = it->second;
    it = substitutions.find(root_id);
  }
  entry.set_root_id(root_id);

  // Add related ops to the computation.
  for (int64_t id : related_ops) {
    if (substitutions.find(id) != substitutions.end()) {
      // Skip adding this instruction; we will replace references to it with the
      // substitution instruction's id.
      continue;
    }
    TF_ASSIGN_OR_RETURN(const HloInstructionProto* instr_src,
                        LookUpInstructionByHandle(id));

    if (instr_src->opcode() == HloOpcodeString(HloOpcode::kGetDimensionSize) ||
        InstrIsSetBound(instr_src)) {
      continue;
    }
    HloInstructionProto* instr = entry.add_instructions();
    *instr = *instr_src;
    // Replace operands in case we have substitutions mapped.
    instr->clear_operand_ids();
    for (int64_t operand_id : instr_src->operand_ids()) {
      auto it = substitutions.find(operand_id);
      while (it != substitutions.end()) {
        operand_id = it->second;
        it = substitutions.find(operand_id);
      }
      instr->add_operand_ids(operand_id);
    }
    // Ensures that the instruction names are unique among the graph.
    const std::string& new_name =
        StrCat(instr->name(), ".", entry.id(), ".", instr->id());
    instr->set_name(new_name);
  }

  XlaComputation computation(entry.id());
  HloModuleProto* module = computation.mutable_proto();
  module->set_name(entry.name());
  module->set_id(entry.id());
  module->set_entry_computation_name(entry.name());
  module->set_entry_computation_id(entry.id());
  *module->mutable_host_program_shape() = *program_shape;
  for (auto& e : embedded_) {
    if (related_calls.find(e.second.id()) != related_calls.end()) {
      *module->add_computations() = e.second;
    }
  }
  *module->add_computations() = std::move(entry);
  if (VLOG_IS_ON(4)) {
    VLOG(4) << "Constant computation:\n" << module->DebugString();
  }
  return std::move(computation);
}

std::unique_ptr<XlaBuilder> XlaBuilder::CreateSubBuilder(
    const std::string& computation_name) {
  auto sub_builder = absl::make_unique<XlaBuilder>(computation_name);
  sub_builder->parent_builder_ = this;
  sub_builder->die_immediately_on_error_ = this->die_immediately_on_error_;
  return sub_builder;
}

/* static */ ConvolutionDimensionNumbers
XlaBuilder::CreateDefaultConvDimensionNumbers(int num_spatial_dims) {
  ConvolutionDimensionNumbers dimension_numbers;
  dimension_numbers.set_input_batch_dimension(kConvBatchDimension);
  dimension_numbers.set_input_feature_dimension(kConvFeatureDimension);
  dimension_numbers.set_output_batch_dimension(kConvBatchDimension);
  dimension_numbers.set_output_feature_dimension(kConvFeatureDimension);
  dimension_numbers.set_kernel_output_feature_dimension(
      kConvKernelOutputDimension);
  dimension_numbers.set_kernel_input_feature_dimension(
      kConvKernelInputDimension);
  for (int i = 0; i < num_spatial_dims; ++i) {
    dimension_numbers.add_input_spatial_dimensions(i + 2);
    dimension_numbers.add_kernel_spatial_dimensions(i + 2);
    dimension_numbers.add_output_spatial_dimensions(i + 2);
  }
  return dimension_numbers;
}

/* static */ Status XlaBuilder::Validate(
    const ConvolutionDimensionNumbers& dnum) {
  if (dnum.input_spatial_dimensions_size() < 2) {
    return FailedPrecondition("input spacial dimension < 2: %d",
                              dnum.input_spatial_dimensions_size());
  }
  if (dnum.kernel_spatial_dimensions_size() < 2) {
    return FailedPrecondition("kernel spacial dimension < 2: %d",
                              dnum.kernel_spatial_dimensions_size());
  }
  if (dnum.output_spatial_dimensions_size() < 2) {
    return FailedPrecondition("output spacial dimension < 2: %d",
                              dnum.output_spatial_dimensions_size());
  }

  if (std::set<int64_t>(
          {dnum.input_batch_dimension(), dnum.input_feature_dimension(),
           dnum.input_spatial_dimensions(0), dnum.input_spatial_dimensions(1)})
          .size() != 4) {
    return FailedPrecondition(
        "dimension numbers for the input are not unique: (%d, %d, %d, "
        "%d)",
        dnum.input_batch_dimension(), dnum.input_feature_dimension(),
        dnum.input_spatial_dimensions(0), dnum.input_spatial_dimensions(1));
  }
  if (std::set<int64_t>({dnum.kernel_output_feature_dimension(),
                         dnum.kernel_input_feature_dimension(),
                         dnum.kernel_spatial_dimensions(0),
                         dnum.kernel_spatial_dimensions(1)})
          .size() != 4) {
    return FailedPrecondition(
        "dimension numbers for the weight are not unique: (%d, %d, %d, "
        "%d)",
        dnum.kernel_output_feature_dimension(),
        dnum.kernel_input_feature_dimension(),
        dnum.kernel_spatial_dimensions(0), dnum.kernel_spatial_dimensions(1));
  }
  if (std::set<int64_t>({dnum.output_batch_dimension(),
                         dnum.output_feature_dimension(),
                         dnum.output_spatial_dimensions(0),
                         dnum.output_spatial_dimensions(1)})
          .size() != 4) {
    return FailedPrecondition(
        "dimension numbers for the output are not unique: (%d, %d, %d, "
        "%d)",
        dnum.output_batch_dimension(), dnum.output_feature_dimension(),
        dnum.output_spatial_dimensions(0), dnum.output_spatial_dimensions(1));
  }
  return Status::OK();
}

StatusOr<XlaOp> XlaBuilder::AddInstruction(HloInstructionProto&& instr,
                                           HloOpcode opcode,
                                           absl::Span<const XlaOp> operands) {
  TF_RETURN_IF_ERROR(first_error_);

  const int64_t handle = GetNextId();
  instr.set_id(handle);
  instr.set_opcode(HloOpcodeString(opcode));
  if (instr.name().empty()) {
    instr.set_name(instr.opcode());
  }
  for (const auto& operand : operands) {
    if (operand.builder_ == nullptr) {
      return InvalidArgument("invalid XlaOp with handle %d", operand.handle());
    }
    if (operand.builder_ != this) {
      return InvalidArgument("Do not add XlaOp from builder %s to builder %s",
                             operand.builder_->name(), this->name());
    }
    instr.add_operand_ids(operand.handle());
  }

  if (one_shot_metadata_.has_value()) {
    *instr.mutable_metadata() = one_shot_metadata_.value();
    one_shot_metadata_.reset();
  } else {
    *instr.mutable_metadata() = metadata_;
  }
  if (sharding_) {
    *instr.mutable_sharding() = *sharding_;
  }
  *instr.mutable_frontend_attributes() = frontend_attributes_;

  handle_to_index_[handle] = instructions_.size();
  instructions_.push_back(std::move(instr));
  instruction_shapes_.push_back(
      absl::make_unique<Shape>(instructions_.back().shape()));

  XlaOp op(handle, this);
  return op;
}

StatusOr<XlaOp> XlaBuilder::AddOpWithShape(HloOpcode opcode, const Shape& shape,
                                           absl::Span<const XlaOp> operands) {
  HloInstructionProto instr;
  *instr.mutable_shape() = shape.ToProto();
  return AddInstruction(std::move(instr), opcode, operands);
}

void XlaBuilder::AddCalledComputation(const XlaComputation& computation,
                                      HloInstructionProto* instr) {
  absl::flat_hash_map<int64_t, int64_t> remapped_ids;
  std::vector<HloComputationProto> imported_computations;
  imported_computations.reserve(computation.proto().computations_size());
  // Before we import the computations by remapping IDs, and capturing the
  // old->new mappings in remapped_ids.
  for (const HloComputationProto& e : computation.proto().computations()) {
    HloComputationProto new_computation(e);
    int64_t computation_id = GetNextId();
    remapped_ids[new_computation.id()] = computation_id;
    SetProtoIdAndName(&new_computation,
                      GetBaseName(new_computation.name(), kNameSeparator),
                      kNameSeparator, computation_id);
    for (auto& instruction : *new_computation.mutable_instructions()) {
      int64_t instruction_id = GetNextId();
      remapped_ids[instruction.id()] = instruction_id;
      SetProtoIdAndName(&instruction,
                        GetBaseName(instruction.name(), kNameSeparator),
                        kNameSeparator, instruction_id);
    }
    new_computation.set_root_id(remapped_ids.at(new_computation.root_id()));

    imported_computations.push_back(std::move(new_computation));
  }
  // Once we have imported all the computations, and captured all the ID
  // mappings, we go back and fixup the IDs in the imported computations.
  instr->add_called_computation_ids(
      remapped_ids.at(computation.proto().entry_computation_id()));
  for (auto& imported_computation : imported_computations) {
    for (auto& instruction : *imported_computation.mutable_instructions()) {
      for (auto& operand_id : *instruction.mutable_operand_ids()) {
        operand_id = remapped_ids.at(operand_id);
      }
      for (auto& control_predecessor_id :
           *instruction.mutable_control_predecessor_ids()) {
        control_predecessor_id = remapped_ids.at(control_predecessor_id);
      }
      for (auto& called_computation_id :
           *instruction.mutable_called_computation_ids()) {
        called_computation_id = remapped_ids.at(called_computation_id);
      }
    }

    int64_t computation_id = imported_computation.id();
    for (int64_t i = 0; i < imported_computation.instructions_size(); ++i) {
      ImportedInstruction imported_instruction;
      imported_instruction.computation_id = computation_id;
      imported_instruction.instruction_index = i;
      handle_to_imported_index_.insert(
          {imported_computation.instructions(i).id(), imported_instruction});
    }
    embedded_.insert({computation_id, std::move(imported_computation)});
  }
}

StatusOr<const HloInstructionProto*> XlaBuilder::LookUpInstruction(
    const XlaOp op) const {
  TF_RETURN_IF_ERROR(first_error_);
  return LookUpInstructionInternal<const HloInstructionProto*>(op);
}

StatusOr<const HloInstructionProto*> XlaBuilder::LookUpInstructionByHandle(
    int64_t handle) const {
  return LookUpInstructionByHandleInternal<const HloInstructionProto*>(handle);
}

StatusOr<HloInstructionProto*> XlaBuilder::LookUpMutableInstruction(
    const XlaOp op) {
  TF_RETURN_IF_ERROR(first_error_);
  return LookUpInstructionInternal<HloInstructionProto*>(op);
}

StatusOr<HloInstructionProto*> XlaBuilder::LookUpMutableInstructionByHandle(
    int64_t handle) {
  return LookUpInstructionByHandleInternal<HloInstructionProto*>(handle);
}

// Enqueues a "retrieve parameter value" instruction for a parameter that was
// passed to the computation.
XlaOp Parameter(XlaBuilder* builder, int64_t parameter_number,
                const Shape& shape, const std::string& name) {
  std::vector<bool> empty_bools;
  return Parameter(builder, parameter_number, shape, name, empty_bools);
}

XlaOp Parameter(XlaBuilder* builder, int64_t parameter_number,
                const Shape& shape, const std::string& name,
                const std::vector<bool>& replicated_at_leaf_buffers) {
  return builder->Parameter(parameter_number, shape, name,
                            replicated_at_leaf_buffers);
}

// Enqueues a constant with the value of the given literal onto the
// computation.
XlaOp ConstantLiteral(XlaBuilder* builder, const LiteralSlice& literal) {
  return builder->ConstantLiteral(literal);
}

XlaOp Broadcast(const XlaOp operand,
                absl::Span<const int64_t> broadcast_sizes) {
  return operand.builder()->Broadcast(operand, broadcast_sizes);
}

XlaOp BroadcastInDim(const XlaOp operand,
                     const absl::Span<const int64_t> out_dim_size,
                     const absl::Span<const int64_t> broadcast_dimensions) {
  return operand.builder()->BroadcastInDim(operand, out_dim_size,
                                           broadcast_dimensions);
}

XlaOp Copy(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kCopy, operand);
}

XlaOp Pad(const XlaOp operand, const XlaOp padding_value,
          const PaddingConfig& padding_config) {
  return operand.builder()->Pad(operand, padding_value, padding_config);
}

XlaOp PadInDim(XlaOp operand, XlaOp padding_value, int64_t dimno,
               int64_t pad_lo, int64_t pad_hi) {
  return operand.builder()->PadInDim(operand, padding_value, dimno, pad_lo,
                                     pad_hi);
}

XlaOp Reshape(const XlaOp operand, absl::Span<const int64_t> dimensions,
              absl::Span<const int64_t> new_sizes) {
  return operand.builder()->Reshape(operand, dimensions, new_sizes);
}

XlaOp Reshape(const XlaOp operand, absl::Span<const int64_t> new_sizes) {
  return operand.builder()->Reshape(operand, new_sizes);
}

XlaOp Reshape(const Shape& shape, XlaOp operand) {
  return operand.builder()->Reshape(shape, operand);
}

XlaOp DynamicReshape(XlaOp operand, absl::Span<const XlaOp> dim_sizes,
                     absl::Span<const int64_t> new_size_bounds,
                     const std::vector<bool>& dims_are_dynamic) {
  return operand.builder()->DynamicReshape(operand, dim_sizes, new_size_bounds,
                                           dims_are_dynamic);
}

XlaOp ReshapeWithInferredDimension(XlaOp operand,
                                   absl::Span<const int64_t> new_sizes,
                                   int64_t inferred_dimension) {
  return operand.builder()->Reshape(operand, new_sizes, inferred_dimension);
}

XlaOp Collapse(const XlaOp operand, absl::Span<const int64_t> dimensions) {
  return operand.builder()->Collapse(operand, dimensions);
}

XlaOp Slice(const XlaOp operand, absl::Span<const int64_t> start_indices,
            absl::Span<const int64_t> limit_indices,
            absl::Span<const int64_t> strides) {
  return operand.builder()->Slice(operand, start_indices, limit_indices,
                                  strides);
}

XlaOp SliceInDim(const XlaOp operand, int64_t start_index, int64_t limit_index,
                 int64_t stride, int64_t dimno) {
  return operand.builder()->SliceInDim(operand, start_index, limit_index,
                                       stride, dimno);
}

XlaOp DynamicSlice(const XlaOp operand, absl::Span<const XlaOp> start_indices,
                   absl::Span<const int64_t> slice_sizes) {
  return operand.builder()->DynamicSlice(operand, start_indices, slice_sizes);
}

XlaOp DynamicUpdateSlice(const XlaOp operand, const XlaOp update,
                         absl::Span<const XlaOp> start_indices) {
  return operand.builder()->DynamicUpdateSlice(operand, update, start_indices);
}

XlaOp ConcatInDim(XlaBuilder* builder, absl::Span<const XlaOp> operands,
                  int64_t dimension) {
  return builder->ConcatInDim(operands, dimension);
}

XlaOp Select(const XlaOp pred, const XlaOp on_true, const XlaOp on_false) {
  return pred.builder()->Select(pred, on_true, on_false);
}

XlaOp Tuple(XlaBuilder* builder, absl::Span<const XlaOp> elements) {
  return builder->Tuple(elements);
}

XlaOp GetTupleElement(const XlaOp tuple_data, int64_t index) {
  return tuple_data.builder()->GetTupleElement(tuple_data, index);
}

XlaOp Eq(const XlaOp lhs, const XlaOp rhs,
         absl::Span<const int64_t> broadcast_dimensions) {
  return Compare(lhs, rhs, broadcast_dimensions, ComparisonDirection::kEq);
}

static XlaOp CompareTotalOrder(const XlaOp lhs, const XlaOp rhs,
                               absl::Span<const int64_t> broadcast_dimensions,
                               ComparisonDirection comparison_direction) {
  auto b = lhs.builder();
  return b->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(auto operand_shape, b->GetShape(lhs));
    auto operand_element_type = operand_shape.element_type();
    auto compare_type =
        primitive_util::IsFloatingPointType(operand_element_type)
            ? Comparison::Type::kFloatTotalOrder
            : Comparison::DefaultComparisonType(operand_element_type);
    return Compare(lhs, rhs, broadcast_dimensions, comparison_direction,
                   compare_type);
  });
}

XlaOp EqTotalOrder(const XlaOp lhs, const XlaOp rhs,
                   absl::Span<const int64_t> broadcast_dimensions) {
  return CompareTotalOrder(lhs, rhs, broadcast_dimensions,
                           ComparisonDirection::kEq);
}

XlaOp Ne(const XlaOp lhs, const XlaOp rhs,
         absl::Span<const int64_t> broadcast_dimensions) {
  return Compare(lhs, rhs, broadcast_dimensions, ComparisonDirection::kNe);
}

XlaOp NeTotalOrder(const XlaOp lhs, const XlaOp rhs,
                   absl::Span<const int64_t> broadcast_dimensions) {
  return CompareTotalOrder(lhs, rhs, broadcast_dimensions,
                           ComparisonDirection::kNe);
}

XlaOp Ge(const XlaOp lhs, const XlaOp rhs,
         absl::Span<const int64_t> broadcast_dimensions) {
  return Compare(lhs, rhs, broadcast_dimensions, ComparisonDirection::kGe);
}

XlaOp GeTotalOrder(const XlaOp lhs, const XlaOp rhs,
                   absl::Span<const int64_t> broadcast_dimensions) {
  return CompareTotalOrder(lhs, rhs, broadcast_dimensions,
                           ComparisonDirection::kGe);
}

XlaOp Gt(const XlaOp lhs, const XlaOp rhs,
         absl::Span<const int64_t> broadcast_dimensions) {
  return Compare(lhs, rhs, broadcast_dimensions, ComparisonDirection::kGt);
}

XlaOp GtTotalOrder(const XlaOp lhs, const XlaOp rhs,
                   absl::Span<const int64_t> broadcast_dimensions) {
  return CompareTotalOrder(lhs, rhs, broadcast_dimensions,
                           ComparisonDirection::kGt);
}

XlaOp Le(const XlaOp lhs, const XlaOp rhs,
         absl::Span<const int64_t> broadcast_dimensions) {
  return Compare(lhs, rhs, broadcast_dimensions, ComparisonDirection::kLe);
}

XlaOp LeTotalOrder(const XlaOp lhs, const XlaOp rhs,
                   absl::Span<const int64_t> broadcast_dimensions) {
  return CompareTotalOrder(lhs, rhs, broadcast_dimensions,
                           ComparisonDirection::kLe);
}

XlaOp Lt(const XlaOp lhs, const XlaOp rhs,
         absl::Span<const int64_t> broadcast_dimensions) {
  return Compare(lhs, rhs, broadcast_dimensions, ComparisonDirection::kLt);
}

XlaOp LtTotalOrder(const XlaOp lhs, const XlaOp rhs,
                   absl::Span<const int64_t> broadcast_dimensions) {
  return CompareTotalOrder(lhs, rhs, broadcast_dimensions,
                           ComparisonDirection::kLt);
}

XlaOp Compare(const XlaOp lhs, const XlaOp rhs,
              absl::Span<const int64_t> broadcast_dimensions,
              ComparisonDirection direction) {
  return lhs.builder()->BinaryOp(HloOpcode::kCompare, lhs, rhs,
                                 broadcast_dimensions, direction);
}

XlaOp Compare(const XlaOp lhs, const XlaOp rhs,
              absl::Span<const int64_t> broadcast_dimensions,
              ComparisonDirection direction, Comparison::Type compare_type) {
  return lhs.builder()->BinaryOp(HloOpcode::kCompare, lhs, rhs,
                                 broadcast_dimensions, direction, compare_type);
}

XlaOp Compare(const XlaOp lhs, const XlaOp rhs, ComparisonDirection direction) {
  return Compare(lhs, rhs, {}, direction);
}

XlaOp Dot(const XlaOp lhs, const XlaOp rhs,
          const PrecisionConfig* precision_config,
          absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->Dot(lhs, rhs, precision_config, preferred_element_type);
}

XlaOp DotGeneral(const XlaOp lhs, const XlaOp rhs,
                 const DotDimensionNumbers& dimension_numbers,
                 const PrecisionConfig* precision_config,
                 absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->DotGeneral(lhs, rhs, dimension_numbers,
                                   precision_config, preferred_element_type);
}

XlaOp Conv(const XlaOp lhs, const XlaOp rhs,
           absl::Span<const int64_t> window_strides, Padding padding,
           int64_t feature_group_count, int64_t batch_group_count,
           const PrecisionConfig* precision_config,
           absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->Conv(lhs, rhs, window_strides, padding,
                             feature_group_count, batch_group_count,
                             precision_config, preferred_element_type);
}

XlaOp ConvWithGeneralPadding(
    const XlaOp lhs, const XlaOp rhs, absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config,
    absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->ConvWithGeneralPadding(
      lhs, rhs, window_strides, padding, feature_group_count, batch_group_count,
      precision_config, preferred_element_type);
}

XlaOp ConvWithGeneralDimensions(
    const XlaOp lhs, const XlaOp rhs, absl::Span<const int64_t> window_strides,
    Padding padding, const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config,
    absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->ConvWithGeneralDimensions(
      lhs, rhs, window_strides, padding, dimension_numbers, feature_group_count,
      batch_group_count, precision_config, preferred_element_type);
}

XlaOp ConvGeneral(const XlaOp lhs, const XlaOp rhs,
                  absl::Span<const int64_t> window_strides,
                  absl::Span<const std::pair<int64_t, int64_t>> padding,
                  const ConvolutionDimensionNumbers& dimension_numbers,
                  int64_t feature_group_count, int64_t batch_group_count,
                  const PrecisionConfig* precision_config,
                  absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->ConvGeneral(
      lhs, rhs, window_strides, padding, dimension_numbers, feature_group_count,
      batch_group_count, precision_config, preferred_element_type);
}

XlaOp ConvGeneralDilated(const XlaOp lhs, const XlaOp rhs,
                         absl::Span<const int64_t> window_strides,
                         absl::Span<const std::pair<int64_t, int64_t>> padding,
                         absl::Span<const int64_t> lhs_dilation,
                         absl::Span<const int64_t> rhs_dilation,
                         const ConvolutionDimensionNumbers& dimension_numbers,
                         int64_t feature_group_count, int64_t batch_group_count,
                         const PrecisionConfig* precision_config,
                         absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->ConvGeneralDilated(
      lhs, rhs, window_strides, padding, lhs_dilation, rhs_dilation,
      dimension_numbers, feature_group_count, batch_group_count,
      precision_config, preferred_element_type);
}

XlaOp DynamicConvInputGrad(
    XlaOp input_sizes, const XlaOp lhs, const XlaOp rhs,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config, PaddingType padding_type,
    absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->DynamicConvInputGrad(
      input_sizes, lhs, rhs, window_strides, padding, lhs_dilation,
      rhs_dilation, dimension_numbers, feature_group_count, batch_group_count,
      precision_config, padding_type, preferred_element_type);
}

XlaOp DynamicConvKernelGrad(
    XlaOp activations, XlaOp gradients,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding,
    absl::Span<const int64_t> lhs_dilation,
    absl::Span<const int64_t> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers,
    int64_t feature_group_count, int64_t batch_group_count,
    const PrecisionConfig* precision_config, PaddingType padding_type,
    absl::optional<PrimitiveType> preferred_element_type) {
  return activations.builder()->DynamicConvKernelGrad(
      activations, gradients, window_strides, padding, lhs_dilation,
      rhs_dilation, dimension_numbers, feature_group_count, batch_group_count,
      precision_config, padding_type, preferred_element_type);
}

XlaOp DynamicConvForward(const XlaOp lhs, const XlaOp rhs,
                         absl::Span<const int64_t> window_strides,
                         absl::Span<const std::pair<int64_t, int64_t>> padding,
                         absl::Span<const int64_t> lhs_dilation,
                         absl::Span<const int64_t> rhs_dilation,
                         const ConvolutionDimensionNumbers& dimension_numbers,
                         int64_t feature_group_count, int64_t batch_group_count,
                         const PrecisionConfig* precision_config,
                         PaddingType padding_type,
                         absl::optional<PrimitiveType> preferred_element_type) {
  return lhs.builder()->DynamicConvForward(
      lhs, rhs, window_strides, padding, lhs_dilation, rhs_dilation,
      dimension_numbers, feature_group_count, batch_group_count,
      precision_config, padding_type, preferred_element_type);
}

XlaOp Fft(const XlaOp operand, FftType fft_type,
          absl::Span<const int64_t> fft_length) {
  return operand.builder()->Fft(operand, fft_type, fft_length);
}

XlaOp TriangularSolve(XlaOp a, XlaOp b, bool left_side, bool lower,
                      bool unit_diagonal,
                      TriangularSolveOptions::Transpose transpose_a) {
  XlaBuilder* builder = a.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* a_shape, builder->GetShapePtr(a));
    TF_ASSIGN_OR_RETURN(const Shape* b_shape, builder->GetShapePtr(b));
    xla::TriangularSolveOptions options;
    options.set_left_side(left_side);
    options.set_lower(lower);
    options.set_unit_diagonal(unit_diagonal);
    options.set_transpose_a(transpose_a);
    TF_ASSIGN_OR_RETURN(Shape shape, ShapeInference::InferTriangularSolveShape(
                                         *a_shape, *b_shape, options));
    return builder->TriangularSolveInternal(shape, a, b, std::move(options));
  });
}

XlaOp Cholesky(XlaOp a, bool lower) {
  XlaBuilder* builder = a.builder();
  return builder->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(const Shape* a_shape, builder->GetShapePtr(a));
    TF_ASSIGN_OR_RETURN(Shape shape,
                        ShapeInference::InferCholeskyShape(*a_shape));
    return builder->CholeskyInternal(shape, a, lower);
  });
}

XlaOp Infeed(XlaBuilder* builder, const Shape& shape,
             const std::string& config) {
  return builder->Infeed(shape, config);
}

void Outfeed(const XlaOp operand, const Shape& shape_with_layout,
             const std::string& outfeed_config) {
  return operand.builder()->Outfeed(operand, shape_with_layout, outfeed_config);
}

XlaOp Call(XlaBuilder* builder, const XlaComputation& computation,
           absl::Span<const XlaOp> operands) {
  return builder->Call(computation, operands);
}

XlaOp CustomCall(
    XlaBuilder* builder, const std::string& call_target_name,
    absl::Span<const XlaOp> operands, const Shape& shape,
    const std::string& opaque, bool has_side_effect,
    absl::Span<const std::pair<ShapeIndex, std::pair<int64_t, ShapeIndex>>>
        output_operand_aliasing,
    const Literal* literal, CustomCallSchedule schedule,
    CustomCallApiVersion api_version) {
  return builder->CustomCall(call_target_name, operands, shape, opaque,
                             /*operand_shapes_with_layout=*/absl::nullopt,
                             has_side_effect, output_operand_aliasing, literal,
                             /*window=*/absl::nullopt, /*dnums=*/absl::nullopt,
                             schedule, api_version);
}

XlaOp CustomCallWithComputation(
    XlaBuilder* builder, const std::string& call_target_name,
    absl::Span<const XlaOp> operands, const XlaComputation& computation,
    const Shape& shape, const std::string& opaque, bool has_side_effect,
    absl::Span<const std::pair<ShapeIndex, std::pair<int64_t, ShapeIndex>>>
        output_operand_aliasing,
    const Literal* literal, CustomCallSchedule schedule,
    CustomCallApiVersion api_version) {
  return builder->CustomCall(
      call_target_name, operands, computation, shape, opaque,
      /*operand_shapes_with_layout=*/absl::nullopt, has_side_effect,
      output_operand_aliasing, literal, schedule, api_version);
}

XlaOp CustomCallWithLayout(
    XlaBuilder* builder, const std::string& call_target_name,
    absl::Span<const XlaOp> operands, const Shape& shape,
    absl::Span<const Shape> operand_shapes_with_layout,
    const std::string& opaque, bool has_side_effect,
    absl::Span<const std::pair<ShapeIndex, std::pair<int64_t, ShapeIndex>>>
        output_operand_aliasing,
    const Literal* literal, CustomCallSchedule schedule,
    CustomCallApiVersion api_version) {
  return builder->CustomCall(
      call_target_name, operands, shape, opaque, operand_shapes_with_layout,
      has_side_effect, output_operand_aliasing, literal,
      /*window=*/absl::nullopt, /*dnums=*/absl::nullopt, schedule, api_version);
}

XlaOp CustomCallWithConvDnums(
    XlaBuilder* builder, const std::string& call_target_name,
    absl::Span<const XlaOp> operands, const Shape& shape,
    absl::Span<const Shape> operand_shapes_with_layout,
    const std::string& opaque, bool has_side_effect,
    absl::Span<const std::pair<ShapeIndex, std::pair<int64_t, ShapeIndex>>>
        output_operand_aliasing,
    const Literal* literal, Window window, ConvolutionDimensionNumbers dnums,
    CustomCallSchedule schedule, CustomCallApiVersion api_version) {
  absl::optional<absl::Span<const Shape>> maybe_operand_shapes;
  if (!operand_shapes_with_layout.empty()) {
    maybe_operand_shapes = operand_shapes_with_layout;
  }
  return builder->CustomCall(call_target_name, operands, shape, opaque,
                             maybe_operand_shapes, has_side_effect,
                             output_operand_aliasing, literal, window, dnums,
                             schedule, api_version);
}

XlaOp OptimizationBarrier(XlaOp operand) {
  return operand.builder()->OptimizationBarrier(operand);
}

XlaOp Complex(const XlaOp lhs, const XlaOp rhs,
              absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kComplex, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Conj(const XlaOp operand) {
  return Complex(Real(operand), Neg(Imag(operand)));
}

XlaOp Add(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kAdd, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Sub(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kSubtract, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Mul(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kMultiply, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Div(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kDivide, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Rem(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kRemainder, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Max(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kMaximum, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Min(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kMinimum, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp And(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kAnd, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Or(const XlaOp lhs, const XlaOp rhs,
         absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kOr, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Xor(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kXor, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Not(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kNot, operand);
}

XlaOp PopulationCount(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kPopulationCount, operand);
}

XlaOp ShiftLeft(const XlaOp lhs, const XlaOp rhs,
                absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kShiftLeft, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp ShiftRightArithmetic(const XlaOp lhs, const XlaOp rhs,
                           absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kShiftRightArithmetic, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp ShiftRightLogical(const XlaOp lhs, const XlaOp rhs,
                        absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kShiftRightLogical, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Reduce(const XlaOp operand, const XlaOp init_value,
             const XlaComputation& computation,
             absl::Span<const int64_t> dimensions_to_reduce) {
  return operand.builder()->Reduce(operand, init_value, computation,
                                   dimensions_to_reduce);
}

// Reduces several arrays simultaneously among the provided dimensions, given
// "computation" as a reduction operator.
XlaOp Reduce(XlaBuilder* builder, absl::Span<const XlaOp> operands,
             absl::Span<const XlaOp> init_values,
             const XlaComputation& computation,
             absl::Span<const int64_t> dimensions_to_reduce) {
  return builder->Reduce(operands, init_values, computation,
                         dimensions_to_reduce);
}

XlaOp ReduceAll(const XlaOp operand, const XlaOp init_value,
                const XlaComputation& computation) {
  return operand.builder()->ReduceAll(operand, init_value, computation);
}

XlaOp ReduceWindow(const XlaOp operand, const XlaOp init_value,
                   const XlaComputation& computation,
                   absl::Span<const int64_t> window_dimensions,
                   absl::Span<const int64_t> window_strides, Padding padding) {
  return operand.builder()->ReduceWindow(operand, init_value, computation,
                                         window_dimensions, window_strides,
                                         padding);
}

XlaOp ReduceWindow(absl::Span<const XlaOp> operands,
                   absl::Span<const XlaOp> init_values,
                   const XlaComputation& computation,
                   absl::Span<const int64_t> window_dimensions,
                   absl::Span<const int64_t> window_strides, Padding padding) {
  CHECK(!operands.empty());
  return operands[0].builder()->ReduceWindow(operands, init_values, computation,
                                             window_dimensions, window_strides,
                                             padding);
}

XlaOp ReduceWindowWithGeneralPadding(
    const XlaOp operand, const XlaOp init_value,
    const XlaComputation& computation,
    absl::Span<const int64_t> window_dimensions,
    absl::Span<const int64_t> window_strides,
    absl::Span<const int64_t> base_dilations,
    absl::Span<const int64_t> window_dilations,
    absl::Span<const std::pair<int64_t, int64_t>> padding) {
  return operand.builder()->ReduceWindowWithGeneralPadding(
      absl::MakeSpan(&operand, 1), absl::MakeSpan(&init_value, 1), computation,
      window_dimensions, window_strides, base_dilations, window_dilations,
      padding);
}

XlaOp ReduceWindowWithGeneralPadding(
    absl::Span<const XlaOp> operands, absl::Span<const XlaOp> init_values,
    const XlaComputation& computation,
    absl::Span<const int64_t> window_dimensions,
    absl::Span<const int64_t> window_strides,
    absl::Span<const int64_t> base_dilations,
    absl::Span<const int64_t> window_dilations,
    absl::Span<const std::pair<int64_t, int64_t>> padding) {
  CHECK(!operands.empty());
  return operands[0].builder()->ReduceWindowWithGeneralPadding(
      operands, init_values, computation, window_dimensions, window_strides,
      base_dilations, window_dilations, padding);
}

XlaOp AllGather(const XlaOp operand, int64_t all_gather_dimension,
                int64_t shard_count,
                absl::Span<const ReplicaGroup> replica_groups,
                const absl::optional<ChannelHandle>& channel_id,
                const absl::optional<Layout>& layout,
                const absl::optional<bool> use_global_device_ids) {
  return operand.builder()->AllGather(operand, all_gather_dimension,
                                      shard_count, replica_groups, channel_id,
                                      layout, use_global_device_ids);
}

XlaOp CrossReplicaSum(const XlaOp operand,
                      absl::Span<const ReplicaGroup> replica_groups) {
  return operand.builder()->CrossReplicaSum(operand, replica_groups);
}

XlaOp AllReduce(const XlaOp operand, const XlaComputation& computation,
                absl::Span<const ReplicaGroup> replica_groups,
                const absl::optional<ChannelHandle>& channel_id,
                const absl::optional<Shape>& shape_with_layout) {
  return operand.builder()->AllReduce(operand, computation, replica_groups,
                                      channel_id, shape_with_layout);
}

XlaOp ReduceScatter(const XlaOp operand, const XlaComputation& computation,
                    int64_t scatter_dimension, int64_t shard_count,
                    absl::Span<const ReplicaGroup> replica_groups,
                    const absl::optional<ChannelHandle>& channel_id,
                    const absl::optional<Layout>& layout,
                    const absl::optional<bool> use_global_device_ids) {
  return operand.builder()->ReduceScatter(
      operand, computation, scatter_dimension, shard_count, replica_groups,
      channel_id, layout, use_global_device_ids);
}

XlaOp AllToAll(const XlaOp operand, int64_t split_dimension,
               int64_t concat_dimension, int64_t split_count,
               absl::Span<const ReplicaGroup> replica_groups,
               const absl::optional<Layout>& layout) {
  return operand.builder()->AllToAll(operand, split_dimension, concat_dimension,
                                     split_count, replica_groups, layout);
}

XlaOp AllToAllTuple(absl::Span<const XlaOp> operands,
                    absl::Span<const ReplicaGroup> replica_groups,
                    const absl::optional<Layout>& layout) {
  CHECK(!operands.empty());
  return operands[0].builder()->AllToAllTuple(operands, replica_groups, layout);
}

XlaOp AllToAllTuple(const XlaOp operand, int64_t split_dimension,
                    int64_t concat_dimension, int64_t split_count,
                    absl::Span<const ReplicaGroup> replica_groups,
                    const absl::optional<Layout>& layout) {
  return operand.builder()->AllToAllTuple(operand, split_dimension,
                                          concat_dimension, split_count,
                                          replica_groups, layout);
}

XlaOp CollectivePermute(
    const XlaOp operand,
    const std::vector<std::pair<int64_t, int64_t>>& source_target_pairs) {
  return operand.builder()->CollectivePermute(operand, source_target_pairs);
}

XlaOp ReplicaId(XlaBuilder* builder) { return builder->ReplicaId(); }

XlaOp SelectAndScatter(const XlaOp operand, const XlaComputation& select,
                       absl::Span<const int64_t> window_dimensions,
                       absl::Span<const int64_t> window_strides,
                       Padding padding, const XlaOp source,
                       const XlaOp init_value, const XlaComputation& scatter) {
  return operand.builder()->SelectAndScatter(operand, select, window_dimensions,
                                             window_strides, padding, source,
                                             init_value, scatter);
}

XlaOp SelectAndScatterWithGeneralPadding(
    const XlaOp operand, const XlaComputation& select,
    absl::Span<const int64_t> window_dimensions,
    absl::Span<const int64_t> window_strides,
    absl::Span<const std::pair<int64_t, int64_t>> padding, const XlaOp source,
    const XlaOp init_value, const XlaComputation& scatter) {
  return operand.builder()->SelectAndScatterWithGeneralPadding(
      operand, select, window_dimensions, window_strides, padding, source,
      init_value, scatter);
}

XlaOp Abs(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kAbs, operand);
}

XlaOp Atan2(const XlaOp lhs, const XlaOp rhs,
            absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kAtan2, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp Exp(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kExp, operand);
}
XlaOp Expm1(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kExpm1, operand);
}
XlaOp Floor(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kFloor, operand);
}
XlaOp Ceil(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kCeil, operand);
}
XlaOp Round(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kRoundNearestAfz, operand);
}
XlaOp Log(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kLog, operand);
}
XlaOp Log1p(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kLog1p, operand);
}
XlaOp Logistic(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kLogistic, operand);
}
XlaOp Sign(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kSign, operand);
}
XlaOp Clz(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kClz, operand);
}
XlaOp Cos(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kCos, operand);
}
XlaOp Sin(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kSin, operand);
}
XlaOp Tanh(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kTanh, operand);
}
XlaOp Real(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kReal, operand);
}
XlaOp Imag(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kImag, operand);
}
XlaOp Sqrt(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kSqrt, operand);
}
XlaOp Cbrt(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kCbrt, operand);
}
XlaOp Rsqrt(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kRsqrt, operand);
}

XlaOp Pow(const XlaOp lhs, const XlaOp rhs,
          absl::Span<const int64_t> broadcast_dimensions) {
  return lhs.builder()->BinaryOp(HloOpcode::kPower, lhs, rhs,
                                 broadcast_dimensions);
}

XlaOp IsFinite(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kIsFinite, operand);
}

XlaOp ConvertElementType(const XlaOp operand, PrimitiveType new_element_type) {
  return operand.builder()->ConvertElementType(operand, new_element_type);
}

XlaOp BitcastConvertType(const XlaOp operand, PrimitiveType new_element_type) {
  return operand.builder()->BitcastConvertType(operand, new_element_type);
}

XlaOp Neg(const XlaOp operand) {
  return operand.builder()->UnaryOp(HloOpcode::kNegate, operand);
}

XlaOp Transpose(const XlaOp operand, absl::Span<const int64_t> permutation) {
  return operand.builder()->Transpose(operand, permutation);
}

XlaOp Rev(const XlaOp operand, absl::Span<const int64_t> dimensions) {
  return operand.builder()->Rev(operand, dimensions);
}

XlaOp Sort(absl::Span<const XlaOp> operands, const XlaComputation& comparator,
           int64_t dimension, bool is_stable) {
  return operands[0].builder()->Sort(operands, comparator, dimension,
                                     is_stable);
}

XlaOp Clamp(const XlaOp min, const XlaOp operand, const XlaOp max) {
  return min.builder()->Clamp(min, operand, max);
}

XlaOp Map(XlaBuilder* builder, absl::Span<const XlaOp> operands,
          const XlaComputation& computation,
          absl::Span<const int64_t> dimensions,
          absl::Span<const XlaOp> static_operands) {
  return builder->Map(operands, computation, dimensions, static_operands);
}

XlaOp RngNormal(const XlaOp mu, const XlaOp sigma, const Shape& shape) {
  return mu.builder()->RngNormal(mu, sigma, shape);
}

XlaOp RngUniform(const XlaOp a, const XlaOp b, const Shape& shape) {
  return a.builder()->RngUniform(a, b, shape);
}

XlaOp RngBitGenerator(RandomAlgorithm algorithm, const XlaOp initial_state,
                      const Shape& shape) {
  return initial_state.builder()->RngBitGenerator(algorithm, initial_state,
                                                  shape);
}

XlaOp While(const XlaComputation& condition, const XlaComputation& body,
            const XlaOp init) {
  return init.builder()->While(condition, body, init);
}

XlaOp Conditional(const XlaOp predicate, const XlaOp true_operand,
                  const XlaComputation& true_computation,
                  const XlaOp false_operand,
                  const XlaComputation& false_computation) {
  return predicate.builder()->Conditional(predicate, true_operand,
                                          true_computation, false_operand,
                                          false_computation);
}

XlaOp Conditional(const XlaOp branch_index,
                  absl::Span<const XlaComputation* const> branch_computations,
                  absl::Span<const XlaOp> branch_operands) {
  return branch_index.builder()->Conditional(branch_index, branch_computations,
                                             branch_operands);
}

XlaOp ReducePrecision(const XlaOp operand, const int exponent_bits,
                      const int mantissa_bits) {
  return operand.builder()->ReducePrecision(operand, exponent_bits,
                                            mantissa_bits);
}

XlaOp Gather(const XlaOp input, const XlaOp start_indices,
             const GatherDimensionNumbers& dimension_numbers,
             absl::Span<const int64_t> slice_sizes, bool indices_are_sorted) {
  return input.builder()->Gather(input, start_indices, dimension_numbers,
                                 slice_sizes, indices_are_sorted);
}

XlaOp Scatter(const XlaOp input, const XlaOp scatter_indices,
              const XlaOp updates, const XlaComputation& update_computation,
              const ScatterDimensionNumbers& dimension_numbers,
              bool indices_are_sorted, bool unique_indices) {
  return input.builder()->Scatter(input, scatter_indices, updates,
                                  update_computation, dimension_numbers,
                                  indices_are_sorted, unique_indices);
}

XlaOp Scatter(absl::Span<const XlaOp> inputs, XlaOp scatter_indices,
              absl::Span<const XlaOp> updates,
              const XlaComputation& update_computation,
              const ScatterDimensionNumbers& dimension_numbers,
              bool indices_are_sorted, bool unique_indices) {
  return scatter_indices.builder()->Scatter(
      inputs, scatter_indices, updates, update_computation, dimension_numbers,
      indices_are_sorted, unique_indices);
}

void Send(const XlaOp operand, const ChannelHandle& handle) {
  return operand.builder()->Send(operand, handle);
}

XlaOp Recv(XlaBuilder* builder, const Shape& shape,
           const ChannelHandle& handle) {
  return builder->Recv(shape, handle);
}

XlaOp SendWithToken(const XlaOp operand, const XlaOp token,
                    const ChannelHandle& handle) {
  return operand.builder()->SendWithToken(operand, token, handle);
}

XlaOp RecvWithToken(const XlaOp token, const Shape& shape,
                    const ChannelHandle& handle) {
  return token.builder()->RecvWithToken(token, shape, handle);
}

XlaOp SendToHost(const XlaOp operand, const XlaOp token,
                 const Shape& shape_with_layout, const ChannelHandle& handle) {
  return operand.builder()->SendToHost(operand, token, shape_with_layout,
                                       handle);
}

XlaOp RecvFromHost(const XlaOp token, const Shape& shape,
                   const ChannelHandle& handle) {
  return token.builder()->RecvFromHost(token, shape, handle);
}

XlaOp InfeedWithToken(const XlaOp token, const Shape& shape,
                      const std::string& config) {
  return token.builder()->InfeedWithToken(token, shape, config);
}

XlaOp OutfeedWithToken(const XlaOp operand, const XlaOp token,
                       const Shape& shape_with_layout,
                       const std::string& outfeed_config) {
  return operand.builder()->OutfeedWithToken(operand, token, shape_with_layout,
                                             outfeed_config);
}

XlaOp CreateToken(XlaBuilder* builder) { return builder->CreateToken(); }

XlaOp AfterAll(XlaBuilder* builder, absl::Span<const XlaOp> tokens) {
  return builder->AfterAll(tokens);
}

XlaOp BatchNormTraining(const XlaOp operand, const XlaOp scale,
                        const XlaOp offset, float epsilon,
                        int64_t feature_index) {
  return operand.builder()->BatchNormTraining(operand, scale, offset, epsilon,
                                              feature_index);
}

XlaOp BatchNormInference(const XlaOp operand, const XlaOp scale,
                         const XlaOp offset, const XlaOp mean,
                         const XlaOp variance, float epsilon,
                         int64_t feature_index) {
  return operand.builder()->BatchNormInference(
      operand, scale, offset, mean, variance, epsilon, feature_index);
}

XlaOp BatchNormGrad(const XlaOp operand, const XlaOp scale,
                    const XlaOp batch_mean, const XlaOp batch_var,
                    const XlaOp grad_output, float epsilon,
                    int64_t feature_index) {
  return operand.builder()->BatchNormGrad(operand, scale, batch_mean, batch_var,
                                          grad_output, epsilon, feature_index);
}

XlaOp Iota(XlaBuilder* builder, PrimitiveType type, int64_t size) {
  return builder->Iota(type, size);
}

XlaOp Iota(XlaBuilder* builder, const Shape& shape, int64_t iota_dimension) {
  return builder->Iota(shape, iota_dimension);
}

XlaOp GetDimensionSize(const XlaOp operand, int64_t dimension) {
  return operand.builder()->GetDimensionSize(operand, dimension);
}

XlaOp SetDimensionSize(const XlaOp operand, const XlaOp val,
                       int64_t dimension) {
  return operand.builder()->SetDimensionSize(operand, val, dimension);
}

XlaOp RemoveDynamicDimension(const XlaOp operand, int64_t dimension) {
  return operand.builder()->RemoveDynamicDimension(operand, dimension);
}

}  // namespace xla
