// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/scoped_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/event.h"

namespace ui {
namespace {

class ClientChangeVerifier {
 public:
  ClientChangeVerifier()
     : previous_client_(NULL),
       next_client_(NULL),
       call_expected_(false),
       on_will_change_focused_client_called_(false),
       on_did_change_focused_client_called_(false),
       on_text_input_state_changed_(false) {
  }

  // Expects that focused text input client will not be changed.
  void ExpectClientDoesNotChange() {
    previous_client_ = NULL;
    next_client_ = NULL;
    call_expected_ = false;
    on_will_change_focused_client_called_ = false;
    on_did_change_focused_client_called_ = false;
    on_text_input_state_changed_ = false;
  }

  // Expects that focused text input client will be changed from
  // |previous_client| to |next_client|.
  void ExpectClientChange(TextInputClient* previous_client,
                          TextInputClient* next_client) {
    previous_client_ = previous_client;
    next_client_ = next_client;
    call_expected_ = true;
    on_will_change_focused_client_called_ = false;
    on_did_change_focused_client_called_ = false;
    on_text_input_state_changed_ = false;
  }

  // Verifies the result satisfies the expectation or not.
  void Verify() {
    EXPECT_EQ(call_expected_, on_will_change_focused_client_called_);
    EXPECT_EQ(call_expected_, on_did_change_focused_client_called_);
    EXPECT_EQ(call_expected_, on_text_input_state_changed_);
  }

  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) {
    EXPECT_TRUE(call_expected_);

    // Check arguments
    EXPECT_EQ(previous_client_, focused_before);
    EXPECT_EQ(next_client_, focused);

    // Check call order
    EXPECT_FALSE(on_will_change_focused_client_called_);
    EXPECT_FALSE(on_did_change_focused_client_called_);
    EXPECT_FALSE(on_text_input_state_changed_);

    on_will_change_focused_client_called_ = true;
  }

  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) {
    EXPECT_TRUE(call_expected_);

    // Check arguments
    EXPECT_EQ(previous_client_, focused_before);
    EXPECT_EQ(next_client_, focused);

    // Check call order
    EXPECT_TRUE(on_will_change_focused_client_called_);
    EXPECT_FALSE(on_did_change_focused_client_called_);
    EXPECT_FALSE(on_text_input_state_changed_);

    on_did_change_focused_client_called_ = true;
 }

  void OnTextInputStateChanged(const TextInputClient* client) {
    EXPECT_TRUE(call_expected_);

    // Check arguments
    EXPECT_EQ(next_client_, client);

    // Check call order
    EXPECT_TRUE(on_will_change_focused_client_called_);
    EXPECT_TRUE(on_did_change_focused_client_called_);
    EXPECT_FALSE(on_text_input_state_changed_);

    on_text_input_state_changed_ = true;
 }

 private:
  TextInputClient* previous_client_;
  TextInputClient* next_client_;
  bool call_expected_;
  bool on_will_change_focused_client_called_;
  bool on_did_change_focused_client_called_;
  bool on_text_input_state_changed_;

  DISALLOW_COPY_AND_ASSIGN(ClientChangeVerifier);
};

class SimpleMockInputMethodBase : public InputMethodBase {
 public:
  SimpleMockInputMethodBase() {
  }
  virtual ~SimpleMockInputMethodBase() {
  }

 private:
  // Overriden from InputMethod.
  virtual bool OnUntranslatedIMEMessage(
      const base::NativeEvent& event,
      InputMethod::NativeEventResult* result) OVERRIDE {
    return false;
  }
  virtual bool DispatchKeyEvent(const ui::KeyEvent&) OVERRIDE {
    return false;
  }
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE {
  }
  virtual std::string GetInputLocale() OVERRIDE{
    return "";
  }
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE {
    return base::i18n::UNKNOWN_DIRECTION;
  }
  virtual bool IsActive() OVERRIDE {
    return false;
  }
  virtual bool IsCandidatePopupOpen() const OVERRIDE {
    return false;
  }
  DISALLOW_COPY_AND_ASSIGN(SimpleMockInputMethodBase);
};

class MockInputMethodBase : public SimpleMockInputMethodBase {
 public:
  // Note: this class does not take the ownership of |verifier|.
  explicit MockInputMethodBase(ClientChangeVerifier* verifier)
      : verifier_(verifier) {
  }
  virtual ~MockInputMethodBase() {
  }

