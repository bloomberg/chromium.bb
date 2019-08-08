// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_verifier.h"

namespace extensions {

crx_file::VerifierFormat GetWebstoreVerifierFormat() {
  return crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
}

crx_file::VerifierFormat GetPolicyVerifierFormat(
    bool insecure_updates_enabled) {
  // TODO(crbug.com/740715): Eliminate CRX2.
  if (insecure_updates_enabled)
    return crx_file::VerifierFormat::CRX2_OR_CRX3;
  return crx_file::VerifierFormat::CRX3;
}

crx_file::VerifierFormat GetExternalVerifierFormat() {
  return crx_file::VerifierFormat::CRX3;
}

crx_file::VerifierFormat GetTestVerifierFormat() {
  return crx_file::VerifierFormat::CRX3;
}

}  // namespace extensions
