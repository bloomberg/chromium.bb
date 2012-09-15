// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/i18n/char_iterator.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_ibus.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/rect.h"

namespace ui {
namespace {
const int kCreateInputContextMaxTrialCount = 10;

uint32 GetOffsetInUTF16(const std::string& utf8_string, uint32 utf8_offset) {
  string16 utf16_string = UTF8ToUTF16(utf8_string);
  DCHECK_LT(utf8_offset, utf16_string.size());
  base::i18n::UTF16CharIterator char_iterator(&utf16_string);
  for (size_t i = 0; i < utf8_offset; ++i)
    char_iterator.Advance();
  return char_iterator.array_pos();
}

}  // namespace


class TestableInputMethodIBus : public InputMethodIBus {
 public:
  explicit TestableInputMethodIBus(internal::InputMethodDelegate* delegate)
      : InputMethodIBus(delegate) {
  }

  // Change access rights.
  using InputMethodIBus::ExtractCompositionText;
};

class CreateInputContextSuccessHandler {
 public:
  explicit CreateInputContextSuccessHandler(const dbus::ObjectPath& object_path)
      : object_path_(object_path) {
  }

  void Run(const std::string& client_name,
           const chromeos::IBusClient::CreateInputContextCallback& callback,
           const chromeos::IBusClient::ErrorCallback& error_callback) {
    EXPECT_EQ("chrome", client_name);
    callback.Run(object_path_);
  }

 private:
  dbus::ObjectPath object_path_;

  DISALLOW_COPY_AND_ASSIGN(CreateInputContextSuccessHandler);
};

class CreateInputContextFailHandler {
 public:
  CreateInputContextFailHandler() {}
  void Run(const std::string& client_name,
           const chromeos::IBusClient::CreateInputContextCallback& callback,
           const chromeos::IBusClient::ErrorCallback& error_callback) {
    error_callback.Run();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateInputContextFailHandler);
};

class CreateInputContextNoResponseHandler {
 public:
  CreateInputContextNoResponseHandler() {}
  void Run(const std::string& client_name,
           const chromeos::IBusClient::CreateInputContextCallback& callback,
           const chromeos::IBusClient::ErrorCallback& error_callback) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateInputContextNoResponseHandler);
};

class CreateInputContextDelayHandler {
 public:
  explicit CreateInputContextDelayHandler(const dbus::ObjectPath& object_path)
    : object_path_(object_path) {
  }

  void Run(const std::string& client_name,
           const chromeos::IBusClient::CreateInputContextCallback& callback,
           const chromeos::IBusClient::ErrorCallback& error_callback) {
    callback_ = callback;
    error_callback_ = error_callback;
  }

  void RunCallback(bool success) {
    if (success)
      callback_.Run(object_path_);
    else
      error_callback_.Run();
  }

 private:
  dbus::ObjectPath object_path_;
  chromeos::IBusClient::CreateInputContextCallback callback_;
  chromeos::IBusClient::ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(CreateInputContextDelayHandler);
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
    // |thread_manager_| will be released by DBusThreadManager::Shutdown
    // function in TearDown function.
    // Current MockIBusInputContext is strongly depend on gmock, but gmock is
    // banned in ui/*. So just use stab implementation for testing.
    mock_dbus_thread_manager_ =
        new chromeos::MockDBusThreadManagerWithoutGMock();
    chromeos::DBusThreadManager::InitializeForTesting(
        mock_dbus_thread_manager_);

    mock_ibus_client_ = mock_dbus_thread_manager_->mock_ibus_client();
    mock_ibus_input_context_client_ =
        mock_dbus_thread_manager_->mock_ibus_input_context_client();

