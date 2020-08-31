// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file._

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_CERT_UTILS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_CERT_UTILS_IMPL_H_

#include "net/cert/asn1_util.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"

namespace quic {

class QuicCertUtilsImpl {
 public:
  static bool ExtractSubjectNameFromDERCert(
      quiche::QuicheStringPiece cert,
      quiche::QuicheStringPiece* subject_out) {
    return net::asn1::ExtractSubjectFromDERCert(cert, subject_out);
  }
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_CERT_UTILS_IMPL_H_
