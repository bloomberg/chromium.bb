// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc/mojo_ipc_server.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/mojo_ipc/mojo_ipc_test_util.h"
#include "remoting/host/mojom/testing.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::Return;

using EchoStringHandler = base::RepeatingCallback<void(
    const std::string& input,
    test::mojom::Echo::EchoStringCallback callback)>;

void SendEchoAndVerifyResponse(mojo::Remote<test::mojom::Echo>& echo_remote) {
  base::RunLoop echo_response_run_loop;
  base::MockCallback<test::mojom::Echo::EchoStringCallback> callback;
  EXPECT_CALL(callback, Run("test string"))
      .WillOnce(
          base::test::RunOnceClosure(echo_response_run_loop.QuitClosure()));
  echo_remote->EchoString("test string", callback.Get());
  echo_response_run_loop.Run();
}

}  // namespace

class MojoIpcServerTest : public testing::Test, public test::mojom::Echo {
 public:
  MojoIpcServerTest();
  ~MojoIpcServerTest() override;

  void SetUp() override;

 protected:
  mojo::PlatformChannelEndpoint ConnectToTestServer();
  mojo::Remote<test::mojom::Echo> ConnectAndCreateEchoRemote(
      mojo::IsolatedConnection& client_connection);
  void WaitForInvitationSent();

  std::unique_ptr<MojoIpcServer<test::mojom::Echo>> ipc_server_;
  mojo::ReceiverId last_echo_string_receiver_id_ = 0u;

  // If this is set, EchoString() will run this callback instead of responding
  // to the callback automatically.
  EchoStringHandler echo_string_handler_;

 private:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override;

  void OnInvitationSent();

  mojo::NamedPlatformChannel::ServerName test_server_name_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  // Run loops that wait for MojoIpcServerBase::ObserverForTesting methods to
  // be called.
  std::unique_ptr<base::RunLoop> on_invitation_sent_run_loop_;
};

MojoIpcServerTest::MojoIpcServerTest() {
  test_server_name_ = test::GenerateRandomServerName();
  ipc_server_ = std::make_unique<MojoIpcServer<test::mojom::Echo>>(
      test_server_name_, this);
  ipc_server_->set_on_invitation_sent_callback_for_testing(base::BindRepeating(
      &MojoIpcServerTest::OnInvitationSent, base::Unretained(this)));
  on_invitation_sent_run_loop_ = std::make_unique<base::RunLoop>();
}

MojoIpcServerTest::~MojoIpcServerTest() = default;

void MojoIpcServerTest::SetUp() {
  ipc_server_->StartServer();
  WaitForInvitationSent();
}

mojo::PlatformChannelEndpoint MojoIpcServerTest::ConnectToTestServer() {
  return mojo::NamedPlatformChannel::ConnectToServer(test_server_name_);
}

mojo::Remote<test::mojom::Echo> MojoIpcServerTest::ConnectAndCreateEchoRemote(
    mojo::IsolatedConnection& client_connection) {
  return mojo::Remote<test::mojom::Echo>(mojo::PendingRemote<test::mojom::Echo>(
      client_connection.Connect(ConnectToTestServer()), /* version= */ 0));
}

void MojoIpcServerTest::WaitForInvitationSent() {
  on_invitation_sent_run_loop_->Run();
  on_invitation_sent_run_loop_ = std::make_unique<base::RunLoop>();
}

void MojoIpcServerTest::EchoString(const std::string& input,
                                   EchoStringCallback callback) {
  if (echo_string_handler_) {
    echo_string_handler_.Run(input, std::move(callback));
    return;
  }

  std::move(callback).Run(input);
  last_echo_string_receiver_id_ = ipc_server_->current_receiver();
  ASSERT_EQ(base::GetCurrentProcId(), ipc_server_->current_peer_pid());
}

void MojoIpcServerTest::OnInvitationSent() {
  on_invitation_sent_run_loop_->Quit();
}

TEST_F(MojoIpcServerTest, SendEcho) {
  mojo::IsolatedConnection client_connection;
  auto echo_remote = ConnectAndCreateEchoRemote(client_connection);
  SendEchoAndVerifyResponse(echo_remote);
}

TEST_F(MojoIpcServerTest, DeleteMojoServer_NoLingeringInvitations) {
  ipc_server_.reset();
  base::RunLoop().RunUntilIdle();
  // For posix, the socket doesn't seem to be closed immediately after the
  // isolated connection is deleted, so we wait for 1s to make sure the socket
  // is really closed.
  base::PlatformThread::Sleep(base::Seconds(1));

  auto endpoint = ConnectToTestServer();
  ASSERT_FALSE(endpoint.is_valid());
}

