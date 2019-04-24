// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_
#define CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/platform/modules/broadcastchannel/broadcast_channel.mojom.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT BroadcastChannelProvider
    : public base::RefCountedThreadSafe<BroadcastChannelProvider>,
      public blink::mojom::BroadcastChannelProvider {
 public:
  BroadcastChannelProvider();

  using RenderProcessHostId = int;
  mojo::BindingId Connect(
      RenderProcessHostId render_process_host_id,
      blink::mojom::BroadcastChannelProviderRequest request);

  void ConnectToChannel(
      const url::Origin& origin,
      const std::string& name,
      blink::mojom::BroadcastChannelClientAssociatedPtrInfo client,
      blink::mojom::BroadcastChannelClientAssociatedRequest connection)
      override;

  auto& bindings_for_testing() { return bindings_; }

 private:
  friend class base::RefCountedThreadSafe<BroadcastChannelProvider>;
  class Connection;

  ~BroadcastChannelProvider() override;

  void UnregisterConnection(Connection*);
  void ReceivedMessageOnConnection(Connection*,
                                   const blink::CloneableMessage& message);

  mojo::BindingSet<blink::mojom::BroadcastChannelProvider, RenderProcessHostId>
      bindings_;
  std::map<url::Origin, std::multimap<std::string, std::unique_ptr<Connection>>>
      connections_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_
