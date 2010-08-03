// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The PasswordManagerAutocompleteTests in this file test only the
// PasswordAutocompleteListener class implementation (and not any of the
// higher level dom autocomplete framework).

#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/webpasswordautocompletelistener_impl.h"

using WebKit::WebString;
using webkit_glue::FormField;
using webkit_glue::PasswordFormDomManager;
using webkit_glue::PasswordFormFillData;
using webkit_glue::WebInputElementDelegate;
using webkit_glue::WebPasswordAutocompleteListenerImpl;

class TestWebInputElementDelegate : public WebInputElementDelegate {
 public:
  TestWebInputElementDelegate()
      : WebInputElementDelegate(),
        selection_start_(0),
        selection_end_(0),
        is_editable_(true),
        is_autofilled_(false) {
  }

  // Override those methods we implicitly invoke in the tests.
  virtual bool IsEditable() const {
    return is_editable_;
  }

  virtual void SetValue(const string16& value) {
    value_ = value;
  }

  virtual bool IsAutofilled() const { return is_autofilled_; }

  virtual void SetAutofilled(bool autofilled) {
    is_autofilled_ = autofilled;
  }

  virtual void SetSelectionRange(size_t start, size_t end) {
    selection_start_ = start;
    selection_end_ = end;
  }

  // Testing-only methods.
  void set_is_editable(bool editable) {
    is_editable_ = editable;
  }

  string16 value() const {
    return value_;
  }

  size_t selection_start() const {
    return selection_start_;
  }

  size_t selection_end() const {
    return selection_end_;
  }

 private:
  string16 value_;
  size_t selection_start_;
  size_t selection_end_;
  bool is_editable_;
  bool is_autofilled_;
};

namespace {
class PasswordManagerAutocompleteTests : public testing::Test {
 public:
  PasswordManagerAutocompleteTests()
      : username_delegate_(NULL),
        password_delegate_(NULL) {
  }

  virtual void SetUp() {
    // Add a preferred login and an additional login to the FillData.
    username1_ = ASCIIToUTF16("alice");
    password1_ = ASCIIToUTF16("password");
    username2_ = ASCIIToUTF16("bob");
    password2_ = ASCIIToUTF16("bobsyouruncle");
    data_.basic_data.fields.push_back(FormField(string16(),
                                                string16(),
                                                username1_,
                                                string16(),
                                                0));
    data_.basic_data.fields.push_back(FormField(string16(),
                                                string16(),
                                                password1_,
                                                string16(),
                                                0));
    data_.additional_logins[username2_] = password2_;

    username_delegate_ = new TestWebInputElementDelegate();
    password_delegate_ = new TestWebInputElementDelegate();

    testing::Test::SetUp();
  }

 protected:
  // Create the WebPasswordAutocompleteListener associated with
  // username_delegate_ and password_delegate_, should be called only once at
  // the begining of the test.
  WebKit::WebPasswordAutocompleteListener* CreateListener(
      bool wait_for_username) {
    DCHECK(!listener_.get());
    data_.wait_for_username = wait_for_username;
    listener_.reset(new WebPasswordAutocompleteListenerImpl(username_delegate_,
                                                            password_delegate_,
                                                            data_));
    return listener_.get();
  }

  string16 username1_;
  string16 password1_;
  string16 username2_;
  string16 password2_;
  PasswordFormFillData data_;
  TestWebInputElementDelegate* username_delegate_;
  TestWebInputElementDelegate* password_delegate_;

 private:
  scoped_ptr<WebPasswordAutocompleteListenerImpl> listener_;
};

TEST_F(PasswordManagerAutocompleteTests, OnBlur) {
  WebKit::WebPasswordAutocompleteListener* listener = CreateListener(false);

  // Make the password field read-only.
  password_delegate_->set_is_editable(false);
  // Simulate a blur event on the username field, but r/o password won't fill.
  listener->didBlurInputElement(username1_);
  EXPECT_TRUE(password_delegate_->value().empty());
  EXPECT_FALSE(password_delegate_->IsAutofilled());
  password_delegate_->set_is_editable(true);

  // Simulate a blur event on the username field and expect a password autofill.
  listener->didBlurInputElement(username1_);
  EXPECT_EQ(password1_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());

  // Now the user goes back and changes the username to something we don't
  // have saved. The password should remain unchanged.
  listener->didBlurInputElement(ASCIIToUTF16("blahblahblah"));
  EXPECT_EQ(password1_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());

  // Now they type in the additional login username.
  listener->didBlurInputElement(username2_);
  EXPECT_EQ(password2_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());
}

TEST_F(PasswordManagerAutocompleteTests, OnInlineAutocompleteNeeded) {
  WebKit::WebPasswordAutocompleteListener* listener = CreateListener(false);

  // Simulate the user typing in the first letter of 'alice', a stored username.
  listener->performInlineAutocomplete(ASCIIToUTF16("a"), false, false);
  // Both the username and password delegates should reflect selection
  // of the stored login.
  EXPECT_EQ(username1_, username_delegate_->value());
  EXPECT_TRUE(username_delegate_->IsAutofilled());
  EXPECT_EQ(password1_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());
  // And the selection should have been set to 'lice', the last 4 letters.
  EXPECT_EQ(1U, username_delegate_->selection_start());
  EXPECT_EQ(username1_.length(), username_delegate_->selection_end());
  // And both fields should have the autofill style.
  EXPECT_TRUE(username_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->IsAutofilled());

  // Now the user types the next letter of the same username, 'l'.
  listener->performInlineAutocomplete(ASCIIToUTF16("al"), false, false);
  // Now the fields should have the same value, but the selection should have a
  // different start value.
  EXPECT_EQ(username1_, username_delegate_->value());
  EXPECT_TRUE(username_delegate_->IsAutofilled());
  EXPECT_EQ(password1_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());
  EXPECT_EQ(2U, username_delegate_->selection_start());
  EXPECT_EQ(username1_.length(), username_delegate_->selection_end());

  // Now lets say the user goes astray from the stored username and types
  // the letter 'f', spelling 'alf'.  We don't know alf (that's just sad),
  // so in practice the username should no longer be 'alice' and the selected
  // range should be empty. In our case, when the autocomplete code doesn't
  // know the text, it won't set the value or the selection and hence our
  // delegate methods won't get called. The WebCore::HTMLInputElement's value
  // and selection would be set directly by WebCore in practice.

  // Reset the delegate's test state so we can determine what, if anything,
  // was set during performInlineAutocomplete.
  username_delegate_->SetValue(string16());
  username_delegate_->SetSelectionRange(0, 0);
  listener->performInlineAutocomplete(ASCIIToUTF16("alf"), false, false);
  EXPECT_EQ(0U, username_delegate_->selection_start());
  EXPECT_EQ(0U, username_delegate_->selection_end());
  // Username should not have been filled by us.
  EXPECT_TRUE(username_delegate_->value().empty());
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->value().empty());
  EXPECT_FALSE(password_delegate_->IsAutofilled());

