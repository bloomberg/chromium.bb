// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_callback.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/server_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/socket_factory.h"
#include "services/network/tcp_connected_socket.h"
#include "services/network/tcp_server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// A mock ServerSocket that completes Accept() using a specified result.
class MockServerSocket : public net::ServerSocket {
 public:
  MockServerSocket() {}
  ~MockServerSocket() override {}

  // net::ServerSocket implementation.
  int Listen(const net::IPEndPoint& address, int backlog) override {
    return net::OK;
  }

  int GetLocalAddress(net::IPEndPoint* address) const override {
    return net::OK;
  }

  int Accept(std::unique_ptr<net::StreamSocket>* socket,
             const net::CompletionCallback& callback) override {
    DCHECK(accept_callback_.is_null());
    if (accept_result_ == net::OK && mode_ == net::SYNCHRONOUS)
      *socket = CreateMockAcceptSocket();
    if (mode_ == net::ASYNC || accept_result_ == net::ERR_IO_PENDING) {
      accept_socket_ = socket;
      accept_callback_ = callback;
    }
    run_loop_.Quit();

    if (mode_ == net::ASYNC) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&MockServerSocket::CompleteAccept,
                                    base::Unretained(this), accept_result_));
      return net::ERR_IO_PENDING;
    }
    return accept_result_;
  }

  void SetAcceptResult(net::IoMode mode, int result) {
    // It doesn't make sense to return net::ERR_IO_PENDING asynchronously.
    DCHECK(!(mode == net::ASYNC && result == net::ERR_IO_PENDING));

    mode_ = mode;
    accept_result_ = result;
  }

  void WaitForFirstAccept() { run_loop_.Run(); }

  void CompleteAccept(int result) {
    DCHECK(!accept_callback_.is_null());
    DCHECK_NE(net::ERR_IO_PENDING, result);

    *accept_socket_ = CreateMockAcceptSocket();
    accept_socket_ = nullptr;
    base::ResetAndReturn(&accept_callback_).Run(result);
  }

 private:
  // Must live longer than all SocketDataProviders.
  const net::MockRead kReads[1] = {
      net::MockRead(net::ASYNC, net::ERR_IO_PENDING)};

  std::unique_ptr<net::StreamSocket> CreateMockAcceptSocket() {
    auto data_provider = std::make_unique<net::StaticSocketDataProvider>(
        kReads, arraysize(kReads), nullptr, 0);
    data_provider->set_connect_data(
        net::MockConnect(net::SYNCHRONOUS, net::OK));
    auto mock_socket = std::make_unique<net::MockTCPClientSocket>(
        net::AddressList(), nullptr /*netlog*/, data_provider.get());
    data_providers_.push_back(std::move(data_provider));
    EXPECT_EQ(net::OK, mock_socket->Connect(base::DoNothing()));
    return std::move(mock_socket);
  }

  net::IoMode mode_ = net::SYNCHRONOUS;
  int accept_result_ = net::OK;
  net::CompletionCallback accept_callback_;
  std::unique_ptr<net::StreamSocket>* accept_socket_;
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> data_providers_;
};

class TestTCPConnectedSocketObserver
    : public mojom::TCPConnectedSocketObserver {
 public:
  TestTCPConnectedSocketObserver() : binding_(this) {}
  ~TestTCPConnectedSocketObserver() override {
    EXPECT_EQ(net::OK, read_error_);
    EXPECT_EQ(net::OK, write_error_);
  }

  // Returns a mojo pointer. This can only be called once.
  mojom::TCPConnectedSocketObserverPtr GetObserverPtr() {
    DCHECK(!binding_);
    mojom::TCPConnectedSocketObserverPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // Waits for Read and Write error. Returns the error observed.
  int WaitForReadError() {
    read_loop_.Run();
    int error = read_error_;
    read_error_ = net::OK;
    return error;
  }

  int WaitForWriteError() {
    write_loop_.Run();
    int error = write_error_;
    write_error_ = net::OK;
    return error;
  }

 private:
  // mojom::TCPConnectedSocketObserver implementation.
  void OnReadError(int net_error) override {
    read_error_ = net_error;
    read_loop_.Quit();
  }

  void OnWriteError(int net_error) override {
    write_error_ = net_error;
    write_loop_.Quit();
  }

  int read_error_ = net::OK;
  int write_error_ = net::OK;
  base::RunLoop read_loop_;
  base::RunLoop write_loop_;
  mojo::Binding<mojom::TCPConnectedSocketObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestTCPConnectedSocketObserver);
};

