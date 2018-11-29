// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/ws/ime/test_ime_driver/public/mojom/constants.mojom.h"
#include "services/ws/ime/tests_catalog_source.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

class TestTextInputClient : public ws::mojom::TextInputClient {
 public:
  explicit TestTextInputClient(ws::mojom::TextInputClientRequest request)
      : binding_(this, std::move(request)) {}

  std::unique_ptr<ui::Event> WaitUntilInsertChar() {
    if (!receieved_event_) {
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
      run_loop_.reset();
    }

    return std::move(receieved_event_);
  }

 private:
  void SetCompositionText(const ui::CompositionText& composition) override {}
  void ConfirmCompositionText() override {}
  void ClearCompositionText() override {}
  void InsertText(const base::string16& text) override {}
  void InsertChar(std::unique_ptr<ui::Event> event) override {
    receieved_event_ = std::move(event);
    if (run_loop_)
      run_loop_->Quit();
  }
  void DispatchKeyEventPostIME(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventPostIMECallback callback) override {
    std::move(callback).Run(false);
  }

  mojo::Binding<ws::mojom::TextInputClient> binding_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ui::Event> receieved_event_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

class IMEAppTest : public testing::Test {
 public:
  IMEAppTest()
      : test_service_manager_(ws::test::CreateImeTestCatalog()),
        test_service_binding_(
            &test_service_,
            test_service_manager_.RegisterTestInstance("ime_unittests")) {
    // test_ime_driver will register itself as the current IMEDriver.
    // TODO(https://crbug.com/904148): This should not use |WarmService()|.
    connector()->WarmService(service_manager::ServiceFilter::ByName(
        test_ime_driver::mojom::kServiceName));
    connector()->BindInterface(ws::mojom::kServiceName, &ime_driver_);
  }

  ~IMEAppTest() override {}

  service_manager::Connector* connector() {
    return test_service_binding_.GetConnector();
  }

  bool ProcessKeyEvent(ws::mojom::InputMethodPtr* input_method,
                       std::unique_ptr<ui::Event> event) {
    (*input_method)
        ->ProcessKeyEvent(std::move(event),
                          base::Bind(&IMEAppTest::ProcessKeyEventCallback,
                                     base::Unretained(this)));

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();

    return handled_;
  }

 protected:
  void ProcessKeyEventCallback(bool handled) {
    handled_ = handled;
    run_loop_->Quit();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::Service test_service_;
  service_manager::ServiceBinding test_service_binding_;

  ws::mojom::IMEDriverPtr ime_driver_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool handled_;

  DISALLOW_COPY_AND_ASSIGN(IMEAppTest);
};

// Tests sending a KeyEvent to the IMEDriver through the Mus IMEDriver.
TEST_F(IMEAppTest, ProcessKeyEvent) {
  ws::mojom::InputMethodPtr input_method;
  ws::mojom::StartSessionDetailsPtr details =
      ws::mojom::StartSessionDetails::New();
  TestTextInputClient client(MakeRequest(&details->client));
  details->input_method_request = MakeRequest(&input_method);
  ime_driver_->StartSession(std::move(details));

  // Send character key event.
  ui::KeyEvent char_event('A', ui::VKEY_A, ui::DomCode::NONE, 0);
  EXPECT_TRUE(ProcessKeyEvent(&input_method, ui::Event::Clone(char_event)));

  std::unique_ptr<ui::Event> received_event = client.WaitUntilInsertChar();
  ASSERT_TRUE(received_event && received_event->IsKeyEvent());

  ui::KeyEvent* received_key_event = received_event->AsKeyEvent();
  EXPECT_EQ(ui::ET_KEY_PRESSED, received_key_event->type());
  EXPECT_TRUE(received_key_event->is_char());
  EXPECT_EQ(char_event.GetCharacter(), received_key_event->GetCharacter());

  // Send non-character key event.
  ui::KeyEvent nonchar_event(ui::ET_KEY_PRESSED, ui::VKEY_LEFT, 0);
  EXPECT_FALSE(ProcessKeyEvent(&input_method, ui::Event::Clone(nonchar_event)));
}
