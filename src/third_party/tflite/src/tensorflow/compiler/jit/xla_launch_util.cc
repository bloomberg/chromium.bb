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

#include "tensorflow/compiler/jit/xla_launch_util.h"

#include <memory>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "tensorflow/compiler/jit/defs.h"
#include "tensorflow/compiler/tf2xla/shape_util.h"
#include "tensorflow/compiler/tf2xla/xla_compiler.h"
#include "tensorflow/compiler/xla/client/client_library.h"
#include "tensorflow/compiler/xla/client/local_client.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/core/common_runtime/dma_helper.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/common_runtime/gpu_device_context.h"
#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/resource_mgr.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/refcount.h"
#include "tensorflow/core/util/stream_executor_util.h"

namespace tensorflow {
namespace {
using xla::ScopedShapedBuffer;
using xla::ShapedBuffer;

const char kPossibleNonVariableResourceHintMessage[] =
    "If the error is similar to `Trying to access resource using the wrong "
    "type`, this is likely because XLA only accepts Resource Variables as "
    "inputs by snapshotting their values. Other TensorFlow resource types like "
    "TensorList/TensorArray/Stack are not supported. Try removing non-variable "
    "resource inputs to XLA.";
}  // anonymous namespace

VariableInfo::VariableInfo(int index, absl::string_view name, Var* var)
    : index_(index), name_(name), var_(var) {}
VariableInfo::VariableInfo(VariableInfo&& other)
    : index_(other.index_), var_(other.var_), lock_held_(other.lock_held_) {
  other.index_ = -1;
  other.var_ = nullptr;
}

VariableInfo& VariableInfo::operator=(VariableInfo&& other) {
  index_ = other.index_;
  var_ = other.var_;
  lock_held_ = other.lock_held_;

  other.index_ = -1;
  other.var_ = nullptr;

  return *this;
}

VariableInfo::~VariableInfo() {
  // Release the variable's lock if we hold it. Ensures that the lock is
  // released even on error.  It does not matter in what order we release the
  // locks.
  if (var()) {
    if (lock_held()) {
      var()->mu()->unlock();
    }

    // Unref the variable so it can be released by ResourceManager.
    var()->Unref();
  }
}

// Returns a vector of VariableInfo instances for the resource variable inputs
// to the kernel with context `ctx`.  The input indices for the resource
// variable inputs are in `variable_indices`.
Status GetVariableInfosFromCtxInputs(OpKernelContext* ctx,
                                     absl::Span<const int> variable_indices,
                                     std::vector<VariableInfo>* result) {
  std::vector<const ResourceHandle*> resource_handles;
  absl::c_transform(
      variable_indices, std::back_inserter(resource_handles),
      [&](int variable_idx) { return &HandleFromInput(ctx, variable_idx); });

  std::vector<core::RefCountPtr<Var>> variables;
  Status s = LookupResources(ctx, resource_handles, &variables);
  if (!s.ok()) {
    errors::AppendToMessage(&s, kPossibleNonVariableResourceHintMessage);
    return s;
  }

  result->clear();
  result->reserve(variable_indices.size());
  for (int i = 0; i < variable_indices.size(); i++) {
    // *Release* the variable because we're going to unref it later in
    // ~VariableInfo.
    Var* variable = variables[i].release();
    int input_idx = variable_indices[i];
    std::string var_name = HandleFromInput(ctx, input_idx).name();
    result->emplace_back(input_idx, var_name, variable);
  }

  return Status::OK();
}

Status LockVariables(absl::Span<VariableInfo> variables) {
  std::vector<int> lock_order(variables.size());
  std::iota(lock_order.begin(), lock_order.end(), 0);

  // VariableInfoComparator orders all empty VariableInfo instances as
  // equivalent so it looks like we may want to stable sort these to maintain a
  // deterministic order between the empty VariableInfo instances.  However
  // since we're sorting by pointer value the sort is pretty non-deterministic
  // anyway so we don't bother using std::stable_sort for now.
  absl::c_sort(lock_order, [&](int a, int b) {
    if (variables[a].var() && variables[b].var()) {
      return variables[a].var()->mu() < variables[b].var()->mu();
    }

    // Move all the empty VariableInfo instances to the end.
    return variables[a].var() != nullptr;
  });

  mutex* prev = nullptr;
  for (int i : lock_order) {
    Var* variable = variables[i].var();
    if (variable == nullptr) {
      // All empty VariableInfo instances are at the end of the order
      // so we're done.
      break;
    }
    mutex* mu = variable->mu();
    if (prev == mu) {
      // It is an error to pass the same variable handle twice to the same XLA
      // cluster because we would not handle variable updates correctly.  Any
      // locks we have already acquired will be released when the VariableInfo
      // objects are destroyed.
      // TODO(b/128495870) Add support for passing aliased resource variables.
      return errors::Unimplemented("Duplicate variable passed to XLA cluster");
    }
    VLOG(4) << "Acquiring lock for variable "
            << reinterpret_cast<void*>(variable);
    mu->lock();
    variables[i].set_lock_held();
    prev = mu;
  }
  VLOG(4) << "Finished acquiring variable locks.";
  return Status::OK();
}

Status SnapshotResourceVariables(OpKernelContext* ctx,
                                 absl::Span<const int> variable_indices,
                                 absl::Span<VariableInfo const> variable_infos,
                                 ResourceVarsSnapshot* result) {
  for (int i = 0; i < variable_indices.size(); i++) {
    Var* var = variable_infos[i].var();
    (*result)[variable_indices[i]] =
        var ? absl::make_optional(*var->tensor()) : absl::nullopt;
  }
  return Status::OK();
}

XlaComputationLaunchContext::XlaComputationLaunchContext(
    xla::LocalClient* client, se::DeviceMemoryAllocator* xla_allocator,
    bool allocate_xla_tensors, bool use_multiple_streams)
    : client_(client),
      xla_allocator_(xla_allocator),
      allocate_xla_tensors_(allocate_xla_tensors),
      use_multiple_streams_(use_multiple_streams) {
  if (use_multiple_streams_) {
    CHECK(allocate_xla_tensors_) << "To use multiple streams correctly we must "
                                    "be allocating XLA tensors!";
  }
}

void XlaComputationLaunchContext::PopulateInputs(
    OpKernelContext* ctx,
    const XlaCompiler::CompilationResult* compilation_result,
    const ResourceVarsSnapshot& variables, int missing_ctx_input_prefix) {
  // Build ShapedBuffers that point directly to the Tensor buffers.
  arg_ptrs_ =
      std::vector<ShapedBuffer*>(compilation_result->xla_input_shapes.size());

  xla::TransferManager* transfer_manager =
      client_->backend().transfer_manager();
  for (int i = 0; i < compilation_result->xla_input_shapes.size(); ++i) {
    int arg_num = compilation_result->input_mapping[i];
    CHECK_GE(arg_num, missing_ctx_input_prefix);
    const xla::Shape& shape = compilation_result->xla_input_shapes[i];
    const Tensor* t = variables.count(arg_num)
                          ? &(variables.at(arg_num).value())
                          : &(ctx->input(arg_num - missing_ctx_input_prefix));
    CHECK(t);

    if (use_multiple_streams_) {
      CHECK(ctx->op_device_context() && ctx->op_device_context()->stream())
          << "Must have a stream available when using XLA tensors!";
      XlaTensor* xla_tensor = XlaTensor::FromTensor(t);
      CHECK(xla_tensor);
      xla_tensor->WaitForDefinitionEventOnStream(
          ctx->op_device_context()->stream());
    }

    if (xla::Shape::Equal().MinorToMajorOnlyInLayout()(
            shape, transfer_manager->HostShapeToDeviceShape(shape))) {
      se::DeviceMemoryBase dmem = XlaTensor::DeviceMemoryFromTensor(*t);
      arg_buffers_.emplace_back(
          /*on_host_shape=*/shape, /*on_device_shape=*/shape,
          client_->platform(), client_->default_device_ordinal());
      arg_buffers_.back().set_buffer(dmem, /*index=*/{});
      arg_ptrs_[i] = &arg_buffers_.back();
    } else {
      const XlaTensor* xla_tensor = XlaTensor::FromTensor(t);
      CHECK(xla_tensor && xla_tensor->has_shaped_buffer());
      arg_ptrs_[i] = const_cast<ShapedBuffer*>(&xla_tensor->shaped_buffer());
    }
  }
}

static bool MustAliasOutput(
    const xla::HloInputOutputAliasConfig& input_output_alias, int output_num) {
  xla::ShapeIndex output_index;
  if (input_output_alias.shape().IsTuple()) {
    output_index = {output_num};
  } else {
    DCHECK_EQ(output_num, 0)
        << "output_num must be 0 for non-tuple shapes but is " << output_num;
    output_index = {};
  }
  if (input_output_alias.shape().tuple_shapes_size() == 0) {
    return false;
  }
  return input_output_alias.OutputHasAlias(output_index) &&
         input_output_alias.GetAliasedParameter(output_index).value().kind ==
             xla::HloInputOutputAliasConfig::kUserAlias;
}

// Returns an aliased tensor if it exists, nullptr otherwise.
static const Tensor* FindAliasedTensorForOutput(
    int output_num, OpKernelContext* ctx, int missing_ctx_input_prefix,
    const xla::HloInputOutputAliasConfig& input_output_alias,
    absl::Span<const int> input_mapping,
    const ResourceVarsSnapshot& resource_var_snapshots) {
  if (MustAliasOutput(input_output_alias, output_num)) {
    int xla_param = input_output_alias.GetAliasedParameter({output_num})
                        .value()
                        .parameter_number;
    int tf_param = input_mapping[xla_param] - missing_ctx_input_prefix;
    const Tensor* input_tensor = &ctx->input(tf_param);

    // If input tensor is a resource variable, alias to the snapshot we took at
    // entry time.
    if (input_tensor->dtype() == DT_RESOURCE) {
      auto& v = resource_var_snapshots.at(missing_ctx_input_prefix + tf_param);
      CHECK(v.has_value());
      return &v.value();
    }
    return input_tensor;
  }
  return nullptr;
}

// Construct the tensor for given type and buffer.
static Tensor MakeTensor(DataType dtype, const TensorShape& shape,
                         se::DeviceMemoryBase buffer, Allocator* allocator) {
  size_t expected_size = shape.num_elements() * DataTypeSize(dtype);
  auto* tensor_buffer = new XlaTensorBuffer(buffer.opaque(), expected_size,
                                            buffer.size(), allocator);
  Tensor t(dtype, shape, tensor_buffer);
  tensor_buffer->Unref();
  return t;
}

// Get aliased tensor, or make a new one for the corresponding output operation.
static Tensor GetOrCreateTensorForOutput(
    int output_num, OpKernelContext* ctx, int missing_ctx_input_prefix,
    const xla::HloInputOutputAliasConfig& input_output_alias,
    absl::Span<const int> input_mapping,
    const ResourceVarsSnapshot& resource_var_snapshots, DataType output_dtype,
    const TensorShape& output_shape, se::DeviceMemoryBase output_buffer,
    Allocator* output_allocator) {
  if (const Tensor* aliased_tensor = FindAliasedTensorForOutput(
          output_num, ctx, missing_ctx_input_prefix, input_output_alias,
          input_mapping, resource_var_snapshots)) {
    return *aliased_tensor;
  }
  return MakeTensor(output_dtype, output_shape, output_buffer,
                    output_allocator);
}

static Status SetBufferForTensorUnderAllocateXlaTensors(
    const xla::HloInputOutputAliasConfig& input_output_alias, int output_num,
    OpKernelContext* ctx, int i, tensorflow::TensorShape shape,
    xla::ScopedShapedBuffer* output,
    std::shared_ptr<se::Event> definition_event, se::Stream* stream,
    bool use_multiple_streams) {
  if (MustAliasOutput(input_output_alias, output_num)) {
    return errors::Unimplemented(
        "Aliasing is not yet supported for allocate_xla_tensors_.");
  }
  Tensor* output_tensor;
  TF_RETURN_IF_ERROR(ctx->allocate_output(i, shape, &output_tensor));
  XlaTensor* xla_tensor = XlaTensor::FromTensor(output_tensor);
  if (xla_tensor) {
    xla_tensor->set_shaped_buffer(output->TakeSubTree({output_num}));
    if (use_multiple_streams) {
      xla_tensor->ResetDefinitionEvent(definition_event, stream);
    }
  } else {
    // xla_tensor wasn't valid, which must mean this is a zero-element
    // tensor.
    CHECK_EQ(output_tensor->TotalBytes(), 0);
  }
  return Status::OK();
}

static Status SetBufferForResourceVarTensorUnderAllocateXlaTensors(
    const xla::HloInputOutputAliasConfig& input_output_alias, int output_num,
    OpKernelContext* ctx, int i, const XlaCompiler::ResourceUpdate& write,
    xla::ScopedShapedBuffer* output,
    std::shared_ptr<se::Event> definition_event,
    absl::Span<const VariableInfo> variable_infos, se::Stream* stream,
    bool use_multiple_streams) {
  if (MustAliasOutput(input_output_alias, output_num)) {
    return errors::Unimplemented(
        "Aliasing is not yet supported for allocate_xla_tensors_.");
  }
  Tensor output_tensor;
  TF_RETURN_IF_ERROR(
      ctx->allocate_temp(write.type, write.shape, &output_tensor));
  if (write.shape.num_elements() > 0) {
    XlaTensor* xla_tensor = XlaTensor::FromTensor(&output_tensor);
    CHECK(xla_tensor);
    xla_tensor->set_shaped_buffer(output->TakeSubTree({output_num}));
    if (use_multiple_streams) {
      xla_tensor->ResetDefinitionEvent(definition_event, stream);
    }
  }
  *variable_infos[i].var()->tensor() = output_tensor;
  variable_infos[i].var()->is_initialized |= write.modified;
  return Status::OK();
}

// Sets output `output_num` for `ctx` provided it is known at a compile time.
static Status SetOutputForConstant(
    OpKernelContext* ctx, se::Stream* stream,
    const XlaCompiler::CompilationResult* compilation_result, int output_num) {
  CHECK(compilation_result->outputs[output_num].is_constant);
  // Output is a constant.
  const Tensor& const_tensor =
      compilation_result->outputs[output_num].constant_value;
  Tensor* output_tensor;
  const size_t total_bytes = const_tensor.TotalBytes();
  if (stream && total_bytes > 0) {
    // Copy host -> device. (Empty tensors don't have backing buffers.)
    // Manually allocate memory using an XlaTensorBuffer so we can allocate
    // as much memory as the device requires (as given by
    // GetByteSizeRequirement). This avoids XlaTransferManager having to
    // reallocate the device buffer later.
    VLOG(1) << "Constant output tensor on device";

    TF_RETURN_IF_ERROR(
        ctx->allocate_output(output_num, const_tensor.shape(), &output_tensor));
    Device* device = dynamic_cast<Device*>(ctx->device());
    if (device == nullptr) {
      return errors::Internal("DeviceBase was not a Device.");
    }
    ctx->op_device_context()->CopyCPUTensorToDevice(
        &const_tensor, device, output_tensor,
        [&](Status status) { TF_CHECK_OK(status); });

    if (device->device_type() == DEVICE_GPU) {
      // The GPUDeviceContext enqueues the host->device transfer in a
      // separate stream from the main compute stream. We must ensure the
      // compute stream is synchronized with the host->device transfer
      // stream now otherwise we will create a race condition.
      auto* gpu_device_context =
          static_cast<GPUDeviceContext*>(ctx->op_device_context());
      gpu_device_context->stream()->ThenWaitFor(
          gpu_device_context->host_to_device_stream());
    }
  } else {
    // No copy required.
    ctx->set_output(output_num, const_tensor);
    output_tensor = ctx->mutable_output(output_num);
  }
  if (XlaTensor* xla_tensor = XlaTensor::FromTensor(output_tensor)) {
    xla_tensor->set_host_tensor(const_tensor);
  }
  return Status::OK();
}

// Creates a list of updates resource variables.
static xla::StatusOr<std::vector<VariableInfo>> GatherVariableInfo(
    OpKernelContext* ctx,
    const XlaCompiler::CompilationResult* compilation_result,
    int missing_ctx_input_prefix) {
  std::vector<VariableInfo> variable_infos;
  variable_infos.reserve(compilation_result->resource_updates.size());

  for (int i = 0; i < compilation_result->resource_updates.size(); ++i) {
    const XlaCompiler::ResourceUpdate& write =
        compilation_result->resource_updates[i];
    int actual_input_index = write.input_index - missing_ctx_input_prefix;
    if (actual_input_index < 0 || actual_input_index >= ctx->num_inputs()) {
      return errors::Internal("Invalid input index for variable write.");
    }

    // TODO(b/35625933): tensorflow::Var should contain a PersistentTensor,
    // not a Tensor.
    Var* variable = nullptr;
    const ResourceHandle handle = HandleFromInput(ctx, actual_input_index);
    TF_RETURN_IF_ERROR(LookupOrCreateResource<Var>(ctx, handle, &variable,
                                                   [&write](Var** ptr) {
                                                     *ptr = new Var(write.type);
                                                     return Status::OK();
                                                   }));
    variable_infos.emplace_back(actual_input_index, handle.name(), variable);
  }
  return variable_infos;
}

Status XlaComputationLaunchContext::PopulateOutputs(
    OpKernelContext* ctx,
    const XlaCompiler::CompilationResult* compilation_result,
    ScopedShapedBuffer output, int missing_ctx_input_prefix,
    const xla::HloInputOutputAliasConfig& input_output_alias,
    const ResourceVarsSnapshot& resource_var_snapshots) {
  se::Stream* stream =
      ctx->op_device_context() ? ctx->op_device_context()->stream() : nullptr;
  Allocator* allocator = ctx->device()->GetAllocator({});

  // Computation output should always be a tuple.
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "Result tuple shape: " << output.on_host_shape().DebugString();
    VLOG(2) << "Result tuple shape (on device): "
            << output.on_device_shape().DebugString();
  }
  CHECK_EQ(ctx->num_outputs(), compilation_result->outputs.size());

