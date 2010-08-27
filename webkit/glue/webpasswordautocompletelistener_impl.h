// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A concrete definition of the DOM autocomplete framework defined by
// autocomplete_input_listener.h, for the password manager.

#ifndef WEBKIT_GLUE_PASSWORDAUTOCOMPLETELISTENER_IMPL_H_
#define WEBKIT_GLUE_PASSWORDAUTOCOMPLETELISTENER_IMPL_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPasswordAutocompleteListener.h"
#include "webkit/glue/password_form_dom_manager.h"

using WebKit::WebInputElement;
using WebKit::WebString;

namespace webkit_glue {

// A proxy interface to a WebInputElement for inline autocomplete. The proxy
// is overridden by webpasswordautocompletelistener_unittest.
class WebInputElementDelegate {
 public:
  WebInputElementDelegate();
  WebInputElementDelegate(const WebInputElement& element);
  virtual ~WebInputElementDelegate();

  // These are virtual to support unit testing.
  virtual bool IsEditable() const;
  virtual void SetValue(const string16& value);
  virtual bool IsAutofilled() const;
  virtual void SetAutofilled(bool autofilled);
  virtual void SetSelectionRange(size_t start, size_t end);
  virtual void RefreshAutofillPopup(const std::vector<string16>& suggestions);

 private:
  // The underlying DOM element we're wrapping.
  WebInputElement element_;

  DISALLOW_COPY_AND_ASSIGN(WebInputElementDelegate);
};

class WebPasswordAutocompleteListenerImpl :
    public WebKit::WebPasswordAutocompleteListener {
 public:
  WebPasswordAutocompleteListenerImpl(
      WebInputElementDelegate* username_element,
      WebInputElementDelegate* password_element,
      const PasswordFormFillData& data);
  virtual ~WebPasswordAutocompleteListenerImpl();

  // WebKit::PasswordAutocompleteListener methods:
  virtual void didBlurInputElement(const WebString& user_input);
  virtual void performInlineAutocomplete(const WebString& user_input,
                                         bool backspace_or_delete_pressed,
                                         bool show_suggestions);
  virtual bool showSuggestionPopup(const WebString& value);

 private:
  // Check if the input string resembles a potential matching login
  // (username/password) and if so, match them up by autocompleting the edit
  // delegates.
  bool TryToMatch(const string16& input,
                  const string16& username,
                  const string16& password);

  // Scan |data_| for prefix matches of |input| and add each to |suggestions|.
  void GetSuggestions(const string16& input,
                      std::vector<string16>* suggestions);

  // Access to password field to autocomplete on blur/username updates.
  scoped_ptr<WebInputElementDelegate> password_delegate_;
  scoped_ptr<WebInputElementDelegate> username_delegate_;

  // Contains the extra logins for matching on delta/blur.
  PasswordFormFillData data_;

  DISALLOW_COPY_AND_ASSIGN(WebPasswordAutocompleteListenerImpl);
};

}  // webkit_glue

#endif  // WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H_
