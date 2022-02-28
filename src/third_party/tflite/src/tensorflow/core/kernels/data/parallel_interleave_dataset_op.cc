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
#include "tensorflow/core/kernels/data/parallel_interleave_dataset_op.h"

#include <sys/types.h>

#include <atomic>
#include <deque>
#include <memory>
#include <utility>

#include "absl/strings/str_format.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/common_runtime/input_colocation_exemption_registry.h"
#include "tensorflow/core/data/captured_function.h"
#include "tensorflow/core/data/dataset_utils.h"
#include "tensorflow/core/data/name_utils.h"
#include "tensorflow/core/data/stats_utils.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/metrics.h"
#include "tensorflow/core/framework/model.h"
#include "tensorflow/core/framework/partial_tensor_shape.h"
#include "tensorflow/core/framework/stats_aggregator.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/lib/gtl/cleanup.h"
#include "tensorflow/core/lib/random/random.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/lib/strings/stringprintf.h"
#include "tensorflow/core/platform/blocking_counter.h"
#include "tensorflow/core/platform/cpu_info.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/stringprintf.h"
#include "tensorflow/core/profiler/lib/traceme.h"
#include "tensorflow/core/profiler/lib/traceme_encode.h"

namespace tensorflow {
namespace data {

// See documentation in ../../ops/dataset_ops.cc for a high-level
// description of the following op.

/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kDatasetType;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kInputDataset;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kOtherArguments;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kCycleLength;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kBlockLength;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kBufferOutputElements;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kPrefetchInputElements;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kNumParallelCalls;
/* static */ constexpr const char* const ParallelInterleaveDatasetOp::kFunc;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kTarguments;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kOutputTypes;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kOutputShapes;
/* static */ constexpr const char* const
    ParallelInterleaveDatasetOp::kDeterministic;
/* static */ constexpr const char* const ParallelInterleaveDatasetOp::kSloppy;

namespace {

constexpr char kParallelism[] = "parallelism";
constexpr char kBlockIndex[] = "block_index";
constexpr char kCycleIndex[] = "cycle_index";
constexpr char kEndOfInput[] = "end_of_input";
constexpr char kElementIdCounter[] = "element_id_counter";
constexpr char kCurrentElements[] = "current_elements";
constexpr char kCurrentElementsSize[] = "current_elements.size";
constexpr char kFutureElements[] = "future_elements";
constexpr char kFutureElementsSize[] = "future_elements.size";
constexpr char kResultsSuffix[] = ".results";
constexpr char kCodeSuffix[] = ".code";
constexpr char kErrorMessageSuffix[] = ".error_message";
constexpr char kIdSuffix[] = ".id";
constexpr char kSizeSuffix[] = ".size";
constexpr char kInputsSuffix[] = ".inputs";
constexpr char kIsReadySuffix[] = ".is_ready";

constexpr char kParallelInterleaveDatasetV2[] = "ParallelInterleaveDatasetV2";
constexpr char kParallelInterleaveDatasetV3[] = "ParallelInterleaveDatasetV3";
constexpr char kParallelInterleaveDatasetV4[] = "ParallelInterleaveDatasetV4";

// `kCyclePrefetchFactor * cycle_length` is the default number of future cycle
// elements that will be prefetched ahead of time. The purpose of prefetching
// future cycle elements is to overlap expensive initialization (e.g. opening of
// a remote file) with other computation.
constexpr double kDefaultCyclePrefetchFactor = 2.0L;

// `kPerIteratorPrefetchFactor * block_length + 1` is the default number of
// per-iterator results that will be prefetched ahead of time. The `+ 1` is to
// match the behavior of the original implementation.
constexpr double kDefaultPerIteratorPrefetchFactor = 2.0L;

// Period between reporting dataset statistics.
constexpr int kStatsReportingPeriodMillis = 1000;

inline int64_t CeilDiv(int64_t numerator, int64_t denominator) {
  return (numerator + denominator - 1) / denominator;
}

int64_t ComputeBufferOutputElements(int64_t configured_buffer_output_elements,
                                    int64_t block_length) {
  if (configured_buffer_output_elements != model::kAutotune) {
    return configured_buffer_output_elements;
  }
  return kDefaultPerIteratorPrefetchFactor * block_length + 1;
}

int64_t ComputePrefetchInputElements(int64_t configured_prefetch_input_elements,
                                     int64_t cycle_length) {
  if (configured_prefetch_input_elements != model::kAutotune) {
    return configured_prefetch_input_elements;
  }
  return kDefaultCyclePrefetchFactor * cycle_length;
}

int64_t OpVersionFromOpName(absl::string_view op_name) {
  if (op_name == kParallelInterleaveDatasetV2) {
    return 2;
  } else if (op_name == kParallelInterleaveDatasetV3) {
    return 3;
  } else {
    DCHECK_EQ(op_name, kParallelInterleaveDatasetV4);
    return 4;
  }
}

}  // namespace

// The motivation for creating an alternative implementation of parallel
// interleave is to decouple the degree of parallelism from the cycle length.
// This makes it possible to change the degree of parallelism (e.g. through
// auto-tuning) without changing the cycle length (which would change the order
// in which elements are produced).
class ParallelInterleaveDatasetOp::Dataset : public DatasetBase {
 public:
  Dataset(OpKernelContext* ctx, const DatasetBase* input,
          std::unique_ptr<CapturedFunction> captured_func, int64_t cycle_length,
          int64_t block_length, int64_t buffer_output_elements,
          int64_t prefetch_input_elements, int64_t num_parallel_calls,
          DeterminismPolicy deterministic, const DataTypeVector& output_types,
          const std::vector<PartialTensorShape>& output_shapes, int op_version)
      : DatasetBase(DatasetContext(ctx)),
        input_(input),
        captured_func_(std::move(captured_func)),
        cycle_length_(cycle_length),
        block_length_(block_length),
        buffer_output_elements_(
            ComputeBufferOutputElements(buffer_output_elements, block_length)),
        prefetch_input_elements_(ComputePrefetchInputElements(
            prefetch_input_elements, cycle_length)),
        num_parallel_calls_(num_parallel_calls),
        deterministic_(deterministic),
        output_types_(output_types),
        output_shapes_(output_shapes),
        op_version_(op_version),
        traceme_metadata_(
            {{"autotune",
              num_parallel_calls == model::kAutotune ? "true" : "false"},
             {"block_length",
              strings::Printf("%lld", static_cast<long long>(block_length))},
             {"cycle_length",
              strings::Printf("%lld", static_cast<long long>(cycle_length))},
             {"deterministic",
              deterministic.IsNondeterministic() ? "false" : "true"}}) {
    input_->Ref();
  }

  ~Dataset() override { input_->Unref(); }

  std::unique_ptr<IteratorBase> MakeIteratorInternal(
      const string& prefix) const override {
    name_utils::IteratorPrefixParams params;
    params.op_version = op_version_;
    bool deterministic =
        deterministic_.IsDeterministic() || deterministic_.IsDefault();
    return absl::make_unique<ParallelInterleaveIterator>(
        ParallelInterleaveIterator::Params{
            this,
            name_utils::IteratorPrefix(
                ParallelInterleaveDatasetOp::kDatasetType, prefix, params)},
        deterministic);
  }

  const DataTypeVector& output_dtypes() const override { return output_types_; }

  const std::vector<PartialTensorShape>& output_shapes() const override {
    return output_shapes_;
  }

  string DebugString() const override {
    name_utils::DatasetDebugStringParams params;
    params.op_version = op_version_;
    return name_utils::DatasetDebugString(
        ParallelInterleaveDatasetOp::kDatasetType, params);
  }

  int64_t CardinalityInternal() const override {
    int64_t n = input_->Cardinality();
    if (n == kInfiniteCardinality) {
      return n;
    }
    return kUnknownCardinality;
  }

  Status InputDatasets(std::vector<const DatasetBase*>* inputs) const override {
    inputs->push_back(input_);
    return Status::OK();
  }

  Status CheckExternalState() const override {
    TF_RETURN_IF_ERROR(captured_func_->CheckExternalState());
    return input_->CheckExternalState();
  }