  // If the on-host-shape isn't a tuple, create a new single-element tuple
  // buffer with a nullptr root index table. This allows the code below to treat
  // output as a tuple unconditionally.
  if (!output.on_host_shape().IsTuple()) {
    ShapedBuffer nontuple_buffer = output.release();
    ShapedBuffer buffer(
        xla::ShapeUtil::MakeTupleShape({nontuple_buffer.on_host_shape()}),
        xla::ShapeUtil::MakeTupleShape({nontuple_buffer.on_device_shape()}),
        output.platform(), output.device_ordinal());
    buffer.buffers().CopySubtreeFrom(nontuple_buffer.buffers(),
                                     /*source_base_index=*/{},
                                     /*target_base_index=*/{0});
    output = ScopedShapedBuffer(std::move(buffer), output.memory_allocator());
  }

  std::shared_ptr<se::Event> definition_event;
  if (use_multiple_streams_) {
    definition_event = std::make_shared<se::Event>(stream->parent());
    if (!definition_event->Init()) {
      return errors::Internal("Failed to initialize tensor definition event.");
    }
    stream->ThenRecordEvent(definition_event.get());
  }

  std::vector<TensorShape> output_tensor_shapes;
  output_tensor_shapes.reserve(ctx->num_outputs());
  if (output.on_host_shape().is_dynamic()) {
    TF_ASSIGN_OR_RETURN(
        auto transfer_manager,
        xla::TransferManager::GetForPlatform(stream->parent()->platform()));

    xla::Shape output_host_shape = output.on_host_shape();
    xla::Shape output_device_shape = output.on_device_shape();
    TF_RETURN_IF_ERROR(transfer_manager->ReadDynamicShapes(
        stream, &output, &output_host_shape, &output_device_shape));

    output.set_shapes(output_host_shape, output_device_shape);
    for (int i = 0; i < ctx->num_outputs(); ++i) {
      const xla::Shape& subshape =
          xla::ShapeUtil::GetSubshape(output_host_shape, {i});
      TensorShape shape;
      TF_RETURN_IF_ERROR(XLAShapeToTensorShape(subshape, &shape));
      output_tensor_shapes.push_back(shape);
    }
  } else {
    for (int i = 0; i < ctx->num_outputs(); ++i) {
      output_tensor_shapes.push_back(compilation_result->outputs[i].shape);
    }
  }

