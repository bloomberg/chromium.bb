// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_CONNECT_PARAMS_H_
#define SERVICES_SERVICE_MANAGER_CONNECT_PARAMS_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service_filter.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

// This class represents a request for the application manager to connect to an
// application.
//
// TODO(https://crbug.com/904240): This should really just be an internal type
// within ServiceManager. At this point its only real value is in providing a
// default response to a |BindInterface()| or |WarmService()| request when the
// ServiceManager otherwise drops the request without responding. This is of
// questionable value.
class ConnectParams {
 public:
  using ConnectionCallback =
      base::OnceCallback<void(mojom::ConnectResult result,
                              const base::Optional<Identity>& identity)>;

  ConnectParams();
  ~ConnectParams();

  void set_source(const Identity& source) { source_ = source;  }
  const Identity& source() const { return source_; }

  void set_target(const ServiceFilter& target) { target_ = target; }
  const ServiceFilter& target() const { return target_; }

  void set_client_process_info(
      mojom::ServicePtr service,
      mojom::PIDReceiverRequest pid_receiver_request) {
    service_ = std::move(service);
    pid_receiver_request_ = std::move(pid_receiver_request);
  }
  bool HasClientProcessInfo() const {
    return service_.is_bound() && pid_receiver_request_.is_pending();
  }
  mojom::ServicePtr TakeService() {
    return std::move(service_);
  }
  mojom::PIDReceiverRequest TakePIDReceiverRequest() {
    return std::move(pid_receiver_request_);
  }

  void set_interface_request_info(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) {
    interface_name_ = interface_name;
    interface_pipe_ = std::move(interface_pipe);
  }
  const std::string& interface_name() const {
    return interface_name_;
  }
  bool HasInterfaceRequestInfo() const {
    return !interface_name_.empty() && interface_pipe_.is_valid();
  }
  mojo::ScopedMessagePipeHandle TakeInterfaceRequestPipe() {
    return std::move(interface_pipe_);
  }

  void set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
  }

  ConnectionCallback TakeConnectionCallback() {
    return std::move(connection_callback_);
  }

  void set_response_data(mojom::ConnectResult result,
                         const base::Optional<Identity>& resolved_identity) {
    result_ = result;
    resolved_identity_ = resolved_identity;
  }

 private:
  // The identity of the instance making this connection request.
  Identity source_;

  // A filter defining the search criteria for a running target instance to
  // which the source would like to connect.
  ServiceFilter target_;

  mojom::ServicePtr service_;
  mojom::PIDReceiverRequest pid_receiver_request_;
  std::string interface_name_;
  mojo::ScopedMessagePipeHandle interface_pipe_;
  ConnectionCallback connection_callback_;

  // These values are supplied to the response callback for WarmService()/
  // BindInterface() etc. when the connection is completed.
  mojom::ConnectResult result_ = mojom::ConnectResult::INVALID_ARGUMENT;
  base::Optional<Identity> resolved_identity_;

  DISALLOW_COPY_AND_ASSIGN(ConnectParams);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_CONNECT_PARAMS_H_
