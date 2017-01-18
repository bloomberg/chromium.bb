// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_CONNECT_PARAMS_H_
#define SERVICES_SERVICE_MANAGER_CONNECT_PARAMS_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace service_manager {

// This class represents a request for the application manager to connect to an
// application.
class ConnectParams {
 public:
  ConnectParams();
  ~ConnectParams();

  void set_source(const Identity& source) { source_ = source;  }
  const Identity& source() const { return source_; }
  void set_target(const Identity& target) { target_ = target; }
  const Identity& target() const { return target_; }

  void set_remote_interfaces(mojom::InterfaceProviderRequest value) {
    remote_interfaces_ = std::move(value);
  }
  mojom::InterfaceProviderRequest TakeRemoteInterfaces() {
    return std::move(remote_interfaces_);
  }

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

  void set_connect_callback(const mojom::Connector::ConnectCallback& value) {
    connect_callback_ = value;
  }
  const mojom::Connector::ConnectCallback& connect_callback() const {
    return connect_callback_;
  }

  void set_bind_interface_callback(
      const mojom::Connector::BindInterfaceCallback& callback) {
    bind_interface_callback_ = callback;
  }
  const mojom::Connector::BindInterfaceCallback&
      bind_interface_callback() const {
    return bind_interface_callback_;
  }

 private:
  // It may be null (i.e., is_null() returns true) which indicates that there is
  // no source (e.g., for the first application or in tests).
  Identity source_;
  // The identity of the application being connected to.
  Identity target_;

  mojom::InterfaceProviderRequest remote_interfaces_;
  mojom::ServicePtr service_;
  mojom::PIDReceiverRequest pid_receiver_request_;
  std::string interface_name_;
  mojo::ScopedMessagePipeHandle interface_pipe_;
  mojom::Connector::ConnectCallback connect_callback_;
  mojom::Connector::BindInterfaceCallback bind_interface_callback_;

  DISALLOW_COPY_AND_ASSIGN(ConnectParams);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_CONNECT_PARAMS_H_
