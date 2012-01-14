// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib-object.h>

#include <cstring>

#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_ibus.h"
#include "ui/base/ime/mock_ibus_client.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/rect.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

class TestableInputMethodIBus : public InputMethodIBus {
 public:
  TestableInputMethodIBus(internal::InputMethodDelegate* delegate,
                          scoped_ptr<internal::IBusClient> new_client)
      : InputMethodIBus(delegate) {
    set_ibus_client(new_client.Pass());
  }

  // Change access rights.
  using InputMethodIBus::ibus_client;
  using InputMethodIBus::GetBus;
};

class InputMethodIBusTest : public internal::InputMethodDelegate,
                            public testing::Test,
                            public TextInputClient {
 public:
  InputMethodIBusTest() {
    ResetFlags();
  }

  virtual ~InputMethodIBusTest() {
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    scoped_ptr<internal::IBusClient> client(new internal::MockIBusClient);
    ime_.reset(new TestableInputMethodIBus(this, client.Pass()));
    ime_->SetFocusedTextInputClient(this);
  }

  virtual void TearDown() OVERRIDE {
    if (ime_.get())
      ime_->SetFocusedTextInputClient(NULL);
    ime_.reset();
  }

  // ui::internal::InputMethodDelegate overrides:
  virtual void DispatchKeyEventPostIME(
      const base::NativeEvent& native_key_event) OVERRIDE {
    dispatched_native_event_ = native_key_event;
  }
  virtual void DispatchFabricatedKeyEventPostIME(ui::EventType type,
                                                 ui::KeyboardCode key_code,
                                                 int flags) OVERRIDE {
    dispatched_fabricated_event_type_ = type;
    dispatched_fabricated_event_key_code_ = key_code;
    dispatched_fabricated_event_flags_ = flags;
  }

  // ui::TextInputClient overrides:
  virtual void SetCompositionText(
      const CompositionText& composition) OVERRIDE {
    composition_text_ = composition;
  }
  virtual void ConfirmCompositionText() OVERRIDE {
    confirmed_text_ = composition_text_;
    composition_text_.Clear();
  }
  virtual void ClearCompositionText() OVERRIDE {
    composition_text_.Clear();
  }
  virtual void InsertText(const string16& text) OVERRIDE {
    inserted_text_ = text;
  }
  virtual void InsertChar(char16 ch, int flags) OVERRIDE {
    inserted_char_ = ch;
    inserted_char_flags_ = flags;
  }
  virtual TextInputType GetTextInputType() const OVERRIDE {
    return input_type_;
  }
  virtual bool CanComposeInline() const OVERRIDE {
    return can_compose_inline_;
  }
  virtual gfx::Rect GetCaretBounds() OVERRIDE {
    return caret_bounds_;
  }
  virtual bool HasCompositionText() OVERRIDE {
    CompositionText empty;
    return composition_text_ != empty;
  }
  virtual bool GetTextRange(Range* range) OVERRIDE { return false; }
  virtual bool GetCompositionTextRange(Range* range) OVERRIDE { return false; }
  virtual bool GetSelectionRange(Range* range) OVERRIDE { return false; }
  virtual bool SetSelectionRange(const Range& range) OVERRIDE { return false; }
  virtual bool DeleteRange(const Range& range) OVERRIDE { return false; }
  virtual bool GetTextFromRange(const Range& range,
                                string16* text) OVERRIDE { return false; }
  virtual void OnInputMethodChanged() OVERRIDE {
    ++on_input_method_changed_call_count_;
  }
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE { return false; }

  IBusBus* GetBus() {
    return ime_->GetBus();
  }

  internal::MockIBusClient* GetClient() {
    return static_cast<internal::MockIBusClient*>(ime_->ibus_client());
  }

  bool HasNativeEvent() const {
    base::NativeEvent empty;
    std::memset(&empty, 0, sizeof(empty));
    return !!std::memcmp(&dispatched_native_event_,
                         &empty,
                         sizeof(dispatched_native_event_));
  }

  void ResetFlags() {
    std::memset(&dispatched_native_event_, 0, sizeof(dispatched_native_event_));
    DCHECK(!HasNativeEvent());
    dispatched_fabricated_event_type_ = ET_UNKNOWN;
    dispatched_fabricated_event_key_code_ = VKEY_UNKNOWN;
    dispatched_fabricated_event_flags_ = 0;

    composition_text_.Clear();
    confirmed_text_.Clear();
    inserted_text_.clear();
    inserted_char_ = 0;
    inserted_char_flags_ = 0;
    on_input_method_changed_call_count_ = 0;

    input_type_ = TEXT_INPUT_TYPE_NONE;
    can_compose_inline_ = true;
    caret_bounds_ = gfx::Rect();
  }

  scoped_ptr<TestableInputMethodIBus> ime_;

  // Variables for remembering the parameters that are passed to
  // ui::internal::InputMethodDelegate functions.
  base::NativeEvent dispatched_native_event_;
  ui::EventType dispatched_fabricated_event_type_;
  ui::KeyboardCode dispatched_fabricated_event_key_code_;
  int dispatched_fabricated_event_flags_;

  // Variables for remembering the parameters that are passed to
  // ui::TextInputClient functions.
  CompositionText composition_text_;
  CompositionText confirmed_text_;
  string16 inserted_text_;
  char16 inserted_char_;
  unsigned int on_input_method_changed_call_count_;
  int inserted_char_flags_;

  // Variables that will be returned from the ui::TextInputClient functions.
  TextInputType input_type_;
  bool can_compose_inline_;
  gfx::Rect caret_bounds_;
};

