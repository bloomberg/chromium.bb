// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ssl_hmac_channel_authenticator.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "crypto/secure_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_server_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_server_config.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/p2p_stream_socket.h"

#if defined(OS_NACL)
#include "net/socket/ssl_client_socket_impl.h"
#else
#include "net/socket/client_socket_factory.h"
#endif

namespace remoting {
namespace protocol {

namespace {

// A CertVerifier which rejects every certificate.
class FailingCertVerifier : public net::CertVerifier {
 public:
  FailingCertVerifier() {}
  ~FailingCertVerifier() override {}

  int Verify(net::X509Certificate* cert,
             const std::string& hostname,
             const std::string& ocsp_response,
             int flags,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<Request>* out_req,
             const net::BoundNetLog& net_log) override {
    verify_result->verified_cert = cert;
    verify_result->cert_status = net::CERT_STATUS_INVALID;
    return net::ERR_CERT_INVALID;
  }
};

// Implements net::StreamSocket interface on top of P2PStreamSocket to be passed
// to net::SSLClientSocket and net::SSLServerSocket.
class NetStreamSocketAdapter : public net::StreamSocket {
 public:
  NetStreamSocketAdapter(std::unique_ptr<P2PStreamSocket> socket)
      : socket_(std::move(socket)) {}
  ~NetStreamSocketAdapter() override {}

  int Read(net::IOBuffer* buf, int buf_len,
           const net::CompletionCallback& callback) override {
    return socket_->Read(buf, buf_len, callback);
  }
  int Write(net::IOBuffer* buf, int buf_len,
            const net::CompletionCallback& callback) override {
    return socket_->Write(buf, buf_len, callback);
  }

  int SetReceiveBufferSize(int32_t size) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }

  int SetSendBufferSize(int32_t size) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }

  int Connect(const net::CompletionCallback& callback) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }
  void Disconnect() override { socket_.reset(); }
  bool IsConnected() const override { return true; }
  bool IsConnectedAndIdle() const override { return true; }
  int GetPeerAddress(net::IPEndPoint* address) const override {
    // SSL sockets call this function so it must return some result.
    *address = net::IPEndPoint(net::IPAddress::IPv4AllZeros(), 0);
    return net::OK;
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    NOTREACHED();
    return net::ERR_FAILED;
  }
  const net::BoundNetLog& NetLog() const override { return net_log_; }
  void SetSubresourceSpeculation() override { NOTREACHED(); }
  void SetOmniboxSpeculation() override { NOTREACHED(); }
  bool WasEverUsed() const override {
    NOTREACHED();
    return true;
  }
  void EnableTCPFastOpenIfSupported() override { NOTREACHED(); }
  bool WasNpnNegotiated() const override {
    NOTREACHED();
    return false;
  }
  net::NextProto GetNegotiatedProtocol() const override {
    NOTREACHED();
    return net::kProtoUnknown;
  }
  bool GetSSLInfo(net::SSLInfo* ssl_info) override {
    NOTREACHED();
    return false;
  }
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override {
    NOTREACHED();
  }
  void ClearConnectionAttempts() override { NOTREACHED(); }
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
    NOTREACHED();
  }
  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }

 private:
  std::unique_ptr<P2PStreamSocket> socket_;
  net::BoundNetLog net_log_;
};

// Implements P2PStreamSocket interface on top of net::StreamSocket.
class P2PStreamSocketAdapter : public P2PStreamSocket {
 public:
  P2PStreamSocketAdapter(std::unique_ptr<net::StreamSocket> socket,
                         std::unique_ptr<net::SSLServerContext> server_context)
      : server_context_(std::move(server_context)),
        socket_(std::move(socket)) {}
  ~P2PStreamSocketAdapter() override {}

  int Read(const scoped_refptr<net::IOBuffer>& buf, int buf_len,
           const net::CompletionCallback& callback) override {
    return socket_->Read(buf.get(), buf_len, callback);
  }
  int Write(const scoped_refptr<net::IOBuffer>& buf, int buf_len,
            const net::CompletionCallback& callback) override {
    return socket_->Write(buf.get(), buf_len, callback);
  }

 private:
  // The server_context_ will be a nullptr for client sockets.
  // The server_context_ must outlive any sockets it spawns.
  std::unique_ptr<net::SSLServerContext> server_context_;
  std::unique_ptr<net::StreamSocket> socket_;
};

}  // namespace

// static
std::unique_ptr<SslHmacChannelAuthenticator>
SslHmacChannelAuthenticator::CreateForClient(const std::string& remote_cert,
                                             const std::string& auth_key) {
  std::unique_ptr<SslHmacChannelAuthenticator> result(
      new SslHmacChannelAuthenticator(auth_key));
  result->remote_cert_ = remote_cert;
  return result;
}