// A server implemented using mojom::TCPServerSocket. It owns the server socket
// pointer and as well as client connections. SendData() and StartReading()
// operate on the newest client connection.
class TestServer {
 public:
  TestServer()
      : TestServer(net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)) {}
  explicit TestServer(const net::IPEndPoint& server_addr)
      : factory_(nullptr),
        readable_handle_watcher_(FROM_HERE,
                                 mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
        server_addr_(server_addr) {}
  ~TestServer() {}

  void Start(uint32_t backlog) {
    int net_error = net::ERR_FAILED;
    base::RunLoop run_loop;
    factory_.CreateTCPServerSocket(
        server_addr_, backlog, TRAFFIC_ANNOTATION_FOR_TESTS,
        mojo::MakeRequest(&server_socket_),
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_out,
               net::IPEndPoint* local_addr_out, int result,
               const base::Optional<net::IPEndPoint>& local_addr) {
              *result_out = result;
              if (local_addr)
                *local_addr_out = local_addr.value();
              run_loop->Quit();
            },
            base::Unretained(&run_loop), base::Unretained(&net_error),
            base::Unretained(&server_addr_)));
    run_loop.Run();
    EXPECT_EQ(net::OK, net_error);
  }

  // Accepts one connection. Upon successful completion, |callback| will be
  // invoked.
  void AcceptOneConnection(net::CompletionOnceCallback callback) {
    observer_ = std::make_unique<TestTCPConnectedSocketObserver>();
    server_socket_->Accept(
        observer_->GetObserverPtr(),
        base::BindOnce(&TestServer::OnAccept, base::Unretained(this),
                       std::move(callback)));
  }

  // Sends data over the most recent connection that is established.
  void SendData(const std::string& msg) {
    EXPECT_TRUE(
        mojo::common::BlockingCopyFromString(msg, server_socket_send_handle_));
  }

  // Starts reading. Can be called multiple times. It cancels any previous
  // StartReading(). Once |expected_contents| is read, |callback| will be
  // invoked. If an error occurs or the pipe is broken before read can
  // complete, |callback| will be run, but ADD_FAILURE() will be called.
  void StartReading(const std::string& expected_contents,
                    base::OnceClosure callback) {
    readable_handle_watcher_.Cancel();
    received_contents_.clear();
    expected_contents_ = expected_contents;
    read_callback_ = std::move(callback);
    readable_handle_watcher_.Watch(
        server_socket_receive_handle_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&TestServer::OnReadable, base::Unretained(this)));
  }

  void DestroyServerSocket() { server_socket_.reset(); }

  const net::IPEndPoint& server_addr() { return server_addr_; }

 private:
  void OnAccept(net::CompletionOnceCallback callback,
                int result,
                const base::Optional<net::IPEndPoint>& remote_addr,
                mojom::TCPConnectedSocketPtr connected_socket,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
    connected_sockets_.push_back(std::move(connected_socket));
    server_socket_receive_handle_ = std::move(receive_pipe_handle);
    server_socket_send_handle_ = std::move(send_pipe_handle);
    std::move(callback).Run(result);
  }

  void OnReadable(MojoResult result) {
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "Unexpected broken pipe with error: " << result;
      EXPECT_EQ(expected_contents_, received_contents_);
      std::move(read_callback_).Run();
      return;
    }
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    result = server_socket_receive_handle_->ReadData(buffer, &read_size,
                                                     MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      return;
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "Unexpected read error: " << result;
      EXPECT_EQ(expected_contents_, received_contents_);
      std::move(read_callback_).Run();
      return;
    }

    received_contents_.append(buffer, read_size);

    if (received_contents_.size() == expected_contents_.size()) {
      EXPECT_EQ(expected_contents_, received_contents_);
      std::move(read_callback_).Run();
    }
  }

  SocketFactory factory_;
  mojom::TCPServerSocketPtr server_socket_;
  std::vector<mojom::TCPConnectedSocketPtr> connected_sockets_;
  std::unique_ptr<TestTCPConnectedSocketObserver> observer_;
  mojo::ScopedDataPipeConsumerHandle server_socket_receive_handle_;
  mojo::ScopedDataPipeProducerHandle server_socket_send_handle_;
  mojo::SimpleWatcher readable_handle_watcher_;
  net::IPEndPoint server_addr_;
  std::string expected_contents_;
  base::OnceClosure read_callback_;
  std::string received_contents_;
};

}  // namespace