 private:
  // Overriden from InputMethodBase.
  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) OVERRIDE {
    verifier_->OnWillChangeFocusedClient(focused_before, focused);
  }

  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) OVERRIDE {
    verifier_->OnDidChangeFocusedClient(focused_before, focused);
  }

  ClientChangeVerifier* verifier_;
  DISALLOW_COPY_AND_ASSIGN(MockInputMethodBase);
};

class SimpleMockInputMethodObserver : public InputMethodObserver {
 public:
  SimpleMockInputMethodObserver()
      : on_caret_bounds_changed_(0),
        on_input_locale_changed_(0) {
  }
  virtual ~SimpleMockInputMethodObserver() {
  }
  void Reset() {
    on_caret_bounds_changed_ = 0;
    on_input_locale_changed_ = 0;
  }
  size_t on_caret_bounds_changed() const {
    return on_caret_bounds_changed_;
  }
  size_t on_input_locale_changed() const {
    return on_input_locale_changed_;
  }

 private:
  // Overriden from InputMethodObserver.
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE{
  }
  virtual void OnFocus() OVERRIDE{
  }
  virtual void OnBlur() OVERRIDE{
  }
  virtual void OnUntranslatedIMEMessage(
    const base::NativeEvent& event) OVERRIDE{
  }
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE{
    ++on_caret_bounds_changed_;
  }
  virtual void OnInputLocaleChanged() OVERRIDE{
    ++on_input_locale_changed_;
  }
  virtual void OnTextInputStateChanged(const TextInputClient* client) OVERRIDE{
  }
  virtual void OnInputMethodDestroyed(const InputMethod* client) OVERRIDE{
  }

  size_t on_caret_bounds_changed_;
  size_t on_input_locale_changed_;
  DISALLOW_COPY_AND_ASSIGN(SimpleMockInputMethodObserver);
};

class MockInputMethodObserver : public SimpleMockInputMethodObserver {
 public:
  // Note: this class does not take the ownership of |verifier|.
  explicit MockInputMethodObserver(ClientChangeVerifier* verifier)
      : verifier_(verifier) {
  }
  virtual ~MockInputMethodObserver() {
  }

 private:
  // Overriden from SimpleMockInputMethodObserver.
  virtual void OnTextInputStateChanged(const TextInputClient* client) OVERRIDE{
    verifier_->OnTextInputStateChanged(client);
  }

  ClientChangeVerifier* verifier_;
  DISALLOW_COPY_AND_ASSIGN(MockInputMethodObserver);
};

typedef ScopedObserver<InputMethod, InputMethodObserver>
    InputMethodScopedObserver;

