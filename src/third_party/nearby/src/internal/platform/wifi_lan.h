// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLATFORM_PUBLIC_WIFI_LAN_H_
#define PLATFORM_PUBLIC_WIFI_LAN_H_

#include "absl/container/flat_hash_map.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"

namespace location {
namespace nearby {

class WifiLanSocket final {
 public:
  WifiLanSocket() = default;
  WifiLanSocket(const WifiLanSocket&) = default;
  WifiLanSocket& operator=(const WifiLanSocket&) = default;
  ~WifiLanSocket() = default;
  explicit WifiLanSocket(std::unique_ptr<api::WifiLanSocket> socket)
      : impl_(std::move(socket)) {}

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  InputStream& GetInputStream() { return impl_->GetInputStream(); }

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  OutputStream& GetOutputStream() { return impl_->GetOutputStream(); }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() { return impl_->Close(); }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by WifiLanMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while WifiLanSocket object is
  // itself valid. Typically WifiLanSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::WifiLanSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::WifiLanSocket> impl_;
};

class WifiLanServerSocket final {
 public:
  WifiLanServerSocket() = default;
  WifiLanServerSocket(const WifiLanServerSocket&) = default;
  WifiLanServerSocket& operator=(const WifiLanServerSocket&) = default;
  ~WifiLanServerSocket() = default;
  explicit WifiLanServerSocket(std::unique_ptr<api::WifiLanServerSocket> socket)
      : impl_(std::move(socket)) {}

  // Returns ip address.
  std::string GetIPAddress() { return impl_->GetIPAddress(); }

  // Returns port.
  int GetPort() { return impl_->GetPort(); }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  WifiLanSocket Accept() {
    std::unique_ptr<api::WifiLanSocket> socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO)
          << "WifiLanServerSocket Accept() failed on server socket: " << this;
    }
    return WifiLanSocket(std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "WifiLanServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::WifiLanServerSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::WifiLanServerSocket> impl_;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium {
 public:
  using Platform = api::ImplementationPlatform;

  struct DiscoveredServiceCallback {
    std::function<void(NsdServiceInfo service_info,
                       const std::string& service_type)>
        service_discovered_cb =
            DefaultCallback<NsdServiceInfo, const std::string&>();
    std::function<void(NsdServiceInfo service_info,
                       const std::string& service_type)>
        service_lost_cb = DefaultCallback<NsdServiceInfo, const std::string&>();
  };

  struct DiscoveryCallbackInfo {
    std::string service_id;
    DiscoveredServiceCallback medium_callback;
  };

  WifiLanMedium() : impl_(Platform::CreateWifiLanMedium()) {}
  ~WifiLanMedium() = default;

  // Starts WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service is now advertising.
  // On error if the service cannot start to advertise or the nsd_type in
  // NsdServiceInfo has been passed previously which StopAdvertising is not
  // been called.
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info);

  // Stops WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service stops advertising.
  // On error if the service cannot stop advertising or the nsd_type in
  // NsdServiceInfo cannot be found.
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info);

  // Returns true once the WifiLan discovery has been initiated.
  bool StartDiscovery(const std::string& service_id,
                      const std::string& service_type,
                      DiscoveredServiceCallback callback);

  // Returns true once service_type is associated to existing callback. If the
  // callback is the last found then WifiLan discovery will be stopped.
  bool StopDiscovery(const std::string& service_type);

  // Returns a new WifiLanSocket.
  // On Success, WifiLanSocket::IsValid() returns true.
  WifiLanSocket ConnectToService(const NsdServiceInfo& remote_service_info,
                                 CancellationFlag* cancellation_flag);

  // Returns a new WifiLanSocket by ip address and port.
  // On Success, WifiLanSocket::IsValid()returns true.
  WifiLanSocket ConnectToService(const std::string& ip_address, int port,
                                 CancellationFlag* cancellation_flag);

  // Returns a new WifiLanServerSocket.
  // On Success, WifiLanServerSocket::IsValid() returns true.
  WifiLanServerSocket ListenForService(int port = 0) {
    return WifiLanServerSocket(impl_->ListenForService(port));
  }

  // Returns the port range as a pair of min and max port.
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
    return impl_->GetDynamicPortRange();
  }

  bool IsValid() const { return impl_ != nullptr; }

  api::WifiLanMedium& GetImpl() { return *impl_; }

 private:
  Mutex mutex_;
  std::unique_ptr<api::WifiLanMedium> impl_;
  absl::flat_hash_map<std::string, std::unique_ptr<DiscoveryCallbackInfo>>
      discovery_callbacks_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<std::string> discovery_services_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_WIFI_LAN_H_