  // Copy XLA results to the OpOutputList.
  int output_num = 0;
  for (int i = 0; i < ctx->num_outputs(); ++i) {
    const TensorShape& shape = output_tensor_shapes[i];
    const DataType& type = compilation_result->outputs[i].type;
    VLOG(2) << "Retval " << i << " shape " << shape.DebugString() << " type "
            << DataTypeString(type);
    if (type == DT_VARIANT) {
      return errors::Unimplemented(
          "Support for TensorList crossing the XLA/TF boundary "
          "is not implemented");
    }

    if (compilation_result->outputs[i].is_constant) {
      TF_RETURN_IF_ERROR(
          SetOutputForConstant(ctx, stream, compilation_result, i));
    } else if (type == DT_RESOURCE) {
      int input_index =
          compilation_result->outputs[i].input_index - missing_ctx_input_prefix;
      TF_RET_CHECK(input_index >= 0 && input_index < ctx->num_inputs())
          << "Invalid input for outputs " << i << ": " << input_index;
      ctx->set_output(i, ctx->input(input_index));
    } else {
      if (allocate_xla_tensors_) {
        TF_RETURN_IF_ERROR(SetBufferForTensorUnderAllocateXlaTensors(
            input_output_alias, output_num, ctx, i, shape, &output,
            definition_event, stream, use_multiple_streams_));

      } else {
        se::DeviceMemoryBase buffer = output.buffer({output_num});
        Tensor output_tensor = GetOrCreateTensorForOutput(
            output_num, ctx, missing_ctx_input_prefix, input_output_alias,
            compilation_result->input_mapping, resource_var_snapshots,
            ctx->expected_output_dtype(i), shape, buffer, allocator);
        output.set_buffer(se::OwningDeviceMemory(), {output_num});
        ctx->set_output(i, output_tensor);
      }
      ++output_num;
    }

    if (VLOG_IS_ON(3)) {
      VLOG(3) << ctx->mutable_output(i)->DeviceSafeDebugString();
    }
  }