class TCPSocketTest : public testing::Test {
 public:
  TCPSocketTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        factory_(nullptr /*net_log*/) {}
  ~TCPSocketTest() override {}

  // Reads |num_bytes| from |handle| or reads until an error occurs. Returns the
  // bytes read as a string.
  std::string Read(mojo::ScopedDataPipeConsumerHandle* handle,
                   size_t num_bytes) {
    std::string received_contents;
    while (received_contents.size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::vector<char> buffer(num_bytes);
      uint32_t read_size = static_cast<uint32_t>(num_bytes);
      MojoResult result = handle->get().ReadData(buffer.data(), &read_size,
                                                 MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return received_contents;
      received_contents.append(buffer.data(), read_size);
    }
    return received_contents;
  }

  // Creates a TCPServerSocket with the mock server socket, |socket|.
  void CreateServerSocketWithMockSocket(
      uint32_t backlog,
      mojom::TCPServerSocketRequest request,
      std::unique_ptr<net::ServerSocket> socket) {
    auto server_socket_impl = std::make_unique<TCPServerSocket>(
        &factory_, nullptr /*netlog*/, TRAFFIC_ANNOTATION_FOR_TESTS);
    server_socket_impl->SetSocketForTest(std::move(socket));
    net::IPEndPoint local_addr;
    EXPECT_EQ(net::OK,
              server_socket_impl->Listen(local_addr, backlog, &local_addr));
    tcp_server_socket_bindings_.AddBinding(std::move(server_socket_impl),
                                           std::move(request));
  }

  // Creates a TCPConnectedSocket with a mock client socket, |socket|.
  // This can be only called once.
  void CreateConnectedSocketWithMockSocket(
      mojom::TCPConnectedSocketRequest request,
      mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
      mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
      std::unique_ptr<net::MockTCPClientSocket> socket) {
    net::TestCompletionCallback dummy_callback;
    EXPECT_EQ(net::OK, socket->Connect(dummy_callback.callback()));
    auto socket_impl = std::make_unique<TCPConnectedSocket>(
        test_observer_.GetObserverPtr(), std::move(socket),
        std::move(receive_pipe_handle), std::move(send_pipe_handle),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    tcp_connected_socket_bindings_.AddBinding(std::move(socket_impl),
                                              std::move(request));
  }

  int CreateTCPConnectedSocketSync(
      mojom::TCPConnectedSocketRequest request,
      mojom::TCPConnectedSocketObserverPtr observer,
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::IPEndPoint& remote_addr,
      mojo::ScopedDataPipeConsumerHandle* receive_pipe_handle_out,
      mojo::ScopedDataPipeProducerHandle* send_pipe_handle_out) {
    net::AddressList remote_addr_list(remote_addr);
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    factory_.CreateTCPConnectedSocket(
        local_addr, remote_addr_list, TRAFFIC_ANNOTATION_FOR_TESTS,
        std::move(request), std::move(observer),
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_out,
               mojo::ScopedDataPipeConsumerHandle* consumer_handle,
               mojo::ScopedDataPipeProducerHandle* producer_handle, int result,
               mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
               mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              *result_out = result;
              *consumer_handle = std::move(receive_pipe_handle);
              *producer_handle = std::move(send_pipe_handle);
              run_loop->Quit();
            },
            base::Unretained(&run_loop), base::Unretained(&net_error),
            receive_pipe_handle_out, send_pipe_handle_out));
    run_loop.Run();
    return net_error;
  }