 protected:
  Status AsGraphDefInternal(SerializationContext* ctx,
                            DatasetGraphDefBuilder* b,
                            Node** output) const override {
    std::vector<std::pair<size_t, Node*>> inputs;
    std::vector<std::pair<size_t, gtl::ArraySlice<Node*>>> list_inputs;
    int input_index = 0;

    Node* input_node;
    TF_RETURN_IF_ERROR(b->AddInputDataset(ctx, input_, &input_node));
    inputs.emplace_back(input_index++, input_node);

    std::vector<Node*> other_arguments;
    DataTypeVector other_arguments_types;
    TF_RETURN_IF_ERROR(captured_func_->AddToGraph(ctx, b, &other_arguments,
                                                  &other_arguments_types));
    list_inputs.emplace_back(input_index++, other_arguments);

    Node* cycle_length_node;
    TF_RETURN_IF_ERROR(b->AddScalar(cycle_length_, &cycle_length_node));
    inputs.emplace_back(input_index++, cycle_length_node);

    Node* block_length_node;
    TF_RETURN_IF_ERROR(b->AddScalar(block_length_, &block_length_node));
    inputs.emplace_back(input_index++, block_length_node);

    if (op_version_ >= 4) {
      Node* buffer_output_elements_node;
      TF_RETURN_IF_ERROR(
          b->AddScalar(buffer_output_elements_, &buffer_output_elements_node));
      inputs.emplace_back(input_index++, buffer_output_elements_node);

      Node* prefetch_input_elements_node;
      TF_RETURN_IF_ERROR(b->AddScalar(prefetch_input_elements_,
                                      &prefetch_input_elements_node));
      inputs.emplace_back(input_index++, prefetch_input_elements_node);
    }

    Node* num_parallel_calls_node;
    TF_RETURN_IF_ERROR(
        b->AddScalar(num_parallel_calls_, &num_parallel_calls_node));
    inputs.emplace_back(input_index++, num_parallel_calls_node);

    std::vector<std::pair<StringPiece, AttrValue>> attrs;
    AttrValue f;
    b->BuildAttrValue(captured_func_->func(), &f);
    attrs.emplace_back(kFunc, f);

    AttrValue other_arguments_types_attr;
    b->BuildAttrValue(other_arguments_types, &other_arguments_types_attr);
    attrs.emplace_back(kTarguments, other_arguments_types_attr);

    if (op_version_ == 2) {
      AttrValue sloppy_attr;
      b->BuildAttrValue(deterministic_.IsNondeterministic(), &sloppy_attr);
      attrs.emplace_back(kSloppy, sloppy_attr);
    }
    if (op_version_ >= 3) {
      AttrValue deterministic_attr;
      b->BuildAttrValue(deterministic_.String(), &deterministic_attr);
      attrs.emplace_back(kDeterministic, deterministic_attr);
    }

    TF_RETURN_IF_ERROR(b->AddDataset(this, inputs, list_inputs, attrs, output));
    return Status::OK();
  }

 private:
  class ParallelInterleaveIterator : public DatasetIterator<Dataset> {
   public:
    ParallelInterleaveIterator(const Params& params, bool deterministic)
        : DatasetIterator<Dataset>(params),
          mu_(std::make_shared<mutex>()),
          num_parallel_calls_cond_var_(std::make_shared<condition_variable>()),
          num_parallel_calls_(std::make_shared<model::SharedState>(
              params.dataset->num_parallel_calls_, mu_,
              num_parallel_calls_cond_var_)),
          deterministic_(deterministic),
          current_elements_(params.dataset->cycle_length_) {}

    ~ParallelInterleaveIterator() override { CancelThreads(/*wait=*/true); }

    // TODO(jsimsa): Register cancellation callback once the implementation is
    // refactored not to hold mu_ while calling `GetNext` on the input.
    Status Initialize(IteratorContext* ctx) override {
      mutex_lock l(*mu_);
      interleave_depth_ = ctx->interleave_depth();

      // Note that if `ctx->thread_pool()` is non-null, then instead of creating
      // a dedicated thread pool of size `num_threads`, computation will be
      // scheduled into the shared threadpool. The threadpool is guaranteed to
      // support `num_threads` concurrent tasks without blocking indefinitely.
      //
      // Allocate one thread for the worker manager, one thread for stats
      // collection, `cycle_length_` threads for the current workers, and
      // `future_elements_prefetch_` for the future workers.
      int max_current_workers = dataset()->cycle_length_;
      int future_workers =
          dataset()->prefetch_input_elements_ + dataset()->cycle_length_;
      int num_threads = 1 + max_current_workers + future_workers;
      if (ctx->stats_aggregator()) {
        num_threads++;
      }
      thread_pool_ = ctx->CreateThreadPool(
          "data_parallel_interleave_worker_pool", num_threads);
      if (num_parallel_calls_->value == model::kAutotune) {
        num_parallel_calls_->value = dataset()->cycle_length_;
      }
      ctx_ = std::make_unique<IteratorContext>(*ctx);
      cancellation_manager_ = absl::make_unique<CancellationManager>();
      IteratorContext::Params params(ctx);
      params.interleave_depth += 1;
      params.cancellation_manager = cancellation_manager_.get();
      TF_RETURN_IF_ERROR(dataset()->input_->MakeIterator(
          IteratorContext(params), this, prefix(), &input_impl_));
      return dataset()->captured_func_->Instantiate(
          ctx, &instantiated_captured_func_);
    }

    Status GetNextInternal(IteratorContext* ctx,
                           std::vector<Tensor>* out_tensors,
                           bool* end_of_sequence) override {
      std::shared_ptr<Result> result;
      {
        mutex_lock l(*mu_);
        EnsureInitialElementsCreated();
        EnsureThreadsStarted();
        while (!cancelled_ && !Consume(&result)) {
          RecordStop(ctx);
          if (deterministic_) {
            VLOG(3) << "Blocked waiting for element "
                    << current_elements_[cycle_index_]->id;
            current_elements_[cycle_index_]->cond_var.wait(l);
          } else {
            any_element_available_cond_var_.wait(l);
          }
          RecordStart(ctx);
        }
        if (cancelled_) {
          return errors::Cancelled("Iterator was cancelled");
        }
      }
      if (!result) {
        *end_of_sequence = true;
        return Status::OK();
      }
      profiler::TraceMe traceme([&] {
        return profiler::TraceMeEncode("ParallelInterleaveConsume",
                                       {{"element_id", result->id}});
      });
      if (result->status.ok()) {
        *out_tensors = std::move(result->return_values);
        RecordBufferDequeue(ctx, *out_tensors);
      }
      *end_of_sequence = false;
      return result->status;
    }

   protected:
    std::shared_ptr<model::Node> CreateNode(
        IteratorContext* ctx, model::Node::Args args) const override {
      bool increase_min =
          GetExperiments().contains("min_outer_interleave_parallelism") &&
          (ctx->interleave_depth() == 0);
      // When the `min_outer_interleave_parallelism` is enabled, we increase
      // the minimum parallelism based on empirical evidence (see
      // go/tf-data-max-autotuning for details).
      double min =
          increase_min
              ? std::min(
                    static_cast<double>(dataset()->cycle_length_),
                    std::ceil(std::pow(27 * dataset()->cycle_length_, 0.5)))
              : 1;
      return model::MakeAsyncInterleaveManyNode(
          std::move(args),
          {model::MakeParameter(kParallelism, num_parallel_calls_, /*min=*/min,
                                /*max=*/dataset()->cycle_length_)});
    }

