// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A concrete definition of the DOM autocomplete framework defined by
// autocomplete_input_listener.h, for the password manager.

#ifndef WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H_
#define WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/api/src/PasswordAutocompleteListener.h"
#include "webkit/glue/password_form_dom_manager.h"

namespace WebCore {
class HTMLInputElement;
}

namespace webkit_glue {

// A proxy interface to a WebCore::HTMLInputElement for inline autocomplete.
// The delegate does not own the WebCore element; it only interfaces it.
class HTMLInputDelegate {
 public:
  explicit HTMLInputDelegate(WebCore::HTMLInputElement* element);
  virtual ~HTMLInputDelegate();

  // These are virtual to support unit testing.
  virtual void SetValue(const string16& value);
  virtual void SetSelectionRange(size_t start, size_t end);
  virtual void OnFinishedAutocompleting();
  virtual void RefreshAutofillPopup(
      const std::vector<string16>& suggestions,
      int default_suggestion_index);

 private:
  // The underlying DOM element we're wrapping. We reference the underlying
  // HTMLInputElement for its lifetime to ensure it does not get freed by
  // WebCore while in use by the delegate instance.
  RefPtr<WebCore::HTMLInputElement> element_;

  DISALLOW_COPY_AND_ASSIGN(HTMLInputDelegate);
};

class PasswordAutocompleteListenerImpl :
    public WebKit::PasswordAutocompleteListener {
 public:
  PasswordAutocompleteListenerImpl(
      HTMLInputDelegate* username_delegate,
      HTMLInputDelegate* password_delegate,
      const PasswordFormDomManager::FillData& data);
  ~PasswordAutocompleteListenerImpl() {
  }

  // WebKit::PasswordAutocompleteListener methods:
  virtual void didBlurInputElement(const WebCore::String& user_input);
  virtual void performInlineAutocomplete(const WebCore::String& user_input,
                                         bool backspace_or_delete_pressed,
                                         bool show_suggestions);

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
  scoped_ptr<HTMLInputDelegate> password_delegate_;
  scoped_ptr<HTMLInputDelegate> username_delegate_;

  // Contains the extra logins for matching on delta/blur.
  PasswordFormDomManager::FillData data_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutocompleteListenerImpl);
};

}  // webkit_glue

#endif  // WEBKIT_GLUE_PASSWORD_AUTOCOMPLETE_LISTENER_H_
