// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy client, which connects to a specified port and sends QUIC
// request to that endpoint.

#ifndef NET_TOOLS_QUIC_QUIC_CLIENT_H_
#define NET_TOOLS_QUIC_QUIC_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

namespace net {

class ProofVerifier;

namespace tools {

class QuicEpollConnectionHelper;

namespace test {
class QuicClientPeer;
}  // namespace test

class QuicClient : public EpollCallbackInterface,
                   public QuicDataStream::Visitor {
 public:
  class ResponseListener {
   public:
    ResponseListener() {}
    virtual ~ResponseListener() {}
    virtual void OnCompleteResponse(QuicStreamId id,
                                    const BalsaHeaders& response_headers,
                                    const string& response_body) = 0;
  };

  QuicClient(IPEndPoint server_address,
             const string& server_hostname,
             const QuicVersionVector& supported_versions,
             bool print_response);
  QuicClient(IPEndPoint server_address,
             const std::string& server_hostname,
             const QuicConfig& config,
             const QuicVersionVector& supported_versions);

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

  // Returns true if the crypto handshake has yet to establish encryption.
  // Returns false if encryption is active (even if the server hasn't confirmed
  // the handshake) or if the connection has been closed.
  bool EncryptionBeingEstablished();

  // Disconnects from the QUIC server.
  void Disconnect();

  // Sends a request simple GET for each URL in |args|, and then waits for
  // each to complete.
  void SendRequestsAndWaitForResponse(const CommandLine::StringVector& args);

  // Returns a newly created QuicSpdyClientStream, owned by the
  // QuicClient.
  QuicSpdyClientStream* CreateReliableClientStream();

  // Wait for events until the stream with the given ID is closed.
  void WaitForStreamToClose(QuicStreamId id);

  // Wait for events until the handshake is confirmed.
  void WaitForCryptoHandshakeConfirmed();

  // Wait up to 50ms, and handle any events which occur.
  // Returns true if there are any outstanding requests.
  bool WaitForEvents();

  // From EpollCallbackInterface
  virtual void OnRegistration(EpollServer* eps,
                              int fd,
                              int event_mask) OVERRIDE {}
  virtual void OnModification(int fd, int event_mask) OVERRIDE {}
  virtual void OnEvent(int fd, EpollEvent* event) OVERRIDE;
  // |fd_| can be unregistered without the client being disconnected. This
  // happens in b3m QuicProber where we unregister |fd_| to feed in events to
  // the client from the SelectServer.
  virtual void OnUnregistration(int fd, bool replaced) OVERRIDE {}
  virtual void OnShutdown(EpollServer* eps, int fd) OVERRIDE {}

  // QuicDataStream::Visitor
  virtual void OnClose(QuicDataStream* stream) OVERRIDE;

  QuicPacketCreator::Options* options();

  QuicClientSession* session() { return session_.get(); }

  bool connected() const;

  void set_bind_to_address(IPAddressNumber address) {
    bind_to_address_ = address;
  }

  IPAddressNumber bind_to_address() const { return bind_to_address_; }

  void set_local_port(int local_port) { local_port_ = local_port; }

  const IPEndPoint& server_address() const { return server_address_; }

  const IPEndPoint& client_address() const { return client_address_; }

  EpollServer* epoll_server() { return &epoll_server_; }

  int fd() { return fd_; }

  string server_hostname() {
    return server_hostname_;
  }

  // This should only be set before the initial Connect()
  void set_server_hostname(const string& hostname) {
    server_hostname_ = hostname;
  }

  // SetProofVerifier sets the ProofVerifier that will be used to verify the
  // server's certificate and takes ownership of |verifier|.
  void SetProofVerifier(ProofVerifier* verifier) {
    // TODO(rtenneti): We should set ProofVerifier in QuicClientSession.
    crypto_config_.SetProofVerifier(verifier);
  }

  // SetChannelIDSigner sets a ChannelIDSigner that will be called when the
  // server supports channel IDs to sign a message proving possession of the
  // given ChannelID. This object takes ownership of |signer|.
  void SetChannelIDSigner(ChannelIDSigner* signer) {
    crypto_config_.SetChannelIDSigner(signer);
  }

  void SetSupportedVersions(const QuicVersionVector& versions) {
    DCHECK(!session_.get());
    supported_versions_ = versions;
  }

  // Takes ownership of the listener.
  void set_response_listener(ResponseListener* listener) {
    response_listener_.reset(listener);
  }

 protected:
  virtual QuicGuid GenerateGuid();
  virtual QuicEpollConnectionHelper* CreateQuicConnectionHelper();
  virtual QuicPacketWriter* CreateQuicPacketWriter();

 private:
  friend class net::tools::test::QuicClientPeer;

  // Read a UDP packet and hand it to the framer.
  bool ReadAndProcessPacket();

  // Address of the server.
  const IPEndPoint server_address_;

  // Hostname of the server. This may be a DNS name or an IP address literal.
  std::string server_hostname_;

  // config_ and crypto_config_ contain configuration and cached state about
  // servers.
  QuicConfig config_;
  QuicCryptoClientConfig crypto_config_;

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

  // Helper to be used by created connections.
  scoped_ptr<QuicEpollConnectionHelper> helper_;

  // Listens for full responses.
  scoped_ptr<ResponseListener> response_listener_;

  // Writer used to actually send packets to the wire.
  scoped_ptr<QuicPacketWriter> writer_;

  // Tracks if the client is initialized to connect.
  bool initialized_;

  // If overflow_supported_ is true, this will be the number of packets dropped
  // during the lifetime of the server.  This may overflow if enough packets
  // are dropped.
  int packets_dropped_;

  // True if the kernel supports SO_RXQ_OVFL, the number of packets dropped
  // because the socket would otherwise overflow.
  bool overflow_supported_;

  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary). We will always pick supported_versions_[0] as the
  // initial version to use.
  QuicVersionVector supported_versions_;

  // If true, then the contents of each response will be printed to stdout
  // when the stream is closed (in OnClose).
  bool print_response_;

  DISALLOW_COPY_AND_ASSIGN(QuicClient);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_CLIENT_H_
