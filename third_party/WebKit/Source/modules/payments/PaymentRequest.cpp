// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/modules/v8/V8AndroidPayMethodData.h"
#include "bindings/modules/v8/V8PaymentDetails.h"
#include "core/EventTypeNames.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/EventQueue.h"
#include "core/frame/FrameOwner.h"
#include "core/html/HTMLIFrameElement.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/payments/AndroidPayMethodData.h"
#include "modules/payments/AndroidPayTokenization.h"
#include "modules/payments/HTMLIFrameElementPayments.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/PaymentRequestUpdateEvent.h"
#include "modules/payments/PaymentResponse.h"
#include "modules/payments/PaymentShippingOption.h"
#include "modules/payments/PaymentsValidators.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/wtf_array.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/HashSet.h"
#include <stddef.h>
#include <utility>

using payments::mojom::blink::ActivePaymentQueryResult;
using payments::mojom::blink::PaymentAddressPtr;
using payments::mojom::blink::PaymentCurrencyAmount;
using payments::mojom::blink::PaymentCurrencyAmountPtr;
using payments::mojom::blink::PaymentDetailsModifierPtr;
using payments::mojom::blink::PaymentDetailsPtr;
using payments::mojom::blink::PaymentErrorReason;
using payments::mojom::blink::PaymentItemPtr;
using payments::mojom::blink::PaymentMethodDataPtr;
using payments::mojom::blink::PaymentOptionsPtr;
using payments::mojom::blink::PaymentResponsePtr;
using payments::mojom::blink::PaymentShippingOptionPtr;
using payments::mojom::blink::PaymentShippingType;

