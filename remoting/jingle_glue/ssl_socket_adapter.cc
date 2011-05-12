// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/ssl_socket_adapter.h"

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "jingle/glue/utils.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"

namespace remoting {

namespace {

// NSS doesn't load root certificates when running in sandbox, so we
// need to have gmail's cert hardcoded.
//
// TODO(sergeyu): Remove this when we don't make XMPP connection from
// inside of sandbox.
const char kGmailCertBase64[] =
    "MIIC2TCCAkKgAwIBAgIDBz+SMA0GCSqGSIb3DQEBBQUAME4xCzAJBgNVBAYTAlVT"
    "MRAwDgYDVQQKEwdFcXVpZmF4MS0wKwYDVQQLEyRFcXVpZmF4IFNlY3VyZSBDZXJ0"
    "aWZpY2F0ZSBBdXRob3JpdHkwHhcNMDcwNDExMTcxNzM4WhcNMTIwNDEwMTcxNzM4"
    "WjBkMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQGA1UEBxMN"
    "TW91bnRhaW4gVmlldzEUMBIGA1UEChMLR29vZ2xlIEluYy4xEjAQBgNVBAMTCWdt"
    "YWlsLmNvbTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA1Hds2jWwXAVGef06"
    "7PeSJF/h9BnoYlTdykx0lBTDc92/JLvuq0lJkytqll1UR4kHmF4vwqQkwcqOK03w"
    "k8qDK8fh6M13PYhvPEXP02ozsuL3vqE8hcCva2B9HVnOPY17Qok37rYQ+yexswN5"
    "eh0+93nddEa1PyHgEQ8CDKCJaWUCAwEAAaOBrjCBqzAOBgNVHQ8BAf8EBAMCBPAw"
    "HQYDVR0OBBYEFJcjzXEevMEDIEvuQiT7puEJY737MDoGA1UdHwQzMDEwL6AtoCuG"
    "KWh0dHA6Ly9jcmwuZ2VvdHJ1c3QuY29tL2NybHMvc2VjdXJlY2EuY3JsMB8GA1Ud"
    "IwQYMBaAFEjmaPkr0rKV10fYIyAQTzOYkJ/UMB0GA1UdJQQWMBQGCCsGAQUFBwMB"
    "BggrBgEFBQcDAjANBgkqhkiG9w0BAQUFAAOBgQB74cGpjdENf9U+WEd29dfzY3Tz"
    "JehnlY5cH5as8bOTe7PNPzj967OJ7TPWEycMwlS7CsqIsmfRGOFFfoHxo+iPugZ8"
    "uO2Kd++QHCXL+MumGjkW4FcTFmceV/Q12Wdh3WApcqIZZciQ79MAeFh7bzteAYqf"
    "wC98YQwylC9wVhf1yw==";

}  // namespace

SSLSocketAdapter* SSLSocketAdapter::Create(AsyncSocket* socket) {
  return new SSLSocketAdapter(socket);
}

SSLSocketAdapter::SSLSocketAdapter(AsyncSocket* socket)
    : SSLAdapter(socket),
      ignore_bad_cert_(false),
      cert_verifier_(new net::CertVerifier()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connected_callback_(this, &SSLSocketAdapter::OnConnected)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &SSLSocketAdapter::OnRead)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &SSLSocketAdapter::OnWrite)),
      ssl_state_(SSLSTATE_NONE),
      read_state_(IOSTATE_NONE),
      write_state_(IOSTATE_NONE) {
  transport_socket_ = new TransportSocket(socket, this);
}

SSLSocketAdapter::~SSLSocketAdapter() {
}

int SSLSocketAdapter::StartSSL(const char* hostname, bool restartable) {
  DCHECK(!restartable);
  hostname_ = hostname;

  if (socket_->GetState() != Socket::CS_CONNECTED) {
    ssl_state_ = SSLSTATE_WAIT;
    return 0;
  } else {
    return BeginSSL();
  }
}

int SSLSocketAdapter::BeginSSL() {
  if (!MessageLoop::current()) {
    // Certificate verification is done via the Chrome message loop.
    // Without this check, if we don't have a chrome message loop the
    // SSL connection just hangs silently.
    LOG(DFATAL) << "Chrome message loop (needed by SSL certificate "
                << "verification) does not exist";
    return net::ERR_UNEXPECTED;
  }

  // SSLConfigService is not thread-safe, and the default values for SSLConfig
  // are correct for us, so we don't use the config service to initialize this
  // object.
  net::SSLConfig ssl_config;

  std::string gmail_cert_binary;
  base::Base64Decode(kGmailCertBase64, &gmail_cert_binary);
  scoped_refptr<net::X509Certificate> gmail_cert =
      net::X509Certificate::CreateFromBytes(gmail_cert_binary.data(),
                                            gmail_cert_binary.size());
  DCHECK(gmail_cert);
  net::SSLConfig::CertAndStatus gmail_cert_status;
  gmail_cert_status.cert = gmail_cert;
  gmail_cert_status.cert_status = 0;
  ssl_config.allowed_bad_certs.push_back(gmail_cert_status);

  transport_socket_->set_addr(talk_base::SocketAddress(hostname_, 0));
  ssl_socket_.reset(
      net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
          transport_socket_, net::HostPortPair(hostname_, 443), ssl_config,
          NULL /* ssl_host_info */,
          cert_verifier_.get()));

  int result = ssl_socket_->Connect(&connected_callback_);

  if (result == net::ERR_IO_PENDING || result == net::OK) {
    return 0;
  } else {
    LOG(ERROR) << "Could not start SSL: " << net::ErrorToString(result);
    return result;
  }
}

