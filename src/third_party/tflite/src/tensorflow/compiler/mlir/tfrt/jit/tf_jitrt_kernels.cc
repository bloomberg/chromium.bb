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

#define EIGEN_USE_THREADS

#include <memory>
#include <string>
#include <utility>

#include "mlir/Dialect/Async/IR/AsyncTypes.h"
#include "mlir/Dialect/Bufferization/Transforms/Bufferize.h"
#include "mlir/ExecutionEngine/AsyncRuntime.h"
#include "tensorflow/compiler/jit/flags.h"
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_jitrt.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_jitrt_kernels_registration.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_jitrt_pipeline.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_jitrt_request_context.h"
#include "tensorflow/compiler/mlir/tfrt/jit/transforms/tf_jitrt_passes.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/platform/dynamic_annotations.h"
#include "tensorflow/core/platform/threadpool.h"
#include "tensorflow/core/profiler/lib/traceme.h"
#include "tensorflow/core/runtime_fallback/kernel/kernel_fallback_compat_request_state.h"
#include "tensorflow/core/tfrt/utils/fallback_tensor.h"
#include "tfrt/jitrt/async_runtime.h"  // from @tf_runtime
#include "tfrt/jitrt/async_runtime_api.h"  // from @tf_runtime
#include "tfrt/jitrt/jitrt.h"  // from @tf_runtime
#include "tfrt/jitrt/jitrt_compiler.h"  // from @tf_runtime
#include "tfrt/dtype/dtype.h"  // from @tf_runtime
#include "tfrt/host_context/async_dispatch.h"  // from @tf_runtime
#include "tfrt/host_context/async_value_ref.h"  // from @tf_runtime
#include "tfrt/host_context/chain.h"  // from @tf_runtime
#include "tfrt/host_context/execution_context.h"  // from @tf_runtime
#include "tfrt/host_context/host_buffer.h"  // from @tf_runtime
#include "tfrt/host_context/host_context.h"  // from @tf_runtime
#include "tfrt/host_context/kernel_registry.h"  // from @tf_runtime
#include "tfrt/host_context/kernel_utils.h"  // from @tf_runtime
#include "tfrt/host_context/shared_context.h"  // from @tf_runtime
#include "tfrt/support/error_util.h"  // from @tf_runtime
#include "tfrt/support/forward_decls.h"  // from @tf_runtime
#include "tfrt/support/rc_array.h"  // from @tf_runtime
#include "tfrt/support/string_util.h"  // from @tf_runtime
#include "tfrt/tensor/tensor_metadata.h"  // from @tf_runtime
#include "tfrt/tensor/tensor_shape.h"  // from @tf_runtime

namespace tensorflow {
namespace {

#if __cplusplus >= 201703L
using ::std::any_cast;
#else
using ::llvm::any_cast;
#endif

using ::llvm::Expected;
using ::llvm::None;
using ::llvm::Optional;

using ::tfrt::Argument;
using ::tfrt::ArrayRef;
using ::tfrt::AsyncValue;
using ::tfrt::AsyncValuePtr;
using ::tfrt::AsyncValueRef;
using ::tfrt::Attribute;
using ::tfrt::Chain;
using ::tfrt::CompilationUnitAttribute;
using ::tfrt::DecodedDiagnostic;
using ::tfrt::DType;
using ::tfrt::EmitErrorAsync;
using ::tfrt::ExecutionContext;
using ::tfrt::HostContext;
using ::tfrt::IndirectAsyncValue;
using ::tfrt::KernelRegistry;
using ::tfrt::MakeAvailableAsyncValueRef;
using ::tfrt::MakeConstructedAsyncValueRef;
using ::tfrt::MakeErrorAsyncValueRef;
using ::tfrt::MakeStringError;
using ::tfrt::RCArray;
using ::tfrt::RCReference;
using ::tfrt::RemainingResults;
using ::tfrt::RepeatedArguments;
using ::tfrt::RequestContext;
using ::tfrt::SharedContext;
using ::tfrt::StrCat;
using ::tfrt::StringAttribute;
using ::tfrt::TaskFunction;

using ::tfrt::jitrt::CompilationOptions;
using ::tfrt::jitrt::CompilationPipelineOptions;
using ::tfrt::jitrt::CreateDefaultJitRtCompilationPipeline;
using ::tfrt::jitrt::EigenThreadPoolAsyncTaskRunner;
using ::tfrt::jitrt::Executable;
using ::tfrt::jitrt::JitExecutable;
using ::tfrt::jitrt::JitExecutableCache;
using ::tfrt::jitrt::MemrefDesc;
using ::tfrt::jitrt::OperandConstraint;
using ::tfrt::jitrt::RegisterDefaultJitRtDialects;
using ::tfrt::jitrt::ReturnErrors;
using ::tfrt::jitrt::ReturnStridedMemref;
using ::tfrt::jitrt::ReturnValueConversion;
using ::tfrt::jitrt::SpecializationListener;
using ::tfrt::jitrt::StaticReturnValueConverter;

using ::tensorflow::profiler::TraceMe;
using ::tensorflow::profiler::TraceMeEncode;
using ::tensorflow::tfd::KernelFallbackCompatRequestState;
using ::tensorflow::tfrt_stub::FallbackTensor;
using ::tensorflow::thread::ThreadPool;

// -------------------------------------------------------------------------- //
// Dedicated thread pool for running compilation tasks.
// -------------------------------------------------------------------------- //

class CompilationThreadPool : public SharedContext {
 public:
  explicit CompilationThreadPool(HostContext* host) { Reset(); }

