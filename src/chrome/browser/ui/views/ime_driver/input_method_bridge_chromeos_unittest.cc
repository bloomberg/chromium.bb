// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/ime_driver/input_method_bridge_chromeos.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

enum class CompositionEventType {
  SET,
  CONFIRM,
  CLEAR,
  INSERT_TEXT,
  INSERT_CHAR
};

struct CompositionEvent {
  CompositionEventType type;
  base::string16 text_data;
  base::char16 char_data;
};

class TestTextInputClient : public ws::mojom::TextInputClient {
 public:
  explicit TestTextInputClient(ws::mojom::TextInputClientRequest request)
      : binding_(this, std::move(request)) {}

  CompositionEvent WaitUntilCompositionEvent() {
    if (!receieved_event_.has_value()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
    CompositionEvent result = receieved_event_.value();
    receieved_event_.reset();
    return result;
  }

  void set_manual_ack_dispatch_key_event_post_ime_callback(bool manual) {
    manual_ack_dispatch_key_event_post_ime_callback_ = manual;
  }

  void AckDispatchKeyEventPostIMECallback() {
    DCHECK(manual_ack_dispatch_key_event_post_ime_callback_);

    if (!pending_dispatch_key_event_post_ime_callback_) {
      pending_dispatch_key_event_post_ime_callback_wait_loop_ =
          std::make_unique<base::RunLoop>();
      pending_dispatch_key_event_post_ime_callback_wait_loop_->Run();
      pending_dispatch_key_event_post_ime_callback_wait_loop_.reset();
    }

    std::move(pending_dispatch_key_event_post_ime_callback_).Run(false, false);
  }

 private:
  void SetCompositionText(const ui::CompositionText& composition) override {
    CompositionEvent ev = {CompositionEventType::SET, composition.text, 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void ConfirmCompositionText() override {
    CompositionEvent ev = {CompositionEventType::CONFIRM, base::string16(), 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void ClearCompositionText() override {
    CompositionEvent ev = {CompositionEventType::CLEAR, base::string16(), 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void InsertText(const base::string16& text) override {
    CompositionEvent ev = {CompositionEventType::INSERT_TEXT, text, 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void InsertChar(std::unique_ptr<ui::Event> event) override {
    ASSERT_TRUE(event->IsKeyEvent());
    CompositionEvent ev = {CompositionEventType::INSERT_CHAR, base::string16(),
                           event->AsKeyEvent()->GetCharacter()};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void DispatchKeyEventPostIME(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventPostIMECallback callback) override {
    if (manual_ack_dispatch_key_event_post_ime_callback_) {
      // Only one pending callback is expected.
      EXPECT_FALSE(pending_dispatch_key_event_post_ime_callback_);
      pending_dispatch_key_event_post_ime_callback_ = std::move(callback);
      if (pending_dispatch_key_event_post_ime_callback_wait_loop_)
        pending_dispatch_key_event_post_ime_callback_wait_loop_->Quit();
      return;
    }

    std::move(callback).Run(false, false);
  }
  void EnsureCaretNotInRect(const gfx::Rect& rect) override {}
  void SetEditableSelectionRange(const gfx::Range& range) override {}
  void DeleteRange(const gfx::Range& range) override {}
  void OnInputMethodChanged() override {}
  void ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override {}
  void ExtendSelectionAndDelete(uint32_t before, uint32_t after) override {}

  mojo::Binding<ws::mojom::TextInputClient> binding_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Optional<CompositionEvent> receieved_event_;

  bool manual_ack_dispatch_key_event_post_ime_callback_ = false;
  DispatchKeyEventPostIMECallback pending_dispatch_key_event_post_ime_callback_;
  std::unique_ptr<base::RunLoop>
      pending_dispatch_key_event_post_ime_callback_wait_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

class InputMethodBridgeChromeOSTest : public testing::Test {
 public:
  InputMethodBridgeChromeOSTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~InputMethodBridgeChromeOSTest() override {}

  void SetUp() override {
    ui::IMEBridge::Initialize();

    ws::mojom::TextInputClientPtr client_ptr;
    client_ = std::make_unique<TestTextInputClient>(MakeRequest(&client_ptr));
    ws::mojom::SessionDetailsPtr details = ws::mojom::SessionDetails::New();
    details->state = ws::mojom::TextInputState::New(
        ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
        base::i18n::LEFT_TO_RIGHT, 0);
    details->data = ws::mojom::TextInputClientData::New();
    input_method_ = std::make_unique<InputMethodBridge>(
        std::make_unique<RemoteTextInputClient>(std::move(client_ptr),
                                                std::move(details)));
  }

  bool ProcessKeyEvent(std::unique_ptr<ui::Event> event) {
    handled_.reset();

    input_method_->ProcessKeyEvent(
        std::move(event),
        base::Bind(&InputMethodBridgeChromeOSTest::ProcessKeyEventCallback,
                   base::Unretained(this)));

    if (!handled_.has_value()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }

    return handled_.value();
  }

  std::unique_ptr<ui::Event> UnicodeKeyPress(ui::KeyboardCode vkey,
                                             ui::DomCode code,
                                             int flags,
                                             base::char16 character) const {
    return std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED, vkey, code, flags,
                                          ui::DomKey::FromCharacter(character),
                                          ui::EventTimeForNow());
  }

 protected:
  void ProcessKeyEventCallback(bool handled) {
    handled_ = handled;
    if (run_loop_)
      run_loop_->Quit();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestTextInputClient> client_;
  std::unique_ptr<InputMethodBridge> input_method_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Optional<bool> handled_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridgeChromeOSTest);
};

// Tests if hexadecimal composition provided by ui::CharacterComposer works
// correctly. ui::CharacterComposer is tried if no input method extensions
// have been registered yet.
TEST_F(InputMethodBridgeChromeOSTest, HexadecimalComposition) {
  struct {
    ui::KeyboardCode vkey;
    ui::DomCode code;
    int flags;
    base::char16 character;
    std::string composition_text;
  } kTestSequence[] = {
      {ui::VKEY_U, ui::DomCode::US_U, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
       'U', "u"},
      {ui::VKEY_3, ui::DomCode::DIGIT3, 0, '3', "u3"},
      {ui::VKEY_0, ui::DomCode::DIGIT0, 0, '0', "u30"},
      {ui::VKEY_4, ui::DomCode::DIGIT4, 0, '4', "u304"},
      {ui::VKEY_2, ui::DomCode::DIGIT2, 0, '2', "u3042"},
  };

  // Send the Ctrl-Shift-U,3,4,0,2 sequence.
  for (size_t i = 0; i < base::size(kTestSequence); i++) {
    EXPECT_TRUE(ProcessKeyEvent(
        UnicodeKeyPress(kTestSequence[i].vkey, kTestSequence[i].code,
                        kTestSequence[i].flags, kTestSequence[i].character)));
    CompositionEvent ev = client_->WaitUntilCompositionEvent();
    EXPECT_EQ(CompositionEventType::SET, ev.type);
    EXPECT_EQ(base::UTF8ToUTF16(kTestSequence[i].composition_text),
              ev.text_data);
  }

  // Press the return key and verify that the composition text was converted
  // to the desired text.
  EXPECT_TRUE(ProcessKeyEvent(
      UnicodeKeyPress(ui::VKEY_RETURN, ui::DomCode::ENTER, 0, '\r')));
  CompositionEvent ev = client_->WaitUntilCompositionEvent();
  EXPECT_EQ(CompositionEventType::INSERT_TEXT, ev.type);
  EXPECT_EQ(base::string16(1, 0x3042), ev.text_data);
}

// Test that Ctrl-C, Ctrl-X, and Ctrl-V are not handled.
TEST_F(InputMethodBridgeChromeOSTest, ClipboardAccelerators) {
  EXPECT_FALSE(ProcessKeyEvent(UnicodeKeyPress(ui::VKEY_C, ui::DomCode::US_C,
                                               ui::EF_CONTROL_DOWN, 'C')));
  EXPECT_FALSE(ProcessKeyEvent(UnicodeKeyPress(ui::VKEY_X, ui::DomCode::US_X,
                                               ui::EF_CONTROL_DOWN, 'X')));
  EXPECT_FALSE(ProcessKeyEvent(UnicodeKeyPress(ui::VKEY_V, ui::DomCode::US_V,
                                               ui::EF_CONTROL_DOWN, 'V')));
}

// Test that multiple DispatchKeyEventPostIME calls are handled serially.
TEST_F(InputMethodBridgeChromeOSTest, SerialDispatchKeyEventPostIME) {
  client_->set_manual_ack_dispatch_key_event_post_ime_callback(true);

  // Send multiple key events.
  input_method_->ProcessKeyEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                     ui::EF_NONE),
      base::DoNothing());
  input_method_->ProcessKeyEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_RELEASED, ui::VKEY_A,
                                     ui::EF_NONE),
      base::DoNothing());

  // TestTextInputClient::DispatchKeyEventPostIME is called via mojo.
  // RemoteTextInputClient should make the calls serially. Spin message loop to
  // see whether |client_| complains about more than one call at a time.
  base::RunLoop().RunUntilIdle();

  // Ack the pending key events.
  client_->AckDispatchKeyEventPostIMECallback();
  client_->AckDispatchKeyEventPostIMECallback();
}
