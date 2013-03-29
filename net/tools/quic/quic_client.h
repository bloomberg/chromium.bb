// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy client, which connects to a specified port and sends QUIC
// request to that endpoint.

#ifndef NET_TOOLS_QUIC_QUIC_CLIENT_H_
#define NET_TOOLS_QUIC_QUIC_CLIENT_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/tools/flip_server/epoll_server.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/quic_reliable_client_stream.h"

namespace net {

class QuicClient : public EpollCallbackInterface {
 public:
  QuicClient(IPEndPoint server_address, const std::string& server_hostname);
  virtual ~QuicClient();

  // Initializes the client to create a connection. Should be called exactly
  // once before calling StartConnect or Connect. Returns true if the
  // initialization succeeds, false otherwise.
  bool Initialize();

  // "Connect" to the QUIC server, including performing synchronous crypto
  // handshake.
  bool Connect();

  // Start the crypto handshake.  This can be done in place of the synchronous
  // Connect(), but callers are responsible for making sure the crypto handshake
  // completes.
  bool StartConnect();

  // Returns true if the crypto handshake is in progress.
  // Returns false if the handshake is complete or the connection has been
  // closed.
  bool CryptoHandshakeInProgress();

  // Disconnects from the QUIC server.
  void Disconnect();

  // Sends a request simple GET for each URL in arg, and then waits for
  // each to complete.
  void SendRequestsAndWaitForResponse(int argc, char *argv[]);

  // Returns a newly created CreateReliableClientStream, owned by the
  // QuicClient.
  QuicReliableClientStream* CreateReliableClientStream();

  // Wait for events until the stream with the given ID is closed.
  void WaitForStreamToClose(QuicStreamId id);

  // Wait up to 50ms, and handle any events which occur.
  // Returns true if there are any outstanding requests.
  bool WaitForEvents();

  // From EpollCallbackInterface
  virtual void OnRegistration(
      EpollServer* eps, int fd, int event_mask) OVERRIDE {}
  virtual void OnModification(int fd, int event_mask) OVERRIDE {}
  virtual void OnEvent(int fd, EpollEvent* event) OVERRIDE;
  // |fd_| can be unregistered without the client being disconnected. This
  // happens in b3m QuicProber where we unregister |fd_| to feed in events to
  // the client from the SelectServer.
  virtual void OnUnregistration(int fd, bool replaced) OVERRIDE {}
  virtual void OnShutdown(EpollServer* eps, int fd) OVERRIDE {}

  QuicPacketCreator::Options* options();

  QuicClientSession* session() { return session_.get(); }

  bool connected() const;

  int packets_dropped() { return packets_dropped_; }

  void set_bind_to_address(IPAddressNumber address) {
    bind_to_address_ = address;
  }

  IPAddressNumber bind_to_address() const { return bind_to_address_; }

  void set_local_port(int local_port) { local_port_ = local_port; }

  int local_port() { return local_port_; }

  const IPEndPoint& server_address() const { return server_address_; }

  const IPEndPoint& client_address() const { return client_address_; }

  EpollServer* epoll_server() { return &epoll_server_; }

  int fd() { return fd_; }

 private:
  // Read a UDP packet and hand it to the framer.
  bool ReadAndProcessPacket();

  // Set of streams created (and owned) by this client
  base::hash_set<QuicReliableClientStream*> streams_;

  // Address of the server.
  const IPEndPoint server_address_;

  // Hostname of the server. This may be a DNS name or an IP address literal.
  const std::string server_hostname_;

  // Address of the client if the client is connected to the server.
  IPEndPoint client_address_;

  // If initialized, the address to bind to.
  IPAddressNumber bind_to_address_;
  // Local port to bind to. Initialize to 0.
  int local_port_;

  // Session which manages streams.
  scoped_ptr<QuicClientSession> session_;
  // Listens for events on the client socket.
  EpollServer epoll_server_;
  // UDP socket.
  int fd_;

  // Tracks if the client is initialized to connect.
  bool initialized_;

  // If overflow_supported_ is true, this will be the number of packets dropped
  // during the lifetime of the server.  This may overflow if enough packets
  // are dropped.
  int packets_dropped_;

  // True if the kernel supports SO_RXQ_OVFL, the number of packets dropped
  // because the socket would otherwise overflow.
  bool overflow_supported_;

  DISALLOW_COPY_AND_ASSIGN(QuicClient);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_CLIENT_H_