    ime_.reset(new TestableInputMethodIBus(this));
    ime_->SetFocusedTextInputClient(this);
  }

  virtual void TearDown() OVERRIDE {
    if (ime_.get())
      ime_->SetFocusedTextInputClient(NULL);
    ime_.reset();
    chromeos::DBusThreadManager::Shutdown();
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
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) OVERRIDE {
    return false;
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
  virtual void ExtendSelectionAndDelete(size_t before,
                                        size_t after) OVERRIDE { }

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

  void SetCreateContextSuccessHandler() {
    CreateInputContextSuccessHandler create_input_context_handler(
        dbus::ObjectPath("InputContext_1"));
    mock_ibus_client_->set_create_input_context_handler(base::Bind(
        &CreateInputContextSuccessHandler::Run,
        base::Unretained(&create_input_context_handler)));
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

  // Variables for mock dbus connections.
  chromeos::MockDBusThreadManagerWithoutGMock* mock_dbus_thread_manager_;
  chromeos::MockIBusClient* mock_ibus_client_;
  chromeos::MockIBusInputContextClient* mock_ibus_input_context_client_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodIBusTest);
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
  EXPECT_FALSE(ime_->CanComposeInline());
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
  SetCreateContextSuccessHandler();
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  // An input context should be created immediately since is_connected_ is true.
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->set_capabilities_call_count());
  // However, since the current text input type is 'NONE' (the default), FocusIn
  // shouldn't be called.
  EXPECT_EQ(0, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_TRUE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Create ui::InputMethodIBus, then start ibus-daemon.
TEST_F(InputMethodIBusTest, InitiallyDisconnected) {
  SetCreateContextSuccessHandler();
  ime_->Init(true);
  // A context shouldn't be created since the daemon is not running.
  EXPECT_EQ(0, mock_ibus_client_->create_input_context_call_count());
  // Start the daemon.
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->OnConnected();
  // A context should be created upon the signal delivery.
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_TRUE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus does not crash on "disconnected" signal
// delivery.
TEST_F(InputMethodIBusTest, Disconnect) {
  SetCreateContextSuccessHandler();
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  // Currently we can't shutdown IBusBus connection except in
  // DBusThreadManager's shutting down. So set ibus_bus_ as NULL to emulate
  // dynamical shutting down.
  mock_dbus_thread_manager_->set_ibus_bus(NULL);
  ime_->OnDisconnected();
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus re-creates an input context when ibus-daemon
// restarts.
TEST_F(InputMethodIBusTest, DisconnectThenReconnect) {
  SetCreateContextSuccessHandler();
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_EQ(0,
            mock_ibus_input_context_client_->reset_object_proxy_call_caount());
  mock_dbus_thread_manager_->set_ibus_bus(NULL);
  ime_->OnDisconnected();
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->OnConnected();
  // Check if the old context is deleted.
  EXPECT_EQ(1,
            mock_ibus_input_context_client_->reset_object_proxy_call_caount());
  // Check if a new context is created.
  EXPECT_EQ(2, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(2, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_TRUE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus does not crash even if NULL context is
// passed.
// TODO(yusukes): Currently, ui::InputMethodIBus does not try to create ic once
// it fails (unless ibus sends the "connected" signal to Chrome again). It might
// be better to add some retry logic. Will revisit later.
TEST_F(InputMethodIBusTest, CreateContextFail) {
  CreateInputContextFailHandler create_input_context_handler;
  mock_ibus_client_->set_create_input_context_handler(base::Bind(
      &CreateInputContextFailHandler::Run,
      base::Unretained(&create_input_context_handler)));

  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  // InputMethodIBus tries several times if the CreateInputContext method call
  // is failed.
  EXPECT_EQ(kCreateInputContextMaxTrialCount,
            mock_ibus_client_->create_input_context_call_count());
  // |set_capabilities_call_count()| should be zero since a context is not
  // created yet.
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus does not crash even if ibus-daemon does not
// respond.
TEST_F(InputMethodIBusTest, CreateContextNoResp) {
  CreateInputContextNoResponseHandler create_input_context_handler;
  mock_ibus_client_->set_create_input_context_handler(base::Bind(
      &CreateInputContextNoResponseHandler::Run,
      base::Unretained(&create_input_context_handler)));

  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus does not crash even if ibus-daemon responds
// after ui::InputMethodIBus is deleted.
TEST_F(InputMethodIBusTest, CreateContextFailDelayed) {
  CreateInputContextDelayHandler create_input_context_handler(
      dbus::ObjectPath("Sample object path"));
  mock_ibus_client_->set_create_input_context_handler(base::Bind(
      &CreateInputContextDelayHandler::Run,
      base::Unretained(&create_input_context_handler)));

  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  ime_->SetFocusedTextInputClient(NULL);
  ime_.reset();
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  create_input_context_handler.RunCallback(false);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus does not crash even if ibus-daemon responds
// after ui::InputMethodIBus is deleted.
TEST_F(InputMethodIBusTest, CreateContextSuccessDelayed) {
  CreateInputContextDelayHandler create_input_context_handler(
      dbus::ObjectPath("Sample object path"));
  mock_ibus_client_->set_create_input_context_handler(base::Bind(
      &CreateInputContextDelayHandler::Run,
      base::Unretained(&create_input_context_handler)));

  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  ime_->SetFocusedTextInputClient(NULL);
  ime_.reset();
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  create_input_context_handler.RunCallback(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus does not crash even if ibus-daemon responds
// after disconnected from ibus-daemon.
TEST_F(InputMethodIBusTest, CreateContextSuccessDelayedAfterDisconnection) {
  CreateInputContextDelayHandler create_input_context_handler(
      dbus::ObjectPath("Sample object path"));
  mock_ibus_client_->set_create_input_context_handler(base::Bind(
      &CreateInputContextDelayHandler::Run,
      base::Unretained(&create_input_context_handler)));

  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  ime_->OnDisconnected();
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  create_input_context_handler.RunCallback(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that ui::InputMethodIBus does not crash even if ibus-daemon responds
// after disconnected from ibus-daemon.
TEST_F(InputMethodIBusTest, CreateContextFailDelayedAfterDisconnection) {
  CreateInputContextDelayHandler create_input_context_handler(
      dbus::ObjectPath("Sample object path"));
  mock_ibus_client_->set_create_input_context_handler(base::Bind(
      &CreateInputContextDelayHandler::Run,
      base::Unretained(&create_input_context_handler)));

  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  ime_->OnDisconnected();
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());

  create_input_context_handler.RunCallback(false);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->set_capabilities_call_count());
  EXPECT_FALSE(mock_ibus_input_context_client_->IsObjectProxyReady());
}

// Confirm that IBusClient::FocusIn is called on "connected" if input_type_ is
// TEXT.
TEST_F(InputMethodIBusTest, FocusIn_Text) {
  SetCreateContextSuccessHandler();
  ime_->Init(true);
  // A context shouldn't be created since the daemon is not running.
  EXPECT_EQ(0, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  // Start the daemon.
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->OnConnected();
  // A context should be created upon the signal delivery.
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  // Since a form has focus, IBusClient::FocusIn() should be called.
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(1,
            mock_ibus_input_context_client_->set_cursor_location_call_count());
  // ui::TextInputClient::OnInputMethodChanged() should be called when
  // ui::InputMethodIBus connects/disconnects to/from ibus-daemon and the
  // current text input type is not NONE.
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that IBusClient::FocusIn is NOT called on "connected" if input_type_
// is PASSWORD.
TEST_F(InputMethodIBusTest, FocusIn_Password) {
  SetCreateContextSuccessHandler();
  ime_->Init(true);
  EXPECT_EQ(0, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->OnConnected();
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  // Since a form has focus, IBusClient::FocusIn() should NOT be called.
  EXPECT_EQ(0, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodIBusTest, FocusOut_None) {
  SetCreateContextSuccessHandler();
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->focus_out_call_count());
  input_type_ = TEXT_INPUT_TYPE_NONE;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_out_call_count());
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodIBusTest, FocusOut_Password) {
  SetCreateContextSuccessHandler();
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->focus_out_call_count());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_out_call_count());
}

// Confirm that IBusClient::FocusOut is NOT called.
TEST_F(InputMethodIBusTest, FocusOut_Url) {
  SetCreateContextSuccessHandler();
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  ime_->Init(true);
  EXPECT_EQ(1, mock_ibus_client_->create_input_context_call_count());
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->focus_out_call_count());
  input_type_ = TEXT_INPUT_TYPE_URL;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ibus_input_context_client_->focus_in_call_count());
  EXPECT_EQ(0, mock_ibus_input_context_client_->focus_out_call_count());
}

// Test if the new |caret_bounds_| is correctly sent to ibus-daemon.
TEST_F(InputMethodIBusTest, OnCaretBoundsChanged) {
  SetCreateContextSuccessHandler();
  chromeos::DBusThreadManager::Get()->InitIBusBus("dummy address");
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->Init(true);
  EXPECT_EQ(0,
            mock_ibus_input_context_client_->set_cursor_location_call_count());
  caret_bounds_ = gfx::Rect(1, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(1,
            mock_ibus_input_context_client_->set_cursor_location_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(2,
            mock_ibus_input_context_client_->set_cursor_location_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);  // unchanged
  ime_->OnCaretBoundsChanged(this);
  // Current InputMethodIBus implementation performs the IPC regardless of the
  // bounds are changed or not.
  EXPECT_EQ(3,
            mock_ibus_input_context_client_->set_cursor_location_call_count());
}

TEST_F(InputMethodIBusTest, ExtractCompositionTextTest_NoAttribute) {
  const char kSampleText[] = "Sample Text";
  const uint32 kCursorPos = 2UL;

  const string16 utf16_string = UTF8ToUTF16(kSampleText);
  chromeos::ibus::IBusText ibus_text;
  ibus_text.set_text(kSampleText);

  CompositionText composition_text;
  ime_->ExtractCompositionText(ibus_text, kCursorPos, &composition_text);
  EXPECT_EQ(UTF8ToUTF16(kSampleText), composition_text.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  // If there is no underline, |underlines| contains one underline and it is
  // whole text underline.
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(0UL, composition_text.underlines[0].start_offset);
  EXPECT_EQ(utf16_string.size(), composition_text.underlines[0].end_offset);
  EXPECT_FALSE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodIBusTest, ExtractCompositionTextTest_SingleUnderline) {
  const char kSampleText[] = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86"
                             "\xE3\x81\x88\xE3\x81\x8A";
  const uint32 kCursorPos = 2UL;

  // Set up ibus text with one underline attribute.
  chromeos::ibus::IBusText ibus_text;
  ibus_text.set_text(kSampleText);
  chromeos::ibus::IBusText::UnderlineAttribute underline;
  underline.type = chromeos::ibus::IBusText::IBUS_TEXT_UNDERLINE_SINGLE;
  underline.start_index = 1UL;
  underline.end_index = 4UL;
  ibus_text.mutable_underline_attributes()->push_back(underline);

  CompositionText composition_text;
  ime_->ExtractCompositionText(ibus_text, kCursorPos, &composition_text);
  EXPECT_EQ(UTF8ToUTF16(kSampleText), composition_text.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_index),
            composition_text.underlines[0].end_offset);
  // Single underline represents as black thin line.
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_FALSE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodIBusTest, ExtractCompositionTextTest_DoubleUnderline) {
  const char kSampleText[] = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86"
                             "\xE3\x81\x88\xE3\x81\x8A";
  const uint32 kCursorPos = 2UL;

  // Set up ibus text with one underline attribute.
  chromeos::ibus::IBusText ibus_text;
  ibus_text.set_text(kSampleText);
  chromeos::ibus::IBusText::UnderlineAttribute underline;
  underline.type = chromeos::ibus::IBusText::IBUS_TEXT_UNDERLINE_DOUBLE;
  underline.start_index = 1UL;
  underline.end_index = 4UL;
  ibus_text.mutable_underline_attributes()->push_back(underline);

  CompositionText composition_text;
  ime_->ExtractCompositionText(ibus_text, kCursorPos, &composition_text);
  EXPECT_EQ(UTF8ToUTF16(kSampleText), composition_text.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_index),
            composition_text.underlines[0].end_offset);
  // Double underline represents as black thick line.
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodIBusTest, ExtractCompositionTextTest_ErrorUnderline) {
  const char kSampleText[] = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86"
                             "\xE3\x81\x88\xE3\x81\x8A";
  const uint32 kCursorPos = 2UL;

  // Set up ibus text with one underline attribute.
  chromeos::ibus::IBusText ibus_text;
  ibus_text.set_text(kSampleText);
  chromeos::ibus::IBusText::UnderlineAttribute underline;
  underline.type = chromeos::ibus::IBusText::IBUS_TEXT_UNDERLINE_ERROR;
  underline.start_index = 1UL;
  underline.end_index = 4UL;
  ibus_text.mutable_underline_attributes()->push_back(underline);

  CompositionText composition_text;
  ime_->ExtractCompositionText(ibus_text, kCursorPos, &composition_text);
  EXPECT_EQ(UTF8ToUTF16(kSampleText), composition_text.text);
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_index),
            composition_text.underlines[0].end_offset);
  // Error underline represents as red thin line.
  EXPECT_EQ(SK_ColorRED, composition_text.underlines[0].color);
  EXPECT_FALSE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodIBusTest, ExtractCompositionTextTest_Selection) {
  const char kSampleText[] = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86"
                             "\xE3\x81\x88\xE3\x81\x8A";
  const uint32 kCursorPos = 2UL;

  // Set up ibus text with one underline attribute.
  chromeos::ibus::IBusText ibus_text;
  ibus_text.set_text(kSampleText);
  chromeos::ibus::IBusText::SelectionAttribute selection;
  selection.start_index = 1UL;
  selection.end_index = 4UL;
  ibus_text.mutable_selection_attributes()->push_back(selection);

  CompositionText composition_text;
  ime_->ExtractCompositionText(ibus_text, kCursorPos, &composition_text);
  EXPECT_EQ(UTF8ToUTF16(kSampleText), composition_text.text);
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.end_index),
            composition_text.underlines[0].end_offset);
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodIBusTest,
       ExtractCompositionTextTest_SelectionStartWithCursor) {
  const char kSampleText[] = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86"
                             "\xE3\x81\x88\xE3\x81\x8A";
  const uint32 kCursorPos = 1UL;

  // Set up ibus text with one underline attribute.
  chromeos::ibus::IBusText ibus_text;
  ibus_text.set_text(kSampleText);
  chromeos::ibus::IBusText::SelectionAttribute selection;
  selection.start_index = kCursorPos;
  selection.end_index = 4UL;
  ibus_text.mutable_selection_attributes()->push_back(selection);

  CompositionText composition_text;
  ime_->ExtractCompositionText(ibus_text, kCursorPos, &composition_text);
  EXPECT_EQ(UTF8ToUTF16(kSampleText), composition_text.text);
  // If the cursor position is same as selection bounds, selection start
  // position become opposit side of selection from cursor.
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.end_index),
            composition_text.selection.start());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, kCursorPos),
            composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.end_index),
            composition_text.underlines[0].end_offset);
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodIBusTest, ExtractCompositionTextTest_SelectionEndWithCursor) {
  const char kSampleText[] = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86"
                             "\xE3\x81\x88\xE3\x81\x8A";
  const uint32 kCursorPos = 4UL;

  // Set up ibus text with one underline attribute.
  chromeos::ibus::IBusText ibus_text;
  ibus_text.set_text(kSampleText);
  chromeos::ibus::IBusText::SelectionAttribute selection;
  selection.start_index = 1UL;
  selection.end_index = kCursorPos;
  ibus_text.mutable_selection_attributes()->push_back(selection);

  CompositionText composition_text;
  ime_->ExtractCompositionText(ibus_text, kCursorPos, &composition_text);
  EXPECT_EQ(UTF8ToUTF16(kSampleText), composition_text.text);
  // If the cursor position is same as selection bounds, selection start
  // position become opposit side of selection from cursor.
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.start_index),
            composition_text.selection.start());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, kCursorPos),
            composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, selection.end_index),
            composition_text.underlines[0].end_offset);
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
}

// TODO(nona): Write more tests, especially for key event functions.

}  // namespace ui
