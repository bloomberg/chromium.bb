// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_DEFAULT_PROOF_PROVIDERS_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_DEFAULT_PROOF_PROVIDERS_IMPL_H_

#include <memory>

#include "net/third_party/quic/core/crypto/proof_verifier.h"

namespace quic {

std::unique_ptr<ProofVerifier> CreateDefaultProofVerifierImpl();

}
#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_DEFAULT_PROOF_PROVIDERS_IMPL_H_
