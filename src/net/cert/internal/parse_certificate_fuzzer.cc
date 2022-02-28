// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::CertErrors errors;
  scoped_refptr<net::ParsedCertificate> cert = net::ParsedCertificate::Create(
      net::x509_util::CreateCryptoBuffer(base::make_span(data, size)), {},
      &errors);

  // Severe errors must be provided iff the parsing failed.
  CHECK_EQ(errors.ContainsAnyErrorWithSeverity(net::CertError::SEVERITY_HIGH),
           cert == nullptr);

  return 0;
}
