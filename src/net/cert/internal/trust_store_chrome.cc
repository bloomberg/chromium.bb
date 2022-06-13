// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

struct ChromeRootCertInfo {
  base::span<const uint8_t> root_cert_der;
};

namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-inc.cc"
}  // namespace

TrustStoreChrome::TrustStoreChrome()
    : TrustStoreChrome(kChromeRootCertList, /*certs_are_static=*/true) {}

TrustStoreChrome::TrustStoreChrome(base::span<const ChromeRootCertInfo> certs,
                                   bool certs_are_static) {
  // TODO(hchao, sleevi): Explore keeping a CRYPTO_BUFFER of just the DER
  // certificate and subject name. This would hopefully save memory compared
  // to keeping the full parsed representation in memory, especially when
  // there are multiple instances of TrustStoreChrome.
  for (const auto& cert_info : certs) {
    bssl::UniquePtr<CRYPTO_BUFFER> cert;
    if (certs_are_static) {
      // TODO(mattm,hchao): When the component updater is implemented, ensure
      // the static data crypto_buffers for the compiled-in roots are kept
      // alive, so that roots from the component updater data will de-dupe
      // against them.
      cert = x509_util::CreateCryptoBufferFromStaticDataUnsafe(
          cert_info.root_cert_der);
    } else {
      cert = x509_util::CreateCryptoBuffer(cert_info.root_cert_der);
    }
    CertErrors errors;
    auto parsed = ParsedCertificate::Create(
        std::move(cert), x509_util::DefaultParseCertificateOptions(), &errors);
    DCHECK(parsed);
    // TODO(hchao): Figure out how to fail gracefully when the Chrome Root Store
    // gets a bad component update.
    trust_store_.AddTrustAnchor(parsed);
  }
}

TrustStoreChrome::~TrustStoreChrome() = default;

void TrustStoreChrome::SyncGetIssuersOf(const ParsedCertificate* cert,
                                        ParsedCertificateList* issuers) {
  trust_store_.SyncGetIssuersOf(cert, issuers);
}

CertificateTrust TrustStoreChrome::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) const {
  return trust_store_.GetTrust(cert, debug_data);
}

bool TrustStoreChrome::Contains(const ParsedCertificate* cert) const {
  return trust_store_.Contains(cert);
}

// static
std::unique_ptr<TrustStoreChrome> TrustStoreChrome::CreateTrustStoreForTesting(
    base::span<const ChromeRootCertInfo> certs) {
  // Note: wrap_unique is used because the constructor is private.
  return base::WrapUnique(
      new TrustStoreChrome(certs, /*certs_are_static=*/false));
}

}  // namespace net
