// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_RECEIVER_SESSION_CLIENT_H_
#define FUCHSIA_ENGINE_BROWSER_RECEIVER_SESSION_CLIENT_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/callback.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

// Holds a Cast Streaming Receiver Session.
class ReceiverSessionClient {
 public:
  explicit ReceiverSessionClient(
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
      bool video_only_receiver);
  ReceiverSessionClient(const ReceiverSessionClient& other) = delete;

  ~ReceiverSessionClient();

  ReceiverSessionClient& operator=(const ReceiverSessionClient&) = delete;

  void SetCastStreamingReceiver(
      mojo::AssociatedRemote<mojom::CastStreamingReceiver>
          cast_streaming_receiver);

 private:
  // Populated in the ctor, and removed when |receiver_session_| is created in
  // SetCastStreamingReceiver().
  fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request_;

  // Created in SetCastStreamingReceiver(), and empty prior to that call.
  std::unique_ptr<cast_streaming::ReceiverSession> receiver_session_;

  const bool video_only_receiver_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_RECEIVER_SESSION_CLIENT_H_
