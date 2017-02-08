// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAppManager.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "modules/payments/PaymentAppManifest.h"
#include "modules/payments/PaymentAppOption.h"
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
    output->name = input.hasName() ? input.name() : WTF::emptyString;
    output->icon = input.hasIcon() ? input.icon() : WTF::String();
    output->id = input.hasId() ? input.id() : WTF::emptyString;
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
    output->name = input.hasName() ? input.name() : WTF::emptyString;
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

PaymentAppManager* PaymentAppManager::create(
    ServiceWorkerRegistration* registration) {
  return new PaymentAppManager(registration);
}

ScriptPromise PaymentAppManager::setManifest(
    ScriptState* scriptState,
    const PaymentAppManifest& manifest) {
  if (!m_manager) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "Payment app manager unavailable."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  m_manager->SetManifest(
      payments::mojom::blink::PaymentAppManifest::From(manifest),
      convertToBaseCallback(WTF::bind(&PaymentAppManager::onSetManifest,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver))));

  return promise;
}

ScriptPromise PaymentAppManager::getManifest(ScriptState* scriptState) {
  if (!m_manager) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "Payment app manager unavailable."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  m_manager->GetManifest(convertToBaseCallback(
      WTF::bind(&PaymentAppManager::onGetManifest, wrapPersistent(this),
                wrapPersistent(resolver))));

  return promise;
}

DEFINE_TRACE(PaymentAppManager) {
  visitor->trace(m_registration);
}

PaymentAppManager::PaymentAppManager(ServiceWorkerRegistration* registration)
    : m_registration(registration) {
  DCHECK(registration);
  Platform::current()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&m_manager));

  m_manager.set_connection_error_handler(convertToBaseCallback(WTF::bind(
      &PaymentAppManager::onServiceConnectionError, wrapWeakPersistent(this))));

  m_manager->Init(m_registration->scope());
}

void PaymentAppManager::onSetManifest(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentAppManifestError error) {
  DCHECK(resolver);
  switch (error) {
    case payments::mojom::blink::PaymentAppManifestError::NONE:
      resolver->resolve();
      break;
    case payments::mojom::blink::PaymentAppManifestError::NOT_IMPLEMENTED:
      resolver->reject(
          DOMException::create(NotSupportedError, "Not implemented yet."));
      break;
    case payments::mojom::blink::PaymentAppManifestError::NO_ACTIVE_WORKER:
      resolver->reject(
          DOMException::create(InvalidStateError, "No active service worker."));
      break;
    case payments::mojom::blink::PaymentAppManifestError::
        MANIFEST_STORAGE_OPERATION_FAILED:
      resolver->reject(DOMException::create(
          InvalidStateError, "Storing manifest data is failed."));
      break;
  }
}

void PaymentAppManager::onGetManifest(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentAppManifestPtr manifest,
    payments::mojom::blink::PaymentAppManifestError error) {
  DCHECK(resolver);
  switch (error) {
    case payments::mojom::blink::PaymentAppManifestError::NONE:
      resolver->resolve(
          mojo::ConvertTo<PaymentAppManifest>(std::move(manifest)));
      break;
    case payments::mojom::blink::PaymentAppManifestError::NOT_IMPLEMENTED:
      resolver->reject(
          DOMException::create(NotSupportedError, "Not implemented yet."));
      break;
    case payments::mojom::blink::PaymentAppManifestError::NO_ACTIVE_WORKER:
    case payments::mojom::blink::PaymentAppManifestError::
        MANIFEST_STORAGE_OPERATION_FAILED:
      resolver->reject(DOMException::create(
          AbortError,
          "No payment app manifest associated with the service worker."));
      break;
  }
}

void PaymentAppManager::onServiceConnectionError() {
  m_manager.reset();
}

}  // namespace blink