  // Apply variable updates, if any.
  VLOG(2) << "Applying variable updates";
  TF_ASSIGN_OR_RETURN(
      std::vector<VariableInfo> variable_infos,
      GatherVariableInfo(ctx, compilation_result, missing_ctx_input_prefix));
  TF_RETURN_IF_ERROR(LockVariables(absl::MakeSpan(variable_infos)));

  for (int i = 0; i < compilation_result->resource_updates.size(); ++i) {
    const XlaCompiler::ResourceUpdate& write =
        compilation_result->resource_updates[i];
    if (variable_infos[i].var()->tensor()->dtype() != write.type) {
      return errors::Internal("Mismatched type in variable write");
    }

    if (allocate_xla_tensors_) {
      TF_RETURN_IF_ERROR(SetBufferForResourceVarTensorUnderAllocateXlaTensors(
          input_output_alias, output_num, ctx, i, write, &output,
          definition_event, variable_infos, stream, use_multiple_streams_));
    } else {
      se::DeviceMemoryBase buffer = output.buffer({output_num});
      output.set_buffer(se::OwningDeviceMemory(), {output_num});
      Tensor output_tensor = GetOrCreateTensorForOutput(
          output_num, ctx, missing_ctx_input_prefix, input_output_alias,
          compilation_result->input_mapping, resource_var_snapshots, write.type,
          write.shape, buffer, allocator);
      *variable_infos[i].var()->tensor() = output_tensor;
      variable_infos[i].var()->is_initialized |= write.modified;
    }
    ++output_num;
  }
  return Status::OK();
}

