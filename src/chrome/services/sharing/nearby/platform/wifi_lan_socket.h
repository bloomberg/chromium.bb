// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_SOCKET_H_

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "chrome/services/sharing/nearby/platform/bidirectional_stream.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "third_party/nearby/src/cpp/platform/api/wifi_lan.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace location {
namespace nearby {
namespace chrome {

// An implementation of Nearby Connections's abstract class api::WifiLanSocket.
// This implementation wraps a TCPConnectedSocket which lives until Close() is
// called or the WifiLanSocket instance is destroyed.
class WifiLanSocket : public api::WifiLanSocket {
 public:
  // Parameters needed to construct a WifiLanSocket.
  struct ConnectedSocketParameters {
    ConnectedSocketParameters(
        const net::IPEndPoint& remote_end_point,
        mojo::PendingRemote<network::mojom::TCPConnectedSocket>
            tcp_connected_socket,
        mojo::ScopedDataPipeConsumerHandle receive_stream,
        mojo::ScopedDataPipeProducerHandle send_stream);
    ~ConnectedSocketParameters();
    ConnectedSocketParameters(ConnectedSocketParameters&&);
    ConnectedSocketParameters& operator=(ConnectedSocketParameters&&);

    net::IPEndPoint remote_end_point;
    mojo::PendingRemote<network::mojom::TCPConnectedSocket>
        tcp_connected_socket;
    mojo::ScopedDataPipeConsumerHandle receive_stream;
    mojo::ScopedDataPipeProducerHandle send_stream;
  };

  WifiLanSocket(const net::IPEndPoint& remote_end_point,
                mojo::PendingRemote<network::mojom::TCPConnectedSocket>
                    tcp_connected_socket,
                mojo::ScopedDataPipeConsumerHandle receive_stream,
                mojo::ScopedDataPipeProducerHandle send_stream);
  explicit WifiLanSocket(ConnectedSocketParameters connected_socket_parameters);
  WifiLanSocket(const WifiLanSocket&) = delete;
  WifiLanSocket& operator=(const WifiLanSocket&) = delete;
  ~WifiLanSocket() override;

  // api::WifiLanSocket:
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

  // Returns whether or not the underlying TCP connected socket is bound (not
  // closed) or unbound (closed).
  bool IsClosed() const;

 private:
  // Close if the TCP socket disconnects.
  void OnTcpConnectedSocketDisconnected();

  void CloseTcpSocketIfNecessary();

  // Protects |tcp_connected_socket_| while closing.
  base::Lock lock_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::SharedRemote<network::mojom::TCPConnectedSocket> tcp_connected_socket_;
  BidirectionalStream bidirectional_stream_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_SOCKET_H_
