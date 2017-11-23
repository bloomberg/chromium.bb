// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/PasswordCredential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/html/forms/FormData.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/ListedElement.h"
#include "core/html_names.h"
#include "modules/credentialmanager/PasswordCredentialData.h"
#include "platform/credentialmanager/PlatformPasswordCredential.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {

PasswordCredential* PasswordCredential::Create(
    WebPasswordCredential* web_password_credential) {
  return new PasswordCredential(web_password_credential);
}

// https://w3c.github.io/webappsec-credential-management/#construct-passwordcredential-data
PasswordCredential* PasswordCredential::Create(
    const PasswordCredentialData& data,
    ExceptionState& exception_state) {
  if (data.id().IsEmpty()) {
    exception_state.ThrowTypeError("'id' must not be empty.");
    return nullptr;
  }
  if (data.password().IsEmpty()) {
    exception_state.ThrowTypeError("'password' must not be empty.");
    return nullptr;
  }

  KURL icon_url = ParseStringAsURL(data.iconURL(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  return new PasswordCredential(data.id(), data.password(), data.name(),
                                icon_url);
}

// https://w3c.github.im/webappsec-credential-management/#construct-passwordcredential-form
PasswordCredential* PasswordCredential::Create(
    HTMLFormElement* form,
    ExceptionState& exception_state) {
  // Extract data from the form, then use the extracted |form_data| object's
  // value to populate |data|.
  FormData* form_data = FormData::Create(form);
  PasswordCredentialData data;
  for (ListedElement* submittable_element : form->ListedElements()) {
    // The "form data set" contains an entry for a |submittable_element| only if
    // it has a non-empty `name` attribute.
    // https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#constructing-the-form-data-set
    DCHECK(!submittable_element->GetName().IsEmpty());

    FileOrUSVString value;
    form_data->get(submittable_element->GetName(), value);
    if (!value.IsUSVString())
      continue;

    Vector<String> autofill_tokens;
    ToHTMLElement(submittable_element)
        ->FastGetAttribute(HTMLNames::autocompleteAttr)
        .GetString()
        .DeprecatedLower()
        .Split(' ', autofill_tokens);
    for (const auto& token : autofill_tokens) {
      if (token == "current-password" || token == "new-password") {
        data.setPassword(value.GetAsUSVString());
      } else if (token == "photo") {
        data.setIconURL(value.GetAsUSVString());
      } else if (token == "name" || token == "nickname") {
        data.setName(value.GetAsUSVString());
      } else if (token == "username") {
        data.setId(value.GetAsUSVString());
      }
    }
  }

  // Create a PasswordCredential using the data gathered above.
  return PasswordCredential::Create(data, exception_state);
}

PasswordCredential::PasswordCredential(
    WebPasswordCredential* web_password_credential)
    : Credential(web_password_credential->GetPlatformCredential()) {}

PasswordCredential::PasswordCredential(const String& id,
                                       const String& password,
                                       const String& name,
                                       const KURL& icon)
    : Credential(PlatformPasswordCredential::Create(id, password, name, icon)) {
}

const String& PasswordCredential::password() const {
  return static_cast<PlatformPasswordCredential*>(platform_credential_.Get())
      ->Password();
}

const String& PasswordCredential::name() const {
  return static_cast<PlatformPasswordCredential*>(platform_credential_.Get())
      ->Name();
}

const KURL& PasswordCredential::iconURL() const {
  return static_cast<PlatformPasswordCredential*>(platform_credential_.Get())
      ->IconURL();
}

}  // namespace blink