  // Ok, so now the user removes all the text and enters the letter 'b'.
  listener->performInlineAutocomplete(ASCIIToUTF16("b"), false, false);
  // The username and password fields should match the 'bob' entry.
  EXPECT_EQ(username2_, username_delegate_->value());
  EXPECT_TRUE(username_delegate_->IsAutofilled());
  EXPECT_EQ(password2_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());
  EXPECT_EQ(1U, username_delegate_->selection_start());
  EXPECT_EQ(username2_.length(), username_delegate_->selection_end());
}

TEST_F(PasswordManagerAutocompleteTests, TestWaitUsername) {
  // If we had an action authority mismatch (for example), we don't want to
  // automatically autofill anything without some user interaction first.
  // We require an explicit blur on the username field, and that a valid
  // matching username is in the field, before we autofill passwords.
  WebKit::WebPasswordAutocompleteListener* listener = CreateListener(true);

  // In all cases, username_delegate should remain empty because we should
  // never modify it when wait_for_username is true; only the user can by
  // typing into (in real life) the HTMLInputElement.
  password_delegate_->SetValue(string16());
  listener->performInlineAutocomplete(ASCIIToUTF16("a"), false, false);
  EXPECT_TRUE(username_delegate_->value().empty());
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->value().empty());
  EXPECT_FALSE(password_delegate_->IsAutofilled());
  listener->performInlineAutocomplete(ASCIIToUTF16("al"), false, false);
  EXPECT_TRUE(username_delegate_->value().empty());
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->value().empty());
  EXPECT_FALSE(password_delegate_->IsAutofilled());
  listener->performInlineAutocomplete(ASCIIToUTF16("alice"), false, false);
  EXPECT_TRUE(username_delegate_->value().empty());
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->value().empty());
  EXPECT_FALSE(password_delegate_->IsAutofilled());

  listener->didBlurInputElement(ASCIIToUTF16("a"));
  EXPECT_TRUE(username_delegate_->value().empty());
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->value().empty());
  EXPECT_FALSE(password_delegate_->IsAutofilled());
  listener->didBlurInputElement(ASCIIToUTF16("ali"));
  EXPECT_TRUE(username_delegate_->value().empty());
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->value().empty());
  EXPECT_FALSE(password_delegate_->IsAutofilled());

  // Blur with 'alice' should allow password autofill.
  listener->didBlurInputElement(ASCIIToUTF16("alice"));
  EXPECT_TRUE(username_delegate_->value().empty());
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_EQ(password1_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());
}

// Tests that editing the password clears the autofilled password field.
TEST_F(PasswordManagerAutocompleteTests, TestPasswordClearOnEdit) {
  WebKit::WebPasswordAutocompleteListener* listener = CreateListener(false);

  // User enters a known login.
  listener->performInlineAutocomplete(ASCIIToUTF16("alice"), false, false);
  // We are autofilled.
  EXPECT_TRUE(username_delegate_->IsAutofilled());
  EXPECT_EQ(password1_, password_delegate_->value());
  EXPECT_TRUE(password_delegate_->IsAutofilled());

  // User modifies the login name to an unknown one.
  listener->performInlineAutocomplete(ASCIIToUTF16("alicia"), false, false);
  // We should not be autofilled anymore and the password should have been
  // cleared.
  EXPECT_FALSE(username_delegate_->IsAutofilled());
  EXPECT_FALSE(password_delegate_->IsAutofilled());
  EXPECT_TRUE(password_delegate_->value().empty());
}

}  // namespace
