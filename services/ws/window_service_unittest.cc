// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_service.h"

#include <memory>

#include "base/run_loop.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/test_wm.mojom.h"
#include "services/ws/window_manager_interface.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ws {
namespace {

// Used as callback from ScheduleEmbed().
void ScheduleEmbedCallback(base::UnguessableToken* result_token,
                           const base::UnguessableToken& actual_token) {
  *result_token = actual_token;
}

}  // namespace

TEST(WindowServiceTest, DeleteWithClients) {
  // Use |test_setup| to configure aura and other state.
  WindowServiceTestSetup test_setup;

  // Create another WindowService.
  TestWindowServiceDelegate test_window_service_delegate;
  WindowService window_service(&test_window_service_delegate, nullptr,
                               test_setup.focus_controller());
  service_manager::TestConnectorFactory factory;
  window_service.BindServiceRequest(
      factory.RegisterInstance(mojom::kServiceName));

  // Connect to |window_service| and ask for a new WindowTree.
  mojom::WindowTreeFactoryPtr window_tree_factory;
  factory.GetDefaultConnector()->BindInterface(mojom::kServiceName,
                                               &window_tree_factory);
  mojom::WindowTreePtr window_tree;
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = MakeRequest(&client);
  window_tree_factory->CreateWindowTree(MakeRequest(&window_tree),
                                        std::move(client));

  // Use FlushForTesting() to ensure WindowService processes the request.
  window_tree_factory.FlushForTesting();

  // There should be at least one WindowTree.
  EXPECT_FALSE(window_service.window_trees().empty());

  // Destroying the |window_service| should remove all the WindowTrees and
  // ensure a DCHECK isn't hit in ~WindowTree.
}

// Implementation of mojom::TestWm that sets a boolean when DoIt() is called.
class TestWm : public WindowManagerInterface, public test::mojom::TestWm {
 public:
  TestWm(mojo::ScopedInterfaceEndpointHandle handle, bool* do_it_called)
      : binding_(this,
                 mojo::AssociatedInterfaceRequest<test::mojom::TestWm>(
                     std::move(handle))),
        do_it_called_(do_it_called) {}

  // test::mojom::TestWm:
  void DoIt() override { *do_it_called_ = true; }

 private:
  mojo::AssociatedBinding<test::mojom::TestWm> binding_;
  bool* do_it_called_;

  DISALLOW_COPY_AND_ASSIGN(TestWm);
};

// Subclass os TestWindowServiceDelegate that creates TestWm.
class TestWindowServiceDelegateWithInterface
    : public TestWindowServiceDelegate {
 public:
  TestWindowServiceDelegateWithInterface() = default;
  ~TestWindowServiceDelegateWithInterface() override = default;

  bool do_it_called() const { return do_it_called_; }

  // TestWindowServiceDelegate:
  std::unique_ptr<WindowManagerInterface> CreateWindowManagerInterface(
      WindowTree* window_tree,
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    if (name != test::mojom::TestWm::Name_)
      return nullptr;

    return std::make_unique<TestWm>(std::move(handle), &do_it_called_);
  }

 private:
  bool do_it_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWindowServiceDelegateWithInterface);
};

TEST(WindowServiceTest, GetWindowManagerInterface) {
  // Use |test_setup| to configure aura and other state.
  WindowServiceTestSetup test_setup;

  // Create another WindowService.
  TestWindowServiceDelegateWithInterface test_window_service_delegate;
  WindowService window_service(&test_window_service_delegate, nullptr,
                               test_setup.focus_controller());
  service_manager::TestConnectorFactory factory;
  window_service.BindServiceRequest(
      factory.RegisterInstance(mojom::kServiceName));

  // Connect to |window_service| and ask for a new WindowTree.
  mojom::WindowTreeFactoryPtr window_tree_factory;
  factory.GetDefaultConnector()->BindInterface(mojom::kServiceName,
                                               &window_tree_factory);
  mojom::WindowTreePtr window_tree;
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = MakeRequest(&client);
  window_tree_factory->CreateWindowTree(MakeRequest(&window_tree),
                                        std::move(client));

  // Request the TestWm interface and call a function on it.
  mojom::WindowManagerAssociatedPtr wm;
  window_tree->BindWindowManagerInterface(test::mojom::TestWm::Name_,
                                          MakeRequest(&wm));
  test::mojom::TestWmAssociatedPtr test_wm(
      mojo::AssociatedInterfacePtrInfo<test::mojom::TestWm>(
          wm.PassInterface().PassHandle(), test::mojom::TestWm::Version_));
  test_wm->DoIt();
  test_wm.FlushForTesting();
  EXPECT_TRUE(test_window_service_delegate.do_it_called());
}

