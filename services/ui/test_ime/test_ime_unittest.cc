// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/common/common_type_converters.h"
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
  IMEAppTest() : ServiceTest("exe:test_ime_unittests") {}
  ~IMEAppTest() override {}

  // shell::test::ServiceTest:
  void SetUp() override {
    ServiceTest::SetUp();
    connector()->ConnectToInterface("mojo:test_ime", &ime_driver_);
    ASSERT_TRUE(ime_driver_);
  }

 protected:
  ui::mojom::IMEDriverPtr ime_driver_;

  DISALLOW_COPY_AND_ASSIGN(IMEAppTest);
};

// Test that calling InputMethod::ProcessKeyEvent() after cancelling the session
// encounters an error.
TEST_F(IMEAppTest, TestCancelSession) {
  ui::mojom::TextInputClientPtr client_ptr;
  TestTextInputClient client(GetProxy(&client_ptr));

  ui::mojom::InputMethodPtr input_method;
  ime_driver_->StartSession(1, std::move(client_ptr), GetProxy(&input_method));
  ASSERT_TRUE(input_method.is_bound());

  ime_driver_->CancelSession(1);

  base::RunLoop run_loop;
  input_method.set_connection_error_handler(run_loop.QuitClosure());
  input_method->ProcessKeyEvent(std::unique_ptr<ui::KeyEvent>(
      new ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0)));
  run_loop.Run();

  ASSERT_TRUE(input_method.encountered_error());
}

TEST_F(IMEAppTest, TestProcessKeyEvent) {
  ui::mojom::TextInputClientPtr client_ptr;
  TestTextInputClient client(GetProxy(&client_ptr));

  ui::mojom::InputMethodPtr input_method;
  ime_driver_->StartSession(1, std::move(client_ptr), GetProxy(&input_method));
  ASSERT_TRUE(input_method.is_bound());

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