  TestTCPConnectedSocketObserver* observer() { return &test_observer_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  SocketFactory factory_;
  TestTCPConnectedSocketObserver test_observer_;
  mojo::StrongBindingSet<mojom::TCPServerSocket> tcp_server_socket_bindings_;
  mojo::StrongBindingSet<mojom::TCPConnectedSocket>
      tcp_connected_socket_bindings_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketTest);
};

TEST_F(TCPSocketTest, ReadAndWrite) {
  const struct TestData {
    base::Optional<net::IPEndPoint> client_addr;
    net::IPEndPoint server_addr;
  } kTestCases[] = {
      {base::nullopt, net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)},
      {base::nullopt, net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)},
      {net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)},
      {net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)},
  };
  for (auto test : kTestCases) {
    TestServer server(test.server_addr);
    server.Start(1 /*backlog*/);
    net::TestCompletionCallback accept_callback;
    server.AcceptOneConnection(accept_callback.callback());

    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

    mojom::TCPConnectedSocketPtr client_socket;
    TestTCPConnectedSocketObserver observer;
    EXPECT_EQ(net::OK,
              CreateTCPConnectedSocketSync(
                  mojo::MakeRequest(&client_socket), observer.GetObserverPtr(),
                  test.client_addr, server.server_addr(),
                  &client_socket_receive_handle, &client_socket_send_handle));
    ASSERT_EQ(net::OK, accept_callback.WaitForResult());

    // Test sending data from server to client.
    const char kTestMsg[] = "hello";
    server.SendData(kTestMsg);
    EXPECT_EQ(kTestMsg,
              Read(&client_socket_receive_handle, arraysize(kTestMsg) - 1));

    // Test sending data from client to server.
    base::RunLoop read_run_loop;
    server.StartReading(kTestMsg, read_run_loop.QuitClosure());
    EXPECT_TRUE(mojo::common::BlockingCopyFromString(
        kTestMsg, client_socket_send_handle));
    read_run_loop.Run();
  }
}

TEST_F(TCPSocketTest, CannotConnectToWrongInterface) {
  const struct TestData {
    net::IPEndPoint client_addr;
    net::IPEndPoint server_addr;
  } kTestCases[] = {
      {net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)},
      {net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)},
  };
  for (auto test : kTestCases) {
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

    TestTCPConnectedSocketObserver observer;
    TestServer server(test.server_addr);
    server.Start(1 /*backlog*/);
    net::TestCompletionCallback accept_callback;
    server.AcceptOneConnection(accept_callback.callback());

    mojom::TCPConnectedSocketPtr client_socket;
    int result = CreateTCPConnectedSocketSync(
        mojo::MakeRequest(&client_socket), observer.GetObserverPtr(),
        test.client_addr, server.server_addr(), &client_socket_receive_handle,
        &client_socket_send_handle);
    // Both net::ERR_INVALID_ARGUMENT and net::ERR_ADDRESS_UNREACHABLE can be
    // returned. On Linux, for eample, the former is returned when talking ipv4
    // to a ipv6 remote, and the latter is returned when talking ipv6 to a ipv4
    // remote. net::ERR_CONNECTION_FAILED is returned on Windows.
    EXPECT_TRUE(result == net::ERR_CONNECTION_FAILED ||
                result == net::ERR_INVALID_ARGUMENT ||
                result == net::ERR_ADDRESS_UNREACHABLE)
        << "actual result: " << result;
  }
}

