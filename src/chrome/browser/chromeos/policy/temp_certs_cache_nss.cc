// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/temp_certs_cache_nss.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chromeos/network/onc/onc_utils.h"
#include "net/cert/x509_util_nss.h"

namespace policy {

TempCertsCacheNSS::TempCertsCacheNSS(const net::CertificateList& certificates) {
  for (const auto& certificate : certificates) {
    net::ScopedCERTCertificate x509_cert =
        net::x509_util::CreateCERTCertificateFromX509Certificate(
            certificate.get());
    if (!x509_cert) {
      LOG(ERROR) << "Unable to create CERTCertificate";
      continue;
    }

    temp_certs_.push_back(std::move(x509_cert));
  }
}

TempCertsCacheNSS::~TempCertsCacheNSS() {}

}  // namespace policy