Status XlaComputationLaunchContext::BuildXlaCompilerArguments(
    const std::map<int, Tensor>& constant_args,
    absl::Span<VariableInfo const> variable_args, OpKernelContext* ctx,
    std::vector<XlaCompiler::Argument>* args) {
  args->resize(ctx->num_inputs());

  absl::flat_hash_map<int, const VariableInfo*> variable_info_lookup;
  for (const VariableInfo& info : variable_args) {
    CHECK(!info.var() || info.lock_held())
        << "Need to hold the lock on resource variables "
           "before calling BuildXlaCompilerArguments";
    variable_info_lookup.emplace(info.index(), &info);
  }

  for (int64 input_num = 0; input_num < ctx->num_inputs(); ++input_num) {
    XlaCompiler::Argument& arg = (*args)[input_num];

    if (constant_args.count(input_num) > 0) {
      // Handles compile-time constants.
      const Tensor& input = constant_args.at(input_num);
      TF_RET_CHECK(input.dtype() != DT_RESOURCE);
      arg.kind = XlaCompiler::Argument::kConstant;
      arg.type = input.dtype();
      arg.shape = input.shape();
      arg.constant_value = input;
    } else if (variable_info_lookup.count(input_num) == 0) {
      // Handles the non-constant arguments.
      const Tensor& input = ctx->input(input_num);
      TF_RET_CHECK(input.dtype() != DT_RESOURCE);
      if (input.NumElements() > 0) {
        arg.kind = XlaCompiler::Argument::kParameter;
      } else {
        arg.kind = XlaCompiler::Argument::kConstant;
        arg.constant_value = input;
      }
      arg.type = input.dtype();
      arg.shape = input.shape();
    } else {
      // Handles resource variables.
      const Tensor& input = ctx->input(input_num);
      TF_RET_CHECK(input.dtype() == DT_RESOURCE);
      const VariableInfo& variable = *variable_info_lookup[input_num];
      arg.name = std::string(variable.name());
      arg.kind = XlaCompiler::Argument::kResource;
      arg.resource_kind = XlaResource::kVariable;
      if (variable.var()) {
        const Tensor* value = variable.var()->tensor();
        arg.type = value->dtype();
        arg.shape = value->shape();
        arg.initialized = true;
      } else {
        // The values of uninitialized variables are not passed as inputs, since
        // they are meaningless. However, it is legal to assign to a resource
        // variable for the first time inside the XLA computation, so we do
        // permit uninitialized variables.
        arg.initialized = false;
        arg.type = DT_INVALID;
        arg.shape = TensorShape();
      }
    }
  }

  return Status::OK();
}

}  // namespace tensorflow
