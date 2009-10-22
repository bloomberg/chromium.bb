// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides the implementaiton of the password manager's autocomplete
// component.

#include "config.h"

#include "HTMLInputElement.h"
#undef LOG

#include "base/string_util.h"
#include "webkit/api/public/WebNode.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/password_autocomplete_listener.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

namespace webkit_glue {

HTMLInputDelegate::HTMLInputDelegate(WebCore::HTMLInputElement* element)
    : element_(element) {
  // Reference the element for the lifetime of this delegate.
  // element is NULL when testing.
  if (element_)
    element_->ref();
}

HTMLInputDelegate::~HTMLInputDelegate() {
  if (element_)
    element_->deref();
}

void HTMLInputDelegate::SetValue(const string16& value) {
  element_->setValue(String16ToString(value));
}

void HTMLInputDelegate::SetSelectionRange(size_t start, size_t end) {
  element_->setSelectionRange(start, end);
}

void HTMLInputDelegate::OnFinishedAutocompleting() {
  // This sets the input element to an autofilled state which will result in it
  // having a yellow background.
  element_->setAutofilled(true);
  // Notify any changeEvent listeners.
  element_->dispatchFormControlChangeEvent();
}

void HTMLInputDelegate::RefreshAutofillPopup(
    const std::vector<string16>& suggestions,
    int default_suggestion_index) {
  WebFrameImpl* webframe =
      WebFrameImpl::FromFrame(element_->document()->frame());
  WebViewImpl* webview = webframe->GetWebViewImpl();
  if (!webview)
    return;

  webview->applyAutofillSuggestions(
      webkit_glue::NodeToWebNode(element_), suggestions, 0);
}

PasswordAutocompleteListener::PasswordAutocompleteListener(
    HTMLInputDelegate* username_delegate,
    HTMLInputDelegate* password_delegate,
    const PasswordFormDomManager::FillData& data)
    : password_delegate_(password_delegate),
      username_delegate_(username_delegate),
      data_(data) {
}

void PasswordAutocompleteListener::OnBlur(WebCore::HTMLInputElement* element,
                                          const WebCore::String& user_input) {
  // If this listener exists, its because the password manager had more than
  // one match for the password form, which implies it had at least one
  // [preferred] username/password pair.
  ASSERT(data_.basic_data.values.size() == 2);

  string16 user_input16 = webkit_glue::StringToString16(user_input);

  // Set the password field to match the current username.
  if (data_.basic_data.values[0] == user_input16) {
    // Preferred username/login is selected.
    password_delegate_->SetValue(data_.basic_data.values[1]);
  } else if (data_.additional_logins.find(user_input16) !=
             data_.additional_logins.end()) {
    // One of the extra username/logins is selected.
    password_delegate_->SetValue(data_.additional_logins[user_input16]);
  }
  password_delegate_->OnFinishedAutocompleting();
}

void PasswordAutocompleteListener::OnInlineAutocompleteNeeded(
    WebCore::HTMLInputElement* element,
    const WebCore::String& user_input,
    bool backspace_or_delete,
    bool with_suggestion_popup) {
  // If wait_for_username is true, we only autofill the password when
  // the username field is blurred (i.e not inline) with a matching
  // username string entered.
  if (data_.wait_for_username)
    return;

  string16 user_input16 = webkit_glue::StringToString16(user_input);

  if (with_suggestion_popup) {
    std::vector<string16> suggestions;
    GetSuggestions(user_input16, &suggestions);
    username_delegate_->RefreshAutofillPopup(suggestions, 0);
  }

  if (backspace_or_delete)
    return;  // Don't inline autocomplete when the user deleted something.

  // Look for any suitable matches to current field text.
  // TODO(timsteele): The preferred login (in basic_data.values) and
  // additional logins could be bundled into the same data structure
  // (possibly even as WebCore strings) upon construction of the
  // PasswordAutocompleteListener to simplify lookup and save string
  // conversions (see SetValue) on each successful call to
  // OnInlineAutocompleteNeeded.
  if (TryToMatch(user_input16,
                 data_.basic_data.values[0],
                 data_.basic_data.values[1])) {
    return;
  }

  // Scan additional logins for a match.
  for (PasswordFormDomManager::LoginCollection::iterator it =
           data_.additional_logins.begin();
       it != data_.additional_logins.end();
       ++it) {
    if (TryToMatch(user_input16, it->first, it->second))
      return;
  }
}

bool PasswordAutocompleteListener::TryToMatch(const string16& input,
                                              const string16& username,
                                              const string16& password) {
  if (!StartsWith(username, input, false))
    return false;

  // Input matches the username, fill in required values.
  username_delegate_->SetValue(username);
  username_delegate_->SetSelectionRange(input.length(), username.length());
  username_delegate_->OnFinishedAutocompleting();
  password_delegate_->SetValue(password);
  password_delegate_->OnFinishedAutocompleting();
  return true;
}

void PasswordAutocompleteListener::GetSuggestions(
    const string16& input, std::vector<string16>* suggestions) {
  if (StartsWith(data_.basic_data.values[0], input, false))
    suggestions->push_back(data_.basic_data.values[0]);

  for (PasswordFormDomManager::LoginCollection::iterator it =
       data_.additional_logins.begin();
       it != data_.additional_logins.end();
       ++it) {
    if (StartsWith(it->first, input, false))
      suggestions->push_back(it->first);
  }
}

}  // webkit_glue
