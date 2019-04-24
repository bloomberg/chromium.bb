// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <vector>

#include "ash/public/cpp/manifest.h"
#include "ash/public/cpp/test_manifest.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/window_properties.mojom.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/test/test_service.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/screen_base.h"
#include "ui/wm/core/wm_state.h"

namespace ash {

const char kTestServiceName[] = "ash_unittests";

class WindowTreeClientDelegate : public aura::WindowTreeClientDelegate {
 public:
  WindowTreeClientDelegate() = default;
  ~WindowTreeClientDelegate() override = default;

  void WaitForEmbed() { run_loop_.Run(); }

  void DestroyWindowTreeHost() { window_tree_host_.reset(); }

 private:
  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override {
    window_tree_host_ = std::move(window_tree_host);
    run_loop_.Quit();
  }
  void OnEmbedRootDestroyed(
      aura::WindowTreeHostMus* window_tree_host) override {}
  void OnLostConnection(aura::WindowTreeClient* client) override {}
  aura::PropertyConverter* GetPropertyConverter() override {
    return &property_converter_;
  }

  base::RunLoop run_loop_;
  ::wm::WMState wm_state_;
  aura::PropertyConverter property_converter_;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientDelegate);
};

class AshServiceTest : public testing::Test {
 public:
  AshServiceTest()
      : test_service_manager_(
            {service_manager::Manifest(GetManifest())
                 .Amend(GetManifestOverlayForTesting()),
             service_manager::ManifestBuilder()
                 .WithServiceName(kTestServiceName)
                 .RequireCapability(ws::mojom::kServiceName, "app")
                 .Build()}),
        test_service_(
            test_service_manager_.RegisterTestInstance(kTestServiceName)) {}
  ~AshServiceTest() override = default;

  // service_manager::test::ServiceTest:
  void SetUp() override {
    old_mode_ = aura::test::EnvTestHelper().SetMode(aura::Env::Mode::MUS);
  }

  void TearDown() override {
    // Unset the screen installed by the test.
    display::Screen::SetScreenInstance(nullptr);
    aura::test::EnvTestHelper().SetMode(old_mode_);
  }

 protected:
  service_manager::Connector* connector() { return test_service_.connector(); }

 private:
  aura::Env::Mode old_mode_ = aura::Env::Mode::LOCAL;
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::TestService test_service_;

  DISALLOW_COPY_AND_ASSIGN(AshServiceTest);
};

void OnEmbed(bool success) {
  ASSERT_TRUE(success);
}

TEST_F(AshServiceTest, OpenWindow) {
  // This test launches ash in a separate service. That doesn't make sense with
  // SingleProcessMash.
  if (features::IsSingleProcessMash())
    return;

  display::ScreenBase screen;
  screen.display_list().AddDisplay(
      display::Display(1, gfx::Rect(0, 0, 200, 200)),
      display::DisplayList::Type::PRIMARY);
  display::Screen::SetScreenInstance(&screen);

  WindowTreeClientDelegate window_tree_delegate;

  // Connect to mus and create a new top level window. The request goes to
  // |ash|, but is async.
  std::unique_ptr<aura::WindowTreeClient> client =
      aura::WindowTreeClient::CreateForWindowTreeFactory(
          connector(), &window_tree_delegate, false, nullptr);
  aura::test::EnvWindowTreeClientSetter env_window_tree_client_setter(
      client.get());
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ws::mojom::WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ws::mojom::WindowType::WINDOW));
  aura::WindowTreeHostMus window_tree_host_mus(
      aura::CreateInitParamsForTopLevel(client.get(), std::move(properties)));
  window_tree_host_mus.InitHost();
  aura::Window* child_window = new aura::Window(nullptr);
  child_window->Init(ui::LAYER_NOT_DRAWN);
  window_tree_host_mus.window()->AddChild(child_window);

  // Create another WindowTreeClient by way of embedding in
  // |child_window|. This blocks until it succeeds.
  ws::mojom::WindowTreeClientPtr tree_client;
  auto tree_client_request = MakeRequest(&tree_client);
  aura::WindowPortMus::Get(child_window)
      ->Embed(std::move(tree_client), 0u, base::BindOnce(&OnEmbed));
  std::unique_ptr<aura::WindowTreeClient> child_client =
      aura::WindowTreeClient::CreateForEmbedding(
          connector(), &window_tree_delegate, std::move(tree_client_request),
          false);
  window_tree_delegate.WaitForEmbed();
  ASSERT_TRUE(!child_client->GetRoots().empty());
  window_tree_delegate.DestroyWindowTreeHost();
}

}  // namespace ash
