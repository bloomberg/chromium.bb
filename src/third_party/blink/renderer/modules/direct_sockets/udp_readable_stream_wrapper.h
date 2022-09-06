// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_READABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_READABLE_STREAM_WRAPPER_H_

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class MODULES_EXPORT UDPReadableStreamWrapper
    : public GarbageCollected<UDPReadableStreamWrapper>,
      public ReadableStreamWrapper {
 public:
  UDPReadableStreamWrapper(ScriptState* script_state,
                           const Member<UDPSocketMojoRemote> udp_socket,
                           base::OnceCallback<void(bool)> on_close,
                           uint32_t high_water_mark);

  void Pull() override;

  bool Push(base::span<const uint8_t> data,
            const absl::optional<net::IPEndPoint>& src_addr) override;

  void CloseStream(bool error) override;
  void CloseSocket(bool error) override;

  void Trace(Visitor* visitor) const override;

 private:
  class UDPUnderlyingSource;

  const Member<UDPSocketMojoRemote> udp_socket_;
  base::OnceCallback<void(bool)> on_close_;

  int32_t pending_receive_requests_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_READABLE_STREAM_WRAPPER_H_