std::unique_ptr<SslHmacChannelAuthenticator>
SslHmacChannelAuthenticator::CreateForHost(const std::string& local_cert,
                                           scoped_refptr<RsaKeyPair> key_pair,
                                           const std::string& auth_key) {
  std::unique_ptr<SslHmacChannelAuthenticator> result(
      new SslHmacChannelAuthenticator(auth_key));
  result->local_cert_ = local_cert;
  result->local_key_pair_ = key_pair;
  return result;
}

SslHmacChannelAuthenticator::SslHmacChannelAuthenticator(
    const std::string& auth_key)
    : auth_key_(auth_key) {
}

SslHmacChannelAuthenticator::~SslHmacChannelAuthenticator() {
}

void SslHmacChannelAuthenticator::SecureAndAuthenticate(
    std::unique_ptr<P2PStreamSocket> socket,
    const DoneCallback& done_callback) {
  DCHECK(CalledOnValidThread());

  done_callback_ = done_callback;

  int result;
  if (is_ssl_server()) {
#if defined(OS_NACL)
    // Client plugin doesn't use server SSL sockets, and so SSLServerSocket
    // implementation is not compiled for NaCl as part of net_nacl.
    NOTREACHED();
    result = net::ERR_FAILED;
#else
    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromBytes(local_cert_.data(),
                                              local_cert_.length());
    if (!cert.get()) {
      LOG(ERROR) << "Failed to parse X509Certificate";
      NotifyError(net::ERR_FAILED);
      return;
    }

    net::SSLServerConfig ssl_config;
    ssl_config.require_ecdhe = true;

    server_context_ = net::CreateSSLServerContext(
        cert.get(), *local_key_pair_->private_key(), ssl_config);

    std::unique_ptr<net::SSLServerSocket> server_socket =
        server_context_->CreateSSLServerSocket(
            base::WrapUnique(new NetStreamSocketAdapter(std::move(socket))));
    net::SSLServerSocket* raw_server_socket = server_socket.get();
    socket_ = std::move(server_socket);
    result = raw_server_socket->Handshake(
        base::Bind(&SslHmacChannelAuthenticator::OnConnected,
                   base::Unretained(this)));
#endif
  } else {
    transport_security_state_.reset(new net::TransportSecurityState);
    cert_verifier_.reset(new FailingCertVerifier);

    net::SSLConfig::CertAndStatus cert_and_status;
    cert_and_status.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
    cert_and_status.der_cert = remote_cert_;

    net::SSLConfig ssl_config;
    // Certificate verification and revocation checking are not needed
    // because we use self-signed certs. Disable it so that the SSL
    // layer doesn't try to initialize OCSP (OCSP works only on the IO
    // thread).
    ssl_config.cert_io_enabled = false;
    ssl_config.rev_checking_enabled = false;
    ssl_config.allowed_bad_certs.push_back(cert_and_status);
    ssl_config.require_ecdhe = true;

    net::HostPortPair host_and_port(kSslFakeHostName, 0);
    net::SSLClientSocketContext context;
    context.transport_security_state = transport_security_state_.get();
    context.cert_verifier = cert_verifier_.get();
    std::unique_ptr<net::ClientSocketHandle> socket_handle(
        new net::ClientSocketHandle);
    socket_handle->SetSocket(
        base::WrapUnique(new NetStreamSocketAdapter(std::move(socket))));

#if defined(OS_NACL)
    // net_nacl doesn't include ClientSocketFactory.
    socket_.reset(new net::SSLClientSocketImpl(
        std::move(socket_handle), host_and_port, ssl_config, context));
#else
    socket_ =
        net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
            std::move(socket_handle), host_and_port, ssl_config, context);
#endif

    result = socket_->Connect(
        base::Bind(&SslHmacChannelAuthenticator::OnConnected,
                   base::Unretained(this)));
  }

  if (result == net::ERR_IO_PENDING)
    return;

  OnConnected(result);
}

bool SslHmacChannelAuthenticator::is_ssl_server() {
  return local_key_pair_.get() != nullptr;
}

