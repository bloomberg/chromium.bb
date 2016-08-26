// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/cpp/service_test.h"
#include "services/ui/public/interfaces/ime.mojom.h"
#include "ui/events/event.h"

class TestTextInputClient : public ui::mojom::TextInputClient {
 public:
  explicit TestTextInputClient(ui::mojom::TextInputClientRequest request)
      : binding_(this, std::move(request)) {}

  ui::mojom::CompositionEventPtr ReceiveCompositionEvent() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();

    return std::move(receieved_composition_event_);
  }

 private:
  void OnCompositionEvent(ui::mojom::CompositionEventPtr event) override {
    receieved_composition_event_ = std::move(event);
    run_loop_->Quit();
  }

  mojo::Binding<ui::mojom::TextInputClient> binding_;
  std::unique_ptr<base::RunLoop> run_loop_;
  ui::mojom::CompositionEventPtr receieved_composition_event_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

class IMEAppTest : public shell::test::ServiceTest {
 public:
  IMEAppTest() : ServiceTest("exe:mus_ime_unittests") {}
  ~IMEAppTest() override {}

  // shell::test::ServiceTest:
  void SetUp() override {
    ServiceTest::SetUp();
    // test_ime_driver will register itself as the current IMEDriver.
    connector()->Connect("mojo:test_ime_driver");
    connector()->ConnectToInterface("mojo:ui", &ime_server_);
  }

 protected:
  ui::mojom::IMEServerPtr ime_server_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(IMEAppTest);
};

// Tests sending a KeyEvent to the IMEDriver through the Mus IMEServer.
TEST_F(IMEAppTest, ProcessKeyEvent) {
  ui::mojom::TextInputClientPtr client_ptr;
  TestTextInputClient client(GetProxy(&client_ptr));

  ui::mojom::InputMethodPtr input_method;
  ime_server_->StartSession(std::move(client_ptr), GetProxy(&input_method));

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);

  input_method->ProcessKeyEvent(ui::Event::Clone(key_event));

  ui::mojom::CompositionEventPtr composition_event =
      client.ReceiveCompositionEvent();
  ASSERT_EQ(ui::mojom::CompositionEventType::INSERT_CHAR,
            composition_event->type);
  ASSERT_TRUE(composition_event->key_event);
  ASSERT_TRUE(composition_event->key_event.value()->IsKeyEvent());

  ui::KeyEvent* received_key_event =
      composition_event->key_event.value()->AsKeyEvent();
  ASSERT_EQ(ui::ET_KEY_PRESSED, received_key_event->type());
  ASSERT_EQ(key_event.GetCharacter(), received_key_event->GetCharacter());
}
