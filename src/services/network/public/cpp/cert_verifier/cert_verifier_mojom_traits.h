// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_VERIFIER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_VERIFIER_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/big_buffer.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"

namespace mojo {

template <>
struct StructTraits<cert_verifier::mojom::RequestParamsDataView,
                    net::CertVerifier::RequestParams> {
  static const scoped_refptr<net::X509Certificate>& certificate(
      const net::CertVerifier::RequestParams& params) {
    return params.certificate();
  }
  static const std::string& hostname(
      const net::CertVerifier::RequestParams& params) {
    return params.hostname();
  }
  static int32_t flags(const net::CertVerifier::RequestParams& params) {
    return params.flags();
  }
  static const std::string& ocsp_response(
      const net::CertVerifier::RequestParams& params) {
    return params.ocsp_response();
  }
  static const std::string& sct_list(
      const net::CertVerifier::RequestParams& params) {
    return params.sct_list();
  }

  static bool Read(cert_verifier::mojom::RequestParamsDataView data,
                   net::CertVerifier::RequestParams* params);
};

template <>
struct StructTraits<cert_verifier::mojom::CertVerifierConfigDataView,
                    net::CertVerifier::Config> {
  static bool enable_rev_checking(const net::CertVerifier::Config& config) {
    return config.enable_rev_checking;
  }
  static bool require_rev_checking_local_anchors(
      const net::CertVerifier::Config& config) {
    return config.require_rev_checking_local_anchors;
  }
  static bool enable_sha1_local_anchors(
      const net::CertVerifier::Config& config) {
    return config.enable_sha1_local_anchors;
  }
  static bool disable_symantec_enforcement(
      const net::CertVerifier::Config& config) {
    return config.disable_symantec_enforcement;
  }
  static mojo_base::BigBuffer crl_set(const net::CertVerifier::Config& config) {
    if (!config.crl_set) {
      return mojo_base::BigBuffer();
    }
    const std::string& bytes = config.crl_set->unparsed_crl_set();
    CHECK(!bytes.empty());
    return mojo_base::BigBuffer(base::make_span(
        reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
  }
  static const std::vector<scoped_refptr<net::X509Certificate>>&
  additional_trust_anchors(const net::CertVerifier::Config& config) {
    return config.additional_trust_anchors;
  }

  static bool Read(cert_verifier::mojom::CertVerifierConfigDataView data,
                   net::CertVerifier::Config* config);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_VERIFIER_MOJOM_TRAITS_H_
