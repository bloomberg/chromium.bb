// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/direct_sockets/direct_sockets_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_udp_socket.h"
#include "services/network/test/udp_socket_test_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "url/gurl.h"

// The tests in this file use the Network Service implementation of
// NetworkContext, to test sending and receiving of data over UDP sockets.

namespace content {

namespace {

net::Error UnconditionallyPermitConnection(
    const blink::mojom::DirectSocketOptions& options) {
  DCHECK(options.remote_hostname.has_value());
  return net::OK;
}

constexpr char kLocalhostAddress[] = "127.0.0.1";

}  // anonymous namespace

class DirectSocketsUdpBrowserTest : public ContentBrowserTest {
 public:
  ~DirectSocketsUdpBrowserTest() override = default;

  GURL GetTestPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/udp.html");
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return browser_context()->GetDefaultStoragePartition()->GetNetworkContext();
  }

 protected:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    std::string origin_list = GetTestPageURL().spec();

    command_line->AppendSwitchASCII(switches::kIsolatedAppOrigins, origin_list);
  }

  std::pair<net::IPEndPoint, network::test::UDPSocketTestHelper>
  CreateUDPServerSocket(mojo::PendingRemote<network::mojom::UDPSocketListener>
                            listener_receiver_remote) {
    GetNetworkContext()->CreateUDPSocket(
        server_socket_.BindNewPipeAndPassReceiver(),
        std::move(listener_receiver_remote));

    server_socket_.set_disconnect_handler(
        base::BindLambdaForTesting([]() { NOTREACHED(); }));

    net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 0);
    network::test::UDPSocketTestHelper server_helper(&server_socket_);

    int result = server_helper.BindSync(server_addr, nullptr, &server_addr);
    DCHECK_EQ(net::OK, result);

    return {server_addr, server_helper};
  }

  mojo::Remote<network::mojom::UDPSocket>& GetUDPServerSocket() {
    return server_socket_;
  }

 private:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  test::IsolatedAppContentBrowserClient client_;
  ScopedContentBrowserClientSetting setting{&client_};
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<network::mojom::UDPSocket> server_socket_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, CloseUdp) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const std::string script = "closeUdp('::1', 993, {})";

  EXPECT_EQ("closeUdp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, SendUdp) {
  // We send datagrams with one byte, two bytes, three bytes, ...
  const uint32_t kRequiredDatagrams = 35;
  const uint32_t kRequiredBytes =
      kRequiredDatagrams * (kRequiredDatagrams + 1) / 2;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  // Any attempt to make this a class member results into
  // "This caller requires a single-threaded context".
  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  GetUDPServerSocket()->ReceiveMore(kRequiredDatagrams);

  const std::string script =
      JsReplace("sendUdp($1, $2, {}, $3)", server_address.ToStringWithoutPort(),
                server_address.port(), static_cast<int>(kRequiredBytes));

  EXPECT_EQ("send succeeded", EvalJs(shell(), script));

  listener.WaitForReceivedResults(kRequiredDatagrams);
  EXPECT_EQ(listener.results().size(), kRequiredDatagrams);

  uint32_t bytes_received = 0, expected_data_size = 0;
  for (const network::test::UDPSocketListenerImpl::ReceivedResult& result :
       listener.results()) {
    expected_data_size++;
    EXPECT_EQ(result.net_error, net::OK);
    EXPECT_TRUE(result.src_addr.has_value());
    EXPECT_TRUE(result.data.has_value());
    EXPECT_EQ(result.data->size(), expected_data_size);
    for (uint8_t current : *result.data) {
      EXPECT_EQ(current, bytes_received % 256);
      ++bytes_received;
    }
  }
  EXPECT_EQ(bytes_received, kRequiredBytes);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, SendUdpAfterClose) {
  const int32_t kRequiredBytes = 1;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const std::string script = JsReplace("sendUdpAfterClose($1, $2, {}, $3)",
                                       kLocalhostAddress, 993, kRequiredBytes);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              ::testing::HasSubstr("Stream closed."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadUdp) {
  const uint32_t kRequiredDatagrams = 35;
  const uint32_t kRequiredBytes =
      kRequiredDatagrams * (kRequiredDatagrams + 1) / 2;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  // Why so complicated? Turns out that in order to send udp datagrams from
  // server to client we need to be aware what the client's local port is.
  // It cannot be predefined, so the first step is to create a socket in the
  // global scope and retrieve the assigned local port.
  const std::string open_socket = JsReplace(
      R"((async () => {
        socket = new UDPSocket($1, $2);
        let { localPort } = await socket.connection;
        return localPort;
      })())",
      server_address.ToStringWithoutPort(), server_address.port());

  const uint16_t local_port = EvalJs(shell(), open_socket).ExtractInt();

  auto runner =
      std::make_unique<content::test::AsyncJsRunner>(shell()->web_contents());
  const std::string async_read = content::test::WrapAsync(JsReplace(
      R"(
        let { readable } = await socket.connection;
        let reader = readable.getReader();
        return await readLoop(reader, $1);
      )",
      static_cast<int>(kRequiredBytes)));
  auto future = runner->RunScript(async_read);

  // With a client socket listening in the javascript code, we can finally start
  // sending out data.
  net::IPEndPoint client_addr(net::IPAddress::IPv4Localhost(), local_port);
  uint32_t bytesSent = 0;
  for (uint32_t i = 0; i < kRequiredDatagrams; i++) {
    std::vector<uint8_t> message(i + 1);
    for (uint8_t& byte : message) {
      byte = bytesSent % 256;
      bytesSent++;
    }
    EXPECT_EQ(net::OK, server_helper.SendToSync(client_addr, message));
  }

  // Blocks until script execution is complete and returns the resulting
  // message.
  ASSERT_EQ(future->Get(), "readLoop succeeded.");
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadUdpAfterSocketClose) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string script =
      JsReplace("readUdpAfterSocketClose($1, $2, {})",
                server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_EQ("readUdpAferSocketClose succeeded.", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadUdpAfterStreamClose) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string script =
      JsReplace("readUdpAfterStreamClose($1, $2, {})",
                server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_EQ("readUdpAferSocketClose succeeded.", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, CloseWithActiveReader) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string open_socket =
      JsReplace("closeUdpWithLockedReadable($1, $2)",
                server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_THAT(EvalJs(shell(), open_socket).ExtractString(),
              ::testing::StartsWith("closeUdpWithLockedReadable failed"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest,
                       CloseWithActiveReaderForce) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string open_socket =
      JsReplace("closeUdpWithLockedReadable($1, $2, true)",
                server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_THAT(EvalJs(shell(), open_socket).ExtractString(),
              ::testing::StartsWith("closeUdpWithLockedReadable succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadWriteUdpOnSendError) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  content::test::MockNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  const std::string open_socket = JsReplace(
      R"(
        socket = new UDPSocket($1, 0);
        await socket.connection;
      )",
      kLocalhostAddress);

  ASSERT_TRUE(
      EvalJs(shell(), content::test::WrapAsync(open_socket)).value.is_none());

  auto runner =
      std::make_unique<content::test::AsyncJsRunner>(shell()->web_contents());
  const std::string async_read = "readWriteUdpOnError(socket);";
  auto future = runner->RunScript(async_read);

  // MockNetworkContext owns the MockUDPSocket and therefore outlives it.
  mock_network_context.get_udp_socket()->SetAdditionalSendCallback(
      base::BindOnce(
          [](content::test::MockNetworkContext* context) {
            context->get_udp_socket()->MockSend(net::ERR_UNEXPECTED);
          },
          &mock_network_context));

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("readWriteUdpOnError succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadWriteUdpOnSocketError) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  content::test::MockNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  const std::string open_socket = JsReplace(
      R"(
        socket = new UDPSocket($1, 0);
        await socket.connection;
      )",
      kLocalhostAddress);

  ASSERT_TRUE(
      EvalJs(shell(), content::test::WrapAsync(open_socket)).value.is_none());

  // MockNetworkContext owns the MockUDPSocket and therefore outlives it.
  mock_network_context.get_udp_socket()->SetAdditionalSendCallback(
      base::BindOnce(
          [](content::test::MockNetworkContext* context) {
            context->get_udp_socket()->get_listener().reset();
          },
          &mock_network_context));

  auto runner =
      std::make_unique<content::test::AsyncJsRunner>(shell()->web_contents());
  const std::string script = "readWriteUdpOnError(socket)";
  auto future = runner->RunScript(script);

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("readWriteUdpOnError succeeded"));
}

}  // namespace content
