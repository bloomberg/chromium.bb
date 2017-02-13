// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/input_method_mus.h"

#include "services/ui/public/interfaces/ime/ime.mojom.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method_delegate.h"

namespace aura {
namespace {

// Empty implementation of InputMethodDelegate.
class TestInputMethodDelegate : public ui::internal::InputMethodDelegate {
 public:
  TestInputMethodDelegate() {}
  ~TestInputMethodDelegate() override {}

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(ui::KeyEvent* key) override {
    return ui::EventDispatchDetails();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethodDelegate);
};

using ProcessKeyEventCallback = base::Callback<void(bool)>;
using ProcessKeyEventCallbacks = std::vector<ProcessKeyEventCallback>;
using EventResultCallback = base::Callback<void(ui::mojom::EventResult)>;

// InputMethod implementation that queues up the callbacks supplied to
// ProcessKeyEvent().
class TestInputMethod : public ui::mojom::InputMethod {
 public:
  TestInputMethod() {}
  ~TestInputMethod() override {}

  ProcessKeyEventCallbacks* process_key_event_callbacks() {
    return &process_key_event_callbacks_;
  }

  // ui::ime::InputMethod:
  void OnTextInputTypeChanged(ui::TextInputType text_input_type) override {}
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {}
  void ProcessKeyEvent(std::unique_ptr<ui::Event> key_event,
                       const ProcessKeyEventCallback& callback) override {
    process_key_event_callbacks_.push_back(callback);
  }
  void CancelComposition() override {}

 private:
  ProcessKeyEventCallbacks process_key_event_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

}  // namespace

using InputMethodMusTest = test::AuraTestBaseMus;

class InputMethodMusTestApi {
 public:
  static void SetInputMethod(InputMethodMus* input_method_mus,
                             ui::mojom::InputMethod* input_method) {
    input_method_mus->input_method_ = input_method;
  }
  static void CallSendKeyEventToInputMethod(
      InputMethodMus* input_method_mus,
      const ui::KeyEvent& event,
      std::unique_ptr<EventResultCallback> ack_callback) {
    input_method_mus->SendKeyEventToInputMethod(event, std::move(ack_callback));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InputMethodMusTestApi);
};

namespace {

// Used in closure supplied to processing the event.
void RunFunctionWithEventResult(bool* was_run, ui::mojom::EventResult result) {
  *was_run = true;
}

}  // namespace

TEST_F(InputMethodMusTest, PendingCallbackRunFromDestruction) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  bool was_event_result_callback_run = false;
  // Create an InputMethodMus and foward an event to it.
  {
    TestInputMethodDelegate input_method_delegate;
    InputMethodMus input_method_mus(&input_method_delegate, &window);
    TestInputMethod test_input_method;
    InputMethodMusTestApi::SetInputMethod(&input_method_mus,
                                          &test_input_method);
    std::unique_ptr<EventResultCallback> callback =
        base::MakeUnique<EventResultCallback>(base::Bind(
            &RunFunctionWithEventResult, &was_event_result_callback_run));
    InputMethodMusTestApi::CallSendKeyEventToInputMethod(
        &input_method_mus, ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0),
        std::move(callback));
    // Add a null callback as well, to make sure null is deal with.
    InputMethodMusTestApi::CallSendKeyEventToInputMethod(
        &input_method_mus, ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0),
        nullptr);
    // The event should have been queued.
    EXPECT_EQ(2u, test_input_method.process_key_event_callbacks()->size());
    // Callback should not have been run yet.
    EXPECT_FALSE(was_event_result_callback_run);
  }

  // When the destructor is run the callback should be run.
  EXPECT_TRUE(was_event_result_callback_run);
}

}  // namespace aura
