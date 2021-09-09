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

#ifndef TENSORFLOW_CORE_DATA_SERVICE_DISPATCHER_IMPL_H_
#define TENSORFLOW_CORE_DATA_SERVICE_DISPATCHER_IMPL_H_

#include "absl/container/flat_hash_map.h"
#include "tensorflow/core/data/service/common.pb.h"
#include "tensorflow/core/data/service/data_service.h"
#include "tensorflow/core/data/service/dispatcher.pb.h"
#include "tensorflow/core/data/service/worker.grpc.pb.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/public/session.h"

namespace tensorflow {
namespace data {

// A service which coordinates a pool of workers to serve dataset elements over
// RPC.
//
// Glossary:
// * Dataset: A definition of how to generate a potentially large collection of
//   elements.
// * Job: A coordinated phase of reading from the tf.data service. A job
//   produces some amount of data, and (potentially multiple) consumers consume
//   the data from the job until there is no data left. Each job has a
//   ProcessingModeDef which determines what data it produces.
// * Task: A job is broken into multiple tasks, which each represent
//   iterating over all of or part of the dataset. Workers process tasks.
class DataServiceDispatcherImpl {
 public:
  explicit DataServiceDispatcherImpl(const std::string protocol);

  // See dispatcher.proto for API documentation.

  /// Worker-facing API.
  Status RegisterWorker(const RegisterWorkerRequest* request,
                        RegisterWorkerResponse* response);
  Status WorkerUpdate(const WorkerUpdateRequest* request,
                      WorkerUpdateResponse* response);

  /// Client-facing API.
  Status GetOrRegisterDataset(const GetOrRegisterDatasetRequest* request,
                              GetOrRegisterDatasetResponse* response);
  Status CreateJob(const CreateJobRequest* request,
                   CreateJobResponse* response);
  Status GetOrCreateJob(const GetOrCreateJobRequest* request,
                        GetOrCreateJobResponse* response);
  Status GetTasks(const GetTasksRequest* request, GetTasksResponse* response);
  Status GetWorkers(const GetWorkersRequest* request,
                    GetWorkersResponse* response);

 private:
  class Worker {
   public:
    Worker(int64 worker_id, const std::string address)
        : worker_id_(worker_id), address_(address) {}

    int64 worker_id() { return worker_id_; }
    std::string address() { return address_; }
    WorkerService::Stub* stub() { return stub_.get(); }
    void set_stub(std::unique_ptr<WorkerService::Stub> stub) {
      stub_ = std::move(stub);
    }

    std::string DebugString() {
      return absl::StrCat("id: ", worker_id_, " address: ", address_);
    }

   private:
    const int64 worker_id_;
    const std::string address_;
    std::unique_ptr<WorkerService::Stub> stub_;
  };

  class Dataset {
   public:
    Dataset(int64 dataset_id, int64 fingerprint, const DatasetDef& dataset_def)
        : dataset_id_(dataset_id),
          fingerprint_(fingerprint),
          dataset_def_(dataset_def) {}

    int64 dataset_id() const { return dataset_id_; }
    int64 fingerprint() const { return fingerprint_; }
    const DatasetDef& dataset_def() { return dataset_def_; }

   private:
    const int64 dataset_id_;
    const int64 fingerprint_;
    const DatasetDef dataset_def_;
  };

  class Job {
   public:
    Job(int64 job_id, int64 dataset_id, ProcessingMode processing_mode,
        absl::optional<absl::string_view> job_name)
        : job_id_(job_id),
          dataset_id_(dataset_id),
          processing_mode_(processing_mode),
          job_name_(job_name) {}

    int64 job_id() const { return job_id_; }
    int64 dataset_id() const { return dataset_id_; }
    ProcessingMode processing_mode() const { return processing_mode_; }
    absl::optional<std::string> name() const { return job_name_; }
    const std::vector<int64>& task_ids() const { return task_ids_; }
    void add_task_id(int64 task_id) { task_ids_.push_back(task_id); }
    void task_finished(int64 task_id) {
      finished_tasks_.push_back(task_id);
      if (finished_tasks_.size() == task_ids_.size()) {
        finished_ = true;
      }
    }
    bool finished() const { return finished_; }

   private:
    const int64 job_id_;
    const int64 dataset_id_;
    const ProcessingMode processing_mode_;
    const absl::optional<std::string> job_name_;
    std::vector<int64> task_ids_;
    std::vector<int64> finished_tasks_;
    bool finished_ = false;
  };

