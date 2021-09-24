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

#include <string>
#include <utility>

#include "mlir/Dialect/Async/IR/AsyncTypes.h"
#include "mlir/ExecutionEngine/AsyncRuntime.h"
#include "mlir/Transforms/Bufferize.h"
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_cpurt.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_cpurt_pipeline.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_cpurt_request_context.h"
#include "tensorflow/compiler/mlir/tfrt/jit/transforms/tf_cpurt_passes.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/platform/dynamic_annotations.h"
#include "tensorflow/core/profiler/lib/traceme.h"
#include "tensorflow/core/runtime_fallback/kernel/kernel_fallback_compat_request_state.h"
#include "tensorflow/core/tfrt/utils/fallback_tensor.h"
#include "tfrt/cpu/jit/async_runtime.h"  // from @tf_runtime
#include "tfrt/cpu/jit/async_runtime_api.h"  // from @tf_runtime
#include "tfrt/cpu/jit/cpurt.h"  // from @tf_runtime
#include "tfrt/dtype/dtype.h"  // from @tf_runtime
#include "tfrt/host_context/async_dispatch.h"  // from @tf_runtime
#include "tfrt/host_context/async_value_ref.h"  // from @tf_runtime
#include "tfrt/host_context/chain.h"  // from @tf_runtime
#include "tfrt/host_context/execution_context.h"  // from @tf_runtime
#include "tfrt/host_context/host_buffer.h"  // from @tf_runtime
#include "tfrt/host_context/host_context.h"  // from @tf_runtime
#include "tfrt/host_context/kernel_registry.h"  // from @tf_runtime
#include "tfrt/host_context/kernel_utils.h"  // from @tf_runtime
#include "tfrt/support/error_util.h"  // from @tf_runtime
#include "tfrt/support/forward_decls.h"  // from @tf_runtime
#include "tfrt/support/rc_array.h"  // from @tf_runtime
#include "tfrt/support/string_util.h"  // from @tf_runtime
#include "tfrt/tensor/tensor_metadata.h"  // from @tf_runtime
#include "tfrt/tensor/tensor_shape.h"  // from @tf_runtime

