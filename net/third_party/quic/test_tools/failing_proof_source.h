// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_FAILING_PROOF_SOURCE_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_FAILING_PROOF_SOURCE_H_

#include "net/third_party/quic/core/crypto/proof_source.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {
namespace test {

class FailingProofSource : public ProofSource {
 public:
  void GetProof(const QuicSocketAddress& server_address,
                const QuicString& hostname,
                const QuicString& server_config,
                QuicTransportVersion transport_version,
                QuicStringPiece chlo_hash,
                std::unique_ptr<Callback> callback) override;

  QuicReferenceCountedPointer<Chain> GetCertChain(
      const QuicSocketAddress& server_address,
      const QuicString& hostname) override;

  void ComputeTlsSignature(
      const QuicSocketAddress& server_address,
      const QuicString& hostname,
      uint16_t signature_algorithm,
      QuicStringPiece in,
      std::unique_ptr<SignatureCallback> callback) override;
};

}  // namespace test
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_FAILING_PROOF_SOURCE_H_
