// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_test_utils.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_contents_tester.h"

namespace content::test {

// MockHostResolver implementation

MockHostResolver::MockHostResolver(
    mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
    net::HostResolver* internal_resolver)
    : receiver_(this), internal_resolver_(internal_resolver) {
  receiver_.Bind(std::move(resolver_receiver));
}

MockHostResolver::~MockHostResolver() = default;

void MockHostResolver::ResolveHost(
    const ::net::HostPortPair& host,
    const ::net::NetworkIsolationKey& network_isolation_key,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    ::mojo::PendingRemote<network::mojom::ResolveHostClient>
        pending_response_client) {
  DCHECK(!internal_request_);
  DCHECK(!response_client_.is_bound());

  internal_request_ = internal_resolver_->CreateRequest(
      host, network_isolation_key,
      net::NetLogWithSource::Make(net::NetLog::Get(),
                                  net::NetLogSourceType::NONE),
      absl::nullopt);
  mojo::Remote<network::mojom::ResolveHostClient> response_client(
      std::move(pending_response_client));

  int rv = internal_request_->Start(
      base::BindOnce(&MockHostResolver::OnComplete, base::Unretained(this)));
  if (rv != net::ERR_IO_PENDING) {
    response_client->OnComplete(
        rv, internal_request_->GetResolveErrorInfo(),
        base::OptionalFromPtr(internal_request_->GetAddressResults()));
    return;
  }

  response_client_ = std::move(response_client);
}

void MockHostResolver::MdnsListen(
    const ::net::HostPortPair& host,
    ::net::DnsQueryType query_type,
    ::mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
    MdnsListenCallback callback) {
  NOTIMPLEMENTED();
}

void MockHostResolver::OnComplete(int error) {
  DCHECK(response_client_.is_bound());
  DCHECK(internal_request_);

  response_client_->OnComplete(
      error, internal_request_->GetResolveErrorInfo(),
      base::OptionalFromPtr(internal_request_->GetAddressResults()));
  response_client_.reset();
}

// MockUDPSocket implementation

MockUDPSocket::MockUDPSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  receiver_.Bind(std::move(receiver));
  listener_.Bind(std::move(listener));
}

MockUDPSocket::~MockUDPSocket() {
  if (callback_) {
    std::move(callback_).Run(net::OK);
  }
}

void MockUDPSocket::Connect(const net::IPEndPoint& remote_addr,
                            network::mojom::UDPSocketOptionsPtr socket_options,
                            ConnectCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), net::OK,
                     net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}));
}

void MockUDPSocket::Send(
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  if (additional_send_callback_) {
    std::move(additional_send_callback_).Run();
  }
}

void MockUDPSocket::MockSend(int32_t result,
                             const absl::optional<base::span<uint8_t>>& data) {
  listener_->OnReceived(result, {}, data);
}

// MockNetworkContext implementation

MockNetworkContext::MockNetworkContext() = default;
MockNetworkContext::~MockNetworkContext() = default;

void MockNetworkContext::CreateUDPSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  socket_ = CreateMockUDPSocket(std::move(receiver), std::move(listener));
}

void MockNetworkContext::CreateHostResolver(
    const absl::optional<net::DnsConfigOverrides>& config_overrides,
    mojo::PendingReceiver<network::mojom::HostResolver> receiver) {
  internal_resolver_ = net::HostResolver::CreateStandaloneResolver(
      net::NetLog::Get(), /*options=*/absl::nullopt, host_mapping_rules_,
      /*enable_caching=*/false);
  host_resolver_ = std::make_unique<MockHostResolver>(std::move(receiver),
                                                      internal_resolver_.get());
}

std::unique_ptr<MockUDPSocket> MockNetworkContext::CreateMockUDPSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  return std::make_unique<MockUDPSocket>(std::move(receiver),
                                         std::move(listener));
}

// AsyncJsRunner implementation

AsyncJsRunner::AsyncJsRunner(content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {
  registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                 Source<WebContents>(web_contents));
}

AsyncJsRunner::~AsyncJsRunner() = default;

std::unique_ptr<base::test::TestFuture<std::string>> AsyncJsRunner::RunScript(
    const std::string& async_script) {
  // Do not leave behind hanging futures from previous invocations.
  DCHECK(!future_callback_);
  auto future = std::make_unique<base::test::TestFuture<std::string>>();

  if (web_contents_) {
    token_ = base::Token::CreateRandom();
    future_callback_ = future->GetCallback();
    const std::string wrapped_script =
        MakeScriptSendResultToDomQueue(async_script);
    ExecuteScriptAsync(web_contents_.get(), wrapped_script);
  }

  return future;
}

void AsyncJsRunner::Observe(int type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  Details<std::string> dom_op_result(details);
  // Check that future is valid and not yet fulfilled.
  DCHECK(future_callback_);

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(
      *dom_op_result.ptr(), base::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(parsed.value);
  DCHECK_EQ(parsed.value->type(), base::Value::Type::LIST);

  const auto& list = parsed.value->GetList();
  DCHECK_EQ(list.size(), 2U);
  DCHECK(list[0].is_string());
  DCHECK(list[1].is_string());

  if (list[1].GetString() == token_.ToString()) {
    std::move(future_callback_).Run(list[0].GetString());
  }
}

std::string AsyncJsRunner::MakeScriptSendResultToDomQueue(
    const std::string& script) const {
  DCHECK(!script.empty());
  return WrapAsync(base::StringPrintf(
      R"(
        let result = await %s;
        window.domAutomationController.send([result, '%s']);
      )",
      script.c_str(), token_.ToString().c_str()));
}

// misc
std::string WrapAsync(const std::string& script) {
  DCHECK(!script.empty());
  return base::StringPrintf(R"((async () => {%s})())", script.c_str());
}

}  // namespace content::test