// Tests public APIs in ui::InputMethod first.

TEST_F(InputMethodIBusTest, GetInputLocale) {
  // ui::InputMethodIBus does not support the API.
  ime_->Init(true);
  EXPECT_EQ("", ime_->GetInputLocale());
}

TEST_F(InputMethodIBusTest, GetInputTextDirection) {
  // ui::InputMethodIBus does not support the API.
  ime_->Init(true);
  EXPECT_EQ(base::i18n::UNKNOWN_DIRECTION, ime_->GetInputTextDirection());
}

TEST_F(InputMethodIBusTest, IsActive) {
  ime_->Init(true);
  // ui::InputMethodIBus always returns true.
  EXPECT_TRUE(ime_->IsActive());
}

TEST_F(InputMethodIBusTest, GetInputTextType) {
  ime_->Init(true);
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_TEXT, ime_->GetTextInputType());
}

TEST_F(InputMethodIBusTest, CanComposeInline) {
  ime_->Init(true);
  EXPECT_EQ(true, ime_->CanComposeInline());
  can_compose_inline_ = false;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(false, ime_->CanComposeInline());
}

TEST_F(InputMethodIBusTest, GetTextInputClient) {
  ime_->Init(true);
  EXPECT_EQ(this, ime_->GetTextInputClient());
  ime_->SetFocusedTextInputClient(NULL);
  EXPECT_EQ(NULL, ime_->GetTextInputClient());
}

TEST_F(InputMethodIBusTest, GetInputTextType_WithoutFocusedClient) {
  ime_->Init(true);
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  ime_->SetFocusedTextInputClient(NULL);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  // The OnTextInputTypeChanged() call above should be ignored since |this| is
  // not the current focused client.
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());

  ime_->SetFocusedTextInputClient(this);
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
}

TEST_F(InputMethodIBusTest, GetInputTextType_WithoutFocusedWindow) {
  ime_->Init(true);
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  ime_->OnBlur();
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  // The OnTextInputTypeChanged() call above should be ignored since the top-
  // level window which the ime_ is attached to is not currently focused.
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());

  ime_->OnFocus();
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
}

TEST_F(InputMethodIBusTest, GetInputTextType_WithoutFocusedWindow2) {
  ime_->Init(false);  // the top-level is initially unfocused.
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());

  ime_->OnFocus();
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
}

// Then, tests internal behavior of ui::InputMethodIBus, especially if the input
// method implementation calls ui::internal::IBusClient APIs as expected.

// Start ibus-daemon first, then create ui::InputMethodIBus. Check if a new
// input context is created.
TEST_F(InputMethodIBusTest, InitiallyConnected) {
  GetClient()->is_connected_ = true;
  ime_->Init(true);
  // An input context should be created immediately since is_connected_ is true.
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(1U, GetClient()->set_capabilities_call_count_);
  // However, since the current text input type is 'NONE' (the default), FocusIn
  // shouldn't be called.
  EXPECT_EQ(0U, GetClient()->focus_in_call_count_);
}

// Create ui::InputMethodIBus, then start ibus-daemon.
TEST_F(InputMethodIBusTest, InitiallyDisconnected) {
  ime_->Init(true);
  // A context shouldn't be created since the daemon is not running.
  EXPECT_EQ(0U, GetClient()->create_context_call_count_);
  // Start the daemon.
  GetClient()->is_connected_ = true;
  g_signal_emit_by_name(GetBus(), "connected");
  // A context should be created upon the signal delivery.
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(1U, GetClient()->set_capabilities_call_count_);
  EXPECT_EQ(0U, GetClient()->focus_in_call_count_);
}

// Confirm that ui::InputMethodIBus does not crash on "disconnected" signal
// delivery.
TEST_F(InputMethodIBusTest, Disconnect) {
  GetClient()->is_connected_ = true;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  GetClient()->is_connected_ = false;
  g_signal_emit_by_name(GetBus(), "disconnected");
}

// Confirm that ui::InputMethodIBus re-creates an input context when ibus-daemon
// restarts.
TEST_F(InputMethodIBusTest, DisconnectThenReconnect) {
  GetClient()->is_connected_ = true;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(1U, GetClient()->set_capabilities_call_count_);
  EXPECT_EQ(0U, GetClient()->destroy_proxy_call_count_);
  GetClient()->is_connected_ = false;
  g_signal_emit_by_name(GetBus(), "disconnected");
  GetClient()->is_connected_ = true;
  g_signal_emit_by_name(GetBus(), "connected");
  // Check if the old context is deleted.
  EXPECT_EQ(1U, GetClient()->destroy_proxy_call_count_);
  // Check if a new context is created.
  EXPECT_EQ(2U, GetClient()->create_context_call_count_);
  EXPECT_EQ(2U, GetClient()->set_capabilities_call_count_);
}

