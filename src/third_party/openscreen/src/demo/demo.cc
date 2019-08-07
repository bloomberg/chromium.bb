// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "api/public/mdns_service_listener_factory.h"
#include "api/public/mdns_service_publisher_factory.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/presentation/presentation_controller.h"
#include "api/public/presentation/presentation_receiver.h"
#include "api/public/protocol_connection_client.h"
#include "api/public/protocol_connection_client_factory.h"
#include "api/public/protocol_connection_server.h"
#include "api/public/protocol_connection_server_factory.h"
#include "api/public/service_listener.h"
#include "api/public/service_publisher.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "platform/api/network_interface.h"
#include "third_party/abseil/src/absl/strings/string_view.h"
#include "third_party/tinycbor/src/src/cbor.h"

namespace openscreen {
namespace {

const char* kReceiverLogFilename = "_recv_fifo";
const char* kControllerLogFilename = "_cntl_fifo";

bool g_done = false;
bool g_dump_services = false;

void sigusr1_dump_services(int) {
  g_dump_services = true;
}

void sigint_stop(int) {
  OSP_LOG << "caught SIGINT, exiting...";
  g_done = true;
}

void SignalThings() {
  struct sigaction usr1_sa;
  struct sigaction int_sa;
  struct sigaction unused;

  usr1_sa.sa_handler = &sigusr1_dump_services;
  sigemptyset(&usr1_sa.sa_mask);
  usr1_sa.sa_flags = 0;

  int_sa.sa_handler = &sigint_stop;
  sigemptyset(&int_sa.sa_mask);
  int_sa.sa_flags = 0;

  sigaction(SIGUSR1, &usr1_sa, &unused);
  sigaction(SIGINT, &int_sa, &unused);

  OSP_LOG << "signal handlers setup" << std::endl << "pid: " << getpid();
}

class AutoMessage final
    : public ProtocolConnectionClient::ConnectionRequestCallback {
 public:
  ~AutoMessage() override = default;

  void TakeRequest(ProtocolConnectionClient::ConnectRequest&& request) {
    request_ = std::move(request);
  }

  void OnConnectionOpened(
      uint64_t request_id,
      std::unique_ptr<ProtocolConnection> connection) override {
    request_ = ProtocolConnectionClient::ConnectRequest();
    msgs::CborEncodeBuffer buffer;
    msgs::PresentationConnectionMessage message;
    message.connection_id = 0;
    message.presentation_id = "qD0wuy6EIwQFfIEe9BKl";
    message.message.which = decltype(message.message.which)::kString;
    new (&message.message.str) std::string("message from client");
    if (msgs::EncodePresentationConnectionMessage(message, &buffer))
      connection->Write(buffer.data(), buffer.size());
    connection->CloseWriteEnd();
  }

  void OnConnectionFailed(uint64_t request_id) override {
    request_ = ProtocolConnectionClient::ConnectRequest();
  }

 private:
  ProtocolConnectionClient::ConnectRequest request_;
};

class ListenerObserver final : public ServiceListener::Observer {
 public:
  ~ListenerObserver() override = default;
  void OnStarted() override { OSP_LOG << "listener started!"; }
  void OnStopped() override { OSP_LOG << "listener stopped!"; }
  void OnSuspended() override { OSP_LOG << "listener suspended!"; }
  void OnSearching() override { OSP_LOG << "listener searching!"; }

  void OnReceiverAdded(const ServiceInfo& info) override {
    OSP_LOG << "found! " << info.friendly_name;
    if (!auto_message_) {
      auto_message_ = std::make_unique<AutoMessage>();
      auto_message_->TakeRequest(
          NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
              info.v4_endpoint, auto_message_.get()));
    }
  }
  void OnReceiverChanged(const ServiceInfo& info) override {
    OSP_LOG << "changed! " << info.friendly_name;
  }
  void OnReceiverRemoved(const ServiceInfo& info) override {
    OSP_LOG << "removed! " << info.friendly_name;
  }
  void OnAllReceiversRemoved() override { OSP_LOG << "all removed!"; }
  void OnError(ServiceListenerError) override {}
  void OnMetrics(ServiceListener::Metrics) override {}

 private:
  std::unique_ptr<AutoMessage> auto_message_;
};

class PublisherObserver final : public ServicePublisher::Observer {
 public:
  ~PublisherObserver() override = default;

  void OnStarted() override { OSP_LOG << "publisher started!"; }
  void OnStopped() override { OSP_LOG << "publisher stopped!"; }
  void OnSuspended() override { OSP_LOG << "publisher suspended!"; }

  void OnError(ServicePublisherError) override {}
  void OnMetrics(ServicePublisher::Metrics) override {}
};

class ConnectionClientObserver final
    : public ProtocolConnectionServiceObserver {
 public:
  ~ConnectionClientObserver() override = default;
  void OnRunning() override {}
  void OnStopped() override {}

  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}
};