TEST_F(MojoIpcServerTest, DisconnectHandler) {
  base::RunLoop disconnect_run_loop;
  base::MockCallback<base::RepeatingClosure> disconnect_handler;
  EXPECT_CALL(disconnect_handler, Run()).WillOnce([&]() {
    ASSERT_EQ(last_echo_string_receiver_id_, ipc_server_->current_receiver());
    disconnect_run_loop.Quit();
  });
  ipc_server_->set_disconnect_handler(disconnect_handler.Get());

  auto client_connection = std::make_unique<mojo::IsolatedConnection>();
  auto echo_remote = ConnectAndCreateEchoRemote(*client_connection);

  // Must send some IPC to be considered connected.
  SendEchoAndVerifyResponse(echo_remote);

  echo_remote.reset();
  client_connection.reset();
  disconnect_run_loop.Run();
}

TEST_F(MojoIpcServerTest, DeleteMojoServer_RemoteDisconnected) {
  mojo::IsolatedConnection client_connection;
  auto echo_remote = ConnectAndCreateEchoRemote(client_connection);
  SendEchoAndVerifyResponse(echo_remote);

  base::RunLoop disconnect_run_loop;
  echo_remote.set_disconnect_handler(disconnect_run_loop.QuitClosure());
  ipc_server_.reset();
  disconnect_run_loop.Run();
}

TEST_F(MojoIpcServerTest, StopServer_RemoteDisconnected) {
  mojo::IsolatedConnection client_connection;
  auto echo_remote = ConnectAndCreateEchoRemote(client_connection);
  SendEchoAndVerifyResponse(echo_remote);

  base::RunLoop disconnect_run_loop;
  echo_remote.set_disconnect_handler(disconnect_run_loop.QuitClosure());
  ipc_server_->StopServer();
  disconnect_run_loop.Run();
}

TEST_F(MojoIpcServerTest, CloseReceiver_RemoteDisconnected) {
  mojo::IsolatedConnection client_connection;
  auto echo_remote = ConnectAndCreateEchoRemote(client_connection);
  SendEchoAndVerifyResponse(echo_remote);
  ASSERT_EQ(1u, ipc_server_->GetNumberOfActiveConnectionsForTesting());

  base::RunLoop disconnect_run_loop;
  echo_remote.set_disconnect_handler(disconnect_run_loop.QuitClosure());
  ipc_server_->Close(last_echo_string_receiver_id_);
  disconnect_run_loop.Run();
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
}

TEST_F(MojoIpcServerTest, CloseNonexistentReceiver_NoCrash) {
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
  ipc_server_->Close(1u);
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
}

TEST_F(MojoIpcServerTest, RemoteDisconnected_ConnectionRemoved) {
  mojo::IsolatedConnection client_connection;
  auto echo_remote = ConnectAndCreateEchoRemote(client_connection);
  SendEchoAndVerifyResponse(echo_remote);
  ASSERT_EQ(1u, ipc_server_->GetNumberOfActiveConnectionsForTesting());

  base::RunLoop disconnect_run_loop;
  ipc_server_->set_disconnect_handler(disconnect_run_loop.QuitClosure());
  echo_remote.reset();
  disconnect_run_loop.Run();
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
}

TEST_F(MojoIpcServerTest, RemoteDisconnectedBeforeBound_NewInvitationIsSent) {
  mojo::IsolatedConnection client_connection;
  auto handle = client_connection.Connect(ConnectToTestServer());
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { handle.reset(); }));
  WaitForInvitationSent();
}

TEST_F(MojoIpcServerTest, RemoteConnectsAndHangs_NewInvitationIsSent) {
  mojo::IsolatedConnection client_connection;
  auto handle = client_connection.Connect(ConnectToTestServer());
  WaitForInvitationSent();
}

TEST_F(MojoIpcServerTest, ParallelIpcs) {
  base::RunLoop echo_response_run_loop;
  base::MockCallback<EchoStringCallback> echo_mock_callback;
  EXPECT_CALL(echo_mock_callback, Run(_))
      .WillOnce(Return())
      .WillOnce(
          base::test::RunOnceClosure(echo_response_run_loop.QuitClosure()));

  base::MockCallback<EchoStringHandler> mock_echo_string_handler;
  echo_string_handler_ = mock_echo_string_handler.Get();
  std::string first_input;
  EchoStringCallback first_callback;
  EXPECT_CALL(mock_echo_string_handler, Run(_, _))
      .WillOnce([&](const std::string& input, EchoStringCallback callback) {
        first_input = input;
        first_callback = std::move(callback);
      })
      .WillOnce([&](const std::string& input, EchoStringCallback callback) {
        std::move(first_callback).Run(first_input);
        std::move(callback).Run(input);
      });

  mojo::IsolatedConnection client_connection_1;
  auto echo_remote_1 = ConnectAndCreateEchoRemote(client_connection_1);
  echo_remote_1->EchoString("test string 1", echo_mock_callback.Get());
  WaitForInvitationSent();

  mojo::IsolatedConnection client_connection_2;
  auto echo_remote_2 = ConnectAndCreateEchoRemote(client_connection_2);
  echo_remote_2->EchoString("test string 2", echo_mock_callback.Get());
  echo_response_run_loop.Run();
}

}  // namespace remoting
