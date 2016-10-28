// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/JSONValuesForV8.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/modules/v8/V8PaymentDetails.h"
#include "core/EventTypeNames.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/EventQueue.h"
#include "core/frame/FrameOwner.h"
#include "core/html/HTMLIFrameElement.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/payments/HTMLIFrameElementPayments.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/PaymentRequestUpdateEvent.h"
#include "modules/payments/PaymentResponse.h"
#include "modules/payments/PaymentShippingOption.h"
#include "modules/payments/PaymentsValidators.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/wtf_array.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/HashSet.h"
#include <utility>

namespace mojo {

using blink::mojom::blink::PaymentCurrencyAmount;
using blink::mojom::blink::PaymentCurrencyAmountPtr;
using blink::mojom::blink::PaymentDetails;
using blink::mojom::blink::PaymentDetailsModifier;
using blink::mojom::blink::PaymentDetailsModifierPtr;
using blink::mojom::blink::PaymentDetailsPtr;
using blink::mojom::blink::PaymentErrorReason;
using blink::mojom::blink::PaymentItem;
using blink::mojom::blink::PaymentItemPtr;
using blink::mojom::blink::PaymentMethodData;
using blink::mojom::blink::PaymentMethodDataPtr;
using blink::mojom::blink::PaymentOptions;
using blink::mojom::blink::PaymentOptionsPtr;
using blink::mojom::blink::PaymentShippingOption;
using blink::mojom::blink::PaymentShippingOptionPtr;
using blink::mojom::blink::PaymentShippingType;

template <>
struct TypeConverter<PaymentCurrencyAmountPtr, blink::PaymentCurrencyAmount> {
  static PaymentCurrencyAmountPtr Convert(
      const blink::PaymentCurrencyAmount& input) {
    PaymentCurrencyAmountPtr output = PaymentCurrencyAmount::New();
    output->currency = input.currency();
    output->value = input.value();
    return output;
  }
};

template <>
struct TypeConverter<PaymentItemPtr, blink::PaymentItem> {
  static PaymentItemPtr Convert(const blink::PaymentItem& input) {
    PaymentItemPtr output = PaymentItem::New();
    output->label = input.label();
    output->amount = PaymentCurrencyAmount::From(input.amount());
    output->pending = input.pending();
    return output;
  }
};

template <>
struct TypeConverter<PaymentShippingOptionPtr, blink::PaymentShippingOption> {
  static PaymentShippingOptionPtr Convert(
      const blink::PaymentShippingOption& input) {
    PaymentShippingOptionPtr output = PaymentShippingOption::New();
    output->id = input.id();
    output->label = input.label();
    output->amount = PaymentCurrencyAmount::From(input.amount());
    output->selected = input.hasSelected() && input.selected();
    return output;
  }
};

template <>
struct TypeConverter<PaymentDetailsModifierPtr, blink::PaymentDetailsModifier> {
  static PaymentDetailsModifierPtr Convert(
      const blink::PaymentDetailsModifier& input) {
    PaymentDetailsModifierPtr output = PaymentDetailsModifier::New();
    output->supported_methods =
        WTF::Vector<WTF::String>(input.supportedMethods());

    if (input.hasTotal())
      output->total = PaymentItem::From(input.total());

    if (input.hasAdditionalDisplayItems()) {
      for (size_t i = 0; i < input.additionalDisplayItems().size(); ++i) {
        output->additional_display_items.append(
            PaymentItem::From(input.additionalDisplayItems()[i]));
      }
    }
    return output;
  }
};

template <>
struct TypeConverter<PaymentDetailsPtr, blink::PaymentDetails> {
  static PaymentDetailsPtr Convert(const blink::PaymentDetails& input) {
    PaymentDetailsPtr output = PaymentDetails::New();
    output->total = PaymentItem::From(input.total());

    if (input.hasDisplayItems()) {
      for (size_t i = 0; i < input.displayItems().size(); ++i) {
        output->display_items.append(
            PaymentItem::From(input.displayItems()[i]));
      }
    }

    if (input.hasShippingOptions()) {
      for (size_t i = 0; i < input.shippingOptions().size(); ++i) {
        output->shipping_options.append(
            PaymentShippingOption::From(input.shippingOptions()[i]));
      }
    }

    if (input.hasModifiers()) {
      for (size_t i = 0; i < input.modifiers().size(); ++i) {
        output->modifiers.append(
            PaymentDetailsModifier::From(input.modifiers()[i]));
      }
    }

    if (input.hasError())
      output->error = input.error();
    else
      output->error = WTF::emptyString();

    return output;
  }
};

template <>
struct TypeConverter<PaymentOptionsPtr, blink::PaymentOptions> {
  static PaymentOptionsPtr Convert(const blink::PaymentOptions& input) {
    PaymentOptionsPtr output = PaymentOptions::New();
    output->request_payer_name = input.requestPayerName();
    output->request_payer_email = input.requestPayerEmail();
    output->request_payer_phone = input.requestPayerPhone();
    output->request_shipping = input.requestShipping();

    if (input.shippingType() == "delivery")
      output->shipping_type = PaymentShippingType::DELIVERY;
    else if (input.shippingType() == "pickup")
      output->shipping_type = PaymentShippingType::PICKUP;
    else
      output->shipping_type = PaymentShippingType::SHIPPING;

    return output;
  }
};

template <>
struct TypeConverter<WTFArray<PaymentMethodDataPtr>,
                     WTF::Vector<blink::PaymentRequest::MethodData>> {
  static WTFArray<PaymentMethodDataPtr> Convert(
      const WTF::Vector<blink::PaymentRequest::MethodData>& input) {
    WTFArray<PaymentMethodDataPtr> output(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
      output[i] = PaymentMethodData::New();
      output[i]->supported_methods =
          WTF::Vector<WTF::String>(input[i].supportedMethods);
      output[i]->stringified_data = input[i].stringifiedData;
    }
    return output;
  }
};

}  // namespace mojo

