// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_handler.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "net/websockets/websocket_handshake_constants.h"

namespace net {

void ComputeSecWebSocketAccept(const std::string& key,
                               std::string* accept) {
  DCHECK(accept);

  std::string hash =
      base::SHA1HashString(key + websockets::kWebSocketGuid);
  base::Base64Encode(hash, accept);
}

}  // namespace net