TEST_F(TCPSocketTest, ServerReceivesMultipleAccept) {
  uint32_t backlog = 10;
  TestServer server;
  server.Start(backlog);

  std::vector<std::unique_ptr<net::TestCompletionCallback>> accept_callbacks;
  // Issue |backlog| Accepts(), so the queue is filled up.
  for (size_t i = 0; i < backlog; ++i) {
    auto callback = std::make_unique<net::TestCompletionCallback>();
    server.AcceptOneConnection(callback->callback());
    accept_callbacks.push_back(std::move(callback));
  }
  // Accept() beyond the queue size should fail immediately.
  net::TestCompletionCallback callback;
  server.AcceptOneConnection(callback.callback());
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, callback.WaitForResult());

  // After handling incoming connections, all callbacks should now complete.
  std::vector<mojom::TCPConnectedSocketPtr> client_sockets;
  for (size_t i = 0; i < backlog; ++i) {
    TestTCPConnectedSocketObserver observer;
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
    mojom::TCPConnectedSocketPtr client_socket;
    EXPECT_EQ(net::OK,
              CreateTCPConnectedSocketSync(
                  mojo::MakeRequest(&client_socket), observer.GetObserverPtr(),
                  base::nullopt /*local_addr*/, server.server_addr(),
                  &client_socket_receive_handle, &client_socket_send_handle));
    client_sockets.push_back(std::move(client_socket));
  }
  for (const auto& callback : accept_callbacks) {
    EXPECT_EQ(net::OK, callback->WaitForResult());
  }
}

// Tests that if a socket is closed, the other side can observe that the pipes
// are broken.
TEST_F(TCPSocketTest, SocketClosed) {
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  mojom::TCPConnectedSocketPtr client_socket;

  TestTCPConnectedSocketObserver observer;
  const char kTestMsg[] = "hello";
  auto server = std::make_unique<TestServer>();
  server->Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server->AcceptOneConnection(accept_callback.callback());

  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                mojo::MakeRequest(&client_socket), observer.GetObserverPtr(),
                base::nullopt /*local_addr*/, server->server_addr(),
                &client_socket_receive_handle, &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Send some data from server to client.
  server->SendData(kTestMsg);
  EXPECT_EQ(kTestMsg,
            Read(&client_socket_receive_handle, arraysize(kTestMsg) - 1));
  // Resetting the |server| destroys the TCPConnectedSocket ptr owned by the
  // server.
  server = nullptr;

  // Read should return EOF.
  EXPECT_EQ("", Read(&client_socket_receive_handle, 1));

  // Read from |client_socket_receive_handle| again should return that the pipe
  // is broken.
  char buffer[16];
  uint32_t read_size = sizeof(buffer);
  MojoResult mojo_result = client_socket_receive_handle->ReadData(
      buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, mojo_result);
  EXPECT_TRUE(client_socket_receive_handle->QuerySignalsState().peer_closed());

  // Send pipe should be closed.
  while (true) {
    base::RunLoop().RunUntilIdle();
    uint32_t size = arraysize(kTestMsg);
    MojoResult r = client_socket_send_handle->WriteData(
        kTestMsg, &size, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    if (r == MOJO_RESULT_SHOULD_WAIT)
      continue;
    if (r == MOJO_RESULT_FAILED_PRECONDITION)
      break;
  }
  EXPECT_TRUE(client_socket_send_handle->QuerySignalsState().peer_closed());
  int result = observer.WaitForWriteError();
  EXPECT_TRUE(result == net::ERR_CONNECTION_RESET ||
              result == net::ERR_CONNECTION_ABORTED)
      << "actual result: " << result;
}

TEST_F(TCPSocketTest, ReadPipeClosed) {
  TestTCPConnectedSocketObserver observer;
  const char kTestMsg[] = "hello";
  TestServer server;
  server.Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server.AcceptOneConnection(accept_callback.callback());

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  mojom::TCPConnectedSocketPtr client_socket;
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                mojo::MakeRequest(&client_socket), observer.GetObserverPtr(),
                base::nullopt /*local_addr*/, server.server_addr(),
                &client_socket_receive_handle, &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Close |client_socket_receive_handle|. The socket should remain open.
  client_socket_receive_handle.reset();

  // Send should proceed as normal.
  base::RunLoop read_run_loop;
  server.StartReading(kTestMsg, read_run_loop.QuitClosure());
  EXPECT_TRUE(mojo::common::BlockingCopyFromString(kTestMsg,
                                                   client_socket_send_handle));
  read_run_loop.Run();
}

TEST_F(TCPSocketTest, WritePipeClosed) {
  TestTCPConnectedSocketObserver observer;
  const char kTestMsg[] = "hello";
  TestServer server;
  server.Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server.AcceptOneConnection(accept_callback.callback());

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  mojom::TCPConnectedSocketPtr client_socket;
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                mojo::MakeRequest(&client_socket), observer.GetObserverPtr(),
                base::nullopt /*local_addr*/, server.server_addr(),
                &client_socket_receive_handle, &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Close |client_socket_send_handle|. The socket should remain open.
  client_socket_send_handle.reset();

  // Receive should proceed as normal.
  server.SendData(kTestMsg);
  EXPECT_EQ(kTestMsg,
            Read(&client_socket_receive_handle, arraysize(kTestMsg) - 1));
}

// Tests that if the server socket is destroyed, any connected sockets that it
// handed out remain alive.
TEST_F(TCPSocketTest, ServerSocketClosedAcceptedSocketAlive) {
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  TestTCPConnectedSocketObserver observer;
  const char kTestMsg[] = "hello";
  TestServer server;
  server.Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server.AcceptOneConnection(accept_callback.callback());

  mojom::TCPConnectedSocketPtr client_socket;
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                mojo::MakeRequest(&client_socket), observer.GetObserverPtr(),
                base::nullopt /*local_addr*/, server.server_addr(),
                &client_socket_receive_handle, &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Now destroys the server socket.
  server.DestroyServerSocket();
  base::RunLoop().RunUntilIdle();

  // Sending and receiving should still work.
  server.SendData(kTestMsg);
  EXPECT_EQ(kTestMsg,
            Read(&client_socket_receive_handle, arraysize(kTestMsg) - 1));

  base::RunLoop read_run_loop;
  server.StartReading(kTestMsg, read_run_loop.QuitClosure());
  EXPECT_TRUE(mojo::common::BlockingCopyFromString(kTestMsg,
                                                   client_socket_send_handle));
  read_run_loop.Run();
}

// Tests both async and sync cases.
class TCPSocketWithMockSocketTest
    : public TCPSocketTest,
      public ::testing::WithParamInterface<net::IoMode> {};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        TCPSocketWithMockSocketTest,
                        testing::Values(net::SYNCHRONOUS, net::ASYNC));