// Confirm that ui::InputMethodIBus does not crash even if NULL context is
// passed.
// TODO(yusukes): Currently, ui::InputMethodIBus does not try to create ic once
// it fails (unless ibus sends the "connected" signal to Chrome again). It might
// be better to add some retry logic. Will revisit later.
TEST_F(InputMethodIBusTest, CreateContextFail) {
  GetClient()->is_connected_ = true;
  GetClient()->create_context_result_ =
      internal::MockIBusClient::kCreateContextFail;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  // |set_capabilities_call_count_| should be zero since a context is not
  // created yet.
  EXPECT_EQ(0U, GetClient()->set_capabilities_call_count_);
}

// Confirm that ui::InputMethodIBus does not crash even if ibus-daemon does not
// respond.
TEST_F(InputMethodIBusTest, CreateContextNoResp) {
  GetClient()->is_connected_ = true;
  GetClient()->create_context_result_ =
      internal::MockIBusClient::kCreateContextNoResponse;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(0U, GetClient()->set_capabilities_call_count_);
}

// Confirm that ui::InputMethodIBus does not crash even if ibus-daemon responds
// after ui::InputMethodIBus is deleted. See comments in ~MockIBusClient() as
// well.
TEST_F(InputMethodIBusTest, CreateContextDelayed) {
  GetClient()->is_connected_ = true;
  GetClient()->create_context_result_ =
      internal::MockIBusClient::kCreateContextDelayed;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(0U, GetClient()->set_capabilities_call_count_);
  // After this line, the destructor for |ime_| will run first. Then, the
  // destructor for the client will run. In the latter function, a new input
  // context will be created and passed to StoreOrAbandonInputContext().
}

// Confirm that IBusClient::FocusIn is called on "connected" if input_type_ is
// TEXT.
TEST_F(InputMethodIBusTest, FocusIn_Text) {
  ime_->Init(true);
  // A context shouldn't be created since the daemon is not running.
  EXPECT_EQ(0U, GetClient()->create_context_call_count_);
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  // Start the daemon.
  GetClient()->is_connected_ = true;
  g_signal_emit_by_name(GetBus(), "connected");
  // A context should be created upon the signal delivery.
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  // Since a form has focus, IBusClient::FocusIn() should be called.
  EXPECT_EQ(1U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(1U, GetClient()->set_cursor_location_call_count_);
  // ui::TextInputClient::OnInputMethodChanged() should be called when
  // ui::InputMethodIBus connects/disconnects to/from ibus-daemon and the
  // current text input type is not NONE.
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that IBusClient::FocusIn is NOT called on "connected" if input_type_
// is PASSWORD.
TEST_F(InputMethodIBusTest, FocusIn_Password) {
  ime_->Init(true);
  EXPECT_EQ(0U, GetClient()->create_context_call_count_);
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  GetClient()->is_connected_ = true;
  g_signal_emit_by_name(GetBus(), "connected");
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  // Since a form has focus, IBusClient::FocusIn() should NOT be called.
  EXPECT_EQ(0U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodIBusTest, FocusOut_None) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  GetClient()->is_connected_ = true;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(1U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(0U, GetClient()->focus_out_call_count_);
  input_type_ = TEXT_INPUT_TYPE_NONE;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(1U, GetClient()->focus_out_call_count_);
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodIBusTest, FocusOut_Password) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  GetClient()->is_connected_ = true;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(1U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(0U, GetClient()->focus_out_call_count_);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(1U, GetClient()->focus_out_call_count_);
}

// Confirm that IBusClient::FocusOut is NOT called.
TEST_F(InputMethodIBusTest, FocusOut_Url) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  GetClient()->is_connected_ = true;
  ime_->Init(true);
  EXPECT_EQ(1U, GetClient()->create_context_call_count_);
  EXPECT_EQ(1U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(0U, GetClient()->focus_out_call_count_);
  input_type_ = TEXT_INPUT_TYPE_URL;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1U, GetClient()->focus_in_call_count_);
  EXPECT_EQ(0U, GetClient()->focus_out_call_count_);
}

// Test if the new |caret_bounds_| is correctly sent to ibus-daemon.
TEST_F(InputMethodIBusTest, OnCaretBoundsChanged) {
  GetClient()->is_connected_ = true;
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->Init(true);
  EXPECT_EQ(0U, GetClient()->set_cursor_location_call_count_);
  caret_bounds_ = gfx::Rect(1, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(1U, GetClient()->set_cursor_location_call_count_);
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(2U, GetClient()->set_cursor_location_call_count_);
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);  // unchanged
  ime_->OnCaretBoundsChanged(this);
  // Current InputMethodIBus implementation performs the IPC regardless of the
  // bounds are changed or not.
  EXPECT_EQ(3U, GetClient()->set_cursor_location_call_count_);
}

// TODO(yusukes): Write more tests, especially for key event functions.

}  // namespace ui