namespace mojo {

template <>
struct TypeConverter<PaymentCurrencyAmountPtr, blink::PaymentCurrencyAmount> {
  static PaymentCurrencyAmountPtr Convert(
      const blink::PaymentCurrencyAmount& input) {
    PaymentCurrencyAmountPtr output = PaymentCurrencyAmount::New();
    output->currency = input.currency();
    output->value = input.value();
    if (input.hasCurrencySystem())
      output->currencySystem = input.currencySystem();
    return output;
  }
};

template <>
struct TypeConverter<PaymentItemPtr, blink::PaymentItem> {
  static PaymentItemPtr Convert(const blink::PaymentItem& input) {
    PaymentItemPtr output = payments::mojom::blink::PaymentItem::New();
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
    PaymentShippingOptionPtr output =
        payments::mojom::blink::PaymentShippingOption::New();
    output->id = input.id();
    output->label = input.label();
    output->amount = PaymentCurrencyAmount::From(input.amount());
    output->selected = input.hasSelected() && input.selected();
    return output;
  }
};

template <>
struct TypeConverter<PaymentOptionsPtr, blink::PaymentOptions> {
  static PaymentOptionsPtr Convert(const blink::PaymentOptions& input) {
    PaymentOptionsPtr output = payments::mojom::blink::PaymentOptions::New();
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

}  // namespace mojo

namespace blink {
namespace {

using payments::mojom::blink::AndroidPayCardNetwork;
using payments::mojom::blink::AndroidPayTokenization;

// If the website does not call complete() 60 seconds after show() has been
// resolved, then behave as if the website called complete("fail").
static const int completeTimeoutSeconds = 60;

const struct {
  const AndroidPayCardNetwork code;
  const char* name;
} kAndroidPayNetwork[] = {{AndroidPayCardNetwork::AMEX, "AMEX"},
                          {AndroidPayCardNetwork::DISCOVER, "DISCOVER"},
                          {AndroidPayCardNetwork::MASTERCARD, "MASTERCARD"},
                          {AndroidPayCardNetwork::VISA, "VISA"}};

const struct {
  const AndroidPayTokenization code;
  const char* name;
} kAndroidPayTokenization[] = {
    {AndroidPayTokenization::GATEWAY_TOKEN, "GATEWAY_TOKEN"},
    {AndroidPayTokenization::NETWORK_TOKEN, "NETWORK_TOKEN"}};

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

void validateAndConvertDisplayItems(const HeapVector<PaymentItem>& input,
                                    Vector<PaymentItemPtr>& output,
                                    ExceptionState& exceptionState) {
  for (const PaymentItem& item : input) {
    validateShippingOptionOrPaymentItem(item, exceptionState);
    if (exceptionState.hadException())
      return;
    output.append(payments::mojom::blink::PaymentItem::From(item));
  }
}

// Validates and converts |input| shipping options into |output|. Throws an
// exception if the data is not valid, except for duplicate identifiers, which
// returns an empty |output| instead of throwing an exception. There's no need
// to clear |output| when an exception is thrown, because the caller takes care
// of deleting |output|.
void validateAndConvertShippingOptions(
    const HeapVector<PaymentShippingOption>& input,
    Vector<PaymentShippingOptionPtr>& output,
    ExceptionState& exceptionState) {
  HashSet<String> uniqueIds;
  for (const PaymentShippingOption& option : input) {
    if (!option.hasId() || option.id().isEmpty()) {
      exceptionState.throwTypeError("ShippingOption id required");
      return;
    }

    if (uniqueIds.contains(option.id())) {
      // Clear |output| instead of throwing an exception.
      output.clear();
      return;
    }

    uniqueIds.add(option.id());

    validateShippingOptionOrPaymentItem(option, exceptionState);
    if (exceptionState.hadException())
      return;

    output.append(payments::mojom::blink::PaymentShippingOption::From(option));
  }
}

void validateAndConvertTotal(const PaymentItem& input,
                             PaymentItemPtr& output,
                             ExceptionState& exceptionState) {
  validateShippingOptionOrPaymentItem(input, exceptionState);
  if (exceptionState.hadException())
    return;

  if (input.amount().value()[0] == '-') {
    exceptionState.throwTypeError("Total amount value should be non-negative");
    return;
  }

  output = payments::mojom::blink::PaymentItem::From(input);
}

void maybeSetAndroidPayMethodData(const ScriptValue& input,
                                  PaymentMethodDataPtr& output,
                                  ExceptionState& exceptionState) {
  AndroidPayMethodData androidPay;
  DummyExceptionStateForTesting dummyExceptionState;
  V8AndroidPayMethodData::toImpl(input.isolate(), input.v8Value(), androidPay,
                                 dummyExceptionState);
  if (dummyExceptionState.hadException())
    return;

  if (androidPay.hasEnvironment() && androidPay.environment() == "TEST")
    output->environment = payments::mojom::blink::AndroidPayEnvironment::TEST;

  output->merchant_name = androidPay.merchantName();
  output->merchant_id = androidPay.merchantId();

  if (androidPay.hasAllowedCardNetworks()) {
    for (const String& allowedCardNetwork : androidPay.allowedCardNetworks()) {
      for (size_t i = 0; i < arraysize(kAndroidPayNetwork); ++i) {
        if (allowedCardNetwork == kAndroidPayNetwork[i].name) {
          output->allowed_card_networks.append(kAndroidPayNetwork[i].code);
          break;
        }
      }
    }
  }

  if (androidPay.hasPaymentMethodTokenizationParameters()) {
    const blink::AndroidPayTokenization& tokenization =
        androidPay.paymentMethodTokenizationParameters();
    output->tokenization_type =
        payments::mojom::blink::AndroidPayTokenization::UNSPECIFIED;
    if (tokenization.hasTokenizationType()) {
      for (size_t i = 0; i < arraysize(kAndroidPayTokenization); ++i) {
        if (tokenization.tokenizationType() ==
            kAndroidPayTokenization[i].name) {
          output->tokenization_type = kAndroidPayTokenization[i].code;
          break;
        }
      }
    }

    if (tokenization.hasParameters()) {
      const Vector<String>& keys =
          tokenization.parameters().getPropertyNames(exceptionState);
      if (exceptionState.hadException())
        return;
      String value;
      for (const String& key : keys) {
        if (!DictionaryHelper::get(tokenization.parameters(), key, value))
          continue;
        output->parameters.append(
            payments::mojom::blink::AndroidPayTokenizationParameter::New());
        output->parameters.back()->key = key;
        output->parameters.back()->value = value;
      }
    }
  }
}

void stringifyAndParseMethodSpecificData(const ScriptValue& input,
                                         PaymentMethodDataPtr& output,
                                         ExceptionState& exceptionState) {
  DCHECK(!input.isEmpty());
  if (!input.v8Value()->IsObject() || input.v8Value()->IsArray()) {
    exceptionState.throwTypeError("Data should be a JSON-serializable object");
    return;
  }

  v8::Local<v8::String> value;
  if (!v8::JSON::Stringify(input.context(), input.v8Value().As<v8::Object>())
           .ToLocal(&value)) {
    exceptionState.throwTypeError(
        "Unable to parse payment method specific data");
    return;
  }

  output->stringified_data =
      v8StringToWebCoreString<String>(value, DoNotExternalize);
  maybeSetAndroidPayMethodData(input, output, exceptionState);
}

void validateAndConvertPaymentDetailsModifiers(
    const HeapVector<PaymentDetailsModifier>& input,
    Vector<PaymentDetailsModifierPtr>& output,
    ExceptionState& exceptionState) {
  if (input.isEmpty()) {
    exceptionState.throwTypeError(
        "Must specify at least one payment details modifier");
    return;
  }

  for (const PaymentDetailsModifier& modifier : input) {
    output.append(payments::mojom::blink::PaymentDetailsModifier::New());
    if (modifier.hasTotal()) {
      validateAndConvertTotal(modifier.total(), output.back()->total,
                              exceptionState);
      if (exceptionState.hadException())
        return;
    }

    if (modifier.hasAdditionalDisplayItems()) {
      validateAndConvertDisplayItems(modifier.additionalDisplayItems(),
                                     output.back()->additional_display_items,
                                     exceptionState);
      if (exceptionState.hadException())
        return;
    }

    if (modifier.supportedMethods().isEmpty()) {
      exceptionState.throwTypeError(
          "Must specify at least one payment method identifier");
      return;
    }

    output.back()->method_data =
        payments::mojom::blink::PaymentMethodData::New();
    output.back()->method_data->supported_methods = modifier.supportedMethods();

    if (modifier.hasData() && !modifier.data().isEmpty()) {
      stringifyAndParseMethodSpecificData(
          modifier.data(), output.back()->method_data, exceptionState);
    } else {
      output.back()->method_data->stringified_data = "";
    }
  }
}

String getSelectedShippingOption(
    const Vector<PaymentShippingOptionPtr>& shippingOptions) {
  String result;
  for (const PaymentShippingOptionPtr& shippingOption : shippingOptions) {
    if (shippingOption->selected)
      result = shippingOption->id;
  }
  return result;
}

void validateAndConvertPaymentDetails(const PaymentDetails& input,
                                      bool requestShipping,
                                      PaymentDetailsPtr& output,
                                      String& shippingOptionOutput,
                                      ExceptionState& exceptionState) {
  if (!input.hasTotal()) {
    exceptionState.throwTypeError("Must specify total");
    return;
  }

  validateAndConvertTotal(input.total(), output->total, exceptionState);
  if (exceptionState.hadException())
    return;

  if (input.hasDisplayItems()) {
    validateAndConvertDisplayItems(input.displayItems(), output->display_items,
                                   exceptionState);
    if (exceptionState.hadException())
      return;
  }

  if (input.hasShippingOptions() && requestShipping) {
    validateAndConvertShippingOptions(input.shippingOptions(),
                                      output->shipping_options, exceptionState);
    if (exceptionState.hadException())
      return;
  }

  shippingOptionOutput = getSelectedShippingOption(output->shipping_options);

  if (input.hasModifiers()) {
    validateAndConvertPaymentDetailsModifiers(
        input.modifiers(), output->modifiers, exceptionState);
    if (exceptionState.hadException())
      return;
  }

  if (input.hasError() && !input.error().isNull()) {
    String errorMessage;
    if (!PaymentsValidators::isValidErrorMsgFormat(input.error(),
                                                   &errorMessage)) {
      exceptionState.throwTypeError(errorMessage);
      return;
    }
    output->error = input.error();
  } else {
    output->error = "";
  }
}

void validateAndConvertPaymentMethodData(
    const HeapVector<PaymentMethodData>& input,
    Vector<payments::mojom::blink::PaymentMethodDataPtr>& output,
    ExceptionState& exceptionState) {
  if (input.isEmpty()) {
    exceptionState.throwTypeError(
        "Must specify at least one payment method identifier");
    return;
  }

  for (const PaymentMethodData paymentMethodData : input) {
    if (paymentMethodData.supportedMethods().isEmpty()) {
      exceptionState.throwTypeError(
          "Must specify at least one payment method identifier");
      return;
    }

    output.append(payments::mojom::blink::PaymentMethodData::New());
    output.back()->supported_methods = paymentMethodData.supportedMethods();

    if (paymentMethodData.hasData() && !paymentMethodData.data().isEmpty()) {
      stringifyAndParseMethodSpecificData(paymentMethodData.data(),
                                          output.back(), exceptionState);
    } else {
      output.back()->stringified_data = "";
    }
  }
}

String getValidShippingType(const String& shippingType) {
  static const char* const validValues[] = {
      "shipping", "delivery", "pickup",
  };
  for (size_t i = 0; i < arraysize(validValues); i++) {
    if (shippingType == validValues[i])
      return shippingType;
  }
  return validValues[0];
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

ScriptPromise PaymentRequest::canMakeActivePayment(ScriptState* scriptState) {
  if (!m_paymentProvider.is_bound() || m_canMakeActivePaymentResolver ||
      !scriptState->contextIsValid()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "Cannot query payment request"));
  }

