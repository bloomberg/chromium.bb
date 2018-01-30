// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_test_util.h"

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "net/proxy_resolution/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "url/origin.h"

namespace net {

namespace {

const uint64_t kA = (static_cast<uint64_t>(0x5851f42d) << 32) +
                    static_cast<uint64_t>(0x4c957f2d);
const uint64_t kC = 12345;
const uint64_t kM = static_cast<uint64_t>(1) << 48;

}  // namespace

LinearCongruentialGenerator::LinearCongruentialGenerator(uint32_t seed)
    : current_(seed) {}

uint32_t LinearCongruentialGenerator::Generate() {
  uint64_t result = current_;
  current_ = (current_ * kA + kC) % kM;
  return static_cast<uint32_t>(result >> 16);
}

std::string WebSocketStandardRequest(
    const std::string& path,
    const std::string& host,
    const url::Origin& origin,
    const std::string& send_additional_request_headers,
    const std::string& extra_headers) {
  return WebSocketStandardRequestWithCookies(path, host, origin, std::string(),
                                             send_additional_request_headers,
                                             extra_headers);
}

std::string WebSocketStandardRequestWithCookies(
    const std::string& path,
    const std::string& host,
    const url::Origin& origin,
    const std::string& cookies,
    const std::string& send_additional_request_headers,
    const std::string& extra_headers) {
  // Unrelated changes in net/http may change the order and default-values of
  // HTTP headers, causing WebSocket tests to fail. It is safe to update this
  // in that case.
  HttpRequestHeaders headers;
  std::stringstream request_headers;

  request_headers << base::StringPrintf("GET %s HTTP/1.1\r\n", path.c_str());
  headers.SetHeader("Host", host);
  headers.SetHeader("Connection", "Upgrade");
  headers.SetHeader("Pragma", "no-cache");
  headers.SetHeader("Cache-Control", "no-cache");
  headers.SetHeader("Upgrade", "websocket");
  headers.SetHeader("Origin", origin.Serialize());
  headers.SetHeader("Sec-WebSocket-Version", "13");
  headers.SetHeader("User-Agent", "");
  headers.AddHeadersFromString(send_additional_request_headers);
  headers.SetHeader("Accept-Encoding", "gzip, deflate");
  headers.SetHeader("Accept-Language", "en-us,fr");
  headers.AddHeadersFromString(cookies);
  headers.SetHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
  headers.SetHeader("Sec-WebSocket-Extensions",
                    "permessage-deflate; client_max_window_bits");
  headers.AddHeadersFromString(extra_headers);

  request_headers << headers.ToString();
  return request_headers.str();
}

std::string WebSocketStandardResponse(const std::string& extra_headers) {
  return base::StringPrintf(
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "%s\r\n",
      extra_headers.c_str());
}

struct WebSocketMockClientSocketFactoryMaker::Detail {
  std::string expect_written;
  std::string return_to_read;
  std::vector<MockRead> reads;
  MockWrite write;
  std::vector<std::unique_ptr<SequencedSocketData>> socket_data_vector;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssl_socket_data_vector;
  MockClientSocketFactory factory;
};

WebSocketMockClientSocketFactoryMaker::WebSocketMockClientSocketFactoryMaker()
    : detail_(new Detail) {
}

WebSocketMockClientSocketFactoryMaker::
    ~WebSocketMockClientSocketFactoryMaker() = default;

MockClientSocketFactory* WebSocketMockClientSocketFactoryMaker::factory() {
  return &detail_->factory;
}

void WebSocketMockClientSocketFactoryMaker::SetExpectations(
    const std::string& expect_written,
    const std::string& return_to_read) {
  const size_t kHttpStreamParserBufferSize = 4096;
  // We need to extend the lifetime of these strings.
  detail_->expect_written = expect_written;
  detail_->return_to_read = return_to_read;
  int sequence = 0;
  detail_->write = MockWrite(SYNCHRONOUS,
                             detail_->expect_written.data(),
                             detail_->expect_written.size(),
                             sequence++);
  // HttpStreamParser reads 4KB at a time. We need to take this implementation
  // detail into account if |return_to_read| is big enough.
  for (size_t place = 0; place < detail_->return_to_read.size();
       place += kHttpStreamParserBufferSize) {
    detail_->reads.push_back(
        MockRead(SYNCHRONOUS, detail_->return_to_read.data() + place,
                 std::min(detail_->return_to_read.size() - place,
                          kHttpStreamParserBufferSize),
                 sequence++));
  }
  std::unique_ptr<SequencedSocketData> socket_data(new SequencedSocketData(
      detail_->reads.data(), detail_->reads.size(), &detail_->write, 1));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  AddRawExpectations(std::move(socket_data));
}

void WebSocketMockClientSocketFactoryMaker::AddRawExpectations(
    std::unique_ptr<SequencedSocketData> socket_data) {
  detail_->factory.AddSocketDataProvider(socket_data.get());
  detail_->socket_data_vector.push_back(std::move(socket_data));
}

void WebSocketMockClientSocketFactoryMaker::AddSSLSocketDataProvider(
    std::unique_ptr<SSLSocketDataProvider> ssl_socket_data) {
  detail_->factory.AddSSLSocketDataProvider(ssl_socket_data.get());
  detail_->ssl_socket_data_vector.push_back(std::move(ssl_socket_data));
}

WebSocketTestURLRequestContextHost::WebSocketTestURLRequestContextHost()
    : url_request_context_(true), url_request_context_initialized_(false) {
  url_request_context_.set_client_socket_factory(maker_.factory());
}

WebSocketTestURLRequestContextHost::~WebSocketTestURLRequestContextHost() =
    default;

void WebSocketTestURLRequestContextHost::AddRawExpectations(
    std::unique_ptr<SequencedSocketData> socket_data) {
  maker_.AddRawExpectations(std::move(socket_data));
}

void WebSocketTestURLRequestContextHost::AddSSLSocketDataProvider(
    std::unique_ptr<SSLSocketDataProvider> ssl_socket_data) {
  maker_.AddSSLSocketDataProvider(std::move(ssl_socket_data));
}

void WebSocketTestURLRequestContextHost::SetProxyConfig(
    const std::string& proxy_rules) {
  DCHECK(!url_request_context_initialized_);
  proxy_resolution_service_ = ProxyResolutionService::CreateFixed(proxy_rules);
  url_request_context_.set_proxy_resolution_service(
      proxy_resolution_service_.get());
}

TestURLRequestContext*
WebSocketTestURLRequestContextHost::GetURLRequestContext() {
  if (!url_request_context_initialized_) {
    url_request_context_.Init();
    // A Network Delegate is required to make the URLRequest::Delegate work.
    url_request_context_.set_network_delegate(&network_delegate_);
    url_request_context_initialized_ = true;
  }
  return &url_request_context_;
}

int FakeWebSocketHandshakeStreamBase::ReadResponseBody(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  NOTREACHED();
  return ERR_IO_PENDING;
}

bool FakeWebSocketHandshakeStreamBase::IsResponseBodyComplete() const {
  NOTREACHED();
  return false;
}

bool FakeWebSocketHandshakeStreamBase::IsConnectionReused() const {
  NOTREACHED();
  return false;
}
void FakeWebSocketHandshakeStreamBase::SetConnectionReused() {
  NOTREACHED();
}

bool FakeWebSocketHandshakeStreamBase::CanReuseConnection() const {
  return false;
}

int64_t FakeWebSocketHandshakeStreamBase::GetTotalReceivedBytes() const {
  return 0;
}

int64_t FakeWebSocketHandshakeStreamBase::GetTotalSentBytes() const {
  return 0;
}

bool FakeWebSocketHandshakeStreamBase::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  NOTREACHED();
  return false;
}

