// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log.h"
#include "services/network/host_resolver.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class HostResolverTest : public testing::Test {
 public:
  HostResolverTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

net::IPEndPoint CreateExpectedEndPoint(const std::string& address,
                                       uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(address));
  return net::IPEndPoint(ip_address, port);
}

class TestResolveHostClient : public mojom::ResolveHostClient {
 public:
  // If |run_loop| is non-null, will call RunLoop::Quit() on completion.
  TestResolveHostClient(mojom::ResolveHostClientPtr* interface_ptr,
                        base::RunLoop* run_loop)
      : binding_(this, mojo::MakeRequest(interface_ptr)),
        complete_(false),
        result_error_(net::ERR_UNEXPECTED),
        run_loop_(run_loop) {}

  void CloseBinding() { binding_.Close(); }

  void OnComplete(int error,
                  const base::Optional<net::AddressList>& addresses) override {
    DCHECK(!complete_);

    complete_ = true;
    result_error_ = error;
    result_addresses_ = addresses;
    if (run_loop_)
      run_loop_->Quit();
  }

  bool complete() const { return complete_; }

  int result_error() const {
    DCHECK(complete_);
    return result_error_;
  }

  const base::Optional<net::AddressList>& result_addresses() const {
    DCHECK(complete_);
    return result_addresses_;
  }

 private:
  mojo::Binding<mojom::ResolveHostClient> binding_;

  bool complete_;
  int result_error_;
  base::Optional<net::AddressList> result_addresses_;
  base::RunLoop* const run_loop_;
};

TEST_F(HostResolverTest, Sync) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->set_synchronous_mode(true);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.0.1", 160)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_EQ(net::DEFAULT_PRIORITY, inner_resolver->last_request_priority());
}

TEST_F(HostResolverTest, Async) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->set_synchronous_mode(false);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.0.1", 160)));
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_EQ(net::DEFAULT_PRIORITY, inner_resolver->last_request_priority());
}

TEST_F(HostResolverTest, DnsQueryType) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::HostResolver::DnsQueryType::AAAA;

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("::1", 160)));
}

TEST_F(HostResolverTest, InitialPriority) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->initial_priority = net::HIGHEST;

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.0.1", 80)));
  EXPECT_EQ(net::HIGHEST, inner_resolver->last_request_priority());
}

TEST_F(HostResolverTest, Failure_Sync) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddSimulatedFailure("example.com");
  inner_resolver->set_synchronous_mode(true);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("example.com", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Failure_Async) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddSimulatedFailure("example.com");
  inner_resolver->set_synchronous_mode(false);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("example.com", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, NoOptionalParameters) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, NoControlHandle) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, CloseControlHandle) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  control_handle = nullptr;
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160),
                                    CreateExpectedEndPoint("::1", 160)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Cancellation) {
  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.Run();

  // On cancellation, should receive an ERR_FAILED result, and the internal
  // resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_ABORTED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Cancellation_SubsequentRequest) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, nullptr);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be ERR_ABORTED or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(response_client.result_error() == net::ERR_ABORTED ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojom::ResolveHostClientPtr response_client_ptr2;
  TestResolveHostClient response_client2(&response_client_ptr2, &run_loop2);
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, DestroyResolver) {
  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();
  net::NetLog net_log;

  auto resolver =
      std::make_unique<HostResolver>(inner_resolver.get(), &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80),
                        std::move(optional_parameters),
                        std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  resolver = nullptr;
  run_loop.Run();

  // On context destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(HostResolverTest, CloseClient) {
  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  response_client.CloseBinding();
  run_loop.RunUntilIdle();

  // Response pipe is closed, so no results to check. Internal request should be
  // cancelled.
  EXPECT_FALSE(response_client.complete());
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, CloseClient_SubsequentRequest) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, nullptr);

  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr));

  response_client.CloseBinding();
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be incomplete or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(!response_client.complete() ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojom::ResolveHostClientPtr response_client_ptr2;
  TestResolveHostClient response_client2(&response_client_ptr2, &run_loop2);
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Binding) {
  mojom::HostResolverPtr resolver_ptr;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(mojo::MakeRequest(&resolver_ptr),
                        std::move(shutdown_callback), inner_resolver.get(),
                        &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver_ptr->ResolveHost(net::HostPortPair("localhost", 160),
                            std::move(optional_parameters),
                            std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160),
                                    CreateExpectedEndPoint("::1", 160)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_FALSE(shutdown_resolver);
}

TEST_F(HostResolverTest, CloseBinding) {
  mojom::HostResolverPtr resolver_ptr;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();
  net::NetLog net_log;

  HostResolver resolver(mojo::MakeRequest(&resolver_ptr),
                        std::move(shutdown_callback), inner_resolver.get(),
                        &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);
  resolver_ptr->ResolveHost(net::HostPortPair("localhost", 160),
                            std::move(optional_parameters),
                            std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  resolver_ptr = nullptr;
  run_loop.Run();

  // Request should be cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Callback should have been called.
  EXPECT_EQ(&resolver, shutdown_resolver);
}

TEST_F(HostResolverTest, CloseBinding_SubsequentRequest) {
  mojom::HostResolverPtr resolver_ptr;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(mojo::MakeRequest(&resolver_ptr),
                        std::move(shutdown_callback), inner_resolver.get(),
                        &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, nullptr);
  resolver_ptr->ResolveHost(net::HostPortPair("localhost", 160), nullptr,
                            std::move(response_client_ptr));

  resolver_ptr = nullptr;
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be ERR_FAILED or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(response_client.result_error() == net::ERR_FAILED ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Callback should have been called.
  EXPECT_EQ(&resolver, shutdown_resolver);

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojom::ResolveHostClientPtr response_client_ptr2;
  TestResolveHostClient response_client2(&response_client_ptr2, &run_loop2);
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

}  // namespace
}  // namespace network