namespace blink {
namespace {

// If the website does not call complete() 60 seconds after show() has been
// resolved, then behave as if the website called complete("fail").
static const int completeTimeoutSeconds = 60;

// Validates ShippingOption or PaymentItem, which happen to have identical
// fields, except for "id", which is present only in ShippingOption.
template <typename T>
void validateShippingOptionOrPaymentItem(const T& item,
                                         ExceptionState& exceptionState) {
  if (!item.hasLabel() || item.label().isEmpty()) {
    exceptionState.throwTypeError("Item label required");
    return;
  }

  if (!item.hasAmount()) {
    exceptionState.throwTypeError("Currency amount required");
    return;
  }

  if (!item.amount().hasCurrency()) {
    exceptionState.throwTypeError("Currency code required");
    return;
  }

  if (!item.amount().hasValue()) {
    exceptionState.throwTypeError("Currency value required");
    return;
  }

  String errorMessage;
  if (!PaymentsValidators::isValidCurrencyCodeFormat(
          item.amount().currency(), item.amount().currencySystem(),
          &errorMessage)) {
    exceptionState.throwTypeError(errorMessage);
    return;
  }

  if (!PaymentsValidators::isValidAmountFormat(item.amount().value(),
                                               &errorMessage)) {
    exceptionState.throwTypeError(errorMessage);
    return;
  }
}

void validateDisplayItems(const HeapVector<PaymentItem>& items,
                          ExceptionState& exceptionState) {
  for (const auto& item : items) {
    validateShippingOptionOrPaymentItem(item, exceptionState);
    if (exceptionState.hadException())
      return;
  }
}

// Returns false if |options| should be ignored, even if an exception was not
// thrown. TODO(rouslan): Clear shipping options instead of ignoring them when
// http://crbug.com/601193 is fixed.
bool validateShippingOptions(const HeapVector<PaymentShippingOption>& options,
                             ExceptionState& exceptionState) {
  HashSet<String> uniqueIds;
  for (const auto& option : options) {
    if (!option.hasId() || option.id().isEmpty()) {
      exceptionState.throwTypeError("ShippingOption id required");
      return false;
    }

    if (uniqueIds.contains(option.id()))
      return false;

    uniqueIds.add(option.id());

    validateShippingOptionOrPaymentItem(option, exceptionState);
    if (exceptionState.hadException())
      return false;
  }

  return true;
}

void validatePaymentDetailsModifiers(
    const HeapVector<PaymentDetailsModifier>& modifiers,
    ExceptionState& exceptionState) {
  if (modifiers.isEmpty()) {
    exceptionState.throwTypeError(
        "Must specify at least one payment details modifier");
    return;
  }

  for (const auto& modifier : modifiers) {
    if (modifier.supportedMethods().isEmpty()) {
      exceptionState.throwTypeError(
          "Must specify at least one payment method identifier");
      return;
    }

    if (modifier.hasTotal()) {
      validateShippingOptionOrPaymentItem(modifier.total(), exceptionState);
      if (exceptionState.hadException())
        return;

      if (modifier.total().amount().value()[0] == '-') {
        exceptionState.throwTypeError(
            "Total amount value should be non-negative");
        return;
      }
    }

    if (modifier.hasAdditionalDisplayItems()) {
      validateDisplayItems(modifier.additionalDisplayItems(), exceptionState);
      if (exceptionState.hadException())
        return;
    }
  }
}

// Returns false if the shipping options should be ignored without throwing an
// exception.
bool validatePaymentDetails(const PaymentDetails& details,
                            ExceptionState& exceptionState) {
  bool keepShippingOptions = true;
  if (!details.hasTotal()) {
    exceptionState.throwTypeError("Must specify total");
    return keepShippingOptions;
  }

  validateShippingOptionOrPaymentItem(details.total(), exceptionState);
  if (exceptionState.hadException())
    return keepShippingOptions;

  if (details.total().amount().value()[0] == '-') {
    exceptionState.throwTypeError("Total amount value should be non-negative");
    return keepShippingOptions;
  }

  if (details.hasDisplayItems()) {
    validateDisplayItems(details.displayItems(), exceptionState);
    if (exceptionState.hadException())
      return keepShippingOptions;
  }

  if (details.hasShippingOptions()) {
    keepShippingOptions =
        validateShippingOptions(details.shippingOptions(), exceptionState);

    if (exceptionState.hadException())
      return keepShippingOptions;
  }

  if (details.hasModifiers()) {
    validatePaymentDetailsModifiers(details.modifiers(), exceptionState);
    if (exceptionState.hadException())
      return keepShippingOptions;
  }

  String errorMessage;
  if (!PaymentsValidators::isValidErrorMsgFormat(details.error(),
                                                 &errorMessage)) {
    exceptionState.throwTypeError(errorMessage);
  }

  return keepShippingOptions;
}

void validateAndConvertPaymentMethodData(
    const HeapVector<PaymentMethodData>& paymentMethodData,
    Vector<PaymentRequest::MethodData>* methodData,
    ExceptionState& exceptionState) {
  if (paymentMethodData.isEmpty()) {
    exceptionState.throwTypeError(
        "Must specify at least one payment method identifier");
    return;
  }

  for (const auto& pmd : paymentMethodData) {
    if (pmd.supportedMethods().isEmpty()) {
      exceptionState.throwTypeError(
          "Must specify at least one payment method identifier");
      return;
    }

    String stringifiedData = "";
    if (pmd.hasData() && !pmd.data().isEmpty()) {
      std::unique_ptr<JSONValue> value =
          toJSONValue(pmd.data().context(), pmd.data().v8Value());
      if (!value) {
        exceptionState.throwTypeError(
            "Unable to parse payment method specific data");
        return;
      }
      if (!value->isNull()) {
        if (value->getType() != JSONValue::TypeObject) {
          exceptionState.throwTypeError(
              "Data should be a JSON-serializable object");
          return;
        }
        stringifiedData = JSONObject::cast(value.get())->toJSONString();
      }
    }
    methodData->append(
        PaymentRequest::MethodData(pmd.supportedMethods(), stringifiedData));
  }
}

String getSelectedShippingOption(const PaymentDetails& details) {
  String result;
  if (!details.hasShippingOptions())
    return result;

  for (int i = details.shippingOptions().size() - 1; i >= 0; --i) {
    if (details.shippingOptions()[i].hasSelected() &&
        details.shippingOptions()[i].selected()) {
      return details.shippingOptions()[i].id();
    }
  }

  return result;
}

String getValidShippingType(const String& shippingType) {
  static const char* const validValues[] = {
      "shipping", "delivery", "pickup",
  };
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(validValues); i++) {
    if (shippingType == validValues[i])
      return shippingType;
  }
  return validValues[0];
}

mojom::blink::PaymentDetailsPtr maybeKeepShippingOptions(
    mojom::blink::PaymentDetailsPtr details,
    bool keep) {
  if (!keep)
    details->shipping_options.resize(0);

  return details;
}

bool allowedToUsePaymentRequest(const Frame* frame) {
  // To determine whether a Document object |document| is allowed to use the
  // feature indicated by attribute name |allowpaymentrequest|, run these steps:

  // 1. If |document| has no browsing context, then return false.
  if (!frame)
    return false;

  // 2. If |document|'s browsing context is a top-level browsing context, then
  // return true.
  if (frame->isMainFrame())
    return true;

  // 3. If |document|'s browsing context has a browsing context container that
  // is an iframe element with an |allowpaymentrequest| attribute specified, and
  // whose node document is allowed to use the feature indicated by
  // |allowpaymentrequest|, then return true.
  HTMLFrameOwnerElement* ownerElement = toHTMLFrameOwnerElement(frame->owner());
  if (ownerElement && isHTMLIFrameElement(ownerElement)) {
    HTMLIFrameElement* iframe = toHTMLIFrameElement(ownerElement);
    if (HTMLIFrameElementPayments::from(*iframe).allowPaymentRequest(*iframe))
      return allowedToUsePaymentRequest(frame->tree().parent());
  }

  // 4. Return false.
  return false;
}

WTF::Vector<mojom::blink::PaymentMethodDataPtr> ConvertPaymentMethodData(
    const Vector<PaymentRequest::MethodData>& blinkMethods) {
  WTF::Vector<mojom::blink::PaymentMethodDataPtr> mojoMethods(
      blinkMethods.size());
  for (size_t i = 0; i < blinkMethods.size(); ++i) {
    mojoMethods[i] = mojom::blink::PaymentMethodData::New();
    mojoMethods[i]->supported_methods =
        WTF::Vector<WTF::String>(blinkMethods[i].supportedMethods);
    mojoMethods[i]->stringified_data = blinkMethods[i].stringifiedData;
  }
  return mojoMethods;
}

}  // namespace

