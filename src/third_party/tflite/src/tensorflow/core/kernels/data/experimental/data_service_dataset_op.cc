/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/core/kernels/data/experimental/data_service_dataset_op.h"

#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "tensorflow/core/data/dataset.pb.h"
#include "tensorflow/core/data/dataset_utils.h"
#include "tensorflow/core/data/name_utils.h"
#include "tensorflow/core/data/serialization_utils.h"
#include "tensorflow/core/data/service/common.h"
#include "tensorflow/core/data/service/common.pb.h"
#include "tensorflow/core/data/service/dispatcher.pb.h"
#include "tensorflow/core/data/service/dispatcher_client.h"
#include "tensorflow/core/data/service/grpc_util.h"
#include "tensorflow/core/data/service/worker.pb.h"
#include "tensorflow/core/data/service/worker_client.h"
#include "tensorflow/core/data/service/worker_impl.h"
#include "tensorflow/core/distributed_runtime/rpc/grpc_util.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/model.h"
#include "tensorflow/core/framework/partial_tensor_shape.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/gtl/cleanup.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/snappy.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/statusor.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/profiler/lib/traceme.h"
#include "tensorflow/core/profiler/lib/traceme_encode.h"
#include "tensorflow/core/protobuf/data_service.pb.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"

namespace tensorflow {
namespace data {

/* static */ constexpr const char* const DataServiceDatasetOp::kDatasetType;
/* static */ constexpr const char* const DataServiceDatasetOp::kDatasetId;
/* static */ constexpr const char* const DataServiceDatasetOp::kProcessingMode;
/* static */ constexpr const char* const DataServiceDatasetOp::kAddress;
/* static */ constexpr const char* const DataServiceDatasetOp::kProtocol;
/* static */ constexpr const char* const
    DataServiceDatasetOp::kDataTransferProtocol;
/* static */ constexpr const char* const DataServiceDatasetOp::kJobName;
/* static */ constexpr const char* const DataServiceDatasetOp::kConsumerIndex;
/* static */ constexpr const char* const DataServiceDatasetOp::kNumConsumers;
/* static */ constexpr const char* const
    DataServiceDatasetOp::kMaxOutstandingRequests;
/* static */ constexpr const char* const
    DataServiceDatasetOp::kTaskRefreshIntervalHintMs;
/* static */ constexpr const char* const DataServiceDatasetOp::kTargetWorkers;
/* static */ constexpr const char* const
    DataServiceDatasetOp::kIterationCounter;
/* static */ constexpr const char* const DataServiceDatasetOp::kOutputTypes;
/* static */ constexpr const char* const DataServiceDatasetOp::kOutputShapes;

namespace {
// Default interval between task list refreshes.
const int64_t kDefaultTaskRefreshIntervalMs = 1000;  // 1 second.

constexpr char kDataServiceDatasetV1[] = "DataServiceDataset";
constexpr char kDataServiceDatasetV2[] = "DataServiceDatasetV2";

constexpr const char kParallelEpochs[] = "parallel_epochs";
constexpr const char kDistributedEpoch[] = "distributed_epoch";
}  // namespace

// Dataset for reading data from the tf.data service non-deterministically.
//
// This dataset interleaves dataset elements produced by multiple tf.data
// workers. We periodically query the dispatcher to determine which workers
// to read from (in case workers are added or removed).
class DataServiceDatasetOp::Dataset : public DatasetBase {
 public:
  Dataset(OpKernelContext* ctx, int op_version, int64_t dataset_id,
          const ProcessingModeDef& processing_mode, const std::string& address,
          const std::string& protocol,
          const std::string& data_transfer_protocol,
          const std::string& job_name, absl::optional<int64_t> consumer_index,
          absl::optional<int64_t> num_consumers,
          int64_t max_outstanding_requests, int64_t task_refresh_interval_ms,
          const TargetWorkers target_workers,
          IterationCounter* iteration_counter, bool owns_resource,
          ResourceHandle iteration_counter_handle,
          const DataTypeVector& output_types,
          const std::vector<PartialTensorShape>& output_shapes)
      : DatasetBase(DatasetContext(ctx)),
        op_version_(op_version),
        dataset_id_(dataset_id),
        processing_mode_(processing_mode),
        address_(address),
        protocol_(protocol),
        data_transfer_protocol_(data_transfer_protocol),
        job_name_(job_name),
        consumer_index_(consumer_index),
        num_consumers_(num_consumers),
        max_outstanding_requests_(max_outstanding_requests),
        task_refresh_interval_ms_(task_refresh_interval_ms),
        target_workers_(target_workers),
        iteration_counter_(iteration_counter),
        owns_resource_(owns_resource),
        iteration_counter_handle_(iteration_counter_handle),
        resource_mgr_(ctx->resource_manager()),
        output_types_(output_types),
        output_shapes_(output_shapes) {}

  ~Dataset() override {
    iteration_counter_->Unref();
    if (owns_resource_) {
      Status s = resource_mgr_->Delete<IterationCounter>(
          iteration_counter_handle_.container(),
          iteration_counter_handle_.name());
      if (!s.ok()) {
        LOG(WARNING) << "Failed to delete iteration counter resource: " << s;
      }
    }
  }

  std::unique_ptr<IteratorBase> MakeIteratorInternal(
      const string& prefix) const override {
    return absl::make_unique<Iterator>(
        Iterator::Params{this,
                         name_utils::IteratorPrefix(kDatasetType, prefix)},
        iteration_counter_->GetAndIncrement());
  }

  const DataTypeVector& output_dtypes() const override { return output_types_; }

  const std::vector<PartialTensorShape>& output_shapes() const override {
    return output_shapes_;
  }

  string DebugString() const override {
    return name_utils::DatasetDebugString(kDatasetType);
  }