TEST(InputMethodBaseTest, SetFocusedTextInputClient) {
  DummyTextInputClient text_input_client_1st;
  DummyTextInputClient text_input_client_2nd;

  ClientChangeVerifier verifier;
  MockInputMethodBase input_method(&verifier);
  MockInputMethodObserver input_method_observer(&verifier);
  InputMethodScopedObserver scoped_observer(&input_method_observer);
  scoped_observer.Add(&input_method);

  // Assume that the top-level-widget gains focus.
  input_method.OnFocus();

  {
    SCOPED_TRACE("Focus from NULL to 1st TextInputClient");

    ASSERT_EQ(NULL, input_method.GetTextInputClient());
    verifier.ExpectClientChange(NULL, &text_input_client_1st);
    input_method.SetFocusedTextInputClient(&text_input_client_1st);
    EXPECT_EQ(&text_input_client_1st, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Redundant focus events must be ignored");
    verifier.ExpectClientDoesNotChange();
    input_method.SetFocusedTextInputClient(&text_input_client_1st);
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Focus from 1st to 2nd TextInputClient");

    ASSERT_EQ(&text_input_client_1st, input_method.GetTextInputClient());
    verifier.ExpectClientChange(&text_input_client_1st,
                                &text_input_client_2nd);
    input_method.SetFocusedTextInputClient(&text_input_client_2nd);
    EXPECT_EQ(&text_input_client_2nd, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Focus from 2nd TextInputClient to NULL");

    ASSERT_EQ(&text_input_client_2nd, input_method.GetTextInputClient());
    verifier.ExpectClientChange(&text_input_client_2nd, NULL);
    input_method.SetFocusedTextInputClient(NULL);
    EXPECT_EQ(NULL, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Redundant focus events must be ignored");
    verifier.ExpectClientDoesNotChange();
    input_method.SetFocusedTextInputClient(NULL);
    verifier.Verify();
  }
}

TEST(InputMethodBaseTest, DetachTextInputClient) {
  DummyTextInputClient text_input_client;
  DummyTextInputClient text_input_client_the_other;

  ClientChangeVerifier verifier;
  MockInputMethodBase input_method(&verifier);
  MockInputMethodObserver input_method_observer(&verifier);
  InputMethodScopedObserver scoped_observer(&input_method_observer);
  scoped_observer.Add(&input_method);

  // Assume that the top-level-widget gains focus.
  input_method.OnFocus();

  // Initialize for the next test.
  {
    verifier.ExpectClientChange(NULL, &text_input_client);
    input_method.SetFocusedTextInputClient(&text_input_client);
    verifier.Verify();
  }

  {
    SCOPED_TRACE("DetachTextInputClient must be ignored for other clients");
    ASSERT_EQ(&text_input_client, input_method.GetTextInputClient());
    verifier.ExpectClientDoesNotChange();
    input_method.DetachTextInputClient(&text_input_client_the_other);
    EXPECT_EQ(&text_input_client, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("DetachTextInputClient must succeed even after the "
                 "top-level loses the focus");

    ASSERT_EQ(&text_input_client, input_method.GetTextInputClient());
    input_method.OnBlur();
    input_method.OnFocus();
    verifier.ExpectClientChange(&text_input_client, NULL);
    input_method.DetachTextInputClient(&text_input_client);
    EXPECT_EQ(NULL, input_method.GetTextInputClient());
    verifier.Verify();
  }
}

TEST(InputMethodBaseTest, OnCaretBoundsChanged) {
  DummyTextInputClient text_input_client;
  DummyTextInputClient text_input_client_the_other;

  SimpleMockInputMethodBase input_method;
  SimpleMockInputMethodObserver input_method_observer;
  InputMethodScopedObserver scoped_observer(&input_method_observer);
  scoped_observer.Add(&input_method);

  // Assume that the top-level-widget gains focus.
  input_method.OnFocus();

  {
    SCOPED_TRACE("OnCaretBoundsChanged callback must not be fired when no text "
        "input client is focused");
    ASSERT_EQ(NULL, input_method.GetTextInputClient());

    input_method_observer.Reset();
    input_method.OnCaretBoundsChanged(&text_input_client);
    EXPECT_EQ(0u, input_method_observer.on_caret_bounds_changed());
    input_method.OnCaretBoundsChanged(NULL);
    EXPECT_EQ(0u, input_method_observer.on_caret_bounds_changed());
  }

  {
    SCOPED_TRACE("OnCaretBoundsChanged callback must be fired when and only "
        "the event is notified from the focused text input client");

    input_method.SetFocusedTextInputClient(&text_input_client);
    ASSERT_EQ(&text_input_client, input_method.GetTextInputClient());

    // Must fire the event
    input_method_observer.Reset();
    input_method.OnCaretBoundsChanged(&text_input_client);
    EXPECT_EQ(1u, input_method_observer.on_caret_bounds_changed());

    // Must not fire the event
    input_method_observer.Reset();
    input_method.OnCaretBoundsChanged(NULL);
    EXPECT_EQ(0u, input_method_observer.on_caret_bounds_changed());

    // Must not fire the event
    input_method_observer.Reset();
    input_method.OnCaretBoundsChanged(&text_input_client_the_other);
    EXPECT_EQ(0u, input_method_observer.on_caret_bounds_changed());
  }
}

TEST(InputMethodBaseTest, OnInputLocaleChanged) {
  DummyTextInputClient text_input_client;

  SimpleMockInputMethodBase input_method;
  SimpleMockInputMethodObserver input_method_observer;
  InputMethodScopedObserver scoped_observer(&input_method_observer);
  scoped_observer.Add(&input_method);

  // Assume that the top-level-widget gains focus.
  input_method.OnFocus();

  {
    SCOPED_TRACE("OnInputLocaleChanged callback can be fired even when no text "
        "input client is focused");
    ASSERT_EQ(NULL, input_method.GetTextInputClient());

    input_method_observer.Reset();
    input_method.OnInputLocaleChanged();
    EXPECT_EQ(1u, input_method_observer.on_input_locale_changed());

    input_method.SetFocusedTextInputClient(&text_input_client);
    input_method_observer.Reset();
    input_method.OnInputLocaleChanged();
    EXPECT_EQ(1u, input_method_observer.on_input_locale_changed());
  }
}

}  // namespace
}  // namespace ui