PaymentRequest* PaymentRequest::create(
    ScriptState* scriptState,
    const HeapVector<PaymentMethodData>& methodData,
    const PaymentDetails& details,
    ExceptionState& exceptionState) {
  return new PaymentRequest(scriptState, methodData, details, PaymentOptions(),
                            exceptionState);
}

PaymentRequest* PaymentRequest::create(
    ScriptState* scriptState,
    const HeapVector<PaymentMethodData>& methodData,
    const PaymentDetails& details,
    const PaymentOptions& options,
    ExceptionState& exceptionState) {
  return new PaymentRequest(scriptState, methodData, details, options,
                            exceptionState);
}

PaymentRequest::~PaymentRequest() {}

ScriptPromise PaymentRequest::show(ScriptState* scriptState) {
  if (!m_paymentProvider.is_bound() || m_showResolver)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError, "Already called show() once"));

  if (!scriptState->domWindow() || !scriptState->domWindow()->frame())
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "Cannot show the payment request"));

  m_paymentProvider->Show();

  m_showResolver = ScriptPromiseResolver::create(scriptState);
  return m_showResolver->promise();
}

ScriptPromise PaymentRequest::abort(ScriptState* scriptState) {
  if (m_abortResolver)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError,
                             "Cannot abort() again until the previous abort() "
                             "has resolved or rejected"));

  if (!m_showResolver)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError,
                             "Never called show(), so nothing to abort"));

  m_abortResolver = ScriptPromiseResolver::create(scriptState);
  m_paymentProvider->Abort();
  return m_abortResolver->promise();
}