  Status CheckExternalState() const override {
    return Status(
        error::FAILED_PRECONDITION,
        strings::StrCat(DebugString(), " does not yet support serialization."));
  }

  Status InputDatasets(std::vector<const DatasetBase*>* inputs) const override {
    inputs->clear();
    return Status::OK();
  }

 protected:
  Status AsGraphDefInternal(SerializationContext* ctx,
                            DatasetGraphDefBuilder* b,
                            Node** output) const override {
    std::vector<Node*> inputs;

    Node* dataset_id;
    TF_RETURN_IF_ERROR(b->AddScalar(dataset_id_, &dataset_id));
    inputs.push_back(dataset_id);

    Node* processing_mode;
    tstring processing_mode_str = processing_mode_.SerializeAsString();
    TF_RETURN_IF_ERROR(b->AddScalar(processing_mode_str, &processing_mode));
    inputs.push_back(processing_mode);

    Node* address;
    TF_RETURN_IF_ERROR(b->AddScalar(address_, &address));
    inputs.push_back(address);

    Node* protocol;
    TF_RETURN_IF_ERROR(b->AddScalar(protocol_, &protocol));
    inputs.push_back(protocol);

    AttrValue data_transfer_protocol;
    b->BuildAttrValue(data_transfer_protocol_, &data_transfer_protocol);

    Node* job_name;
    TF_RETURN_IF_ERROR(b->AddScalar(job_name_, &job_name));
    inputs.push_back(job_name);

    if (op_version_ == 2) {
      Node* consumer_index;
      TF_RETURN_IF_ERROR(
          b->AddScalar(consumer_index_.value_or(-1), &consumer_index));
      inputs.push_back(consumer_index);

      Node* num_consumers;
      TF_RETURN_IF_ERROR(
          b->AddScalar(num_consumers_.value_or(-1), &num_consumers));
      inputs.push_back(num_consumers);
    }

    Node* max_outstanding_requests;
    TF_RETURN_IF_ERROR(
        b->AddScalar(max_outstanding_requests_, &max_outstanding_requests));
    inputs.push_back(max_outstanding_requests);

    Node* iteration_counter_handle = nullptr;
    Tensor handle(DT_RESOURCE, TensorShape({}));
    handle.scalar<ResourceHandle>()() = iteration_counter_handle_;
    TF_RETURN_IF_ERROR(b->AddTensor(handle, &iteration_counter_handle));
    inputs.push_back(iteration_counter_handle);

    AttrValue task_refresh_interval_hint_ms;
    b->BuildAttrValue(task_refresh_interval_ms_,
                      &task_refresh_interval_hint_ms);

    AttrValue target_workers;
    b->BuildAttrValue(TargetWorkersToString(target_workers_), &target_workers);

    TF_RETURN_IF_ERROR(b->AddDataset(
        this, inputs,
        {std::make_pair(kTaskRefreshIntervalHintMs,
                        task_refresh_interval_hint_ms),
         std::make_pair(kDataTransferProtocol, data_transfer_protocol),
         std::make_pair(kTargetWorkers, target_workers)},
        output));
    return Status::OK();
  }

 private:
  class Iterator : public DatasetIterator<Dataset> {
   public:
    explicit Iterator(const Params& params, int64_t iterator_index)
        : DatasetIterator<Dataset>(params),
          iterator_index_(iterator_index),
          max_outstanding_requests_(params.dataset->max_outstanding_requests_) {
    }

    ~Iterator() override {
      VLOG(1) << "Destroying data service dataset iterator for job id "
              << job_client_id_;
      CancelThreads();
      if (deregister_fn_) deregister_fn_();
      task_thread_manager_.reset();
      if (initialized_) {
        Status s = dispatcher_->ReleaseJobClient(job_client_id_);
        if (!s.ok()) {
          LOG(WARNING) << "Failed to release job client id: " << s;
        }
      }
      for (auto& worker_thread : worker_threads_) {
        worker_thread.reset();
      }
      DeleteLocalWorkerTasks();
      VLOG(1) << "Destroyed data service dataset iterator for job id "
              << job_client_id_;
    }

    Status Initialize(IteratorContext* ctx) override {
      TF_RETURN_IF_ERROR(ValidateDataset());
      VLOG(3) << "Connecting to " << dataset()->address_
              << " in data service dataset op";
      TF_RETURN_IF_ERROR(RegisterCancellationCallback(
          ctx->cancellation_manager(), [this]() { CancelThreads(); },
          &deregister_fn_));
      dispatcher_ = absl::make_unique<DataServiceDispatcherClient>(
          dataset()->address_, dataset()->protocol_);
      int64_t deadline_micros = kint64max;
      absl::optional<JobKey> key;
      if (!dataset()->job_name_.empty()) {
        key.emplace();
        key.value().set_job_name(std::string(dataset()->job_name_));
        key.value().set_job_name_index(iterator_index_);
      }
      TF_RETURN_IF_ERROR(grpc_util::Retry(
          [&]() {
            return dispatcher_->GetOrCreateJob(
                dataset()->dataset_id_, dataset()->processing_mode_, key,
                dataset()->num_consumers_, dataset()->target_workers_,
                job_client_id_);
          },
          /*description=*/
          strings::StrCat("get or create job with dispatcher at ",
                          dataset()->address_),
          deadline_micros));
      initialized_ = true;
      return Status::OK();
    }

