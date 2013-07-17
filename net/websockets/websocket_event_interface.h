// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_
#define NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

// Interface for events sent from the network layer to the content layer. These
// events will generally be sent as-is to the renderer process.
class NET_EXPORT WebSocketEventInterface {
 public:
  typedef int WebSocketMessageType;
  virtual ~WebSocketEventInterface() {}
  // Called in response to an AddChannelRequest. This generally means that a
  // response has been received from the remote server, but the response might
  // have been generated internally. If |fail| is true, the channel cannot be
  // used and it is valid to delete the WebSocketChannel from within this
  // callback.
  virtual void OnAddChannelResponse(
      bool fail,
      const std::string& selected_subprotocol) = 0;

  // Called when a data frame has been received from the remote host and needs
  // to be forwarded to the renderer process. It is not safe to delete the
  // WebSocketChannel object from within this callback.
  virtual void OnDataFrame(bool fin,
                           WebSocketMessageType type,
                           const std::vector<char>& data) = 0;

  // Called to provide more send quota for this channel to the renderer
  // process. Currently the quota units are always bytes of message body
  // data. In future it might depend on the type of multiplexing in use. It is
  // not safe to delete the WebSocketChannel from within this callback.
  virtual void OnFlowControl(int64 quota) = 0;

  // Called when the remote server has Started the WebSocket Closing
  // Handshake. The client should not attempt to send any more messages after
  // receiving this message. It will be followed by OnDropChannel() when the
  // closing handshake is complete. It is not safe to delete the
  // WebSocketChannel from within this callback.
  virtual void OnClosingHandshake() = 0;

  // Called when the channel has been dropped, either due to a network close, a
  // network error, or a protocol error. This may or may not be preceeded by a
  // call to OnClosingHandshake().
  //
  // Warning: Both the |code| and |reason| are passed through to Javascript, so
  // callers must take care not to provide details that could be useful to
  // attackers attempting to use WebSockets to probe networks.
  //
  // The channel should not be used again after OnDropChannel() has been
  // called.
  //
  // It is not safe to delete the WebSocketChannel from within this
  // callback. It is recommended to delete the channel after returning to the
  // event loop.
  virtual void OnDropChannel(uint16 code, const std::string& reason) = 0;

 protected:
  WebSocketEventInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketEventInterface);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_
