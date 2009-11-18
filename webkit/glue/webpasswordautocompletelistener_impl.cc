// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides the implementaiton of the password manager's autocomplete
// component.

#include "webkit/glue/webpasswordautocompletelistener_impl.h"

#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

using namespace WebKit;

namespace webkit_glue {

WebInputElementDelegate::WebInputElementDelegate() {
}

WebInputElementDelegate::WebInputElementDelegate(WebInputElement& element)
    : element_(element) {
}

WebInputElementDelegate::~WebInputElementDelegate() {
}

void WebInputElementDelegate::SetValue(const string16& value) {
  element_.setValue(value);
}

void WebInputElementDelegate::SetSelectionRange(size_t start, size_t end) {
  element_.setSelectionRange(start, end);
}

void WebInputElementDelegate::OnFinishedAutocompleting() {
  // This sets the input element to an autofilled state which will result in it
  // having a yellow background.
  element_.setAutofilled(true);
  // Notify any changeEvent listeners.
  element_.dispatchFormControlChangeEvent();
}

void WebInputElementDelegate::RefreshAutofillPopup(
    const std::vector<string16>& suggestions,
    int default_suggestion_index) {
  WebView* webview = element_.frame()->view();
  if (webview)
    webview->applyAutofillSuggestions(element_, suggestions, 0);
}


WebPasswordAutocompleteListenerImpl::WebPasswordAutocompleteListenerImpl(
    WebInputElementDelegate* username_delegate,
    WebInputElementDelegate* password_delegate,
    const PasswordFormDomManager::FillData& data)
    : password_delegate_(password_delegate),
      username_delegate_(username_delegate),
      data_(data) {
}

void WebPasswordAutocompleteListenerImpl::didBlurInputElement(
    const WebString& user_input) {
  // If this listener exists, its because the password manager had more than
  // one match for the password form, which implies it had at least one
  // [preferred] username/password pair.
//  DCHECK(data_.basic_data.values.size() == 2);

  string16 user_input16 = user_input;

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

void WebPasswordAutocompleteListenerImpl::performInlineAutocomplete(
    const WebString& user_input,
    bool backspace_or_delete_pressed,
    bool show_suggestions) {
  // If wait_for_username is true, we only autofill the password when
  // the username field is blurred (i.e not inline) with a matching
  // username string entered.
  if (data_.wait_for_username)
    return;

  string16 user_input16 = user_input;

  if (show_suggestions) {
    std::vector<string16> suggestions;
    GetSuggestions(user_input16, &suggestions);
    username_delegate_->RefreshAutofillPopup(suggestions, 0);
  }

  if (backspace_or_delete_pressed)
    return;  // Don't inline autocomplete when the user deleted something.

  // Look for any suitable matches to current field text.
  // TODO(timsteele): The preferred login (in basic_data.values) and
  // additional logins could be bundled into the same data structure
  // (possibly even as WebCore strings) upon construction of the
  // PasswordAutocompleteListenerImpl to simplify lookup and save string
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

bool WebPasswordAutocompleteListenerImpl::TryToMatch(const string16& input,
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

void WebPasswordAutocompleteListenerImpl::GetSuggestions(
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
