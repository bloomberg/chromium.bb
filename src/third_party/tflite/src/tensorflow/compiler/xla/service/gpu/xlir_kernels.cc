// Copyright 2021 The TensorFlow Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file implements the XLIR kernels.

#include <iterator>
#include <utility>
#include <vector>

#include "llvm/Support/Error.h"
#include "tensorflow/compiler/xla/service/custom_call_status_internal.h"
#include "tensorflow/compiler/xla/service/custom_call_target_registry.h"
#include "tensorflow/compiler/xla/service/gpu/nccl_collective_permute_thunk.h"
#include "tensorflow/compiler/xla/service/gpu/xlir_ops.h"
#include "tfrt/gpu/gpu_types.h"  // from @tf_runtime
#include "tfrt/gpu/kernels/kernels_detail.h"  // from @tf_runtime
#include "tfrt/host_context/async_dispatch.h"  // from @tf_runtime
#include "tfrt/host_context/kernel_utils.h"  // from @tf_runtime
#include "tfrt/support/error_util.h"  // from @tf_runtime

#if XLA_ENABLE_XCCL
#include "tensorflow/compiler/xla/service/gpu/nccl_utils.h"
#include "tfrt/gpu/wrapper/ccl_wrapper.h"  // from @tf_runtime
#endif  // XLA_ENABLE_XCCL

