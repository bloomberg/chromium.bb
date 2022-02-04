/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/compiler/xla/service/gpu/gpu_executable.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "tensorflow/compiler/xla/map_util.h"
#include "tensorflow/compiler/xla/service/gpu/buffer_allocations.h"
#include "tensorflow/compiler/xla/service/gpu/gpu_constants.h"
#include "tensorflow/compiler/xla/service/gpu/gpu_executable_run_options.h"
#include "tensorflow/compiler/xla/service/gpu/gpu_types.h"
#include "tensorflow/compiler/xla/service/gpu/stream_executor_util.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_parser.h"
#include "tensorflow/compiler/xla/service/llvm_ir/buffer_assignment_util.h"
#include "tensorflow/compiler/xla/service/logical_buffer.h"
#include "tensorflow/compiler/xla/service/shaped_buffer.h"
#include "tensorflow/compiler/xla/service/transfer_manager.h"
#include "tensorflow/compiler/xla/service/xla_debug_info_manager.h"
#include "tensorflow/compiler/xla/shape_tree.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/core/lib/gtl/map_util.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/profiler/lib/scoped_annotation.h"
#include "tensorflow/core/profiler/lib/traceme.h"
#include "tensorflow/stream_executor/platform.h"

#if BEF_EXECUTABLE
#include "llvm/Support/SourceMgr.h"
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/Diagnostics.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/utils/name_utils.h"
#include "tensorflow/core/tfrt/runtime/work_queue_interface.h"
#include "tensorflow/stream_executor/cuda/cuda_driver.h"
#include "tensorflow/stream_executor/gpu/gpu_executor.h"
#include "tensorflow/stream_executor/gpu/gpu_stream.h"
#include "tfrt/gpu/gpu_executor.h"  // from @tf_runtime
#include "tfrt/gpu/gpu_types.h"  // from @tf_runtime
#include "tfrt/bef/bef_buffer.h"  // from @tf_runtime
#include "tfrt/bef_converter/bef_to_mlir.h"  // from @tf_runtime
#include "tfrt/bef_executor/bef_file.h"  // from @tf_runtime
#include "tfrt/core_runtime/core_runtime.h"  // from @tf_runtime
#include "tfrt/host_context/async_dispatch.h"  // from @tf_runtime
#include "tfrt/host_context/chain.h"  // from @tf_runtime
#include "tfrt/host_context/execution_context.h"  // from @tf_runtime
#include "tfrt/host_context/function.h"  // from @tf_runtime
#include "tfrt/host_context/host_allocator.h"  // from @tf_runtime
#include "tfrt/host_context/host_context.h"  // from @tf_runtime
#endif  // BEF_EXECUTABLE

namespace xla {
namespace gpu {
namespace {

using ::tensorflow::profiler::ScopedAnnotation;

bool NeedsAsyncCommsStream(Thunk& thunk) {
  switch (thunk.kind()) {
    case Thunk::Kind::kNcclAllReduceStart:
    case Thunk::Kind::kNcclAllReduceDone:
      return true;
    default:
      return false;
  }
}

static std::string ModuleUniqueName(absl::string_view module_name,
                                    const HloModule* module) {
  std::string unique_id;
  if (module != nullptr) {
    unique_id = absl::StrCat("module.", module->unique_id(), ".");
  }
  return absl::StrCat(unique_id, module_name);
}

}  // namespace

void GpuExecutable::BefBufferDeleter::operator()(uint8_t* ptr) const {
#if BEF_EXECUTABLE
  tfrt::AlignedFree(ptr);
#else
  LOG(FATAL) << "OwnedBefBuffer only supported with BEF_EXECUTABLE";
#endif
}

#if BEF_EXECUTABLE
struct GpuExecutable::BefExecutable {
 private:
  explicit BefExecutable(OwnedBefBuffer buffer)
      : bef_buffer(std::move(buffer)),
        host_ctx(tfrt::gpu::GetDiagHandler(&mlir_ctx),
                 tfrt::CreateMallocAllocator(),
                 tfrt::CreateSingleThreadedWorkQueue()) {
    tfrt::RegisterStaticKernels(host_ctx.GetMutableRegistry());
  }

  Status Initialize() {
    bef_file =
        tfrt::BEFFile::Open({bef_buffer.get(), bef_buffer.get_deleter().size},
                            host_ctx.GetKernelRegistry(),
                            host_ctx.diag_handler(), host_ctx.allocator());
    if (!bef_file) {
      return InternalError("Failed to decode BEF buffer");
    }

    auto req_ctx = tfrt::RequestContextBuilder(&host_ctx, nullptr).build();
    if (!req_ctx) {
      return tensorflow::errors::Internal(toString(req_ctx.takeError()));
    }
    tfrt::ExecutionContext exec_ctx(*req_ctx);

    auto expected_entry_point = tfrt::gpu::GetEntryPoint(*bef_file, exec_ctx);
    if (!expected_entry_point) {
      return tensorflow::errors::Internal(
          toString(expected_entry_point.takeError()));
    }
    entry_point = *expected_entry_point;

    const auto& func_name = entry_point.function_name;
    function = bef_file->GetFunction(func_name);
    if (!function) {
      return InternalError("Failed to get '%s' function", func_name);
    }

    return Status::OK();
  }

 public:
  static StatusOr<BefExecutable*> Create(OwnedBefBuffer buffer) {
    std::unique_ptr<BefExecutable> result(new BefExecutable(std::move(buffer)));
    TF_RETURN_IF_ERROR(result->Initialize());
    return result.release();
  }

