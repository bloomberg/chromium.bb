// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller_mojo_impl.h"

#include <memory>
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/public/keyboard_controller.mojom.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace keyboard {

namespace {

class TestObserver : public mojom::KeyboardControllerObserver {
 public:
  explicit TestObserver(mojom::KeyboardController* controller) {
    keyboard::mojom::KeyboardControllerObserverAssociatedPtrInfo ptr_info;
    keyboard_controller_observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
    controller->AddObserver(std::move(ptr_info));
  }
  ~TestObserver() override = default;

  // mojom::KeyboardControllerObserver:
  void OnKeyboardWindowDestroyed() override {}
  void OnKeyboardVisibilityChanged(bool visible) override {}
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override {}
  void OnKeyboardConfigChanged(mojom::KeyboardConfigPtr config) override {
    config_ = *config;
  }

  const mojom::KeyboardConfig& config() const { return config_; }
  void set_config(const mojom::KeyboardConfig& config) { config_ = config; }

 private:
  mojo::AssociatedBinding<keyboard::mojom::KeyboardControllerObserver>
      keyboard_controller_observer_binding_{this};

  mojom::KeyboardConfig config_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestClient {
 public:
  explicit TestClient(service_manager::Connector* connector) {
    connector->BindInterface("test", &keyboard_controller_);

    test_observer_ = std::make_unique<TestObserver>(keyboard_controller_.get());
  }

  void GetKeyboardConfig() {
    keyboard_controller_->GetKeyboardConfig(base::BindOnce(
        &TestClient::OnGetKeyboardConfig, base::Unretained(this)));
    keyboard_controller_.FlushForTesting();
  }

  void SetKeyboardConfig(mojom::KeyboardConfigPtr config) {
    keyboard_controller_->SetKeyboardConfig(std::move(config));
    keyboard_controller_.FlushForTesting();
  }

  int got_keyboard_config_count() const { return got_keyboard_config_count_; }
  const mojom::KeyboardConfig& keyboard_config() const {
    return keyboard_config_;
  }
  TestObserver* test_observer() const { return test_observer_.get(); }

 private:
  void OnGetKeyboardConfig(mojom::KeyboardConfigPtr config) {
    ++got_keyboard_config_count_;
    keyboard_config_ = *config;
  }

  mojom::KeyboardControllerPtr keyboard_controller_;
  std::unique_ptr<TestObserver> test_observer_;

  int got_keyboard_config_count_ = 0;
  mojom::KeyboardConfig keyboard_config_;
};

class KeyboardControllerMojoImplTest : public testing::Test {
 public:
  KeyboardControllerMojoImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {}
  ~KeyboardControllerMojoImplTest() override = default;

  void SetUp() override {
    keyboard_controller_ = std::make_unique<::keyboard::KeyboardController>();
    // Call EnableKeyboard() so that observer methods get called.
    auto test_ui = std::make_unique<TestKeyboardUI>(nullptr /* input_method */);
    keyboard_controller_->EnableKeyboard(std::move(test_ui),
                                         nullptr /* delegate */);
    mojo_impl_ = std::make_unique<KeyboardControllerMojoImpl>(
        keyboard_controller_.get());

    // Create a local service manager connector to handle requests to
    // mojom::KeyboardController.
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);

    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity("test"), mojom::KeyboardController::Name_,
        base::BindRepeating(
            &KeyboardControllerMojoImplTest::AddKeyboardControllerBinding,
            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    test_client_ = std::make_unique<TestClient>(connector_.get());
  }

  void TearDown() override {
    test_client_.reset();
    mojo_impl_.reset();
    keyboard_controller_->DisableKeyboard();
    keyboard_controller_.reset();
  }

  void AddKeyboardControllerBinding(mojo::ScopedMessagePipeHandle handle) {
    mojo_impl_->BindRequest(
        mojom::KeyboardControllerRequest(std::move(handle)));
  }

  ::keyboard::KeyboardController* keyboard_controller() {
    return keyboard_controller_.get();
  }
  KeyboardControllerMojoImpl* mojo_impl() { return mojo_impl_.get(); }
  TestClient* test_client() { return test_client_.get(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<service_manager::Connector> connector_;

  std::unique_ptr<::keyboard::KeyboardController> keyboard_controller_;
  std::unique_ptr<KeyboardControllerMojoImpl> mojo_impl_;
  std::unique_ptr<TestClient> test_client_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerMojoImplTest);
};

}  // namespace

TEST_F(KeyboardControllerMojoImplTest, GetKeyboardConfig) {
  test_client()->GetKeyboardConfig();
  EXPECT_EQ(1, test_client()->got_keyboard_config_count());
}

TEST_F(KeyboardControllerMojoImplTest, SetKeyboardConfig) {
  test_client()->GetKeyboardConfig();
  EXPECT_EQ(1, test_client()->got_keyboard_config_count());
  mojom::KeyboardConfigPtr config =
      mojom::KeyboardConfig::New(test_client()->keyboard_config());
  // Set the observer config to the client (default) config.
  test_client()->test_observer()->set_config(*config);

  // Test that the config changes.
  bool old_auto_complete = config->auto_complete;
  config->auto_complete = !config->auto_complete;
  test_client()->SetKeyboardConfig(std::move(config));
  test_client()->GetKeyboardConfig();
  EXPECT_NE(old_auto_complete, test_client()->keyboard_config().auto_complete);

  // Test that the test observer received the change.
  EXPECT_NE(old_auto_complete,
            test_client()->test_observer()->config().auto_complete);
}

}  // namespace keyboard