    // TODO(aaudibert): Refactor the implementations to avoid the need for
    // `IteratorContext` when saving the state of the iterator.
    Status SaveInternal(SerializationContext* ctx,
                        IteratorStateWriter* writer) override {
      TF_RETURN_IF_ERROR(ctx->HandleCheckExternalStateStatus(
          dataset()->captured_func_->CheckExternalState()));
      mutex_lock l(*mu_);
      wait_for_checkpoint_ = true;
      // Wait for all in-flight calls to complete.
      while (num_active_workers_ > 0) {
        zero_active_workers_cond_var_.wait(l);
      }
      // Initialize all elements and filter out elements with no input.
      InitializeInputs(element_id_counter_);
      for (auto& element : current_elements_) {
        if (element && element->no_input) {
          element.reset();
        }
      }
      while (!future_elements_.empty() && future_elements_.back()->no_input) {
        future_elements_.pop_back();
      }
      wait_for_checkpoint_ = false;
      DCHECK_EQ(num_active_workers_, 0);
      VLOG(4) << "State before save:\n" << DebugString();
      TF_RETURN_IF_ERROR(SaveInput(ctx, writer, input_impl_));
      TF_RETURN_IF_ERROR(
          writer->WriteScalar(prefix(), kBlockIndex, block_index_));
      TF_RETURN_IF_ERROR(
          writer->WriteScalar(prefix(), kCycleIndex, cycle_index_));
      if (end_of_input_) {
        TF_RETURN_IF_ERROR(writer->WriteScalar(prefix(), kEndOfInput, ""));
      }
      TF_RETURN_IF_ERROR(writer->WriteScalar(prefix(), kElementIdCounter,
                                             element_id_counter_));
      TF_RETURN_IF_ERROR(WriteCurrentElements(ctx, writer));
      TF_RETURN_IF_ERROR(WriteFutureElements(ctx, writer));
      // Wake workers back up.
      current_workers_cond_var_.notify_all();
      future_workers_cond_var_.notify_all();
      return Status::OK();
    }

    Status RestoreInternal(IteratorContext* ctx,
                           IteratorStateReader* reader) override {
      {
        mutex_lock l(*mu_);
        DCHECK(!threads_initialized_);
        DCHECK(!initial_elements_created_);
        TF_RETURN_IF_ERROR(RestoreInput(ctx, reader, input_impl_));
        TF_RETURN_IF_ERROR(
            reader->ReadScalar(prefix(), kBlockIndex, &block_index_));
        TF_RETURN_IF_ERROR(
            reader->ReadScalar(prefix(), kCycleIndex, &cycle_index_));
        TF_RETURN_IF_ERROR(reader->ReadScalar(prefix(), kElementIdCounter,
                                              &element_id_counter_));
        end_of_input_ = reader->Contains(prefix(), kEndOfInput);
      }
      TF_RETURN_IF_ERROR(ReadCurrentElements(ctx, reader));
      TF_RETURN_IF_ERROR(ReadFutureElements(ctx, reader));
      mutex_lock l(*mu_);
      initial_elements_created_ = false;
      for (int i = 0; i < current_elements_.size(); ++i) {
        int index = (cycle_index_ + i) % current_elements_.size();
        auto element = current_elements_[index];
        if (element) {
          elements_to_process_.push_back(index);
          element->initialized = true;
          element->cycle_index = index;
          initial_elements_created_ = true;
        }
      }
      for (const auto& element : future_elements_) {
        element->initialized = true;
      }
      last_valid_current_element_ = current_elements_.size() - 1;
      while (last_valid_current_element_ >= 0 &&
             !current_elements_[last_valid_current_element_]) {
        last_valid_current_element_--;
      }
      VLOG(2) << "Parallel interleave iterator restored";
      VLOG(4) << "State after restore:\n" << DebugString();
      return Status::OK();
    }

    TraceMeMetadata GetTraceMeMetadata() const override {
      int64_t parallelism = -1;
      int64_t results_ready = -1;
      int64_t active_elements = -1;
      // NOTE: We only set the parallelism value if the lock can be acquired
      // right away to avoid introducing tracing overhead.
      if (mu_->try_lock()) {
        parallelism = num_parallel_calls_->value;
        results_ready = 0;
        active_elements = 0;
        for (int i = 0; i < current_elements_.size(); ++i) {
          if (current_elements_[i]) {
            results_ready += current_elements_[i]->results.size();
            if (current_elements_[i]->active) {
              active_elements++;
            }
          }
        }
        mu_->unlock();
      }
      auto result = dataset()->traceme_metadata_;
      result.push_back(std::make_pair(
          "parallelism",
          parallelism == -1
              ? kTraceInfoUnavailable
              : strings::Printf("%lld", static_cast<long long>(parallelism))));
      result.push_back(std::make_pair(
          "results_ready", results_ready == -1
                               ? kTraceInfoUnavailable
                               : strings::Printf("%lld", static_cast<long long>(
                                                             results_ready))));
      result.push_back(std::make_pair(
          "active_elements",
          results_ready == -1 ? kTraceInfoUnavailable
                              : strings::Printf("%lld", static_cast<long long>(
                                                            active_elements))));
      result.push_back(std::make_pair(
          "interleave_depth",
          strings::Printf("%lld", static_cast<long long>(interleave_depth_))));
      return result;
    }

   private:
    // Represents the result of fetching an element from a dataset.
    struct Result {
      Status status;
      int64_t id = -1;
      std::vector<Tensor> return_values;
    };

    // The interleave transformation repeatedly inputs elements, applies the
    // user-provided function to transform the input elements to datasets, and
    // interleaves the elements of these datasets as its output.
    //
    // This structure represents an input element and derived state.
    struct Element {
      // Unique identifier, needed to support checkpointing.
      int64_t id TF_GUARDED_BY(&ParallelInterleaveIterator::mu_);
      // The actual input element.  Iterator created from the input element. A
      // null value indicates that the element either reached end of input or
      // hasn't been initialized yet.
      std::unique_ptr<std::vector<Tensor>> inputs
          TF_GUARDED_BY(&ParallelInterleaveIterator::mu_);
      // Iterator created from the input element. A null value indicates that
      // the element either reached end of input or hasn't been initialized yet.
      std::unique_ptr<IteratorBase> iterator
          TF_GUARDED_BY(&ParallelInterleaveIterator::mu_);
      // Buffer for storing the outputs of `iterator`.
      std::deque<std::shared_ptr<Result>> TF_GUARDED_BY(
          &ParallelInterleaveIterator::mu_) results;
      // The element's index in the cycle, if it is in the current cycle.
      // -1 if the element is not in the current cycle.
      int64_t cycle_index TF_GUARDED_BY(&ParallelInterleaveIterator::mu_) = -1;
      // Whether the element is currently being processed by a worker thread.
      // This is used to ensure that only one thread at a time tries to process
      // an element.
      bool active TF_GUARDED_BY(&ParallelInterleaveIterator::mu_) = false;
      // Whether the inputs and iterator have been initialized.
      bool initialized TF_GUARDED_BY(&ParallelInterleaveIterator::mu_) = false;
      // Whether we tried to initialize the element, but the input iterator
      // was exhausted so we could produce no inputs.
      bool no_input TF_GUARDED_BY(&ParallelInterleaveIterator::mu_) = false;
      // Condition variable for communicating between current worker threads
      // and GetNext.
      condition_variable cond_var;

      std::string DebugString()
          TF_EXCLUSIVE_LOCKS_REQUIRED(&ParallelInterleaveIterator::mu_) {
        return absl::StrFormat(
            "Element(id: %d, iterator_null: %d, results_size: %d, "
            "cycle_index: %d, active: %d, initialized: %d, no_input: %d)",
            id, iterator == nullptr, results.size(), cycle_index, active,
            initialized, no_input);
      }
    };

    // Sets the cancellation bit and wakes up all threads that need to be
    // cancelled. Optionally, the method waits until all threads finish
    // executing.
    void CancelThreads(bool wait) TF_LOCKS_EXCLUDED(mu_) {
      cancellation_manager_->StartCancel();
      mutex_lock l(*mu_);
      cancelled_ = true;
      // Wake up all threads so that they can exit. This will also wake up any
      // threads waiting in GetNextInternal.
      for (const auto& element : current_elements_) {
        if (element) {
          element->cond_var.notify_all();
        }
      }
      current_workers_cond_var_.notify_all();
      future_workers_cond_var_.notify_all();
      num_parallel_calls_cond_var_->notify_all();
      stats_thread_cond_var_.notify_all();
      while (wait && outstanding_threads_ > 0) {
        outstanding_threads_finished_cond_var_.wait(l);
      }
      any_element_available_cond_var_.notify_all();
      zero_active_workers_cond_var_.notify_all();
    }