    Status GetNextInternal(IteratorContext* ctx,
                           std::vector<Tensor>* out_tensors,
                           bool* end_of_sequence) override {
      VLOG(3) << "Calling GetNext in data service dataset op";
      mutex_lock l(mu_);
      EnsureThreadsStarted(ctx);
      bool skip = true;
      while (skip) {
        while ((results_.empty() || !results_.front().ready) && !Finished() &&
               !cancelled_ && status_.ok()) {
          VLOG(3) << "Blocking in GetNext. results_.size():" << results_.size()
                  << " results_.front().ready:"
                  << (!results_.empty() && results_.front().ready)
                  << " job_finished_:" << job_finished_
                  << " tasks size:" << tasks_.size()
                  << " finished_tasks_:" << finished_tasks_
                  << " num_running_worker_threads_:"
                  << num_running_worker_threads_;
          get_next_cv_.wait(l);
        }
        if (cancelled_) {
          VLOG(3) << "Returning from GetNext due to cancellation";
          return errors::Cancelled("Data service iterator was cancelled");
        }
        if (!status_.ok()) {
          VLOG(3) << "Returning from GetNext with error " << status_;
          return status_;
        }
        if (results_.empty()) {
          *end_of_sequence = true;
          VLOG(3) << "Returning from GetNext with end_of_sequence";
          return Status::OK();
        }
        skip = results_.front().skip;
        if (skip) {
          results_.pop();
          worker_thread_cv_.notify_one();
        }
      }
      auto& result = results_.front();
      *end_of_sequence = result.end_of_sequence;
      if (!*end_of_sequence) {
        out_tensors->swap(result.element);
        if (StrictRoundRobin()) {
          VLOG(1) << "Consumer " << dataset()->consumer_index_.value()
                  << ": Result " << get_next_index_++
                  << " from GetNext comes from task " << result.task_id
                  << " element " << result.element_index;
        }
      }
      results_.pop();
      worker_thread_cv_.notify_one();
      return Status::OK();
    }

   protected:
    std::shared_ptr<model::Node> CreateNode(
        IteratorContext* ctx, model::Node::Args args) const override {
      return model::MakeKnownRatioNode(std::move(args),
                                       /*ratio=*/1);
    }

    Status SaveInternal(SerializationContext* ctx,
                        IteratorStateWriter* writer) override {
      return errors::Unimplemented("SaveInternal is not yet supported");
    }

    Status RestoreInternal(IteratorContext* ctx,
                           IteratorStateReader* reader) override {
      return errors::Unimplemented("RestoreInternal is not yet supported");
    }

    data::TraceMeMetadata GetTraceMeMetadata() const override {
      data::TraceMeMetadata result;
      int64_t num_tasks = -1;
      if (mu_.try_lock()) {
        num_tasks = tasks_.size() - finished_tasks_;
        mu_.unlock();
      }
      result.push_back(std::make_pair(
          "num_tasks",
          num_tasks == -1
              ? kTraceInfoUnavailable
              : strings::Printf("%lld", static_cast<long long>(num_tasks))));
      result.push_back(std::make_pair("job_name", dataset()->job_name_));
      result.push_back(std::make_pair(
          "max_outstanding_requests",
          strings::Printf("%lld", static_cast<long long>(
                                      dataset()->max_outstanding_requests_))));
      return result;
    }

   private:
    struct Task {
      Task(const TaskInfo& info,
           std::unique_ptr<DataServiceWorkerClient> worker)
          : info(info), worker(std::move(worker)) {}

      const TaskInfo info;
      // Client for fetching task elements from the tf.data service worker.
      const std::unique_ptr<DataServiceWorkerClient> worker;
      // The next round to read from the task.
      int64_t round = 0;
      // Whether the task has been removed. The task will eventually be
      // deleted from `tasks_` on the next dispatcher heartbeat.
      bool removed = false;
      bool skipped_previous_round = false;
      // Indicates whether a worker thread is currently processing the task.
      bool in_use TF_GUARDED_BY(&Iterator::mu_) = false;
      // Indicates whether the worker has returned end_of_sequence for the task.
      bool end_of_sequence TF_GUARDED_BY(&Iterator::mu_) = false;
    };

    struct Result {
      // Whether the result has been computed yet. GetNext needs to block
      // until the next result is ready.
      bool ready TF_GUARDED_BY(&Iterator::mu_) = false;
      std::vector<Tensor> element TF_GUARDED_BY(&Iterator::mu_);
      // The element's index within the tf.data worker it came from. Used for
      // debugging.
      int64_t element_index TF_GUARDED_BY(&Iterator::mu_) = -1;
      // The id of the task that generated the result.
      int64_t task_id TF_GUARDED_BY(&Iterator::mu_) = -1;
      bool end_of_sequence TF_GUARDED_BY(&Iterator::mu_) = false;
      bool skip TF_GUARDED_BY(&Iterator::mu_) = false;
    };

    Status ValidateDataset() const {
      if (dataset()->target_workers_ == TARGET_WORKERS_LOCAL &&
          LocalWorkers::Empty()) {
        if (IsStaticShard(dataset()->processing_mode_)) {
          return errors::InvalidArgument(
              "Static sharding policy <",
              ProcessingModeDef::ShardingPolicy_Name(
                  dataset()->processing_mode_.sharding_policy()),
              "> requires local tf.data workers, but no local worker is found. "
              "You need to run local tf.data service workers in your training "
              "workers. Static sharding also requires a fixed worker pool and "
              "a list of worker addresses in the DispatcherConfig. See the "
              "\"Processing Modes\" section in the module doc for details.");
        }
        return errors::InvalidArgument(
            "Local reads require local tf.data workers, but no local worker "
            "is found. You need to run local tf.data service workers in your "
            "training workers.");
      }
      if (dataset()->target_workers_ == TARGET_WORKERS_LOCAL &&
          StrictRoundRobin()) {
        return errors::InvalidArgument(
            "Coordinated reads require non-local workers, but `target_workers` "
            "is \"LOCAL\".");
      }
      if (IsStaticShard(dataset()->processing_mode_) &&
          dataset()->target_workers_ != TARGET_WORKERS_LOCAL) {
        return errors::InvalidArgument(
            "Static sharding policy <",
            ProcessingModeDef::ShardingPolicy_Name(
                dataset()->processing_mode_.sharding_policy()),
            "> requires reading from local workers, but `target_workers` is ",
            TargetWorkersToString(dataset()->target_workers_), ".");
      }
      return Status::OK();
    }

