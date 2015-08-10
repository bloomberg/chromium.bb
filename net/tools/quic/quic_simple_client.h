// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy client, which connects to a specified port and sends QUIC
// request to that endpoint.

#ifndef NET_TOOLS_QUIC_QUIC_SIMPLE_CLIENT_H_
#define NET_TOOLS_QUIC_QUIC_SIMPLE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_packet_reader.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

namespace net {

struct HttpRequestInfo;
class ProofVerifier;
class QuicServerId;
class QuicConnectionHelper;
class UDPClientSocket;

namespace tools {

namespace test {
class QuicClientPeer;
}  // namespace test

class QuicSimpleClient : public QuicDataStream::Visitor,
                         public QuicPacketReader::Visitor {
 public:
  class ResponseListener {
   public:
    ResponseListener() {}
    virtual ~ResponseListener() {}
    virtual void OnCompleteResponse(QuicStreamId id,
                                    const HttpResponseHeaders& response_headers,
                                    const std::string& response_body) = 0;
  };

  // The client uses these objects to keep track of any data to resend upon
  // receipt of a stateless reject.  Recall that the client API allows callers
  // to optimistically send data to the server prior to handshake-confirmation.
  // If the client subsequently receives a stateless reject, it must tear down
  // its existing session, create a new session, and resend all previously sent
  // data.  It uses these objects to keep track of all the sent data, and to
  // resend the data upon a subsequent connection.
  class QuicDataToResend {
   public:
    // Takes ownership of |headers|.  |headers| may be null, since it's possible
    // to send data without headers.
    QuicDataToResend(HttpRequestInfo* headers,
                     base::StringPiece body,
                     bool fin);

    virtual ~QuicDataToResend();

    // Must be overridden by specific classes with the actual method for
    // re-sending data.
    virtual void Resend() = 0;

   protected:
    HttpRequestInfo* headers_;
    base::StringPiece body_;
    bool fin_;

   private:
    DISALLOW_COPY_AND_ASSIGN(QuicDataToResend);
  };

  // Create a quic client, which will have events managed by an externally owned
  // EpollServer.
  QuicSimpleClient(IPEndPoint server_address,
                   const QuicServerId& server_id,
                   const QuicVersionVector& supported_versions);
  QuicSimpleClient(IPEndPoint server_address,
                   const QuicServerId& server_id,
                   const QuicVersionVector& supported_versions,
                   const QuicConfig& config);

  ~QuicSimpleClient() override;

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
  void StartConnect();

  // Returns true if the crypto handshake has yet to establish encryption.
  // Returns false if encryption is active (even if the server hasn't confirmed
  // the handshake) or if the connection has been closed.
  bool EncryptionBeingEstablished();

  // Disconnects from the QUIC server.
  void Disconnect();

  // Sends an HTTP request and does not wait for response before returning.
  void SendRequest(const HttpRequestInfo& headers,
                   base::StringPiece body,
                   bool fin);

  // Sends an HTTP request and waits for response before returning.
  void SendRequestAndWaitForResponse(const HttpRequestInfo& headers,
                                     base::StringPiece body,
                                     bool fin);

  // Sends a request simple GET for each URL in |args|, and then waits for
  // each to complete.
  void SendRequestsAndWaitForResponse(
      const base::CommandLine::StringVector& url_list);

  // Returns a newly created QuicSpdyClientStream, owned by the
  // QuicSimpleClient.
  QuicSpdyClientStream* CreateReliableClientStream();

  // Wait for events until the stream with the given ID is closed.
  void WaitForStreamToClose(QuicStreamId id);

  // Wait for events until the handshake is confirmed.
  void WaitForCryptoHandshakeConfirmed();

  // Wait up to 50ms, and handle any events which occur.
  // Returns true if there are any outstanding requests.
  bool WaitForEvents();

  // Migrate to a new socket during an active connection.
  bool MigrateSocket(const IPAddressNumber& new_host);

  // QuicPacketReader::Visitor
  void OnReadError(int result) override;
  bool OnPacket(const QuicEncryptedPacket& packet,
                IPEndPoint local_address,
                IPEndPoint peer_address) override;

  // QuicDataStream::Visitor
  void OnClose(QuicDataStream* stream) override;

  // If the crypto handshake has not yet been confirmed, adds the data to the
  // queue of data to resend if the client receives a stateless reject.
  // Otherwise, deletes the data.  Takes ownerership of |data_to_resend|.
  void MaybeAddQuicDataToResend(QuicDataToResend* data_to_resend);

  QuicClientSession* session() { return session_.get(); }

  bool connected() const;
  bool goaway_received() const;

  void set_bind_to_address(IPAddressNumber address) {
    bind_to_address_ = address;
  }

  IPAddressNumber bind_to_address() const { return bind_to_address_; }

  void set_local_port(int local_port) { local_port_ = local_port; }

  const IPEndPoint& server_address() const { return server_address_; }

  const IPEndPoint& client_address() const { return client_address_; }

  const QuicServerId& server_id() const { return server_id_; }

  // This should only be set before the initial Connect()
  void set_server_id(const QuicServerId& server_id) {
    server_id_ = server_id;
  }

  void SetUserAgentID(const std::string& user_agent_id) {
    crypto_config_.set_user_agent_id(user_agent_id);
  }

  // SetProofVerifier sets the ProofVerifier that will be used to verify the
  // server's certificate and takes ownership of |verifier|.
  void SetProofVerifier(ProofVerifier* verifier) {
    // TODO(rtenneti): We should set ProofVerifier in QuicClientSession.
    crypto_config_.SetProofVerifier(verifier);
  }

  // SetChannelIDSource sets a ChannelIDSource that will be called, when the
  // server supports channel IDs, to obtain a channel ID for signing a message
  // proving possession of the channel ID. This object takes ownership of
  // |source|.
  void SetChannelIDSource(ChannelIDSource* source) {
    crypto_config_.SetChannelIDSource(source);
  }

  void SetSupportedVersions(const QuicVersionVector& versions) {
    supported_versions_ = versions;
  }

  // Takes ownership of the listener.
  void set_response_listener(ResponseListener* listener) {
    response_listener_.reset(listener);
  }

  QuicConfig* config() { return &config_; }

  void set_store_response(bool val) { store_response_ = val; }

  size_t latest_response_code() const;
  const std::string& latest_response_headers() const;
  const std::string& latest_response_body() const;

  // Change the initial maximum packet size of the connection.  Has to be called
  // before Connect()/StartConnect() in order to have any effect.
  void set_initial_max_packet_length(QuicByteCount initial_max_packet_length) {
    initial_max_packet_length_ = initial_max_packet_length;
  }

  int num_stateless_rejects_received() const {
    return num_stateless_rejects_received_;
  }

  // The number of client hellos sent, taking stateless rejects into
  // account.  In the case of a stateless reject, the initial
  // connection object may be torn down and a new one created.  The
  // user cannot rely upon the latest connection object to get the
  // total number of client hellos sent, and should use this function
  // instead.
  int GetNumSentClientHellos();

  // Returns any errors that occurred at the connection-level (as
  // opposed to the session-level).  When a stateless reject occurs,
  // the error of the last session may not reflect the overall state
  // of the connection.
  QuicErrorCode connection_error() const;

 protected:
  // Generates the next ConnectionId for |server_id_|.  By default, if the
  // cached server config contains a server-designated ID, that ID will be
  // returned.  Otherwise, the next random ID will be returned.
  QuicConnectionId GetNextConnectionId();

  // Returns the next server-designated ConnectionId from the cached config for
  // |server_id_|, if it exists.  Otherwise, returns 0.
  QuicConnectionId GetNextServerDesignatedConnectionId();

  // Generates a new, random connection ID (as opposed to a server-designated
  // connection ID).
  virtual QuicConnectionId GenerateNewConnectionId();

  virtual QuicConnectionHelper* CreateQuicConnectionHelper();
  virtual QuicPacketWriter* CreateQuicPacketWriter();

  virtual QuicClientSession* CreateQuicClientSession(
      const QuicConfig& config,
      QuicConnection* connection,
      const QuicServerId& server_id,
      QuicCryptoClientConfig* crypto_config);

 private:
  friend class net::tools::test::QuicClientPeer;

  // A packet writer factory that always returns the same writer
  class DummyPacketWriterFactory : public QuicConnection::PacketWriterFactory {
   public:
    DummyPacketWriterFactory(QuicPacketWriter* writer);
    ~DummyPacketWriterFactory() override;

    QuicPacketWriter* Create(QuicConnection* connection) const override;

   private:
    QuicPacketWriter* writer_;
  };

  // Specific QuicClient class for storing data to resend.
  class ClientQuicDataToResend : public QuicDataToResend {
   public:
    // Takes ownership of |headers|.
    ClientQuicDataToResend(HttpRequestInfo* headers,
                           base::StringPiece body,
                           bool fin,
                           QuicSimpleClient* client)
        : QuicDataToResend(headers, body, fin), client_(client) {
      DCHECK(headers);
      DCHECK(client);
    }

    ~ClientQuicDataToResend() override {}

    void Resend() override;

   private:
    QuicSimpleClient* client_;

    DISALLOW_COPY_AND_ASSIGN(ClientQuicDataToResend);
  };

  // Used during initialization: creates the UDP socket FD, sets socket options,
  // and binds the socket to our address.
  bool CreateUDPSocket();

  // Read a UDP packet and hand it to the framer.
  bool ReadAndProcessPacket();

  //  Used by |helper_| to time alarms.
  QuicClock clock_;

  // Address of the server.
  const IPEndPoint server_address_;

  // |server_id_| is a tuple (hostname, port, is_https) of the server.
  QuicServerId server_id_;

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

  // Writer used to actually send packets to the wire. Needs to outlive
  // |session_|.
  scoped_ptr<QuicPacketWriter> writer_;

  // Session which manages streams.
  scoped_ptr<QuicClientSession> session_;

  // UDP socket connected to the server.
  scoped_ptr<UDPClientSocket> socket_;

  // Connection on the socket. Owned by |session_|.
  QuicConnection* connection_;

  // Helper to be used by created connections.
  scoped_ptr<QuicConnectionHelper> helper_;

  // Listens for full responses.
  scoped_ptr<ResponseListener> response_listener_;

  // Tracks if the client is initialized to connect.
  bool initialized_;

  // If overflow_supported_ is true, this will be the number of packets dropped
  // during the lifetime of the server.
  QuicPacketCount packets_dropped_;

  // True if the kernel supports SO_RXQ_OVFL, the number of packets dropped
  // because the socket would otherwise overflow.
  bool overflow_supported_;

  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary). We will always pick supported_versions_[0] as the
  // initial version to use.
  QuicVersionVector supported_versions_;