bool FakeWebSocketHandshakeStreamBase::GetAlternativeService(
    AlternativeService* alternative_service) const {
  NOTREACHED();
  return false;
}

void FakeWebSocketHandshakeStreamBase::GetSSLInfo(SSLInfo* ssl_info) {}

void FakeWebSocketHandshakeStreamBase::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
}

bool FakeWebSocketHandshakeStreamBase::GetRemoteEndpoint(IPEndPoint* endpoint) {
  return false;
}

Error FakeWebSocketHandshakeStreamBase::GetTokenBindingSignature(
    crypto::ECPrivateKey* key,
    TokenBindingType tb_type,
    std::vector<uint8_t>* out) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

void FakeWebSocketHandshakeStreamBase::Drain(HttpNetworkSession* session) {
  NOTREACHED();
}

void FakeWebSocketHandshakeStreamBase::PopulateNetErrorDetails(
    NetErrorDetails* details) {
  NOTREACHED();
}

void FakeWebSocketHandshakeStreamBase::SetPriority(RequestPriority priority) {
  NOTREACHED();
}

HttpStream* FakeWebSocketHandshakeStreamBase::RenewStreamForAuth() {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<WebSocketStream> FakeWebSocketHandshakeStreamBase::Upgrade() {
  NOTREACHED();
  return std::unique_ptr<WebSocketStream>();
}

int FakeWebSocketHandshakeStream::InitializeStream(
    const HttpRequestInfo* request_info,
    bool can_send_early,
    RequestPriority priority,
    const NetLogWithSource& net_log,
    const CompletionCallback& callback) {
  state_.Initialize(request_info, can_send_early, priority, net_log, callback);
  return OK;
}

int FakeWebSocketHandshakeStream::SendRequest(
    const HttpRequestHeaders& request_headers,
    HttpResponseInfo* response,
    const CompletionCallback& callback) {
  return parser()->SendRequest(state_.GenerateRequestLine(), request_headers,
                               TRAFFIC_ANNOTATION_FOR_TESTS, response,
                               callback);
}

int FakeWebSocketHandshakeStream::ReadResponseHeaders(
    const CompletionCallback& callback) {
  return parser()->ReadResponseHeaders(callback);
}

void FakeWebSocketHandshakeStream::Close(bool not_reusable) {
  if (parser())
    parser()->Close(true);
}

std::unique_ptr<WebSocketHandshakeStreamBase>
FakeWebSocketStreamCreateHelper::CreateBasicStream(
    std::unique_ptr<ClientSocketHandle> connection,
    bool using_proxy) {
  return std::make_unique<FakeWebSocketHandshakeStream>(std::move(connection),
                                                        using_proxy);
}

}  // namespace net
