// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/quic_channel_factory.h"

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/p2p/quic_p2p_crypto_config.h"
#include "net/quic/p2p/quic_p2p_session.h"
#include "net/quic/p2p/quic_p2p_stream.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/datagram_channel_factory.h"
#include "remoting/protocol/p2p_datagram_socket.h"
#include "remoting/protocol/quic_channel.h"

namespace remoting {
namespace protocol {

namespace {

// The maximum receive window sizes for QUIC sessions and streams. These are
// the same values that are used in chrome.
const int kQuicSessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
const int kQuicStreamMaxRecvWindowSize = 6 * 1024 * 1024;    // 6 MB

class P2PQuicPacketWriter : public net::QuicPacketWriter {
 public:
  P2PQuicPacketWriter(net::QuicConnection* connection,
                      P2PDatagramSocket* socket)
      : connection_(connection), socket_(socket), weak_factory_(this) {}
  ~P2PQuicPacketWriter() override {}

  // QuicPacketWriter interface.
  net::WriteResult WritePacket(const char* buffer,
                               size_t buf_len,
                               const net::IPAddressNumber& self_address,
                               const net::IPEndPoint& peer_address) override {
    DCHECK(!write_blocked_);

    scoped_refptr<net::StringIOBuffer> buf(
        new net::StringIOBuffer(std::string(buffer, buf_len)));
    int result = socket_->Send(buf, buf_len,
                               base::Bind(&P2PQuicPacketWriter::OnSendComplete,
                                          weak_factory_.GetWeakPtr()));
    net::WriteStatus status = net::WRITE_STATUS_OK;
    if (result < 0) {
      if (result == net::ERR_IO_PENDING) {
        status = net::WRITE_STATUS_BLOCKED;
        write_blocked_ = true;
      } else {
        status = net::WRITE_STATUS_ERROR;
      }
    }

    return net::WriteResult(status, result);
  }
  bool IsWriteBlockedDataBuffered() const override {
    // P2PDatagramSocket::Send() method buffer the data until the Send is
    // unblocked.
    return true;
  }
  bool IsWriteBlocked() const override { return write_blocked_; }
  void SetWritable() override { write_blocked_ = false; }
  net::QuicByteCount GetMaxPacketSize(const net::IPEndPoint& peer_address) const
      override {
    return net::kMaxPacketSize;
  }

 private:
  void OnSendComplete(int result){
    DCHECK_NE(result, net::ERR_IO_PENDING);
    write_blocked_ = false;
    if (result < 0) {
      connection_->OnWriteError(result);
    }
    connection_->OnCanWrite();
  }

  net::QuicConnection* connection_;
  P2PDatagramSocket* socket_;

  // Whether a write is currently in flight.
  bool write_blocked_ = false;

  base::WeakPtrFactory<P2PQuicPacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(P2PQuicPacketWriter);
};

class QuicPacketWriterFactory
    : public net::QuicConnection::PacketWriterFactory {
 public:
  explicit QuicPacketWriterFactory(P2PDatagramSocket* socket)
      : socket_(socket) {}
  ~QuicPacketWriterFactory() override {}

  net::QuicPacketWriter* Create(
      net::QuicConnection* connection) const override {
    return new P2PQuicPacketWriter(connection, socket_);
  }

 private:
  P2PDatagramSocket* socket_;
};

class P2PDatagramSocketAdapter : public net::Socket {
 public:
  explicit P2PDatagramSocketAdapter(scoped_ptr<P2PDatagramSocket> socket)
      : socket_(socket.Pass()) {}
  ~P2PDatagramSocketAdapter() override {}

  int Read(net::IOBuffer* buf, int buf_len,
           const net::CompletionCallback& callback) override {
    return socket_->Recv(buf, buf_len, callback);
  }
  int Write(net::IOBuffer* buf, int buf_len,
            const net::CompletionCallback& callback) override {
    return socket_->Send(buf, buf_len, callback);
  }

  int SetReceiveBufferSize(int32_t size) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }

  int SetSendBufferSize(int32_t size) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }

 private:
  scoped_ptr<P2PDatagramSocket> socket_;
};

}  // namespace

class QuicChannelFactory::Core : public net::QuicP2PSession::Delegate {
 public:
  Core(const std::string& session_id, bool is_server);
  virtual ~Core();

  // Called from ~QuicChannelFactory() to synchronously release underlying
  // socket. Core is destroyed later asynchronously.
  void Close();

