// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include "base/command_line.h"
#include "base/test/scoped_task_environment.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui_devtools {

// Tests whether the server for Views is properly created so we can connect to
// it.
TEST(UIDevToolsServerTest, ConnectionToViewsServer) {
  static constexpr char fake_flag[] = "ui_devtools";
  // Use port 80 to prevent firewall issues.
  static constexpr int fake_port = 80;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(fake_flag);
  base::test::ScopedTaskEnvironment scoped_task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);

  network::mojom::NetworkServicePtr network_service_ptr;
  network::mojom::NetworkServiceRequest network_service_request =
      mojo::MakeRequest(&network_service_ptr);
  auto network_service =
      network::NetworkService::Create(std::move(network_service_request),
                                      /*netlog=*/nullptr);
  network::mojom::NetworkContextPtr network_context_ptr;
  network_service_ptr->CreateNetworkContext(
      mojo::MakeRequest(&network_context_ptr),
      network::mojom::NetworkContextParams::New());

  std::unique_ptr<UiDevToolsServer> server = UiDevToolsServer::CreateForViews(
      network_context_ptr.get(), fake_flag, fake_port);
  // Connect to the server socket.
  net::AddressList addr(
      net::IPEndPoint(net::IPAddress(127, 0, 0, 1), fake_port));
  auto client_socket = std::make_unique<net::TCPClientSocket>(
      addr, nullptr, nullptr, net::NetLogSource());
  net::TestCompletionCallback callback;
  int connect_result =
      callback.GetResult(client_socket->Connect(callback.callback()));
  ASSERT_EQ(net::OK, connect_result);
  ASSERT_TRUE(client_socket->IsConnected());
}

}  // namespace ui_devtools
