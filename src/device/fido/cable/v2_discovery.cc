// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_discovery.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace device {
namespace cablev2 {

Discovery::Discovery(
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kQRKeySize> qr_generator_key,
    std::vector<std::unique_ptr<Pairing>> pairings,
    base::Optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
        pairing_callback)
    : FidoDeviceDiscovery(
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy),
      network_context_(network_context),
      local_identity_seed_(fido_parsing_utils::Materialize(
          base::span<const uint8_t, kQRSeedSize>(qr_generator_key.data(),
                                                 kQRSeedSize))),
      qr_secret_(fido_parsing_utils::Materialize(
          base::span<const uint8_t, kQRSecretSize>(
              qr_generator_key.data() + kQRSeedSize,
              kQRSecretSize))),
      eid_key_(Derive<EXTENT(eid_key_)>(qr_secret_,
                                        base::span<const uint8_t>(),
                                        DerivedValueType::kEIDKey)),
      pairings_(std::move(pairings)),
      pairing_callback_(std::move(pairing_callback)) {
  static_assert(EXTENT(qr_generator_key) == kQRSecretSize + kQRSeedSize, "");
}

Discovery::~Discovery() = default;

void Discovery::StartInternal() {
  DCHECK(!started_);

  for (auto& pairing : pairings_) {
    tunnels_pending_advert_.emplace_back(std::make_unique<FidoTunnelDevice>(
        network_context_, std::move(pairing)));
  }
  pairings_.clear();

  started_ = true;
  NotifyDiscoveryStarted(true);

  std::vector<std::array<uint8_t, kAdvertSize>> pending_adverts(
      std::move(pending_adverts_));
  for (const auto& advert : pending_adverts) {
    OnBLEAdvertSeen(advert);
  }
}

void Discovery::OnBLEAdvertSeen(
    const std::array<uint8_t, kAdvertSize>& advert) {
  if (!started_) {
    pending_adverts_.push_back(advert);
    return;
  }

  if (base::Contains(observed_adverts_, advert)) {
    return;
  }
  observed_adverts_.insert(advert);

  // Check whether the EID satisfies any pending tunnels.
  for (std::vector<std::unique_ptr<FidoTunnelDevice>>::iterator i =
           tunnels_pending_advert_.begin();
       i != tunnels_pending_advert_.end(); i++) {
    if (!(*i)->MatchAdvert(advert)) {
      continue;
    }

    FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert)
                    << " matches pending tunnel)";
    std::unique_ptr<FidoTunnelDevice> device(std::move(*i));
    tunnels_pending_advert_.erase(i);
    AddDevice(std::move(device));
    return;
  }

  // Check whether the EID matches a QR code.
  base::Optional<CableEidArray> plaintext = eid::Decrypt(advert, eid_key_);
  if (plaintext) {
    FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert) << " matches QR code)";
    AddDevice(std::make_unique<cablev2::FidoTunnelDevice>(
        network_context_,
        base::BindOnce(&Discovery::AddPairing, weak_factory_.GetWeakPtr()),
        qr_secret_, local_identity_seed_, *plaintext));
    return;
  }

  FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert) << ": no v2 match)";
}

void Discovery::AddPairing(std::unique_ptr<Pairing> pairing) {
  if (!pairing_callback_) {
    return;
  }

  pairing_callback_->Run(std::move(pairing));
}

}  // namespace cablev2
}  // namespace device