int SSLSocketAdapter::Send(const void* buf, size_t len) {
  if (ssl_state_ != SSLSTATE_CONNECTED) {
    return AsyncSocketAdapter::Send(buf, len);
  } else {
    scoped_refptr<net::IOBuffer> transport_buf(new net::IOBuffer(len));
    memcpy(transport_buf->data(), buf, len);

    int result = ssl_socket_->Write(transport_buf, len, NULL);
    if (result == net::ERR_IO_PENDING) {
      SetError(EWOULDBLOCK);
    }
    transport_buf = NULL;
    return result;
  }
}

int SSLSocketAdapter::Recv(void* buf, size_t len) {
  switch (ssl_state_) {
    case SSLSTATE_NONE:
      return AsyncSocketAdapter::Recv(buf, len);

    case SSLSTATE_WAIT:
      SetError(EWOULDBLOCK);
      return -1;

    case SSLSTATE_CONNECTED:
      switch (read_state_) {
        case IOSTATE_NONE: {
          transport_buf_ = new net::IOBuffer(len);
          int result = ssl_socket_->Read(transport_buf_, len, &read_callback_);
          if (result >= 0) {
            memcpy(buf, transport_buf_->data(), len);
          }

          if (result == net::ERR_IO_PENDING) {
            read_state_ = IOSTATE_PENDING;
            SetError(EWOULDBLOCK);
          } else {
            if (result < 0) {
              SetError(result);
              VLOG(1) << "Socket error " << result;
            }
            transport_buf_ = NULL;
          }
          return result;
        }
        case IOSTATE_PENDING:
          SetError(EWOULDBLOCK);
          return -1;

        case IOSTATE_COMPLETE:
          memcpy(buf, transport_buf_->data(), len);
          transport_buf_ = NULL;
          read_state_ = IOSTATE_NONE;
          return data_transferred_;
      }
  }

  NOTREACHED();
  return -1;
}

void SSLSocketAdapter::OnConnected(int result) {
  if (result == net::OK) {
    ssl_state_ = SSLSTATE_CONNECTED;
    OnConnectEvent(this);
  } else {
    LOG(WARNING) << "OnConnected failed with error " << result;
  }
}

void SSLSocketAdapter::OnRead(int result) {
  DCHECK(read_state_ == IOSTATE_PENDING);
  read_state_ = IOSTATE_COMPLETE;
  data_transferred_ = result;
  AsyncSocketAdapter::OnReadEvent(this);
}

void SSLSocketAdapter::OnWrite(int result) {
  DCHECK(write_state_ == IOSTATE_PENDING);
  write_state_ = IOSTATE_COMPLETE;
  data_transferred_ = result;
  AsyncSocketAdapter::OnWriteEvent(this);
}

void SSLSocketAdapter::OnConnectEvent(talk_base::AsyncSocket* socket) {
  if (ssl_state_ != SSLSTATE_WAIT) {
    AsyncSocketAdapter::OnConnectEvent(socket);
  } else {
    ssl_state_ = SSLSTATE_NONE;
    int result = BeginSSL();
    if (0 != result) {
      // TODO(zork): Handle this case gracefully.
      LOG(WARNING) << "BeginSSL() failed with " << result;
    }
  }
}

TransportSocket::TransportSocket(talk_base::AsyncSocket* socket,
                                 SSLSocketAdapter *ssl_adapter)
    : read_callback_(NULL),
      write_callback_(NULL),
      read_buffer_len_(0),
      write_buffer_len_(0),
      socket_(socket),
      was_used_to_convey_data_(false) {
  socket_->SignalReadEvent.connect(this, &TransportSocket::OnReadEvent);
  socket_->SignalWriteEvent.connect(this, &TransportSocket::OnWriteEvent);
}

TransportSocket::~TransportSocket() {
}

int TransportSocket::Connect(net::CompletionCallback* callback) {
  // Connect is never called by SSLClientSocket, instead SSLSocketAdapter
  // calls Connect() on socket_ directly.
  NOTREACHED();
  return false;
}

void TransportSocket::Disconnect() {
  socket_->Close();
}

bool TransportSocket::IsConnected() const {
  return (socket_->GetState() == talk_base::Socket::CS_CONNECTED);
}