class ConnectionServerObserver final
    : public ProtocolConnectionServer::Observer {
 public:
  class ConnectionObserver final : public ProtocolConnection::Observer {
   public:
    explicit ConnectionObserver(ConnectionServerObserver* parent)
        : parent_(parent) {}
    ~ConnectionObserver() override = default;

    void OnConnectionClosed(const ProtocolConnection& connection) override {
      auto& connections = parent_->connections_;
      connections.erase(
          std::remove_if(
              connections.begin(), connections.end(),
              [this](const std::pair<std::unique_ptr<ConnectionObserver>,
                                     std::unique_ptr<ProtocolConnection>>& p) {
                return p.first.get() == this;
              }),
          connections.end());
    }

   private:
    ConnectionServerObserver* const parent_;
  };

  ~ConnectionServerObserver() override = default;

  void OnRunning() override {}
  void OnStopped() override {}
  void OnSuspended() override {}

  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}

  void OnIncomingConnection(
      std::unique_ptr<ProtocolConnection> connection) override {
    auto observer = std::make_unique<ConnectionObserver>(this);
    connection->SetObserver(observer.get());
    connections_.emplace_back(std::move(observer), std::move(connection));
    connections_.back().second->CloseWriteEnd();
  }

 private:
  std::vector<std::pair<std::unique_ptr<ConnectionObserver>,
                        std::unique_ptr<ProtocolConnection>>>
      connections_;
};

class ReceiverConnectionDelegate final
    : public presentation::Connection::Delegate {
 public:
  ReceiverConnectionDelegate() = default;
  ~ReceiverConnectionDelegate() override = default;

  void OnConnected() override {
    OSP_LOG << "presentation connection connected";
  }
  void OnClosedByRemote() override {
    OSP_LOG << "presentation connection closed by remote";
  }
  void OnDiscarded() override {}
  void OnError(const absl::string_view message) override {}
  void OnTerminatedByRemote() override { OSP_LOG << "presentation terminated"; }

  void OnStringMessage(const absl::string_view message) override {
    OSP_LOG << "got message: " << message;
    connection->SendString("--echo-- " + std::string(message));
  }
  void OnBinaryMessage(const std::vector<uint8_t>& data) override {}

  presentation::Connection* connection;
};

class ReceiverDelegate final : public presentation::ReceiverDelegate {
 public:
  ~ReceiverDelegate() override = default;

  std::vector<msgs::PresentationUrlAvailability> OnUrlAvailabilityRequest(
      uint64_t client_id,
      uint64_t request_duration,
      std::vector<std::string> urls) override {
    std::vector<msgs::PresentationUrlAvailability> result;
    result.reserve(urls.size());
    for (const auto& url : urls) {
      OSP_LOG << "got availability request for: " << url;
      result.push_back(msgs::kCompatible);
    }
    return result;
  }

  bool StartPresentation(const presentation::Connection::PresentationInfo& info,
                         uint64_t source_id,
                         const std::string& http_headers) override {
    presentation_id = info.id;
    connection = std::make_unique<presentation::Connection>(
        info, presentation::Connection::Role::kReceiver, &cd);
    cd.connection = connection.get();
    presentation::Receiver::Get()->OnPresentationStarted(
        info.id, connection.get(), presentation::ResponseResult::kSuccess);
    return true;
  }

  bool ConnectToPresentation(uint64_t request_id,
                             const std::string& id,
                             uint64_t source_id) override {
    connection = std::make_unique<presentation::Connection>(
        presentation::Connection::PresentationInfo{
            id, connection->get_presentation_info().url},
        presentation::Connection::Role::kReceiver, &cd);
    cd.connection = connection.get();
    presentation::Receiver::Get()->OnConnectionCreated(
        request_id, connection.get(), presentation::ResponseResult::kSuccess);
    return true;
  }

  void TerminatePresentation(const std::string& id,
                             presentation::TerminationReason reason) override {
    presentation::Receiver::Get()->OnPresentationTerminated(id, reason);
  }

  std::string presentation_id;
  std::unique_ptr<presentation::Connection> connection;
  ReceiverConnectionDelegate cd;
};

void ListenerDemo() {
  SignalThings();

  ListenerObserver listener_observer;
  MdnsServiceListenerConfig listener_config;
  auto mdns_listener =
      MdnsServiceListenerFactory::Create(listener_config, &listener_observer);

  MessageDemuxer demuxer;
  ConnectionClientObserver client_observer;
  auto connection_client =
      ProtocolConnectionClientFactory::Create(&demuxer, &client_observer);

  auto* network_service = NetworkServiceManager::Create(
      std::move(mdns_listener), nullptr, std::move(connection_client), nullptr);

  network_service->GetMdnsServiceListener()->Start();
  network_service->GetProtocolConnectionClient()->Start();

  while (!g_done) {
    network_service->RunEventLoopOnce();
  }

  network_service->GetMdnsServiceListener()->Stop();
  network_service->GetProtocolConnectionClient()->Stop();

  NetworkServiceManager::Dispose();
}