// Tests that a server socket handles Accept() correctly when the underlying
// implementation completes Accept() in sync and async mode.
TEST_P(TCPSocketWithMockSocketTest,
       ServerAcceptClientConnectionWithMockSocket) {
  net::IoMode mode = GetParam();
  auto mock_server_socket = std::make_unique<MockServerSocket>();
  MockServerSocket* mock_server_socket_raw = mock_server_socket.get();
  mojom::TCPServerSocketPtr server_socket;
  uint32_t kBacklog = 10;
  // Use a mock socket to control net::ServerSocket::Accept() behavior.
  CreateServerSocketWithMockSocket(kBacklog, mojo::MakeRequest(&server_socket),
                                   std::move(mock_server_socket));

  // Complete first Accept() using manual completion via CompleteAccept().
  mock_server_socket_raw->SetAcceptResult(net::SYNCHRONOUS,
                                          net::ERR_IO_PENDING);
  std::vector<std::unique_ptr<net::TestCompletionCallback>> accept_callbacks;
  for (size_t i = 0; i < kBacklog; ++i) {
    auto callback = std::make_unique<net::TestCompletionCallback>();
    TestTCPConnectedSocketObserver observer;
    server_socket->Accept(
        observer.GetObserverPtr(),
        base::BindOnce(
            [](net::CompletionOnceCallback callback, int result,
               const base::Optional<net::IPEndPoint>& remote_addr,
               mojom::TCPConnectedSocketPtr connected_socket,
               mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
               mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              std::move(callback).Run(result);
            },
            std::move(callback->callback())));
    accept_callbacks.push_back(std::move(callback));
  }

  mock_server_socket_raw->WaitForFirstAccept();
  mock_server_socket_raw->SetAcceptResult(mode, net::OK);
  mock_server_socket_raw->CompleteAccept(net::OK);

  // First net::ServerSocket::Accept() will complete asynchronously
  // internally. Other queued Accept() will complete
  // synchronously/asynchronously depending on |mode| internally.
  for (const auto& callback : accept_callbacks) {
    EXPECT_EQ(net::OK, callback->WaitForResult());
  }

  // New Accept() should complete synchronously internally. Make sure this is
  // okay.
  auto callback = std::make_unique<net::TestCompletionCallback>();
  TestTCPConnectedSocketObserver observer;
  server_socket->Accept(
      observer.GetObserverPtr(),
      base::BindOnce(
          [](net::CompletionOnceCallback callback, int result,
             const base::Optional<net::IPEndPoint>& remote_addr,
             mojom::TCPConnectedSocketPtr connected_socket,
             mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
             mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
            std::move(callback).Run(result);
          },
          std::move(callback->callback())));
  EXPECT_EQ(net::OK, callback->WaitForResult());
}