// Test client ids assigned to window trees that connect to the window service.
TEST(WindowServiceTest, ClientIds) {
  // Use |test_setup| to configure aura and other state.
  WindowServiceTestSetup test_setup;

  // Create another WindowService.
  TestWindowServiceDelegate test_window_service_delegate;
  WindowService window_service(&test_window_service_delegate, nullptr,
                               test_setup.focus_controller());

  // The first window tree should have the initial client id.
  auto tree = window_service.CreateWindowTree(nullptr);
  EXPECT_EQ(kInitialClientId, WindowTreeTestHelper(tree.get()).client_id());

  // The second window tree should have an incremented client id.
  tree = window_service.CreateWindowTree(nullptr);
  EXPECT_EQ(kInitialClientId + 1, WindowTreeTestHelper(tree.get()).client_id());
}

// Test client ids assigned to window trees in the decrementing mode.
TEST(WindowServiceTest, ClientIdsDecrement) {
  // Use |test_setup| to configure aura and other state.
  WindowServiceTestSetup test_setup;

  // Create another WindowService that decrements window ids.
  const bool decrement = true;
  TestWindowServiceDelegate test_window_service_delegate;
  WindowService window_service(&test_window_service_delegate, nullptr,
                               test_setup.focus_controller(), decrement);

  // The first window tree should have the initial decrementing client id.
  auto tree = window_service.CreateWindowTree(nullptr);
  EXPECT_EQ(kInitialClientIdDecrement,
            WindowTreeTestHelper(tree.get()).client_id());

  // The second window tree should have a decremented client id.
  tree = window_service.CreateWindowTree(nullptr);
  EXPECT_EQ(kInitialClientIdDecrement - 1,
            WindowTreeTestHelper(tree.get()).client_id());
}

TEST(WindowServiceTest, ScheduleEmbedForExistingClientUsingLocalWindow) {
  WindowServiceTestSetup setup;
  // Schedule an embed in the tree created by |setup|.
  base::UnguessableToken token;
  const uint32_t window_id_in_child = 149;
  setup.window_tree_test_helper()
      ->window_tree()
      ->ScheduleEmbedForExistingClient(
          window_id_in_child, base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  // Create a window that will serve as the parent for the remote window and
  // complete the embedding.
  std::unique_ptr<aura::Window> local_window =
      std::make_unique<aura::Window>(nullptr);
  local_window->Init(ui::LAYER_NOT_DRAWN);
  ASSERT_TRUE(setup.service()->CompleteScheduleEmbedForExistingClient(
      local_window.get(), token, /* embed_flags */ 0));
  EXPECT_TRUE(WindowService::HasRemoteClient(local_window.get()));
}

TEST(WindowServiceTest,
     ScheduleEmbedForExistingClientUsingLocalWindowResetOnTreeDeleted) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient window_tree_client2;
  std::unique_ptr<WindowTree> window_tree2 =
      setup.service()->CreateWindowTree(&window_tree_client2);
  window_tree2->InitFromFactory();

  // Schedule an embed in |window_tree2|.
  base::UnguessableToken token;
  const uint32_t window_id_in_child = 212;
  WindowTreeTestHelper(window_tree2.get())
      .window_tree()
      ->ScheduleEmbedForExistingClient(
          window_id_in_child, base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  // Create a window that will serve as the parent for the remote window and
  // complete the embedding.
  std::unique_ptr<aura::Window> local_window =
      std::make_unique<aura::Window>(nullptr);
  local_window->Init(ui::LAYER_NOT_DRAWN);
  ASSERT_TRUE(setup.service()->CompleteScheduleEmbedForExistingClient(
      local_window.get(), token, /* embed_flags */ 0));
  EXPECT_TRUE(WindowService::HasRemoteClient(local_window.get()));

  // Deleting |window_tree2| should remove the remote client.
  window_tree2.reset();
  EXPECT_FALSE(WindowService::HasRemoteClient(local_window.get()));
}

}  // namespace ws