    void EnsureInitialElementsCreated() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (!initial_elements_created_) {
        for (int i = 0; i < dataset()->cycle_length_; ++i) {
          current_elements_[i] = MakeElement();
          if (!current_elements_[i]) {
            break;
          }
          current_elements_[i]->cycle_index = i;
          elements_to_process_.push_back(i);
          last_valid_current_element_ = i;
        }
        initial_elements_created_ = true;
      }
    }

    void EnsureThreadsStarted() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (!threads_initialized_) {
        IncrementOutstandingThreads();
        thread_pool_->Schedule([this]() { WorkerManagerThread(); });
        if (ctx_->stats_aggregator()) {
          IncrementOutstandingThreads();
          thread_pool_->Schedule([this]() { StatsThread(); });
        }
        threads_initialized_ = true;
      }
    }

    // Advances the position in the interleave cycle to the next cycle
    // element.
    void AdvanceToNextInCycle() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      DCHECK_NE(last_valid_current_element_, -1);
      block_index_ = 0;
      cycle_index_ = (cycle_index_ + 1) % (last_valid_current_element_ + 1);
    }

    // Advances the position in the interleave cycle by one.
    void AdvancePosition() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      ++block_index_;
      if (block_index_ == dataset()->block_length_) {
        AdvanceToNextInCycle();
      }
    }

    // Consumes a result (if available), returning an indication of whether
    // a result is available. If `true` is returned, `result` either
    // points to a valid result or is null if end of input has been reached.
    bool Consume(std::shared_ptr<Result>* result)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (deterministic_) {
        return ConsumeHelper(result);
      }
      // If we are allowed to be nondeterministic (i.e. return results out of
      // order), try to find an element in the cycle that has a result
      // available.
      for (int i = 0; i < dataset()->cycle_length_; ++i) {
        if (ConsumeHelper(result)) {
          return true;
        }
        AdvanceToNextInCycle();
      }
      return false;
    }

    // Consumes a result (if available), returning an indication of whether
    // a result is available. If `true` is returned, `result` either
    // points to a valid result or is null if end of input has been reached.
    bool ConsumeHelper(std::shared_ptr<Result>* result)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      while (true) {
        if (last_valid_current_element_ == -1) {
          // Reached end of input.
          return true;
        }
        for (int64_t i = 0; i < (last_valid_current_element_ + 1); ++i) {
          int64_t index =
              (cycle_index_ + i) % (last_valid_current_element_ + 1);
          if (current_elements_[index]) {
            cycle_index_ = index;
            if (i > 0) {
              block_index_ = 0;
            }
            break;
          }
        }
        DCHECK(current_elements_[cycle_index_]);
        std::shared_ptr<Element> element = current_elements_[cycle_index_];
        if (!element->results.empty()) {
          // We found a result.
          std::swap(*result, element->results.front());
          element->results.pop_front();
          if (!element->active) {
            elements_to_process_.push_back(cycle_index_);
            current_workers_cond_var_.notify_one();
          }
          AdvancePosition();
          return true;
        }
        if (!element->initialized || element->iterator) {
          // The element is still producing results, so we wait.
          return false;
        }
        // We've consumed all results from the element. Get a new element from
        // future_elements, or create a new element if no future elements are
        // available.
        if (!future_elements_.empty()) {
          std::shared_ptr<Element> future_element =
              std::move(future_elements_.front());
          future_elements_.pop_front();
          if (future_element->iterator) {
            EnableAutotune(ctx_.get(), future_element->iterator.get());
          }
          future_element->cycle_index = cycle_index_;
          current_elements_[cycle_index_] = std::move(future_element);
          future_workers_cond_var_.notify_one();
          if (!current_elements_[cycle_index_]->active) {
            current_workers_cond_var_.notify_one();
          }
        } else {
          current_elements_[cycle_index_] = MakeElement();
          if (current_elements_[cycle_index_]) {
            current_elements_[cycle_index_]->cycle_index = cycle_index_;
            elements_to_process_.push_back(cycle_index_);
            element->cycle_index = cycle_index_;
            current_workers_cond_var_.notify_one();
          }
          while (last_valid_current_element_ >= 0 &&
                 !current_elements_[last_valid_current_element_]) {
            last_valid_current_element_--;
            if (cycle_index_ > last_valid_current_element_) {
              // We are about to move the cycle index below in
              // AdvanceToNextInCycle().
              cycle_index_ = last_valid_current_element_;
            }
          }
        }
        if (last_valid_current_element_ != -1) {
          AdvanceToNextInCycle();
        }
      }
    }

    // Creates a new element.
    std::shared_ptr<Element> MakeElement() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (end_of_input_) {
        return nullptr;
      }
      auto element = std::make_shared<Element>();
      element->id = element_id_counter_++;
      uninitialized_elements_.push_back(element);
      return element;
    }

    // Thread responsible for launching all worker threads. The thread stays
    // around after startup in case autotuning increases num_parallel_calls.
    void WorkerManagerThread() TF_LOCKS_EXCLUDED(mu_) {
      RecordStart(ctx_.get());
      auto cleanup = gtl::MakeCleanup([this]() {
        RecordStop(ctx_.get());
        mutex_lock l(*mu_);
        DecrementOutstandingThreads();
      });
      int initial_current_workers;
      // When elements are moved from `future_elements_` to `current_elements_`,
      // the future worker which created the element may continue to process
      // the element for some time. That is why we need an additional
      // `cycle_length_` future workers to guarantee that whenever
      // `future_element_.size() < future_elements_prefetch_`, there will be a
      // future worker available to create a new future element.
      int future_workers =
          dataset()->prefetch_input_elements_ + dataset()->cycle_length_;
      {
        mutex_lock l(*mu_);
        initial_current_workers = num_parallel_calls_->value;
        outstanding_threads_ += initial_current_workers + future_workers;
        num_current_workers_ += initial_current_workers;
        num_active_workers_ += initial_current_workers + future_workers;
        num_current_active_workers_ += initial_current_workers;
      }
      // Start current workers before future workers to improve startup time.
      for (int i = 0; i < initial_current_workers; ++i) {
        StartCurrentWorkerThread();
      }
      for (int i = 0; i < future_workers; ++i) {
        StartFutureWorkerThread();
      }
      while (true) {
        {
          mutex_lock l(*mu_);
          while (!cancelled_ &&
                 num_current_workers_ >= num_parallel_calls_->value) {
            RecordStop(ctx_.get());
            num_parallel_calls_cond_var_->wait(l);
            RecordStart(ctx_.get());
          }
          if (cancelled_ || end_of_input_) {
            return;
          }
          IncrementOutstandingThreads();
          IncrementCurrentWorkers();
          IncrementActiveWorkers();
          IncrementCurrentActiveWorkers();
          StartCurrentWorkerThread();
        }
      }
    }

    void StartCurrentWorkerThread() {
      thread_pool_->Schedule([this]() { CurrentWorkerThread(); });
    }

    void StartFutureWorkerThread() {
      thread_pool_->Schedule([this]() { FutureWorkerThread(); });
    }

    // Current workers are responsible for keeping elements in
    // `current_elements_` processed. An element is processed if it is either
    // done or its `results` buffer is full (contains `kPerIteratorPrefetch`
    // elements).
    //
    // Current workers cycle between two phases: (1) finding an element and (2)
    // processing it. When a worker is processing an element, it will
    // claim the element by setting `element->active`, then continue to produce
    // results for the element until enough results have been computed for the
    // current cycle and the results buffer is full.
    void CurrentWorkerThread() TF_LOCKS_EXCLUDED(mu_) {
      RecordStart(ctx_.get());
      auto done = [this]() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
        RecordStop(ctx_.get());
        DecrementActiveWorkers();
        DecrementCurrentActiveWorkers();
        DecrementOutstandingThreads();
        DecrementCurrentWorkers();
      };
      while (true) {
        int element_index;
        std::shared_ptr<Element> element;
        // Find an element to process.
        {
          mutex_lock l(*mu_);
          // In case autotune changes num_parallel_calls.
          if (num_current_workers_ > num_parallel_calls_->value) {
            done();
            return;
          }
          // Look for an element that needs processing.
          element.reset();
          while (!cancelled_) {
            while (!elements_to_process_.empty() && !wait_for_checkpoint_) {
              int index = elements_to_process_.front();
              elements_to_process_.pop_front();
              auto& e = current_elements_[index];
              if (NeedsProcessing(e) && !e->active) {
                element_index = index;
                element = e;
                break;
              }
            }
            if (element) {
              break;
            }
            DecrementCurrentActiveWorkers();
            WaitWorkerThread(&current_workers_cond_var_, &l);
            IncrementCurrentActiveWorkers();
          }
          if (cancelled_) {
            done();
            return;
          }
          VLOG(3) << "Current worker woke up to process " << element->id;
          element->active = true;
        }
        // Loop on the element until we fill its results buffer or reach end of
        // input for the element.
        while (true) {
          ProcessElement(element);
          {
            mutex_lock l(*mu_);
            // Check whether we have produced enough results for the current
            // cycle.
            if (!NeedsProcessing(element)) {
              element->active = false;
              break;
            }
          }
        }
      }
    }

    // Future workers process elements after the current interleave cycle. A
    // future worker's job is to keep `future_elements_` filled with elements.
    // Elements in `future_elements` have had their first `kPerIteratorPrefetch`
    // results computed.
    void FutureWorkerThread() TF_LOCKS_EXCLUDED(mu_) {
      RecordStart(ctx_.get());
      auto done = [this]() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
        RecordStop(ctx_.get());
        DecrementActiveWorkers();
        DecrementOutstandingThreads();
      };
      std::shared_ptr<Element> element;
      while (true) {
        {
          mutex_lock l(*mu_);
          if (element) {
            element->active = false;
            if (element->cycle_index != -1) {
              element->cond_var.notify_one();
              // A current worker may need to process the element further.
              elements_to_process_.push_back(element->cycle_index);
              current_workers_cond_var_.notify_one();
            }
          }
          while (!cancelled_ && (future_elements_.size() >=
                                     dataset()->prefetch_input_elements_ ||
                                 wait_for_checkpoint_)) {
            WaitWorkerThread(&future_workers_cond_var_, &l);
          }
          if (cancelled_) {
            done();
            return;
          }
          element = MakeElement();
          if (!element) {
            done();
            return;
          }
          VLOG(3) << "Future worker created element " << element->id;
          element->active = true;
          future_elements_.push_back(element);
        }
        ProcessElement(element);
      }
    }

    // Generates results for the given element until the element's results
    // buffer is full or the element is done producing results.
    void ProcessElement(std::shared_ptr<Element> element)
        TF_LOCKS_EXCLUDED(mu_) {
      DCHECK(element != nullptr);
      IteratorBase* iterator;
      int64_t input_element_id;
      // Initialize the inputs and iterator if necessary.
      {
        mutex_lock l(*mu_);
        DCHECK(element->active);
        input_element_id = element->id;
        if (!element->iterator) {
          InitializeInputs(input_element_id);
          if (!element->iterator) {
            return;
          }
        }
        // `iterator` will remain valid after releasing the lock because we have
        // marked the element as active, so no other thread will modify its
        // iterator.
        iterator = element->iterator.get();
      }
      DCHECK(iterator != nullptr);
      // Process until the results queue is full or we reach end of input.
      while (true) {
        auto result = std::make_shared<Result>();
        profiler::TraceMe traceme([&] {
          result->id = profiler::TraceMe::NewActivityId();
          return profiler::TraceMeEncode(
              "ParallelInterleaveProduce",
              {{"input_element_id", input_element_id},
               {"element_id", result->id}});
        });
        bool end_of_input = false;
        result->status =
            iterator->GetNext(MakeNestedIteratorContext(ctx_.get()),
                              &result->return_values, &end_of_input);
        if (end_of_input) {
          mutex_lock l(*mu_);
          element->iterator.reset();
          element->inputs.reset();
          NotifyElementUpdate(element);
          break;
        }
        RecordBufferEnqueue(ctx_.get(), result->return_values);
        mutex_lock l(*mu_);
        element->results.push_back(std::move(result));
        NotifyElementUpdate(element);
        if (element->results.size() == dataset()->buffer_output_elements_) {
          break;
        }
      }
    }

    // Initialize inputs and create an iterator for all elements up to
    // element_id.
    void InitializeInputs(int element_id) TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      while (!uninitialized_elements_.empty() &&
             uninitialized_elements_.front()->id <= element_id) {
        std::shared_ptr<Element> element = uninitialized_elements_.front();
        uninitialized_elements_.pop_front();
        element->initialized = true;
        // Check if we've already reached end of input.
        if (end_of_input_) {
          element->no_input = true;
          NotifyElementUpdate(element);
          continue;
        }
        profiler::TraceMe traceme([input_element_id = element->id] {
          return profiler::TraceMeEncode(
              "ParallelInterleaveInitializeInput",
              {{"input_element_id", input_element_id}});
        });
        std::vector<Tensor> inputs;
        Status status;
        {
          // TODO(aaudibert): Refactor the implementation to move calls of
          // `GetNext` out of the scope of `mu_`.
          status = input_impl_->GetNext(ctx_.get(), &inputs, &end_of_input_);
        }
        if (!status.ok()) {
          AddErrorResult(element, status);
          continue;
        }
        if (end_of_input_) {
          element->no_input = true;
          NotifyElementUpdate(element);
          continue;
        }
        element->inputs =
            absl::make_unique<std::vector<Tensor>>(std::move(inputs));
        IteratorContext::Params params(ctx_.get());
        params.interleave_depth += 1;
        IteratorContext ctx(params);
        status = MakeIteratorFromInputElement(
            &ctx, this, *element->inputs, element->id,
            *instantiated_captured_func_, prefix(), &element->iterator,
            model_node());
        if (!status.ok()) {
          element->inputs.reset();
          element->iterator.reset();
          AddErrorResult(element, status);
          continue;
        }
        if (element->cycle_index == -1) {
          DisableAutotune(ctx_.get(), element->iterator.get());
        }
      }
    }

    // Adds an error result for the given element.
    void AddErrorResult(std::shared_ptr<Element> element, Status status)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      auto result = std::make_shared<Result>();
      result->status = status;
      element->results.push_back(std::move(result));
      NotifyElementUpdate(element);
    }

    // Cancels all threads (including the manager) and waits for them to finish.
    void StopAllThreads(mutex_lock* l) TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {}

    // Waits on the given cond_var in a worker thread.
    void WaitWorkerThread(condition_variable* cond_var, mutex_lock* l)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      DecrementActiveWorkers();
      RecordStop(ctx_.get());
      cond_var->wait(*l);
      RecordStart(ctx_.get());
      IncrementActiveWorkers();
    }

    void NotifyElementUpdate(std::shared_ptr<Element> element)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (deterministic_) {
        element->cond_var.notify_one();
      } else {
        any_element_available_cond_var_.notify_one();
      }
    }

    bool NeedsProcessing(const std::shared_ptr<Element>& element)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (!element) {
        return false;
      }
      if (!element->initialized) {
        return true;
      }
      return element->iterator &&
             element->results.size() < dataset()->buffer_output_elements_;
    }

    inline void IncrementCurrentWorkers() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      num_current_workers_++;
    }

    inline void DecrementCurrentWorkers() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      num_current_workers_--;
    }

    inline void IncrementActiveWorkers() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      num_active_workers_++;
    }

    inline void DecrementActiveWorkers() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      num_active_workers_--;
      if (num_active_workers_ == 0) {
        zero_active_workers_cond_var_.notify_one();
      }
    }

    inline void IncrementCurrentActiveWorkers()
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      num_current_active_workers_++;
    }

    inline void DecrementCurrentActiveWorkers()
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      num_current_active_workers_--;
    }

    inline void IncrementOutstandingThreads() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      outstanding_threads_++;
    }

    inline void DecrementOutstandingThreads() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      outstanding_threads_--;
      if (outstanding_threads_ == 0) {
        outstanding_threads_finished_cond_var_.notify_one();
      }
    }

    void StatsThread() {
      for (int64_t step = 0;; ++step) {
        int num_current_active_workers;
        int num_current_workers;
        {
          mutex_lock l(*mu_);
          if (step != 0 && !cancelled_) {
            stats_thread_cond_var_.wait_for(
                l, std::chrono::milliseconds(kStatsReportingPeriodMillis));
          }
          if (cancelled_) {
            DecrementOutstandingThreads();
            return;
          }
          num_current_active_workers = num_current_active_workers_;
          num_current_workers = num_current_workers_;
        }
        if (num_current_workers == 0) {
          // Avoid division by zero.
          num_current_workers = 1;
        }
        ctx_->stats_aggregator()->AddScalar(
            stats_utils::ThreadUtilizationScalarName(dataset()->node_name()),
            static_cast<float>(num_current_active_workers) /
                static_cast<float>(num_current_workers),
            step);
      }
    }

    Status WriteStatusLocked(IteratorStateWriter* writer,
                             const string& iterator_name, size_t idx,
                             const Status& status)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      TF_RETURN_IF_ERROR(writer->WriteScalar(
          iterator_name, CodeKey(idx), static_cast<int64_t>(status.code())));
      if (!status.ok()) {
        TF_RETURN_IF_ERROR(writer->WriteScalar(
            iterator_name, ErrorMessageKey(idx), status.error_message()));
      }
      return Status::OK();
    }

    Status ReadStatusLocked(IteratorStateReader* reader,
                            const string& iterator_name, size_t idx,
                            Status* status) TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      int64_t code_int;
      TF_RETURN_IF_ERROR(
          reader->ReadScalar(iterator_name, CodeKey(idx), &code_int));
      error::Code code = static_cast<error::Code>(code_int);

      if (code != error::Code::OK) {
        tstring error_message;
        TF_RETURN_IF_ERROR(reader->ReadScalar(
            iterator_name, ErrorMessageKey(idx), &error_message));
        *status = Status(code, error_message);
      } else {
        *status = Status::OK();
      }
      return Status::OK();
    }

    string CodeKey(size_t idx) {
      return absl::StrCat(kResultsSuffix, "[", idx, "]", kCodeSuffix);
    }

    string ErrorMessageKey(size_t idx) {
      return absl::StrCat(kResultsSuffix, "[", idx, "]", kErrorMessageSuffix);
    }

    Status WriteElement(SerializationContext* ctx,
                        std::shared_ptr<Element> element, int idx,
                        const string& key_prefix, IteratorStateWriter* writer)
        TF_EXCLUSIVE_LOCKS_REQUIRED(*mu_) {
      const auto& iterator_name =
          absl::StrCat(prefix(), "::", key_prefix, "::", idx);
      if (element->iterator) {
        TF_RETURN_IF_ERROR(SaveInput(ctx, writer, element->iterator));
        TF_RETURN_IF_ERROR(
            writer->WriteScalar(iterator_name, kIdSuffix, element->id));
        TF_RETURN_IF_ERROR(writer->WriteScalar(
            iterator_name, absl::StrCat(kInputsSuffix, kSizeSuffix),
            element->inputs->size()));
        for (int i = 0; i < element->inputs->size(); i++) {
          TF_RETURN_IF_ERROR(writer->WriteTensor(
              iterator_name, absl::StrCat(kInputsSuffix, "[", i, "]"),
              element->inputs->at(i)));
        }
      }
      TF_RETURN_IF_ERROR(writer->WriteScalar(
          iterator_name, absl::StrCat(kResultsSuffix, kSizeSuffix),
          element->results.size()));
      for (size_t i = 0; i < element->results.size(); i++) {
        std::shared_ptr<Result> result = element->results[i];
        TF_RETURN_IF_ERROR(
            WriteStatusLocked(writer, iterator_name, i, result->status));
        TF_RETURN_IF_ERROR(writer->WriteScalar(
            iterator_name,
            absl::StrCat(kResultsSuffix, "[", i, "]", kSizeSuffix),
            result->return_values.size()));
        for (size_t j = 0; j < result->return_values.size(); j++) {
          TF_RETURN_IF_ERROR(writer->WriteTensor(
              iterator_name, absl::StrCat(kResultsSuffix, "[", i, "][", j, "]"),
              result->return_values[j]));
        }
        TF_RETURN_IF_ERROR(writer->WriteScalar(
            iterator_name,
            absl::StrCat(kResultsSuffix, "[", i, "]", kIsReadySuffix), ""));
      }
      return Status::OK();
    }

    Status WriteCurrentElements(SerializationContext* ctx,
                                IteratorStateWriter* writer)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      TF_RETURN_IF_ERROR(writer->WriteScalar(prefix(), kCurrentElementsSize,
                                             current_elements_.size()));
      for (int idx = 0; idx < current_elements_.size(); idx++) {
        if (current_elements_[idx]) {
          TF_RETURN_IF_ERROR(WriteElement(ctx, current_elements_[idx], idx,
                                          kCurrentElements, writer));
        }
      }
      return Status::OK();
    }

    Status WriteFutureElements(SerializationContext* ctx,
                               IteratorStateWriter* writer)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      TF_RETURN_IF_ERROR(writer->WriteScalar(prefix(), kFutureElementsSize,
                                             future_elements_.size()));
      for (int idx = 0; idx < future_elements_.size(); idx++) {
        if (future_elements_[idx]) {
          TF_RETURN_IF_ERROR(WriteElement(ctx, future_elements_[idx], idx,
                                          kFutureElements, writer));
        }
      }
      return Status::OK();
    }

    Status ReadElement(IteratorContext* ctx, IteratorStateReader* reader,
                       int idx, const string& key_prefix,
                       std::shared_ptr<Element>* out) {
      std::unique_ptr<IteratorBase> iterator;
      auto element = std::make_shared<Element>();
      {
        mutex_lock l(*mu_);
        const auto& iterator_name =
            absl::StrCat(prefix(), "::", key_prefix, "::", idx);
        if (!reader->Contains(iterator_name,
                              absl::StrCat(kResultsSuffix, kSizeSuffix))) {
          return Status::OK();
        }
        int64_t results_size;
        TF_RETURN_IF_ERROR(reader->ReadScalar(
            iterator_name, absl::StrCat(kResultsSuffix, kSizeSuffix),
            &results_size));
        element->results.resize(results_size);
        for (size_t i = 0; i < results_size; i++) {
          auto result = std::make_shared<Result>();
          TF_RETURN_IF_ERROR(
              ReadStatusLocked(reader, iterator_name, i, &result->status));
          int64_t num_return_values;
          TF_RETURN_IF_ERROR(reader->ReadScalar(
              iterator_name,
              absl::StrCat(kResultsSuffix, "[", i, "]", kSizeSuffix),
              &num_return_values));
          result->return_values.reserve(num_return_values);
          for (size_t j = 0; j < num_return_values; j++) {
            result->return_values.emplace_back();
            TF_RETURN_IF_ERROR(reader->ReadTensor(
                ctx->flr(), iterator_name,
                absl::StrCat(kResultsSuffix, "[", i, "][", j, "]"),
                &result->return_values.back()));
          }
          RecordBufferEnqueue(ctx, result->return_values);
          element->results[i] = std::move(result);
        }
        if (!reader->Contains(iterator_name,
                              absl::StrCat(kInputsSuffix, kSizeSuffix))) {
          element->iterator.reset();
          *out = std::move(element);
          return Status::OK();
        }
        int64_t inputs_size;
        TF_RETURN_IF_ERROR(reader->ReadScalar(
            iterator_name, absl::StrCat(kInputsSuffix, kSizeSuffix),
            &inputs_size));
        element->inputs = std::make_unique<std::vector<Tensor>>(inputs_size);
        for (int i = 0; i < inputs_size; i++) {
          TF_RETURN_IF_ERROR(
              reader->ReadTensor(ctx->flr(), iterator_name,
                                 absl::StrCat(kInputsSuffix, "[", i, "]"),
                                 &element->inputs->at(i)));
        }
        TF_RETURN_IF_ERROR(
            reader->ReadScalar(iterator_name, kIdSuffix, &element->id));
        IteratorContext::Params params(ctx);
        params.interleave_depth += 1;
        IteratorContext ctx_copy(params);
        TF_RETURN_IF_ERROR(MakeIteratorFromInputElement(
            &ctx_copy, this, *element->inputs, element->id,
            *instantiated_captured_func_.get(), prefix(), &iterator,
            model_node()));
      }
      TF_RETURN_IF_ERROR(RestoreInput(ctx, reader, iterator));
      mutex_lock l(*mu_);
      element->iterator = std::move(iterator);
      *out = std::move(element);
      return Status::OK();
    }

    Status ReadCurrentElements(IteratorContext* ctx,
                               IteratorStateReader* reader) {
      int64_t size;
      {
        mutex_lock l(*mu_);
        TF_RETURN_IF_ERROR(
            reader->ReadScalar(prefix(), kCurrentElementsSize, &size));
        if (current_elements_.size() != size) {
          // This could mean two things: (1) the user created their checkpoint
          // from a dataset with one cycle_length, then changed the cycle_length
          // and tried to restore from the old checkpoint, or (2) the user set
          // the cycle length to tf.data.AUTOTUNE, wrote the checkpoint from one
          // machine, then tried to restore the checkpoint on another machine
          // with a different CPU budget (causing autotune to pick a different
          // cycle length).
          return errors::FailedPrecondition(
              "The iterator cycle length ", current_elements_.size(),
              " is different from the cycle length to restore from the "
              "checkpoint: ",
              size);
        }
      }
      if (size == 0) {
        return Status::OK();
      }
      std::vector<std::shared_ptr<Element>> elements;
      TF_RETURN_IF_ERROR(
          ReadElementsParallel(ctx, reader, size, kCurrentElements, &elements));
      mutex_lock l(*mu_);
      for (auto& element : current_elements_) {
        DCHECK(element == nullptr);
      }
      for (int idx = 0; idx < size; ++idx) {
        current_elements_[idx] = std::move(elements[idx]);
      }
      return Status::OK();
    }

    Status ReadFutureElements(IteratorContext* ctx,
                              IteratorStateReader* reader) {
      int64_t size;
      {
        mutex_lock l(*mu_);
        TF_RETURN_IF_ERROR(
            reader->ReadScalar(prefix(), kFutureElementsSize, &size));
        future_elements_.resize(size);
      }
      if (size == 0) {
        return Status::OK();
      }
      std::vector<std::shared_ptr<Element>> elements;
      TF_RETURN_IF_ERROR(
          ReadElementsParallel(ctx, reader, size, kFutureElements, &elements));
      mutex_lock l(*mu_);
      for (auto& element : future_elements_) {
        DCHECK(element == nullptr);
      }
      for (int idx = 0; idx < size; ++idx) {
        future_elements_[idx] = std::move(elements[idx]);
      }
      return Status::OK();
    }

    Status ReadElementsParallel(
        IteratorContext* ctx, IteratorStateReader* reader, int64_t size,
        const string& name, std::vector<std::shared_ptr<Element>>* elements) {
      elements->resize(size);
      Status s = Status::OK();
      BlockingCounter counter(size);
      for (int idx = 0; idx < size; ++idx) {
        thread_pool_->Schedule([this, ctx, reader, idx, name, &s, &counter,
                                elements] {
          RecordStart(ctx);
          auto cleanup = gtl::MakeCleanup([this, ctx, &counter]() {
            RecordStop(ctx);
            counter.DecrementCount();
          });
          std::shared_ptr<Element> elem;
          Status ret_status = ReadElement(ctx, reader, idx, name, &elem);
          mutex_lock l(*mu_);
          if (cancelled_) {
            s.Update(errors::Cancelled("Cancelled in ReadElementsParallel"));
            return;
          }
          if (!ret_status.ok()) {
            s.Update(ret_status);
            return;
          }
          (*elements)[idx] = elem;
        });
      }
      counter.Wait();
      return s;
    }

    std::string DebugString() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      std::string result;
      result.append(strings::StrCat("Cycle index: ", cycle_index_, "\n"));
      result.append(strings::StrCat("Block index: ", block_index_, "\n"));
      result.append(strings::StrCat("End of input: ", end_of_input_, "\n"));
      {
        result.append("Current elements:\n");
        for (int i = 0; i < current_elements_.size(); ++i) {
          string element_string = "null";
          if (current_elements_[i]) {
            element_string = current_elements_[i]->DebugString();
          }
          result.append(absl::StrFormat("%d: %s\n", i, element_string));
        }
      }
      {
        result.append("Future elements:\n");
        for (int i = 0; i < future_elements_.size(); ++i) {
          string element_string = "null";
          if (future_elements_[i]) {
            element_string = future_elements_[i]->DebugString();
          }
          result.append(absl::StrFormat("%d: %s\n", i, element_string));
        }
      }
      return result;
    }

    // Indices of `current_elements_` which need to be processed by a current
    // worker.
    std::deque<int> elements_to_process_;

    // The last index in `current_elements_` containing a non-null element.
    // This allows us to optimize the situation when the cycle_length is large
    // but the input dataset doesn't have many elements. By tracking the index
    // of the last valid element, GetNext can avoid checking many null entries
    // each time through the cycle.
    //
    // TODO(aaudibert): Generalize this optimization by removing null elements
    // from `current_elements_`, e.g. by compacting the vector when x% of
    // its elements are null.
    int64_t last_valid_current_element_ TF_GUARDED_BY(mu_) = -1;

    // Identifies whether the current_elements_ vector has been initialized.
    bool initial_elements_created_ TF_GUARDED_BY(mu_) = false;

    // Identifies whether the element threads have been initialized.
    bool threads_initialized_ TF_GUARDED_BY(mu_) = false;

    // Used for coordination between the main thread, the manager threads, and
    // the worker threads.
    //
    // NOTE: We should never call GetNext on the input while holding this mutex.
    const std::shared_ptr<mutex> mu_;

    // Condition variable for waking up current workers.
    condition_variable current_workers_cond_var_;

    // Condition variable for waking up future workers.
    condition_variable future_workers_cond_var_;

    // Condition variable for waking up the stats thread.
    condition_variable stats_thread_cond_var_;

    // Number of active worker threads which might be processing elements,
    // including both current workers and future workers. Used by
    // checkpointing to wait for outstanding work to finish.
    int num_active_workers_ TF_GUARDED_BY(mu_) = 0;

    // Number of active current worker threads.
    int num_current_active_workers_ TF_GUARDED_BY(mu_) = 0;

    // Condition variable notified whenever the total number of active workers
    // drops to zero. Used for checkpointing.
    condition_variable zero_active_workers_cond_var_;

    // Condition notified whenever num_parallel_calls_ changes. Shared so that
    // autotuning can notify us when num_parallel_calls_ changes.
    std::shared_ptr<condition_variable> num_parallel_calls_cond_var_;

    // Identifies the maximum number of parallel calls.
    const std::shared_ptr<model::SharedState> num_parallel_calls_;

    // The number of current workers currently alive or scheduled to be started.
    // This includes current workers which are blocked waiting for work.
    int num_current_workers_ TF_GUARDED_BY(mu_) = 0;

    // Condition variable to signal that a result has been produced by some
    // element thread. Only used when `deterministic` is false.
    condition_variable any_element_available_cond_var_;

    // Determines whether outputs can be produced in deterministic order.
    const bool deterministic_;

    // Controls cancellation of `input_impl_`. Must be ordered before
    // `input_impl_` so that `input_impl_` is destroyed first.
    std::unique_ptr<CancellationManager> cancellation_manager_;

    // Iterator for input elements.
    std::unique_ptr<IteratorBase> input_impl_ TF_GUARDED_BY(mu_);

    // Identifies position in the interleave cycle.
    int64_t block_index_ TF_GUARDED_BY(mu_) = 0;
    // It is an invariant that either `last_valid_current_element_ == -1` or
    // `cycle_index_ <= last_valid_current_element_`.
    int64_t cycle_index_ TF_GUARDED_BY(mu_) = 0;

    // Elements of the current interleave cycle.
    std::vector<std::shared_ptr<Element>> current_elements_ TF_GUARDED_BY(mu_);

    // Elements which still need their inputs and iterators to be initialized.
    // Elements at the front need to be initialized first.
    std::deque<std::shared_ptr<Element>> uninitialized_elements_
        TF_GUARDED_BY(mu_);

    // Elements to be used in the interleave cycle in the future. The element
    // at the front is the next element to add to the interleave cycle when a
    // current element is exhausted.
    std::deque<std::shared_ptr<Element>> future_elements_ TF_GUARDED_BY(mu_);

    // Identifies whether the global end of input has been reached.
    bool end_of_input_ TF_GUARDED_BY(mu_) = false;

    // The number of outstanding element threads.
    int outstanding_threads_ TF_GUARDED_BY(mu_) = 0;

    // Condition variable notified when outstanding_threads_ drops to 0.
    condition_variable outstanding_threads_finished_cond_var_;

    std::unique_ptr<thread::ThreadPool> thread_pool_;

    int64_t element_id_counter_ TF_GUARDED_BY(mu_) = 0;

    // Iterator context used in worker threads.
    std::unique_ptr<IteratorContext> ctx_;

    // Set to true during checkpointing to alert element threads that they
    // should pause operation. This is needed to prevent constantly-active
    // worker threads from blocking checkpointing indefinitely.
    bool wait_for_checkpoint_ = false;

    std::unique_ptr<InstantiatedCapturedFunction> instantiated_captured_func_;

    // Identifies whether background threads should be cancelled.
    bool cancelled_ TF_GUARDED_BY(mu_) = false;

    // Records the number of ParallelInterleave operations in the path from the
    // root node to this node (not including this node) in the input pipeline
    // tree. We record the interleave depth so that it can be included in the
    // trace metadata.
    int64 interleave_depth_ = -1;
  };

  const DatasetBase* const input_;
  const std::unique_ptr<CapturedFunction> captured_func_;
  const int64_t cycle_length_;
  const int64_t block_length_;
  const int64_t buffer_output_elements_;
  const int64_t prefetch_input_elements_;
  const int64_t num_parallel_calls_;
  const DeterminismPolicy deterministic_;
  const DataTypeVector output_types_;
  const std::vector<PartialTensorShape> output_shapes_;
  const int op_version_;
  const TraceMeMetadata traceme_metadata_;
};