bool PaymentRequest::hasPendingActivity() const {
  return m_showResolver || m_completeResolver;
}

const AtomicString& PaymentRequest::interfaceName() const {
  return EventTargetNames::PaymentRequest;
}

ExecutionContext* PaymentRequest::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

ScriptPromise PaymentRequest::complete(ScriptState* scriptState,
                                       PaymentComplete result) {
  if (m_completeResolver)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "Already called complete() once"));

  if (!m_completeTimer.isActive())
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "Timed out after 60 seconds, complete() called too late"));

  // User has cancelled the transaction while the website was processing it.
  if (!m_paymentProvider)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError, "Request cancelled"));

  m_completeTimer.stop();

  // The payment provider should respond in PaymentRequest::OnComplete().
  m_paymentProvider->Complete(mojom::blink::PaymentComplete(result));

  m_completeResolver = ScriptPromiseResolver::create(scriptState);
  return m_completeResolver->promise();
}

void PaymentRequest::onUpdatePaymentDetails(
    const ScriptValue& detailsScriptValue) {
  if (!m_showResolver || !m_paymentProvider)
    return;

  PaymentDetails details;
  TrackExceptionState exceptionState;
  V8PaymentDetails::toImpl(detailsScriptValue.isolate(),
                           detailsScriptValue.v8Value(), details,
                           exceptionState);
  if (exceptionState.hadException()) {
    m_showResolver->reject(
        DOMException::create(SyntaxError, exceptionState.message()));
    clearResolversAndCloseMojoConnection();
    return;
  }

  bool keepShippingOptions = validatePaymentDetails(details, exceptionState);
  if (exceptionState.hadException()) {
    m_showResolver->reject(
        DOMException::create(SyntaxError, exceptionState.message()));
    clearResolversAndCloseMojoConnection();
    return;
  }

  if (m_options.requestShipping()) {
    if (keepShippingOptions)
      m_shippingOption = getSelectedShippingOption(details);
    else
      m_shippingOption = String();
  }

  m_paymentProvider->UpdateWith(maybeKeepShippingOptions(
      mojom::blink::PaymentDetails::From(details), keepShippingOptions));
}

