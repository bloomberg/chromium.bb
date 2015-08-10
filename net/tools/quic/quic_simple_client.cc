// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_client.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/spdy_utils.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/udp/udp_client_socket.h"

using std::string;
using std::vector;

namespace net {
namespace tools {

void QuicSimpleClient::ClientQuicDataToResend::Resend() {
  client_->SendRequest(*headers_, body_, fin_);
  delete headers_;
  headers_ = nullptr;
}

QuicSimpleClient::QuicSimpleClient(IPEndPoint server_address,
                                   const QuicServerId& server_id,
                                   const QuicVersionVector& supported_versions)
    : server_address_(server_address),
      server_id_(server_id),
      local_port_(0),
      helper_(CreateQuicConnectionHelper()),
      initialized_(false),
      supported_versions_(supported_versions),
      weak_factory_(this) {
}

QuicSimpleClient::QuicSimpleClient(IPEndPoint server_address,
                                   const QuicServerId& server_id,
                                   const QuicVersionVector& supported_versions,
                                   const QuicConfig& config)
    : server_address_(server_address),
      server_id_(server_id),
      config_(config),
      local_port_(0),
      helper_(CreateQuicConnectionHelper()),
      initialized_(false),
      supported_versions_(supported_versions),
      initial_max_packet_length_(0),
      num_stateless_rejects_received_(0),
      num_sent_client_hellos_(0),
      connection_error_(QUIC_NO_ERROR),
      connected_or_attempting_connect_(false),
      weak_factory_(this) {}

QuicSimpleClient::~QuicSimpleClient() {
  if (connected()) {
    session()->connection()->SendConnectionClosePacket(
        QUIC_PEER_GOING_AWAY, "");
  }
  STLDeleteElements(&data_to_resend_on_connect_);
  STLDeleteElements(&data_sent_before_handshake_);
}

bool QuicSimpleClient::Initialize() {
  DCHECK(!initialized_);

  num_sent_client_hellos_ = 0;
  num_stateless_rejects_received_ = 0;
  connection_error_ = QUIC_NO_ERROR;
  connected_or_attempting_connect_ = false;

  if (!CreateUDPSocket()) {
    return false;
  }

  initialized_ = true;
  return true;
}

QuicSimpleClient::DummyPacketWriterFactory::DummyPacketWriterFactory(
    QuicPacketWriter* writer)
    : writer_(writer) {}

QuicSimpleClient::DummyPacketWriterFactory::~DummyPacketWriterFactory() {}

QuicPacketWriter* QuicSimpleClient::DummyPacketWriterFactory::Create(
    QuicConnection* /*connection*/) const {
  return writer_;
}

QuicSimpleClient::QuicDataToResend::QuicDataToResend(HttpRequestInfo* headers,
                                                     StringPiece body,
                                                     bool fin)
    : headers_(headers), body_(body), fin_(fin) {}

QuicSimpleClient::QuicDataToResend::~QuicDataToResend() {
  if (headers_) {
    delete headers_;
  }
}

bool QuicSimpleClient::CreateUDPSocket() {
  scoped_ptr<UDPClientSocket> socket(
      new UDPClientSocket(DatagramSocket::DEFAULT_BIND,
                          RandIntCallback(),
                          &net_log_,
                          NetLog::Source()));

  int address_family = server_address_.GetSockAddrFamily();
  if (bind_to_address_.size() != 0) {
    client_address_ = IPEndPoint(bind_to_address_, local_port_);
  } else if (address_family == AF_INET) {
    IPAddressNumber any4;
    CHECK(net::ParseIPLiteralToNumber("0.0.0.0", &any4));
    client_address_ = IPEndPoint(any4, local_port_);
  } else {
    IPAddressNumber any6;
    CHECK(net::ParseIPLiteralToNumber("::", &any6));
    client_address_ = IPEndPoint(any6, local_port_);
  }

  int rc = socket->Connect(server_address_);
  if (rc != OK) {
    LOG(ERROR) << "Connect failed: " << ErrorToShortString(rc);
    return false;
  }

  rc = socket->SetReceiveBufferSize(kDefaultSocketReceiveBuffer);
  if (rc != OK) {
    LOG(ERROR) << "SetReceiveBufferSize() failed: " << ErrorToShortString(rc);
    return false;
  }

  rc = socket->SetSendBufferSize(kDefaultSocketReceiveBuffer);
  if (rc != OK) {
    LOG(ERROR) << "SetSendBufferSize() failed: " << ErrorToShortString(rc);
    return false;
  }

  rc = socket->GetLocalAddress(&client_address_);
  if (rc != OK) {
    LOG(ERROR) << "GetLocalAddress failed: " << ErrorToShortString(rc);
    return false;
  }

  socket_.swap(socket);
  packet_reader_.reset(new QuicPacketReader(socket_.get(), this,
                                            BoundNetLog()));

  if (socket != nullptr) {
    socket->Close();
  }

  return true;
}

bool QuicSimpleClient::Connect() {
  // Attempt multiple connects until the maximum number of client hellos have
  // been sent.
  while (!connected() &&
         GetNumSentClientHellos() <= QuicCryptoClientStream::kMaxClientHellos) {
    StartConnect();
    while (EncryptionBeingEstablished()) {
      WaitForEvents();
    }
    if (FLAGS_enable_quic_stateless_reject_support && connected() &&
        !data_to_resend_on_connect_.empty()) {
      // A connection has been established and there was previously queued data
      // to resend.  Resend it and empty the queue.
      for (QuicDataToResend* data : data_to_resend_on_connect_) {
        data->Resend();
      }
      STLDeleteElements(&data_to_resend_on_connect_);
    }
    if (session_.get() != nullptr &&
        session_->error() != QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
      // We've successfully created a session but we're not connected, and there
      // is no stateless reject to recover from.  Give up trying.
      break;
    }
  }
  if (!connected() &&
      GetNumSentClientHellos() > QuicCryptoClientStream::kMaxClientHellos &&
      session_ != nullptr &&
      session_->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
    // The overall connection failed due too many stateless rejects.
    connection_error_ = QUIC_CRYPTO_TOO_MANY_REJECTS;
  }
  return session_->connection()->connected();
}

QuicClientSession* QuicSimpleClient::CreateQuicClientSession(
    const QuicConfig& config,
    QuicConnection* connection,
    const QuicServerId& server_id,
    QuicCryptoClientConfig* crypto_config) {
  return new QuicClientSession(config, connection, server_id_, &crypto_config_);
}

void QuicSimpleClient::StartConnect() {
  DCHECK(initialized_);
  DCHECK(!connected());

  writer_.reset(CreateQuicPacketWriter());

  DummyPacketWriterFactory factory(writer_.get());

  if (connected_or_attempting_connect_) {
    // Before we destroy the last session and create a new one, gather its stats
    // and update the stats for the overall connection.
    num_sent_client_hellos_ += session_->GetNumSentClientHellos();
    if (session_->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
      // If the last error was due to a stateless reject, queue up the data to
      // be resent on the next successful connection.
      // TODO(jokulik): I'm a little bit concerned about ordering here.  Maybe
      // we should just maintain one queue?
      ++num_stateless_rejects_received_;
      DCHECK(data_to_resend_on_connect_.empty());
      data_to_resend_on_connect_.swap(data_sent_before_handshake_);
    }
  }

  session_.reset(CreateQuicClientSession(
      config_,
      new QuicConnection(GetNextConnectionId(), server_address_, helper_.get(),
                         factory,
                         /* owns_writer= */ false, Perspective::IS_CLIENT,
                         server_id_.is_https(), supported_versions_),
      server_id_, &crypto_config_));
  if (initial_max_packet_length_ != 0) {
    session_->connection()->set_max_packet_length(initial_max_packet_length_);
  }
  session_->Initialize();
  session_->CryptoConnect();
  connected_or_attempting_connect_ = true;
}

bool QuicSimpleClient::EncryptionBeingEstablished() {
  return !session_->IsEncryptionEstablished() &&
      session_->connection()->connected();
}

void QuicSimpleClient::Disconnect() {
  DCHECK(initialized_);

  if (connected()) {
    session()->connection()->SendConnectionClose(QUIC_PEER_GOING_AWAY);
  }
  STLDeleteElements(&data_to_resend_on_connect_);
  STLDeleteElements(&data_sent_before_handshake_);

  writer_.reset();
  packet_reader_.reset();

  initialized_ = false;
}

void QuicSimpleClient::SendRequest(const HttpRequestInfo& headers,
                                   base::StringPiece body,
                                   bool fin) {
  QuicSpdyClientStream* stream = CreateReliableClientStream();
  if (stream == nullptr) {
    LOG(DFATAL) << "stream creation failed!";
    return;
  }
  SpdyHeaderBlock header_block;
  SpdyMajorVersion spdy_version =
      SpdyUtils::GetSpdyVersionForQuicVersion(stream->version());
  CreateSpdyHeadersFromHttpRequest(headers, headers.extra_headers, spdy_version,
                                   true, &header_block);
  stream->set_visitor(this);
  stream->SendRequest(header_block, body, fin);
  if (FLAGS_enable_quic_stateless_reject_support) {
    // Record this in case we need to resend.
    auto new_headers = new HttpRequestInfo;
    *new_headers = headers;
    auto data_to_resend =
        new ClientQuicDataToResend(new_headers, body, fin, this);
    MaybeAddQuicDataToResend(data_to_resend);
  }
}

void QuicSimpleClient::MaybeAddQuicDataToResend(
    QuicDataToResend* data_to_resend) {
  DCHECK(FLAGS_enable_quic_stateless_reject_support);
  if (session_->IsCryptoHandshakeConfirmed()) {
    // The handshake is confirmed.  No need to continue saving requests to
    // resend.
    STLDeleteElements(&data_sent_before_handshake_);
    delete data_to_resend;
    return;
  }

  // The handshake is not confirmed.  Push the data onto the queue of data to
  // resend if statelessly rejected.
  data_sent_before_handshake_.push_back(data_to_resend);
}

void QuicSimpleClient::SendRequestAndWaitForResponse(
    const HttpRequestInfo& request,
    base::StringPiece body,
    bool fin) {
  SendRequest(request, body, fin);
  while (WaitForEvents()) {}
}

void QuicSimpleClient::SendRequestsAndWaitForResponse(
    const base::CommandLine::StringVector& url_list) {
  for (size_t i = 0; i < url_list.size(); ++i) {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL(url_list[i]);
    SendRequest(request, "", true);
  }

  while (WaitForEvents()) {}
}

QuicSpdyClientStream* QuicSimpleClient::CreateReliableClientStream() {
  if (!connected()) {
    return nullptr;
  }

  return session_->CreateOutgoingDynamicStream();
}

void QuicSimpleClient::WaitForStreamToClose(QuicStreamId id) {
  DCHECK(connected());

  while (connected() && !session_->IsClosedStream(id)) {
    WaitForEvents();
  }
}

void QuicSimpleClient::WaitForCryptoHandshakeConfirmed() {
  DCHECK(connected());

  while (connected() && !session_->IsCryptoHandshakeConfirmed()) {
    WaitForEvents();
  }
}

bool QuicSimpleClient::WaitForEvents() {
  DCHECK(connected());

  base::RunLoop().RunUntilIdle();

  DCHECK(session_ != nullptr);
  if (!connected() &&
      session_->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
    DCHECK(FLAGS_enable_quic_stateless_reject_support);
    DVLOG(1) << "Detected stateless reject while waiting for events.  "
             << "Attempting to reconnect.";
    Connect();
  }

  return session_->num_active_requests() != 0;
}

bool QuicSimpleClient::MigrateSocket(const IPAddressNumber& new_host) {
  if (!connected()) {
    return false;
  }

  bind_to_address_ = new_host;
  if (!CreateUDPSocket()) {
    return false;
  }

  session_->connection()->SetSelfAddress(client_address_);

  QuicPacketWriter* writer = CreateQuicPacketWriter();
  DummyPacketWriterFactory factory(writer);
  if (writer_.get() != writer) {
    writer_.reset(writer);
  }
  session_->connection()->SetQuicPacketWriter(writer, false);

  return true;
}

void QuicSimpleClient::OnClose(QuicDataStream* stream) {
  DCHECK(stream != nullptr);
  QuicSpdyClientStream* client_stream =
      static_cast<QuicSpdyClientStream*>(stream);
  HttpResponseInfo response;
  SpdyMajorVersion spdy_version =
      SpdyUtils::GetSpdyVersionForQuicVersion(client_stream->version());
  SpdyHeadersToHttpResponse(client_stream->headers(), spdy_version, &response);
  if (response_listener_.get() != nullptr) {
    response_listener_->OnCompleteResponse(
        stream->id(), *response.headers, client_stream->data());
  }

  // Store response headers and body.
  if (store_response_) {
    latest_response_code_ = client_stream->response_code();
    response.headers->GetNormalizedHeaders(&latest_response_headers_);
    latest_response_body_ = client_stream->data();
  }
}

bool QuicSimpleClient::connected() const {
  return session_.get() && session_->connection() &&
      session_->connection()->connected();
}

bool QuicSimpleClient::goaway_received() const {
  return session_ != nullptr && session_->goaway_received();
}

size_t QuicSimpleClient::latest_response_code() const {
  LOG_IF(DFATAL, !store_response_) << "Response not stored!";
  return latest_response_code_;
}

const string& QuicSimpleClient::latest_response_headers() const {
  LOG_IF(DFATAL, !store_response_) << "Response not stored!";
  return latest_response_headers_;
}

const string& QuicSimpleClient::latest_response_body() const {
  LOG_IF(DFATAL, !store_response_) << "Response not stored!";
  return latest_response_body_;
}

int QuicSimpleClient::GetNumSentClientHellos() {
  // If we are not actively attempting to connect, the session object
  // corresponds to the previous connection and should not be used.
  const int current_session_hellos = !connected_or_attempting_connect_
                                         ? 0
                                         : session_->GetNumSentClientHellos();
  return num_sent_client_hellos_ + current_session_hellos;
}

QuicErrorCode QuicSimpleClient::connection_error() const {
  // Return the high-level error if there was one.  Otherwise, return the
  // connection error from the last session.
  if (connection_error_ != QUIC_NO_ERROR) {
    return connection_error_;
  }
  if (session_.get() == nullptr) {
    return QUIC_NO_ERROR;
  }
  return session_->error();
}

QuicConnectionId QuicSimpleClient::GetNextConnectionId() {
  QuicConnectionId server_designated_id = GetNextServerDesignatedConnectionId();
  return server_designated_id ? server_designated_id
                              : GenerateNewConnectionId();
}

QuicConnectionId QuicSimpleClient::GetNextServerDesignatedConnectionId() {
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_.LookupOrCreate(server_id_);
  // If the cached state indicates that we should use a server-designated
  // connection ID, then return that connection ID.
  CHECK(cached != nullptr) << "QuicClientCryptoConfig::LookupOrCreate returned "
                           << "unexpected nullptr.";
  return cached->has_server_designated_connection_id()
             ? cached->GetNextServerDesignatedConnectionId()
             : 0;
}

QuicConnectionId QuicSimpleClient::GenerateNewConnectionId() {
  return helper_->GetRandomGenerator()->RandUint64();
}

QuicConnectionHelper* QuicSimpleClient::CreateQuicConnectionHelper() {
  return new QuicConnectionHelper(base::ThreadTaskRunnerHandle::Get().get(),
                                  &clock_, QuicRandom::GetInstance());
}

QuicPacketWriter* QuicSimpleClient::CreateQuicPacketWriter() {
  return new QuicDefaultPacketWriter(socket_.get());
}

void QuicSimpleClient::OnReadError(int result) {
  LOG(ERROR) << "QuicSimpleClient read failed: " << ErrorToShortString(result);
  Disconnect();
}

bool QuicSimpleClient::OnPacket(const QuicEncryptedPacket& packet,
                                IPEndPoint local_address,
                                IPEndPoint peer_address) {
  session_->connection()->ProcessUdpPacket(local_address, peer_address, packet);
  if (!session_->connection()->connected()) {
    return false;
  }

  return true;
}

}  // namespace tools
}  // namespace net
