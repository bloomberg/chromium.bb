// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_connections_dependencies_provider.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/nearby/bluetooth_adapter_manager.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chrome/browser/nearby_sharing/firewall_hole/nearby_connections_firewall_hole_factory.h"
#include "chrome/browser/nearby_sharing/sharing_mojo_service.h"
#include "chrome/browser/nearby_sharing/tachyon_ice_config_fetcher.h"
#include "chrome/browser/nearby_sharing/tcp_socket/nearby_connections_tcp_socket_factory.h"
#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

namespace ash {
namespace nearby {
namespace {

template <typename T>
struct MojoPipe {
  mojo::PendingRemote<T> remote;
  mojo::PendingReceiver<T> receiver{remote.InitWithNewPipeAndPassReceiver()};
};

class P2PTrustedSocketManagerClientImpl
    : public network::mojom::P2PTrustedSocketManagerClient {
 public:
  explicit P2PTrustedSocketManagerClientImpl(
      mojo::PendingRemote<network::mojom::P2PTrustedSocketManager>
          socket_manager)
      : socket_manager_(std::move(socket_manager)) {}
  ~P2PTrustedSocketManagerClientImpl() override = default;

  // network::mojom::P2PTrustedSocketManagerClient:
  void InvalidSocketPortRangeRequested() override { NOTIMPLEMENTED(); }
  void DumpPacket(const std::vector<uint8_t>& packet_header,
                  uint64_t packet_length,
                  bool incoming) override {
    NOTIMPLEMENTED();
  }

 private:
  mojo::Remote<network::mojom::P2PTrustedSocketManager> socket_manager_;
};

class MdnsResponderFactory
    : public location::nearby::connections::mojom::MdnsResponderFactory {
 public:
  explicit MdnsResponderFactory(Profile* profile) : profile_(profile) {}

  void CreateMdnsResponder(mojo::PendingReceiver<network::mojom::MdnsResponder>
                               responder_receiver) override {
    auto* partition = profile_->GetDefaultStoragePartition();
    if (!partition) {
      LOG(ERROR) << "MdnsResponderFactory::" << __func__
                 << ": GetDefaultStoragePartition(profile) failed.";
      // When |responder_receiver| goes out of scope the pipe will disconnect.
      return;
    }

    auto* network_context = partition->GetNetworkContext();
    if (!network_context) {
      LOG(ERROR) << "MdnsResponderFactory::" << __func__
                 << ": GetNetworkContext() failed.";
      // When |responder_receiver| goes out of scope the pipe will disconnect.
      return;
    }

    network_context->CreateMdnsResponder(std::move(responder_receiver));
  }

 private:
  Profile* profile_;
};

}  // namespace

NearbyConnectionsDependenciesProvider::NearbyConnectionsDependenciesProvider(
    Profile* profile,
    signin::IdentityManager* identity_manager)
    : profile_(profile), identity_manager_(identity_manager) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  bluetooth_manager_ = std::make_unique<BluetoothAdapterManager>();
}

NearbyConnectionsDependenciesProvider::NearbyConnectionsDependenciesProvider() =
    default;

NearbyConnectionsDependenciesProvider::
    ~NearbyConnectionsDependenciesProvider() = default;

location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr
NearbyConnectionsDependenciesProvider::GetDependencies() {
  if (shut_down_)
    return nullptr;

  auto dependencies = location::nearby::connections::mojom::
      NearbyConnectionsDependencies::New();

  if (device::BluetoothAdapterFactory::IsBluetoothSupported())
    dependencies->bluetooth_adapter = GetBluetoothAdapterPendingRemote();
  else
    dependencies->bluetooth_adapter = mojo::NullRemote();

  dependencies->webrtc_dependencies = GetWebRtcDependencies();
  dependencies->wifilan_dependencies = GetWifiLanDependencies();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNearbyShareVerboseLogging)) {
    dependencies->min_log_severity =
        location::nearby::api::LogMessage::Severity::kVerbose;
  }

  return dependencies;
}

