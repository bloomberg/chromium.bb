// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_HINTS_BROWSER_SIMPLE_NETWORK_HINTS_HANDLER_IMPL_H_
#define COMPONENTS_NETWORK_HINTS_BROWSER_SIMPLE_NETWORK_HINTS_HANDLER_IMPL_H_

#include "base/macros.h"
#include "components/network_hints/common/network_hints.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace network_hints {

// Simple browser-side handler for DNS prefetch requests.
// Each renderer process requires its own filter.
class SimpleNetworkHintsHandlerImpl : public mojom::NetworkHintsHandler {
 public:
  SimpleNetworkHintsHandlerImpl(int render_process_id, int render_frame_id);
  ~SimpleNetworkHintsHandlerImpl() override;

  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<mojom::NetworkHintsHandler> receiver);

  // mojom::NetworkHintsHandler methods:
  void PrefetchDNS(const std::vector<std::string>& names) override;
  void Preconnect(const GURL& url, bool allow_credentials) override;

 private:
  const int render_process_id_;
  const int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(SimpleNetworkHintsHandlerImpl);
};

}  // namespace network_hints

#endif  // COMPONENTS_NETWORK_HINTS_BROWSER_SIMPLE_NETWORK_HINTS_HANDLER_IMPL_H_
