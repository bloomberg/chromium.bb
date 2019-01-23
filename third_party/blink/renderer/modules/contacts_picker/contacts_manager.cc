// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/contacts_manager.h"

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/contacts_picker/contact_info.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace mojo {

template <>
struct TypeConverter<blink::ContactInfo*, blink::mojom::blink::ContactInfoPtr> {
  static blink::ContactInfo* Convert(
      const blink::mojom::blink::ContactInfoPtr& contact);
};

blink::ContactInfo*
TypeConverter<blink::ContactInfo*, blink::mojom::blink::ContactInfoPtr>::
    Convert(const blink::mojom::blink::ContactInfoPtr& contact) {
  Vector<String> names;
  Vector<String> emails;
  Vector<String> numbers;
  if (contact->name.has_value()) {
    for (const auto& name : *contact->name)
      names.push_back(name);
  }
  if (contact->email.has_value()) {
    for (const auto& email : *contact->email)
      emails.push_back(email);
  }
  if (contact->tel.has_value()) {
    for (const auto& number : *contact->tel)
      numbers.push_back(number);
  }
  return blink::MakeGarbageCollected<blink::ContactInfo>(
      names.IsEmpty() ? base::Optional<Vector<String>>() : names,
      emails.IsEmpty() ? base::Optional<Vector<String>>() : emails,
      numbers.IsEmpty() ? base::Optional<Vector<String>>() : numbers);
}

}  // namespace mojo

namespace blink {

ContactsManager::ContactsManager() = default;
ContactsManager::~ContactsManager() = default;

mojom::blink::ContactsManagerPtr& ContactsManager::GetContactsManager(
    ScriptState* script_state) {
  if (!contacts_manager_) {
    ExecutionContext::From(script_state)
        ->GetInterfaceProvider()
        ->GetInterface(mojo::MakeRequest(&contacts_manager_));
  }
  return contacts_manager_;
}

ScriptPromise ContactsManager::select(ScriptState* script_state,
                                      ContactsSelectOptions* options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!options->hasProperties()) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kAbortError, "Dictionary must contain 'properties'"));
    return promise;
  }

  bool names = false;
  bool emails = false;
  bool tel = false;
  for (const String& property : options->properties()) {
    if (property == "name")
      names = true;
    else if (property == "email")
      emails = true;
    else if (property == "tel")
      tel = true;
  }

  if (!names && !emails && !tel) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kAbortError,
                             "'properties' must contain at least one entry"));
    return promise;
  }

  // TODO(finnur): Figure out empty-array vs null.
  GetContactsManager(script_state)
      ->Select(options->multiple(), names, emails, tel,
               WTF::Bind(&ContactsManager::OnContactsSelected,
                         WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void ContactsManager::OnContactsSelected(
    ScriptPromiseResolver* resolver,
    base::Optional<Vector<mojom::blink::ContactInfoPtr>> contacts) {
  if (!contacts.has_value()) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kAbortError,
                                          "Unable to open a contact selector"));
    return;
  }

  ScriptState::Scope scope(resolver->GetScriptState());

  HeapVector<Member<ContactInfo>> contacts_list;
  for (const auto& contact : *contacts)
    contacts_list.push_back(contact.To<blink::ContactInfo*>());

  resolver->Resolve(contacts_list);
}

}  // namespace blink