bool TransportSocket::IsConnectedAndIdle() const {
  // Not implemented.
  NOTREACHED();
  return false;
}

int TransportSocket::GetPeerAddress(net::AddressList* address) const {
  talk_base::SocketAddress socket_address = socket_->GetRemoteAddress();

  // libjingle supports only IPv4 addresses.
  sockaddr_in ipv4addr;
  socket_address.ToSockAddr(&ipv4addr);

  struct addrinfo ai;
  memset(&ai, 0, sizeof(ai));
  ai.ai_family = ipv4addr.sin_family;
  ai.ai_socktype = SOCK_STREAM;
  ai.ai_protocol = IPPROTO_TCP;
  ai.ai_addr = reinterpret_cast<struct sockaddr*>(&ipv4addr);
  ai.ai_addrlen = sizeof(ipv4addr);

  *address = net::AddressList::CreateByCopyingFirstAddress(&ai);
  return net::OK;
}

int TransportSocket::GetLocalAddress(net::IPEndPoint* address) const {
  talk_base::SocketAddress socket_address = socket_->GetLocalAddress();
  if (jingle_glue::SocketAddressToIPEndPoint(socket_address, address)) {
    return net::OK;
  } else {
    return net::ERR_FAILED;
  }
}

const net::BoundNetLog& TransportSocket::NetLog() const {
  return net_log_;
}

void TransportSocket::SetSubresourceSpeculation() {
  NOTREACHED();
}

void TransportSocket::SetOmniboxSpeculation() {
  NOTREACHED();
}

bool TransportSocket::WasEverUsed() const {
  // We don't use this in ClientSocketPools, so this should never be used.
  NOTREACHED();
  return was_used_to_convey_data_;
}

bool TransportSocket::UsingTCPFastOpen() const {
  return false;
}

int TransportSocket::Read(net::IOBuffer* buf, int buf_len,
                          net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(!read_callback_);
  DCHECK(!read_buffer_.get());
  int result = socket_->Recv(buf->data(), buf_len);
  if (result < 0) {
    result = net::MapSystemError(socket_->GetError());
    if (result == net::ERR_IO_PENDING) {
      read_callback_ = callback;
      read_buffer_ = buf;
      read_buffer_len_ = buf_len;
    }
  }
  if (result != net::ERR_IO_PENDING)
    was_used_to_convey_data_ = true;
  return result;
}

int TransportSocket::Write(net::IOBuffer* buf, int buf_len,
                           net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(!write_callback_);
  DCHECK(!write_buffer_.get());
  int result = socket_->Send(buf->data(), buf_len);
  if (result < 0) {
    result = net::MapSystemError(socket_->GetError());
    if (result == net::ERR_IO_PENDING) {
      write_callback_ = callback;
      write_buffer_ = buf;
      write_buffer_len_ = buf_len;
    }
  }
  if (result != net::ERR_IO_PENDING)
    was_used_to_convey_data_ = true;
  return result;
}

bool TransportSocket::SetReceiveBufferSize(int32 size) {
  // Not implemented.
  return false;
}

bool TransportSocket::SetSendBufferSize(int32 size) {
  // Not implemented.
  return false;
}

void TransportSocket::OnReadEvent(talk_base::AsyncSocket* socket) {
  if (read_callback_) {
    DCHECK(read_buffer_.get());
    net::CompletionCallback* callback = read_callback_;
    scoped_refptr<net::IOBuffer> buffer = read_buffer_;
    int buffer_len = read_buffer_len_;

    read_callback_ = NULL;
    read_buffer_ = NULL;
    read_buffer_len_ = 0;

    int result = socket_->Recv(buffer->data(), buffer_len);
    if (result < 0) {
      result = net::MapSystemError(socket_->GetError());
      if (result == net::ERR_IO_PENDING) {
        read_callback_ = callback;
        read_buffer_ = buffer;
        read_buffer_len_ = buffer_len;
        return;
      }
    }
    was_used_to_convey_data_ = true;
    callback->RunWithParams(Tuple1<int>(result));
  }
}

void TransportSocket::OnWriteEvent(talk_base::AsyncSocket* socket) {
  if (write_callback_) {
    DCHECK(write_buffer_.get());
    net::CompletionCallback* callback = write_callback_;
    scoped_refptr<net::IOBuffer> buffer = write_buffer_;
    int buffer_len = write_buffer_len_;

    write_callback_ = NULL;
    write_buffer_ = NULL;
    write_buffer_len_ = 0;

    int result = socket_->Send(buffer->data(), buffer_len);
    if (result < 0) {
      result = net::MapSystemError(socket_->GetError());
      if (result == net::ERR_IO_PENDING) {
        write_callback_ = callback;
        write_buffer_ = buffer;
        write_buffer_len_ = buffer_len;
        return;
      }
    }
    was_used_to_convey_data_ = true;
    callback->RunWithParams(Tuple1<int>(result));
  }
}

}  // namespace remoting
