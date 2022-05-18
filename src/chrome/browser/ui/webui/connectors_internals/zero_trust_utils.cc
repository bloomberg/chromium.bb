// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/zero_trust_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace enterprise_connectors {
namespace utils {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

connectors_internals::mojom::KeyTrustLevel ParseTrustLevel(
    BPKUR::KeyTrustLevel trust_level) {
  switch (trust_level) {
    case BPKUR::CHROME_BROWSER_TPM_KEY:
      return connectors_internals::mojom::KeyTrustLevel::TPM;
    case BPKUR::CHROME_BROWSER_OS_KEY:
      return connectors_internals::mojom::KeyTrustLevel::OS;
    default:
      return connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED;
  }
}

connectors_internals::mojom::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return connectors_internals::mojom::KeyType::RSA;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return connectors_internals::mojom::KeyType::EC;
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

connectors_internals::mojom::KeyInfoPtr GetKeyInfo() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();
  if (key_manager) {
    auto metadata = key_manager->GetLoadedKeyMetadata();
    if (metadata) {
      return connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::KEY_LOADED,
          ParseTrustLevel(metadata->trust_level),
          AlgorithmToType(metadata->algorithm));
    } else {
      return connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
          connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED,
          connectors_internals::mojom::KeyType::UNKNOWN);
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return connectors_internals::mojom::KeyInfo::New(
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
      connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED,
      connectors_internals::mojom::KeyType::UNKNOWN);
}

}  // namespace utils
}  // namespace enterprise_connectors