void PaymentRequest::onUpdatePaymentDetailsFailure(const String& error) {
  if (m_showResolver)
    m_showResolver->reject(DOMException::create(AbortError, error));
  if (m_completeResolver)
    m_completeResolver->reject(DOMException::create(AbortError, error));
  clearResolversAndCloseMojoConnection();
}

DEFINE_TRACE(PaymentRequest) {
  visitor->trace(m_options);
  visitor->trace(m_shippingAddress);
  visitor->trace(m_showResolver);
  visitor->trace(m_completeResolver);
  visitor->trace(m_abortResolver);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

void PaymentRequest::onCompleteTimeoutForTesting() {
  m_completeTimer.stop();
  onCompleteTimeout(0);
}

PaymentRequest::PaymentRequest(ScriptState* scriptState,
                               const HeapVector<PaymentMethodData>& methodData,
                               const PaymentDetails& details,
                               const PaymentOptions& options,
                               ExceptionState& exceptionState)
    : ContextLifecycleObserver(scriptState->getExecutionContext()),
      ActiveScriptWrappable(this),
      m_options(options),
      m_clientBinding(this),
      m_completeTimer(this, &PaymentRequest::onCompleteTimeout) {
  Vector<MethodData> validatedMethodData;
  validateAndConvertPaymentMethodData(methodData, &validatedMethodData,
                                      exceptionState);
  if (exceptionState.hadException())
    return;

  if (!scriptState->getExecutionContext()->isSecureContext()) {
    exceptionState.throwSecurityError("Must be in a secure context");
    return;
  }

  if (!allowedToUsePaymentRequest(scriptState->domWindow()->frame())) {
    exceptionState.throwSecurityError(
        "Must be in a top-level browsing context or an iframe needs to specify "
        "'allowpaymentrequest' explicitly");
    return;
  }

  bool keepShippingOptions = validatePaymentDetails(details, exceptionState);
  if (exceptionState.hadException())
    return;

  if (details.hasError() && !details.error().isEmpty()) {
    exceptionState.throwTypeError("Error value should be empty");
    return;
  }

  if (m_options.requestShipping()) {
    if (keepShippingOptions)
      m_shippingOption = getSelectedShippingOption(details);
    m_shippingType = getValidShippingType(m_options.shippingType());
  }

  scriptState->domWindow()->frame()->interfaceProvider()->getInterface(
      mojo::GetProxy(&m_paymentProvider));
  m_paymentProvider.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&PaymentRequest::OnError, wrapWeakPersistent(this),
                mojom::blink::PaymentErrorReason::UNKNOWN)));
  m_paymentProvider->Init(
      m_clientBinding.CreateInterfacePtrAndBind(),
      ConvertPaymentMethodData(validatedMethodData),
      maybeKeepShippingOptions(
          mojom::blink::PaymentDetails::From(details),
          keepShippingOptions && m_options.requestShipping()),
      mojom::blink::PaymentOptions::From(m_options));
}