void HandleReceiverCommand(absl::string_view command,
                           absl::string_view argument_tail,
                           ReceiverDelegate& delegate,
                           NetworkServiceManager* manager) {
  if (command == "avail") {
    ServicePublisher* publisher = manager->GetMdnsServicePublisher();

    if (publisher->state() == ServicePublisher::State::kSuspended) {
      publisher->Resume();
    } else {
      publisher->Suspend();
    }
  } else if (command == "close") {
    delegate.connection->Close(presentation::Connection::CloseReason::kClosed);
  } else if (command == "msg") {
    delegate.connection->SendString(argument_tail);
  } else if (command == "term") {
    presentation::Receiver::Get()->OnPresentationTerminated(
        delegate.presentation_id,
        presentation::TerminationReason::kReceiverUserTerminated);
  } else {
    OSP_LOG_FATAL << "Received unknown receiver command: " << command;
  }
}

void RunReceiverPollLoop(pollfd& file_descriptor,
                         NetworkServiceManager* manager,
                         ReceiverDelegate& delegate) {
  write(STDOUT_FILENO, "$ ", 2);

  while (poll(&file_descriptor, 1, 10) >= 0) {
    if (g_done) {
      break;
    }

    manager->RunEventLoopOnce();

    if (file_descriptor.revents == 0) {
      continue;
    } else if (file_descriptor.revents & (POLLERR | POLLHUP)) {
      break;
    }

    std::string line;
    if (!std::getline(std::cin, line)) {
      break;
    }

    std::size_t split_index = line.find_first_of(' ');
    absl::string_view line_view(line);

    HandleReceiverCommand(line_view.substr(0, split_index),
                          line_view.substr(split_index), delegate, manager);

    write(STDOUT_FILENO, "$ ", 2);
  }
}

void CleanupPublisherDemo(NetworkServiceManager* manager) {
  presentation::Receiver::Get()->SetReceiverDelegate(nullptr);
  presentation::Receiver::Get()->Deinit();
  manager->GetMdnsServicePublisher()->Stop();
  manager->GetProtocolConnectionServer()->Stop();

  NetworkServiceManager::Dispose();
}

void PublisherDemo(absl::string_view friendly_name) {
  SignalThings();

  constexpr uint16_t server_port = 6667;

  PublisherObserver publisher_observer;
  // TODO(btolsch): aggregate initialization probably better?
  ServicePublisher::Config publisher_config;
  publisher_config.friendly_name = std::string(friendly_name);
  publisher_config.hostname = "turtle-deadbeef";
  publisher_config.service_instance_name = "deadbeef";
  publisher_config.connection_server_port = server_port;

  auto mdns_publisher = MdnsServicePublisherFactory::Create(
      publisher_config, &publisher_observer);

  ServerConfig server_config;
  std::vector<platform::InterfaceAddresses> interfaces =
      platform::GetInterfaceAddresses();
  for (const auto& interface : interfaces) {
    server_config.connection_endpoints.push_back(
        IPEndpoint{interface.addresses[0].address, server_port});
  }

  MessageDemuxer demuxer;
  ConnectionServerObserver server_observer;
  auto connection_server = ProtocolConnectionServerFactory::Create(
      server_config, &demuxer, &server_observer);
  auto* network_service =
      NetworkServiceManager::Create(nullptr, std::move(mdns_publisher), nullptr,
                                    std::move(connection_server));

  ReceiverDelegate receiver_delegate;
  presentation::Receiver::Get()->Init();
  presentation::Receiver::Get()->SetReceiverDelegate(&receiver_delegate);
  network_service->GetMdnsServicePublisher()->Start();
  network_service->GetProtocolConnectionServer()->Start();

  pollfd stdin_pollfd{STDIN_FILENO, POLLIN};

  RunReceiverPollLoop(stdin_pollfd, network_service, receiver_delegate);

  CleanupPublisherDemo(network_service);
}

}  // namespace
}  // namespace openscreen

struct InputArgs {
  absl::string_view friendly_server_name;
  bool is_verbose;
};

InputArgs GetInputArgs(int argc, char** argv) {
  InputArgs args;

  int c;
  while ((c = getopt(argc, argv, "v")) != -1) {
    switch (c) {
      case 'v':
        args.is_verbose = true;
        break;
    }
  }

  if (optind < argc) {
    args.friendly_server_name = argv[optind];
  }

  return args;
}

int main(int argc, char** argv) {
  using openscreen::platform::LogLevel;

  std::cout << "Usage: demo [-v] [friendly_name]" << std::endl
            << "-v: enable more verbose logging" << std::endl
            << "friendly_name: server name, runs the publisher demo."
            << std::endl
            << "               omitting runs the listener demo." << std::endl
            << std::endl;

  InputArgs args = GetInputArgs(argc, argv);

  const bool is_receiver_demo = !args.friendly_server_name.empty();
  const char* log_filename = is_receiver_demo
                                 ? openscreen::kReceiverLogFilename
                                 : openscreen::kControllerLogFilename;
  openscreen::platform::LogInit(log_filename);

  LogLevel level = args.is_verbose ? LogLevel::kVerbose : LogLevel::kInfo;
  openscreen::platform::SetLogLevel(level);

  if (is_receiver_demo) {
    openscreen::PublisherDemo(args.friendly_server_name);
  } else {
    openscreen::ListenerDemo();
  }

  return 0;
}
