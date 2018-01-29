// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/udp_socket_test_util.h"

#include <utility>

#include "base/run_loop.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace test {

int UDPSocketTestHelper::OpenSync(mojom::UDPSocketPtr* socket,
                                  net::AddressFamily address_family) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket->get()->Open(
      address_family,
      base::BindOnce(
          [](base::RunLoop* run_loop, int* result_out, int result) {
            *result_out = result;
            run_loop->Quit();
          },
          base::Unretained(&run_loop), base::Unretained(&net_error)));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::ConnectSync(mojom::UDPSocketPtr* socket,
                                     const net::IPEndPoint& remote_addr,
                                     net::IPEndPoint* local_addr_out) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket->get()->Connect(
      remote_addr,
      base::BindOnce(
          [](base::RunLoop* run_loop, int* result_out,
             net::IPEndPoint* local_addr_out, int result,
             const base::Optional<net::IPEndPoint>& local_addr) {
            *result_out = result;
            if (local_addr) {
              *local_addr_out = local_addr.value();
            }
            run_loop->Quit();
          },
          base::Unretained(&run_loop), base::Unretained(&net_error),
          base::Unretained(local_addr_out)));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::BindSync(mojom::UDPSocketPtr* socket,
                                  const net::IPEndPoint& local_addr,
                                  net::IPEndPoint* local_addr_out) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket->get()->Bind(
      local_addr, base::BindOnce(
                      [](base::RunLoop* run_loop, int* result_out,
                         net::IPEndPoint* local_addr_out, int result,
                         const base::Optional<net::IPEndPoint>& local_addr) {
                        *result_out = result;
                        if (local_addr) {
                          *local_addr_out = local_addr.value();
                        }
                        run_loop->Quit();
                      },
                      base::Unretained(&run_loop), base::Unretained(&net_error),
                      base::Unretained(local_addr_out)));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SendToSync(mojom::UDPSocketPtr* socket,
                                    const net::IPEndPoint& remote_addr,
                                    const std::vector<uint8_t>& input) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket->get()->SendTo(
      remote_addr, input,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      base::BindOnce(
          [](base::RunLoop* run_loop, int* result_out, int result) {
            *result_out = result;
            run_loop->Quit();
          },
          base::Unretained(&run_loop), base::Unretained(&net_error)));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SendSync(mojom::UDPSocketPtr* socket,
                                  const std::vector<uint8_t>& input) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket->get()->Send(
      input,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      base::BindOnce(
          [](base::RunLoop* run_loop, int* result_out, int result) {
            *result_out = result;
            run_loop->Quit();
          },
          base::Unretained(&run_loop), base::Unretained(&net_error)));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SetSendBufferSizeSync(mojom::UDPSocketPtr* socket,
                                               size_t size) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket->get()->SetSendBufferSize(
      size, base::BindOnce(
                [](base::RunLoop* run_loop, int* result_out, int result) {
                  *result_out = result;
                  run_loop->Quit();
                },
                base::Unretained(&run_loop), base::Unretained(&net_error)));
  run_loop.Run();
  return net_error;
}

int UDPSocketTestHelper::SetReceiveBufferSizeSync(mojom::UDPSocketPtr* socket,
                                                  size_t size) {
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  socket->get()->SetReceiveBufferSize(
      size, base::BindOnce(
                [](base::RunLoop* run_loop, int* result_out, int result) {
                  *result_out = result;
                  run_loop->Quit();
                },
                base::Unretained(&run_loop), base::Unretained(&net_error)));
  run_loop.Run();
  return net_error;
}

UDPSocketReceiverImpl::ReceivedResult::ReceivedResult(
    int net_error_arg,
    const base::Optional<net::IPEndPoint>& src_addr_arg,
    base::Optional<std::vector<uint8_t>> data_arg)
    : net_error(net_error_arg),
      src_addr(src_addr_arg),
      data(std::move(data_arg)) {}

UDPSocketReceiverImpl::ReceivedResult::ReceivedResult(
    const ReceivedResult& other) = default;

UDPSocketReceiverImpl::ReceivedResult::~ReceivedResult() {}

UDPSocketReceiverImpl::UDPSocketReceiverImpl()
    : run_loop_(std::make_unique<base::RunLoop>()),
      expected_receive_count_(0) {}

UDPSocketReceiverImpl::~UDPSocketReceiverImpl() {}

void UDPSocketReceiverImpl::WaitForReceivedResults(size_t count) {
  DCHECK_LE(results_.size(), count);
  DCHECK_EQ(0u, expected_receive_count_);

  if (results_.size() == count)
    return;

  expected_receive_count_ = count;
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void UDPSocketReceiverImpl::OnReceived(
    int32_t result,
    const base::Optional<net::IPEndPoint>& src_addr,
    base::Optional<base::span<const uint8_t>> data) {
  // OnReceive() API contracts specifies that this method will not be called
  // with a |result| that is > 0.
  DCHECK_GE(0, result);
  DCHECK(result < 0 || data);

  results_.emplace_back(result, src_addr,
                        data ? base::make_optional(std::vector<uint8_t>(
                                   data.value().begin(), data.value().end()))
                             : base::nullopt);
  if (results_.size() == expected_receive_count_) {
    expected_receive_count_ = 0;
    run_loop_->Quit();
  }
}

}  // namespace test

}  // namespace network
