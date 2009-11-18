// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The PasswordManagerAutocompleteTests in this file test only the
// PasswordAutocompleteListener class implementation (and not any of the
// higher level dom autocomplete framework).

#include <string>

#include "base/string_util.h"
#include "webkit/glue/webpasswordautocompletelistener_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebKit::WebString;
using webkit_glue::WebPasswordAutocompleteListenerImpl;
using webkit_glue::PasswordFormDomManager;
using webkit_glue::WebInputElementDelegate;

class TestWebInputElementDelegate : public WebInputElementDelegate {
 public:
  TestWebInputElementDelegate() : WebInputElementDelegate(),
                                  did_call_on_finish_(false),
                                  did_set_value_(false),
                                  did_set_selection_(false),
                                  selection_start_(0),
                                  selection_end_(0) {
  }

  // Override those methods we implicitly invoke in the tests.
  virtual void SetValue(const string16& value) {
    value_ = value;
    did_set_value_ = true;
  }

  virtual void OnFinishedAutocompleting() {
    did_call_on_finish_ = true;
  }

  virtual void SetSelectionRange(size_t start, size_t end) {
    selection_start_ = start;
    selection_end_ = end;
    did_set_selection_ = true;
  }

  // Testing-only methods.
  void ResetTestState() {
    did_call_on_finish_ = false;
    did_set_value_ = false;
    did_set_selection_ = false;
  }

  string16 value() const {
    return value_;
  }

  bool did_call_on_finish() const {
    return did_call_on_finish_;
  }

  bool did_set_value() const {
    return did_set_value_;
  }

  bool did_set_selection() const {
    return did_set_selection_;
  }

  size_t selection_start() const {
    return selection_start_;
  }

  size_t selection_end() const {
    return selection_end_;
  }

 private:
  bool did_call_on_finish_;
  bool did_set_value_;
  bool did_set_selection_;
  string16 value_;
  size_t selection_start_;
  size_t selection_end_;
};

namespace {
class PasswordManagerAutocompleteTests : public testing::Test {
 public:
  virtual void SetUp() {
    // Add a preferred login and an additional login to the FillData.
    username1_ = ASCIIToUTF16("alice");
    password1_ = ASCIIToUTF16("password");
    username2_ = ASCIIToUTF16("bob");
    password2_ = ASCIIToUTF16("bobsyouruncle");
    data_.basic_data.values.push_back(username1_);
    data_.basic_data.values.push_back(password1_);
    data_.additional_logins[username2_] = password2_;
    testing::Test::SetUp();
  }

