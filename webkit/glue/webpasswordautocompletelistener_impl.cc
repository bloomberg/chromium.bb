// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides the implementaiton of the password manager's autocomplete
// component.

#include "webkit/glue/webpasswordautocompletelistener_impl.h"

#include <vector>

#include "base/string_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebFrame;
using WebKit::WebView;

namespace webkit_glue {

WebInputElementDelegate::WebInputElementDelegate() {
}

WebInputElementDelegate::WebInputElementDelegate(const WebInputElement& element)
    : element_(element) {
}

WebInputElementDelegate::~WebInputElementDelegate() {
}

bool WebInputElementDelegate::IsEditable() const {
  return element_.isEnabledFormControl() && !element_.hasAttribute("readonly");
}

bool WebInputElementDelegate::IsValidValue(const string16& value) {
  return element_.isValidValue(value);
}

void WebInputElementDelegate::SetValue(const string16& value) {
  element_.setValue(value);
}

bool WebInputElementDelegate::IsAutofilled() const {
  return element_.isAutofilled();
}

void WebInputElementDelegate::SetAutofilled(bool autofilled) {
  if (element_.isAutofilled() == autofilled)
    return;
  element_.setAutofilled(autofilled);
  // Notify any changeEvent listeners.
  element_.dispatchFormControlChangeEvent();
}

void WebInputElementDelegate::SetSelectionRange(size_t start, size_t end) {
  element_.setSelectionRange(start, end);
}

void WebInputElementDelegate::RefreshAutoFillPopup(
    const std::vector<string16>& suggestions) {
  WebView* webview = element_.document().frame()->view();
  if (webview) {
    std::vector<string16> names;
    std::vector<string16> labels;
    std::vector<string16> icons;
    std::vector<int> unique_ids;

    for (size_t i = 0; i < suggestions.size(); ++i) {
      names.push_back(suggestions[i]);
      labels.push_back(string16());
      icons.push_back(string16());
      unique_ids.push_back(0);
    }

    webview->applyAutoFillSuggestions(
        element_, names, labels, icons, unique_ids, -1);
  }
}

void WebInputElementDelegate::HideAutoFillPopup() {
  WebView* webview = element_.document().frame()->view();
  if (webview) {
    webview->hidePopups();
  }
}

WebPasswordAutocompleteListenerImpl::WebPasswordAutocompleteListenerImpl(
    WebInputElementDelegate* username_delegate,
    WebInputElementDelegate* password_delegate,
    const PasswordFormFillData& data)
    : password_delegate_(password_delegate),
      username_delegate_(username_delegate),
      data_(data) {
}

WebPasswordAutocompleteListenerImpl::~WebPasswordAutocompleteListenerImpl() {
}

void WebPasswordAutocompleteListenerImpl::didBlurInputElement(
    const WebString& user_input) {
  // If this listener exists, its because the password manager had more than
  // one match for the password form, which implies it had at least one
  // [preferred] username/password pair.
//  DCHECK(data_.basic_data.values.size() == 2);

  if (!password_delegate_->IsEditable())
    return;

  string16 user_input16 = user_input;

  // If enabled, set the password field to match the current username.
  if (data_.basic_data.fields[0].value == user_input16) {
    if (password_delegate_->IsValidValue(data_.basic_data.fields[1].value)) {
      // Preferred username/login is selected.
      password_delegate_->SetValue(data_.basic_data.fields[1].value);
      password_delegate_->SetAutofilled(true);
    }
  } else if (data_.additional_logins.find(user_input16) !=
      data_.additional_logins.end()) {
    if (password_delegate_->IsValidValue(
          data_.additional_logins[user_input16])) {
      // One of the extra username/logins is selected.
      password_delegate_->SetValue(data_.additional_logins[user_input16]);
      password_delegate_->SetAutofilled(true);
    }
  }
}

void WebPasswordAutocompleteListenerImpl::performInlineAutocomplete(
    const WebString& user_input,
    bool backspace_or_delete_pressed,
    bool show_suggestions) {
  // If wait_for_username is true, we only autofill the password when the
  // username field is blurred (i.e not inline) with a matching username string
  // entered.
  if (data_.wait_for_username)
    return;

  string16 user_input16 = user_input;

  // The input text is being changed, so any autofilled password is now
  // outdated.
  username_delegate_->SetAutofilled(false);
  if (password_delegate_->IsAutofilled()) {
    password_delegate_->SetValue(string16());
    password_delegate_->SetAutofilled(false);
  }

  if (show_suggestions && !showSuggestionPopup(user_input16))
      username_delegate_->HideAutoFillPopup();

  if (backspace_or_delete_pressed)
    return;  // Don't inline autocomplete when the user deleted something.

  // Look for any suitable matches to current field text.
  // TODO(timsteele): The preferred login (in basic_data.values) and additional
  // logins could be bundled into the same data structure (possibly even as
  // WebCore strings) upon construction of the PasswordAutocompleteListenerImpl
  // to simplify lookup and save string conversions (see SetValue) on each
  // successful call to OnInlineAutocompleteNeeded.
  if (TryToMatch(user_input16,
                 data_.basic_data.fields[0].value,
                 data_.basic_data.fields[1].value)) {
    return;
  }

  // Scan additional logins for a match.
  for (PasswordFormFillData::LoginCollection::iterator it =
           data_.additional_logins.begin();
       it != data_.additional_logins.end();
       ++it) {
    if (TryToMatch(user_input16, it->first, it->second))
      return;
  }
}

bool WebPasswordAutocompleteListenerImpl::showSuggestionPopup(
    const WebString& value) {
  std::vector<string16> suggestions;
  GetSuggestions(value, &suggestions);
  if (suggestions.empty())
    return false;

  username_delegate_->RefreshAutoFillPopup(suggestions);
  return true;
}

bool WebPasswordAutocompleteListenerImpl::TryToMatch(const string16& input,
                                                     const string16& username,
                                                     const string16& password) {
  if (!StartsWith(username, input, false))
    return false;

  if (!username_delegate_->IsValidValue(username))
    return false;

  // Input matches the username, fill in required values.
  username_delegate_->SetValue(username);
  username_delegate_->SetSelectionRange(input.length(), username.length());
  username_delegate_->SetAutofilled(true);
  if (password_delegate_->IsEditable() &&
      password_delegate_->IsValidValue(password)) {
    password_delegate_->SetValue(password);
    password_delegate_->SetAutofilled(true);
  }
  return true;
}

void WebPasswordAutocompleteListenerImpl::GetSuggestions(
    const string16& input, std::vector<string16>* suggestions) {
  if (StartsWith(data_.basic_data.fields[0].value, input, false))
    suggestions->push_back(data_.basic_data.fields[0].value);

  for (PasswordFormFillData::LoginCollection::iterator it =
       data_.additional_logins.begin();
       it != data_.additional_logins.end();
       ++it) {
    if (StartsWith(it->first, input, false))
      suggestions->push_back(it->first);
  }
}

}  // namespace webkit_glue
