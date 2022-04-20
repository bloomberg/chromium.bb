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

#include "tensorflow/dtensor/mlir/expansions/save_restore_spmd_expander.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FormatVariadic.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/IR/Matchers.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_attributes.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/collection_ops_util.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/path.h"
#include "tensorflow/dtensor/cc/dstatus.h"
#include "tensorflow/dtensor/cc/save_restore_util.h"
#include "tensorflow/dtensor/cc/tensor_layout.h"
#include "tensorflow/dtensor/mlir/device_utils.h"
#include "tensorflow/dtensor/mlir/ir/tf_dtensor.h"
#include "tensorflow/dtensor/mlir/layout_parsing.h"
#include "tensorflow/dtensor/mlir/op_utils.h"
#include "tensorflow/dtensor/mlir/shape_utils.h"
#include "tensorflow/dtensor/mlir/spmd_expander_common.h"
#include "tensorflow/dtensor/mlir/value_utils.h"

namespace tensorflow {
namespace dtensor {

namespace {
// Maps a device_id to a 0 based switch-case branch index.
//
// For Save/Restore ops, constructing a switch-case on all global devices is not
// going to scale to larger slices as the function grows with the number of
// devices. Instead, we only need to look at devices that are local to the
// current host and generate SPMD for those. This allows the SPMD become
// O(variables) since the local devices are constant for all device types.
//
// The challenge is that the switch-case op branch index is 0 based, meaning
// that we can not use the device_id the same way in the global devices switch.
// To deal with that, we will use this function to map the local_device_id on
// the hosts into a 0 base, by constructing a 1D tensor with all local device
// ids and using the index of the tensor as the branch index.
//
// A concrete example would be:
//
// local_device_ids = [1, 2, 4, 5, 6] -- We shouldn't assume continuity in
// device_ids.
//
// switching device_id = [4]
//
// branch_index = idx_of(local_device_ids) = 2
//
// The tf op equivalent would be:
// tf.reshape(tf.where(tf.equal(local_device_ids, device_id)), ())
mlir::Value DeviceIdToLocalBranchIndex(
    const mlir::Location& location,
    const llvm::ArrayRef<int64_t>& local_device_ids, mlir::Value device_id,
    mlir::OpBuilder& builder) {
  mlir::Value local_device_id_tensors =
      IntConst(builder, location,
               llvm::SmallVector<int32_t>(local_device_ids.begin(),
                                          local_device_ids.end()));
  mlir::Value condition = builder.create<mlir::TF::EqualOp>(
      location, local_device_id_tensors, device_id,
      /*incompatible_shape_error=*/builder.getBoolAttr(true));
  auto where_op = builder.create<mlir::TF::WhereOp>(
      location, mlir::RankedTensorType::get({1, 1}, builder.getI64Type()),
      condition);
  // cast to int32 as where_op returns a int64 array.
  auto cast_op = builder.create<mlir::TF::CastOp>(
      location, mlir::RankedTensorType::get({1, 1}, builder.getI32Type()),
      where_op.getResult());

  // Reshape the output to i32 Scalar.
  auto size_type = mlir::RankedTensorType::get({}, builder.getI32Type());
  mlir::Value scalar_shape = mlir::TF::collection_ops_util::GetR1Const(
      size_type.getShape(), builder, location);
  auto branch_index_scalar = builder.create<mlir::TF::ReshapeOp>(
      location, mlir::ArrayRef<mlir::Type>{size_type},
      mlir::ArrayRef<mlir::Value>{cast_op.getResult(), scalar_shape},
      mlir::ArrayRef<mlir::NamedAttribute>{});

  return branch_index_scalar.getResult();
}

// Builds a switch case function that only conditionally runs save with its
// slice_specs on sharded tensors.
//
// Note that this would generate multiple prefixes for saving rather than the
// single one passed in from the original op.
// DTensor uses DTensorShardedPrefix to query the generated ones and use those
// in MergeV2.
StatusOr<mlir::TF::CaseOp> ConditionalSave(
    mlir::TF::SaveV2Op original_save, const Mesh& mesh,
    const absl::flat_hash_map<
        int64_t, absl::flat_hash_map<int64_t, std::vector<std::string>>>&
        saving_specs) {
  mlir::ModuleOp module = original_save->getParentOfType<mlir::ModuleOp>();
  if (!module)
    return errors::Internal("SaveV2 op isn't enclosed inside a mlir::ModuleOp");

  mlir::SymbolTable symbol_table(module);

  mlir::OpBuilder builder(original_save);
  const auto& location = original_save.getLoc();

  llvm::SmallVector<mlir::func::FuncOp, 8> branch_funs;

  // Try to extract prefix out as constants and build new shard prefix base on
  // it.
  TF_ASSIGN_OR_RETURN(std::string prefix, ExtractConstScalarStringFromValue(
                                              original_save.prefix()));

  // Best effort extraction on shape_and_slices and verify they are empty. If
  // the extraction failed to just ignore those values and work as if those are
  // empty.
  llvm::SmallVector<std::string, 4> original_shape_and_slices;
  const Status extraction_status = ExtractConstStringVectorFromValue(
      original_save.shape_and_slices(), original_shape_and_slices);
  if (extraction_status.ok()) {
    for (const std::string& shape_and_slice : original_shape_and_slices) {
      if (!shape_and_slice.empty())
        return errors::InvalidArgument(
            absl::StrCat("DTensor SaveV2 requires shape_and_slices() field to "
                         "be empty for tensors, but get : ",
                         shape_and_slice));
    }
  } else {
    VLOG(2) << "Failed to extract and verify shape_and_slices() from "
               "original SaveV2 op. SaveV2 SPMD would proceed as if "
               "shape_and_slices are empty for all the tensors.";
  }

  // Branch functions have shared function type, where input is simply all the
  // inputs from origial saveV2 and no outputs.
  auto func_type = mlir::FunctionType::get(builder.getContext(),
                                           original_save.getOperandTypes(),
                                           /*results=*/{});
  // Only generates save functions for devices that is local to the client.
  // This would mean that we will run different functions on different client,
  // but it would be fine as we're running on CPU for this.
  for (int device_id : mesh.local_device_ids()) {
    //  If saving_spec doesn't contain the device_id, then that device_id is a
    //  no-op on the save.
    const auto& it = saving_specs.find(device_id);
    if (it == saving_specs.end()) {
      // Builds place holder for the no_op function, which takes the exact same
      // args as the original save op and returns nothing.
      mlir::func::FuncOp no_op = mlir::func::FuncOp::create(
          location,
          llvm::formatv("{0}_no_op_on_device_{1}_{2}", OpName(original_save),
                        device_id, OpHash(original_save))
              .str(),
          func_type, llvm::ArrayRef<mlir::NamedAttribute>{});
      // Set function visibility to private to indicate that it is only used in
      // this module.
      no_op.setVisibility(mlir::SymbolTable::Visibility::Private);
      symbol_table.insert(no_op);

      mlir::Block* fn_block = no_op.addEntryBlock();
      mlir::OpBuilder fn_builder = mlir::OpBuilder::atBlockBegin(fn_block);
      fn_builder.create<mlir::TF::NoOp>(location);
      fn_builder.create<mlir::func::ReturnOp>(location);

      branch_funs.push_back(no_op);
    } else {
      const absl::flat_hash_map<int64_t, std::vector<std::string>>&
          per_device_specs = it->second;

      // Build the new SaveV2 that contains proper SliceSpec on this device.
      // tensor_names and slice_spec would be concatted into a 1d string tensor.
      mlir::func::FuncOp new_save = mlir::func::FuncOp::create(
          location,
          llvm::formatv("{0}_save_op_on_device_{1}_{2}", OpName(original_save),
                        device_id, OpHash(original_save))
              .str(),
          func_type, llvm::ArrayRef<mlir::NamedAttribute>{});
      // Set function visibility to private to indicate that it is only used in
      // this module.
      new_save.setVisibility(mlir::SymbolTable::Visibility::Private);
      symbol_table.insert(new_save);

      mlir::Block* fn_block = new_save.addEntryBlock();
      mlir::OpBuilder fn_builder = mlir::OpBuilder::atBlockBegin(fn_block);

      mlir::Value tensor_names = new_save.getArgument(1);
      // It is currently unsupported if user passes in shape_and_slices.
      // TODO(hthu): Implement this.
      // mlir::Value shape_and_slices = new_save.getArgument(2);

      // First run a split op on the tensor_names so that we can use the proper
      // splitted output(one of the tensor_name) to reconstruct tensor_names
      // field in the new SaveV2 op.
      TF_ASSIGN_OR_RETURN(
          llvm::ArrayRef<int64_t> tensor_names_shape,
          GetGlobalShapeOfValueFromDTensorLayout(original_save.tensor_names()));
      if (tensor_names_shape.size() != 1)
        return errors::Internal(
            llvm::formatv("SaveV2 op got `tensor_names` with rank {0}) but "
                          "expects rank to be 1.",
                          tensor_names_shape.size())
                .str());
      mlir::TF::SplitOp name_splits;
      TF_RETURN_IF_ERROR(CreateSplitOp(/*num_split=*/tensor_names_shape[0],
                                       /*split_dimension=*/0, location,
                                       /*src_input=*/tensor_names, &fn_builder,
                                       &name_splits));

      // Builds the per device saving spec, that takes care of tensor_name
      // uniqueness requirement. Each save op should use new_tensor_indices and
      // new_specs to map the corresponding saving tensor and its slice spec.
      SaveOpSpecs specs =
          BuildPerDeviceSave(per_device_specs, device_id, prefix);
      const std::vector<std::vector<int>>& new_tensor_indices =
          specs.tensor_indices;
      const std::vector<std::vector<std::string>>& new_specs =
          specs.shape_and_slice_spec;

      // Prepare corresponding SaveOp arguments.
      for (int save_op_index = 0; save_op_index < new_tensor_indices.size();
           ++save_op_index) {
        llvm::SmallVector<mlir::Value, 4> new_tensor_names;
        llvm::SmallVector<std::string, 4> new_shape_and_slices;
        llvm::SmallVector<mlir::Value, 4> new_tensors;

        // Per_device_specs records the index of the tensor_names from the
        // original save, and all slice_specs needed to save that tensor.
        // The corresponding saving tensor can be found in the original save op
        // by adding 3 to the index (as 0, 1, 2) are fixed inputs for prefix,
        // tensor_names and shapes_and_slices.
        for (int i = 0; i < new_tensor_indices[save_op_index].size(); ++i) {
          int tensor_name_index = new_tensor_indices[save_op_index][i];
          int tensor_index = 3 + tensor_name_index;
          new_tensor_names.push_back(name_splits.getResult(tensor_name_index));
          new_shape_and_slices.push_back(new_specs[save_op_index][i]);
          new_tensors.push_back(new_save.getArgument(tensor_index));
        }
        // Build the new SaveV2 op.
        mlir::Value tensor_names = new_tensor_names[0];
        if (new_tensor_names.size() > 1) {
          // For tensor_names that has more than 1 entry, we concat the list of
          // names into a 1d vector.
          tensor_names =
              fn_builder
                  .create<mlir::TF::ConcatOp>(
                      location,
                      /*output=*/original_save.tensor_names().getType(),
                      /*concat_dim=*/
                      IntConst(fn_builder, location, /*values=*/{0}),
                      new_tensor_names)
                  .getResult();
        }

        // Builds a unique prefix for this device and this save_op.
        std::string new_prefix =
            prefix +
            llvm::formatv("_device_{0}_save_op_{1}", device_id, save_op_index)
                .str();

        fn_builder.create<mlir::TF::SaveV2Op>(
            location,
            StringConst(fn_builder, location,
                        {specs.new_prefixes[save_op_index]}),
            /*tensor_name=*/tensor_names,
            /*shape_and_slices=*/
            StringConst(
                fn_builder, location,
                llvm::SmallVector<llvm::StringRef>(new_shape_and_slices.begin(),
                                                   new_shape_and_slices.end())),
            new_tensors);
      }
      branch_funs.push_back(new_save);
      fn_builder.create<mlir::func::ReturnOp>(location);
    }
  }

  llvm::SmallVector<mlir::Attribute, 4> symbols;
  for (auto& func : branch_funs)
    symbols.push_back(mlir::SymbolRefAttr::get(func));

  TF_ASSIGN_OR_RETURN(mlir::Value device_id, DeviceId(original_save));
  llvm::SmallVector<int64_t> local_device_ids(mesh.local_device_ids().begin(),
                                              mesh.local_device_ids().end());
  mlir::Value branch_index = DeviceIdToLocalBranchIndex(
      location, local_device_ids, device_id, builder);

  auto case_op = builder.create<mlir::TF::CaseOp>(
      location,
      // SaveV2 doesn't return a value.
      /*output=*/llvm::ArrayRef<mlir::Type>{},
      /*branch_index=*/branch_index,
      /*input=*/original_save.getOperands(),
      /*branches=*/builder.getArrayAttr(symbols),
      /*is_stateless=*/builder.getBoolAttr(false));

  return case_op;
}

StatusOr<mlir::Operation*> ExpandSaveV2Op(mlir::Operation* op) {
  if (!llvm::isa<mlir::TF::SaveV2Op>(op)) {
    return errors::InvalidArgument(
        llvm::formatv("Expecting SaveV2Op but got {0}", OpName(op)).str());
  }

  TF_ASSIGN_OR_RETURN(Mesh mesh, ExtractDeviceMeshEnclosingCluster(op));
  auto save_v2 = mlir::cast<mlir::TF::SaveV2Op>(op);

  mlir::OpBuilder builder(save_v2);

  absl::flat_hash_map<int64_t, std::pair<std::vector<int64_t>, Layout>>
      tensor_shape_layout_map;
  std::vector<SavingTensorMetadata> metadata;
  for (const auto& it : llvm::enumerate(save_v2.tensors())) {
    mlir::Value tensor = it.value();
    // We use index to select the tensor names and shape_and_slices from the
    // inputs. This is generic regardless whether the inputs are constants or
    // just arguments.
    int index = it.index();
    TF_ASSIGN_OR_RETURN(absl::optional<Layout> layout,
                        ExtractLayoutFromOperand(tensor));
    if (!layout)
      return errors::InvalidArgument(
          "layout is required when saving a DTensor but find no layout "
          "attached");

    TF_ASSIGN_OR_RETURN(llvm::ArrayRef<int64_t> tensor_shape,
                        GetGlobalShapeOfValueFromDTensorLayout(it.value()));

    metadata.push_back(SavingTensorMetadata(
        index, std::vector<int64_t>(tensor_shape.begin(), tensor_shape.end()),
        *layout));
  }
  TF_ASSIGN_OR_RETURN(auto saving_specs, BuildSavingSpec(metadata));

  // Now we have a complete map on device_id and its saving tensors and specs.
  // Build a switch case conditioned on device_id and do saves properly.
  TF_ASSIGN_OR_RETURN(mlir::TF::CaseOp case_op,
                      ConditionalSave(save_v2, mesh, saving_specs));

  save_v2->replaceAllUsesWith(case_op);
  save_v2->erase();

  return case_op.getOperation();
}

// SPMD Expander for MergeV2.
//
// The op is expected to have one and only one of the prefix input, which is
// used as a key to query all the saved shard prefixed generated in SaveV2 op
// SPMD.
//
// The expanded MergeV2 contains all the shard_prefix generated, and only runs
// on Device 0.
StatusOr<mlir::Operation*> ExpandMergeV2Op(mlir::Operation* op) {
  mlir::TF::MergeV2CheckpointsOp merge_v2 =
      mlir::dyn_cast<mlir::TF::MergeV2CheckpointsOp>(op);
  if (!merge_v2) {
    return errors::InvalidArgument(
        llvm::formatv("Expecting MergeV2CheckpointsOp but got {0}", OpName(op))
            .str());
  }

  // Build an if op that only runs MergeV2 on device 0. Note that if condition
  // is tested false when device_id == 0, so that the `then` branch will be
  // no_op while the else branch will be the real MergeV2 op that is on device
  // 0.
  auto module = merge_v2->getParentOfType<mlir::ModuleOp>();
  mlir::SymbolTable symbol_table(module);
  auto location = merge_v2.getLoc();
  mlir::OpBuilder builder(merge_v2);

  auto func_type =
      mlir::FunctionType::get(builder.getContext(), merge_v2.getOperandTypes(),
                              llvm::ArrayRef<mlir::Type>{});
  // Build then_func that is the branch of device_id != 0, which only contains a
  // single NoOp.
  mlir::func::FuncOp then_func = mlir::func::FuncOp::create(
      location,
      llvm::formatv("{0}_then_func_{1}", OpName(merge_v2), OpHash(merge_v2))
          .str(),
      func_type, llvm::ArrayRef<mlir::NamedAttribute>{});
  // Set function visibility to private to indicate that it is only used in
  // this module.
  then_func.setVisibility(mlir::SymbolTable::Visibility::Private);
  mlir::Block* then_fn_block = then_func.addEntryBlock();
  mlir::OpBuilder then_fn_builder =
      mlir::OpBuilder::atBlockBegin(then_fn_block);
  then_fn_builder.create<mlir::TF::NoOp>(location);
  then_fn_builder.create<mlir::func::ReturnOp>(location);

  // Build else_func that is the branch of device_id == 0.
  // The else func is just the original MergeV2 itself.
  mlir::func::FuncOp else_func = mlir::func::FuncOp::create(
      location,
      llvm::formatv("{0}_else_func_{1}", OpName(merge_v2), OpHash(merge_v2))
          .str(),
      func_type, llvm::ArrayRef<mlir::NamedAttribute>{});
  // Set function visibility to private to indicate that it is only used in
  // this module.
  else_func.setVisibility(mlir::SymbolTable::Visibility::Private);

  mlir::Block* else_fn_block = else_func.addEntryBlock();
  mlir::OpBuilder else_fn_builder =
      mlir::OpBuilder::atBlockBegin(else_fn_block);
  mlir::Value checkpoint_prefixes = else_fn_block->getArgument(0);
  mlir::Value destination_prefixes = else_fn_block->getArgument(1);

  else_fn_builder.create<mlir::TF::MergeV2CheckpointsOp>(
      location, checkpoint_prefixes, destination_prefixes,
      merge_v2.delete_old_dirs());

  else_fn_builder.create<mlir::func::ReturnOp>(location);

  symbol_table.insert(then_func);
  symbol_table.insert(else_func);

  TF_ASSIGN_OR_RETURN(mlir::Value device_id, DeviceId(merge_v2));
  auto if_op = builder.create<mlir::TF::IfOp>(
      location, then_func.getFunctionType().getResults(), /*cond=*/device_id,
      /*input=*/merge_v2.getOperands(),
      /*then_branch=*/then_func.getSymName(),
      /*else_branch=*/else_func.getSymName(), /*is_stateless=*/false);

  merge_v2->replaceAllUsesWith(if_op);
  merge_v2.erase();
  return if_op.getOperation();
}

// SPMD Expander for RestoreV2 op.
//
// The original RestoreV2 op will be updated to only restore the slice for the
// given device_id. For replicated tensors, that would be the full tensor slice.
// For sharded tensors, we compute its slice using device coordinates and tensor
// layout.
StatusOr<mlir::Operation*> ExpandRestoreV2Op(mlir::Operation* op) {
  // Stock RestoreV2 is pass through as those saveables runs on non-dtensor
  // device. The restore operation for them is as-is.
  // TODO(hthu): Figure out better placement for things like SaveCounters.
  if (llvm::isa<mlir::TF::RestoreV2Op>(op)) return op;

  mlir::TF::DTensorRestoreV2Op restore_v2 =
      mlir::dyn_cast<mlir::TF::DTensorRestoreV2Op>(op);
  if (!restore_v2) {
    return errors::InvalidArgument(
        llvm::formatv("Expecting DTensorRestoreV2Op but got {0}", OpName(op))
            .str());
  }

  // Infer local shape as output type relies on it.
  // Most of the time the inference is no-op as at this point, out shape are
  // unknown until we actually load it back.
  //
  // It is possible to recover the shape and update to local shape using similar
  // mechanism as finding AssignVariableOp, but it seems uncessary for now.
  restore_v2 = mlir::cast<mlir::TF::DTensorRestoreV2Op>(
      InferSPMDExpandedLocalShape(restore_v2));

  TF_ASSIGN_OR_RETURN(Mesh mesh, ExtractDeviceMeshEnclosingCluster(restore_v2));

  // Prepare for building CaseOp.
  mlir::ModuleOp module = restore_v2->getParentOfType<mlir::ModuleOp>();
  if (!module)
    return errors::Internal(
        "DTensorRestoreV2 op isn't enclosed inside a mlir::ModuleOp");

  mlir::SymbolTable symbol_table(module);

  mlir::OpBuilder builder(restore_v2);
  const auto& location = restore_v2.getLoc();

  // Tracks case branch functions for each local_device_id.
  llvm::SmallVector<mlir::func::FuncOp> branch_funcs(
      mesh.local_device_ids().size());
  // Stores restore ops for each device_id in a function, that is suitable for
  // feeding into a CaseOp.

  // Branch functions have shared function type as original restore_v2.
  auto func_type = mlir::FunctionType::get(builder.getContext(),
                                           restore_v2.getOperandTypes(),
                                           restore_v2.getResultTypes());

  llvm::SmallVector<std::string, 4> new_shape_and_slices(
      restore_v2.getNumResults(), "");
  for (int local_device_idx = 0;
       local_device_idx < mesh.local_device_ids().size(); ++local_device_idx) {
    int device_id = mesh.local_device_ids()[local_device_idx];
    TF_ASSIGN_OR_RETURN(const DeviceLocation& coords,
                        mesh.device_location(device_id));

    llvm::SmallVector<std::string> new_shapes_and_slices(
        restore_v2.tensors().size());

    mlir::ArrayAttr input_shapes_attr =
        restore_v2->getAttrOfType<mlir::ArrayAttr>("input_shapes");
    if (!input_shapes_attr) {
      return errors::InvalidArgument(
          "DTensorRestoreV2Op requires input_shapes attributes.");
    }

    llvm::SmallVector<llvm::SmallVector<int64_t, 4>, 4> input_shapes;
    input_shapes.reserve(input_shapes_attr.size());
    for (const auto& shape : input_shapes_attr) {
      mlir::TF::ShapeAttr shape_attr = shape.cast<mlir::TF::ShapeAttr>();
      if (!shape_attr.hasStaticShape()) {
        return errors::InvalidArgument(
            llvm::formatv("DTensorRestoreV2Op requires statically known input "
                          "shape, but got non-static shape: {0}.",
                          shape_attr)
                .str());
      }
      input_shapes.push_back(llvm::SmallVector<int64_t, 4>(
          shape_attr.getShape().begin(), shape_attr.getShape().end()));
    }

    mlir::ArrayAttr input_layouts_attr = restore_v2.input_layouts();
    if (!input_layouts_attr) {
      return errors::InvalidArgument(
          "DTensorRestoreV2Op requires input_layouts attributes.");
    }
    llvm::SmallVector<std::string> input_layouts;
    input_layouts.reserve(input_layouts_attr.size());
    for (const auto& layout : input_layouts_attr.getValue().vec()) {
      input_layouts.push_back(layout.cast<mlir::StringAttr>().getValue().str());
    }

    // For each tensor, build its restore shape_and_slice.
    for (const auto& it :
         llvm::enumerate(llvm::zip(input_shapes, input_layouts))) {
      llvm::ArrayRef<int64_t> global_shape = std::get<0>(it.value());
      TF_ASSIGN_OR_RETURN(Layout layout,
                          Layout::FromString(std::get<1>(it.value())));

      // Fully replicated tensor does not need a slice and spec field and we
      // simply leave it as empty string. Note that Non-DTensor restore will
      // use replicated layout from SaveSpec.
      if (layout.IsFullyReplicated()) {
        new_shape_and_slices[it.index()] = "";
        continue;
      }

      TF_ASSIGN_OR_RETURN(
          std::vector<std::string> slice_specs,
          SliceSpecOnDevice(layout, mesh, coords, global_shape));

      // Concat shape and slice specs
      new_shape_and_slices[it.index()] =
          llvm::formatv("{0} {1}", absl::StrJoin(global_shape, " "),
                        absl::StrJoin(slice_specs, ":"))
              .str();
    }

    // Builds the restore op on device_id.
    mlir::OpBuilder builder(op);
    restore_v2.shape_and_slicesMutable().assign(StringConst(
        builder, restore_v2.getLoc(),
        llvm::SmallVector<llvm::StringRef>(new_shape_and_slices.begin(),
                                           new_shape_and_slices.end())));
    mlir::func::FuncOp device_restore_fn = mlir::func::FuncOp::create(
        location,
        llvm::formatv("{0}_on_device_{1}_{2}", OpName(restore_v2), device_id,
                      OpHash(restore_v2))
            .str(),
        func_type, llvm::ArrayRef<mlir::NamedAttribute>{});
    // Set function visibility to private to indicate that it is only used in
    // this module.
    device_restore_fn.setVisibility(mlir::SymbolTable::Visibility::Private);
    symbol_table.insert(device_restore_fn);

    mlir::Block* fn_block = device_restore_fn.addEntryBlock();
    mlir::OpBuilder fn_builder = mlir::OpBuilder::atBlockBegin(fn_block);
    mlir::Value prefix = device_restore_fn.getArgument(0);
    mlir::Value tensor_names = device_restore_fn.getArgument(1);
    // Constructs shapes and slices ourselves while reusing all other
    // arguments.
    auto new_restore_v2 = fn_builder.create<mlir::TF::RestoreV2Op>(
        location, restore_v2.getResultTypes(), prefix, tensor_names,
        StringConst(
            fn_builder, location,
            llvm::SmallVector<llvm::StringRef>(new_shape_and_slices.begin(),
                                               new_shape_and_slices.end())));
    fn_builder.create<mlir::func::ReturnOp>(location,
                                            new_restore_v2.getResults());

    branch_funcs[local_device_idx] = device_restore_fn;
  }

  // Builds the final case op.
  llvm::SmallVector<mlir::Attribute, 4> symbols;
  for (auto& func : branch_funcs)
    symbols.push_back(mlir::SymbolRefAttr::get(func));

  TF_ASSIGN_OR_RETURN(mlir::Value device_id, DeviceId(restore_v2));
  llvm::SmallVector<int64_t> local_device_ids(mesh.local_device_ids().begin(),
                                              mesh.local_device_ids().end());
  mlir::Value branch_index = DeviceIdToLocalBranchIndex(
      location, local_device_ids, device_id, builder);

  auto case_op = builder.create<mlir::TF::CaseOp>(
      location,
      /*output=*/restore_v2.getResultTypes(),
      /*branch_index=*/branch_index,
      /*input=*/restore_v2.getOperands(),
      /*branches=*/builder.getArrayAttr(symbols),
      /*is_stateless=*/builder.getBoolAttr(false));

  restore_v2.replaceAllUsesWith(case_op);
  restore_v2.erase();

  return case_op.getOperation();
}

}  // namespace

StatusOr<mlir::Operation*> SaveRestoreSPMDExpander::ExpandOp(
    mlir::Operation* op) {
  if (llvm::isa<mlir::TF::SaveV2Op>(op)) {
    return ExpandSaveV2Op(op);
  }
  if (llvm::isa<mlir::TF::MergeV2CheckpointsOp>(op)) {
    return ExpandMergeV2Op(op);
  }
  if (llvm::isa<mlir::TF::RestoreV2Op, mlir::TF::DTensorRestoreV2Op>(op)) {
    return ExpandRestoreV2Op(op);
  }

  return errors::Unimplemented(
      llvm::formatv("SPMD for op : {0} is not implemented ", OpName(op)).str());
}

StatusOr<llvm::DenseMap<int, Layout>>
SaveRestoreSPMDExpander::ComputeLayoutForward(
    mlir::Operation* op, const llvm::DenseMap<int, Layout>& input_layouts) {
  // Save op doesn't have return values.
  if (llvm::isa<mlir::TF::SaveV2Op, mlir::TF::MergeV2CheckpointsOp>(op)) {
    return llvm::DenseMap<int, Layout>();
  }
  if (llvm::isa<mlir::TF::RestoreV2Op>(op)) {
    // Normal RestoreV2 is generatred for objects that is not placed on DTensor,
    // such as SaveCounters, etc.
    // For those, we simply set a stub layout and pass through.
    // Ideally, those ops shouldn't even run on DTensor Device.
    mlir::TF::RestoreV2Op restore_v2 = mlir::cast<mlir::TF::RestoreV2Op>(op);
    llvm::DenseMap<int, Layout> output_layouts(restore_v2.getNumResults());
    for (const auto& it : llvm::enumerate(restore_v2.getResults())) {
      // TODO(hthu): Maybe stub proper replicated with rank if we know the
      // output.
      output_layouts[it.index()] = Layout::Empty();
    }
    return output_layouts;
  }
  if (llvm::isa<mlir::TF::DTensorRestoreV2Op>(op)) {
    mlir::TF::DTensorRestoreV2Op restore_v2 =
        mlir::cast<mlir::TF::DTensorRestoreV2Op>(op);
    llvm::DenseMap<int, Layout> output_layouts(restore_v2.getNumResults());
    // Output layout is simply the layout from the arguments.
    for (const auto& it : llvm::enumerate(restore_v2.input_layouts())) {
      TF_ASSIGN_OR_RETURN(
          Layout layout,
          Layout::FromString(
              it.value().cast<mlir::StringAttr>().getValue().str()));
      output_layouts[it.index()] = layout;
    }
    return output_layouts;
  }
  return errors::Unimplemented(
      llvm::formatv("Layout propagation for op : {0} is not implemented",
                    OpName(op))
          .str());
}

StatusOr<llvm::DenseMap<int, Layout>>
SaveRestoreSPMDExpander::ComputeLayoutBackward(
    mlir::Operation* op, const llvm::DenseMap<int, Layout>& output_layouts) {
  return llvm::DenseMap<int, Layout>();
}

}  // namespace dtensor
}  // namespace tensorflow
