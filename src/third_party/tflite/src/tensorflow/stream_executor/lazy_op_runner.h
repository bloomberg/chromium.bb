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
------------------------------------------------------------------------------*/

#ifndef TENSORFLOW_STREAM_EXECUTOR_LAZY_OP_RUNNER_H_
#define TENSORFLOW_STREAM_EXECUTOR_LAZY_OP_RUNNER_H_

#include "tensorflow/stream_executor/dnn.h"
#include "tensorflow/stream_executor/stream.h"
// #include "tensorflow/stream_executor/stream_executor_pimpl.h"

namespace stream_executor {
namespace dnn {

// A lazily-initialized OpRunner from an AlgorithmDesc.
//
// This exists to hold a choice of conv algorithm for a particular config,
// initialize its OpRunner at most once, and defer that initialization until the
// config is first needed.  This allows AoT autotuning to load configurations
// for all convolutions it knows about, without doing expensive initialization
// (e.g. runtime codegen) and retaining non-negligible resources (e.g.  compiled
// kernels) for potentially irrelevant configurations.  It also enables XLA conv
// thunks to defer binding to a particular stream executor until the first run.
//
// `Op` must satisfy the following "concept":
//
// struct Op {
//   // The function type signature parameter of an OpRunner.
//   using Signature = _;
//
//   // The parameter to be used by GetOrCreateRunner.
//   struct Config;
//
//   // Use a StreamExecutor to create an OpRunner.
//   static StatusOr<OpRunner<Config>> OpRunnerFromDesc(
//       const AlgorithmDesc& desc, Config config, StreamExecutor* stream);
// };
template <typename Op>
class LazyOpRunner {
 public:
  // Construct from a pre-initialized OpRunner; all calls to GetOrCreateRunner
  // will return a pointer to exactly this runner.
  static port::StatusOr<std::unique_ptr<LazyOpRunner>> FromOpRunner(
      std::unique_ptr<const OpRunner<typename Op::Signature>> runner) {
    if (!runner) {
      return port::InternalError("Null runner argument to FromOpRunner");
    }
    SE_ASSIGN_OR_RETURN(auto desc, runner->ToAlgorithmDesc());
    // Private constructor cannot be called by make_unique :(
    return {std::unique_ptr<LazyOpRunner>(
        new LazyOpRunner(desc, std::move(runner)))};
  }

  // Construct from an AlgorithmDesc, with no pre-initialized OpRunner; it will
  // be created on the first call to GetOrCreateRunner.
  explicit LazyOpRunner(AlgorithmDesc desc) : LazyOpRunner(desc, nullptr) {}

  // Returns an already-initialized OpRunner if available, or creates one.
  //
  // Invariant: a particular instance of this class shall only receive calls
  // with identical `config`s and `stream_executor`s.  If the config is changed,
  // only the first config it sees will have any effect, and second and
  // subsequent configs will be ignored.  If the stream executor is changed,
  // some operations on the returned `OpRunner` using the changed stream
  // executor will be errors.
  //
  // The result is owned by LazyOpRunner.
  port::StatusOr<const OpRunner<typename Op::Signature>*> GetOrCreateRunner(
      typename Op::Config config, Stream* stream) {
    absl::MutexLock lock(&mu_);
    if (!runner_) {
      SE_ASSIGN_OR_RETURN(runner_, Op::RunnerFromAlgorithmDesc(
                                       desc_, std::move(config), stream));
    }
    return runner_.get();
  }

  // Get the contained runner with the invariant that it's already initialized.
  port::StatusOr<const OpRunner<typename Op::Signature>*> GetRunner() {
    absl::MutexLock lock(&mu_);
    if (!runner_) {
      return port::InternalError("LazyOpRunner::GetRunner: not initialized");
    }
    return runner_.get();
  }

  bool operator==(const LazyOpRunner& other) const {
    return desc_ == other.desc_;
  }

  std::string ToString() const { return desc_.ToString(); }

  const AlgorithmDesc& ToAlgorithmDesc() const { return desc_; }

 private:
  LazyOpRunner(AlgorithmDesc desc,
               std::unique_ptr<const OpRunner<typename Op::Signature>> runner)
      : desc_(std::move(desc)), runner_(std::move(runner)) {}

  AlgorithmDesc desc_;
  absl::Mutex mu_;
  std::unique_ptr<const OpRunner<typename Op::Signature>> runner_
      ABSL_GUARDED_BY(mu_);
};

// Implementation of the concept required by LazyOpRunner, for ConvRunner.
struct ConvOp {
  using Signature = ConvSignature;

  struct Config {
    ConvolutionKind kind;
    DataType input_type, output_type;
    const BatchDescriptor& input_descriptor;
    const FilterDescriptor& filter_descriptor;
    const BatchDescriptor& output_descriptor;
    const ConvolutionDescriptor& convolution_descriptor;
  };

  static port::StatusOr<std::unique_ptr<const OpRunner<ConvSignature>>>
  RunnerFromAlgorithmDesc(const AlgorithmDesc& desc, Config config,
                          Stream* stream) {
    return stream->ConvolveRunnerFromDesc(
        desc, config.kind, config.input_type, config.output_type,
        config.input_descriptor, config.filter_descriptor,
        config.output_descriptor, config.convolution_descriptor);
  }
};

// Implementation of the concept required by LazyOpRunner, for LazyConvRunner.
struct FusedConvOp {
  using Signature = FusedConvSignature;

  struct Config {
    ConvolutionKind kind;
    DataType input_type, bias_type, output_type;
    double conv_scale, side_input_scale;
    const BatchDescriptor& input_descriptor;
    const FilterDescriptor& filter_descriptor;
    const BatchDescriptor& bias_descriptor;
    const BatchDescriptor& output_descriptor;
    const ConvolutionDescriptor& convolution_descriptor;
    ActivationMode activation_mode;
  };

  static port::StatusOr<std::unique_ptr<const OpRunner<FusedConvSignature>>>
  RunnerFromAlgorithmDesc(const AlgorithmDesc& desc, Config config,
                          Stream* stream) {
    return stream->FusedConvolveRunnerFromDesc(
        desc, config.kind, config.input_type, config.bias_type,
        config.output_type, config.conv_scale, config.side_input_scale,
        config.input_descriptor, config.filter_descriptor,
        config.bias_descriptor, config.output_descriptor,
        config.convolution_descriptor, config.activation_mode);
  }
};

}  // namespace dnn
}  // namespace stream_executor

#endif  // TENSORFLOW_STREAM_EXECUTOR_LAZY_OP_RUNNER_H_
