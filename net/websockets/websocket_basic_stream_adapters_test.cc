// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream_adapters.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager_impl.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_config.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

const char* const kGroupName = "ssl/www.example.org:443";

class WebSocketClientSocketHandleAdapterTest : public testing::Test {
 protected:
  WebSocketClientSocketHandleAdapterTest()
      : host_port_pair_("www.example.org", 443),
        socket_pool_manager_(std::make_unique<ClientSocketPoolManagerImpl>(
            &net_log_,
            &socket_factory_,
            nullptr,
            nullptr,
            &host_resolver,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            "test_shard",
            nullptr,
            HttpNetworkSession::NORMAL_SOCKET_POOL)),
        transport_params_(base::MakeRefCounted<TransportSocketParams>(
            host_port_pair_,
            false,
            OnHostResolutionCallback(),
            TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT)),
        ssl_params_(base::MakeRefCounted<SSLSocketParams>(transport_params_,
                                                          nullptr,
                                                          nullptr,
                                                          host_port_pair_,
                                                          SSLConfig(),
                                                          PRIVACY_MODE_DISABLED,
                                                          0,
                                                          false)) {}

  bool InitClientSocketHandle(ClientSocketHandle* connection) {
    TestCompletionCallback callback;
    int rv = connection->Init(
        kGroupName, ssl_params_, MEDIUM, SocketTag(),
        ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
        socket_pool_manager_->GetSSLSocketPool(), NetLogWithSource());
    rv = callback.GetResult(rv);
    return rv == OK;
  }

  const HostPortPair host_port_pair_;
  NetLog net_log_;
  MockClientSocketFactory socket_factory_;
  MockHostResolver host_resolver;
  std::unique_ptr<ClientSocketPoolManagerImpl> socket_pool_manager_;
  scoped_refptr<TransportSocketParams> transport_params_;
  scoped_refptr<SSLSocketParams> ssl_params_;
};

TEST_F(WebSocketClientSocketHandleAdapterTest, Uninitialized) {
  auto connection = std::make_unique<ClientSocketHandle>();
  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_FALSE(adapter.is_initialized());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, IsInitialized) {
  StaticSocketDataProvider data(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  ClientSocketHandle* const connection_ptr = connection.get();

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_FALSE(adapter.is_initialized());

  EXPECT_TRUE(InitClientSocketHandle(connection_ptr));

  EXPECT_TRUE(adapter.is_initialized());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, Disconnect) {
  StaticSocketDataProvider data(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  StreamSocket* const socket = connection->socket();

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  EXPECT_TRUE(socket->IsConnected());
  adapter.Disconnect();
  EXPECT_FALSE(socket->IsConnected());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, Read) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, "foo"), MockRead("bar")};
  StaticSocketDataProvider data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  // Buffer larger than each MockRead.
  const int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  int rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", base::StringPiece(read_buf->data(), rv));

  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("bar", base::StringPiece(read_buf->data(), rv));

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, ReadIntoSmallBuffer) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, "foo"), MockRead("bar")};
  StaticSocketDataProvider data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  // Buffer smaller than each MockRead.
  const int kReadBufSize = 2;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  int rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionCallback());
  ASSERT_EQ(2, rv);
  EXPECT_EQ("fo", base::StringPiece(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionCallback());
  ASSERT_EQ(1, rv);
  EXPECT_EQ("o", base::StringPiece(read_buf->data(), rv));

  TestCompletionCallback callback1;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback1.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback1.WaitForResult();
  ASSERT_EQ(2, rv);
  EXPECT_EQ("ba", base::StringPiece(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionCallback());
  ASSERT_EQ(1, rv);
  EXPECT_EQ("r", base::StringPiece(read_buf->data(), rv));

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, Write) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, "foo"), MockWrite("bar")};
  StaticSocketDataProvider data(nullptr, 0, writes, arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  auto write_buf1 = base::MakeRefCounted<StringIOBuffer>("foo");
  int rv = adapter.Write(write_buf1.get(), write_buf1->size(),
                         CompletionCallback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(3, rv);

  auto write_buf2 = base::MakeRefCounted<StringIOBuffer>("bar");
  TestCompletionCallback callback;
  rv = adapter.Write(write_buf2.get(), write_buf2->size(), callback.callback(),
                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Test that if both Read() and Write() returns asynchronously,
// the two callbacks are handled correctly.
TEST_F(WebSocketClientSocketHandleAdapterTest, AsyncReadAndWrite) {
  MockRead reads[] = {MockRead("foobar")};
  MockWrite writes[] = {MockWrite("baz")};
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  const int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  TestCompletionCallback read_callback;
  int rv = adapter.Read(read_buf.get(), kReadBufSize, read_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  auto write_buf = base::MakeRefCounted<StringIOBuffer>("baz");
  TestCompletionCallback write_callback;
  rv = adapter.Write(write_buf.get(), write_buf->size(),
                     write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = read_callback.WaitForResult();
  ASSERT_EQ(6, rv);
  EXPECT_EQ("foobar", base::StringPiece(read_buf->data(), rv));

  rv = write_callback.WaitForResult();
  ASSERT_EQ(3, rv);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

}  // namespace test

}  // namespace net