void PaymentRequest::contextDestroyed() {
  clearResolversAndCloseMojoConnection();
}

void PaymentRequest::OnShippingAddressChange(
    mojom::blink::PaymentAddressPtr address) {
  DCHECK(m_showResolver);
  DCHECK(!m_completeResolver);

  String errorMessage;
  if (!PaymentsValidators::isValidShippingAddress(address, &errorMessage)) {
    m_showResolver->reject(DOMException::create(SyntaxError, errorMessage));
    clearResolversAndCloseMojoConnection();
    return;
  }

  m_shippingAddress = new PaymentAddress(std::move(address));
  PaymentRequestUpdateEvent* event =
      PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
  event->setTarget(this);
  event->setPaymentDetailsUpdater(this);
  bool success = getExecutionContext()->getEventQueue()->enqueueEvent(event);
  DCHECK(success);
  ALLOW_UNUSED_LOCAL(success);
}

void PaymentRequest::OnShippingOptionChange(const String& shippingOptionId) {
  DCHECK(m_showResolver);
  DCHECK(!m_completeResolver);
  m_shippingOption = shippingOptionId;
  PaymentRequestUpdateEvent* event =
      PaymentRequestUpdateEvent::create(EventTypeNames::shippingoptionchange);
  event->setTarget(this);
  event->setPaymentDetailsUpdater(this);
  bool success = getExecutionContext()->getEventQueue()->enqueueEvent(event);
  DCHECK(success);
  ALLOW_UNUSED_LOCAL(success);
}