ParallelInterleaveDatasetOp::ParallelInterleaveDatasetOp(
    OpKernelConstruction* ctx)
    : UnaryDatasetOpKernel(ctx),
      op_version_(OpVersionFromOpName(ctx->def().op())) {
  OP_REQUIRES_OK(ctx, FunctionMetadata::Create(ctx, kFunc, /*params=*/{},
                                               &func_metadata_));
  OP_REQUIRES_OK(ctx, ctx->GetAttr(kOutputTypes, &output_types_));
  OP_REQUIRES_OK(ctx, ctx->GetAttr(kOutputShapes, &output_shapes_));
  if (op_version_ == 2) {
    bool sloppy;
    OP_REQUIRES_OK(ctx, ctx->GetAttr(kSloppy, &sloppy));
    if (sloppy) {
      deterministic_ =
          DeterminismPolicy(DeterminismPolicy::Type::kNondeterministic);
    } else {
      deterministic_ = DeterminismPolicy(DeterminismPolicy::Type::kDefault);
    }
  }
  if (op_version_ >= 3) {
    std::string deterministic;
    OP_REQUIRES_OK(ctx, ctx->GetAttr(kDeterministic, &deterministic));
    OP_REQUIRES_OK(
        ctx, DeterminismPolicy::FromString(deterministic, &deterministic_));
  }
}