  OwnedBefBuffer bef_buffer;
  mlir::MLIRContext mlir_ctx;
  tfrt::HostContext host_ctx;
  tfrt::RCReference<tfrt::BEFFile> bef_file;
  tfrt::gpu::EntryPoint entry_point;
  // Signature: (chain, stream, inputs..., outputs...) -> (chain).
  const tfrt::Function* function;
  tensorflow::mutex mutex;
  tfrt::gpu::GpuContextCache gpu_ctx_cache TF_GUARDED_BY(mutex);
};
#endif

StatusOr<std::unique_ptr<GpuExecutable>> GpuExecutable::Create(Params params) {
  auto thunks_or_bef = std::move(params.thunks_or_bef);
  std::unique_ptr<GpuExecutable> result(new GpuExecutable(std::move(params)));

  if (absl::holds_alternative<OwnedThunkSchedule>(thunks_or_bef)) {
    result->thunks_ = std::move(absl::get<OwnedThunkSchedule>(thunks_or_bef));
    return result;
  }

#if BEF_EXECUTABLE
  if (absl::holds_alternative<OwnedBefBuffer>(thunks_or_bef)) {
    auto& bef_buffer = absl::get<OwnedBefBuffer>(thunks_or_bef);
    TF_ASSIGN_OR_RETURN(result->bef_executable_,
                        BefExecutable::Create(std::move(bef_buffer)));
    return result;
  }
#endif

  return InternalError("No thunk or bef provided");
}

// Implementation note: HLO profiling is always enabled for GPU executables,
// since we can use timers around thunks.
GpuExecutable::GpuExecutable(GpuExecutable::Params params)
    : Executable(std::move(params.debug_module)),
      text_(std::move(params.asm_text)),
      binary_(std::move(params.binary)),
      gpu_version_(params.gpu_version),
      entry_func_attrs_(params.entry_func_attrs),
      module_name_(params.module_name),
      output_shape_(params.output_shape),
      allocations_(std::move(params.allocations)),
      debug_buffer_assignment_(std::move(params.debug_buffer_assignment)),
      verbose_buffer_assignment_string_dumper_(
          params.verbose_buffer_assignment_string_dumper),
      constants_(std::move(params.constants)),
      output_info_(std::move(params.output_info)) {
  XlaDebugInfoManager::Get()->RegisterModule(
      ModuleUniqueName(module_name_, shared_module().get()), shared_module(),
      debug_buffer_assignment_);
}

GpuExecutable::GpuExecutable(
    std::shared_ptr<HloModule> hlo_module, GpuVersion gpu_version,
    xla::EntryFunctionAttributes entry_func_attrs,
    absl::string_view module_name, Shape xla_output_shape,
    std::vector<BufferAllocation> allocations,
    absl::flat_hash_map<ShapeIndex, OutputInfo> output_info,
    BefExecutable* bef_executable)
    : Executable(std::move(hlo_module)),
      gpu_version_(gpu_version),
      entry_func_attrs_(entry_func_attrs),
      module_name_(module_name),
      output_shape_(xla_output_shape),
      allocations_(std::move(allocations)),
      output_info_(std::move(output_info)),
      bef_executable_(bef_executable) {
  XlaDebugInfoManager::Get()->RegisterModule(
      ModuleUniqueName(module_name_, shared_module().get()), shared_module(),
      debug_buffer_assignment_);
}

GpuExecutable::~GpuExecutable() {
  XlaDebugInfoManager::Get()->UnregisterModule(
      ModuleUniqueName(module_name_, shared_module().get()), shared_module(),
      debug_buffer_assignment_);

  {
    // We could have issued host->device mem copies in ResolveConstantGlobals.
    // Wait for those to finish so that we can safely deallocate the backing HLO
    // module.
    //
    // We need for the host->device memcpies to finish they are concurrently
    // reading memory (xla::Literal's) owned by the HLO module.
    tensorflow::mutex_lock lock(module_handle_mutex_);
    for (const auto& pair : module_globals_) {
      CHECK(pair.first->SynchronizeAllActivity());
    }
  }

#if BEF_EXECUTABLE
  delete bef_executable_;
#endif
}

Status GpuExecutable::CheckCompatibilityWithServiceExecutableRunOptions(
    const ServiceExecutableRunOptions* run_options) {
  se::Stream* main_stream = run_options->stream();

  stream_executor::PlatformKind platform_kind =
      main_stream->parent()->platform_kind();
  if (platform_kind == stream_executor::PlatformKind::kROCm) {
    std::string stream_arch = main_stream->parent()
                                  ->GetDeviceDescription()
                                  .rocm_amdgpu_gcn_arch_name();
    std::string gpu_exec_arch = absl::get<std::string>(gpu_version_);
    TF_RET_CHECK(stream_arch == gpu_exec_arch)
        << "AMDGPU GCN ISA version mismatch; expected {" << gpu_exec_arch
        << ", but was " << stream_arch;
  } else if (platform_kind == stream_executor::PlatformKind::kCuda) {
    GpuVersion cc = main_stream->GetCudaComputeCapability();
    TF_RET_CHECK(absl::get<se::CudaComputeCapability>(cc) ==
                 absl::get<se::CudaComputeCapability>(gpu_version_))
        << "Compute capability mismatch; expected {"
        << absl::get<se::CudaComputeCapability>(gpu_version_).ToString()
        << "}, but was {" << absl::get<se::CudaComputeCapability>(cc).ToString()
        << "}";
  } else {
    return InternalError("Unknown platform: %d", platform_kind);
  }

  return Status::OK();
}

namespace {

Status MaybeSyncAndProfile(const ServiceExecutableRunOptions* run_options,
                           uint64_t start_micros, se::Stream* stream_to_sync);

Status ExecuteThunks(const std::string& module_name,
                     const ThunkSchedule& thunk_schedule,
                     const ServiceExecutableRunOptions* run_options,
                     const BufferAllocations& buffer_allocations,
                     bool block_host_until_done) {
  XlaDebugInfoManager::Get()->OnModuleStart(module_name);
  auto cleanup = absl::MakeCleanup(
      [&]() { XlaDebugInfoManager::Get()->OnModuleStop(module_name); });

  se::Stream* main_stream = run_options->stream();
  se::StreamExecutor* executor = main_stream->parent();

  StatusOr<StreamPool::Ptr> async_comms_stream =
      run_options->BorrowStream(executor->device_ordinal());

  // Stream 0 indicates `main_stream` and substreams start from stream 1.
  std::vector<StreamPool::Ptr> sub_streams;
  sub_streams.reserve(thunk_schedule.StreamCount() - 1);
  while (sub_streams.size() + 1 < thunk_schedule.StreamCount()) {
    sub_streams.emplace_back();
    TF_ASSIGN_OR_RETURN(sub_streams.back(),
                        run_options->BorrowStream(executor->device_ordinal()));
    // Require substreams to wait for the main stream, otherwise substreams may
    // execute before the program is scheduled to start on the main stream.
    sub_streams.back()->ThenWaitFor(main_stream);
  }

  uint64_t start_micros = tensorflow::Env::Default()->NowMicros();

  tensorflow::profiler::TraceMe hlo_module_activity(
      [&] { return absl::StrCat(module_name, ":XLA GPU module"); },
      tensorflow::profiler::TraceMeLevel::kInfo);

  absl::flat_hash_map<const Thunk*, std::unique_ptr<se::Event>>
      thunk_to_finish_event;
  for (const std::unique_ptr<Thunk>& thunk : thunk_schedule.TotalOrder()) {
    // Annotate execution of this op if tracing was enabled when we started
    // running this module.  If tracing is enabled *while* we're running the
    // module, we won't get any data, but that's probably an OK trade-off.
    ScopedAnnotation annotation([&] { return thunk->profile_annotation(); });

    int32_t stream_no = thunk_schedule.StreamNumberForThunk(thunk.get());
    se::Stream* stream =
        (stream_no == 0 ? main_stream : sub_streams[stream_no - 1].get());

    for (const Thunk* dependency : thunk_schedule.DependsOn(thunk.get())) {
      stream->ThenWaitFor(FindOrDie(thunk_to_finish_event, dependency).get());
    }

    VLOG(2) << "Executing the thunk for " << thunk->profile_annotation()
            << " on stream " << stream_no;

    TF_RET_CHECK(async_comms_stream.ok() || !NeedsAsyncCommsStream(*thunk))
        << "`run_options` must have a stream borrower for async thunks.";

    const GpuExecutableRunOptions* gpu_options =
        run_options->run_options().gpu_executable_run_options();
    Thunk::ExecuteParams thunk_params{
        &buffer_allocations,
        stream,
        async_comms_stream.ok() ? async_comms_stream->get() : nullptr,
        run_options->run_options().run_id(),
        run_options->run_options().device_assignment(),
        gpu_options && gpu_options->gpu_global_device_ids()
            ? &*gpu_options->gpu_global_device_ids()
            : nullptr,
        gpu_options && gpu_options->nccl_unique_id_callback()
            ? &gpu_options->nccl_unique_id_callback()
            : nullptr};
    TF_RETURN_IF_ERROR(thunk->ExecuteOnStream(thunk_params));
    if (thunk_schedule.Depended(thunk.get())) {
      auto finish_event = absl::make_unique<se::Event>(main_stream->parent());
      finish_event->Init();
      stream->ThenRecordEvent(finish_event.get());
      thunk_to_finish_event[thunk.get()] = std::move(finish_event);
    }
  }

  main_stream->ThenWaitFor(&sub_streams);
  return MaybeSyncAndProfile(run_options, start_micros,
                             block_host_until_done ? main_stream : nullptr);
}

Status MaybeSyncAndProfile(const ServiceExecutableRunOptions* run_options,
                           uint64_t start_micros,
                           se::Stream* stream_to_sync = nullptr) {
  // Make sure kernels are completed before deallocating temporary buffers or
  // the profiler state.
  // TODO(b/30100571): we could potentially postpone deallocating the temp
  // buffers until a different computation is executed.
  if (stream_to_sync) {
    Status block_status = stream_to_sync->BlockHostUntilDone();
    if (!block_status.ok()) {
      return InternalError(
          "Failed to complete all kernels launched on stream %p: %s",
          stream_to_sync, block_status.error_message());
    }
  }

  // FinishExecution() blocks until main_stream has completed if profiling is
  // enabled; we therefore do not need to defer profile collection onto a
  // stream.
  uint64_t end_micros = tensorflow::Env::Default()->NowMicros();

  if (run_options->run_options().execution_profile()) {
    ExecutionProfile* profile = run_options->run_options().execution_profile();
    const double nanoseconds = (end_micros - start_micros) * 1000.0;
    profile->set_compute_time_ns(std::max(nanoseconds, 1.0));
  }

  return Status::OK();
}

}  // namespace

StatusOr<const GpuExecutable::BufferAllocToDeviceMemoryMap*>
GpuExecutable::ResolveConstantGlobals(se::Stream* stream) {
  se::StreamExecutor* executor = stream->parent();

  tensorflow::mutex_lock lock(module_handle_mutex_);
  auto it = module_globals_.find(executor);
  if (it != module_globals_.end()) {
    return &it->second;
  }

  se::MultiModuleLoaderSpec module_spec;
  if (!binary().empty()) {
    module_spec.AddCudaCubinInMemory(binary());
  }
  module_spec.AddCudaPtxInMemory(text().c_str());

  absl::flat_hash_map<int64_t, se::DeviceMemoryBase> globals;
  if (executor->platform_kind() == se::PlatformKind::kCuda &&
      module_spec.cuda_ptx_in_memory() == nullptr) {
    // No custom PTX => no globals.
    return &module_globals_.emplace(executor, std::move(globals)).first->second;
  }

  se::ModuleHandle module_handle;
  TF_RETURN_IF_ERROR(executor->LoadModule(module_spec, &module_handle));

  for (const auto& info : constants_) {
    TF_ASSIGN_OR_RETURN(auto global, executor->GetUntypedSymbol(
                                         info.symbol_name, module_handle));
    VLOG(3) << "Resolved global " << info.symbol_name << " to "
            << global.opaque();

    if (!info.content.empty()) {
      stream->ThenMemcpy(&global, info.content.data(), info.content.size());
    }

    if (info.allocation_index != -1) {
      InsertOrDie(&globals, info.allocation_index, global);
    }
  }

  module_handles_.emplace(executor,
                          se::ScopedModuleHandle(executor, module_handle));
  return &module_globals_.emplace(executor, std::move(globals)).first->second;
}

StatusOr<se::DeviceMemoryBase> GpuExecutable::BufferForAllocation(
    VariantArguments arguments,
    const GpuExecutable::BufferAllocToDeviceMemoryMap* globals,
    const BufferAllocation& allocation,
    se::DeviceMemoryAllocator* const memory_allocator, int device_ordinal,
    int64_t arg_idx) {
  if (allocation.is_thread_local()) {
    return se::DeviceMemoryBase{};
  } else if (allocation.is_entry_computation_parameter()) {
    int64_t param_no = allocation.parameter_number();
    se::DeviceMemoryBase registered_buffer = [&] {
      if (auto unowned_shapedbuffers =
              absl::get_if<absl::Span<const ShapedBuffer* const>>(&arguments)) {
        return (*unowned_shapedbuffers)[param_no]->buffers().element(
            allocation.param_shape_index());
      } else {
        return absl::get<absl::Span<ExecutionInput>>(arguments)[param_no]
            .Buffer(allocation.param_shape_index())
            .AsDeviceMemoryBase();
      }
    }();
    if (registered_buffer.is_null() && registered_buffer.size() > 0) {
      return FailedPrecondition(
          "Cannot run XLA computation because pointer to (sub-)buffer at "
          "index %s of parameter %d was null.  All pointers to "
          "(sub-)buffers must not be null, unless the (sub-)buffer has "
          "zero elements.",
          allocation.param_shape_index().ToString(), param_no);
    }
    return registered_buffer;
  } else if (allocation.is_constant()) {
    auto it = globals->find(arg_idx);
    if (it == globals->end()) {
      return se::DeviceMemoryBase();
    }
    return it->second;
  } else {
    // Allocate each allocation that might escape, or is the temp buffer.
    CHECK(allocation.maybe_live_out() || allocation.IsPreallocatedTempBuffer());
    const int64_t buffer_size = allocation.size();
    se::DeviceMemoryBase buffer_address;
    if (buffer_size > 0) {
      StatusOr<se::OwningDeviceMemory> buffer =
          memory_allocator->Allocate(device_ordinal, buffer_size);
      if (!buffer.ok()) {
        return ResourceExhausted("%s\n%s\n", buffer.status().error_message(),
                                 verbose_buffer_assignment_string_dumper_());
      }
      buffer_address = buffer->Release();
    }
    return buffer_address;
  }
}

static Status CheckAlignment(const BufferAllocation& allocation,
                             se::DeviceMemoryBase buffer, int arg_idx) {
  const int64_t expected_alignment = [&] {
    if (allocation.is_entry_computation_parameter()) {
      return kEntryParameterAlignBytes;
    } else if (allocation.is_constant()) {
      return kConstantBufferAlignBytes;
    } else {
      return kXlaAllocatedBufferAlignBytes;
    }
  }();
  if (!buffer.is_null() &&
      reinterpret_cast<uintptr_t>(buffer.opaque()) % expected_alignment != 0) {
    return InternalError(
        "Address of buffer %d must be a multiple of %x, but "
        "was %p",
        arg_idx, expected_alignment, buffer.opaque());
  }
  return Status::OK();
}

StatusOr<BufferAllocations> GpuExecutable::GenerateBufferAllocations(
    VariantArguments arguments,
    const GpuExecutable::BufferAllocToDeviceMemoryMap* globals,
    se::DeviceMemoryAllocator* const memory_allocator,
    se::StreamExecutor* executor) {
  tensorflow::profiler::TraceMe hlo_module_activity(
      [&] { return std::string("Build buffer allocations"); },
      tensorflow::profiler::TraceMeLevel::kInfo);

  const int64_t num_buffers = allocations_.size();
  std::vector<se::DeviceMemoryBase> buffers;
  buffers.reserve(num_buffers);
  for (int64_t i = 0; i < num_buffers; ++i) {
    const BufferAllocation& allocation = allocations_[i];
    TF_ASSIGN_OR_RETURN(
        se::DeviceMemoryBase buffer,
        BufferForAllocation(arguments, globals, allocation, memory_allocator,
                            executor->device_ordinal(), i));
    buffers.push_back(buffer);
    TF_RETURN_IF_ERROR(CheckAlignment(allocation, buffer, i));
  }
  return {{buffers, executor->device_ordinal(), memory_allocator}};
}

StatusOr<ExecutionOutput> GpuExecutable::ExecuteAsyncOnStream(
    const ServiceExecutableRunOptions* run_options,
    std::vector<ExecutionInput> arguments,
    HloExecutionProfile* hlo_execution_profile) {
  return ExecuteAsyncOnStreamImpl(run_options, absl::MakeSpan(arguments));
}

StatusOr<ScopedShapedBuffer> GpuExecutable::ExecuteAsyncOnStream(
    const ServiceExecutableRunOptions* run_options,
    absl::Span<const ShapedBuffer* const> arguments,
    HloExecutionProfile* hlo_execution_profile) {
  TF_ASSIGN_OR_RETURN(ExecutionOutput out,
                      ExecuteAsyncOnStreamImpl(run_options, arguments));
  return out.ConsumeResult();
}

#if BEF_EXECUTABLE
// TODO(hanbinyoon): Deduplicate with that in bef_thunk.cc.
static tfrt::RCReference<tfrt::AsyncValue> CreateGpuBuffer(
    stream_executor::DeviceMemoryBase* data) {
  tfrt::gpu::wrapper::Pointer<void> pointer(data->opaque(),
                                            tfrt::gpu::wrapper::Platform::CUDA);
  auto allocator =
      tfrt::MakeAvailableAsyncValueRef<tfrt::gpu::GpuOneShotAllocator<void>>(
          pointer);
  auto buffer =
      tfrt::gpu::GpuBuffer::Allocate(std::move(allocator), data->size());
  if (!buffer)
    return tfrt::MakeErrorAsyncValueRef(tfrt::StrCat(buffer.takeError()));
  return tfrt::MakeAvailableAsyncValueRef<tfrt::gpu::GpuBuffer>(
      std::move(*buffer));
}

static Status ExecuteBef(const std::string& module_name,
                         GpuExecutable::BefExecutable* bef_executable,
                         const ServiceExecutableRunOptions* run_options,
                         const BufferAllocations& buffer_allocations,
                         size_t num_allocations, bool block_host_until_done) {
  XlaDebugInfoManager::Get()->OnModuleStart(module_name);
  auto cleanup = absl::MakeCleanup(
      [&]() { XlaDebugInfoManager::Get()->OnModuleStop(module_name); });

  uint64_t start_micros = tensorflow::Env::Default()->NowMicros();

  tensorflow::profiler::TraceMe hlo_module_activity(
      [&] { return absl::StrCat(module_name, ":XLA GPU module"); },
      tensorflow::profiler::TraceMeLevel::kInfo);

  // TODO(hanbinyoon): Expand on the annotation.
  ScopedAnnotation annotation("BefExecution");

  se::gpu::GpuStream* stream = se::gpu::AsGpuStream(run_options->stream());
  auto gpu_context = [&] {
    tensorflow::mutex_lock lock(bef_executable->mutex);
    return bef_executable->gpu_ctx_cache.GetOrCreate(
        stream->parent()->gpu_context()->context());
  }();
  auto gpu_stream =
      tfrt::gpu::MakeBorrowedStream(gpu_context.first, stream->gpu_stream());

  // Create execution context.
  auto req_ctx =
      tfrt::RequestContextBuilder(&bef_executable->host_ctx, gpu_context.second)
          .build();
  if (!req_ctx) {
    return tensorflow::errors::Internal(toString(req_ctx.takeError()));
  }
  tfrt::ExecutionContext exec_ctx(*req_ctx);

  // Create owning handles for arguments and add pointer to them to 'args'.
  const tfrt::Function* function = bef_executable->function;
  tfrt::SmallVector<tfrt::AsyncValue*, 8> args;
  args.reserve(function->num_arguments());
  tfrt::AsyncValueRef<tfrt::Chain> chain = tfrt::GetReadyChain();
  args.push_back(chain.GetAsyncValue());
  args.push_back(gpu_stream.get());
  llvm::SmallVector<tfrt::RCReference<tfrt::AsyncValue>, 8> buffers;
  for (size_t i = 0; i < num_allocations; i++) {
    auto input = buffer_allocations.GetDeviceAddress(i);
    buffers.push_back(CreateGpuBuffer(&input));
    args.push_back(buffers.back().get());
  }
  if (args.size() != function->num_arguments())
    return InternalError("Unexpected argument count.");

  // Create return chain.
  tfrt::RCReference<tfrt::AsyncValue> result;
  if (function->num_results() != 1)
    return InternalError("Unexpected result count.");

  // Capture errors and augment with source.
  std::string diag_str;
  llvm::raw_string_ostream diag_os(diag_str);
  llvm::SourceMgr src_mgr;
  mlir::SourceMgrDiagnosticHandler handler(src_mgr, &bef_executable->mlir_ctx,
                                           diag_os);

  // Execute the function.
  function->Execute(exec_ctx, args, {result});

  // Wait for async execution to complete.
  tfrt::Await(exec_ctx, llvm::makeArrayRef(result));

  // Report error if any, from handler and result.
  if (diag_os.tell()) return tensorflow::errors::Internal(diag_os.str());
  if (auto* error = result->GetErrorIfPresent())
    return tensorflow::errors::Internal(tfrt::StrCat(*error));

  return MaybeSyncAndProfile(
      run_options, start_micros,
      block_host_until_done ? run_options->stream() : nullptr);
}
#endif  // BEF_EXECUTABLE

StatusOr<ExecutionOutput> GpuExecutable::ExecuteAsyncOnStreamImpl(
    const ServiceExecutableRunOptions* run_options,
    VariantArguments arguments) {
  XLA_SCOPED_LOGGING_TIMER(absl::StrCat(
      "GpuExecutable::ExecuteAsyncOnStreamImpl(", module_name_, ")"));
  se::DeviceMemoryAllocator* const memory_allocator = run_options->allocator();
  // Force synchronous execution if the allocator requires it.
  const bool block_host_until_done =
      !memory_allocator->AllowsAsynchronousDeallocation();

  se::StreamExecutor* executor = run_options->stream()->parent();

  // Lock the GPU with a shared lock so that we don't interfere with autotuning
  // that may be running during JIT compilation while allowing multiple XLA
  // computations to use the same GPU simultaneously.
  auto gpu_lock = LockGpuShared(executor);

  const GpuExecutable::BufferAllocToDeviceMemoryMap* globals;
  {
    tensorflow::profiler::TraceMe hlo_module_activity(
        [&] { return std::string("Resolve constant globals"); },
        tensorflow::profiler::TraceMeLevel::kInfo);

    TF_ASSIGN_OR_RETURN(globals, ResolveConstantGlobals(run_options->stream()));
  }

  auto device_ordinal = executor->device_ordinal();
  ExecutionOutput result(/*on_device_shape=*/output_shape_, memory_allocator,
                         device_ordinal);

  TF_ASSIGN_OR_RETURN(BufferAllocations buffer_allocations,
                      GenerateBufferAllocations(arguments, globals,
                                                memory_allocator, executor));
  VLOG(2) << buffer_allocations.ToString();
  std::set<se::DeviceMemoryBase> buffers_in_result;

  const bool is_entire_tuple_contents_aliased = [&] {
    for (auto& p : result.MutableResult()->buffers().leaves()) {
      if (!output_info_.contains(p.first)) {
        continue;
      }
      const OutputInfo& output_info = output_info_.at(p.first);
      if (!output_info.alias_config.has_value()) {
        return false;
      }
    }
    return true;
  }();

  for (auto& p : result.MutableResult()->buffers()) {
    const ShapeIndex& index = p.first;
    if (!output_info_.contains(index)) {
      continue;
    }
    const OutputInfo& output_info = output_info_.at(index);
    const BufferAllocation* allocation =
        &allocations_[output_info.allocation_index];
    se::DeviceMemoryBase& result_buffer = p.second;

    VLOG(4) << "Looking at: allocation " << output_info.allocation_index
            << " @ index: " << index.ToString();

    if (output_info.alias_config) {
      MaybeOwningDeviceMemory* maybe_owning_memory =
          [&]() -> xla::MaybeOwningDeviceMemory* {
        // ScopedBuffer is never an owned buffer.
        if (auto* unowned_shapedbuffers =
                absl::get_if<absl::Span<const ShapedBuffer* const>>(
                    &arguments)) {
          return nullptr;
        } else {
          auto unowned_execution_input =
              absl::get<absl::Span<ExecutionInput>>(arguments);
          ExecutionInput& input =
              unowned_execution_input[allocation->parameter_number()];
          return input.MutableBuffer(allocation->param_shape_index());
        }
      }();
      if (output_info.alias_config->must_alias() && maybe_owning_memory &&
          !maybe_owning_memory->HasOwnership()) {
        return InvalidArgument(
            "An input was configured to be must-alias at "
            "compile time but not donated at runtime: allocation %d",
            output_info.allocation_index);
      }
      if (maybe_owning_memory && maybe_owning_memory->HasOwnership()) {
        absl::optional<tensorflow::se::OwningDeviceMemory> owning =
            maybe_owning_memory->Release();
        // If the caller passes the ownership of the device memory, reuse it
        // as the output buffer. It is up to the caller whether or not to
        // donate a buffer; the aliasing information describes which buffers
        // may alias, not buffers that must alias.
        se::DeviceMemoryBase argument_buffer = owning->Release();
        *maybe_owning_memory = argument_buffer;
        result_buffer = argument_buffer;
        // The caller is giving us the
        // input buffer, but in case of error from the execute call, we should
        // not be releasing it as it contains valid data (for example, it is a
        // parameter which the user wants us to alias, in a gradient update
        // computation). So we store the index into the result in the aliased
        // vector, which will be fed to the ExecutionOutput, which will use
        // the indices to drop the addresses from its own ScopedShapedBuffer
        // result, if the ExecutionOutput is not committed.
        result.AddAliasedIndex(index);
      } else if (!output_info.passthrough &&
                 !ShapeUtil::GetSubshape(output_shape_, index).IsTuple()) {
        // The guard is above is not to insert copy-protection when aliasing
        // pass-through params, as we do not need to write into the output
        // buffer.
        VLOG(3) << "Using copy-protection: aliasing is specified, but the "
                   "buffer is not donated; allocating a fresh buffer";
        int64_t allocation_size =
            ShapeUtil::ByteSizeOf(ShapeUtil::GetSubshape(output_shape_, index));
        StatusOr<se::OwningDeviceMemory> allocated_buffer =
            memory_allocator->Allocate(device_ordinal, allocation_size);
        if (!allocated_buffer.ok()) {
          return ResourceExhausted("%s\n%s\n",
                                   allocated_buffer.status().error_message(),
                                   verbose_buffer_assignment_string_dumper_());
        }
        result_buffer = allocated_buffer->Release();
        se::DeviceMemoryBase& aliased_buffer =
            buffer_allocations.GetMutableDeviceAddress(
                output_info.allocation_index);
        CHECK_EQ(aliased_buffer.size(), result_buffer.size());
        run_options->stream()->ThenMemcpyD2D(&result_buffer, aliased_buffer,
                                             aliased_buffer.size());
        aliased_buffer = result_buffer;
      }
    }

    if (result_buffer.is_null()) {
      // The source instruction should have a non-parameter buffer
      // assigned.
      result_buffer =
          buffer_allocations.GetDeviceAddress(output_info.allocation_index);

      // If the entire tuple contents is aliased, the copy insertion will *not*
      // materialize a new tuple, so we mark it as aliased as well.
      if (is_entire_tuple_contents_aliased) {
        result.AddAliasedIndex(index);
      }
    }
    buffers_in_result.insert(result_buffer);
  }

  TF_RETURN_IF_ERROR(ExecuteThunksOrBef(run_options, buffer_allocations,
                                        block_host_until_done));

  // Free all temporary allocations.
  TF_RETURN_IF_ERROR(
      buffer_allocations.TearDown(buffers_in_result, allocations_));

  // Free allocations for arguments.
  if (auto args = absl::get_if<absl::Span<ExecutionInput>>(&arguments)) {
    MarkToBeReleasedArguments(*args, result);
  }
  return std::move(result);
}

Status GpuExecutable::ExecuteThunksOrBef(
    const ServiceExecutableRunOptions* run_options,
    const BufferAllocations& buffer_allocations, bool block_host_until_done) {
  TF_RETURN_IF_ERROR(
      CheckCompatibilityWithServiceExecutableRunOptions(run_options));

  if (thunks_) {
    se::StreamExecutor* executor = run_options->stream()->parent();
    for (const std::unique_ptr<Thunk>& thunk : thunks_->TotalOrder()) {
      TF_RETURN_IF_ERROR(thunk->Initialize(*this, executor));
    }
    return ExecuteThunks(module_name_, *thunks_, run_options,
                         buffer_allocations, block_host_until_done);
  }

#if BEF_EXECUTABLE
  if (bef_executable_) {
    return ExecuteBef(module_name_, bef_executable_, run_options,
                      buffer_allocations, allocations_.size(),
                      block_host_until_done);
  }
#endif  // BEF_EXECUTABLE

  return FailedPrecondition("Expected thunk or bef is not supplied.");
}

int64_t GpuExecutable::SizeOfGeneratedCodeInBytes() const {
  // Non-empty PTX but empty cubin: compilation must have failed, return
  // "unknown".
  if (binary().empty() && !text_.empty()) {
    return -1;
  }
  int64_t size = binary().size();
  for (BufferAllocation::Index i = 0; i < allocations_.size(); ++i) {
    const BufferAllocation& allocation = allocations_[i];
    if (allocation.is_constant()) {
      size += allocation.size();
    }
  }
  return size;
}

Status GpuExecutable::SetUpMlirAllocation(
    mlir::FuncOp func, llvm::ArrayRef<int64_t> buffer_sizes,
    std::vector<BufferAllocation>* allocations,
    absl::flat_hash_map<ShapeIndex, GpuExecutable::OutputInfo>* output_info,
    Shape* output_shape, int buffer_param_offset) {
  for (int i = 0; i < buffer_sizes.size(); i++) {
    allocations->emplace_back(i, buffer_sizes[i], 0);
  }

  for (int i = 0; i < func.getNumArguments(); i++) {
    if (i < buffer_param_offset) {
      continue;
    }
    const int buffer_index = i - buffer_param_offset;

    if (auto param_attr = func.getArgAttr(i, "lmhlo.params")) {
      xla::ShapeIndex shape_index;
      if (auto shape_index_attr =
              func.getArgAttrOfType<mlir::DenseIntElementsAttr>(
                  i, "lmhlo.param_shape_index")) {
        for (const llvm::APInt& element : shape_index_attr) {
          shape_index.push_back(element.getSExtValue());
        }
      }
      allocations->at(buffer_index)
          .set_entry_computation_parameter(
              param_attr.cast<mlir::IntegerAttr>().getInt(), shape_index,
              static_cast<bool>(func.getArgAttr(i, "lmhlo.output_index")));
    }
    // TODO(timshen): this information is redundant. This is here only for
    // smooth migration to LMHLO. Remove it.
    if (func.getArgAttr(i, "lmhlo.constant_name")) {
      allocations->at(buffer_index).set_constant(true);
    }
    if (auto output_index_attr = func.getArgAttr(i, "lmhlo.output_index")) {
      allocations->at(buffer_index).set_maybe_live_out(true);

      // Reconstruct a shape index from output_index.
      ShapeIndex shape_index;
      for (const llvm::APInt& element :
           output_index_attr.cast<mlir::DenseIntElementsAttr>()) {
        shape_index.push_back(element.getSExtValue());
      }
      auto& o = (*output_info)[shape_index];
      o.allocation_index = buffer_index;
      if (auto param_attr = func.getArgAttr(i, "lmhlo.params")) {
        HloInputOutputAliasConfig::AliasKind kind =
            HloInputOutputAliasConfig::kMayAlias;
        if (func.getArgAttr(i, "lmhlo.must_alias")) {
          kind = HloInputOutputAliasConfig::kMustAlias;
        }
        o.alias_config.emplace(param_attr.cast<mlir::IntegerAttr>().getInt(),
                               ShapeIndex{}, kind);
      }
      if (func.getArgument(i).use_empty()) {
        o.passthrough = true;
      }
    }
  }
  // Expects result_xla_shape as a XLA shape in string form.
  //
  // The attribute is necessary, because GpuExecutable/ExecutionOutput supports
  // tuples / tree-like shapes, while the LMHLO argument list loses the tree
  // form.
  //
  // The string format is necessary since MLIR doesn't support XLA shape with
  // dynamic_dimension.
  //
  // TODO(timshen): now this field is mandatory. Make it optional for
  // non-GpuExecutable outputs.
  TF_ASSIGN_OR_RETURN(
      *output_shape,
      ParseShape(func->getAttrOfType<mlir::StringAttr>("result_xla_shape")
                     .getValue()
                     .str()));

  return Status::OK();
}

#if BEF_EXECUTABLE
static void ApplyEntryFunctionAttributes(
    mlir::MLIRContext& context, mlir::FuncOp& func,
    xla::EntryFunctionAttributes entry_func_attrs, int buffer_param_offset) {
  mlir::OpBuilder builder(&context);
  llvm::SmallVector<mlir::DictionaryAttr, 8> args_attrs;
  for (int i = 0; i < func.getNumArguments(); i++) {
    mlir::NamedAttrList arg_attr_list;
    if (i < buffer_param_offset) {
      args_attrs.push_back(arg_attr_list.getDictionary(&context));
      continue;
    }
    const auto& buffer = entry_func_attrs.buffers(i - buffer_param_offset);

    if (buffer.lmhlo_params_present()) {
      arg_attr_list.set("lmhlo.params",
                        builder.getIndexAttr(buffer.lmhlo_params()));
    }
    if (buffer.has_lmhlo_param_shape_index()) {
      arg_attr_list.set("lmhlo.param_shape_index",
                        builder.getI64TensorAttr(llvm::makeArrayRef(
                            buffer.lmhlo_param_shape_index().indices().begin(),
                            buffer.lmhlo_param_shape_index().indices().end())));
    }
    if (!buffer.lmhlo_constant_name().empty()) {
      arg_attr_list.set("lmhlo.constant_name",
                        builder.getStringAttr(buffer.lmhlo_constant_name()));
    }
    if (buffer.lmhlo_must_alias()) {
      arg_attr_list.set("lmhlo.must_alias", builder.getUnitAttr());
    }
    if (buffer.has_lmhlo_output_index()) {
      arg_attr_list.set("lmhlo.output_index",
                        builder.getI64TensorAttr(llvm::makeArrayRef(
                            buffer.lmhlo_output_index().indices().begin(),
                            buffer.lmhlo_output_index().indices().end())));
    }
    args_attrs.push_back(arg_attr_list.getDictionary(&context));
  }
  func.setAllArgAttrs(args_attrs);
  func->setAttr("result_xla_shape",
                builder.getStringAttr(entry_func_attrs.result_xla_shape()));
}
#endif  // BEF_EXECUTABLE

StatusOr<std::unique_ptr<Executable>> GpuExecutable::LoadFromBef(
    std::shared_ptr<HloModule> hlo_module, absl::string_view bef,
    xla::EntryFunctionAttributes entry_func_attrs, GpuVersion gpu_version) {
#if BEF_EXECUTABLE
  OwnedBefBuffer bef_buffer = [bef]() {
    auto ptr = static_cast<uint8_t*>(
        tfrt::AlignedAlloc(tfrt::GetRequiredBefAlignment(), bef.size()));
    std::copy(bef.begin(), bef.end(), ptr);
    return OwnedBefBuffer(ptr, {bef.size()});
  }();

  mlir::MLIRContext context;
  context.allowUnregisteredDialects();
  mlir::Location location = mlir::UnknownLoc::get(&context);
  llvm::ArrayRef<uint8_t> bef_array(bef_buffer.get(),
                                    bef_buffer.get_deleter().size);
  auto module = tfrt::ConvertBEFToMLIR(location, bef_array, &context);
  TF_ASSIGN_OR_RETURN(BefExecutable * bef_executable,
                      BefExecutable::Create(std::move(bef_buffer)));
  auto func = mlir::cast<mlir::FuncOp>(
      module->lookupSymbol(bef_executable->entry_point.function_name));
  // In tfrt_gpu dialect, buffer arguments start from the third parameter (after
  // tfrt::Chain and GpuStream).
  int buffer_param_offset = 2;
  ApplyEntryFunctionAttributes(context, func, entry_func_attrs,
                               buffer_param_offset);

  std::vector<BufferAllocation> allocations;
  absl::flat_hash_map<ShapeIndex, OutputInfo> output_info;
  Shape result_xla_shape;
  TF_RETURN_IF_ERROR(SetUpMlirAllocation(
      func, bef_executable->entry_point.buffer_sizes, &allocations,
      &output_info, &result_xla_shape, buffer_param_offset));

  std::unique_ptr<Executable> executable;
  std::string module_name = mlir::GetNameFromLoc(module->getLoc());
  // Calling private constructor.
  executable = absl::WrapUnique(
      new GpuExecutable(std::move(hlo_module), gpu_version, entry_func_attrs,
                        module_name, result_xla_shape, std::move(allocations),
                        std::move(output_info), bef_executable));
  return executable;
#else   // BEF_EXECUTABLE
  return FailedPrecondition("LoadFromBef only supported with BEF_EXECUTABLE");
#endif  // BEF_EXECUTABLE
}

StatusOr<absl::flat_hash_map<ShapeIndex, GpuExecutable::OutputInfo>>
GetOutputInfo(const HloModule& hlo_module, const BufferAssignment& assignment) {
  const HloInstruction* root =
      hlo_module.entry_computation()->root_instruction();

  InstructionValueSet root_value_set =
      assignment.dataflow_analysis().GetInstructionValueSet(root);

  if (root_value_set.IsAmbiguous()) {
    return Unimplemented("Points-to set of root instruction is ambiguous");
  }

  using OutputInfoMap =
      absl::flat_hash_map<ShapeIndex, GpuExecutable::OutputInfo>;
  OutputInfoMap output;
  TF_RETURN_IF_ERROR(ShapeUtil::ForEachSubshapeWithStatus(
      root->shape(),
      [&](const Shape& /*sub_shape*/, const ShapeIndex& index) -> Status {
        const auto& sources = root_value_set.element(index);
        // The points-to set is unambiguous so the set should be a
        // singleton. That is, we know exactly which instruction
        // produced the array at this element.
        CHECK_EQ(1, sources.values().size());
        HloInstruction* src_hlo = sources.values()[0]->instruction();

        GpuExecutable::OutputInfo& info = output[index];
        info.passthrough = src_hlo->opcode() == HloOpcode::kParameter;
        TF_ASSIGN_OR_RETURN(
            const BufferAllocation::Slice slice,
            assignment.GetUniqueSlice(src_hlo, sources.values()[0]->index()));
        CHECK_EQ(slice.offset(), 0) << "Parameter should get its own slice";
        info.allocation_index = slice.index();

        output[index].alias_config =
            hlo_module.input_output_alias_config().GetAliasedParameter(index);

        return Status::OK();
      }));
  return output;
}

}  // namespace gpu
}  // namespace xla
