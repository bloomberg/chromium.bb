// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/multinode_test.h"

#include <thread>

#include "ipcz/ipcz.h"
#include "reference_drivers/single_process_reference_driver.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "util/log.h"

namespace ipcz::test {

namespace {

// Launches a new node on a dedicated thread within the same process. All
// connections use the synchronous single-process driver.
class InProcessTestNodeController : public TestNode::TestNodeController {
 public:
  InProcessTestNodeController(DriverMode driver_mode,
                              std::unique_ptr<TestNode> test_node)
      : client_thread_(absl::in_place,
                       &RunTestNode,
                       driver_mode,
                       std::move(test_node)) {}

  ~InProcessTestNodeController() override { ABSL_ASSERT(!client_thread_); }

  // TestNode::TestNodeController:
  bool WaitForShutdown() override {
    if (client_thread_) {
      client_thread_->join();
      client_thread_.reset();
    }

    // In spirit, the point of WaitForShutdown()'s return value is to signal to
    // the running test whether something went wrong in a spawned node. This is
    // necessary to propagate test expectation failures from within child
    // processes when running in a multiprocess test mode.
    //
    // When spawned nodes are running in the main test process however, their
    // test expectation failures already affect the pass/fail state of the
    // running test. In this case there's no need to propagate a redundant
    // failure signal here, hence we always return true.
    return true;
  }

 private:
  static void RunTestNode(DriverMode driver_mode,
                          std::unique_ptr<TestNode> test_node) {
    test_node->Initialize(driver_mode, IPCZ_NO_FLAGS);
    test_node->NodeBody();
  }

  absl::optional<std::thread> client_thread_;
};

}  // namespace

TestNode::~TestNode() {
  for (auto& spawned_node : spawned_nodes_) {
    EXPECT_TRUE(spawned_node->WaitForShutdown());
  }

  // If we never connected to the broker, make sure we don't leak our transport.
  if (transport_ != IPCZ_INVALID_DRIVER_HANDLE) {
    GetDriver().Close(transport_, IPCZ_NO_FLAGS, nullptr);
  }

  CloseThisNode();
}

const IpczDriver& TestNode::GetDriver() const {
  static IpczDriver kInvalidDriver = {};
  switch (driver_mode_) {
    case DriverMode::kSync:
      return reference_drivers::kSingleProcessReferenceDriver;

    default:
      // Other modes not yet supported.
      ABSL_ASSERT(false);
      return kInvalidDriver;
  }
}

void TestNode::Initialize(DriverMode driver_mode,
                          IpczCreateNodeFlags create_node_flags) {
  driver_mode_ = driver_mode;

  ABSL_ASSERT(node_ == IPCZ_INVALID_HANDLE);
  const IpczResult result =
      ipcz().CreateNode(&GetDriver(), IPCZ_INVALID_DRIVER_HANDLE,
                        create_node_flags, nullptr, &node_);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
}

void TestNode::ConnectToBroker(absl::Span<IpczHandle> portals) {
  IpczDriverHandle transport =
      std::exchange(transport_, IPCZ_INVALID_DRIVER_HANDLE);
  ABSL_ASSERT(transport != IPCZ_INVALID_DRIVER_HANDLE);
  const IpczResult result =
      ipcz().ConnectNode(node(), transport, portals.size(),
                         IPCZ_CONNECT_NODE_TO_BROKER, nullptr, portals.data());
  ASSERT_EQ(IPCZ_RESULT_OK, result);
}

IpczHandle TestNode::ConnectToBroker() {
  IpczHandle portal;
  ConnectToBroker({&portal, 1});
  return portal;
}

std::pair<IpczHandle, IpczHandle> TestNode::OpenPortals() {
  return TestBase::OpenPortals(node_);
}

void TestNode::CloseThisNode() {
  if (node_ != IPCZ_INVALID_HANDLE) {
    IpczHandle node = std::exchange(node_, IPCZ_INVALID_HANDLE);
    ipcz().Close(node, IPCZ_NO_FLAGS, nullptr);
  }
}

Ref<TestNode::TestNodeController> TestNode::SpawnTestNodeImpl(
    IpczHandle from_node,
    const internal::TestNodeDetails& details,
    PortalsOrTransport portals_or_transport) {
  struct Connect {
    explicit Connect(TestNode& test) : test(test) {}

    IpczDriverHandle operator()(absl::Span<IpczHandle> portals) {
      TransportPair transports = test.CreateTransports();
      const IpczResult result =
          test.ipcz().ConnectNode(test.node(), transports.ours, portals.size(),
                                  IPCZ_NO_FLAGS, nullptr, portals.data());
      ABSL_ASSERT(result == IPCZ_RESULT_OK);
      return transports.theirs;
    }

    IpczDriverHandle operator()(IpczDriverHandle transport) {
      return transport;
    }

    TestNode& test;
  };

  Connect connect(*this);
  IpczDriverHandle their_transport = absl::visit(connect, portals_or_transport);

  // TODO: Support a multiprocess mode which launches the new node in a child
  // child process, passing the transport there.
  std::unique_ptr<TestNode> test_node = details.factory();
  test_node->SetTransport(their_transport);
  Ref<TestNodeController> controller =
      MakeRefCounted<InProcessTestNodeController>(driver_mode_,
                                                  std::move(test_node));
  spawned_nodes_.push_back(controller);
  return controller;
}

TestNode::TransportPair TestNode::CreateTransports() {
  TransportPair transports;
  switch (driver_mode_) {
    case DriverMode::kSync: {
      const IpczResult result =
          reference_drivers::kSingleProcessReferenceDriver.CreateTransports(
              IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
              IPCZ_NO_FLAGS, nullptr, &transports.ours, &transports.theirs);
      ABSL_ASSERT(result == IPCZ_RESULT_OK);
      break;
    }

    default:
      LOG(FATAL) << "DriverMode not yet supported.";
      return {};
  }

  return transports;
}

void TestNode::SetTransport(IpczDriverHandle transport) {
  ABSL_ASSERT(transport_ == IPCZ_INVALID_DRIVER_HANDLE);
  transport_ = transport;
}

}  // namespace ipcz::test