    // Returns whether all local tasks have finished.
    bool LocalTasksFinished() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      return !tasks_.empty() && finished_tasks_ >= tasks_.size();
    }

    // Returns whether the iterator has finished and should return.
    // If `target_workers_` is LOCAL, it waits for all local tasks to finish.
    // If `target_workers_` is ANY, it waits for the job to finish.
    bool Finished() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (num_running_worker_threads_ > 0) {
        return false;
      }
      if (dataset()->target_workers_ == TARGET_WORKERS_LOCAL) {
        return job_finished_ || LocalTasksFinished();
      }
      return job_finished_;
    }

    void EnsureThreadsStarted(IteratorContext* ctx)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (!task_thread_manager_ && !cancelled_) {
        auto new_ctx = std::make_shared<IteratorContext>(*ctx);
        task_thread_manager_ =
            ctx->StartThread("task-thread-manager",
                             [this, new_ctx]() { TaskThreadManager(new_ctx); });
      }
    }

    void CancelThreads() TF_LOCKS_EXCLUDED(mu_) {
      mutex_lock l(mu_);
      for (const auto& task : tasks_) {
        task->worker->TryCancel();
      }
      cancelled_ = true;
      worker_thread_cv_.notify_all();
      manager_thread_cv_.notify_all();
      get_next_cv_.notify_all();
    }

    void DeleteLocalWorkerTasks() {
      if (dataset()->target_workers_ != TARGET_WORKERS_LOCAL) {
        return;
      }
      std::vector<std::shared_ptr<Task>> tasks;
      {
        mutex_lock l(mu_);
        tasks = tasks_;
      }

      for (const std::shared_ptr<Task>& task : tasks) {
        std::shared_ptr<DataServiceWorkerImpl> worker =
            LocalWorkers::Get(task->info.worker_address());
        if (worker) {
          worker->DeleteLocalTask(task->info);
        }
      }
    }

    // Periodically refresh the task list.
    // Maintain one thread fetching elements for each task.
    // TODO(aaudibert): Instead of polling, have dispatcher send updates when
    // the list of tasks changes.
    void TaskThreadManager(std::shared_ptr<IteratorContext> ctx) {
      auto cleanup =
          gtl::MakeCleanup([] { VLOG(1) << "Task thread manager exiting"; });
      VLOG(1) << "Starting task thread manager";
      uint64 next_check = Env::Default()->NowMicros();
      while (true) {
        {
          mutex_lock l(mu_);
          // All units are microseconds.
          while (!cancelled_ && Env::Default()->NowMicros() < next_check) {
            int64_t remaining_time = next_check - Env::Default()->NowMicros();
            VLOG(4) << "Task thread manager waiting for " << remaining_time
                    << "us";
            manager_thread_cv_.wait_for(
                l, std::chrono::microseconds(remaining_time));
          }
          if (cancelled_) {
            VLOG(3) << "Task thread manager finished";
            return;
          }
        }
        Heartbeat();
        UpdateWorkerThreads(ctx.get());
        next_check = Env::Default()->NowMicros() +
                     dataset()->task_refresh_interval_ms_ * 1000;
      }
    }

