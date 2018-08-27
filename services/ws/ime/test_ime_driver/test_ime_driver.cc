// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/ime/test_ime_driver/test_ime_driver.h"

#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"

namespace ui {
namespace test {

class TestInputMethod : public ws::mojom::InputMethod {
 public:
  explicit TestInputMethod(ws::mojom::TextInputClientPtr client)
      : client_(std::move(client)) {}
  ~TestInputMethod() override = default;

 private:
  // ws::mojom::InputMethod:
  void OnTextInputTypeChanged(TextInputType text_input_type) override {
    NOTIMPLEMENTED();
  }
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {
    NOTIMPLEMENTED();
  }
  void ProcessKeyEvent(std::unique_ptr<Event> key_event,
                       ProcessKeyEventCallback callback) override {
    DCHECK(key_event->IsKeyEvent());

    std::unique_ptr<Event> cloned_event = ui::Event::Clone(*key_event);

    // Using base::Unretained is safe because |client_| is owned by this class.
    client_->DispatchKeyEventPostIME(
        std::move(key_event),
        base::BindOnce(&TestInputMethod::PostProcssKeyEvent,
                       base::Unretained(this), std::move(cloned_event),
                       std::move(callback)));
  }
  void CancelComposition() override { NOTIMPLEMENTED(); }
  void ShowVirtualKeyboardIfEnabled() override { NOTIMPLEMENTED(); }

  void PostProcssKeyEvent(std::unique_ptr<Event> key_event,
                          ProcessKeyEventCallback callback,
                          bool stopped_propagation) {
    // Ignore any events with modifiers set. This is useful for running things
    // like ash_shell_with_content and having accelerators (such as control-n)
    // work.
    if (!stopped_propagation && key_event->type() == ET_KEY_PRESSED &&
        (key_event->flags() &
         (EF_CONTROL_DOWN | EF_ALT_DOWN | EF_COMMAND_DOWN)) == 0 &&
        (key_event->AsKeyEvent()->is_char() ||
         key_event->AsKeyEvent()->GetDomKey().IsCharacter())) {
      client_->InsertChar(std::move(key_event));
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(false);
    }
  }

  ws::mojom::TextInputClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

TestIMEDriver::TestIMEDriver() {}

TestIMEDriver::~TestIMEDriver() {}

void TestIMEDriver::StartSession(ws::mojom::StartSessionDetailsPtr details) {
  mojo::MakeStrongBinding(
      std::make_unique<TestInputMethod>(
          ws::mojom::TextInputClientPtr(std::move(details->client))),
      std::move(details->input_method_request));
}

}  // namespace test
}  // namespace ui
