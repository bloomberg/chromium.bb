// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_test_util.h"

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "net/socket/socket_test_util.h"

namespace net {

namespace {
const uint64 kA =
    (static_cast<uint64>(0x5851f42d) << 32) + static_cast<uint64>(0x4c957f2d);
const uint64 kC = 12345;
const uint64 kM = static_cast<uint64>(1) << 48;

}  // namespace

LinearCongruentialGenerator::LinearCongruentialGenerator(uint32 seed)
    : current_(seed) {}

uint32 LinearCongruentialGenerator::Generate() {
  uint64 result = current_;
  current_ = (current_ * kA + kC) % kM;
  return static_cast<uint32>(result >> 16);
}

std::string WebSocketStandardRequest(const std::string& path,
                                     const std::string& origin,
                                     const std::string& extra_headers) {
  // Unrelated changes in net/http may change the order and default-values of
  // HTTP headers, causing WebSocket tests to fail. It is safe to update this
  // string in that case.
  return base::StringPrintf(
      "GET %s HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Connection: Upgrade\r\n"
      "Upgrade: websocket\r\n"
      "Origin: %s\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "User-Agent:\r\n"
      "Accept-Encoding: gzip,deflate\r\n"
      "Accept-Language: en-us,fr\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "%s\r\n",
      path.c_str(),
      origin.c_str(),
      extra_headers.c_str());
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

struct WebSocketDeterministicMockClientSocketFactoryMaker::Detail {
  std::string expect_written;
  std::string return_to_read;
  MockRead read;
  MockWrite write;
  scoped_ptr<DeterministicSocketData> data;
  DeterministicMockClientSocketFactory factory;
};

WebSocketDeterministicMockClientSocketFactoryMaker::
    WebSocketDeterministicMockClientSocketFactoryMaker()
    : detail_(new Detail) {}

WebSocketDeterministicMockClientSocketFactoryMaker::
    ~WebSocketDeterministicMockClientSocketFactoryMaker() {}

DeterministicMockClientSocketFactory*
WebSocketDeterministicMockClientSocketFactoryMaker::factory() {
  return &detail_->factory;
}

void WebSocketDeterministicMockClientSocketFactoryMaker::SetExpectations(
    const std::string& expect_written,
    const std::string& return_to_read) {
  // We need to extend the lifetime of these strings.
  detail_->expect_written = expect_written;
  detail_->return_to_read = return_to_read;
  detail_->write = MockWrite(SYNCHRONOUS, 0, detail_->expect_written.c_str());
  detail_->read = MockRead(SYNCHRONOUS, 1, detail_->return_to_read.c_str());
  scoped_ptr<DeterministicSocketData> socket_data(
      new DeterministicSocketData(&detail_->read, 1, &detail_->write, 1));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_data->SetStop(2);
  SetRawExpectations(socket_data.Pass());
}

void WebSocketDeterministicMockClientSocketFactoryMaker::SetRawExpectations(
    scoped_ptr<DeterministicSocketData> socket_data) {
  detail_->data = socket_data.Pass();
  detail_->factory.AddSocketDataProvider(detail_->data.get());
}

WebSocketTestURLRequestContextHost::WebSocketTestURLRequestContextHost()
    : url_request_context_(true) {
  url_request_context_.set_client_socket_factory(maker_.factory());
}

WebSocketTestURLRequestContextHost::~WebSocketTestURLRequestContextHost() {}

void WebSocketTestURLRequestContextHost::SetRawExpectations(
    scoped_ptr<DeterministicSocketData> socket_data) {
  maker_.SetRawExpectations(socket_data.Pass());
}

TestURLRequestContext*
WebSocketTestURLRequestContextHost::GetURLRequestContext() {
  url_request_context_.Init();
  // A Network Delegate is required to make the URLRequest::Delegate work.
  url_request_context_.set_network_delegate(&network_delegate_);
  return &url_request_context_;
}

}  // namespace net