    void TryBlockRound(int64_t round) TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (round_robin_round_limit_.has_value() &&
          round_robin_round_limit_.value() == round) {
        return;
      }
      if (current_round_ >= round) {
        // In the next heartbeat, notify the dispatcher that we failed to add
        // the task.
        VLOG(1) << "Rejecting request to block round " << round
                << ", because processing has already begun for round "
                << current_round_;
        return;
      }
      VLOG(1) << "Accepting request to block round " << round;
      round_robin_round_limit_ = round;
    }

    void UpdateJobFinished(bool job_finished) TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (!job_finished) {
        return;
      }
      job_finished_ = true;
      get_next_cv_.notify_all();
      worker_thread_cv_.notify_all();
    }

    Status AddTask(const TaskInfo& task_info) TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      TF_ASSIGN_OR_RETURN(
          std::unique_ptr<DataServiceWorkerClient> worker,
          CreateDataServiceWorkerClient(task_info.transfer_address(),
                                        dataset()->protocol_,
                                        dataset()->data_transfer_protocol_));
      tasks_.push_back(std::make_shared<Task>(task_info, std::move(worker)));
      worker_thread_cv_.notify_one();
      if (StrictRoundRobin()) {
        VLOG(1) << "Consumer " << dataset()->consumer_index_.value()
                << " adding task " << task_info.task_id()
                << " to read from worker " << task_info.worker_address()
                << ". Task starting round: " << task_info.starting_round();
        DCHECK_LE(current_round_, task_info.starting_round());
        if (current_round_ == task_info.starting_round()) {
          DCHECK_EQ(next_task_index_, 0);
        }
      }
      return Status::OK();
    }

    void Heartbeat() TF_LOCKS_EXCLUDED(mu_) {
      ClientHeartbeatRequest req;
      req.set_job_client_id(job_client_id_);
      if (StrictRoundRobin()) {
        mutex_lock l(mu_);
        req.set_current_round(current_round_);
        if (round_robin_round_limit_.has_value()) {
          req.set_blocked_round(round_robin_round_limit_.value());
        }
      }
      ClientHeartbeatResponse resp;
      Status s = dispatcher_->ClientHeartbeat(req, resp);
      if (!s.ok()) {
        if (errors::IsAborted(s) || errors::IsUnavailable(s) ||
            errors::IsCancelled(s)) {
          LOG(WARNING)
              << "Failed to heartbeat to dispatcher from job client id "
              << job_client_id_
              << ". Dispatcher address: " << dataset()->address_
              << ". Error: " << s;
          return;
        }
        mutex_lock l(mu_);
        status_ = s;
        get_next_cv_.notify_all();
      }
      mutex_lock l(mu_);
      UpdateJobFinished(resp.job_finished());
      if (resp.optional_block_round_case() ==
          ClientHeartbeatResponse::kBlockRound) {
        TryBlockRound(resp.block_round());
      } else {
        round_robin_round_limit_ = absl::nullopt;
        worker_thread_cv_.notify_all();
      }
      UpdateTasks(resp);
    }

    void UpdateTasks(const ClientHeartbeatResponse& resp)
        TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      absl::flat_hash_map<int64_t, TaskInfo> task_id_to_task;
      for (auto& task : resp.task_info()) {
        task_id_to_task[task.task_id()] = task;
      }
      if (job_finished_) {
        return;
      }

      int index = 0;
      while (index < tasks_.size()) {
        std::shared_ptr<Task> task = tasks_[index];
        if (task_id_to_task.contains(task->info.task_id())) {
          // Remove already-known tasks from `task_id_to_task`, so that at the
          // end of the loop, only new tasks remain.
          task_id_to_task.erase(task->info.task_id());
          ++index;
        } else {
          // Task has been removed.
          if (task->end_of_sequence) {
            finished_tasks_--;
          }
          tasks_.erase(tasks_.begin() + index);
          if (index < next_task_index_) {
            next_task_index_--;
          }
          if (!tasks_.empty() && next_task_index_ >= tasks_.size()) {
            AdvanceTaskIndex();
          }
        }
      }
      for (auto& task : resp.task_info()) {
        auto it = task_id_to_task.find(task.task_id());
        if (it == task_id_to_task.end()) {
          continue;
        }
        if (!ShouldReadFromTask(task)) {
          VLOG(3) << "Skipping untargeted worker task " << task.task_id();
          continue;
        }
        Status s = AddTask(it->second);
        if (!s.ok()) {
          status_ = s;
          get_next_cv_.notify_all();
          break;
        }
      }
      if (dataset()->max_outstanding_requests_ == model::kAutotune) {
        // Adjust max_outstanding_requests to account for newly added tasks.
        max_outstanding_requests_ = tasks_.size();
      }
    }

    bool ShouldReadFromTask(const TaskInfo& task) const {
      if (dataset()->target_workers_ == TARGET_WORKERS_LOCAL &&
          LocalWorkers::Get(task.worker_address()) == nullptr) {
        return false;
      }
      return true;
    }

    void UpdateWorkerThreads(IteratorContext* ctx) TF_LOCKS_EXCLUDED(mu_) {
      mutex_lock l(mu_);
      while (num_running_worker_threads_ < max_outstanding_requests_ &&
             !cancelled_ && status_.ok()) {
        num_running_worker_threads_++;
        outstanding_requests_++;
        auto done = [this]() {
          mutex_lock l(mu_);
          num_running_worker_threads_--;
          outstanding_requests_--;
          get_next_cv_.notify_all();
        };
        worker_threads_.push_back(ctx->StartThread(
            "tf-data-service-task_thread", [this, done = std::move(done)]() {
              RunWorkerThread(std::move(done));
            }));
      }
    }

    void AdvanceTaskIndex() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      next_task_index_++;
      if (next_task_index_ >= tasks_.size()) {
        current_round_++;
        next_task_index_ = 0;
      }
    }

    // Searches for a task to process, returning nullptr if none is found.
    std::shared_ptr<Task> GetTaskToProcess() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      VLOG(4) << "Searching for task to process";
      for (int i = 0; i < tasks_.size(); ++i) {
        std::shared_ptr<Task>& task = tasks_[next_task_index_];
        if (StrictRoundRobin() &&
            (task->in_use ||
             current_round_ >= round_robin_round_limit_.value_or(
                                   std::numeric_limits<int64_t>::max()))) {
          VLOG(4) << "No round robin task found. in_use: " << task->in_use
                  << ". current_round: " << current_round_
                  << ". round_robin_round_limit: "
                  << round_robin_round_limit_.value_or(-1);
          return nullptr;
        }
        if (current_round_ < task->info.starting_round() || task->in_use ||
            task->end_of_sequence || task->removed) {
          VLOG(3) << "Skipping task " << next_task_index_
                  << ". starting round: " << task->info.starting_round()
                  << ". current round: " << current_round_
                  << ". task->in_use: " << task->in_use
                  << ". end_of_sequence: " << task->end_of_sequence
                  << ". task->removed: " << task->removed;
          AdvanceTaskIndex();
          continue;
        }
        task->round = current_round_;
        AdvanceTaskIndex();
        return task;
      }
      return nullptr;
    }

    void RunWorkerThread(std::function<void()> done) {
      auto cleanup = gtl::MakeCleanup([done = std::move(done)]() {
        done();
        VLOG(1) << "Worker thread exiting";
      });
      VLOG(1) << "Starting worker thread";
      std::shared_ptr<Task> task_to_process;
      while (true) {
        Result* result;
        {
          mutex_lock l(mu_);
          if (task_to_process) {
            task_to_process->in_use = false;
            task_to_process = nullptr;
            worker_thread_cv_.notify_one();
          }
          outstanding_requests_--;
          while (true) {
            if (cancelled_ || job_finished_ ||
                (dataset()->target_workers_ == TARGET_WORKERS_LOCAL &&
                 LocalTasksFinished())) {
              return;
            }
            if (ElementSpaceAvailable()) {
              task_to_process = GetTaskToProcess();
              if (task_to_process) {
                break;
              }
            }
            worker_thread_cv_.wait(l);
          }
          outstanding_requests_++;
          if (StrictRoundRobin()) {
            // Reserve a spot in the results_ queue.
            results_.emplace();
            result = &results_.back();
          }
          DCHECK(task_to_process != nullptr);
          task_to_process->in_use = true;
          VLOG(3) << "Processing task " << task_to_process->info.task_id();
        }
        int64_t deadline_micros = kint64max;
        Status s;
        if (StrictRoundRobin()) {
          s = GetElementTraced(task_to_process.get(), deadline_micros,
                               /*enqueue_result=*/false, *result);
        } else {
          Result r;
          s = GetElementTraced(task_to_process.get(), deadline_micros,
                               /*enqueue_result=*/true, r);
        }
        if (!s.ok()) {
          mutex_lock l(mu_);
          VLOG(1) << "Failed to get element from worker "
                  << task_to_process->info.worker_address() << ": " << s;
          task_to_process->in_use = false;
          status_ = Status(s.code(),
                           absl::StrCat("Failed to get element from worker ",
                                        task_to_process->info.worker_address(),
                                        ": ", s.error_message()));
          get_next_cv_.notify_all();
          return;
        }
      }
    }

    Status TryGetElement(const Task& task, GetElementResult& result) {
      GetElementRequest req;
      req.set_task_id(task.info.task_id());
      req.set_skipped_previous_round(task.skipped_previous_round);
      absl::optional<int64_t> round_index;
      if (StrictRoundRobin()) {
        round_index = task.round;
        req.set_consumer_index(dataset()->consumer_index_.value());
        req.set_round_index(task.round);
        req.set_allow_skip(true);
      }
      return task.worker->GetElement(req, result);
    }

    void ProcessGetElementResponse(bool enqueue_result,
                                   GetElementResult& get_element_result,
                                   Result& result, Task& task) {
      mutex_lock l(mu_);
      result.ready = true;
      result.end_of_sequence = get_element_result.end_of_sequence;
      result.skip = get_element_result.skip;
      if (!get_element_result.end_of_sequence && !get_element_result.skip) {
        task.skipped_previous_round = false;
        result.element = std::move(get_element_result.components);
        result.element_index = get_element_result.element_index;
        result.task_id = task.info.task_id();
      } else if (get_element_result.skip) {
        task.skipped_previous_round = true;
      } else {
        task.end_of_sequence = true;
        finished_tasks_++;
      }
      if (enqueue_result && !result.end_of_sequence) {
        results_.push(std::move(result));
      }
      get_next_cv_.notify_all();
    }

    Status GetElementTraced(Task* task, int64_t deadline_micros,
                            bool enqueue_result, Result& result)
        TF_LOCKS_EXCLUDED(mu_) {
      VLOG(3) << "Getting an element for task id " << task->info.task_id();
      tensorflow::profiler::TraceMe activity(
          "GetDataServiceElement", tensorflow::profiler::TraceMeLevel::kInfo);
      activity.AppendMetadata([&]() {
        return profiler::TraceMeEncode(
            {{"address", task->info.worker_address()}});
      });
      if (StrictRoundRobin()) {
        VLOG(3) << "Requesting element from consumer index "
                << dataset()->consumer_index_.value() << ", round "
                << task->round;
        activity.AppendMetadata([&]() {
          return profiler::TraceMeEncode(
              {{"consumer_index", dataset()->consumer_index_.value()},
               {"round_index", task->round}});
        });
      }
      Status s = GetElement(task, deadline_micros, enqueue_result, result);
      mutex_lock l(mu_);
      VLOG(3) << "Returning from GetElement for task id "
              << task->info.task_id();
      return s;
    }

    Status MaybeRemoveTask(Task& task, int64_t deadline_micros,
                           Result& result) {
      bool removed;
      VLOG(1) << "Requesting task removal for worker "
              << task.info.worker_address() << " in round " << task.round;
      TF_RETURN_IF_ERROR(grpc_util::Retry(
          [&] {
            return dispatcher_->MaybeRemoveTask(
                task.info.task_id(), dataset()->consumer_index_.value(),
                task.round, removed);
          },
          /*should_retry=*/
          [&] {
            mutex_lock l(mu_);
            return !cancelled_;
          },
          /*description=*/"request task removal ", deadline_micros));
      if (removed) {
        mutex_lock l(mu_);
        task.removed = true;
        result.ready = true;
        result.skip = true;
        get_next_cv_.notify_all();
        return Status::OK();
      }
      VLOG(1) << "Failed to remove task for worker "
              << task.info.worker_address();
      return Status::OK();
    }

    Status GetElement(Task* task, int64_t deadline_micros, bool enqueue_result,
                      Result& result) TF_LOCKS_EXCLUDED(mu_) {
      GetElementResult get_element_result;
      for (int num_retries = 0;; ++num_retries) {
        Status s = TryGetElement(*task, get_element_result);
        if (s.ok()) break;
        // Retry all errors that could indicate preemption.
        if (!errors::IsUnavailable(s) && !errors::IsCancelled(s) &&
            !errors::IsAborted(s)) {
          return s;
        }
        {
          mutex_lock l(mu_);
          if (cancelled_) {
            return errors::Cancelled("DataServiceDataset iterator cancelled");
          }
        }
        int64_t now_micros = Env::Default()->NowMicros();
        if (now_micros > deadline_micros) {
          return s;
        }
        if (StrictRoundRobin() && num_retries > 0) {
          TF_RETURN_IF_ERROR(MaybeRemoveTask(*task, deadline_micros, result));
          mutex_lock l(mu_);
          if (result.skip) {
            return Status::OK();
          }
        }
        int64_t backoff_until = std::min(
            deadline_micros,
            now_micros + ::tensorflow::ComputeBackoffMicroseconds(num_retries));
        VLOG(1) << "Failed to get an element from worker "
                << task->info.worker_address() << ": " << s
                << ". Will retry in " << (backoff_until - now_micros)
                << " microseconds";
        Env::Default()->SleepForMicroseconds(backoff_until - now_micros);
      }
      ProcessGetElementResponse(enqueue_result, get_element_result, result,
                                *task);
      return Status::OK();
    }

    // Reports whether we can request another element without violating
    // max_outstanding_requests.
    bool ElementSpaceAvailable() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      // When doing round-robin reads, outstanding requests pre-allocate a
      // result in `results_`, so we only need to check the size of `results_`.
      if (StrictRoundRobin()) {
        return results_.size() < max_outstanding_requests_;
      }
      // Otherwise, results aren't added to `results_` until the data has been
      // successfully retrieved. We need to count requests already added to
      // `results_` as well as in-progress requests.
      return results_.size() + outstanding_requests_ <
             max_outstanding_requests_;
    }

    bool StrictRoundRobin() const {
      return dataset()->num_consumers_.has_value();
    }

    const int64_t iterator_index_;

    mutable mutex mu_;
    condition_variable get_next_cv_ TF_GUARDED_BY(mu_);
    condition_variable worker_thread_cv_ TF_GUARDED_BY(mu_);
    condition_variable manager_thread_cv_ TF_GUARDED_BY(mu_);
    bool cancelled_ TF_GUARDED_BY(mu_) = false;
    // Method for deregistering the cancellation callback.
    std::function<void()> deregister_fn_;

    int64_t outstanding_requests_ TF_GUARDED_BY(mu_) = 0;
    // max_outstanding_requests controls how many elements may be held in memory
    // at the same time. This count includes both in-progress requests for
    // elements as well as completed requests which haven't yet been produced.
    int64_t max_outstanding_requests_ TF_GUARDED_BY(mu_);

    // The number of threads in `worker_threads_` which are still running.
    int64_t num_running_worker_threads_ TF_GUARDED_BY(mu_) = 0;

    // The index of the next task in `tasks_` to read from.
    int64_t next_task_index_ TF_GUARDED_BY(mu_) = 0;

    // The number tasks in the `tasks_` list that have reached end_of_sequence.
    int64_t finished_tasks_ TF_GUARDED_BY(mu_) = 0;

    // List of tasks to read from.
    std::vector<std::shared_ptr<Task>> tasks_ TF_GUARDED_BY(mu_);

    // The current round robin round we are engaged in. A round involves reading
    // from each task once.
    int64_t current_round_ TF_GUARDED_BY(mu_) = 0;

    // Maximum round robin round to read up to before blocking, not inclusive.
    // INVARIANT: current_round_ <= round_robin_round_limit_.
    //            If current_round_ == round_robin_round_limit_,
    //            next_task_index_ must be 0.
    absl::optional<int64_t> round_robin_round_limit_ TF_GUARDED_BY(mu_);

    // A status to be returned from the next call to `GetNext`. This is set by
    // asynchronous threads when they encounter errors.
    Status status_ TF_GUARDED_BY(mu_) = Status::OK();
    // A queue of results for `GetElement` requests to read from. When doing
    // strict round robin reads, the queue will contain placeholder results with
    // their `Result::ready` field false until their data has been retrieved
    // from a worker. When not doing round-robin reads, results are only added
    // to the queue after they are ready, to avoid head-of-line blocking.
    std::queue<Result> results_ TF_GUARDED_BY(mu_);

    bool initialized_ = false;
    // Set once in Initialize().
    int64_t job_client_id_;
    std::unique_ptr<DataServiceDispatcherClient> dispatcher_;
    int64_t get_next_index_ TF_GUARDED_BY(mu_) = 0;

    bool job_finished_ = false;
    std::vector<std::unique_ptr<Thread>> worker_threads_ TF_GUARDED_BY(mu_);
    std::unique_ptr<Thread> task_thread_manager_ TF_GUARDED_BY(mu_);
  };

  const int op_version_;
  const int64_t dataset_id_;
  const ProcessingModeDef processing_mode_;
  const tstring address_;
  const tstring protocol_;
  const tstring data_transfer_protocol_;
  const tstring job_name_;
  const absl::optional<int64_t> consumer_index_;
  const absl::optional<int64_t> num_consumers_;
  const int64_t max_outstanding_requests_;
  const int64_t task_refresh_interval_ms_;
  const TargetWorkers target_workers_;
  IterationCounter* const iteration_counter_;  // Owned
  const bool owns_resource_;
  const ResourceHandle iteration_counter_handle_;
  ResourceMgr* const resource_mgr_;  // Not owned
  const DataTypeVector output_types_;
  const std::vector<PartialTensorShape> output_shapes_;
};