void NearbyConnectionsDependenciesProvider::PrepareForShutdown() {
  if (bluetooth_manager_) {
    bluetooth_manager_->Shutdown();
  }
}

void NearbyConnectionsDependenciesProvider::Shutdown() {
  shut_down_ = true;
}

mojo::PendingRemote<bluetooth::mojom::Adapter>
NearbyConnectionsDependenciesProvider::GetBluetoothAdapterPendingRemote() {
  mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver;
  mojo::PendingRemote<bluetooth::mojom::Adapter> pending_remote =
      pending_receiver.InitWithNewPipeAndPassRemote();
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &BluetoothAdapterManager::Initialize, bluetooth_manager_->GetWeakPtr(),
      std::move(pending_receiver)));
  return pending_remote;
}

location::nearby::connections::mojom::WebRtcDependenciesPtr
NearbyConnectionsDependenciesProvider::GetWebRtcDependencies() {
  MojoPipe<network::mojom::P2PTrustedSocketManagerClient> socket_manager_client;
  MojoPipe<network::mojom::P2PTrustedSocketManager> trusted_socket_manager;
  MojoPipe<network::mojom::P2PSocketManager> socket_manager;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<P2PTrustedSocketManagerClientImpl>(
          std::move(trusted_socket_manager.remote)),
      std::move(socket_manager_client.receiver));

  // Create socket manager.
  GetNetworkContext()->CreateP2PSocketManager(
      net::NetworkIsolationKey::CreateTransient(),
      std::move(socket_manager_client.remote),
      std::move(trusted_socket_manager.receiver),
      std::move(socket_manager.receiver));

  MojoPipe<location::nearby::connections::mojom::MdnsResponderFactory>
      mdns_responder_factory_pipe;
  mojo::MakeSelfOwnedReceiver(std::make_unique<MdnsResponderFactory>(profile_),
                              std::move(mdns_responder_factory_pipe.receiver));

  // Create ice config fetcher.
  auto url_loader_factory = profile_->GetURLLoaderFactory();
  MojoPipe<sharing::mojom::IceConfigFetcher> ice_config_fetcher;
  mojo::MakeSelfOwnedReceiver(std::make_unique<TachyonIceConfigFetcher>(
                                  identity_manager_, url_loader_factory),
                              std::move(ice_config_fetcher.receiver));

  MojoPipe<sharing::mojom::WebRtcSignalingMessenger> messenger;
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebRtcSignalingMessenger>(
                                  identity_manager_, url_loader_factory),
                              std::move(messenger.receiver));

  return location::nearby::connections::mojom::WebRtcDependencies::New(
      std::move(socket_manager.remote),
      std::move(mdns_responder_factory_pipe.remote),
      std::move(ice_config_fetcher.remote), std::move(messenger.remote));
}

location::nearby::connections::mojom::WifiLanDependenciesPtr
NearbyConnectionsDependenciesProvider::GetWifiLanDependencies() {
  if (!base::FeatureList::IsEnabled(features::kNearbySharingWifiLan))
    return nullptr;

  MojoPipe<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config;
  ash::GetNetworkConfigService(std::move(cros_network_config.receiver));

  MojoPipe<sharing::mojom::FirewallHoleFactory> firewall_hole_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NearbyConnectionsFirewallHoleFactory>(),
      std::move(firewall_hole_factory.receiver));

  MojoPipe<sharing::mojom::TcpSocketFactory> tcp_socket_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NearbyConnectionsTcpSocketFactory>(base::BindRepeating(
          &NearbyConnectionsDependenciesProvider::GetNetworkContext,
          base::Unretained(this))),
      std::move(tcp_socket_factory.receiver));

  return location::nearby::connections::mojom::WifiLanDependencies::New(
      std::move(cros_network_config.remote),
      std::move(firewall_hole_factory.remote),
      std::move(tcp_socket_factory.remote));
}

network::mojom::NetworkContext*
NearbyConnectionsDependenciesProvider::GetNetworkContext() {
  return profile_->GetDefaultStoragePartition()->GetNetworkContext();
}

}  // namespace nearby
}  // namespace ash
