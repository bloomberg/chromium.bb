// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_
#define ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_

#include <memory>

#include "ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace base {
class Clock;
class OneShotTimer;
}  // namespace base

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace multidevice_setup {
class MultiDeviceSetupClient;
}

namespace secure_channel {

class SecureChannelClient;

// ConnectionManager implementation which utilizes SecureChannelClient to
// establish a connection to a multidevice host.
class ConnectionManagerImpl : public ConnectionManager,
                              public ConnectionAttempt::Delegate,
                              public ClientChannel::Observer {
 public:
  ConnectionManagerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      device_sync::DeviceSyncClient* device_sync_client,
      SecureChannelClient* secure_channel_client,
      const std::string& feature_name,
      const std::string& metric_name_result,
      const std::string& metric_name_latency,
      const std::string& metric_name_duration);
  ~ConnectionManagerImpl() override;

  // ConnectionManager:
  ConnectionManager::Status GetStatus() const override;
  void AttemptNearbyConnection() override;
  void Disconnect() override;
  void SendMessage(const std::string& payload) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
          file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;

 private:
  friend class ConnectionManagerImplTest;

  class MetricsRecorder : public ConnectionManager::Observer {
   public:
    MetricsRecorder(ConnectionManager* connection_manager,
                    base::Clock* clock,
                    const std::string& metric_name_result,
                    const std::string& metric_name_latency,
                    const std::string& metric_name_duration);
    ~MetricsRecorder() override;
    MetricsRecorder(const MetricsRecorder&) = delete;
    MetricsRecorder* operator=(const MetricsRecorder&) = delete;

    // ConnectionManager::Observer:
    void OnConnectionStatusChanged() override;

   private:
    ConnectionManager* connection_manager_;
    ConnectionManager::Status status_;
    base::Clock* clock_;
    base::Time status_change_timestamp_;
    const std::string metric_name_result_;
    const std::string metric_name_latency_;
    const std::string metric_name_duration_;
  };

  ConnectionManagerImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      device_sync::DeviceSyncClient* device_sync_client,
      SecureChannelClient* secure_channel_client,
      std::unique_ptr<base::OneShotTimer> timer,
      const std::string& feature_name,
      const std::string& metrics_name_result,
      const std::string& metrics_name_latency,
      const std::string& metrics_name_duration,
      base::Clock* clock);

  // ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(std::unique_ptr<ClientChannel> channel) override;

  // ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  void OnConnectionTimeout();
  void TearDownConnection();

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  device_sync::DeviceSyncClient* device_sync_client_;
  SecureChannelClient* secure_channel_client_;
  std::unique_ptr<ConnectionAttempt> connection_attempt_;
  std::unique_ptr<ClientChannel> channel_;
  std::unique_ptr<base::OneShotTimer> timer_;
  const std::string feature_name_;
  std::unique_ptr<MetricsRecorder> metrics_recorder_;
  base::WeakPtrFactory<ConnectionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace ash

#endif  // ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_MANAGER_IMPL_H_
