// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "token_binding.h"

#include "net/base/net_errors.h"

namespace net {

Error BuildTokenBindingMessageFromTokenBindings(
    const std::vector<base::StringPiece>& token_bindings,
    std::string* out) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

Error BuildProvidedTokenBinding(crypto::ECPrivateKey* key,
                                const std::vector<uint8_t>& ekm,
                                std::string* out) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

bool ParseTokenBindingMessage(base::StringPiece token_binding_message,
                              base::StringPiece* ec_point,
                              base::StringPiece* signature) {
  NOTREACHED();
  return false;
}

bool VerifyEKMSignature(base::StringPiece ec_point,
                        base::StringPiece signature,
                        base::StringPiece ekm) {
  NOTREACHED();
  return false;
}

}  // namespace net
