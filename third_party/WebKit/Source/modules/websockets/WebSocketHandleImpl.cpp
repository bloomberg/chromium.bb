// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/websockets/WebSocketHandleImpl.h"

#include "modules/websockets/WebSocketHandleClient.h"
#include "platform/WebTaskRunner.h"
#include "platform/network/NetworkLog.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/network/WebSocketHandshakeResponse.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"

namespace blink {
namespace {

const uint16_t kAbnormalShutdownOpCode = 1006;

}  // namespace

WebSocketHandleImpl::WebSocketHandleImpl()
    : client_(nullptr), client_binding_(this) {
  NETWORK_DVLOG(1) << this << " created";
}

WebSocketHandleImpl::~WebSocketHandleImpl() {
  NETWORK_DVLOG(1) << this << " deleted";

  if (websocket_)
    websocket_->StartClosingHandshake(kAbnormalShutdownOpCode, g_empty_string);
}

void WebSocketHandleImpl::Initialize(mojom::blink::WebSocketPtr websocket) {
  NETWORK_DVLOG(1) << this << " initialize(...)";

  DCHECK(!websocket_);
  websocket_ = std::move(websocket);
  websocket_.set_connection_error_with_reason_handler(
      ConvertToBaseCallback(WTF::Bind(&WebSocketHandleImpl::OnConnectionError,
                                      WTF::Unretained(this))));
}

void WebSocketHandleImpl::Connect(const KURL& url,
                                  const Vector<String>& protocols,
                                  SecurityOrigin* origin,
                                  const KURL& site_for_cookies,
                                  const String& user_agent_override,
                                  WebSocketHandleClient* client,
                                  WebTaskRunner* task_runner) {
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " connect(" << url.GetString() << ", "
                   << origin->ToString() << ")";

  DCHECK(!client_);
  DCHECK(client);
  client_ = client;

  mojom::blink::WebSocketClientPtr client_proxy;
  client_binding_.Bind(mojo::MakeRequest(
      &client_proxy, task_runner->ToSingleThreadTaskRunner()));
  websocket_->AddChannelRequest(
      url, protocols, origin, site_for_cookies,
      user_agent_override.IsNull() ? g_empty_string : user_agent_override,
      std::move(client_proxy));
}

void WebSocketHandleImpl::Send(bool fin,
                               WebSocketHandle::MessageType type,
                               const char* data,
                               size_t size) {
  DCHECK(websocket_);

  mojom::blink::WebSocketMessageType type_to_pass;
  switch (type) {
    case WebSocketHandle::kMessageTypeContinuation:
      type_to_pass = mojom::blink::WebSocketMessageType::CONTINUATION;
      break;
    case WebSocketHandle::kMessageTypeText:
      type_to_pass = mojom::blink::WebSocketMessageType::TEXT;
      break;
    case WebSocketHandle::kMessageTypeBinary:
      type_to_pass = mojom::blink::WebSocketMessageType::BINARY;
      break;
    default:
      NOTREACHED();
      return;
  }

  NETWORK_DVLOG(1) << this << " send(" << fin << ", " << type_to_pass << ", "
                   << "(data size = " << size << "))";

  // TODO(darin): Avoid this copy.
  Vector<uint8_t> data_to_pass(size);
  std::copy(data, data + size, data_to_pass.begin());

  websocket_->SendFrame(fin, type_to_pass, data_to_pass);
}

void WebSocketHandleImpl::FlowControl(int64_t quota) {
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " flowControl(" << quota << ")";

  websocket_->SendFlowControl(quota);
}

void WebSocketHandleImpl::Close(unsigned short code, const String& reason) {
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " close(" << code << ", " << reason << ")";

  websocket_->StartClosingHandshake(code,
                                    reason.IsNull() ? g_empty_string : reason);
}

void WebSocketHandleImpl::Disconnect() {
  websocket_.reset();
  client_ = nullptr;
}

void WebSocketHandleImpl::OnConnectionError(uint32_t custom_reason,
                                            const std::string& description) {
  // Our connection to the WebSocket was dropped. This could be due to
  // exceeding the maximum number of concurrent websockets from this process.
  String failure_message;
  if (custom_reason == mojom::blink::WebSocket::kInsufficientResources) {
    failure_message =
        description.empty()
            ? "Insufficient resources"
            : String::FromUTF8(description.c_str(), description.size());
  } else {
    DCHECK(description.empty());
    failure_message = "Unspecified reason";
  }
  OnFailChannel(failure_message);
}

void WebSocketHandleImpl::OnFailChannel(const String& message) {
  NETWORK_DVLOG(1) << this << " OnFailChannel(" << message << ")";

  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  client->DidFail(this, message);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnStartOpeningHandshake(
    mojom::blink::WebSocketHandshakeRequestPtr request) {
  NETWORK_DVLOG(1) << this << " OnStartOpeningHandshake("
                   << request->url.GetString() << ")";

  scoped_refptr<WebSocketHandshakeRequest> request_to_pass =
      WebSocketHandshakeRequest::Create(request->url);
  for (size_t i = 0; i < request->headers.size(); ++i) {
    const mojom::blink::HttpHeaderPtr& header = request->headers[i];
    request_to_pass->AddHeaderField(AtomicString(header->name),
                                    AtomicString(header->value));
  }
  request_to_pass->SetHeadersText(request->headers_text);
  client_->DidStartOpeningHandshake(this, request_to_pass);
}

void WebSocketHandleImpl::OnFinishOpeningHandshake(
    mojom::blink::WebSocketHandshakeResponsePtr response) {
  NETWORK_DVLOG(1) << this << " OnFinishOpeningHandshake("
                   << response->url.GetString() << ")";

  WebSocketHandshakeResponse response_to_pass;
  response_to_pass.SetStatusCode(response->status_code);
  response_to_pass.SetStatusText(response->status_text);
  for (size_t i = 0; i < response->headers.size(); ++i) {
    const mojom::blink::HttpHeaderPtr& header = response->headers[i];
    response_to_pass.AddHeaderField(AtomicString(header->name),
                                    AtomicString(header->value));
  }
  response_to_pass.SetHeadersText(response->headers_text);
  client_->DidFinishOpeningHandshake(this, &response_to_pass);
}

void WebSocketHandleImpl::OnAddChannelResponse(const String& protocol,
                                               const String& extensions) {
  NETWORK_DVLOG(1) << this << " OnAddChannelResponse(" << protocol << ", "
                   << extensions << ")";

  if (!client_)
    return;

  client_->DidConnect(this, protocol, extensions);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDataFrame(bool fin,
                                      mojom::blink::WebSocketMessageType type,
                                      const Vector<uint8_t>& data) {
  NETWORK_DVLOG(1) << this << " OnDataFrame(" << fin << ", " << type << ", "
                   << "(data size = " << data.size() << "))";
  if (!client_)
    return;

  WebSocketHandle::MessageType type_to_pass =
      WebSocketHandle::kMessageTypeContinuation;
  switch (type) {
    case mojom::blink::WebSocketMessageType::CONTINUATION:
      type_to_pass = WebSocketHandle::kMessageTypeContinuation;
      break;
    case mojom::blink::WebSocketMessageType::TEXT:
      type_to_pass = WebSocketHandle::kMessageTypeText;
      break;
    case mojom::blink::WebSocketMessageType::BINARY:
      type_to_pass = WebSocketHandle::kMessageTypeBinary;
      break;
  }
  const char* data_to_pass =
      reinterpret_cast<const char*>(data.IsEmpty() ? nullptr : &data[0]);
  client_->DidReceiveData(this, fin, type_to_pass, data_to_pass, data.size());
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnFlowControl(int64_t quota) {
  NETWORK_DVLOG(1) << this << " OnFlowControl(" << quota << ")";
  if (!client_)
    return;

  client_->DidReceiveFlowControl(this, quota);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDropChannel(bool was_clean,
                                        uint16_t code,
                                        const String& reason) {
  NETWORK_DVLOG(1) << this << " OnDropChannel(" << was_clean << ", " << code
                   << ", " << reason << ")";

  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  client->DidClose(this, was_clean, code, reason);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnClosingHandshake() {
  NETWORK_DVLOG(1) << this << " OnClosingHandshake()";
  if (!client_)
    return;

  client_->DidStartClosingHandshake(this);
  // |this| can be deleted here.
}

}  // namespace blink
