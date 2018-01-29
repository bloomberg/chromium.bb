// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_UDP_SOCKET_H_
#define SERVICES_NETWORK_UDP_SOCKET_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/interfaces/address_family.mojom.h"
#include "net/interfaces/ip_endpoint.mojom.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/interfaces/udp_socket.mojom.h"

namespace net {
class IOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace network {

class UDPSocket : public mojom::UDPSocket {
 public:
  // Number of Send()/SendTo() requests that are queued internally. Public for
  // testing.
  static const uint32_t kMaxPendingSendRequests = 32;
  // A socket wrapper class that allows tests to substitute the default
  // implementation (implemented using net::UDPSocket) with a test
  // implementation.
  class SocketWrapper {
   public:
    virtual ~SocketWrapper() {}
    // This wrapper class forwards the functions to an concrete udp socket
    // implementation. Please refer to udp_socket_posix.h/udp_socket_win.h for
    // definitions.
    virtual int Open(net::AddressFamily address_family) = 0;
    virtual int Connect(const net::IPEndPoint& remote_addr) = 0;
    virtual int Bind(const net::IPEndPoint& local_addr) = 0;
    virtual int SetSendBufferSize(uint32_t size) = 0;
    virtual int SetReceiveBufferSize(uint32_t size) = 0;
    virtual int SendTo(
        net::IOBuffer* buf,
        int buf_len,
        const net::IPEndPoint& dest_addr,
        const net::CompletionCallback& callback,
        const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
    virtual int Write(
        net::IOBuffer* buf,
        int buf_len,
        const net::CompletionCallback& callback,
        const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
    virtual int RecvFrom(net::IOBuffer* buf,
                         int buf_len,
                         net::IPEndPoint* address,
                         const net::CompletionCallback& callback) = 0;
    virtual int GetLocalAddress(net::IPEndPoint* address) const = 0;
  };

  UDPSocket(mojom::UDPSocketRequest request,
            mojom::UDPSocketReceiverPtr receiver);
  ~UDPSocket() override;

  // Sets connection error handler.
  void set_connection_error_handler(base::OnceClosure handler);

  // UDPSocket implementation.
  void Open(net::AddressFamily address_family, OpenCallback callback) override;
  void Connect(const net::IPEndPoint& remote_addr,
               ConnectCallback callback) override;
  void Bind(const net::IPEndPoint& local_addr, BindCallback callback) override;
  void SetSendBufferSize(uint32_t size,
                         SetSendBufferSizeCallback callback) override;
  void SetReceiveBufferSize(uint32_t size,
                            SetReceiveBufferSizeCallback callback) override;
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override;
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;

 private:
  friend class UDPSocketTest;

  // Represents a pending Send()/SendTo() request that is yet to be sent to the
  // |socket_|. In the case of Send(), |addr| will be not filled in.
  struct PendingSendRequest {
    PendingSendRequest();
    ~PendingSendRequest();

    std::unique_ptr<net::IPEndPoint> addr;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
    scoped_refptr<net::IOBufferWithSize> data;
    SendToCallback callback;
  };

  // Returns whether a successful Connect() or Bind() has been executed.
  bool IsConnectedOrBound() const;

  void DoRecvFrom();
  void DoSendToOrWrite(
      const net::IPEndPoint* dest_addr,
      const base::span<const uint8_t>& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      SendToCallback callback);
  void DoSendToOrWriteBuffer(
      const net::IPEndPoint* dest_addr,
      scoped_refptr<net::IOBufferWithSize> buffer,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      SendToCallback callback);

  void OnRecvFromCompleted(int net_result);
  void OnSendToCompleted(int net_result);

  // Whether an Open() has been successfully executed.
  bool is_opened_;

  // Whether a Bind() has been successfully executed.
  bool is_bound_;

  // Whether a Connect() has been successfully executed.
  bool is_connected_;

  // The interface which gets data from fulfilled receive requests.
  mojom::UDPSocketReceiverPtr receiver_;

  std::unique_ptr<SocketWrapper> wrapped_socket_;

  // Non-null when there is a pending RecvFrom operation on socket.
  scoped_refptr<net::IOBuffer> recvfrom_buffer_;

  // Non-null when there is a pending Send/SendTo operation on socket.
  scoped_refptr<net::IOBufferWithSize> send_buffer_;
  SendToCallback send_callback_;

  // The address of the sender of a received packet. This address might not be
  // filled if an error occurred during the receiving of the packet.
  net::IPEndPoint recvfrom_address_;

  // How many more packets the client side expects to receive.
  uint32_t remaining_recv_slots_;

  // The queue owns the PendingSendRequest instances.
  base::circular_deque<std::unique_ptr<PendingSendRequest>>
      pending_send_requests_;

  mojo::Binding<mojom::UDPSocket> binding_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_UDP_SOCKET_H_