DataServiceDatasetOp::DataServiceDatasetOp(OpKernelConstruction* ctx)
    : DatasetOpKernel(ctx) {
  OP_REQUIRES_OK(ctx, ctx->GetAttr(kTaskRefreshIntervalHintMs,
                                   &task_refresh_interval_hint_ms_));
  if (task_refresh_interval_hint_ms_ == model::kAutotune) {
    task_refresh_interval_hint_ms_ = kDefaultTaskRefreshIntervalMs;
  }
  OP_REQUIRES_OK(ctx, ctx->GetAttr(kOutputTypes, &output_types_));
  OP_REQUIRES_OK(ctx, ctx->GetAttr(kOutputShapes, &output_shapes_));
  if (ctx->HasAttr(kDataTransferProtocol)) {
    OP_REQUIRES_OK(
        ctx, ctx->GetAttr(kDataTransferProtocol, &data_transfer_protocol_));
  }
  if (data_transfer_protocol_.empty()) {
    data_transfer_protocol_ = kGrpcTransferProtocol;
  }

  std::string target_workers_str = "AUTO";
  if (ctx->HasAttr(kTargetWorkers)) {
    OP_REQUIRES_OK(ctx, ctx->GetAttr(kTargetWorkers, &target_workers_str));
  }
  StatusOr<TargetWorkers> status_or_target_workers =
      ParseTargetWorkers(target_workers_str);
  OP_REQUIRES_OK(ctx, status_or_target_workers.status());
  target_workers_ = *status_or_target_workers;

  auto& op_name = ctx->def().op();
  if (op_name == kDataServiceDatasetV1) {
    op_version_ = 1;
  } else if (op_name == kDataServiceDatasetV2) {
    op_version_ = 2;
  } else {
    ctx->CtxFailure(errors::FailedPrecondition(
        "Unrecognized data service dataset op name: ", op_name));
    return;
  }
}

