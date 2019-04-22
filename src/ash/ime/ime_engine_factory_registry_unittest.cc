// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_engine_factory_registry.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
namespace {

class TestImeEngineClient : public ime::mojom::ImeEngineClient {
 public:
  TestImeEngineClient() : binding_(this) {}
  ~TestImeEngineClient() override = default;

  ime::mojom::ImeEngineClientPtr BindInterface() {
    ime::mojom::ImeEngineClientPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

 private:
  // ime::mojom::ImeEngineClient:
  void CommitText(const std::string& text) override {}
  void UpdateCompositionText(const ui::CompositionText& composition,
                             uint32_t cursor_pos,
                             bool visible) override {}
  void DeleteSurroundingText(int32_t offset, uint32_t length) override {}
  void SendKeyEvent(std::unique_ptr<ui::Event> key_event) override {}
  void Reconnect() override {}

  mojo::Binding<ime::mojom::ImeEngineClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestImeEngineClient);
};

class TestImeEngineFactory : public ime::mojom::ImeEngineFactory {
 public:
  TestImeEngineFactory() : binding_(this) {
    run_loop_ = std::make_unique<base::RunLoop>();
  }
  ~TestImeEngineFactory() override = default;

  ime::mojom::ImeEngineFactoryPtr BindInterface() {
    ime::mojom::ImeEngineFactoryPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  void WaitForEngineCreated() { run_loop_->Run(); }
  bool engine_created() const { return engine_created_; }

 private:
  // ime::mojom::ImeEngineFactory:
  void CreateEngine(ime::mojom::ImeEngineRequest engine_request,
                    ime::mojom::ImeEngineClientPtr client) override {
    engine_created_ = true;
    run_loop_->Quit();
  }

  mojo::Binding<ime::mojom::ImeEngineFactory> binding_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool engine_created_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestImeEngineFactory);
};

class ImeEngineFactoryRegistryTest : public AshTestBase {
 public:
  ImeEngineFactoryRegistryTest() {}
  ~ImeEngineFactoryRegistryTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    engine_factory_ = std::make_unique<TestImeEngineFactory>();
    Shell::Get()->ime_engine_factory_registry()->BindRequest(
        mojo::MakeRequest(&registry_ptr_));
  }

  void TearDown() override {
    registry_ptr_.reset();
    engine_factory_.reset();

    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestImeEngineFactory> engine_factory_;
  TestImeEngineClient engine_client_;
  ime::mojom::ImeEngineFactoryRegistryPtr registry_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ImeEngineFactoryRegistryTest);
};

TEST_F(ImeEngineFactoryRegistryTest, ConnectBeforeActivateFactory) {
  ime::mojom::ImeEnginePtr engine_ptr;
  Shell::Get()->ime_engine_factory_registry()->ConnectToImeEngine(
      mojo::MakeRequest(&engine_ptr), engine_client_.BindInterface());
  registry_ptr_->ActivateFactory(engine_factory_->BindInterface());
  engine_factory_->WaitForEngineCreated();
  ASSERT_TRUE(engine_factory_->engine_created());
}

TEST_F(ImeEngineFactoryRegistryTest, ConnectAfterActivateFactory) {
  registry_ptr_->ActivateFactory(engine_factory_->BindInterface());
  ime::mojom::ImeEnginePtr engine_ptr;
  Shell::Get()->ime_engine_factory_registry()->ConnectToImeEngine(
      mojo::MakeRequest(&engine_ptr), engine_client_.BindInterface());
  engine_factory_->WaitForEngineCreated();
  ASSERT_TRUE(engine_factory_->engine_created());
}

}  // namespace
}  // namespace ash
