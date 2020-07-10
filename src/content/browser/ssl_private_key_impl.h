// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_PRIVATE_KEY_IMPL_H_
#define CONTENT_BROWSER_SSL_PRIVATE_KEY_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

class SSLPrivateKeyImpl : public network::mojom::SSLPrivateKey {
 public:
  explicit SSLPrivateKeyImpl(scoped_refptr<net::SSLPrivateKey> ssl_private_key);
  ~SSLPrivateKeyImpl() override;

  // network::mojom::SSLPrivateKey:
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            network::mojom::SSLPrivateKey::SignCallback callback) override;

 private:
  void Callback(network::mojom::SSLPrivateKey::SignCallback callback,
                net::Error net_error,
                const std::vector<uint8_t>& signature);

  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;

  DISALLOW_COPY_AND_ASSIGN(SSLPrivateKeyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_PRIVATE_KEY_IMPL_H_