  // Implementation of all all methods for QuicChannelFactory.
  const std::string& CreateSessionInitiateConfigMessage();
  bool ProcessSessionAcceptConfigMessage(const std::string& message);

  bool ProcessSessionInitiateConfigMessage(const std::string& message);
  const std::string& CreateSessionAcceptConfigMessage();

  void Start(DatagramChannelFactory* factory, const std::string& shared_secret);

  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback);
  void CancelChannelCreation(const std::string& name);

 private:
  friend class QuicChannelFactory;

  struct PendingChannel {
    PendingChannel(const std::string& name,
                   const ChannelCreatedCallback& callback)
        : name(name), callback(callback) {}

    std::string name;
    ChannelCreatedCallback callback;
  };

  // QuicP2PSession::Delegate interface.
  void OnIncomingStream(net::QuicP2PStream* stream) override;
  void OnConnectionClosed(net::QuicErrorCode error) override;

  void OnBaseChannelReady(scoped_ptr<P2PDatagramSocket> socket);

  void OnNameReceived(QuicChannel* channel);

  void OnChannelDestroyed(int stream_id);

  std::string session_id_;
  bool is_server_;
  DatagramChannelFactory* base_channel_factory_ = nullptr;

  net::QuicConfig quic_config_;
  std::string shared_secret_;
  std::string session_initiate_quic_config_message_;
  std::string session_accept_quic_config_message_;

  net::QuicClock quic_clock_;
  net::QuicConnectionHelper quic_helper_;
  scoped_ptr<net::QuicP2PSession> quic_session_;
  bool connected_ = false;

  std::vector<PendingChannel*> pending_channels_;
  std::vector<QuicChannel*> unnamed_incoming_channels_;

  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

QuicChannelFactory::Core::Core(const std::string& session_id, bool is_server)
    : session_id_(session_id),
      is_server_(is_server),
      quic_helper_(base::ThreadTaskRunnerHandle::Get().get(),
                   &quic_clock_,
                   net::QuicRandom::GetInstance()),
      weak_factory_(this) {
  quic_config_.SetInitialSessionFlowControlWindowToSend(
      kQuicSessionMaxRecvWindowSize);
  quic_config_.SetInitialStreamFlowControlWindowToSend(
      kQuicStreamMaxRecvWindowSize);
}

QuicChannelFactory::Core::~Core() {}

void QuicChannelFactory::Core::Close() {
  DCHECK(pending_channels_.empty());

  // Cancel creation of the base channel if it hasn't finished.
  if (base_channel_factory_)
    base_channel_factory_->CancelChannelCreation(kQuicChannelName);

  if (quic_session_ && quic_session_->connection()->connected())
    quic_session_->connection()->CloseConnection(net::QUIC_NO_ERROR, false);

  DCHECK(unnamed_incoming_channels_.empty());
}

void QuicChannelFactory::Core::Start(DatagramChannelFactory* factory,
                                     const std::string& shared_secret) {
  base_channel_factory_ = factory;
  shared_secret_ = shared_secret;

  base_channel_factory_->CreateChannel(
      kQuicChannelName,
      base::Bind(&Core::OnBaseChannelReady, weak_factory_.GetWeakPtr()));
}

const std::string&
QuicChannelFactory::Core::CreateSessionInitiateConfigMessage() {
  DCHECK(!is_server_);

  net::CryptoHandshakeMessage handshake_message;
  handshake_message.set_tag(net::kCHLO);
  quic_config_.ToHandshakeMessage(&handshake_message);

  session_initiate_quic_config_message_ =
      handshake_message.GetSerialized().AsStringPiece().as_string();
  return session_initiate_quic_config_message_;
}

bool QuicChannelFactory::Core::ProcessSessionAcceptConfigMessage(
    const std::string& message) {
  DCHECK(!is_server_);

  session_accept_quic_config_message_ = message;

  scoped_ptr<net::CryptoHandshakeMessage> parsed_message(
      net::CryptoFramer::ParseMessage(message));
  if (!parsed_message) {
    LOG(ERROR) << "Received invalid QUIC config.";
    return false;
  }

  if (parsed_message->tag() != net::kSHLO) {
    LOG(ERROR) << "Received QUIC handshake message with unexpected tag "
               << parsed_message->tag();
    return false;
  }

  std::string error_message;
  net::QuicErrorCode error = quic_config_.ProcessPeerHello(
      *parsed_message, net::SERVER, &error_message);
  if (error != net::QUIC_NO_ERROR) {
    LOG(ERROR) << "Failed to process QUIC handshake message: "
               << error_message;
    return false;
  }

  return true;
}

