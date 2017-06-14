// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentInstruments.h"

#include <utility>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMException.h"
#include "modules/payments/PaymentInstrument.h"
#include "modules/payments/PaymentManager.h"
#include "platform/wtf/Vector.h"

namespace blink {
namespace {

static const char kPaymentManagerUnavailable[] = "Payment manager unavailable";

bool rejectError(ScriptPromiseResolver* resolver,
                 payments::mojom::blink::PaymentHandlerStatus status) {
  switch (status) {
    case payments::mojom::blink::PaymentHandlerStatus::SUCCESS:
      return false;
    case payments::mojom::blink::PaymentHandlerStatus::NOT_IMPLEMENTED:
      resolver->Reject(
          DOMException::Create(kNotSupportedError, "Not implemented yet"));
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::NOT_FOUND:
      resolver->Reject(DOMException::Create(kNotFoundError,
                                            "There is no stored instrument"));
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::NO_ACTIVE_WORKER:
      resolver->Reject(
          DOMException::Create(kInvalidStateError, "No active service worker"));
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::STORAGE_OPERATION_FAILED:
      resolver->Reject(DOMException::Create(kInvalidStateError,
                                            "Storage operation is failed"));
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::
        FETCH_INSTRUMENT_ICON_FAILED:
      resolver->Reject(DOMException::Create(
          kNotFoundError, "Fetch or decode instrument icon failed"));
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace

PaymentInstruments::PaymentInstruments(
    const payments::mojom::blink::PaymentManagerPtr& manager)
    : manager_(manager) {}

ScriptPromise PaymentInstruments::deleteInstrument(
    ScriptState* script_state,
    const String& instrument_key) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->DeletePaymentInstrument(
      instrument_key, ConvertToBaseCallback(WTF::Bind(
                          &PaymentInstruments::onDeletePaymentInstrument,
                          WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

ScriptPromise PaymentInstruments::get(ScriptState* script_state,
                                      const String& instrument_key) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->GetPaymentInstrument(
      instrument_key, ConvertToBaseCallback(WTF::Bind(
                          &PaymentInstruments::onGetPaymentInstrument,
                          WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

ScriptPromise PaymentInstruments::keys(ScriptState* script_state) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->KeysOfPaymentInstruments(ConvertToBaseCallback(
      WTF::Bind(&PaymentInstruments::onKeysOfPaymentInstruments,
                WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

ScriptPromise PaymentInstruments::has(ScriptState* script_state,
                                      const String& instrument_key) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->HasPaymentInstrument(
      instrument_key, ConvertToBaseCallback(WTF::Bind(
                          &PaymentInstruments::onHasPaymentInstrument,
                          WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

ScriptPromise PaymentInstruments::set(ScriptState* script_state,
                                      const String& instrument_key,
                                      const PaymentInstrument& details,
                                      ExceptionState& exception_state) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  payments::mojom::blink::PaymentInstrumentPtr instrument =
      payments::mojom::blink::PaymentInstrument::New();
  instrument->name = details.hasName() ? details.name() : WTF::g_empty_string;
  if (details.hasIcons()) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    for (const ImageObject image_object : details.icons()) {
      KURL parsed_url = context->CompleteURL(image_object.src());
      if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHTTPFamily()) {
        resolver->Reject(V8ThrowException::CreateTypeError(
            script_state->GetIsolate(),
            "'" + image_object.src() + "' is not a valid URL."));
        return promise;
      }

      instrument->icons.push_back(payments::mojom::blink::ImageObject::New());
      instrument->icons.back()->src = parsed_url;
    }
  }

  if (details.hasEnabledMethods()) {
    instrument->enabled_methods = details.enabledMethods();
  }

  if (details.hasCapabilities()) {
    v8::Local<v8::String> value;
    if (!v8::JSON::Stringify(script_state->GetContext(),
                             details.capabilities().V8Value().As<v8::Object>())
             .ToLocal(&value)) {
      exception_state.ThrowTypeError(
          "Capabilities should be a JSON-serializable object");
      return exception_state.Reject(script_state);
    }
    instrument->stringified_capabilities = ToCoreString(value);
  } else {
    instrument->stringified_capabilities = WTF::g_empty_string;
  }

  manager_->SetPaymentInstrument(
      instrument_key, std::move(instrument),
      ConvertToBaseCallback(
          WTF::Bind(&PaymentInstruments::onSetPaymentInstrument,
                    WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

ScriptPromise PaymentInstruments::clear(ScriptState* script_state) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->ClearPaymentInstruments(ConvertToBaseCallback(
      WTF::Bind(&PaymentInstruments::onClearPaymentInstruments,
                WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

DEFINE_TRACE(PaymentInstruments) {}

void PaymentInstruments::onDeletePaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  resolver->Resolve(status ==
                    payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
}

void PaymentInstruments::onGetPaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentInstrumentPtr stored_instrument,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  PaymentInstrument instrument;
  instrument.setName(stored_instrument->name);

  HeapVector<ImageObject> icons;
  for (const auto& icon : stored_instrument->icons) {
    ImageObject image_object;
    image_object.setSrc(icon->src.GetString());
    icons.emplace_back(image_object);
  }
  instrument.setIcons(icons);

  Vector<String> enabled_methods;
  for (const auto& method : stored_instrument->enabled_methods) {
    enabled_methods.push_back(method);
  }

  instrument.setEnabledMethods(enabled_methods);
  if (!stored_instrument->stringified_capabilities.IsEmpty()) {
    ScriptState::Scope scope(resolver->GetScriptState());
    ExceptionState exception_state(resolver->GetScriptState()->GetIsolate(),
                                   ExceptionState::kGetterContext,
                                   "PaymentInstruments", "get");
    instrument.setCapabilities(
        ScriptValue(resolver->GetScriptState(),
                    FromJSONString(resolver->GetScriptState()->GetIsolate(),
                                   stored_instrument->stringified_capabilities,
                                   exception_state)));
    if (exception_state.HadException()) {
      exception_state.Reject(resolver);
      return;
    }
  }
  resolver->Resolve(instrument);
}

void PaymentInstruments::onKeysOfPaymentInstruments(
    ScriptPromiseResolver* resolver,
    const Vector<String>& keys,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve(keys);
}

void PaymentInstruments::onHasPaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  resolver->Resolve(status ==
                    payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
}

void PaymentInstruments::onSetPaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve();
}

void PaymentInstruments::onClearPaymentInstruments(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve();
}

}  // namespace blink