  // If true, store the latest response code, headers, and body.
  bool store_response_;
  // HTTP response code from most recent response.
  size_t latest_response_code_;
  // HTTP headers from most recent response.
  std::string latest_response_headers_;
  // Body of most recent response.
  std::string latest_response_body_;

  // The initial value of maximum packet size of the connection.  If set to
  // zero, the default is used.
  QuicByteCount initial_max_packet_length_;

  // The number of stateless rejects received during the current/latest
  // connection.
  // TODO(jokulik): Consider some consistent naming scheme (or other) for member
  // variables that are kept per-request, per-connection, and over the client's
  // lifetime.
  int num_stateless_rejects_received_;

  // The number of hellos sent during the current/latest connection.
  int num_sent_client_hellos_;

  // Used to store any errors that occurred with the overall connection (as
  // opposed to that associated with the last session object).
  QuicErrorCode connection_error_;

  // True when the client is attempting to connect or re-connect the session (in
  // the case of a stateless reject).  Set to false  between a call to
  // Disconnect() and the subsequent call to StartConnect().  When
  // connected_or_attempting_connect_ is false, the session object corresponds
  // to the previous client-level connection.
  bool connected_or_attempting_connect_;

  // Keeps track of any data sent before the handshake.
  std::vector<QuicDataToResend*> data_sent_before_handshake_;

  // Once the client receives a stateless reject, keeps track of any data that
  // must be resent upon a subsequent successful connection.
  std::vector<QuicDataToResend*> data_to_resend_on_connect_;

  // The log used for the sockets.
  NetLog net_log_;

  scoped_ptr<QuicPacketReader> packet_reader_;

  base::WeakPtrFactory<QuicSimpleClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicSimpleClient);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SIMPLE_CLIENT_H_
