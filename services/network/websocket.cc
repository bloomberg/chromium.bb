// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket.h"

#include <inttypes.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"  // for WebSocketFrameHeader::OpCode
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"

namespace network {
namespace {

typedef net::WebSocketEventInterface::ChannelState ChannelState;

// Convert a network::mojom::WebSocketMessageType to a
// net::WebSocketFrameHeader::OpCode
net::WebSocketFrameHeader::OpCode MessageTypeToOpCode(
    network::mojom::WebSocketMessageType type) {
  DCHECK(type == network::mojom::WebSocketMessageType::CONTINUATION ||
         type == network::mojom::WebSocketMessageType::TEXT ||
         type == network::mojom::WebSocketMessageType::BINARY);
  typedef net::WebSocketFrameHeader::OpCode OpCode;
  // These compile asserts verify that the same underlying values are used for
  // both types, so we can simply cast between them.
  static_assert(
      static_cast<OpCode>(network::mojom::WebSocketMessageType::CONTINUATION) ==
          net::WebSocketFrameHeader::kOpCodeContinuation,
      "enum values must match for opcode continuation");
  static_assert(
      static_cast<OpCode>(network::mojom::WebSocketMessageType::TEXT) ==
          net::WebSocketFrameHeader::kOpCodeText,
      "enum values must match for opcode text");
  static_assert(
      static_cast<OpCode>(network::mojom::WebSocketMessageType::BINARY) ==
          net::WebSocketFrameHeader::kOpCodeBinary,
      "enum values must match for opcode binary");
  return static_cast<OpCode>(type);
}

network::mojom::WebSocketMessageType OpCodeToMessageType(
    net::WebSocketFrameHeader::OpCode opCode) {
  DCHECK(opCode == net::WebSocketFrameHeader::kOpCodeContinuation ||
         opCode == net::WebSocketFrameHeader::kOpCodeText ||
         opCode == net::WebSocketFrameHeader::kOpCodeBinary);
  // This cast is guaranteed valid by the static_assert() statements above.
  return static_cast<network::mojom::WebSocketMessageType>(opCode);
}

}  // namespace

// Implementation of net::WebSocketEventInterface. Receives events from our
// WebSocketChannel object.
class WebSocket::WebSocketEventHandler final
    : public net::WebSocketEventInterface {
 public:
  explicit WebSocketEventHandler(WebSocket* impl);
  ~WebSocketEventHandler() override;

  // net::WebSocketEventInterface implementation

  void OnCreateURLRequest(net::URLRequest* url_request) override;
  ChannelState OnAddChannelResponse(const std::string& selected_subprotocol,
                                    const std::string& extensions) override;
  ChannelState OnDataFrame(bool fin,
                           WebSocketMessageType type,
                           scoped_refptr<net::IOBuffer> buffer,
                           size_t buffer_size) override;
  ChannelState OnClosingHandshake() override;
  ChannelState OnFlowControl(int64_t quota) override;
  ChannelState OnDropChannel(bool was_clean,
                             uint16_t code,
                             const std::string& reason) override;
  ChannelState OnFailChannel(const std::string& message) override;
  ChannelState OnStartOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) override;
  ChannelState OnFinishOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) override;
  ChannelState OnSSLCertificateError(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          callbacks,
      const GURL& url,
      const net::SSLInfo& ssl_info,
      bool fatal) override;

 private:
  WebSocket* const impl_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventHandler);
};

WebSocket::WebSocketEventHandler::WebSocketEventHandler(WebSocket* impl)
    : impl_(impl) {
  DVLOG(1) << "WebSocketEventHandler created @"
           << reinterpret_cast<void*>(this);
}

WebSocket::WebSocketEventHandler::~WebSocketEventHandler() {
  DVLOG(1) << "WebSocketEventHandler destroyed @"
           << reinterpret_cast<void*>(this);
}