void DataServiceDatasetOp::MakeDataset(OpKernelContext* ctx,
                                       DatasetBase** output) {
  int64_t dataset_id;
  OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kDatasetId, &dataset_id));

  tstring processing_mode_str;
  OP_REQUIRES_OK(
      ctx, ParseScalarArgument(ctx, kProcessingMode, &processing_mode_str));
  ProcessingModeDef processing_mode;
  if (processing_mode_str == kParallelEpochs) {
    processing_mode.set_sharding_policy(ProcessingModeDef::OFF);
  } else if (processing_mode_str == kDistributedEpoch) {
    processing_mode.set_sharding_policy(ProcessingModeDef::DYNAMIC);
  } else {
    OP_REQUIRES(ctx, processing_mode.ParseFromString(processing_mode_str),
                errors::InvalidArgument(absl::Substitute(
                    "Failed to parse ProcessingModeDef from string: $0",
                    std::string(processing_mode_str))));
  }
  if (IsStaticShard(processing_mode) &&
      target_workers_ == TARGET_WORKERS_AUTO) {
    VLOG(1) << "Using LOCAL target workers for static sharding mode: "
            << processing_mode.ShortDebugString();
    target_workers_ = TARGET_WORKERS_LOCAL;
  }
  if (target_workers_ == TARGET_WORKERS_LOCAL) {
    data_transfer_protocol_ = kLocalTransferProtocol;
  }

  tstring address;
  OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kAddress, &address));
  OP_REQUIRES(ctx, !address.empty(),
              errors::InvalidArgument(kAddress, " must be non-empty."));

  tstring protocol;
  OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kProtocol, &protocol));
  OP_REQUIRES(ctx, !protocol.empty(),
              errors::InvalidArgument(kProtocol, " must be non-empty."));

  tstring job_name;
  OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kJobName, &job_name));

  absl::optional<int64_t> consumer_index;
  absl::optional<int64_t> num_consumers;
  if (op_version_ >= 2) {
    int64_t consumer_index_int;
    OP_REQUIRES_OK(
        ctx, ParseScalarArgument(ctx, kConsumerIndex, &consumer_index_int));
    if (consumer_index_int >= 0) {
      consumer_index = consumer_index_int;
    }

    int64_t num_consumers_int;
    OP_REQUIRES_OK(ctx,
                   ParseScalarArgument(ctx, kNumConsumers, &num_consumers_int));
    if (num_consumers_int >= 0) {
      num_consumers = num_consumers_int;
    }
  }

  int64_t max_outstanding_requests;
  OP_REQUIRES_OK(ctx, ParseScalarArgument(ctx, kMaxOutstandingRequests,
                                          &max_outstanding_requests));

  ResourceHandle iteration_counter_handle;
  OP_REQUIRES_OK(
      ctx, HandleFromInput(ctx, kIterationCounter, &iteration_counter_handle));
  IterationCounter* iteration_counter = nullptr;
  Status s = ctx->resource_manager()->Lookup<IterationCounter>(
      iteration_counter_handle.container(), iteration_counter_handle.name(),
      &iteration_counter);
  bool owns_resource = false;
  if (errors::IsNotFound(s)) {
    owns_resource = true;
    static std::atomic<int64_t> resource_id_counter(0);
    const std::string& container = ctx->resource_manager()->default_container();
    std::string name =
        strings::StrCat(ctx->op_kernel().name(), "/", kIterationCounter, "_",
                        resource_id_counter.fetch_add(1));
    OP_REQUIRES_OK(ctx,
                   ctx->resource_manager()->LookupOrCreate<IterationCounter>(
                       container, name, &iteration_counter,
                       [](IterationCounter** counter) {
                         *counter = new IterationCounter();
                         return Status::OK();
                       }));
    iteration_counter_handle =
        MakeResourceHandle<IterationCounter>(ctx, container, name);
  } else {
    OP_REQUIRES_OK(ctx, s);
  }

  OP_REQUIRES(
      ctx,
      max_outstanding_requests == model::kAutotune ||
          max_outstanding_requests > 0,
      errors::InvalidArgument(kMaxOutstandingRequests, " must be positive or ",
                              model::kAutotune));

  *output = new Dataset(
      ctx, op_version_, dataset_id, processing_mode, address, protocol,
      data_transfer_protocol_, job_name, consumer_index, num_consumers,
      max_outstanding_requests, task_refresh_interval_hint_ms_, target_workers_,
      iteration_counter, owns_resource, iteration_counter_handle, output_types_,
      output_shapes_);
}

REGISTER_KERNEL_BUILDER(Name("DataServiceDataset").Device(DEVICE_CPU),
                        DataServiceDatasetOp);
REGISTER_KERNEL_BUILDER(Name("DataServiceDatasetV2").Device(DEVICE_CPU),
                        DataServiceDatasetOp);
REGISTER_KERNEL_BUILDER(Name("DummyIterationCounter").Device(DEVICE_CPU),
                        DummyResourceOp<IterationCounter>);

}  // namespace data
}  // namespace tensorflow