namespace tensorflow {
namespace tfrt {
namespace jit {
namespace {

using ::llvm::Expected;

using ::tfrt::ArrayRef;
using ::tfrt::AsyncValue;
using ::tfrt::AsyncValuePtr;
using ::tfrt::AsyncValueRef;
using ::tfrt::Chain;
using ::tfrt::CompilationUnitAttribute;
using ::tfrt::DType;
using ::tfrt::EnqueueWork;
using ::tfrt::ExecutionContext;
using ::tfrt::IndirectAsyncValue;
using ::tfrt::KernelRegistry;
using ::tfrt::MakeConstructedAsyncValueRef;
using ::tfrt::MakeErrorAsyncValueRef;
using ::tfrt::MakeStringError;
using ::tfrt::RCArray;
using ::tfrt::RCReference;
using ::tfrt::RemainingResults;
using ::tfrt::RepeatedArguments;
using ::tfrt::RequestContext;
using ::tfrt::StrCat;
using ::tfrt::StringAttribute;
using ::tfrt::TaskFunction;

using ::tfrt::cpu::jit::CompilationOptions;
using ::tfrt::cpu::jit::EmitErrors;
using ::tfrt::cpu::jit::Executable;
using ::tfrt::cpu::jit::JitExecutable;
using ::tfrt::cpu::jit::JitExecutableCache;
using ::tfrt::cpu::jit::MemrefDesc;
using ::tfrt::cpu::jit::OperandConstraint;
using ::tfrt::cpu::jit::ReturnAsyncStridedMemref;
using ::tfrt::cpu::jit::ReturnStridedMemref;
using ::tfrt::cpu::jit::ReturnValueConverter;

using ::tensorflow::profiler::TraceMe;
using ::tensorflow::profiler::TraceMeEncode;
using ::tensorflow::tfd::KernelFallbackCompatRequestState;
using ::tensorflow::tfrt_stub::FallbackTensor;

// -------------------------------------------------------------------------- //
// JIT compiled kernels use Eigen ThreadPool managed by the kernel fallback as
// an async runtime worker threads.
// -------------------------------------------------------------------------- //

static Expected<Eigen::ThreadPoolInterface*> GetWorkerThreads(
    const ExecutionContext& exec_ctx) {
  RequestContext* req_ctx = exec_ctx.request_ctx();

  auto* fallback = req_ctx->GetDataIfExists<KernelFallbackCompatRequestState>();
  if (!fallback) return MakeStringError("fallback request state was not found");

  // Return user provided intra op thread pool if it is available.
  if (fallback->intra_op_threadpool()) return fallback->intra_op_threadpool();

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

static Expected<AsyncValuePtr<JitExecutable>> CompileImpl(
    CompilationUnitAttribute kernel, const ExecutionContext& exec_ctx) {
  // We only support functions nested in top level compiled module.
  if (kernel.nested_symbols().size() != 1)
    return MakeStringError(
        "kernel function has to be defined in a top-level module");

  // Request context must be initialized with the tf_cpurt state.
  TfCpuRtRequestState* state =
      exec_ctx.request_ctx()->GetDataIfExists<TfCpuRtRequestState>();
  if (!state)
    return MakeStringError("cpurt state not found in the request context");

  JitExecutableCache* jit_executable_cache = state->jit_executable_cache;

  // TODO(ezhulenev): CompilationUnitAttribute in addition to an `id` should
  // provide a hash (or something like sha-256 fingerprint) of its content for
  // cache lookup. Currently we rely on the fact that the SavedModel never
  // unloads a Bef file, and there is a 1-to-1 relationship between the
  // ResourceContext and the SavedModel, so the `id` is guaranteed to be a
  // unique key for the cache lookup.
  intptr_t key = kernel.id();

  // Maybe return JitExecutable from the cache.
  if (auto cached = jit_executable_cache->Find(key)) return cached;

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

  // Attributes required for tracing compilation.
  int64_t request_id = exec_ctx.request_ctx()->id();

  // Compilation (specialized executable compilation) events should be rare, so
  // we can afford to do detailed tracing for every compilation. If compilation
  // events happen too often, it is a much larger problem than the excessive
  // tracing.

  // Custom runner for compiling specializations that enqueues compilation task
  // into the host context work queue and adds tracing.
  auto runner = [kernel, request_id](size_t num_specializations,
                                     ArrayRef<OperandConstraint> constraints,
                                     ArrayRef<MemrefDesc> operands,
                                     TaskFunction compile,
                                     const ExecutionContext& exec_ctx) {
    assert(operands.size() == constraints.size());

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

    // TODO(ezhulenev): BEF file that owns the CompilationUnitAttribute in
    // theory can be unloaded before the completion of the compilation task.
    // It can't happen right now, because we require specialized compilation to
    // finish before returning the response, however for safety tracing
    // attributes that require the kernel attribute should be constructed in the
    // caller thread.

    // Run the actual compilation asynchronously without blocking the caller.
    EnqueueWork(exec_ctx, [request_id, kernel, num_specializations,
                           compile = std::move(compile),
                           args = std::move(args)]() mutable {
      absl::string_view name(kernel.root_symbol().data(),
                             kernel.root_symbol().size());
      TraceMe trace_me([&] {
        return TraceMeEncode("tf_cpurt.CompileSpecialization",
                             {{"id", request_id},
                              {"kernel_id", kernel.id()},
                              {"executable", name},
                              {"num_specializations", num_specializations}});
      });

      for (SpecializationArg& arg : args) {
        trace_me.AppendMetadata([&] {
          return TraceMeEncode({{arg.first, arg.second}});
        });
      }

      absl::string_view serialized_operation(
          kernel.serialized_operation().data(),
          kernel.serialized_operation().size());
      trace_me.AppendMetadata([&] {
        return TraceMeEncode({{"src", serialized_operation}});
      });

      compile();
    });
  };

  // Compile kernel asynchronously in the host context thread pool.
  EnqueueWork(exec_ctx, [kernel, request_id, runner, workers = *worker_threads,
                         ptr = entry.ptr]() {
    TraceMe trace_me([&] {
      absl::string_view name(kernel.root_symbol().data(),
                             kernel.root_symbol().size());
      absl::string_view serialized_operation(
          kernel.serialized_operation().data(),
          kernel.serialized_operation().size());
      return TraceMeEncode("tf_cpurt.CompileDefault",
                           {{"id", request_id},
                            {"kernel_id", kernel.id()},
                            {"executable", name},
                            {"src", serialized_operation}});
    });

    CompilationOptions opts;
    // All entry memrefs must have alignment compatible with Tensorflow.
    opts.alignment = EIGEN_MAX_ALIGN_BYTES;  // Eigen included by tensor.h
    opts.num_worker_threads = workers->NumThreads();
    opts.register_dialects = mlir::RegisterAllTensorFlowDialects;
    opts.register_pass_pipeline = CreateTfCpuRtPipeline;
    opts.type_converter = mlir::BufferizeTypeConverter();

    auto entrypoint = kernel.nested_symbols()[0];
    auto module = kernel.serialized_operation();

    // Instantiate new JitExecutable from the MLIR source.
    Expected<JitExecutable> jit_executable =
        JitExecutable::Instantiate(module, entrypoint, std::move(opts), runner);

    // Set the entry async value state to error or concrete.
    if (auto err = jit_executable.takeError())
      ptr.SetError(std::move(err));
    else
      ptr.emplace(std::move(*jit_executable));
  });

  return entry.ptr;
}

// Compiles kernel into the JitExecutable and updates JitExecutableCache.
static AsyncValueRef<Chain> Compile(StringAttribute device,
                                    CompilationUnitAttribute kernel,
                                    const ExecutionContext& exec_ctx) {
  // Trigger kernel compilation, that will update the JitExecutableCache.
  Expected<AsyncValuePtr<JitExecutable>> executable =
      CompileImpl(kernel, exec_ctx);

  // Return immediately if can't compile the kernel.
  if (auto err = executable.takeError())
    return MakeErrorAsyncValueRef(StrCat(err));

  // Signal compilation completion using an async chain.
  auto compiled = MakeConstructedAsyncValueRef<Chain>();

  executable->AndThen([executable = *executable, res = compiled.CopyRef()]() {
    if (executable.IsError())
      res.SetError(executable.GetError());
    else
      res.SetStateConcrete();
  });

  return compiled;
}

// -------------------------------------------------------------------------- //
// Execute compiled CPURT kernels with Fallback Runtime interop.
// -------------------------------------------------------------------------- //

using TensorflowReturnValueConverter =
    ReturnValueConverter<TensorflowConversionContext>;

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
  ssize_t multiplier = 1;
  for (int i = rank - 1; i >= 0; --i) {
    ssize_t dim_size = tensor.dim_size(i);
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

struct DebugListener : public JitExecutable::Listener {
  void notifyModuleSpecialized(
      ArrayRef<mlir::Type> operands,
      ArrayRef<mlir::DictionaryAttr> attrs) const override {
    std::string message;
    llvm::raw_string_ostream os(message);
    os << "Specialized operands:\n";
    for (auto tuple : llvm::enumerate(llvm::zip(operands, attrs))) {
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

static void ExecuteImpl(Executable& executable,
                        const llvm::SmallVectorImpl<MemrefDesc>& memrefs,
                        RepeatedArguments<FallbackTensor> operands,
                        RemainingResults results,
                        const ExecutionContext& exec_ctx) {
  // Bind execution trace to the request context.
  TraceMe trace_me([&] {
    int64_t id = exec_ctx.request_ctx()->id();
    absl::string_view name(executable.name().data(), executable.name().size());
    return TraceMeEncode("tf_cpurt.Execute",
                         {{"id", id}, {"executable", name}});
  });

  // Keep track of memory address to tensor mapping for result conversion.
  auto ctx = std::make_unique<TensorflowConversionContext>(operands.size());
  for (auto& t : operands)
    ctx->tensor_operands.insert({t.tensor().data(), &t.tensor()});

  // Tensorflow -> CPURT only supportes returning Memrefs as Tensors.
  TensorflowReturnValueConverter converter(results, std::move(ctx));
  converter.AddConversion(ReturnAsyncStridedMemref<ConvertTensor>);
  converter.AddConversion(ReturnStridedMemref<ConvertTensor>);

  // Get the worker threads from the execution context.
  Expected<Eigen::ThreadPoolInterface*> worker_threads =
      GetWorkerThreads(exec_ctx);
  if (auto err = worker_threads.takeError())
    return EmitErrors(results, std::move(err), exec_ctx);

  // Override async runtime worker threads with fallback Eigen thread pool.
  Executable::ExecuteOpts opts;
  opts.async_runtime_worker_threads = *worker_threads;

  // Error propagation happens in the result converter.
  if (auto err = executable.Execute(memrefs, converter, exec_ctx, opts)) return;

  // If executable is async keep operands and conversion context alive until
  // results become available.
  if (executable.IsAsync())
    RunWhenReady(results.values(),
                 [operands = RCArray<AsyncValue>(operands.values()),
                  ctx = converter.TakeConversionContext()] {});
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

  AsyncValuePtr<Executable> executable = jit_executable.GetExecutable(
      memrefs, exec_ctx, debug ? &debug_listener : nullptr);

  // If executable is available execute it inline.
  if (executable.IsAvailable()) {
    if (executable.IsError()) {
      EmitErrors(results, executable.GetError(), exec_ctx);
    } else {
      ExecuteImpl(executable.get(), memrefs, operands, results, exec_ctx);
    }
    return;
  }

  // Otherwise execute it when the executable will become available. This
  // requires careful lifetime extension of all async values passed as operands
  // to the kernel (and also results that will become available asynchronously).

  // Allocate indirect async values for all results, we'll forward them to the
  // actual async values computed by the executable later.
  for (unsigned i = 0; i < results.size(); ++i)
    results.AllocateIndirectResultAt(i);

  // Call executable when it's ready with the original operands.
  executable.AndThen([exec_ctx, executable, memrefs = std::move(memrefs),
                      r = RCArray<AsyncValue>(results.values()),
                      o = RCArray<AsyncValue>(operands.values())] {
    // Allocate storage for the executable results.
    llvm::SmallVector<RCReference<AsyncValue>> results_storage;
    results_storage.resize(r.size());

    // Reconstruct arguments and results from captured async values.
    RepeatedArguments<FallbackTensor> operands(o.values());
    RemainingResults results(exec_ctx.host(), results_storage);

    if (executable.IsError()) {
      EmitErrors(results, executable.GetError(), exec_ctx);
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
                        RemainingResults results, StringAttribute device,
                        CompilationUnitAttribute kernel,
                        const ExecutionContext& exec_ctx, bool debug) {
  // Compile kernel module into the JitExecutable.
  Expected<AsyncValuePtr<JitExecutable>> jit_executable =
      CompileImpl(kernel, exec_ctx);

  if (auto err = jit_executable.takeError())
    return EmitErrors(results, std::move(err), exec_ctx);

  // If kernel is available execute it inline.
  if (jit_executable->IsAvailable()) {
    if (jit_executable->IsError()) {
      EmitErrors(results, jit_executable->GetError(), exec_ctx);
    } else {
      ExecuteImpl(**jit_executable, operands, results, exec_ctx, debug);
    }
    return;
  }

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
    RemainingResults results(exec_ctx.host(), results_storage);

    if (jit_executable.IsError()) {
      EmitErrors(results, jit_executable.GetError(), exec_ctx);
    } else {
      ExecuteImpl(*jit_executable, operands, results, exec_ctx, debug);
    }

    // Forward previously entry indirect results to the actual results.
    for (unsigned i = 0; i < r.size(); ++i)
      llvm::cast<IndirectAsyncValue>(*r[i]).ForwardTo(
          std::move(results_storage[i]));
  });
}

// Compiles kernel into the JitExecutable and executes it with the fallback
// tensors operands.
static void Execute(RepeatedArguments<FallbackTensor> operands,
                    RemainingResults results, StringAttribute device,
                    CompilationUnitAttribute kernel,
                    const ExecutionContext& exec_ctx) {
  ExecuteImpl(operands, results, device, kernel, exec_ctx, /*debug=*/false);
}

// Compiles kernel into the JitExecutable and executes it with the fallback
// tensors operands in the debug mode: prints compilation diagnostics to the
// standard output. Should be used only in tests for verifying compiler
// internals.
static void ExecuteDebug(RepeatedArguments<FallbackTensor> operands,
                         RemainingResults results, StringAttribute device,
                         CompilationUnitAttribute kernel,
                         const ExecutionContext& exec_ctx) {
  ExecuteImpl(operands, results, device, kernel, exec_ctx, /*debug=*/true);
}

}  // namespace

void RegisterTfCpuRuntimeKernels(KernelRegistry* registry) {
  registry->AddKernel("tf_cpurt.fallback.compile", TFRT_KERNEL(Compile));
  registry->AddKernel("tf_cpurt.fallback.execute", TFRT_KERNEL(Execute));
  registry->AddKernel("tf_cpurt.fallback.debug.execute",
                      TFRT_KERNEL(ExecuteDebug));
}

}  // namespace jit
}  // namespace tfrt
}  // namespace tensorflow