namespace xla {
namespace gpu {

#if BEF_THUNKS

static llvm::Expected<tfrt::gpu::GpuModule> ModuleLoad(
    tfrt::Argument<tfrt::gpu::GpuContext> context,
    const tfrt::ExecutionContext& exec_ctx) {
  const GpuModuleData* gpu_module_data =
      exec_ctx.request_ctx()->GetDataIfExists<GpuModuleData>();

  if (gpu_module_data == nullptr) {
    return tfrt::MakeStringError(
        "No GpuModuleData resource found in the request context.");
  }
  llvm::StringRef blob = gpu_module_data->blob;

  if (blob.empty() || blob.back() != 0)
    return tfrt::MakeStringError("blob must be null-terminated");

  auto current = tfrt::gpu::wrapper::CtxSetCurrent(context->get());
  if (!current) return current.takeError();

  auto module = tfrt::gpu::wrapper::ModuleLoadData(*current, blob.data());
  if (!module) return module.takeError();

  // Resolve constants.
  for (const auto& constant : gpu_module_data->constants) {
    if (constant.content.empty()) continue;

    auto global = tfrt::gpu::wrapper::ModuleGetGlobal(
        module->get(), constant.symbol_name.data());
    if (!global) return global.takeError();

    const void* constant_content =
        static_cast<const void*>(constant.content.data());
    tfrt::gpu::GpuPointer constant_content_ptr(
        const_cast<void*>(constant_content), current->platform());

    if (auto error = tfrt::gpu::wrapper::MemcpyAsync(
            *current, global->base, constant_content_ptr, global->size_bytes,
            tfrt::gpu::wrapper::Stream(nullptr, current->platform()))) {
      return error;
    }
  }
  return tfrt::gpu::GpuModule(context.ValueRef(), std::move(*module));
}

#endif  // BEF_THUNKS

static llvm::Expected<DeviceAssignment::LogicalID> GetLogicalId(
    const tfrt::ExecutionContext& exec_ctx) {
  auto* xla_gpu_params =
      exec_ctx.request_ctx()->GetDataIfExists<XlaGpuParams>();
  if (!xla_gpu_params) {
    return tfrt::MakeStringError("Failed to get XlaGpuParams");
  }

  StatusOr<DeviceAssignment::LogicalID> current_logical_id_or =
      xla_gpu_params->device_assn->LogicalIdForDevice(
          xla_gpu_params->global_device_id);
  if (!current_logical_id_or.ok()) {
    return tfrt::MakeStringError(
        current_logical_id_or.status().error_message());
  }
  return current_logical_id_or.ValueOrDie();
}

static llvm::Error ReplicaId(const tfrt::gpu::GpuStream& stream,
                             const tfrt::gpu::GpuBuffer& output,
                             const tfrt::ExecutionContext& exec_ctx) {
  auto current_logical_id = GetLogicalId(exec_ctx);
  if (!current_logical_id) return current_logical_id.takeError();

  auto current = tfrt::gpu::wrapper::CtxSetCurrent(stream.context()->get());
  if (!current) return current.takeError();

  return tfrt::gpu::wrapper::MemsetD32Async(*current, output.pointer(),
                                            current_logical_id->replica_id, 1,
                                            stream.get());
}

static llvm::Error PartitionId(const tfrt::gpu::GpuStream& stream,
                               const tfrt::gpu::GpuBuffer& output,
                               const tfrt::ExecutionContext& exec_ctx) {
  auto current_logical_id = GetLogicalId(exec_ctx);
  if (!current_logical_id) return current_logical_id.takeError();

  auto current = tfrt::gpu::wrapper::CtxSetCurrent(stream.context()->get());
  if (!current) return current.takeError();

  return tfrt::gpu::wrapper::MemsetD32Async(*current, output.pointer(),
                                            current_logical_id->computation_id,
                                            1, stream.get());
}

#if XLA_ENABLE_XCCL
static tfrt::AsyncValueRef<tfrt::gpu::GpuCclHandle> CclCreate(
    tfrt::Argument<tfrt::gpu::GpuContext> context,
    tfrt::Attribute<int64_t> group_mode_attr,
    tfrt::Attribute<int64_t> op_id_attr,
    tfrt::RemainingAttributes replica_groups_attrs,
    const tfrt::ExecutionContext& exec_ctx) {
  auto* xla_gpu_params =
      exec_ctx.request_ctx()->GetDataIfExists<XlaGpuParams>();
  if (!xla_gpu_params) {
    return tfrt::MakeErrorAsyncValueRef("Failed to get XlaGpuParams");
  }

  std::vector<ReplicaGroup> replica_groups;
  replica_groups.reserve(replica_groups_attrs.size());
  for (int i = 0; i < replica_groups_attrs.size(); ++i) {
    ReplicaGroup replica_group;
    for (auto replica_id :
         replica_groups_attrs.GetArrayAttribute<int64_t>(i).data()) {
      replica_group.add_replica_ids(replica_id);
    }
    replica_groups.push_back(replica_group);
  }

  StatusOr<std::vector<GlobalDeviceId>> participants_or =
      GetParticipatingDevices(
          xla_gpu_params->global_device_id, *xla_gpu_params->device_assn,
          replica_groups, static_cast<CollectiveOpGroupMode>(*group_mode_attr));
  if (!participants_or.ok()) {
    return tfrt::MakeErrorAsyncValueRef(
        participants_or.status().error_message());
  }
  std::vector<GlobalDeviceId> participants =
      std::move(participants_or.ValueOrDie());

  if (IsGlobalNcclConfig() &&
      (participants.size() != xla_gpu_params->device_assn->replica_count())) {
    return tfrt::MakeErrorAsyncValueRef(
        "Partial replica groups are not allowed when using NCCL_COMM_ID "
        "environment configuration.");
  }

  auto it = absl::c_find(participants, xla_gpu_params->global_device_id);
  if (it == participants.end()) {
    return tfrt::MakeErrorAsyncValueRef(
        "Device ID not found among participants.");
  }
  int rank = it - participants.begin();

  OpId op_id(*op_id_attr);
  size_t num_local_participants = GetNumLocalParticipants(
      participants, /*local_devices=*/xla_gpu_params->gpu_global_device_ids);

  bool is_local = participants.size() == num_local_participants;
  StatusOr<const NcclUniqueIdCallback*> unique_id_callback_or =
      GetNcclUniqueIdCallback(xla_gpu_params->nccl_unique_id_callback,
                              is_local);
  if (!unique_id_callback_or.ok()) {
    return tfrt::MakeErrorAsyncValueRef(
        unique_id_callback_or.status().error_message());
  }
  const NcclUniqueIdCallback* unique_id_callback =
      unique_id_callback_or.ValueOrDie();

  // AcquireNcclComm() blocks to wait for all participants and therefore needs
  // to run inside a blocking task.
  return tfrt::RunBlockingWork(
      exec_ctx.host(),
      [=, participants = std::move(participants),
       context = context.ValueRef()]() mutable
      -> llvm::Expected<tfrt::gpu::GpuCclHandle> {
        auto current = tfrt::gpu::wrapper::CtxSetCurrent(context->get());
        if (!current) return current.takeError();

        StatusOr<NcclComm::Lock> comm_or = AcquireNcclComm(
            xla_gpu_params->run_id, op_id, std::move(participants),
            num_local_participants, *unique_id_callback, rank);
        if (!comm_or.ok())
          return tfrt::MakeStringError(comm_or.status().error_message());

        auto* comm_ptr = comm_or->get();
        auto comm_deleter =
            [comm = std::move(comm_or.ValueOrDie())](
                tfrt::gpu::wrapper::CclComm /*ccl_comm*/) mutable {
              comm.reset();
            };

        return tfrt::gpu::GpuCclHandle(
            std::move(context),
            tfrt::gpu::wrapper::OwningCclComm({*comm_ptr, current->platform()}),
            std::move(comm_deleter));
      });
}

static tfrt::AsyncValueRef<tfrt::Chain> CclCollectivePermute(
    tfrt::Argument<tfrt::gpu::GpuCclHandle> handle,
    tfrt::Argument<tfrt::gpu::GpuBuffer> input,
    tfrt::Argument<tfrt::gpu::GpuBuffer> output,
    // Needs to be sorted alphabetically by attribute name!
    tfrt::Attribute<int32_t> data_type_attr,
    tfrt::Attribute<int64_t> group_mode_attr, tfrt::ArrayAttr source_peers_attr,
    tfrt::ArrayAttr target_peers_attr, const tfrt::ExecutionContext& exec_ctx) {
  // NCCL 2.8.x has an issue with point-to-point communication primitives if
  // different ranks process different amounts of data. This can happen in the
  // case of a collective permute as certain nodes may not do any send or
  // receives, or do only send or only receive. Sending and receiving to self
  // as well (identity pair) causes this imbalance. NCCL 2.8.x requires the
  // use of NCCL_LAUNCH_MODE=PARALLEL to avoid these issues. See
  // https://docs.nvidia.com/deeplearning/nccl/release-notes/rel_2-8-4.html#rel_2-8-4
  if (!IsNcclLaunchModeParallel()) {
    LOG(WARNING) << "NCCL based collective permute may not work correctly if "
                    "NCCL_LAUNCH_MODE is not set to PARALLEL";
  }

  auto current_logical_id = GetLogicalId(exec_ctx);
  if (!current_logical_id)
    return tfrt::MakeErrorAsyncValueRef(
        llvm::toString(current_logical_id.takeError()));

  const int64_t current_id =
      static_cast<CollectiveOpGroupMode>(*group_mode_attr) ==
              CollectiveOpGroupMode::kCrossReplica
          ? current_logical_id->replica_id
          : current_logical_id->computation_id;

  NcclCollectivePermuteConfig config;
  for (int i = 0; i < source_peers_attr.GetNumElements(); ++i) {
    int64_t source = source_peers_attr.GetValue<int64_t>()[i];
    int64_t target = target_peers_attr.GetValue<int64_t>()[i];

    config.id_to_source_target.insert({target, {}}).first->second.source =
        source;
    config.id_to_source_target.insert({source, {}}).first->second.target =
        target;
  }
  NcclCollectivePermuteConfig::SourceTargetMapEntry source_target =
      config.GetSourceTarget(current_id);
  const absl::optional<int64_t>& source_peer = source_target.source;
  const absl::optional<int64_t>& target_peer = source_target.target;

  auto type = static_cast<ncclDataType_t>(*data_type_attr);
  auto width = tfrt::gpu::wrapper::GetCclDataTypeSizeBytes(type);
  if (!width)
    return tfrt::MakeErrorAsyncValueRef(llvm::toString(width.takeError()));
  assert(*width != 0);

  if (target_peer) {
    handle->AddCallback(
        [input = input.ValueRef(), count = input->size() / *width, type,
         peer = *target_peer](tfrt::gpu::wrapper::CurrentContext current,
                              tfrt::gpu::wrapper::Stream stream,
                              tfrt::gpu::wrapper::CclComm comm) -> llvm::Error {
          return tfrt::gpu::wrapper::CclSend(current, input->pointer(), count,
                                             type, peer, comm, stream);
        });
  }

  if (source_peer) {
    handle->AddCallback(
        [output = output.ValueRef(), count = output->size() / *width, type,
         peer = *source_peer](tfrt::gpu::wrapper::CurrentContext current,
                              tfrt::gpu::wrapper::Stream stream,
                              tfrt::gpu::wrapper::CclComm comm) -> llvm::Error {
          return tfrt::gpu::wrapper::CclRecv(current, output->pointer(), count,
                                             type, peer, comm, stream);
        });
  } else {
    // If there is no source peer, i.e. no one send us any data, zero out dest
    // buffer.
    handle->AddCallback(
        [output = output.ValueRef(), count = output->size() / *width, type,
         peer = *source_peer](tfrt::gpu::wrapper::CurrentContext current,
                              tfrt::gpu::wrapper::Stream stream,
                              tfrt::gpu::wrapper::CclComm comm) -> llvm::Error {
          return tfrt::gpu::wrapper::MemsetD8Async(current, output->pointer(),
                                                   0, output->size(), stream);
        });
  }

  return tfrt::MakeAvailableAsyncValueRef<tfrt::Chain>();
}
#endif  // XLA_ENABLE_XCCL

static llvm::Error CustomCall(
    const tfrt::gpu::GpuStream& stream,
    tfrt::RepeatedArguments<tfrt::gpu::GpuBuffer> buffers_and_chain,
    // Needs to be sorted alphabetically by attribute name!
    tfrt::ArrayAttr indices, tfrt::StringAttribute opaque,
    tfrt::StringAttribute symbol) {
  // Lookup custom call target from registry.
  auto platform = stream->platform();
  auto* target = CustomCallTargetRegistry::Global()->Lookup(
      symbol.str(), tfrt::StrCat(platform));
  if (!target) {
    return tfrt::MakeStringError("Custom call target '", symbol.str(),
                                 "' not registered for platform ", platform);
  }

  auto current = tfrt::gpu::wrapper::CtxSetCurrent(stream.context()->get());
  if (!current) return current.takeError();

  // Create buffer pointer array argument.
  llvm::SmallVector<void*, 16> pointers;
  llvm::transform(indices.GetValue<int32_t>(), std::back_inserter(pointers),
                  [&](int32_t index) -> void* {
                    if (index < 0) return nullptr;
                    return buffers_and_chain[index].pointer().raw(platform);
                  });

  auto stream_ptr = [&]() -> void* {
    switch (platform) {
      case tfrt::gpu::wrapper::Platform::CUDA:
        return static_cast<CUstream>(stream.get());
      case tfrt::gpu::wrapper::Platform::ROCm:
        return static_cast<hipStream_t>(stream.get());
      default:
        return nullptr;
    }
  }();

  XlaCustomCallStatus status;
  using FuncPtr =
      void (*)(void*, void* const*, const char*, size_t, XlaCustomCallStatus*);
  reinterpret_cast<FuncPtr>(target)(stream_ptr, pointers.data(),
                                    opaque.get().data(), opaque.get().size(),
                                    &status);

  if (auto message = CustomCallStatusGetMessage(&status))
    return tfrt::MakeStringError("Custom call failed: ", message->data());

  return llvm::Error::success();
}

static void RegisterXlirKernels(tfrt::KernelRegistry* kernel_reg) {
  kernel_reg->AddKernel("xlir.custom_call",
                        TFRT_KERNEL_WITH_CHAIN_RESULT(CustomCall));
#if BEF_THUNKS
  kernel_reg->AddKernel("xlir.module.load", TFRT_KERNEL(ModuleLoad));
#endif  // BEF_THUNKS
  kernel_reg->AddKernel("xlir.replica_id",
                        TFRT_KERNEL_WITH_CHAIN_RESULT(ReplicaId));
  kernel_reg->AddKernel("xlir.partition_id",
                        TFRT_KERNEL_WITH_CHAIN_RESULT(PartitionId));
#if XLA_ENABLE_XCCL
  kernel_reg->AddKernel("xlir.ccl.create", TFRT_KERNEL(CclCreate));
  kernel_reg->AddKernel("xlir.ccl.collective_permute",
                        TFRT_KERNEL(CclCollectivePermute));
#endif  // XLA_ENABLE_XCCL
}

TFRT_STATIC_KERNEL_REGISTRATION(RegisterXlirKernels);

}  // namespace gpu
}  // namespace xla
