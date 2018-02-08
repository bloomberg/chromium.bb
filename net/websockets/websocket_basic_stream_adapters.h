// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_
#define NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_basic_stream.h"

namespace net {

class ClientSocketHandle;
class IOBuffer;

// Trivial adapter to make WebSocketBasicStream use an HTTP/1.1 connection.
class NET_EXPORT_PRIVATE WebSocketClientSocketHandleAdapter
    : public WebSocketBasicStream::Adapter {
 public:
  explicit WebSocketClientSocketHandleAdapter(
      std::unique_ptr<ClientSocketHandle> connection);
  ~WebSocketClientSocketHandleAdapter() override;

  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  void Disconnect() override;
  bool is_initialized() const override;

 private:
  std::unique_ptr<ClientSocketHandle> connection_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_
