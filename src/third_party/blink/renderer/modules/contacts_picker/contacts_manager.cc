// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/contacts_manager.h"

#include "base/stl_util.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_contact_info.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/contacts_picker/contact_address.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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
  blink::ContactInfo* contact_info = blink::ContactInfo::Create();

  if (contact->name) {
    Vector<String> names;
    names.ReserveInitialCapacity(contact->name->size());

    for (const String& name : *contact->name)
      names.push_back(name);

    contact_info->setName(names);
  }

  if (contact->email) {
    Vector<String> emails;
    emails.ReserveInitialCapacity(contact->email->size());

    for (const String& email : *contact->email)
      emails.push_back(email);

    contact_info->setEmail(emails);
  }

  if (contact->tel) {
    Vector<String> numbers;
    numbers.ReserveInitialCapacity(contact->tel->size());

    for (const String& number : *contact->tel)
      numbers.push_back(number);

    contact_info->setTel(numbers);
  }

  if (contact->address) {
    blink::HeapVector<blink::Member<blink::ContactAddress>> addresses;
    for (auto& address : *contact->address) {
      auto* blink_address = blink::MakeGarbageCollected<blink::ContactAddress>(
          std::move(address));
      addresses.push_back(blink_address);
    }

    contact_info->setAddress(addresses);
  }

  if (contact->icon) {
    blink::HeapVector<blink::Member<blink::Blob>> icons;
    for (blink::mojom::blink::ContactIconBlobPtr& icon : *contact->icon) {
      icons.push_back(blink::Blob::Create(icon->data.data(), icon->data.size(),
                                          icon->mime_type));
    }

    contact_info->setIcon(icons);
  }

  return contact_info;
}

}  // namespace mojo

namespace blink {
namespace {

// ContactProperty enum strings.
constexpr char kAddress[] = "address";
constexpr char kEmail[] = "email";
constexpr char kName[] = "name";
constexpr char kTel[] = "tel";
constexpr char kIcon[] = "icon";

}  // namespace

ContactsManager::ContactsManager() : contacts_manager_(nullptr) {}

ContactsManager::~ContactsManager() = default;

mojom::blink::ContactsManager* ContactsManager::GetContactsManager(
    ScriptState* script_state) {
  if (!contacts_manager_.is_bound()) {
    ExecutionContext::From(script_state)
        ->GetBrowserInterfaceBroker()
        .GetInterface(contacts_manager_.BindNewPipeAndPassReceiver(
            ExecutionContext::From(script_state)
                ->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return contacts_manager_.get();
}

const Vector<String>& ContactsManager::GetProperties(
    ScriptState* script_state) {
  if (properties_.IsEmpty()) {
    properties_ = {kEmail, kName, kTel};

    if (RuntimeEnabledFeatures::ContactsManagerExtraPropertiesEnabled(
            ExecutionContext::From(script_state))) {
      properties_.push_back(kAddress);
      properties_.push_back(kIcon);
    }
  }
  return properties_;
}

ScriptPromise ContactsManager::select(ScriptState* script_state,
                                      const Vector<String>& properties,
                                      ContactsSelectOptions* options,
                                      ExceptionState& exception_state) {
  LocalFrame* frame = script_state->ContextIsValid()
                          ? LocalDOMWindow::From(script_state)->GetFrame()
                          : nullptr;

  if (!frame || !frame->IsMainFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The contacts API can only be used in the top frame");
    return ScriptPromise();
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowSecurityError(
        "A user gesture is required to call this method");
    return ScriptPromise();
  }

  if (properties.IsEmpty()) {
    exception_state.ThrowTypeError("At least one property must be provided");
    return ScriptPromise();
  }

  if (contact_picker_in_use_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Contacts Picker is already in use.");
    return ScriptPromise();
  }

  bool include_names = false;
  bool include_emails = false;
  bool include_tel = false;
  bool include_addresses = false;
  bool include_icons = false;

  for (const String& property : properties) {
    if (!base::Contains(GetProperties(script_state), property)) {
      exception_state.ThrowTypeError(
          "The provided value '" + property +
          "' is not a valid enum value of type ContactProperty");
      return ScriptPromise();
    }

    if (property == kName)
      include_names = true;
    else if (property == kEmail)
      include_emails = true;
    else if (property == kTel)
      include_tel = true;
    else if (property == kAddress)
      include_addresses = true;
    else if (property == kIcon)
      include_icons = true;
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  contact_picker_in_use_ = true;
  GetContactsManager(script_state)
      ->Select(options->multiple(), include_names, include_emails, include_tel,
               include_addresses, include_icons,
               WTF::Bind(&ContactsManager::OnContactsSelected,
                         WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void ContactsManager::OnContactsSelected(
    ScriptPromiseResolver* resolver,
    base::Optional<Vector<mojom::blink::ContactInfoPtr>> contacts) {
  ScriptState* script_state = resolver->GetScriptState();

  if (!script_state->ContextIsValid()) {
    // This can happen if the page is programmatically redirected while
    // contacts are still being chosen.
    return;
  }

  ScriptState::Scope scope(script_state);

  contact_picker_in_use_ = false;

  if (!contacts.has_value()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Unable to open a contact selector"));
    return;
  }

  HeapVector<Member<ContactInfo>> contacts_list;
  for (const auto& contact : *contacts)
    contacts_list.push_back(contact.To<blink::ContactInfo*>());

  resolver->Resolve(contacts_list);
}

ScriptPromise ContactsManager::getProperties(ScriptState* script_state) {
  return ScriptPromise::Cast(script_state,
                             ToV8(GetProperties(script_state), script_state));
}

void ContactsManager::Trace(Visitor* visitor) {
  visitor->Trace(contacts_manager_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