void WebSocket::WebSocketEventHandler::OnCreateURLRequest(
    net::URLRequest* url_request) {
  impl_->delegate_->OnCreateURLRequest(impl_->child_id_, impl_->frame_id_,
                                       url_request);
}

ChannelState WebSocket::WebSocketEventHandler::OnAddChannelResponse(
    const std::string& selected_protocol,
    const std::string& extensions) {
  DVLOG(3) << "WebSocketEventHandler::OnAddChannelResponse @"
           << reinterpret_cast<void*>(this) << " selected_protocol=\""
           << selected_protocol << "\""
           << " extensions=\"" << extensions << "\"";

  impl_->delegate_->OnReceivedResponseFromServer(impl_);

  impl_->client_->OnAddChannelResponse(selected_protocol, extensions);

  return net::WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocket::WebSocketEventHandler::OnDataFrame(
    bool fin,
    net::WebSocketFrameHeader::OpCode type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
  DVLOG(3) << "WebSocketEventHandler::OnDataFrame @"
           << reinterpret_cast<void*>(this) << " fin=" << fin
           << " type=" << type << " data is " << buffer_size << " bytes";

  // TODO(darin): Avoid this copy.
  std::vector<uint8_t> data_to_pass(buffer_size);
  if (buffer_size > 0) {
    std::copy(buffer->data(), buffer->data() + buffer_size,
              data_to_pass.begin());
  }

  impl_->client_->OnDataFrame(fin, OpCodeToMessageType(type), data_to_pass);

  return net::WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocket::WebSocketEventHandler::OnClosingHandshake() {
  DVLOG(3) << "WebSocketEventHandler::OnClosingHandshake @"
           << reinterpret_cast<void*>(this);

  impl_->client_->OnClosingHandshake();

  return net::WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocket::WebSocketEventHandler::OnFlowControl(int64_t quota) {
  DVLOG(3) << "WebSocketEventHandler::OnFlowControl @"
           << reinterpret_cast<void*>(this) << " quota=" << quota;

  impl_->client_->OnFlowControl(quota);

  return net::WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocket::WebSocketEventHandler::OnDropChannel(
    bool was_clean,
    uint16_t code,
    const std::string& reason) {
  DVLOG(3) << "WebSocketEventHandler::OnDropChannel @"
           << reinterpret_cast<void*>(this) << " was_clean=" << was_clean
           << " code=" << code << " reason=\"" << reason << "\"";

  impl_->client_->OnDropChannel(was_clean, code, reason);

  // net::WebSocketChannel requires that we delete it at this point.
  impl_->channel_.reset();

  return net::WebSocketEventInterface::CHANNEL_DELETED;
}

ChannelState WebSocket::WebSocketEventHandler::OnFailChannel(
    const std::string& message) {
  DVLOG(3) << "WebSocketEventHandler::OnFailChannel @"
           << reinterpret_cast<void*>(this) << " message=\"" << message << "\"";

  impl_->client_->OnFailChannel(message);

  // net::WebSocketChannel requires that we delete it at this point.
  impl_->channel_.reset();

  return net::WebSocketEventInterface::CHANNEL_DELETED;
}

ChannelState WebSocket::WebSocketEventHandler::OnStartOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
  bool should_send = impl_->delegate_->CanReadRawCookies();

  DVLOG(3) << "WebSocketEventHandler::OnStartOpeningHandshake @"
           << reinterpret_cast<void*>(this) << " should_send=" << should_send;

  if (!should_send)
    return WebSocketEventInterface::CHANNEL_ALIVE;

  network::mojom::WebSocketHandshakeRequestPtr request_to_pass(
      network::mojom::WebSocketHandshakeRequest::New());
  request_to_pass->url.Swap(&request->url);
  net::HttpRequestHeaders::Iterator it(request->headers);
  while (it.GetNext()) {
    network::mojom::HttpHeaderPtr header(network::mojom::HttpHeader::New());
    header->name = it.name();
    header->value = it.value();
    request_to_pass->headers.push_back(std::move(header));
  }
  request_to_pass->headers_text =
      base::StringPrintf("GET %s HTTP/1.1\r\n",
                         request_to_pass->url.spec().c_str()) +
      request->headers.ToString();

  impl_->client_->OnStartOpeningHandshake(std::move(request_to_pass));

  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocket::WebSocketEventHandler::OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  bool should_send = impl_->delegate_->CanReadRawCookies();

  DVLOG(3) << "WebSocketEventHandler::OnFinishOpeningHandshake "
           << reinterpret_cast<void*>(this) << " should_send=" << should_send;

  if (!should_send)
    return WebSocketEventInterface::CHANNEL_ALIVE;

  network::mojom::WebSocketHandshakeResponsePtr response_to_pass(
      network::mojom::WebSocketHandshakeResponse::New());
  response_to_pass->url.Swap(&response->url);
  response_to_pass->status_code = response->status_code;
  response_to_pass->status_text = response->status_text;
  size_t iter = 0;
  std::string name, value;
  while (response->headers->EnumerateHeaderLines(&iter, &name, &value)) {
    network::mojom::HttpHeaderPtr header(network::mojom::HttpHeader::New());
    header->name = name;
    header->value = value;
    response_to_pass->headers.push_back(std::move(header));
  }
  response_to_pass->headers_text =
      net::HttpUtil::ConvertHeadersBackToHTTPResponse(
          response->headers->raw_headers());

  impl_->client_->OnFinishOpeningHandshake(std::move(response_to_pass));

  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocket::WebSocketEventHandler::OnSSLCertificateError(
    std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DVLOG(3) << "WebSocketEventHandler::OnSSLCertificateError"
           << reinterpret_cast<void*>(this) << " url=" << url.spec()
           << " cert_status=" << ssl_info.cert_status << " fatal=" << fatal;
  impl_->delegate_->OnSSLCertificateError(std::move(callbacks), url,
                                          impl_->child_id_, impl_->frame_id_,
                                          ssl_info, fatal);
  // The above method is always asynchronous.
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

WebSocket::WebSocket(std::unique_ptr<Delegate> delegate,
                     network::mojom::WebSocketRequest request,
                     int child_id,
                     int frame_id,
                     url::Origin origin,
                     base::TimeDelta delay)
    : delegate_(std::move(delegate)),
      binding_(this, std::move(request)),
      delay_(delay),
      pending_flow_control_quota_(0),
      child_id_(child_id),
      frame_id_(frame_id),
      origin_(std::move(origin)),
      handshake_succeeded_(false),
      weak_ptr_factory_(this) {
  binding_.set_connection_error_handler(
      base::BindOnce(&WebSocket::OnConnectionError, base::Unretained(this)));
}

WebSocket::~WebSocket() {}

void WebSocket::GoAway() {
  StartClosingHandshake(static_cast<uint16_t>(net::kWebSocketErrorGoingAway),
                        "");
}

void WebSocket::AddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    const std::string& user_agent_override,
    network::mojom::WebSocketClientPtr client) {
  DVLOG(3) << "WebSocket::AddChannelRequest @" << reinterpret_cast<void*>(this)
           << " socket_url=\"" << socket_url << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin_ << "\" site_for_cookies=\"" << site_for_cookies
           << "\" user_agent_override=\"" << user_agent_override << "\"";

  if (client_ || !client) {
    delegate_->ReportBadMessage(
        Delegate::BadMessageReason::kUnexpectedAddChannelRequest);
    return;
  }

  client_ = std::move(client);

  DCHECK(!channel_);
  if (delay_ > base::TimeDelta()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebSocket::AddChannel, weak_ptr_factory_.GetWeakPtr(),
                       socket_url, requested_protocols, site_for_cookies,
                       user_agent_override),
        delay_);
  } else {
    AddChannel(socket_url, requested_protocols, site_for_cookies,
               user_agent_override);
  }
}

void WebSocket::SendFrame(bool fin,
                          network::mojom::WebSocketMessageType type,
                          const std::vector<uint8_t>& data) {
  DVLOG(3) << "WebSocket::SendFrame @" << reinterpret_cast<void*>(this)
           << " fin=" << fin << " type=" << type << " data is " << data.size()
           << " bytes";

  if (!channel_) {
    // The client should not be sending us frames until after we've informed
    // it that the channel has been opened (OnAddChannelResponse).
    if (handshake_succeeded_) {
      DVLOG(1) << "Dropping frame sent to closed websocket";
    } else {
      delegate_->ReportBadMessage(
          Delegate::BadMessageReason::kUnexpectedSendFrame);
    }
    return;
  }

  // TODO(darin): Avoid this copy.
  scoped_refptr<net::IOBuffer> data_to_pass(new net::IOBuffer(data.size()));
  std::copy(data.begin(), data.end(), data_to_pass->data());

  channel_->SendFrame(fin, MessageTypeToOpCode(type), std::move(data_to_pass),
                      data.size());
}

void WebSocket::SendFlowControl(int64_t quota) {
  DVLOG(3) << "WebSocket::OnFlowControl @" << reinterpret_cast<void*>(this)
           << " quota=" << quota;

  if (!channel_) {
    // WebSocketChannel is not yet created due to the delay introduced by
    // per-renderer WebSocket throttling.
    // SendFlowControl() is called after WebSocketChannel is created.
    pending_flow_control_quota_ += quota;
    return;
  }

  ignore_result(channel_->SendFlowControl(quota));
}

void WebSocket::StartClosingHandshake(uint16_t code,
                                      const std::string& reason) {
  DVLOG(3) << "WebSocket::StartClosingHandshake @"
           << reinterpret_cast<void*>(this) << " code=" << code << " reason=\""
           << reason << "\"";

  if (!channel_) {
    // WebSocketChannel is not yet created due to the delay introduced by
    // per-renderer WebSocket throttling.
    if (client_)
      client_->OnDropChannel(false, net::kWebSocketErrorAbnormalClosure, "");
    return;
  }

  ignore_result(channel_->StartClosingHandshake(code, reason));
}

void WebSocket::OnConnectionError() {
  DVLOG(3) << "WebSocket::OnConnectionError @" << reinterpret_cast<void*>(this);

  delegate_->OnLostConnectionToClient(this);
}

void WebSocket::AddChannel(const GURL& socket_url,
                           const std::vector<std::string>& requested_protocols,
                           const GURL& site_for_cookies,
                           const std::string& user_agent_override) {
  DVLOG(3) << "WebSocket::AddChannel @" << reinterpret_cast<void*>(this)
           << " socket_url=\"" << socket_url << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin_ << "\" site_for_cookies=\"" << site_for_cookies
           << "\" user_agent_override=\"" << user_agent_override << "\"";

  DCHECK(!channel_);

  std::unique_ptr<net::WebSocketEventInterface> event_interface(
      new WebSocketEventHandler(this));
  channel_.reset(new net::WebSocketChannel(std::move(event_interface),
                                           delegate_->GetURLRequestContext()));

  int64_t quota = pending_flow_control_quota_;
  pending_flow_control_quota_ = 0;

  std::string additional_headers;
  if (!user_agent_override.empty()) {
    if (!net::HttpUtil::IsValidHeaderValue(user_agent_override)) {
      delegate_->ReportBadMessage(
          Delegate::BadMessageReason::kInvalidHeaderValue);
      return;
    }
    additional_headers =
        base::StringPrintf("%s:%s", net::HttpRequestHeaders::kUserAgent,
                           user_agent_override.c_str());
  }
  channel_->SendAddChannelRequest(socket_url, requested_protocols, origin_,
                                  site_for_cookies, additional_headers);
  if (quota > 0)
    SendFlowControl(quota);
}

}  // namespace network