  string16 username1_;
  string16 password1_;
  string16 username2_;
  string16 password2_;
  PasswordFormDomManager::FillData data_;
};

TEST_F(PasswordManagerAutocompleteTests, OnBlur) {
  TestWebInputElementDelegate* username_delegate =
      new TestWebInputElementDelegate();
  TestWebInputElementDelegate* password_delegate =
      new TestWebInputElementDelegate();

  scoped_ptr<WebPasswordAutocompleteListenerImpl> listener(
      new WebPasswordAutocompleteListenerImpl(username_delegate,
                                              password_delegate,
                                              data_));

  // Clear the password field.
  password_delegate->SetValue(string16());
  // Simulate a blur event on the username field and expect a password autofill.
  listener->didBlurInputElement(username1_);
  EXPECT_EQ(password1_, password_delegate->value());

  // Now the user goes back and changes the username to something we don't
  // have saved. The password should remain unchanged.
  listener->didBlurInputElement(ASCIIToUTF16("blahblahblah"));
  EXPECT_EQ(password1_, password_delegate->value());

  // Now they type in the additional login username.
  listener->didBlurInputElement(username2_);
  EXPECT_EQ(password2_, password_delegate->value());
}

TEST_F(PasswordManagerAutocompleteTests, OnInlineAutocompleteNeeded) {
  TestWebInputElementDelegate* username_delegate =
      new TestWebInputElementDelegate();
  TestWebInputElementDelegate* password_delegate =
      new TestWebInputElementDelegate();

  scoped_ptr<WebPasswordAutocompleteListenerImpl> listener(
      new WebPasswordAutocompleteListenerImpl(username_delegate,
                                              password_delegate,
                                              data_));

  password_delegate->SetValue(string16());
  // Simulate the user typing in the first letter of 'alice', a stored username.
  listener->performInlineAutocomplete(ASCIIToUTF16("a"), false, false);
  // Both the username and password delegates should reflect selection
  // of the stored login.
  EXPECT_EQ(username1_, username_delegate->value());
  EXPECT_EQ(password1_, password_delegate->value());
  // And the selection should have been set to 'lice', the last 4 letters.
  EXPECT_EQ(1U, username_delegate->selection_start());
  EXPECT_EQ(username1_.length(), username_delegate->selection_end());
  // And both fields should have observed OnFinishedAutocompleting.
  EXPECT_TRUE(username_delegate->did_call_on_finish());
  EXPECT_TRUE(password_delegate->did_call_on_finish());

  // Now the user types the next letter of the same username, 'l'.
  listener->performInlineAutocomplete(ASCIIToUTF16("al"), false, false);
  // Now the fields should have the same value, but the selection should have a
  // different start value.
  EXPECT_EQ(username1_, username_delegate->value());
  EXPECT_EQ(password1_, password_delegate->value());
  EXPECT_EQ(2U, username_delegate->selection_start());
  EXPECT_EQ(username1_.length(), username_delegate->selection_end());

  // Now lets say the user goes astray from the stored username and types
  // the letter 'f', spelling 'alf'.  We don't know alf (that's just sad),
  // so in practice the username should no longer be 'alice' and the selected
  // range should be empty. In our case, when the autocomplete code doesn't
  // know the text, it won't set the value or the selection and hence our
  // delegate methods won't get called. The WebCore::HTMLInputElement's value
  // and selection would be set directly by WebCore in practice.

  // Reset the delegate's test state so we can determine what, if anything,
  // was invoked during performInlineAutocomplete.
  username_delegate->ResetTestState();
  password_delegate->ResetTestState();
  listener->performInlineAutocomplete(ASCIIToUTF16("alf"), false, false);
  EXPECT_FALSE(username_delegate->did_set_selection());
  EXPECT_FALSE(username_delegate->did_set_value());
  EXPECT_FALSE(username_delegate->did_call_on_finish());
  EXPECT_FALSE(password_delegate->did_set_value());
  EXPECT_FALSE(password_delegate->did_call_on_finish());

  // Ok, so now the user removes all the text and enters the letter 'b'.
  listener->performInlineAutocomplete(ASCIIToUTF16("b"), false, false);
  // The username and password fields should match the 'bob' entry.
  EXPECT_EQ(username2_, username_delegate->value());
  EXPECT_EQ(password2_, password_delegate->value());
  EXPECT_EQ(1U, username_delegate->selection_start());
  EXPECT_EQ(username2_.length(), username_delegate->selection_end());
}

TEST_F(PasswordManagerAutocompleteTests, TestWaitUsername) {
  TestWebInputElementDelegate* username_delegate =
      new TestWebInputElementDelegate();
  TestWebInputElementDelegate* password_delegate =
      new TestWebInputElementDelegate();

  // If we had an action authority mismatch (for example), we don't want to
  // automatically autofill anything without some user interaction first.
  // We require an explicit blur on the username field, and that a valid
  // matching username is in the field, before we autofill passwords.
  data_.wait_for_username = true;
  scoped_ptr<WebPasswordAutocompleteListenerImpl> listener(
      new WebPasswordAutocompleteListenerImpl(username_delegate,
                                              password_delegate,
                                              data_));

  string16 empty;
  // In all cases, username_delegate should remain empty because we should
  // never modify it when wait_for_username is true; only the user can by
  // typing into (in real life) the HTMLInputElement.
  password_delegate->SetValue(string16());
  listener->performInlineAutocomplete(ASCIIToUTF16("a"), false, false);
  EXPECT_EQ(empty, username_delegate->value());
  EXPECT_EQ(empty, password_delegate->value());
  listener->performInlineAutocomplete(ASCIIToUTF16("al"), false, false);
  EXPECT_EQ(empty, username_delegate->value());
  EXPECT_EQ(empty, password_delegate->value());
  listener->performInlineAutocomplete(ASCIIToUTF16("alice"), false, false);
  EXPECT_EQ(empty, username_delegate->value());
  EXPECT_EQ(empty, password_delegate->value());

  listener->didBlurInputElement(ASCIIToUTF16("a"));
  EXPECT_EQ(empty, username_delegate->value());
  EXPECT_EQ(empty, password_delegate->value());
  listener->didBlurInputElement(ASCIIToUTF16("ali"));
  EXPECT_EQ(empty, username_delegate->value());
  EXPECT_EQ(empty, password_delegate->value());

  // Blur with 'alice' should allow password autofill.
  listener->didBlurInputElement(ASCIIToUTF16("alice"));
  EXPECT_EQ(empty, username_delegate->value());
  EXPECT_EQ(password1_, password_delegate->value());
}

}  // namespace