void ParallelInterleaveDatasetOp::MakeDataset(OpKernelContext* ctx,
                                              DatasetBase* input,
                                              DatasetBase** output) {
  int64_t block_length = 0;
  OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kBlockLength, &block_length));
  OP_REQUIRES(ctx, block_length > 0,
              errors::InvalidArgument("`block_length` must be > 0"));

  int64_t buffer_output_elements = model::kAutotune;
  int64_t prefetch_input_elements = model::kAutotune;
  if (op_version_ >= 4) {
    OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kBufferOutputElements,
                                            &buffer_output_elements));
    OP_REQUIRES(ctx,
                buffer_output_elements == model::kAutotune ||
                    buffer_output_elements > 0,
                errors::InvalidArgument("`buffer_output_elements` must be ",
                                        model::kAutotune, " or > 0 but is ",
                                        buffer_output_elements));

    OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kPrefetchInputElements,
                                            &prefetch_input_elements));
    OP_REQUIRES(ctx,
                prefetch_input_elements == model::kAutotune ||
                    prefetch_input_elements >= 0,
                errors::InvalidArgument("`prefetch_input_elements` must be ",
                                        model::kAutotune, " or >= 0 but is ",
                                        prefetch_input_elements));
  }

  int64_t num_parallel_calls = 0;
  OP_REQUIRES_OK(
      ctx, ParseScalarArgument(ctx, kNumParallelCalls, &num_parallel_calls));
  OP_REQUIRES(
      ctx, num_parallel_calls > 0 || num_parallel_calls == model::kAutotune,
      errors::InvalidArgument("num_parallel_calls must be greater than zero."));
  int64_t cycle_length = 0;
  OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kCycleLength, &cycle_length));
  if (cycle_length == model::kAutotune) {
    if (num_parallel_calls != model::kAutotune) {
      cycle_length = std::min(num_parallel_calls,
                              static_cast<int64_t>(port::MaxParallelism()));
    } else {
      // If parallelism is to be autotuned, we set the cycle length so that
      // the number of thread created for the current and future cycle elements
      // roughly matches the number of schedulable cores.
      const int num_threads_per_cycle_length = kDefaultCyclePrefetchFactor + 1;
      cycle_length =
          CeilDiv(port::MaxParallelism(), num_threads_per_cycle_length);
    }
  }
  OP_REQUIRES(ctx, cycle_length > 0,
              errors::InvalidArgument("`cycle_length` must be > 0"));

  OP_REQUIRES(
      ctx, num_parallel_calls <= cycle_length,
      errors::InvalidArgument(
          "num_parallel_calls must less than or equal to cycle_length."));

  std::unique_ptr<CapturedFunction> captured_func;
  OP_REQUIRES_OK(ctx,
                 CapturedFunction::Create(ctx, func_metadata_, kOtherArguments,
                                          &captured_func));

  if (num_parallel_calls == model::kAutotune) {
    metrics::RecordTFDataAutotune(kDatasetType);
  }

  *output = new Dataset(
      ctx, input, std::move(captured_func), cycle_length, block_length,
      buffer_output_elements, prefetch_input_elements, num_parallel_calls,
      deterministic_, output_types_, output_shapes_, op_version_);
}

namespace {
REGISTER_KERNEL_BUILDER(Name(kParallelInterleaveDatasetV2).Device(DEVICE_CPU),
                        ParallelInterleaveDatasetOp);
REGISTER_KERNEL_BUILDER(Name(kParallelInterleaveDatasetV3).Device(DEVICE_CPU),
                        ParallelInterleaveDatasetOp);
REGISTER_KERNEL_BUILDER(Name(kParallelInterleaveDatasetV4).Device(DEVICE_CPU),
                        ParallelInterleaveDatasetOp);
REGISTER_INPUT_COLOCATION_EXEMPTION(kParallelInterleaveDatasetV2);
REGISTER_INPUT_COLOCATION_EXEMPTION(kParallelInterleaveDatasetV3);
REGISTER_INPUT_COLOCATION_EXEMPTION(kParallelInterleaveDatasetV4);
}  // namespace
}  // namespace data
}  // namespace tensorflow