  class NamedJobKey {
   public:
    NamedJobKey(absl::string_view name, int64 index)
        : name_(name), index_(index) {}

    friend bool operator==(const NamedJobKey& lhs, const NamedJobKey& rhs) {
      return lhs.name_ == rhs.name_ && lhs.index_ == rhs.index_;
    }

    template <typename H>
    friend H AbslHashValue(H h, const NamedJobKey& k) {
      return H::combine(std::move(h), k.name_, k.index_);
    }

   private:
    const std::string name_;
    const int64 index_;
  };

  class Task {
   public:
    Task(int64 task_id, int64 job_id, int64 dataset_id,
         const std::string& worker_address)
        : task_id_(task_id),
          job_id_(job_id),
          dataset_id_(dataset_id),
          worker_address_(worker_address) {}

    int64 task_id() const { return task_id_; }
    int64 job_id() const { return job_id_; }
    int64 dataset_id() const { return dataset_id_; }
    std::string worker_address() const { return worker_address_; }

   private:
    const int64 task_id_;
    const int64 job_id_;
    const int64 dataset_id_;
    const std::string worker_address_;
  };

  // Registers a dataset with the given fingerprint, returning a new dataset id.
  int64 RegisterDataset(uint64 fingerprint, const DatasetDef& dataset)
      EXCLUSIVE_LOCKS_REQUIRED(mu_);
  // Initializes a workers stub, if it hasn't been initialized already.
  Status EnsureWorkerStubInitialized(Worker* worker);
  // Instructs a worker to begin processing a task.
  Status AllocateTaskToWorker(const Task& task_id, Worker* worker)
      LOCKS_EXCLUDED(mu_);
  // Creates a job and stores its job_id in `*job_id`.
  Status CreateJob(int64 dataset_id, ProcessingMode processing_mode,
                   absl::optional<std::string> job_name, int64* out_job_id)
      LOCKS_EXCLUDED(mu_);
  // Creates a new task for a job, returning a reference to the task.
  const Task& CreateTask(Job* job, const std::string& worker_address)
      LOCKS_EXCLUDED(mu_);
  // Same as `CreateTask`, but expects that the dispatcher lock is already held.
  const Task& CreateTaskLocked(Job* job, const std::string& worker_address)
      EXCLUSIVE_LOCKS_REQUIRED(mu_);
  // Validates that an existing job matches the given processing_mode and
  // dataset_id, returning an error status describing any difference.
  Status ValidateMatchingJob(const Job& job, ProcessingMode processing_mode,
                             int64 dataset_id);
  // Protocol to use for communicating with workers.
  const std::string protocol_;

  mutex mu_;

  int64 next_worker_id_ TF_GUARDED_BY(mu_) = 0;
  int64 next_dataset_id_ TF_GUARDED_BY(mu_) = 0;
  int64 next_job_id_ TF_GUARDED_BY(mu_) = 0;
  int64 next_task_id_ TF_GUARDED_BY(mu_) = 0;

  // Registered workers.
  std::vector<std::shared_ptr<Worker>> workers_ TF_GUARDED_BY(mu_);
  // Registered datasets, keyed by dataset ids.
  absl::flat_hash_map<int64, std::shared_ptr<Dataset>> datasets_by_id_
      TF_GUARDED_BY(mu_);
  // Registered datasets, keyed by dataset fingerprints.
  absl::flat_hash_map<uint64, std::shared_ptr<Dataset>> datasets_by_fingerprint_
      TF_GUARDED_BY(mu_);
  // Information about jobs, keyed by job ids.
  absl::flat_hash_map<int64, std::shared_ptr<Job>> jobs_ TF_GUARDED_BY(mu_);
  // Information about tasks, keyed by task ids.
  absl::flat_hash_map<int64, Task> tasks_ TF_GUARDED_BY(mu_);
  // Named jobs, keyed by their names and indices. Not all jobs have names, so
  // this is a subset of the jobs stored in `jobs_`.
  absl::flat_hash_map<NamedJobKey, std::shared_ptr<Job>> named_jobs_
      TF_GUARDED_BY(mu_);

  TF_DISALLOW_COPY_AND_ASSIGN(DataServiceDispatcherImpl);
};

}  // namespace data
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_DATA_SERVICE_DISPATCHER_IMPL_H_