void PaymentRequest::OnPaymentResponse(
    mojom::blink::PaymentResponsePtr response) {
  DCHECK(m_showResolver);
  DCHECK(!m_completeResolver);
  DCHECK(!m_completeTimer.isActive());

  if (m_options.requestShipping()) {
    if (!response->shipping_address || response->shipping_option.isEmpty()) {
      m_showResolver->reject(DOMException::create(SyntaxError));
      clearResolversAndCloseMojoConnection();
      return;
    }

    String errorMessage;
    if (!PaymentsValidators::isValidShippingAddress(response->shipping_address,
                                                    &errorMessage)) {
      m_showResolver->reject(DOMException::create(SyntaxError, errorMessage));
      clearResolversAndCloseMojoConnection();
      return;
    }

    m_shippingAddress = new PaymentAddress(response->shipping_address.Clone());
    m_shippingOption = response->shipping_option;
  } else {
    if (response->shipping_address || !response->shipping_option.isNull()) {
      m_showResolver->reject(DOMException::create(SyntaxError));
      clearResolversAndCloseMojoConnection();
      return;
    }
  }

  if ((m_options.requestPayerName() && response->payer_name.isEmpty()) ||
      (m_options.requestPayerEmail() && response->payer_email.isEmpty()) ||
      (m_options.requestPayerPhone() && response->payer_phone.isEmpty()) ||
      (!m_options.requestPayerName() && !response->payer_name.isNull()) ||
      (!m_options.requestPayerEmail() && !response->payer_email.isNull()) ||
      (!m_options.requestPayerPhone() && !response->payer_phone.isNull())) {
    m_showResolver->reject(DOMException::create(SyntaxError));
    clearResolversAndCloseMojoConnection();
    return;
  }

  m_completeTimer.startOneShot(completeTimeoutSeconds, BLINK_FROM_HERE);

  m_showResolver->resolve(new PaymentResponse(std::move(response), this));

  // Do not close the mojo connection here. The merchant website should call
  // PaymentResponse::complete(String), which will be forwarded over the mojo
  // connection to display a success or failure message to the user.
  m_showResolver.clear();
}

void PaymentRequest::OnError(mojo::PaymentErrorReason error) {
  bool isError = false;
  ExceptionCode ec = UnknownError;
  String message;

  switch (error) {
    case mojom::blink::PaymentErrorReason::USER_CANCEL:
      message = "Request cancelled";
      break;
    case mojom::blink::PaymentErrorReason::NOT_SUPPORTED:
      isError = true;
      ec = NotSupportedError;
      message = "The payment method is not supported";
      break;
    case mojom::blink::PaymentErrorReason::UNKNOWN:
      isError = true;
      ec = UnknownError;
      message = "Request failed";
      break;
  }

  DCHECK(!message.isEmpty());

  if (isError) {
    if (m_completeResolver)
      m_completeResolver->reject(DOMException::create(ec, message));

    if (m_showResolver)
      m_showResolver->reject(DOMException::create(ec, message));

    if (m_abortResolver)
      m_abortResolver->reject(DOMException::create(ec, message));
  } else {
    if (m_completeResolver)
      m_completeResolver->reject(message);

    if (m_showResolver)
      m_showResolver->reject(message);

    if (m_abortResolver)
      m_abortResolver->reject(message);
  }

  clearResolversAndCloseMojoConnection();
}

void PaymentRequest::OnComplete() {
  DCHECK(m_completeResolver);
  m_completeResolver->resolve();
  clearResolversAndCloseMojoConnection();
}

void PaymentRequest::OnAbort(bool abortedSuccessfully) {
  DCHECK(m_abortResolver);
  DCHECK(m_showResolver);

  if (!abortedSuccessfully) {
    m_abortResolver->reject(DOMException::create(InvalidStateError));
    m_abortResolver.clear();
    return;
  }

  m_showResolver->reject(DOMException::create(AbortError));
  m_abortResolver->resolve();
  clearResolversAndCloseMojoConnection();
}

void PaymentRequest::onCompleteTimeout(TimerBase*) {
  m_paymentProvider->Complete(mojom::blink::PaymentComplete(Fail));
  clearResolversAndCloseMojoConnection();
}

void PaymentRequest::clearResolversAndCloseMojoConnection() {
  m_completeTimer.stop();
  m_completeResolver.clear();
  m_showResolver.clear();
  m_abortResolver.clear();
  if (m_clientBinding.is_bound())
    m_clientBinding.Close();
  m_paymentProvider.reset();
}

}  // namespace blink
