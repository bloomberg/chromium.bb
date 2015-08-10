// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/logging.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_socket_utils.h"
#include "net/tools/quic/quic_spdy_client_stream.h"
#include "net/tools/quic/spdy_balsa_utils.h"

#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif

using std::string;
using std::vector;

namespace net {
namespace tools {

const int kEpollFlags = EPOLLIN | EPOLLOUT | EPOLLET;

void QuicClient::ClientQuicDataToResend::Resend() {
  client_->SendRequest(*headers_, body_, fin_);
  delete headers_;
  headers_ = nullptr;
}

QuicClient::QuicClient(IPEndPoint server_address,
                       const QuicServerId& server_id,
                       const QuicVersionVector& supported_versions,
                       EpollServer* epoll_server)
    : QuicClient(server_address,
                 server_id,
                 supported_versions,
                 QuicConfig(),
                 epoll_server) {}

QuicClient::QuicClient(IPEndPoint server_address,
                       const QuicServerId& server_id,
                       const QuicVersionVector& supported_versions,
                       const QuicConfig& config,
                       EpollServer* epoll_server)
    : server_address_(server_address),
      server_id_(server_id),
      config_(config),
      local_port_(0),
      epoll_server_(epoll_server),
      fd_(-1),
      helper_(CreateQuicConnectionHelper()),
      initialized_(false),
      packets_dropped_(0),
      overflow_supported_(false),
      supported_versions_(supported_versions),
      store_response_(false),
      latest_response_code_(-1),
      initial_max_packet_length_(0),
      num_stateless_rejects_received_(0),
      num_sent_client_hellos_(0),
      connection_error_(QUIC_NO_ERROR),
      connected_or_attempting_connect_(false) {}

QuicClient::~QuicClient() {
  if (connected()) {
    session()->connection()->SendConnectionClose(QUIC_PEER_GOING_AWAY);
  }
  STLDeleteElements(&data_to_resend_on_connect_);
  STLDeleteElements(&data_sent_before_handshake_);

  CleanUpUDPSocketImpl();
}

bool QuicClient::Initialize() {
  DCHECK(!initialized_);

  num_sent_client_hellos_ = 0;
  num_stateless_rejects_received_ = 0;
  connection_error_ = QUIC_NO_ERROR;
  connected_or_attempting_connect_ = false;

  // If an initial flow control window has not explicitly been set, then use the
  // same values that Chrome uses.
  const uint32 kSessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
  const uint32 kStreamMaxRecvWindowSize = 6 * 1024 * 1024;    //  6 MB
  if (config_.GetInitialStreamFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config_.SetInitialStreamFlowControlWindowToSend(kStreamMaxRecvWindowSize);
  }
  if (config_.GetInitialSessionFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config_.SetInitialSessionFlowControlWindowToSend(kSessionMaxRecvWindowSize);
  }

  epoll_server_->set_timeout_in_us(50 * 1000);

  if (!CreateUDPSocket()) {
    return false;
  }

  epoll_server_->RegisterFD(fd_, this, kEpollFlags);
  initialized_ = true;
  return true;
}

QuicClient::DummyPacketWriterFactory::DummyPacketWriterFactory(
    QuicPacketWriter* writer)
    : writer_(writer) {}

QuicClient::DummyPacketWriterFactory::~DummyPacketWriterFactory() {}

QuicPacketWriter* QuicClient::DummyPacketWriterFactory::Create(
    QuicConnection* /*connection*/) const {
  return writer_;
}

QuicClient::QuicDataToResend::QuicDataToResend(BalsaHeaders* headers,
                                               StringPiece body,
                                               bool fin)
    : headers_(headers), body_(body), fin_(fin) {}

QuicClient::QuicDataToResend::~QuicDataToResend() {
  if (headers_) {
    delete headers_;
  }
}

bool QuicClient::CreateUDPSocket() {
  int address_family = server_address_.GetSockAddrFamily();
  fd_ = socket(address_family, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (fd_ < 0) {
    LOG(ERROR) << "CreateSocket() failed: " << strerror(errno);
    return false;
  }

  int get_overflow = 1;
  int rc = setsockopt(fd_, SOL_SOCKET, SO_RXQ_OVFL, &get_overflow,
                      sizeof(get_overflow));
  if (rc < 0) {
    DLOG(WARNING) << "Socket overflow detection not supported";
  } else {
    overflow_supported_ = true;
  }

  if (!QuicSocketUtils::SetReceiveBufferSize(fd_,
                                             kDefaultSocketReceiveBuffer)) {
    return false;
  }

  if (!QuicSocketUtils::SetSendBufferSize(fd_, kDefaultSocketReceiveBuffer)) {
    return false;
  }

  rc = QuicSocketUtils::SetGetAddressInfo(fd_, address_family);
  if (rc < 0) {
    LOG(ERROR) << "IP detection not supported" << strerror(errno);
    return false;
  }

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

  sockaddr_storage raw_addr;
  socklen_t raw_addr_len = sizeof(raw_addr);
  CHECK(client_address_.ToSockAddr(reinterpret_cast<sockaddr*>(&raw_addr),
                           &raw_addr_len));
  rc = bind(fd_,
            reinterpret_cast<const sockaddr*>(&raw_addr),
            sizeof(raw_addr));
  if (rc < 0) {
    LOG(ERROR) << "Bind failed: " << strerror(errno);
    return false;
  }

  SockaddrStorage storage;
  if (getsockname(fd_, storage.addr, &storage.addr_len) != 0 ||
      !client_address_.FromSockAddr(storage.addr, storage.addr_len)) {
    LOG(ERROR) << "Unable to get self address.  Error: " << strerror(errno);
  }

  return true;
}

bool QuicClient::Connect() {
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

QuicClientSession* QuicClient::CreateQuicClientSession(
    const QuicConfig& config,
    QuicConnection* connection,
    const QuicServerId& server_id,
    QuicCryptoClientConfig* crypto_config) {
  return new QuicClientSession(config, connection, server_id_, &crypto_config_);
}

void QuicClient::StartConnect() {
  DCHECK(initialized_);
  DCHECK(!connected());

  QuicPacketWriter* writer = CreateQuicPacketWriter();

  DummyPacketWriterFactory factory(writer);

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

  // Reset |writer_| after |session_| so that the old writer outlives the old
  // session.
  if (writer_.get() != writer) {
    writer_.reset(writer);
  }
  session_->Initialize();
  session_->CryptoConnect();
  connected_or_attempting_connect_ = true;
}

bool QuicClient::EncryptionBeingEstablished() {
  return !session_->IsEncryptionEstablished() &&
      session_->connection()->connected();
}

void QuicClient::Disconnect() {
  DCHECK(initialized_);

  if (connected()) {
    session()->connection()->SendConnectionClose(QUIC_PEER_GOING_AWAY);
  }
  STLDeleteElements(&data_to_resend_on_connect_);
  STLDeleteElements(&data_sent_before_handshake_);

  CleanUpUDPSocket();

  initialized_ = false;
}

void QuicClient::CleanUpUDPSocket() {
  CleanUpUDPSocketImpl();
}

void QuicClient::CleanUpUDPSocketImpl() {
  if (fd_ > -1) {
    epoll_server_->UnregisterFD(fd_);
    int rc = close(fd_);
    DCHECK_EQ(0, rc);
    fd_ = -1;
  }
}

void QuicClient::SendRequest(const BalsaHeaders& headers,
                             StringPiece body,
                             bool fin) {
  QuicSpdyClientStream* stream = CreateReliableClientStream();
  if (stream == nullptr) {
    LOG(DFATAL) << "stream creation failed!";
    return;
  }
  stream->set_visitor(this);
  stream->SendRequest(
      SpdyBalsaUtils::RequestHeadersToSpdyHeaders(headers, stream->version()),
      body, fin);
  if (FLAGS_enable_quic_stateless_reject_support) {
    // Record this in case we need to resend.
    auto new_headers = new BalsaHeaders;
    new_headers->CopyFrom(headers);
    auto data_to_resend =
        new ClientQuicDataToResend(new_headers, body, fin, this);
    MaybeAddQuicDataToResend(data_to_resend);
  }
}

void QuicClient::MaybeAddQuicDataToResend(QuicDataToResend* data_to_resend) {
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

void QuicClient::SendRequestAndWaitForResponse(
    const BalsaHeaders& headers,
    StringPiece body,
    bool fin) {
  SendRequest(headers, body, fin);
  while (WaitForEvents()) {}
}

void QuicClient::SendRequestsAndWaitForResponse(
    const vector<string>& url_list) {
  for (size_t i = 0; i < url_list.size(); ++i) {
    BalsaHeaders headers;
    headers.SetRequestFirstlineFromStringPieces("GET", url_list[i], "HTTP/1.1");
    SendRequest(headers, "", true);
  }
  while (WaitForEvents()) {}
}

QuicSpdyClientStream* QuicClient::CreateReliableClientStream() {
  if (!connected()) {
    return nullptr;
  }

  return session_->CreateOutgoingDynamicStream();
}

void QuicClient::WaitForStreamToClose(QuicStreamId id) {
  DCHECK(connected());

  while (connected() && !session_->IsClosedStream(id)) {
    WaitForEvents();
  }
}

void QuicClient::WaitForCryptoHandshakeConfirmed() {
  DCHECK(connected());

  while (connected() && !session_->IsCryptoHandshakeConfirmed()) {
    WaitForEvents();
  }
}

bool QuicClient::WaitForEvents() {
  DCHECK(connected());

  epoll_server_->WaitForEventsAndExecuteCallbacks();

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

bool QuicClient::MigrateSocket(const IPAddressNumber& new_host) {
  if (!connected()) {
    return false;
  }

  CleanUpUDPSocket();

  bind_to_address_ = new_host;
  if (!CreateUDPSocket()) {
    return false;
  }

  epoll_server_->RegisterFD(fd_, this, kEpollFlags);
  session_->connection()->SetSelfAddress(client_address_);

  QuicPacketWriter* writer = CreateQuicPacketWriter();
  DummyPacketWriterFactory factory(writer);
  if (writer_.get() != writer) {
    writer_.reset(writer);
  }
  session_->connection()->SetQuicPacketWriter(writer, false);

  return true;
}

void QuicClient::OnEvent(int fd, EpollEvent* event) {
  DCHECK_EQ(fd, fd_);

  if (event->in_events & EPOLLIN) {
    while (connected() && ReadAndProcessPacket()) {
    }
  }
  if (connected() && (event->in_events & EPOLLOUT)) {
    writer_->SetWritable();
    session_->connection()->OnCanWrite();
  }
  if (event->in_events & EPOLLERR) {
    DVLOG(1) << "Epollerr";
  }
}

void QuicClient::OnClose(QuicDataStream* stream) {
  DCHECK(stream != nullptr);
  QuicSpdyClientStream* client_stream =
      static_cast<QuicSpdyClientStream*>(stream);
  BalsaHeaders headers;
  SpdyBalsaUtils::SpdyHeadersToResponseHeaders(client_stream->headers(),
                                               &headers, stream->version());

  if (response_listener_.get() != nullptr) {
    response_listener_->OnCompleteResponse(
        stream->id(), headers, client_stream->data());
  }

  // Store response headers and body.
  if (store_response_) {
    latest_response_code_ = headers.parsed_response_code();
    headers.DumpHeadersToString(&latest_response_headers_);
    latest_response_body_ = client_stream->data();
  }
}

bool QuicClient::connected() const {
  return session_.get() && session_->connection() &&
      session_->connection()->connected();
}

bool QuicClient::goaway_received() const {
  return session_ != nullptr && session_->goaway_received();
}

size_t QuicClient::latest_response_code() const {
  LOG_IF(DFATAL, !store_response_) << "Response not stored!";
  return latest_response_code_;
}

const string& QuicClient::latest_response_headers() const {
  LOG_IF(DFATAL, !store_response_) << "Response not stored!";
  return latest_response_headers_;
}

const string& QuicClient::latest_response_body() const {
  LOG_IF(DFATAL, !store_response_) << "Response not stored!";
  return latest_response_body_;
}

int QuicClient::GetNumSentClientHellos() {
  // If we are not actively attempting to connect, the session object
  // corresponds to the previous connection and should not be used.
  const int current_session_hellos = !connected_or_attempting_connect_
                                         ? 0
                                         : session_->GetNumSentClientHellos();
  return num_sent_client_hellos_ + current_session_hellos;
}

QuicErrorCode QuicClient::connection_error() const {
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

QuicConnectionId QuicClient::GetNextConnectionId() {
  QuicConnectionId server_designated_id = GetNextServerDesignatedConnectionId();
  return server_designated_id ? server_designated_id
                              : GenerateNewConnectionId();
}

QuicConnectionId QuicClient::GetNextServerDesignatedConnectionId() {
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

QuicConnectionId QuicClient::GenerateNewConnectionId() {
  return QuicRandom::GetInstance()->RandUint64();
}

QuicEpollConnectionHelper* QuicClient::CreateQuicConnectionHelper() {
  return new QuicEpollConnectionHelper(epoll_server_);
}

QuicPacketWriter* QuicClient::CreateQuicPacketWriter() {
  return new QuicDefaultPacketWriter(fd_);
}

int QuicClient::ReadPacket(char* buffer,
                           int buffer_len,
                           IPEndPoint* server_address,
                           IPAddressNumber* client_ip) {
  return QuicSocketUtils::ReadPacket(
      fd_, buffer, buffer_len,
      overflow_supported_ ? &packets_dropped_ : nullptr, client_ip,
      server_address);
}

bool QuicClient::ReadAndProcessPacket() {
  // Allocate some extra space so we can send an error if the server goes over
  // the limit.
  char buf[2 * kMaxPacketSize];

  IPEndPoint server_address;
  IPAddressNumber client_ip;

  int bytes_read = ReadPacket(buf, arraysize(buf), &server_address, &client_ip);

  if (bytes_read < 0) {
    return false;
  }

  QuicEncryptedPacket packet(buf, bytes_read, false);

  IPEndPoint client_address(client_ip, client_address_.port());
  session_->connection()->ProcessUdpPacket(
      client_address, server_address, packet);
  return true;
}

}  // namespace tools
}  // namespace net
