// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DATA_PARSER_H_
#define ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DATA_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {

constexpr int kEncryptedDataByteSize = 16;
constexpr int kAesBlockByteSize = 16;

}  // namespace

namespace ash {
namespace quick_pair {

// This class is responsible for parsing the untrusted bytes from a Bluetooth
// device during Fast Pair.
class FastPairDataParser : public mojom::FastPairDataParser {
 public:
  explicit FastPairDataParser(
      mojo::PendingReceiver<mojom::FastPairDataParser> receiver);
  ~FastPairDataParser() override;
  FastPairDataParser(const FastPairDataParser&) = delete;
  FastPairDataParser& operator=(const FastPairDataParser&) = delete;

  // Gets the hex string representation of the device's model ID from the
  // service data.
  void GetHexModelIdFromServiceData(
      const std::vector<uint8_t>& service_data,
      GetHexModelIdFromServiceDataCallback callback) override;

  // Decrypts |encrypted_response_bytes| using |aes_key| and returns a parsed
  // DecryptedResponse instance if possible.
  void ParseDecryptedResponse(
      const std::vector<uint8_t>& aes_key_bytes,
      const std::vector<uint8_t>& encrypted_response_bytes,
      ParseDecryptedResponseCallback callback) override;

  // Decrypts |encrypted_passkey_bytes| using |aes_key| and returns a parsed
  // DecryptedPasskey instance if possible.
  void ParseDecryptedPasskey(
      const std::vector<uint8_t>& aes_key_bytes,
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      ParseDecryptedPasskeyCallback callback) override;

  // Attempts to parse a 'Not Discoverable' advertisement from |service_data|.
  void ParseNotDiscoverableAdvertisement(
      const std::vector<uint8_t>& service_data,
      ParseNotDiscoverableAdvertisementCallback callback) override;

 private:
  mojo::Receiver<mojom::FastPairDataParser> receiver_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DATA_PARSER_H_
