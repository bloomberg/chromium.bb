// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_messaging.h"
#include "chrome/browser/chromeos/diagnosticsd/mojo_utils.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/diagnosticsd_client.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/diagnosticsd/dbus-constants.h"

namespace chromeos {

namespace {

// Interval used between successive connection attempts to the diagnosticsd.
// This is a safety measure for avoiding busy loops when the diagnosticsd is
// dysfunctional.
constexpr base::TimeDelta kConnectionAttemptInterval =
    base::TimeDelta::FromSeconds(1);
// The maximum number of consecutive connection attempts to the diagnosticsd
// before giving up. This is to prevent wasting system resources on hopeless
// attempts to connect in cases when the diagnosticsd is dysfunctional.
constexpr int kMaxConnectionAttemptCount = 10;

DiagnosticsdBridge* g_diagnosticsd_bridge_instance = nullptr;

// Real implementation of the DiagnosticsdBridge delegate.
class DiagnosticsdBridgeDelegateImpl final
    : public DiagnosticsdBridge::Delegate {
 public:
  DiagnosticsdBridgeDelegateImpl();
  ~DiagnosticsdBridgeDelegateImpl() override;

  // Delegate overrides:
  void CreateDiagnosticsdServiceFactoryMojoInvitation(
      diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr*
          diagnosticsd_service_factory_mojo_ptr,
      base::ScopedFD* remote_endpoint_fd) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdBridgeDelegateImpl);
};

DiagnosticsdBridgeDelegateImpl::DiagnosticsdBridgeDelegateImpl() = default;

DiagnosticsdBridgeDelegateImpl::~DiagnosticsdBridgeDelegateImpl() = default;

void DiagnosticsdBridgeDelegateImpl::
    CreateDiagnosticsdServiceFactoryMojoInvitation(
        diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr*
            diagnosticsd_service_factory_mojo_ptr,
        base::ScopedFD* remote_endpoint_fd) {
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe = invitation.AttachMessagePipe(
      diagnostics::kDiagnosticsdMojoConnectionChannelToken);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  diagnosticsd_service_factory_mojo_ptr->Bind(
      mojo::InterfacePtrInfo<diagnosticsd::mojom::DiagnosticsdServiceFactory>(
          std::move(server_pipe), 0 /* version */));
  *remote_endpoint_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
}

}  // namespace

DiagnosticsdBridge::Delegate::~Delegate() = default;

// static
DiagnosticsdBridge* DiagnosticsdBridge::Get() {
  return g_diagnosticsd_bridge_instance;
}

// static
base::TimeDelta DiagnosticsdBridge::connection_attempt_interval_for_testing() {
  return kConnectionAttemptInterval;
}

// static
int DiagnosticsdBridge::max_connection_attempt_count_for_testing() {
  return kMaxConnectionAttemptCount;
}