TEST_P(TCPSocketWithMockSocketTest, ReadAndWriteMultiple) {
  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle =
      std::move(receive_pipe.consumer_handle);
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle =
      std::move(send_pipe.producer_handle);

  mojom::TCPConnectedSocketPtr client_socket;
  const char kTestMsg[] = "abcdefghij";
  const size_t kMsgSize = arraysize(kTestMsg) - 1;
  const int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (size_t i = 0; i < kMsgSize; ++i) {
      reads.push_back(net::MockRead(mode, &kTestMsg[i], 1, sequence_number++));
    }
    if (j == kNumIterations - 1) {
      reads.push_back(net::MockRead(mode, net::OK, sequence_number++));
    }
    for (size_t i = 0; i < kMsgSize; ++i) {
      writes.push_back(
          net::MockWrite(mode, &kTestMsg[i], 1, sequence_number++));
    }
  }
  net::StaticSocketDataProvider data_provider(reads.data(), reads.size(),
                                              writes.data(), writes.size());
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  auto mock_socket = std::make_unique<net::MockTCPClientSocket>(
      net::AddressList(), nullptr /*netlog*/, &data_provider);

  CreateConnectedSocketWithMockSocket(mojo::MakeRequest(&client_socket),
                                      std::move(receive_pipe.producer_handle),
                                      std::move(send_pipe.consumer_handle),
                                      std::move(mock_socket));

  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading kMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kMsgSize));
    // Write multiple times.
    for (size_t i = 0; i < kMsgSize; ++i) {
      uint32_t num_bytes = 1;
      EXPECT_EQ(MOJO_RESULT_OK,
                client_socket_send_handle->WriteData(
                    &kTestMsg[i], &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
      // Flush the 1 byte write.
      base::RunLoop().RunUntilIdle();
    }
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketWithMockSocketTest, PartialStreamSocketWrite) {
  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle =
      std::move(receive_pipe.consumer_handle);
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle =
      std::move(send_pipe.producer_handle);

  mojom::TCPConnectedSocketPtr client_socket;
  const char kTestMsg[] = "abcdefghij";
  const size_t kMsgSize = arraysize(kTestMsg) - 1;
  const int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (size_t i = 0; i < kMsgSize; ++i) {
      reads.push_back(net::MockRead(mode, &kTestMsg[i], 1, sequence_number++));
    }
    if (j == kNumIterations - 1) {
      reads.push_back(net::MockRead(mode, net::OK, sequence_number++));
    }
    for (size_t i = 0; i < kMsgSize; ++i) {
      writes.push_back(
          net::MockWrite(mode, &kTestMsg[i], 1, sequence_number++));
    }
  }
  net::StaticSocketDataProvider data_provider(reads.data(), reads.size(),
                                              writes.data(), writes.size());
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  auto mock_socket = std::make_unique<net::MockTCPClientSocket>(
      net::AddressList(), nullptr /*netlog*/, &data_provider);

  CreateConnectedSocketWithMockSocket(mojo::MakeRequest(&client_socket),
                                      std::move(receive_pipe.producer_handle),
                                      std::move(send_pipe.consumer_handle),
                                      std::move(mock_socket));

  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading kMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kMsgSize));
    // Write twice, each with kMsgSize/2 bytes which is bigger than the 1-byte
    // MockWrite(). This is to exercise that StreamSocket::Write() can do
    // partial write.
    uint32_t first_write_size = kMsgSize / 2;
    EXPECT_EQ(MOJO_RESULT_OK,
              client_socket_send_handle->WriteData(
                  &kTestMsg[0], &first_write_size, MOJO_WRITE_DATA_FLAG_NONE));
    // Flush the kMsgSize/2 byte write.
    base::RunLoop().RunUntilIdle();
    uint32_t second_write_size = kMsgSize - first_write_size;
    EXPECT_EQ(MOJO_RESULT_OK,
              client_socket_send_handle->WriteData(&kTestMsg[first_write_size],
                                                   &second_write_size,
                                                   MOJO_WRITE_DATA_FLAG_NONE));
    // Flush the kMsgSize/2 byte write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketWithMockSocketTest, ReadError) {
  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle =
      std::move(receive_pipe.consumer_handle);
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle =
      std::move(send_pipe.producer_handle);

  mojom::TCPConnectedSocketPtr client_socket;
  net::IoMode mode = GetParam();
  net::MockRead reads[] = {net::MockRead(mode, net::ERR_FAILED)};
  const char kTestMsg[] = "hello!";
  net::MockWrite writes[] = {
      net::MockWrite(mode, kTestMsg, arraysize(kTestMsg) - 1, 0)};
  net::StaticSocketDataProvider data_provider(reads, arraysize(reads), writes,
                                              arraysize(writes));
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  auto mock_socket = std::make_unique<net::MockTCPClientSocket>(
      net::AddressList(), nullptr /*netlog*/, &data_provider);

  CreateConnectedSocketWithMockSocket(mojo::MakeRequest(&client_socket),
                                      std::move(receive_pipe.producer_handle),
                                      std::move(send_pipe.consumer_handle),
                                      std::move(mock_socket));

  EXPECT_EQ("", Read(&client_socket_receive_handle, 1));
  EXPECT_EQ(net::ERR_FAILED, observer()->WaitForReadError());
  // Writes can proceed even though there is a read error.
  uint32_t num_bytes = arraysize(kTestMsg) - 1;
  EXPECT_EQ(MOJO_RESULT_OK,
            client_socket_send_handle->WriteData(&kTestMsg, &num_bytes,
                                                 MOJO_WRITE_DATA_FLAG_NONE));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketWithMockSocketTest, WriteError) {
  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle =
      std::move(receive_pipe.consumer_handle);
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle =
      std::move(send_pipe.producer_handle);

  mojom::TCPConnectedSocketPtr client_socket;
  net::IoMode mode = GetParam();
  const char kTestMsg[] = "hello!";
  net::MockRead reads[] = {
      net::MockRead(mode, kTestMsg, arraysize(kTestMsg) - 1, 0),
      net::MockRead(mode, net::OK)};
  net::MockWrite writes[] = {net::MockWrite(mode, net::ERR_FAILED)};
  net::StaticSocketDataProvider data_provider(reads, arraysize(reads), writes,
                                              arraysize(writes));
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  auto mock_socket = std::make_unique<net::MockTCPClientSocket>(
      net::AddressList(), nullptr /*netlog*/, &data_provider);

  CreateConnectedSocketWithMockSocket(mojo::MakeRequest(&client_socket),
                                      std::move(receive_pipe.producer_handle),
                                      std::move(send_pipe.consumer_handle),
                                      std::move(mock_socket));

  uint32_t num_bytes = arraysize(kTestMsg) - 1;
  EXPECT_EQ(MOJO_RESULT_OK,
            client_socket_send_handle->WriteData(&kTestMsg, &num_bytes,
                                                 MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(net::ERR_FAILED, observer()->WaitForWriteError());
  // Reads can proceed even though there is a read error.
  EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, arraysize(kTestMsg)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

}  // namespace network
