// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_
#define CHROMECAST_RENDERER_CAST_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chromecast/renderer/cast_activity_url_filter_manager.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"

namespace chromecast {

// This class allows cast_shell to provide a WebSocketHandshakeThrottle
// implementation to delay or block WebSocket handshakes.
// This must be constructed on the render thread, and then used and destructed
// on a single thread, which can be different from the render thread.
class CastWebSocketHandshakeThrottleProvider
    : public content::WebSocketHandshakeThrottleProvider {
 public:
  explicit CastWebSocketHandshakeThrottleProvider(
      CastActivityUrlFilterManager* url_filter_manager);
  ~CastWebSocketHandshakeThrottleProvider() override;

  // content::WebSocketHandshakeThrottleProvider implementation:
  std::unique_ptr<content::WebSocketHandshakeThrottleProvider> Clone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle> CreateThrottle(
      int render_frame_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CastWebSocketHandshakeThrottleProvider(
      const CastWebSocketHandshakeThrottleProvider& other);

  CastActivityUrlFilterManager* const cast_activity_url_filter_manager_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_ASSIGN(CastWebSocketHandshakeThrottleProvider);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_
