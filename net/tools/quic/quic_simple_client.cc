// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_client.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/udp/udp_client_socket.h"

using std::string;
using std::vector;

namespace net {
namespace tools {

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
      weak_factory_(this) {
}

QuicSimpleClient::~QuicSimpleClient() {
  if (connected()) {
    session()->connection()->SendConnectionClosePacket(
        QUIC_PEER_GOING_AWAY, "");
  }
}

bool QuicSimpleClient::Initialize() {
  DCHECK(!initialized_);

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
    LOG(ERROR) << "Connect failed: " << ErrorToString(rc);
    return false;
  }

  rc = socket->SetReceiveBufferSize(kDefaultSocketReceiveBuffer);
  if (rc != OK) {
    LOG(ERROR) << "SetReceiveBufferSize() failed: " << ErrorToString(rc);
    return false;
  }

  rc = socket->SetSendBufferSize(kDefaultSocketReceiveBuffer);
  if (rc != OK) {
    LOG(ERROR) << "SetSendBufferSize() failed: " << ErrorToString(rc);
    return false;
  }

  rc = socket->GetLocalAddress(&client_address_);
  if (rc != OK) {
    LOG(ERROR) << "GetLocalAddress failed: " << ErrorToString(rc);
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
  StartConnect();
  packet_reader_->StartReading();
  while (EncryptionBeingEstablished()) {
    WaitForEvents();
  }
  return session_->connection()->connected();
}

void QuicSimpleClient::StartConnect() {
  DCHECK(initialized_);
  DCHECK(!connected());

  writer_.reset(CreateQuicPacketWriter());
  connection_ = new QuicConnection(GenerateConnectionId(),
                                   server_address_,
                                   helper_.get(),
                                   DummyPacketWriterFactory(writer_.get()),
                                   /* owns_writer= */ false,
                                   Perspective::IS_CLIENT,
                                   server_id_.is_https(),
                                   supported_versions_);
  session_.reset(new QuicSimpleClientSession(config_, connection_));
  session_->InitializeSession(server_id_, &crypto_config_);
  session_->CryptoConnect();
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

  writer_.reset();
  packet_reader_.reset();

  initialized_ = false;
}

void QuicSimpleClient::SendRequest(const HttpRequestInfo& headers,
                                   base::StringPiece body,
                                   bool fin) {
  QuicSimpleClientStream* stream = CreateReliableClientStream();
  if (stream == nullptr) {
    LOG(DFATAL) << "stream creation failed!";
    return;
  }
  stream->SendRequest(headers, body, fin);
  stream->set_visitor(this);
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

QuicSimpleClientStream* QuicSimpleClient::CreateReliableClientStream() {
  if (!connected()) {
    return nullptr;
  }

  return session_->CreateOutgoingDataStream();
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
  return session_->num_active_requests() != 0;
}

void QuicSimpleClient::OnClose(QuicDataStream* stream) {
  QuicSimpleClientStream* client_stream =
      static_cast<QuicSimpleClientStream*>(stream);
  if (response_listener_.get() != nullptr) {
    response_listener_->OnCompleteResponse(
        stream->id(), *client_stream->headers(), client_stream->data());
  }

  // Store response headers and body.
  if (store_response_) {
    latest_response_code_ = client_stream->headers()->response_code();
    client_stream->headers()->GetNormalizedHeaders(&latest_response_headers_);
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

QuicConnectionId QuicSimpleClient::GenerateConnectionId() {
  return helper_->GetRandomGenerator()->RandUint64();
}

QuicConnectionHelper* QuicSimpleClient::CreateQuicConnectionHelper() {
  return new QuicConnectionHelper(
      base::MessageLoop::current()->message_loop_proxy().get(),
      &clock_,
      QuicRandom::GetInstance());
}

QuicPacketWriter* QuicSimpleClient::CreateQuicPacketWriter() {
  return new QuicDefaultPacketWriter(socket_.get());
}

void QuicSimpleClient::OnReadError(int result) {
  LOG(ERROR) << "QuicSimpleClient read failed: " << ErrorToString(result);
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