  m_paymentProvider->CanMakeActivePayment();

  m_canMakeActivePaymentResolver = ScriptPromiseResolver::create(scriptState);
  return m_canMakeActivePaymentResolver->promise();
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
  m_paymentProvider->Complete(payments::mojom::blink::PaymentComplete(result));

  m_completeResolver = ScriptPromiseResolver::create(scriptState);
  return m_completeResolver->promise();
}

void PaymentRequest::onUpdatePaymentDetails(
    const ScriptValue& detailsScriptValue) {
  if (!m_showResolver || !m_paymentProvider)
    return;

  PaymentDetails details;
  DummyExceptionStateForTesting exceptionState;
  V8PaymentDetails::toImpl(detailsScriptValue.isolate(),
                           detailsScriptValue.v8Value(), details,
                           exceptionState);
  if (exceptionState.hadException()) {
    m_showResolver->reject(
        DOMException::create(SyntaxError, exceptionState.message()));
    clearResolversAndCloseMojoConnection();
    return;
  }

  PaymentDetailsPtr validatedDetails =
      payments::mojom::blink::PaymentDetails::New();
  validateAndConvertPaymentDetails(details, m_options.requestShipping(),
                                   validatedDetails, m_shippingOption,
                                   exceptionState);
  if (exceptionState.hadException()) {
    m_showResolver->reject(
        DOMException::create(SyntaxError, exceptionState.message()));
    clearResolversAndCloseMojoConnection();
    return;
  }

  m_paymentProvider->UpdateWith(std::move(validatedDetails));
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
  visitor->trace(m_canMakeActivePaymentResolver);
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
  Vector<payments::mojom::blink::PaymentMethodDataPtr> validatedMethodData;
  validateAndConvertPaymentMethodData(methodData, validatedMethodData,
                                      exceptionState);
  if (exceptionState.hadException())
    return;

  if (!scriptState->getExecutionContext()->isSecureContext()) {
    exceptionState.throwSecurityError("Must be in a secure context");
    return;
  }

  if (!allowedToUsePaymentRequest(scriptState->domWindow()->frame())) {
    exceptionState.throwSecurityError(
        RuntimeEnabledFeatures::paymentRequestIFrameEnabled()
            ? "Must be in a top-level browsing context or an iframe needs to "
              "specify 'allowpaymentrequest' explicitly"
            : "Must be in a top-level browsing context");
    return;
  }

  PaymentDetailsPtr validatedDetails =
      payments::mojom::blink::PaymentDetails::New();
  validateAndConvertPaymentDetails(details, m_options.requestShipping(),
                                   validatedDetails, m_shippingOption,
                                   exceptionState);
  if (exceptionState.hadException())
    return;

  if (details.hasError()) {
    exceptionState.throwTypeError("Error message not allowed in constructor");
    return;
  }

  if (m_options.requestShipping())
    m_shippingType = getValidShippingType(m_options.shippingType());

  scriptState->domWindow()->frame()->interfaceProvider()->getInterface(
      mojo::GetProxy(&m_paymentProvider));
  m_paymentProvider.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&PaymentRequest::OnError, wrapWeakPersistent(this),
                PaymentErrorReason::UNKNOWN)));
  m_paymentProvider->Init(
      m_clientBinding.CreateInterfacePtrAndBind(),
      std::move(validatedMethodData), std::move(validatedDetails),
      payments::mojom::blink::PaymentOptions::From(m_options));
}

