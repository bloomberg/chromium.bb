// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBTRANSPORT_QUIC_TRANSPORT_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_WEBTRANSPORT_QUIC_TRANSPORT_CONNECTOR_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/quic_transport.mojom.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImpl;

class QuicTransportConnectorImpl final
    : public blink::mojom::QuicTransportConnector {
 public:
  // |frame| is needed for devtools - sometimes (e.g., the connector is for
  // workers) there is not appropriate frame to associate, and in that case
  // nullptr is provided.
  QuicTransportConnectorImpl(
      int process_id,
      base::WeakPtr<RenderFrameHostImpl> frame,
      const url::Origin& origin,
      const net::NetworkIsolationKey& network_isolation_key);
  ~QuicTransportConnectorImpl() override;

  void Connect(const GURL& url,
               mojo::PendingRemote<network::mojom::QuicTransportHandshakeClient>
                   handshake_client) override;

 private:
  const int process_id_;
  const base::WeakPtr<RenderFrameHostImpl> frame_;
  const url::Origin origin_;
  const net::NetworkIsolationKey network_isolation_key_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBTRANSPORT_QUIC_TRANSPORT_CONNECTOR_IMPL_H_