DiagnosticsdBridge::DiagnosticsdBridge(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : DiagnosticsdBridge(std::make_unique<DiagnosticsdBridgeDelegateImpl>(),
                         std::move(url_loader_factory)) {}

DiagnosticsdBridge::DiagnosticsdBridge(
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(std::move(delegate)),
      web_request_service_(std::move(url_loader_factory)) {
  DCHECK(delegate_);
  DCHECK(!g_diagnosticsd_bridge_instance);
  g_diagnosticsd_bridge_instance = this;
  WaitForDBusService();
}

DiagnosticsdBridge::~DiagnosticsdBridge() {
  DCHECK_EQ(g_diagnosticsd_bridge_instance, this);
  g_diagnosticsd_bridge_instance = nullptr;
}

void DiagnosticsdBridge::SetConfigurationData(const std::string* data) {
  configuration_data_ = data;
}

const std::string& DiagnosticsdBridge::GetConfigurationDataForTesting() {
  return configuration_data_ ? *configuration_data_ : base::EmptyString();
}

void DiagnosticsdBridge::WaitForDBusService() {
  if (connection_attempt_ >= kMaxConnectionAttemptCount) {
    DLOG(WARNING) << "Stopping attempts to connect to diagnosticsd - too many "
                     "unsuccessful attempts in a row";
    return;
  }
  ++connection_attempt_;

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  DBusThreadManager::Get()
      ->GetDiagnosticsdClient()
      ->WaitForServiceToBeAvailable(
          base::BindOnce(&DiagnosticsdBridge::OnWaitedForDBusService,
                         dbus_waiting_weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsdBridge::ScheduleWaitingForDBusService() {
  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DiagnosticsdBridge::WaitForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()),
      kConnectionAttemptInterval);
}

void DiagnosticsdBridge::OnWaitedForDBusService(bool service_is_available) {
  if (!service_is_available) {
    DLOG(WARNING) << "The diagnosticsd D-Bus service is unavailable";
    return;
  }

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  BootstrapMojoConnection();
}

void DiagnosticsdBridge::BootstrapMojoConnection() {
  DCHECK(!diagnosticsd_service_factory_mojo_ptr_);

  // Create a Mojo message pipe and attach
  // |diagnosticsd_service_factory_mojo_ptr_| to its local endpoint.
  base::ScopedFD remote_endpoint_fd;
  delegate_->CreateDiagnosticsdServiceFactoryMojoInvitation(
      &diagnosticsd_service_factory_mojo_ptr_, &remote_endpoint_fd);
  DCHECK(diagnosticsd_service_factory_mojo_ptr_);
  DCHECK(remote_endpoint_fd.is_valid());
  diagnosticsd_service_factory_mojo_ptr_.set_connection_error_handler(
      base::BindOnce(&DiagnosticsdBridge::OnMojoConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));

  // Queue a call that would establish full-duplex Mojo communication with the
  // diagnosticsd daemon by sending an interface pointer to the self instance.
  mojo_self_binding_.Close();
  diagnosticsd::mojom::DiagnosticsdClientPtr self_proxy;
  mojo_self_binding_.Bind(mojo::MakeRequest(&self_proxy));
  diagnosticsd_service_factory_mojo_ptr_->GetService(
      mojo::MakeRequest(&diagnosticsd_service_mojo_ptr_), std::move(self_proxy),
      base::BindOnce(&DiagnosticsdBridge::OnMojoGetServiceCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // Send the file descriptor with the Mojo message pipe's remote endpoint to
  // the diagnosticsd daemon via the D-Bus.
  DBusThreadManager::Get()->GetDiagnosticsdClient()->BootstrapMojoConnection(
      std::move(remote_endpoint_fd),
      base::BindOnce(&DiagnosticsdBridge::OnBootstrappedMojoConnection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsdBridge::OnBootstrappedMojoConnection(bool success) {
  if (success)
    return;
  DLOG(ERROR) << "Failed to establish Mojo connection to diagnosticsd";
  diagnosticsd_service_factory_mojo_ptr_.reset();
  diagnosticsd_service_mojo_ptr_.reset();
  ScheduleWaitingForDBusService();
}

void DiagnosticsdBridge::OnMojoGetServiceCompleted() {
  DCHECK(diagnosticsd_service_mojo_ptr_);
  DVLOG(0) << "Established Mojo communication with diagnosticsd";
  // Reset the current connection attempt counter, since a successful
  // initialization of Mojo communication has completed.
  connection_attempt_ = 0;
}

void DiagnosticsdBridge::OnMojoConnectionError() {
  DLOG(WARNING) << "Mojo connection to the diagnosticsd daemon got shut down";
  diagnosticsd_service_factory_mojo_ptr_.reset();
  diagnosticsd_service_mojo_ptr_.reset();
  ScheduleWaitingForDBusService();
}

void DiagnosticsdBridge::PerformWebRequest(
    diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod http_method,
    mojo::ScopedHandle url,
    std::vector<mojo::ScopedHandle> headers,
    mojo::ScopedHandle request_body,
    PerformWebRequestCallback callback) {
  // Extract a GURL value from a ScopedHandle.
  GURL gurl;
  if (url.is_valid()) {
    std::unique_ptr<base::SharedMemory> shared_memory;
    gurl = GURL(GetStringPieceFromMojoHandle(std::move(url), &shared_memory));
    if (!shared_memory) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  // Extract headers from ScopedHandle's.
  std::vector<base::StringPiece> header_contents;
  std::vector<std::unique_ptr<base::SharedMemory>> shared_memories;
  for (auto& header : headers) {
    if (!header.is_valid()) {
      header_contents.push_back("");
      continue;
    }
    shared_memories.push_back(nullptr);
    header_contents.push_back(GetStringPieceFromMojoHandle(
        std::move(header), &shared_memories.back()));
    if (!shared_memories.back()) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  // Extract a string value from a ScopedHandle.
  std::string request_body_content;
  if (request_body.is_valid()) {
    std::unique_ptr<base::SharedMemory> shared_memory;
    request_body_content = std::string(
        GetStringPieceFromMojoHandle(std::move(request_body), &shared_memory));
    if (!shared_memory) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  web_request_service_.PerformRequest(
      http_method, std::move(gurl), std::move(header_contents),
      std::move(request_body_content), std::move(callback));
}

void DiagnosticsdBridge::GetConfigurationData(
    GetConfigurationDataCallback callback) {
  std::move(callback).Run(configuration_data_ ? *configuration_data_
                                              : std::string());
}

void DiagnosticsdBridge::SendDiagnosticsProcessorMessageToUi(
    mojo::ScopedHandle json_message,
    SendDiagnosticsProcessorMessageToUiCallback callback) {
  // Extract the string value of the received message.
  DCHECK(json_message);
  std::unique_ptr<base::SharedMemory> json_message_shared_memory;
  base::StringPiece json_message_string = GetStringPieceFromMojoHandle(
      std::move(json_message), &json_message_shared_memory);
  if (json_message_string.empty()) {
    LOG(ERROR) << "Failed to read data from mojo handle";
    std::move(callback).Run(mojo::ScopedHandle() /* response_json_message */);
    return;
  }

  DeliverDiagnosticsdUiMessageToExtensions(
      json_message_string.as_string(),
      base::BindOnce(
          [](SendDiagnosticsProcessorMessageToUiCallback callback,
             const std::string& response) {
            mojo::ScopedHandle response_mojo_handle;
            if (!response.empty()) {
              response_mojo_handle =
                  CreateReadOnlySharedMemoryMojoHandle(response);
              if (!response_mojo_handle)
                LOG(ERROR) << "Failed to create mojo handle for string";
            }
            std::move(callback).Run(std::move(response_mojo_handle));
          },
          std::move(callback)));
}

}  // namespace chromeos