void SslHmacChannelAuthenticator::OnConnected(int result) {
  if (result != net::OK) {
    LOG(WARNING) << "Failed to establish SSL connection.  Error: "
                 << net::ErrorToString(result);
    NotifyError(result);
    return;
  }

  // Generate authentication digest to write to the socket.
  std::string auth_bytes = GetAuthBytes(
      socket_.get(), is_ssl_server() ?
      kHostAuthSslExporterLabel : kClientAuthSslExporterLabel, auth_key_);
  if (auth_bytes.empty()) {
    NotifyError(net::ERR_FAILED);
    return;
  }

  // Allocate a buffer to write the digest.
  auth_write_buf_ = new net::DrainableIOBuffer(
      new net::StringIOBuffer(auth_bytes), auth_bytes.size());

  // Read an incoming token.
  auth_read_buf_ = new net::GrowableIOBuffer();
  auth_read_buf_->SetCapacity(kAuthDigestLength);

  // If WriteAuthenticationBytes() results in |done_callback_| being
  // called then we must not do anything else because this object may
  // be destroyed at that point.
  bool callback_called = false;
  WriteAuthenticationBytes(&callback_called);
  if (!callback_called)
    ReadAuthenticationBytes();
}

void SslHmacChannelAuthenticator::WriteAuthenticationBytes(
    bool* callback_called) {
  while (true) {
    int result = socket_->Write(
        auth_write_buf_.get(),
        auth_write_buf_->BytesRemaining(),
        base::Bind(&SslHmacChannelAuthenticator::OnAuthBytesWritten,
                   base::Unretained(this)));
    if (result == net::ERR_IO_PENDING)
      break;
    if (!HandleAuthBytesWritten(result, callback_called))
      break;
  }
}

void SslHmacChannelAuthenticator::OnAuthBytesWritten(int result) {
  DCHECK(CalledOnValidThread());

  if (HandleAuthBytesWritten(result, nullptr))
    WriteAuthenticationBytes(nullptr);
}

bool SslHmacChannelAuthenticator::HandleAuthBytesWritten(
    int result, bool* callback_called) {
  if (result <= 0) {
    LOG(ERROR) << "Error writing authentication: " << result;
    if (callback_called)
      *callback_called = false;
    NotifyError(result);
    return false;
  }

  auth_write_buf_->DidConsume(result);
  if (auth_write_buf_->BytesRemaining() > 0)
    return true;

  auth_write_buf_ = nullptr;
  CheckDone(callback_called);
  return false;
}

void SslHmacChannelAuthenticator::ReadAuthenticationBytes() {
  while (true) {
    int result =
        socket_->Read(auth_read_buf_.get(),
                      auth_read_buf_->RemainingCapacity(),
                      base::Bind(&SslHmacChannelAuthenticator::OnAuthBytesRead,
                                 base::Unretained(this)));
    if (result == net::ERR_IO_PENDING)
      break;
    if (!HandleAuthBytesRead(result))
      break;
  }
}

void SslHmacChannelAuthenticator::OnAuthBytesRead(int result) {
  DCHECK(CalledOnValidThread());

  if (HandleAuthBytesRead(result))
    ReadAuthenticationBytes();
}

bool SslHmacChannelAuthenticator::HandleAuthBytesRead(int read_result) {
  if (read_result <= 0) {
    NotifyError(read_result);
    return false;
  }

  auth_read_buf_->set_offset(auth_read_buf_->offset() + read_result);
  if (auth_read_buf_->RemainingCapacity() > 0)
    return true;

  if (!VerifyAuthBytes(std::string(
          auth_read_buf_->StartOfBuffer(),
          auth_read_buf_->StartOfBuffer() + kAuthDigestLength))) {
    LOG(WARNING) << "Mismatched authentication";
    NotifyError(net::ERR_FAILED);
    return false;
  }

  auth_read_buf_ = nullptr;
  CheckDone(nullptr);
  return false;
}

bool SslHmacChannelAuthenticator::VerifyAuthBytes(
    const std::string& received_auth_bytes) {
  DCHECK(received_auth_bytes.length() == kAuthDigestLength);

  // Compute expected auth bytes.
  std::string auth_bytes = GetAuthBytes(
      socket_.get(), is_ssl_server() ?
      kClientAuthSslExporterLabel : kHostAuthSslExporterLabel, auth_key_);
  if (auth_bytes.empty())
    return false;

  return crypto::SecureMemEqual(received_auth_bytes.data(),
                                &(auth_bytes[0]), kAuthDigestLength);
}

void SslHmacChannelAuthenticator::CheckDone(bool* callback_called) {
  if (auth_write_buf_.get() == nullptr && auth_read_buf_.get() == nullptr) {
    DCHECK(socket_.get() != nullptr);
    if (callback_called)
      *callback_called = true;

    base::ResetAndReturn(&done_callback_)
        .Run(net::OK, base::WrapUnique(new P2PStreamSocketAdapter(
                          std::move(socket_), std::move(server_context_))));
  }
}

void SslHmacChannelAuthenticator::NotifyError(int error) {
  base::ResetAndReturn(&done_callback_).Run(error, nullptr);
}

}  // namespace protocol
}  // namespace remoting
