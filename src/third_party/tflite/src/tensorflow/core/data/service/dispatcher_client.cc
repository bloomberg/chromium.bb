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
#include "tensorflow/core/data/service/dispatcher_client.h"

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/channel_arguments.h"
#include "grpcpp/support/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "tensorflow/core/data/service/common.h"
#include "tensorflow/core/data/service/common.pb.h"
#include "tensorflow/core/data/service/credentials_factory.h"
#include "tensorflow/core/data/service/dispatcher.grpc.pb.h"
#include "tensorflow/core/data/service/dispatcher.pb.h"
#include "tensorflow/core/data/service/grpc_util.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/protobuf/data_service.pb.h"

namespace tensorflow {
namespace data {

Status DataServiceDispatcherClient::WorkerHeartbeat(
    const std::string& worker_address, const std::string& transfer_address,
    const std::vector<int64_t>& current_tasks, std::vector<TaskDef>& new_tasks,
    std::vector<int64_t>& tasks_to_delete) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  WorkerHeartbeatRequest req;
  req.set_worker_address(worker_address);
  req.set_transfer_address(transfer_address);
  for (int64_t task : current_tasks) {
    req.add_current_tasks(task);
  }
  WorkerHeartbeatResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->WorkerHeartbeat(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError("Failed to perform worker heartbeat", status);
  }
  for (const auto& task : resp.new_tasks()) {
    new_tasks.push_back(task);
  }
  for (int64_t task_to_delete : resp.tasks_to_delete()) {
    tasks_to_delete.push_back(task_to_delete);
  }
  return Status::OK();
}

Status DataServiceDispatcherClient::WorkerUpdate(
    const std::string& worker_address,
    std::vector<TaskProgress>& task_progress) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  WorkerUpdateRequest req;
  req.set_worker_address(worker_address);
  for (const auto& update : task_progress) {
    *(req.add_updates()) = update;
  }
  WorkerUpdateResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->WorkerUpdate(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError("Failed to send worker update", status);
  }
  return Status::OK();
}

Status DataServiceDispatcherClient::GetDatasetDef(int64_t dataset_id,
                                                  DatasetDef& dataset_def) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  GetDatasetDefRequest req;
  req.set_dataset_id(dataset_id);
  GetDatasetDefResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->GetDatasetDef(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError("Failed to get dataset def", status);
  }
  dataset_def = resp.dataset_def();
  return Status::OK();
}

Status DataServiceDispatcherClient::GetSplit(int64_t job_id, int64_t repetition,
                                             int64_t split_provider_index,
                                             Tensor& split,
                                             bool& end_of_splits) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  GetSplitRequest req;
  req.set_job_id(job_id);
  req.set_repetition(repetition);
  req.set_split_provider_index(split_provider_index);
  GetSplitResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->GetSplit(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError("Failed to get split", status);
  }
  end_of_splits = resp.end_of_splits();
  if (!end_of_splits) {
    if (!split.FromProto(resp.split())) {
      return errors::Internal("Failed to parse split tensor proto");
    }
  }
  return Status::OK();
}

Status DataServiceDispatcherClient::RegisterDataset(
    const DatasetDef& dataset, const absl::optional<std::string>& element_spec,
    int64_t& dataset_id) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  GetOrRegisterDatasetRequest req;
  *req.mutable_dataset() = dataset;
  if (element_spec.has_value()) {
    req.set_element_spec(element_spec.value());
  }

  GetOrRegisterDatasetResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->GetOrRegisterDataset(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError("Failed to register dataset", status);
  }
  dataset_id = resp.dataset_id();
  return Status::OK();
}

Status DataServiceDispatcherClient::GetOrCreateJob(
    int64_t dataset_id, const ProcessingModeDef& processing_mode,
    const absl::optional<JobKey>& job_key, absl::optional<int64> num_consumers,
    TargetWorkers target_workers, int64_t& job_client_id) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  GetOrCreateJobRequest req;
  req.set_dataset_id(dataset_id);
  *req.mutable_processing_mode_def() = processing_mode;
  if (job_key.has_value()) {
    *req.mutable_job_key() = job_key.value();
  }
  if (num_consumers.has_value()) {
    req.set_num_consumers(num_consumers.value());
  }
  req.set_target_workers(target_workers);
  GetOrCreateJobResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->GetOrCreateJob(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError(
        absl::StrCat("Failed to get or create job for dataset with id ",
                     dataset_id),
        status);
  }
  job_client_id = resp.job_client_id();
  return Status::OK();
}

