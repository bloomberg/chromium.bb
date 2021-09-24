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

#include "tensorflow/core/data/service/test_cluster.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "tensorflow/core/data/service/server_lib.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/protobuf/service_config.pb.h"

namespace tensorflow {
namespace data {
namespace {
constexpr const char kProtocol[] = "grpc";
}  // namespace

TestCluster::TestCluster(int num_workers) : num_workers_(num_workers) {}

Status TestCluster::Initialize() {
  if (initialized_) {
    return errors::FailedPrecondition(
        "Test cluster has already been initialized.");
  }
  initialized_ = true;
  experimental::DispatcherConfig config;
  config.set_protocol(kProtocol);
  for (int i = 0; i < num_workers_; ++i) {
    config.add_worker_addresses("localhost");
  }
  TF_RETURN_IF_ERROR(NewDispatchServer(config, dispatcher_));
  TF_RETURN_IF_ERROR(dispatcher_->Start());
  dispatcher_address_ = absl::StrCat("localhost:", dispatcher_->BoundPort());
  workers_.reserve(num_workers_);
  worker_addresses_.reserve(num_workers_);
  for (int i = 0; i < num_workers_; ++i) {
    TF_RETURN_IF_ERROR(AddWorker());
  }
  return Status::OK();
}

Status TestCluster::AddWorker() {
  std::unique_ptr<WorkerGrpcDataServer> worker;
  experimental::WorkerConfig config;
  config.set_protocol(kProtocol);
  config.set_dispatcher_address(dispatcher_address_);
  config.set_worker_address("localhost:%port%");
  TF_RETURN_IF_ERROR(NewWorkerServer(config, worker));
  TF_RETURN_IF_ERROR(worker->Start());
  worker_addresses_.push_back(absl::StrCat("localhost:", worker->BoundPort()));
  workers_.push_back(std::move(worker));
  return Status::OK();
}

std::string TestCluster::DispatcherAddress() const {
  return dispatcher_address_;
}

std::string TestCluster::WorkerAddress(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, worker_addresses_.size());
  return worker_addresses_[index];
}

void TestCluster::StopWorker(size_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, worker_addresses_.size());
  workers_[index]->Stop();
}

void TestCluster::StopWorkers() {
  for (std::unique_ptr<WorkerGrpcDataServer>& worker : workers_) {
    worker->Stop();
  }
}

}  // namespace data
}  // namespace tensorflow