  static CompilationThreadPool& Get(HostContext* host) {
    return host->GetOrCreateSharedContext<CompilationThreadPool>();
  }

  template <typename Task>
  void Schedule(Task&& task) {
    // Because compilation tasks can capture move only types, and Tensorflow
    // thread pool requires std::function tasks, we have to do manual memory
    // management here.
    auto ptr = std::make_unique<Task>(std::forward<Task>(task));
    thread_pool_->Schedule([ptr = ptr.release()]() {
      (*ptr)();
      delete ptr;
    });
  }

  // This is an unsafe function intended only for use in tests. It is undefined
  // behavior to call it concurrently with `Schedule`.
  void Reset() {
    thread_pool_ = std::make_unique<ThreadPool>(
        Env::Default(), "tf-jitrt-compiler", /*num_threads=*/32);
  }

 private:
  std::unique_ptr<ThreadPool> thread_pool_;
};

// -------------------------------------------------------------------------- //
// JIT compiled kernels use Eigen ThreadPool managed by the kernel fallback as
// an async runtime worker threads.
// -------------------------------------------------------------------------- //

static Expected<Eigen::ThreadPoolInterface*> GetWorkerThreads(
    const ExecutionContext& exec_ctx) {
  RequestContext* req_ctx = exec_ctx.request_ctx();

  auto* fallback = req_ctx->GetDataIfExists<KernelFallbackCompatRequestState>();
  if (LLVM_UNLIKELY(!fallback))
    return MakeStringError("fallback request state was not found");

  // Return user provided intra op thread pool if it is available.
  if (LLVM_LIKELY(fallback->intra_op_threadpool()))
    return fallback->intra_op_threadpool();

  // Otherwise find the default CPU device in the device manager.
  Device* host_cpu = fallback->device_manager().HostCPU();
  assert(host_cpu && "fallback state must have a valid host cpu device");

  const Eigen::ThreadPoolDevice* eigen = host_cpu->eigen_cpu_device();
  assert(eigen && "host cpu device must have a valid Eigen thread pool device");

  return eigen->getPool();
}

// -------------------------------------------------------------------------- //
// Compile compilation unit attribute to an executable result.
// -------------------------------------------------------------------------- //

// Options for the `tf-jitrt-pipeline`. We do not use MLIR pass options directly
// because they are not copyable or movable, and we need to pass them cheaply
// across the async compilation tasks boundary.
struct TfJitRtPipelineOpts {
  bool vectorize;
  bool legalize_i1_tensors;
};

// Prints memref descriptor as a tensor type: tensor<NxMxf32>.
static std::string AsTensorType(const MemrefDesc& desc) {
  std::string str;
  llvm::raw_string_ostream os(str);

  os << "tensor<";
  for (ssize_t size : desc.sizes) os << size << "x";
  os << desc.dtype;
  os << ">";

  return str;
}

// Print memref descriptor content to trace value specializations.
static std::string AsTensorContent(const MemrefDesc& desc) {
  std::string str;
  llvm::raw_string_ostream os(str);

  auto print_0d = [&](auto type_tag) {
    os << desc.dtype << ": " << *static_cast<decltype(type_tag)*>(desc.data);
  };

  auto print_1d = [&](auto type_tag) {
    os << desc.dtype << ": [";
    for (size_t i = 0; i < desc.sizes[0]; ++i) {
      if (i != 0) os << ",";
      os << static_cast<decltype(type_tag)*>(desc.data)[i];
    }
    os << "]";
  };

  auto type_dispatch = [&](auto functor) {
    switch (desc.dtype) {
      case DType::I32:
        functor(int32_t{});
        break;
      case DType::I64:
        functor(int64_t{});
        break;
      default:
        os << "<unsupported dtype " << desc.dtype << ">";
    }
  };

  size_t rank = desc.sizes.size();

  switch (rank) {
    case 0:
      type_dispatch(print_0d);
      break;
    case 1:
      type_dispatch(print_1d);
      break;
    default:
      os << "<unsupported rank " << desc.sizes.size() << ">";
  }

  return str;
}

// Gets the session name from the fallback request state.
static const std::string GetSessionName(RequestContext* req_ctx) {
  auto* fallback = req_ctx->GetDataIfExists<KernelFallbackCompatRequestState>();
  if (!fallback) return "<unknown>";

  return fallback->session_metadata().name();
}

static Expected<AsyncValuePtr<JitExecutable>> CompileImpl(
    const CompilationUnitAttribute& kernel, const ExecutionContext& exec_ctx,
    const Optional<TfJitRtPipelineOpts>& opts = None) {
  // Request context must be initialized with the tf_jitrt state.
  auto* state = exec_ctx.request_ctx()->GetDataIfExists<TfJitRtRequestState>();
  if (LLVM_UNLIKELY(!state))
    return MakeStringError("tf_jitrt state not found in the request context");

  // We rely on the unique `id` provided by the CompilationUnitAttribute to look
  // up the JitExecutable in the cache. This id is guaranteed to be unique
  // within a Bef file. Currently we rely on the fact that the SavedModel
  // never unloads a Bef file, and there is a 1-to-1 relationship between the
  // ResourceContext and the SavedModel.
  //
  // TODO(b/206081322): Different compilation options should create unique
  // compiled kernel cache keys.
  size_t key = kernel.id();

  JitExecutableCache* jit_executable_cache = state->jit_executable_cache;

  // Maybe return JitExecutable from the cache.
  auto cached = jit_executable_cache->Find(key);
  if (LLVM_LIKELY(cached)) return cached;

  // Get the worker threads from the execution context. Do this before
  // allocating an async value to make sure that we can try to instantiate the
  // executable.
  Expected<Eigen::ThreadPoolInterface*> worker_threads =
      GetWorkerThreads(exec_ctx);
  if (auto err = worker_threads.takeError()) return std::move(err);

  // Allocate a placeholder for the compiled JitExecutable.
  JitExecutableCache::Entry entry = jit_executable_cache->Allocate(key);

  // We lost the race; some other invocation will do the compilation.
  if (!entry.allocated) return entry.ptr;

  // Given that compilation happens asynchronously, passing (or capturing) these
  // by value prevents use-after-free errors.
  struct KernelInfo {
    intptr_t id;
    std::string entrypoint;
    std::string name;
    std::string serialized_operation;
  } kernel_info;

  // We only support functions nested in top level compiled module.
  if (kernel.nested_symbols().size() != 1)
    return MakeStringError(
        "kernel function has to be defined in a top-level module");

  // TODO(ecg): use designed initializers + const when C++20 is adopted.
  kernel_info.id = kernel.id();
  kernel_info.entrypoint = kernel.nested_symbols()[0];
  kernel_info.name = kernel.root_symbol();
  kernel_info.serialized_operation = kernel.serialized_operation();

  // Compilation (specialized executable compilation) events should be rare, so
  // we can afford to do detailed tracing for every compilation. If compilation
  // events happen too often, it is a much larger problem than the excessive
  // tracing.

  // Custom runner for compiling specializations that schedules compilation task
  // into the dedicated thread pool and adds tracing.
  auto runner = [kernel_info](size_t specialization,
                              ArrayRef<OperandConstraint> constraints,
                              ArrayRef<MemrefDesc> operands,
                              TaskFunction compile,
                              JitExecutable::UserData user_data) {
    assert(operands.size() == constraints.size());

    // Get the context of the request that triggered specialization compilation.
    RequestContext* req_ctx = any_cast<RequestContext*>(user_data);
    HostContext* host = req_ctx->host();

    // Prepare arguments for the compilation tracing in the caller thread,
    // because operands lifetime is shorter than the compilation task.
    using SpecializationArg = std::pair<std::string, std::string>;
    llvm::SmallVector<SpecializationArg> args;
    args.reserve(operands.size());

    // Trace types of all operands of the specialization.
    for (size_t i = 0; i < operands.size(); ++i)
      args.emplace_back(StrCat("%arg", i, " type"), AsTensorType(operands[i]));

    // Trace content of all operands that require value specializations.
    for (size_t i = 0; i < constraints.size(); ++i) {
      if (constraints[i] != OperandConstraint::kValue) continue;
      args.emplace_back(StrCat("%arg", i, " value"),
                        AsTensorContent(operands[i]));
    }

    // Schedule specialization compilation task into the dedicated thread pool.
    CompilationThreadPool& thread_pool = CompilationThreadPool::Get(host);

    thread_pool.Schedule(
        [kernel_info, specialization, request_id = req_ctx->id(),
         session_name = GetSessionName(req_ctx), compile = std::move(compile),
         args = std::move(args)]() mutable {
          TraceMe trace_me([&] {
            return TraceMeEncode("tf_jitrt.CompileSpecialization",
                                 {{"id", request_id},
                                  {"kernel_id", kernel_info.id},
                                  {"executable", kernel_info.name},
                                  {"specialization", specialization}});
          });

          for (SpecializationArg& arg : args) {
            trace_me.AppendMetadata([&] {
              return TraceMeEncode({{arg.first, arg.second}});
            });
          }

          trace_me.AppendMetadata([&] {
            return TraceMeEncode({{"src", kernel_info.serialized_operation}});
          });

          auto compile_start_time = absl::Now();
          LOG(INFO) << "Started JitExecutable specialization compilation for "
                    << kernel_info.name << " (" << session_name << ")";
          compile();
          auto compile_duration = absl::Now() - compile_start_time;

          LOG(INFO) << "JitExecutable specialization compilation for "
                    << kernel_info.name << " took "
                    << absl::ToInt64Milliseconds(compile_duration) << " ms ("
                    << session_name << ")";

          if (compile_duration > absl::Seconds(1))
            LOG(INFO) << "Expensive JitExecutable specialization compilation ("
                      << absl::ToInt64Milliseconds(compile_duration)
                      << " ms):\n"
                      << kernel_info.serialized_operation;

          RecordCompileTime(session_name, kernel_info.name, specialization,
                            compile_duration);
        });
  };

  HostContext* host = exec_ctx.host();
  RequestContext* req_ctx = exec_ctx.request_ctx();

  // Compile kernel asynchronously in the compilation thread pool.
  CompilationThreadPool& thread_pool = CompilationThreadPool::Get(host);

  thread_pool.Schedule([kernel_info, runner, workers = *worker_threads,
                        ref = entry.ptr.CopyRef(), request_id = req_ctx->id(),
                        session_name = GetSessionName(req_ctx),
                        tf_jitrt_opts = opts]() {
    TraceMe trace_me([&] {
      return TraceMeEncode("tf_jitrt.CompileDefault",
                           {{"id", request_id},
                            {"kernel_id", kernel_info.id},
                            {"executable", kernel_info.name},
                            {"src", kernel_info.serialized_operation}});
    });

    // Options for the default JitRt compilation pipeline (lowering to LLVM).
    CompilationPipelineOptions copts;
    copts.alignment = EIGEN_MAX_ALIGN_BYTES;  // Eigen included by tensor.h
    copts.num_worker_threads = workers->NumThreads();
    copts.cost_driven_async_parallel_for =
        GetJitRtFlags().cost_driven_async_parallel_for;

    // Options for the JitRt JitExecutable compilation.
    CompilationOptions opts;
    opts.specialization = GetJitRtFlags().always_specialize
                              ? CompilationOptions::Specialization::kAlways
                              : CompilationOptions::Specialization::kEnabled;

    // Register dialects and interfaces required for the compilation pipeline.
    opts.register_dialects = [](mlir::DialectRegistry& registry) {
      mlir::RegisterAllTensorFlowDialects(registry);
      RegisterDefaultJitRtDialects(registry);
    };

    // Register a custom pipeline for lowering from Tensorflow dialect to LLVM.
    opts.create_compilation_pipeline = [=](mlir::PassManager& pm) {
      TfJitRtPipelineOptions opts;
      if (tf_jitrt_opts) {
        opts.vectorize = tf_jitrt_opts->vectorize;
        opts.legalize_i1_tensors = tf_jitrt_opts->legalize_i1_tensors;
      } else {
        opts.vectorize = GetJitRtFlags().vectorize;
      }

      // Lower from Tensorflow to Linalg on buffers.
      CreateTfJitRtPipeline(pm, opts);

      // Use default JitRt compilation pipeline to lower to LLVM.
      CreateDefaultJitRtCompilationPipeline(pm, copts);
    };

    // Register a custom pipeline to propagate specialization information.
    opts.create_specialization_pipeline = CreateJitRtSpecializationPipeline;

    // When lowering Tensorflow functions to JitRt we convert all input and
    // result tensors to memrefs, and add a kernel context input.
    opts.calling_convention = CompilationOptions::DefaultCallingConvention(
        mlir::bufferization::BufferizeTypeConverter());

    // Instantiate new JitExecutable from the MLIR source.
    auto compile_start_time = absl::Now();
    LOG(INFO) << "Started JitExecutable instantiation compilation for "
              << kernel_info.name << " (" << session_name << ")";
    Expected<JitExecutable> jit_executable = JitExecutable::Instantiate(
        kernel_info.serialized_operation, kernel_info.entrypoint,
        std::move(opts), session_name, runner);
    auto compile_duration = absl::Now() - compile_start_time;

    LOG(INFO) << "JitExecutable instantiation for " << kernel_info.name
              << " took " << absl::ToInt64Milliseconds(compile_duration)
              << " ms (" << session_name << ")";

    if (compile_duration > absl::Seconds(1))
      LOG(INFO) << "Expensive JitExecutable instantiation ("
                << absl::ToInt64Milliseconds(compile_duration) << " ms):\n"
                << kernel_info.serialized_operation;

    RecordCompileTime(session_name, kernel_info.name, absl::nullopt,
                      compile_duration);

    // Set the entry async value state to error or concrete.
    if (auto err = jit_executable.takeError())
      ref.SetError(std::move(err));
    else
      ref.emplace(std::move(*jit_executable));
  });

  return entry.ptr;
}

// -------------------------------------------------------------------------- //
// TFRT kernel function definition for tf_jitrt.fallback.compile operation.
// -------------------------------------------------------------------------- //

// Compiles kernel into the JitExecutable and updates JitExecutableCache.
static AsyncValueRef<Chain> Compile(StringAttribute device,
                                    CompilationUnitAttribute kernel,
                                    const ExecutionContext& exec_ctx) {
  // Trigger kernel compilation, that will update the JitExecutableCache.
  Expected<AsyncValuePtr<JitExecutable>> executable =
      CompileImpl(kernel, exec_ctx);

  // Return error if can't schedule the compilation task.
  if (auto err = executable.takeError())
    return MakeErrorAsyncValueRef(StrCat(err));

  // Mark chain available once we compile the default executable.
  auto chain = MakeConstructedAsyncValueRef<Chain>();
  executable->AndThen([chain]() { chain.SetStateConcrete(); });

  return chain;
}

// -------------------------------------------------------------------------- //
// TFRT kernel function definition for tf_jitrt.test.wait_for_compilation.
// -------------------------------------------------------------------------- //

static AsyncValueRef<Chain> WaitForCompilation(
    Argument<Chain> chain, CompilationUnitAttribute kernel,
    const ExecutionContext& exec_ctx) {
  // Request context must be initialized with the tf_jitrt state.
  auto* state = exec_ctx.request_ctx()->GetDataIfExists<TfJitRtRequestState>();
  if (!state)
    return EmitErrorAsync(exec_ctx,
                          "tf_jitrt state not found in the request context");

  // Wait for the completion of all compilation tasks.
  JitExecutableCache* jit_executable_cache = state->jit_executable_cache;
  if (auto cached = jit_executable_cache->Find(kernel.id()))
    return cached->AllExecutablesCompiled();

  return MakeAvailableAsyncValueRef<Chain>();
}

// -------------------------------------------------------------------------- //
// TFRT kernel function for tf_jitrt.test.reset_compilation_thread_pool.
// -------------------------------------------------------------------------- //

static AsyncValueRef<Chain> ResetCompilationThreadPool(
    Argument<Chain> chain, const ExecutionContext& exec_ctx) {
  // Make sure that we reset the compilation thread pool only from a thread pool
  // (concurrent work queue) managed by the HostContext.
  return EnqueueWork(exec_ctx, [host = exec_ctx.host()]() -> Chain {
    CompilationThreadPool::Get(host).Reset();
    return {};
  });
}

// -------------------------------------------------------------------------- //
// Execute compiled JitRt kernels with Fallback Runtime interop.
// -------------------------------------------------------------------------- //

using ReturnTensorflowTensor =
    ReturnValueConversion<TensorflowConversionContext,
                          ReturnStridedMemref<ConvertTensor>>;

using TensorflowReturnValueConverter =
    StaticReturnValueConverter<TensorflowConversionContext,
                               ReturnTensorflowTensor>;

// Converts Tensor to the Memref Descriptor and verifies that the Tensor
// value is compatible with the memref type.
static void ConvertTensorToMemrefDesc(const tensorflow::Tensor& tensor,
                                      MemrefDesc* memref) {
  memref->dtype = tfd::GetTfrtDtype(tensor.dtype());
  memref->data = const_cast<void*>(tensor.data());
  memref->offset = 0;

  int rank = tensor.dims();
  memref->sizes.resize_for_overwrite(rank);
  memref->strides.resize_for_overwrite(rank);

  // Fill memref sizes and compute strides from the tensor dimensions.
  int64_t multiplier = 1;
  for (int i = rank - 1; i >= 0; --i) {
    int64_t dim_size = tensor.dim_size(i);
    memref->sizes[i] = dim_size;
    memref->strides[i] = multiplier;
    multiplier *= dim_size;
  }
}

static void ConvertTensorOperandsToMemrefDesc(
    RepeatedArguments<FallbackTensor> operands,
    llvm::SmallVectorImpl<MemrefDesc>* memrefs) {
  assert(memrefs->empty() && "memrefs must be empty");
  memrefs->resize(operands.size());

  for (unsigned i = 0; i < operands.size(); ++i)
    ConvertTensorToMemrefDesc(operands[i].tensor(), &(*memrefs)[i]);
}

struct DebugListener : public SpecializationListener {
  void notifyModuleSpecialized(
      ArrayRef<mlir::Type> operands,
      ArrayRef<mlir::DictionaryAttr> attrs) const override {
    std::string message;
    llvm::raw_string_ostream os(message);
    os << "Specialized operands:\n";
    for (auto& tuple : llvm::enumerate(llvm::zip(operands, attrs))) {
      mlir::Type type = std::get<0>(tuple.value());
      mlir::Attribute attr = std::get<1>(tuple.value());
      os << "%arg" << tuple.index() << ": " << type << " " << attr << "\n";
    }
    printf("%s", message.c_str());
    fflush(stdout);
  }

