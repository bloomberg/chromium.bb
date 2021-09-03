/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/data/service/server_lib.h"

#include "tensorflow/core/data/service/credentials_factory.h"
#include "tensorflow/core/data/service/grpc_dispatcher_impl.h"
#include "tensorflow/core/data/service/grpc_util.h"
#include "tensorflow/core/data/service/grpc_worker_impl.h"
#include "tensorflow/core/platform/errors.h"

namespace tensorflow {
namespace data {

namespace {
constexpr char kPortPlaceholder[] = "%port%";
}

GrpcDataServerBase::GrpcDataServerBase(int port, const std::string& protocol)
    : requested_port_(port), protocol_(protocol) {}

Status GrpcDataServerBase::Start() {
  if (stopped_) {
    return errors::FailedPrecondition(
        "Server cannot be started after it has been stopped.");
  }
  if (started_) {
    return Status::OK();
  }
  ::grpc::ServerBuilder builder;
  std::shared_ptr<::grpc::ServerCredentials> credentials;
  TF_RETURN_IF_ERROR(
      CredentialsFactory::CreateServerCredentials(protocol_, &credentials));
  builder.AddListeningPort(strings::StrCat("0.0.0.0:", requested_port_),
                           credentials, &bound_port_);
  builder.SetMaxReceiveMessageSize(-1);

  AddServiceToBuilder(&builder);
  server_ = builder.BuildAndStart();
  if (!server_) {
    return errors::Internal("Could not start gRPC server");
  }

  TF_RETURN_IF_ERROR(StartServiceInternal());

  started_ = true;
  VLOG(1) << "Started tf.data service running at 0.0.0.0:" << BoundPort();
  return Status::OK();
}

void GrpcDataServerBase::Stop() {
  if (stopped_) {
    return;
  }
  server_->Shutdown();
  stopped_ = true;
}

void GrpcDataServerBase::Join() { server_->Wait(); }

int GrpcDataServerBase::BoundPort() { return bound_port(); }

DispatchGrpcDataServer::DispatchGrpcDataServer(int port,
                                               const std::string& protocol)
    : GrpcDataServerBase(port, protocol) {}

DispatchGrpcDataServer::~DispatchGrpcDataServer() { delete service_; }

void DispatchGrpcDataServer::AddServiceToBuilder(grpc::ServerBuilder* builder) {
  auto service = absl::make_unique<GrpcDispatcherImpl>(builder, protocol_);
  service_ = service.release();
}

Status DispatchGrpcDataServer::NumWorkers(int* num_workers) {
  GetWorkersRequest req;
  GetWorkersResponse resp;
  grpc::ServerContext ctx;
  grpc::Status s = service_->GetWorkers(&ctx, &req, &resp);
  if (!s.ok()) {
    return grpc_util::WrapError("Failed to get workers", s);
  }
  *num_workers = resp.workers_size();
  return Status::OK();
}

WorkerGrpcDataServer::WorkerGrpcDataServer(
    int port, const std::string& protocol,
    const std::string& dispatcher_address, const std::string& worker_address)
    : GrpcDataServerBase(port, protocol),
      dispatcher_address_(dispatcher_address),
      worker_address_(worker_address) {}

WorkerGrpcDataServer::~WorkerGrpcDataServer() { delete service_; }

void WorkerGrpcDataServer::AddServiceToBuilder(grpc::ServerBuilder* builder) {
  auto service = absl::make_unique<GrpcWorkerImpl>(builder, dispatcher_address_,
                                                   protocol_);
  service_ = service.release();
}

Status WorkerGrpcDataServer::StartServiceInternal() {
  std::string worker_address = worker_address_;
  if (worker_address.empty()) {
    worker_address = absl::StrCat("localhost:", kPortPlaceholder);
  }
  std::string resolved_address = str_util::StringReplace(
      worker_address, kPortPlaceholder, absl::StrCat(bound_port()),
      /*replace_all=*/false);
  service_->Start(resolved_address);
  return Status::OK();
}

Status NewDispatchServer(int port, const std::string& protocol,
                         std::unique_ptr<DispatchGrpcDataServer>* out_server) {
  *out_server = absl::make_unique<DispatchGrpcDataServer>(port, protocol);
  return Status::OK();
}

Status NewWorkerServer(int port, const std::string& protocol,
                       const std::string& dispatcher_address,
                       std::unique_ptr<WorkerGrpcDataServer>* out_server) {
  return NewWorkerServer(port, protocol, dispatcher_address,
                         /*worker_address=*/"", out_server);
}

Status NewWorkerServer(int port, const std::string& protocol,
                       const std::string& dispatcher_address,
                       const std::string& worker_address,
                       std::unique_ptr<WorkerGrpcDataServer>* out_server) {
  *out_server = absl::make_unique<WorkerGrpcDataServer>(
      port, protocol, dispatcher_address, worker_address);
  return Status::OK();
}

}  // namespace data
}  // namespace tensorflow