bool QuicChannelFactory::Core::ProcessSessionInitiateConfigMessage(
    const std::string& message) {
  DCHECK(is_server_);

  session_initiate_quic_config_message_ = message;

  scoped_ptr<net::CryptoHandshakeMessage> parsed_message(
      net::CryptoFramer::ParseMessage(message));
  if (!parsed_message) {
    LOG(ERROR) << "Received invalid QUIC config.";
    return false;
  }

  if (parsed_message->tag() != net::kCHLO) {
    LOG(ERROR) << "Received QUIC handshake message with unexpected tag "
               << parsed_message->tag();
    return false;
  }

  std::string error_message;
  net::QuicErrorCode error = quic_config_.ProcessPeerHello(
      *parsed_message, net::CLIENT, &error_message);
  if (error != net::QUIC_NO_ERROR) {
    LOG(ERROR) << "Failed to process QUIC handshake message: "
               << error_message;
    return false;
  }

  return true;
}

const std::string&
QuicChannelFactory::Core::CreateSessionAcceptConfigMessage() {
  DCHECK(is_server_);

  if (session_initiate_quic_config_message_.empty()) {
    // Don't send quic-config to the client if the client didn't include the
    // config in the session-initiate message.
    DCHECK(session_accept_quic_config_message_.empty());
    return session_accept_quic_config_message_;
  }

  net::CryptoHandshakeMessage handshake_message;
  handshake_message.set_tag(net::kSHLO);
  quic_config_.ToHandshakeMessage(&handshake_message);

  session_accept_quic_config_message_ =
      handshake_message.GetSerialized().AsStringPiece().as_string();
  return session_accept_quic_config_message_;
}

// StreamChannelFactory interface.
void QuicChannelFactory::Core::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  if (quic_session_ && quic_session_->connection()->connected()) {
    if (!is_server_) {
      net::QuicP2PStream* stream = quic_session_->CreateOutgoingDynamicStream();
      scoped_ptr<QuicChannel> channel(new QuicClientChannel(
          stream, base::Bind(&Core::OnChannelDestroyed, base::Unretained(this),
                             stream->id()),
          name));
      callback.Run(channel.Pass());
    } else {
      // On the server side wait for the client to create a QUIC stream and
      // send the name. The channel will be connected in OnNameReceived().
      pending_channels_.push_back(new PendingChannel(name, callback));
    }
  } else if (!base_channel_factory_) {
    // Fail synchronously if we failed to connect transport.
    callback.Run(nullptr);
  } else {
    // Still waiting for the transport.
    pending_channels_.push_back(new PendingChannel(name, callback));
  }
}

void QuicChannelFactory::Core::CancelChannelCreation(const std::string& name) {
  for (auto it = pending_channels_.begin(); it != pending_channels_.end();
       ++it) {
    if ((*it)->name == name) {
      delete *it;
      pending_channels_.erase(it);
      return;
    }
  }
}

void QuicChannelFactory::Core::OnBaseChannelReady(
    scoped_ptr<P2PDatagramSocket> socket) {
  base_channel_factory_ = nullptr;

  // Failed to connect underlying transport connection. Fail all pending
  // channel.
  if (!socket) {
    while (!pending_channels_.empty()) {
      scoped_ptr<PendingChannel> pending_channel(pending_channels_.front());
      pending_channels_.erase(pending_channels_.begin());
      pending_channel->callback.Run(nullptr);
    }
    return;
  }

  QuicPacketWriterFactory writer_factory(socket.get());
  net::IPAddressNumber ip(net::kIPv4AddressSize, 0);
  scoped_ptr<net::QuicConnection> quic_connection(new net::QuicConnection(
      0, net::IPEndPoint(ip, 0), &quic_helper_, writer_factory,
      true /* owns_writer */,
      is_server_ ? net::Perspective::IS_SERVER : net::Perspective::IS_CLIENT,
      net::QuicSupportedVersions()));

  net::QuicP2PCryptoConfig quic_crypto_config(shared_secret_);
  quic_crypto_config.set_hkdf_input_suffix(
      session_id_ + '\0' + kQuicChannelName + '\0' +
      session_initiate_quic_config_message_ +
      session_accept_quic_config_message_);

  quic_session_.reset(new net::QuicP2PSession(
      quic_config_, quic_crypto_config, quic_connection.Pass(),
      make_scoped_ptr(new P2PDatagramSocketAdapter(socket.Pass()))));
  quic_session_->SetDelegate(this);
  quic_session_->Initialize();

  if (!is_server_) {
    // On the client create streams for all pending channels and send a name for
    // each channel.
    while (!pending_channels_.empty()) {
      scoped_ptr<PendingChannel> pending_channel(pending_channels_.front());
      pending_channels_.erase(pending_channels_.begin());

      net::QuicP2PStream* stream = quic_session_->CreateOutgoingDynamicStream();
      scoped_ptr<QuicChannel> channel(new QuicClientChannel(
          stream, base::Bind(&Core::OnChannelDestroyed, base::Unretained(this),
                             stream->id()),
          pending_channel->name));
      pending_channel->callback.Run(channel.Pass());
    }
  }
}