  void notifyValueSpecialized(unsigned index, mlir::Type type,
                              mlir::Attribute value) const override {
    std::string message;
    llvm::raw_string_ostream(message) << "%arg" << index << " "
                                      << "value specialized: " << value << "\n";
    printf("%s", message.c_str());
    fflush(stdout);
  }
};

// Emits diagnostics for the kernel invocation and returns error for all
// remaining results.
template <typename Error>
static void ReturnErrors(RemainingResults results, Error error,
                         const ExecutionContext& exec_ctx) {
  EmitError(exec_ctx, StrCat(error));
  ReturnErrors(results, std::move(error));
}

static void ExecuteImpl(Executable& executable,
                        const llvm::SmallVectorImpl<MemrefDesc>& memrefs,
                        RepeatedArguments<FallbackTensor> operands,
                        RemainingResults results,
                        const ExecutionContext& exec_ctx) {
  // Bind execution trace to the request context.
  TraceMe trace_me([&] {
    int64_t id = exec_ctx.request_ctx()->id();
    absl::string_view name(executable.name().data(), executable.name().size());
    return TraceMeEncode(
        "tf_jitrt.Execute",
        {{"id", id},
         {"executable", name},
         {"specialization", !executable.specialization().hasValue()
                                ? "default"
                                : std::to_string(*executable.specialization())},
         {"time_to_compile_ms", executable.time_to_compile().count()}});
  });

  // TODO(ezhulenev): Conversion context and async task runner might not outlive
  // the execution of all async tasks, and should be kept alive until all tasks
  // are completed, which will require heap allocation(s).
  assert(!executable.IsAsync() && "async executables are not yet supported");

  // Keep track of memory address to tensor mapping for result conversion.
  TensorflowConversionContext ctx(operands.size(), results.size());
  for (auto& t : operands)
    ctx.runtime_tensors.insert({t.tensor().data(), &t.tensor()});

  TensorflowReturnValueConverter converter(results, ctx);

  // Get the worker threads from the execution context.
  Expected<Eigen::ThreadPoolInterface*> worker_threads =
      GetWorkerThreads(exec_ctx);

  if (LLVM_UNLIKELY(!worker_threads))
    return ReturnErrors(results, worker_threads.takeError(), exec_ctx);

  // Use Eigen thread pool to execute all async tasks.
  EigenThreadPoolAsyncTaskRunner async_task_runner(*worker_threads);

  Executable::ExecuteOpts opts;
  opts.async_task_runner = &async_task_runner;
  opts.kernel_context = &ctx;

  // Execution error automatically forwarded to all results, we only need to
  // notify the HostContext to emit the diagnostics for the kernel invocation.
  auto err = executable.Execute(memrefs, converter, opts);
  if (LLVM_UNLIKELY(err)) {
    EmitError(exec_ctx, StrCat(err));
    return;
  }
}

// Gets a specialized Executable async value from the JitExecutable, and then
// dispatches it inline or using and-then continuation depending on the async
// value state.
static void ExecuteImpl(JitExecutable& jit_executable,
                        RepeatedArguments<FallbackTensor> operands,
                        RemainingResults results,
                        const ExecutionContext& exec_ctx, bool debug) {
  // Convert Tensor operands to memref descriptors.
  llvm::SmallVector<MemrefDesc> memrefs;
  ConvertTensorOperandsToMemrefDesc(operands, &memrefs);

  // Get an executable that might be specialized to the operands.
  DebugListener debug_listener;

  // Pass request context to the compilation task runner.
  JitExecutable::UserData user_data = exec_ctx.request_ctx();

  Expected<AsyncValuePtr<Executable>> executable = jit_executable.GetExecutable(
      memrefs, user_data, debug ? &debug_listener : nullptr);

  if (LLVM_UNLIKELY(!executable))
    return ReturnErrors(results, executable.takeError(), exec_ctx);

  // If executable is available execute it inline ...
  if (LLVM_LIKELY(executable->IsConcrete()))
    return ExecuteImpl(executable->get(), memrefs, operands, results, exec_ctx);

  // ... or maybe return errors.
  if (LLVM_UNLIKELY(executable->IsError()))
    return ReturnErrors(results, executable->GetError(), exec_ctx);

  // Otherwise execute it when the executable will become available. This
  // requires careful lifetime extension of all async values passed as operands
  // to the kernel (and also results that will become available asynchronously).

  // Allocate indirect async values for all results, we'll forward them to the
  // actual async values computed by the executable later.
  for (unsigned i = 0; i < results.size(); ++i)
    results.AllocateIndirectResultAt(i);

  // Call executable when it's ready with the original operands.
  executable->AndThen([exec_ctx, executable = *executable,
                       memrefs = std::move(memrefs),
                       r = RCArray<AsyncValue>(results.values()),
                       o = RCArray<AsyncValue>(operands.values())] {
    // Allocate storage for the executable results.
    llvm::SmallVector<RCReference<AsyncValue>> results_storage;
    results_storage.resize(r.size());

    // Reconstruct arguments and results from captured async values.
    RepeatedArguments<FallbackTensor> operands(o.values());
    RemainingResults results(results_storage);

    if (executable.IsError()) {
      ReturnErrors(results, executable.GetError(), exec_ctx);
    } else {
      ExecuteImpl(*executable, memrefs, operands, results, exec_ctx);
    }

    // Forward previously allocated indirect results to the actual results.
    for (unsigned i = 0; i < r.size(); ++i)
      llvm::cast<IndirectAsyncValue>(*r[i]).ForwardTo(
          std::move(results_storage[i]));
  });
}

// Gets a JitExecutable async value from the cache, and then dispatches it
// inline or using and-then continuation depending on the async value state.
static void ExecuteImpl(RepeatedArguments<FallbackTensor> operands,
                        RemainingResults results, const StringAttribute& device,
                        const CompilationUnitAttribute& kernel,
                        const ExecutionContext& exec_ctx, bool debug = false,
                        const Optional<TfJitRtPipelineOpts>& opts = None) {
  // Compile kernel module into the JitExecutable.
  Expected<AsyncValuePtr<JitExecutable>> jit_executable =
      CompileImpl(kernel, exec_ctx, opts);

  if (LLVM_UNLIKELY(!jit_executable))
    return ReturnErrors(results, jit_executable.takeError(), exec_ctx);

  // If kernel is available execute it inline ...
  if (LLVM_LIKELY(jit_executable->IsConcrete()))
    return ExecuteImpl(**jit_executable, operands, results, exec_ctx, debug);

  // ... or maybe return errors.
  if (LLVM_UNLIKELY(jit_executable->IsError()))
    return ReturnErrors(results, jit_executable->GetError(), exec_ctx);

  // Otherwise execute it when the executable will become available. This
  // requires careful lifetime extension of all async values passed as operands
  // to the kernel (and also results that will become available asynchronously).

  // Allocate indirect async values for all results, we'll forward them to the
  // actual async values computed by the executable later.
  for (unsigned i = 0; i < results.size(); ++i)
    results.AllocateIndirectResultAt(i);

  // Call executable when it's ready with the original operands.
  jit_executable->AndThen([exec_ctx, jit_executable = *jit_executable,
                           r = RCArray<AsyncValue>(results.values()),
                           o = RCArray<AsyncValue>(operands.values()), debug] {
    // Allocate storage for compiled executable results.
    llvm::SmallVector<RCReference<AsyncValue>> results_storage;
    results_storage.resize(r.size());

    // Reconstruct arguments and results from captured async values.
    RepeatedArguments<FallbackTensor> operands(o.values());
    RemainingResults results(results_storage);

    if (jit_executable.IsError()) {
      ReturnErrors(results, jit_executable.GetError(), exec_ctx);
    } else {
      ExecuteImpl(*jit_executable, operands, results, exec_ctx, debug);
    }

    // Forward previously entry indirect results to the actual results.
    for (unsigned i = 0; i < r.size(); ++i)
      llvm::cast<IndirectAsyncValue>(*r[i]).ForwardTo(
          std::move(results_storage[i]));
  });
}

// -------------------------------------------------------------------------- //
// TFRT kernel function definitions for tf_jitrt.fallback.execute operations.
// -------------------------------------------------------------------------- //

// Compiles kernel into the JitExecutable and executes it with the fallback
// tensors operands.
static void Execute(RepeatedArguments<FallbackTensor> operands,
                    RemainingResults results, StringAttribute device,
                    CompilationUnitAttribute kernel,
                    const ExecutionContext& exec_ctx) {
  ExecuteImpl(operands, results, device, kernel, exec_ctx);
}

// Compiles kernel into the JitExecutable and executes it with the fallback
// tensors operands in the debug mode: prints compilation diagnostics to the
// standard output. Should be used only in tests for verifying compiler
// internals.
void ExecuteDebug(RepeatedArguments<FallbackTensor> operands,
                  RemainingResults results,
                  Attribute<bool> debug_specializations, StringAttribute device,
                  CompilationUnitAttribute kernel, Attribute<bool> vectorize,
                  Attribute<bool> legalize_i1_tensors,
                  const ExecutionContext& exec_ctx) {
  TfJitRtPipelineOpts opts;
  opts.vectorize = *vectorize;
  opts.legalize_i1_tensors = *legalize_i1_tensors;
  ExecuteImpl(operands, results, device, kernel, exec_ctx,
              *debug_specializations, opts);
}

}  // namespace

void RegisterTfJitRuntimeKernels(KernelRegistry* registry) {
  registry->AddKernel("tf_jitrt.fallback.compile", TFRT_KERNEL(Compile));
  registry->AddKernel("tf_jitrt.fallback.execute", TFRT_KERNEL(Execute));
  registry->AddKernel("tf_jitrt.fallback.debug.execute",
                      TFRT_KERNEL(ExecuteDebug));

  registry->AddKernel("tf_jitrt.test.wait_for_compilation",
                      TFRT_KERNEL(WaitForCompilation));
  registry->AddKernel("tf_jitrt.test.reset_compilation_thread_pool",
                      TFRT_KERNEL(ResetCompilationThreadPool));
}

}  // namespace tensorflow