void PaymentRequest::contextDestroyed() {
  clearResolversAndCloseMojoConnection();
}

void PaymentRequest::OnShippingAddressChange(PaymentAddressPtr address) {
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

void PaymentRequest::OnPaymentResponse(PaymentResponsePtr response) {
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

void PaymentRequest::OnError(PaymentErrorReason error) {
  if (!Platform::current()) {
    // TODO(rockot): Clean this up once renderer shutdown sequence is fixed.
    return;
  }

  bool isError = false;
  ExceptionCode ec = UnknownError;
  String message;

  switch (error) {
    case PaymentErrorReason::USER_CANCEL:
      message = "Request cancelled";
      break;
    case PaymentErrorReason::NOT_SUPPORTED:
      isError = true;
      ec = NotSupportedError;
      message = "The payment method is not supported";
      break;
    case PaymentErrorReason::UNKNOWN:
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

    if (m_canMakeActivePaymentResolver)
      m_canMakeActivePaymentResolver->reject(DOMException::create(ec, message));
  } else {
    if (m_completeResolver)
      m_completeResolver->reject(message);

    if (m_showResolver)
      m_showResolver->reject(message);

    if (m_abortResolver)
      m_abortResolver->reject(message);

    if (m_canMakeActivePaymentResolver)
      m_canMakeActivePaymentResolver->reject(message);
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

void PaymentRequest::OnCanMakeActivePayment(ActivePaymentQueryResult result) {
  DCHECK(m_canMakeActivePaymentResolver);

  switch (result) {
    case ActivePaymentQueryResult::CAN_MAKE_ACTIVE_PAYMENT:
      m_canMakeActivePaymentResolver->resolve(true);
      break;
    case ActivePaymentQueryResult::CANNOT_MAKE_ACTIVE_PAYMENT:
      m_canMakeActivePaymentResolver->resolve(false);
      break;
    case ActivePaymentQueryResult::QUERY_QUOTA_EXCEEDED:
      m_canMakeActivePaymentResolver->reject(
          DOMException::create(QuotaExceededError, "Query quota exceeded"));
      break;
  }

  m_canMakeActivePaymentResolver.clear();
}

void PaymentRequest::onCompleteTimeout(TimerBase*) {
  m_paymentProvider->Complete(payments::mojom::blink::PaymentComplete(Fail));
  clearResolversAndCloseMojoConnection();
}

void PaymentRequest::clearResolversAndCloseMojoConnection() {
  m_completeTimer.stop();
  m_completeResolver.clear();
  m_showResolver.clear();
  m_abortResolver.clear();
  m_canMakeActivePaymentResolver.clear();
  if (m_clientBinding.is_bound())
    m_clientBinding.Close();
  m_paymentProvider.reset();
}

}  // namespace blink