void QuicChannelFactory::Core::OnIncomingStream(net::QuicP2PStream* stream) {
  QuicServerChannel* channel = new QuicServerChannel(
      stream, base::Bind(&Core::OnChannelDestroyed, base::Unretained(this),
                         stream->id()));
  unnamed_incoming_channels_.push_back(channel);
  channel->ReceiveName(
      base::Bind(&Core::OnNameReceived, base::Unretained(this), channel));
}

void QuicChannelFactory::Core::OnConnectionClosed(net::QuicErrorCode error) {
  if (error != net::QUIC_NO_ERROR)
    LOG(ERROR) << "QUIC connection was closed, error_code=" << error;

  while (!pending_channels_.empty()) {
    scoped_ptr<PendingChannel> pending_channel(pending_channels_.front());
    pending_channels_.erase(pending_channels_.begin());
    pending_channel->callback.Run(nullptr);
  }
}

void QuicChannelFactory::Core::OnNameReceived(QuicChannel* channel) {
  DCHECK(is_server_);

  scoped_ptr<QuicChannel> owned_channel(channel);

  auto it = std::find(unnamed_incoming_channels_.begin(),
                      unnamed_incoming_channels_.end(), channel);
  DCHECK(it != unnamed_incoming_channels_.end());
  unnamed_incoming_channels_.erase(it);

  if (channel->name().empty()) {
    // Failed to read a name for incoming channel.
    return;
  }

  for (auto it = pending_channels_.begin();
       it != pending_channels_.end(); ++it) {
    if ((*it)->name == channel->name()) {
      scoped_ptr<PendingChannel> pending_channel(*it);
      pending_channels_.erase(it);
      pending_channel->callback.Run(owned_channel.Pass());
      return;
    }
  }

  LOG(ERROR) << "Unexpected incoming channel: " << channel->name();
}

void QuicChannelFactory::Core::OnChannelDestroyed(int stream_id) {
  if (quic_session_)
    quic_session_->CloseStream(stream_id);
}

QuicChannelFactory::QuicChannelFactory(const std::string& session_id,
                                       bool is_server)
    : core_(new Core(session_id, is_server)) {}

QuicChannelFactory::~QuicChannelFactory() {
  core_->Close();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, core_.release());
}

const std::string& QuicChannelFactory::CreateSessionInitiateConfigMessage() {
  return core_->CreateSessionInitiateConfigMessage();
}

bool QuicChannelFactory::ProcessSessionAcceptConfigMessage(
    const std::string& message) {
  return core_->ProcessSessionAcceptConfigMessage(message);
}

bool QuicChannelFactory::ProcessSessionInitiateConfigMessage(
    const std::string& message) {
  return core_->ProcessSessionInitiateConfigMessage(message);
}

const std::string& QuicChannelFactory::CreateSessionAcceptConfigMessage() {
  return core_->CreateSessionAcceptConfigMessage();
}

void QuicChannelFactory::Start(DatagramChannelFactory* factory,
                               const std::string& shared_secret) {
  core_->Start(factory, shared_secret);
}

void QuicChannelFactory::CreateChannel(const std::string& name,
                                       const ChannelCreatedCallback& callback) {
  core_->CreateChannel(name, callback);
}

void QuicChannelFactory::CancelChannelCreation(const std::string& name) {
  core_->CancelChannelCreation(name);
}

net::QuicP2PSession* QuicChannelFactory::GetP2PSessionForTests() {
  return core_->quic_session_.get();
}

}  // namespace protocol
}  // namespace remoting
