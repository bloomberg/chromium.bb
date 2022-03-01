// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/ppb_x509_util_shared.h"

#include "base/strings/string_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace ppapi {

bool PPB_X509Util_Shared::GetCertificateFields(
    const net::X509Certificate& cert,
    ppapi::PPB_X509Certificate_Fields* fields) {
  const net::CertPrincipal& issuer = cert.issuer();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COMMON_NAME,
                   std::make_unique<base::Value>(issuer.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_LOCALITY_NAME,
                   std::make_unique<base::Value>(issuer.locality_name));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_ISSUER_STATE_OR_PROVINCE_NAME,
      std::make_unique<base::Value>(issuer.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COUNTRY_NAME,
                   std::make_unique<base::Value>(issuer.country_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_NAME,
                   std::make_unique<base::Value>(
                       base::JoinString(issuer.organization_names, "\n")));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_UNIT_NAME,
                   std::make_unique<base::Value>(
                       base::JoinString(issuer.organization_unit_names, "\n")));

  const net::CertPrincipal& subject = cert.subject();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COMMON_NAME,
                   std::make_unique<base::Value>(subject.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_LOCALITY_NAME,
                   std::make_unique<base::Value>(subject.locality_name));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_SUBJECT_STATE_OR_PROVINCE_NAME,
      std::make_unique<base::Value>(subject.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COUNTRY_NAME,
                   std::make_unique<base::Value>(subject.country_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_NAME,
                   std::make_unique<base::Value>(
                       base::JoinString(subject.organization_names, "\n")));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_UNIT_NAME,
                   std::make_unique<base::Value>(base::JoinString(
                       subject.organization_unit_names, "\n")));

  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SERIAL_NUMBER,
                   base::Value::ToUniquePtrValue(base::Value(
                       base::as_bytes(base::make_span(cert.serial_number())))));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_BEFORE,
      std::make_unique<base::Value>(cert.valid_start().ToDoubleT()));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_AFTER,
      std::make_unique<base::Value>(cert.valid_expiry().ToDoubleT()));
  base::StringPiece cert_der =
      net::x509_util::CryptoBufferAsStringPiece(cert.cert_buffer());
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_RAW,
                   base::Value::ToUniquePtrValue(
                       base::Value(base::as_bytes(base::make_span(cert_der)))));
  return true;
}

bool PPB_X509Util_Shared::GetCertificateFields(
    const char* der,
    size_t length,
    ppapi::PPB_X509Certificate_Fields* fields) {
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(
          base::as_bytes(base::make_span(der, length)));
  if (!cert)
    return false;
  return GetCertificateFields(*cert, fields);
}

}  // namespace ppapi
