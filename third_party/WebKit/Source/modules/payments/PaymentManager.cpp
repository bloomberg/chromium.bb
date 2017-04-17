// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentManager.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "modules/payments/PaymentAppManifest.h"
#include "modules/payments/PaymentAppOption.h"
#include "modules/payments/PaymentInstruments.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace mojo {

using payments::mojom::blink::PaymentAppManifest;
using payments::mojom::blink::PaymentAppManifestPtr;
using payments::mojom::blink::PaymentAppOption;
using payments::mojom::blink::PaymentAppOptionPtr;

template <>
struct TypeConverter<PaymentAppOptionPtr, blink::PaymentAppOption> {
  static PaymentAppOptionPtr Convert(const blink::PaymentAppOption& input) {
    PaymentAppOptionPtr output = PaymentAppOption::New();
    output->name = input.hasName() ? input.name() : WTF::g_empty_string;
    output->icon = input.hasIcon() ? input.icon() : WTF::String();
    output->id = input.hasId() ? input.id() : WTF::g_empty_string;
    if (input.hasEnabledMethods()) {
      output->enabled_methods =
          WTF::Vector<WTF::String>(input.enabledMethods());
    }
    return output;
  }
};

template <>
struct TypeConverter<PaymentAppManifestPtr, blink::PaymentAppManifest> {
  static PaymentAppManifestPtr Convert(const blink::PaymentAppManifest& input) {
    PaymentAppManifestPtr output = PaymentAppManifest::New();
    output->name = input.hasName() ? input.name() : WTF::g_empty_string;
    output->icon = input.hasIcon() ? input.icon() : WTF::String();
    if (input.hasOptions()) {
      for (size_t i = 0; i < input.options().size(); ++i) {
        output->options.push_back(PaymentAppOption::From(input.options()[i]));
      }
    }
    return output;
  }
};

template <>
struct TypeConverter<blink::PaymentAppManifest, PaymentAppManifestPtr> {
  static blink::PaymentAppManifest Convert(const PaymentAppManifestPtr& input) {
    blink::PaymentAppManifest output;
    output.setName(input->name);
    output.setIcon(input->icon);
    blink::HeapVector<blink::PaymentAppOption> options;
    for (const auto& option : input->options) {
      options.push_back(mojo::ConvertTo<blink::PaymentAppOption>(option));
    }
    output.setOptions(options);
    return output;
  }
};

template <>
struct TypeConverter<blink::PaymentAppOption, PaymentAppOptionPtr> {
  static blink::PaymentAppOption Convert(const PaymentAppOptionPtr& input) {
    blink::PaymentAppOption output;
    output.setName(input->name);
    output.setIcon(input->icon);
    output.setId(input->id);
    Vector<WTF::String> enabledMethods;
    for (const auto& method : input->enabled_methods) {
      enabledMethods.push_back(method);
    }
    output.setEnabledMethods(enabledMethods);
    return output;
  }
};

}  // namespace mojo

namespace blink {

PaymentManager* PaymentManager::Create(
    ServiceWorkerRegistration* registration) {
  return new PaymentManager(registration);
}

ScriptPromise PaymentManager::setManifest(ScriptState* script_state,
                                          const PaymentAppManifest& manifest) {
  if (!manager_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "Payment app manager unavailable."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->SetManifest(
      payments::mojom::blink::PaymentAppManifest::From(manifest),
      ConvertToBaseCallback(WTF::Bind(&PaymentManager::OnSetManifest,
                                      WrapPersistent(this),
                                      WrapPersistent(resolver))));

  return promise;
}

ScriptPromise PaymentManager::getManifest(ScriptState* script_state) {
  if (!manager_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "Payment app manager unavailable."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->GetManifest(ConvertToBaseCallback(
      WTF::Bind(&PaymentManager::OnGetManifest, WrapPersistent(this),
                WrapPersistent(resolver))));

  return promise;
}

PaymentInstruments* PaymentManager::instruments() {
  if (!instruments_)
    instruments_ = new PaymentInstruments(manager_);
  return instruments_;
}

DEFINE_TRACE(PaymentManager) {
  visitor->Trace(registration_);
  visitor->Trace(instruments_);
}

PaymentManager::PaymentManager(ServiceWorkerRegistration* registration)
    : registration_(registration), instruments_(nullptr) {
  DCHECK(registration);
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&manager_));

  manager_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &PaymentManager::OnServiceConnectionError, WrapWeakPersistent(this))));

  manager_->Init(registration_->scope());
}

void PaymentManager::OnSetManifest(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentAppManifestError error) {
  DCHECK(resolver);
  switch (error) {
    case payments::mojom::blink::PaymentAppManifestError::NONE:
      resolver->Resolve();
      break;
    case payments::mojom::blink::PaymentAppManifestError::NOT_IMPLEMENTED:
      resolver->Reject(
          DOMException::Create(kNotSupportedError, "Not implemented yet."));
      break;
    case payments::mojom::blink::PaymentAppManifestError::NO_ACTIVE_WORKER:
      resolver->Reject(DOMException::Create(kInvalidStateError,
                                            "No active service worker."));
      break;
    case payments::mojom::blink::PaymentAppManifestError::
        MANIFEST_STORAGE_OPERATION_FAILED:
      resolver->Reject(DOMException::Create(
          kInvalidStateError, "Storing manifest data is failed."));
      break;
  }
}

void PaymentManager::OnGetManifest(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentAppManifestPtr manifest,
    payments::mojom::blink::PaymentAppManifestError error) {
  DCHECK(resolver);
  switch (error) {
    case payments::mojom::blink::PaymentAppManifestError::NONE:
      resolver->Resolve(
          mojo::ConvertTo<PaymentAppManifest>(std::move(manifest)));
      break;
    case payments::mojom::blink::PaymentAppManifestError::NOT_IMPLEMENTED:
      resolver->Reject(
          DOMException::Create(kNotSupportedError, "Not implemented yet."));
      break;
    case payments::mojom::blink::PaymentAppManifestError::NO_ACTIVE_WORKER:
    case payments::mojom::blink::PaymentAppManifestError::
        MANIFEST_STORAGE_OPERATION_FAILED:
      resolver->Reject(DOMException::Create(
          kAbortError,
          "No payment app manifest associated with the service worker."));
      break;
  }
}

void PaymentManager::OnServiceConnectionError() {
  manager_.reset();
}

}  // namespace blink
