// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_HTTP_CONNECTION_DELEGATE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_HTTP_CONNECTION_DELEGATE_H_

#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/libassistant/grpc/grpc_http_connection_client.h"

namespace chromeos {
namespace libassistant {

// An interface invoked by GrpcHttpConnectionDelegate to relay the HTTP
// responses to gRPC HttpConnectionService.
class GrpcHttpConnectionDelegate
    : public assistant_client::HttpConnection::Delegate {
 public:
  GrpcHttpConnectionDelegate(int id, GrpcHttpConnectionClient* client);
  GrpcHttpConnectionDelegate(const GrpcHttpConnectionDelegate&) = delete;
  GrpcHttpConnectionDelegate& operator=(const GrpcHttpConnectionDelegate&) =
      delete;
  ~GrpcHttpConnectionDelegate() override;

  // HttpConnection::Delegate:
  void OnHeaderResponse(const std::string& raw_headers) override;
  void OnPartialResponse(const std::string& partial_response) override;
  void OnCompleteResponse(int http_status,
                          const std::string& raw_headers,
                          const std::string& response) override;
  void OnNetworkError(int error_code, const std::string& message) override;
  void OnConnectionDestroyed() override;

 private:
  const int id_;
  GrpcHttpConnectionClient* const grpc_http_connection_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_HTTP_CONNECTION_DELEGATE_H_