Status DataServiceDispatcherClient::ReleaseJobClient(int64_t job_client_id) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  ReleaseJobClientRequest req;
  req.set_job_client_id(job_client_id);
  ReleaseJobClientResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->ReleaseJobClient(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError(
        absl::StrCat("Failed to release job client with id ", job_client_id),
        status);
  }
  return Status::OK();
}

Status DataServiceDispatcherClient::MaybeRemoveTask(int64_t task_id,
                                                    int64_t consumer_index,
                                                    int64_t round,
                                                    bool& removed) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  MaybeRemoveTaskRequest req;
  req.set_task_id(task_id);
  req.set_consumer_index(consumer_index);
  req.set_round(round);
  MaybeRemoveTaskResponse resp;
  grpc::ClientContext client_ctx;
  grpc::Status status = stub_->MaybeRemoveTask(&client_ctx, req, &resp);
  if (!status.ok()) {
    return grpc_util::WrapError("Failed to call MaybeRemoveTask", status);
  }
  removed = resp.removed();
  return Status::OK();
}

Status DataServiceDispatcherClient::ClientHeartbeat(
    ClientHeartbeatRequest& req, ClientHeartbeatResponse& resp) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  grpc::ClientContext ctx;
  grpc::Status s = stub_->ClientHeartbeat(&ctx, req, &resp);
  if (!s.ok()) {
    return grpc_util::WrapError("Failed to get tasks", s);
  }
  return Status::OK();
}

Status DataServiceDispatcherClient::GetWorkers(
    std::vector<WorkerInfo>& workers) {
  TF_RETURN_IF_ERROR(EnsureInitialized());
  GetWorkersRequest req;
  GetWorkersResponse resp;
  grpc::ClientContext ctx;
  grpc::Status s = stub_->GetWorkers(&ctx, req, &resp);
  if (!s.ok()) {
    return grpc_util::WrapError("Failed to get workers", s);
  }
  workers.clear();
  for (auto& worker : resp.workers()) {
    workers.push_back(worker);
  }
  return Status::OK();
}

Status DataServiceDispatcherClient::GetElementSpec(int64_t dataset_id,
                                                   std::string& element_spec) {
  TF_RETURN_IF_ERROR(EnsureInitialized());

  GetElementSpecRequest req;
  req.set_dataset_id(dataset_id);
  GetElementSpecResponse resp;
  grpc::ClientContext ctx;
  grpc::Status s = stub_->GetElementSpec(&ctx, req, &resp);
  if (!s.ok()) {
    return grpc_util::WrapError("Failed to get element_spec", s);
  }
  element_spec = resp.element_spec();
  return Status::OK();
}

Status DataServiceDispatcherClient::EnsureInitialized() {
  mutex_lock l(mu_);
  if (stub_) {
    return Status::OK();
  }
  std::shared_ptr<grpc::ChannelCredentials> credentials;
  TF_RETURN_IF_ERROR(
      CredentialsFactory::CreateClientCredentials(protocol_, &credentials));
  grpc::ChannelArguments args;
  args.SetMaxReceiveMessageSize(std::numeric_limits<int32>::max());
  args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, true);
  auto channel = grpc::CreateCustomChannel(address_, credentials, args);
  stub_ = DispatcherService::NewStub(channel);
  GetVersionRequest req;
  GetVersionResponse resp;
  TF_RETURN_IF_ERROR(grpc_util::Retry(
      [&] {
        grpc::ClientContext ctx;
        grpc::Status s = stub_->GetVersion(&ctx, req, &resp);
        if (!s.ok()) {
          return grpc_util::WrapError(
              absl::StrCat("Failed to get dispatcher version from dispatcher "
                           "running at ",
                           address_),
              s);
        }
        return Status::OK();
      },
      "check service version",
      /*deadline_micros=*/kint64max));
  if (resp.version() != kDataServiceVersion) {
    return errors::FailedPrecondition(
        "Version mismatch with tf.data service server. The server is running "
        "version ",
        resp.version(), ", while the client is running version ",
        kDataServiceVersion,
        ". Please ensure that the client and server side are running the "
        "same version of TensorFlow.");
  }
  return Status::OK();
}

}  // namespace data
}  // namespace tensorflow
