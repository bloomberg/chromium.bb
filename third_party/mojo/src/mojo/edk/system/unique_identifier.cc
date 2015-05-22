// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/unique_identifier.h"

#include <ostream>

#include "base/strings/string_number_conversions.h"
#include "mojo/edk/embedder/platform_support.h"

namespace mojo {
namespace system {

std::ostream& operator<<(std::ostream& out,
                         const UniqueIdentifier& unique_identifier) {
  return out << base::HexEncode(unique_identifier.data_,
                                sizeof(unique_identifier.data_));
}

// static
UniqueIdentifier UniqueIdentifier::Generate(
    embedder::PlatformSupport* platform_support) {
  UniqueIdentifier rv;
  platform_support->GetCryptoRandomBytes(rv.data_, sizeof(rv.data_));
  return rv;
}

}  // namespace system
}  // namespace mojo
