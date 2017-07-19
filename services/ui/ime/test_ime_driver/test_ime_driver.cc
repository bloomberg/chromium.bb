// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ime/test_ime_driver/test_ime_driver.h"

#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/public/interfaces/ime/ime.mojom.h"

namespace ui {
namespace test {

class TestInputMethod : public mojom::InputMethod {
 public:
  explicit TestInputMethod(mojom::TextInputClientPtr client)
      : client_(std::move(client)) {}
  ~TestInputMethod() override {}

 private:
  // mojom::InputMethod:
  void OnTextInputTypeChanged(TextInputType text_input_type) override {}
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {}
  void ProcessKeyEvent(std::unique_ptr<Event> key_event,
                       ProcessKeyEventCallback callback) override {
    DCHECK(key_event->IsKeyEvent());

    if (key_event->AsKeyEvent()->is_char()) {
      client_->InsertChar(std::move(key_event));
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(false);
    }
  }
  void CancelComposition() override {}

  mojom::TextInputClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

TestIMEDriver::TestIMEDriver() {}

TestIMEDriver::~TestIMEDriver() {}

void TestIMEDriver::StartSession(mojom::StartSessionDetailsPtr details) {
  mojo::MakeStrongBinding(
      base::MakeUnique<TestInputMethod>(std::move(details->client)),
      std::move(details->input_method_request));
}

}  // namespace test
}  // namespace ui
