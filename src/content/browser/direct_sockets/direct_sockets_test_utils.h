// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_TEST_UTILS_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_TEST_UTILS_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_udp_socket.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content::test {

// Mock Host Resolver for Direct Sockets browsertests.
class MockHostResolver : public network::mojom::HostResolver {
 public:
  explicit MockHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
      net::HostResolver* internal_resolver);

  ~MockHostResolver() override;

  MockHostResolver(const MockHostResolver&) = delete;
  MockHostResolver& operator=(const MockHostResolver&) = delete;

  void ResolveHost(const ::net::HostPortPair& host,
                   const ::net::NetworkIsolationKey& network_isolation_key,
                   network::mojom::ResolveHostParametersPtr optional_parameters,
                   ::mojo::PendingRemote<network::mojom::ResolveHostClient>
                       pending_response_client) override;

  void MdnsListen(
      const ::net::HostPortPair& host,
      ::net::DnsQueryType query_type,
      ::mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override;

 protected:
  virtual void OnComplete(int error);

  std::unique_ptr<net::HostResolver::ResolveHostRequest> internal_request_;
  mojo::Remote<network::mojom::ResolveHostClient> response_client_;
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  const raw_ptr<net::HostResolver> internal_resolver_;
};

// Mock UDP Socket for Direct Sockets browsertests.
class MockUDPSocket : public network::TestUDPSocket {
 public:
  MockUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener);

  ~MockUDPSocket() override;

  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr socket_options,
               ConnectCallback callback) override;

  void ReceiveMore(uint32_t) override {}

  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;

  void MockSend(int32_t result,
                const absl::optional<base::span<uint8_t>>& data = {});

  mojo::Remote<network::mojom::UDPSocketListener>& get_listener() {
    return listener_;
  }

  mojo::Receiver<network::mojom::UDPSocket>& get_receiver() {
    return receiver_;
  }

  void SetAdditionalSendCallback(base::OnceClosure additional_send_callback) {
    additional_send_callback_ = std::move(additional_send_callback);
  }

 protected:
  mojo::Receiver<network::mojom::UDPSocket> receiver_{this};
  mojo::Remote<network::mojom::UDPSocketListener> listener_;

  SendCallback callback_;
  base::OnceClosure additional_send_callback_;
};

// Mock Network Context for Direct Sockets browsertests.
class MockNetworkContext : public network::TestNetworkContext {
 public:
  MockNetworkContext();

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

  ~MockNetworkContext() override;

  // network::mojom::NetworkContext implementation:
  void CreateUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener) override;

  void CreateHostResolver(
      const absl::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override;

  MockUDPSocket* get_udp_socket() { return socket_.get(); }

  void set_host_mapping_rules(std::string host_mapping_rules) {
    host_mapping_rules_ = std::move(host_mapping_rules);
  }

 protected:
  virtual std::unique_ptr<MockUDPSocket> CreateMockUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener);

  std::string host_mapping_rules_;
  std::unique_ptr<net::HostResolver> internal_resolver_;
  std::unique_ptr<network::mojom::HostResolver> host_resolver_;
  std::unique_ptr<MockUDPSocket> socket_;
};

// A wrapper class that allows running javascript asynchronously.
//
//    * RunScript(...) returns a unique pointer to
//      base::test::TestFuture<std::string>. Call
//      Get(...) on the future pointer to wait for
//      the script to complete.
//    * Note that the observer expects exactly one message per script
//      invocation:
//      DCHECK(...) will fire if more than one message arrives.
//    * Can be reused. The following sketch is totally valid:
//
//      IN_PROC_BROWSER_TEST_F(MyTestFixture, MyTest) {
//        auto runner =
//        std::make_unique<AsyncJsRunner>(shell()->web_contents());
//
//        const std::string script_template = "return $1;";
//
//        const std::string script_a = JsReplace(script_template, "MessageA");
//        auto future_a = runner->RunScript(WrapAsync(script_a));
//        EXPECT_EQ(future_a->Get(), "\"MessageA\"");
//
//        const std::string script_b = JsReplace(script_template, "MessageB");
//        auto future_b = runner->RunScript(WrapAsync(script_b));
//        EXPECT_EQ(future_b->Get(), "\"MessageB\"");
//      }
//
// Make sure to pass async functions to RunScript(...) (see WrapAsync(...)
// below).

class AsyncJsRunner : public WebContentsObserver {
 public:
  explicit AsyncJsRunner(content::WebContents* web_contents);
  ~AsyncJsRunner() override;

  std::unique_ptr<base::test::TestFuture<std::string>> RunScript(
      const std::string& script);

  // WebContentsObserver:
  void DomOperationResponse(RenderFrameHost* render_frame_host,
                            const std::string& json_string) override;

 private:
  std::string MakeScriptSendResultToDomQueue(const std::string& script) const;

  base::OnceCallback<void(std::string)> future_callback_;
  base::Token token_;
};

std::string WrapAsync(const std::string& script);

// Mock ContentBrowserClient that enableds direct sockets via permissions policy
// for isolated apps.
class IsolatedAppContentBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override;

  blink::ParsedPermissionsPolicy GetPermissionsPolicyForIsolatedApp(
      content::BrowserContext* browser_context,
      const url::Origin& app_origin) override;
};

}  // namespace content::test

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_TEST_UTILS_H_
