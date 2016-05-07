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
#include "modules/EventTargetModulesNames.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/PaymentRequestUpdateEvent.h"
#include "modules/payments/PaymentResponse.h"
#include "modules/payments/PaymentsValidators.h"
#include "modules/payments/ShippingAddress.h"
#include "modules/payments/ShippingOption.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/wtf_array.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/ServiceRegistry.h"
#include <utility>

namespace mojo {

using blink::mojom::blink::CurrencyAmount;
using blink::mojom::blink::CurrencyAmountPtr;
using blink::mojom::blink::PaymentDetails;
using blink::mojom::blink::PaymentDetailsPtr;
using blink::mojom::blink::PaymentItem;
using blink::mojom::blink::PaymentItemPtr;
using blink::mojom::blink::PaymentOptions;
using blink::mojom::blink::PaymentOptionsPtr;
using blink::mojom::blink::ShippingOption;
using blink::mojom::blink::ShippingOptionPtr;

template <>
struct TypeConverter<CurrencyAmountPtr, blink::CurrencyAmount> {
    static CurrencyAmountPtr Convert(const blink::CurrencyAmount& input)
    {
        CurrencyAmountPtr output = CurrencyAmount::New();
        output->currency_code = input.currencyCode();
        output->value = input.value();
        return output;
    }
};

template <>
struct TypeConverter<PaymentItemPtr, blink::PaymentItem> {
    static PaymentItemPtr Convert(const blink::PaymentItem& input)
    {
        PaymentItemPtr output = PaymentItem::New();
        output->id = input.id();
        output->label = input.label();
        output->amount = CurrencyAmount::From(input.amount());
        return output;
    }
};

template <>
struct TypeConverter<ShippingOptionPtr, blink::ShippingOption> {
    static ShippingOptionPtr Convert(const blink::ShippingOption& input)
    {
        ShippingOptionPtr output = ShippingOption::New();
        output->id = input.id();
        output->label = input.label();
        output->amount = CurrencyAmount::From(input.amount());
        return output;
    }
};

template <>
struct TypeConverter<PaymentDetailsPtr, blink::PaymentDetails> {
    static PaymentDetailsPtr Convert(const blink::PaymentDetails& input)
    {
        PaymentDetailsPtr output = PaymentDetails::New();
        output->items = mojo::WTFArray<PaymentItemPtr>::From(input.items());
        if (input.hasShippingOptions())
            output->shipping_options = mojo::WTFArray<ShippingOptionPtr>::From(input.shippingOptions());
        else
            output->shipping_options = mojo::WTFArray<ShippingOptionPtr>::New(0);
        return output;
    }
};

template <>
struct TypeConverter<PaymentOptionsPtr, blink::PaymentOptions> {
    static PaymentOptionsPtr Convert(const blink::PaymentOptions& input)
    {
        PaymentOptionsPtr output = PaymentOptions::New();
        output->request_shipping = input.requestShipping();
        return output;
    }
};

} // namespace mojo

namespace blink {
namespace {

// Validates ShippingOption and PaymentItem dictionaries, which happen to have identical fields.
template <typename T>
void validateShippingOptionsOrPaymentItems(HeapVector<T> items, ExceptionState& exceptionState)
{
    String errorMessage;
    for (const auto& item : items) {
        if (!item.hasId() || item.id().isEmpty()) {
            exceptionState.throwTypeError("Item id required");
            return;
        }

        if (!item.hasLabel() || item.label().isEmpty()) {
            exceptionState.throwTypeError("Item label required");
            return;
        }

        if (!item.hasAmount()) {
            exceptionState.throwTypeError("Currency amount required");
            return;
        }

        if (!item.amount().hasCurrencyCode()) {
            exceptionState.throwTypeError("Currency code required");
            return;
        }

        if (!item.amount().hasValue()) {
            exceptionState.throwTypeError("Currency value required");
            return;
        }

        if (!PaymentsValidators::isValidCurrencyCodeFormat(item.amount().currencyCode(), &errorMessage)) {
            exceptionState.throwTypeError(errorMessage);
            return;
        }

        if (!PaymentsValidators::isValidAmountFormat(item.amount().value(), &errorMessage)) {
            exceptionState.throwTypeError(errorMessage);
            return;
        }
    }
}

void validatePaymentDetails(const PaymentDetails& details, ExceptionState& exceptionState)
{
    if (!details.hasItems()) {
        exceptionState.throwTypeError("Must specify items");
        return;
    }

    if (details.items().isEmpty()) {
        exceptionState.throwTypeError("Must specify at least one item");
        return;
    }

    validateShippingOptionsOrPaymentItems(details.items(), exceptionState);
    if (exceptionState.hadException())
        return;

    if (details.hasShippingOptions())
        validateShippingOptionsOrPaymentItems(details.shippingOptions(), exceptionState);
}

} // namespace

PaymentRequest* PaymentRequest::create(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, ExceptionState& exceptionState)
{
    return new PaymentRequest(scriptState, supportedMethods, details, PaymentOptions(), ScriptValue(), exceptionState);
}

PaymentRequest* PaymentRequest::create(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, const PaymentOptions& options, ExceptionState& exceptionState)
{
    return new PaymentRequest(scriptState, supportedMethods, details, options, ScriptValue(), exceptionState);
}

PaymentRequest* PaymentRequest::create(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, const PaymentOptions& options, const ScriptValue& data, ExceptionState& exceptionState)
{
    return new PaymentRequest(scriptState, supportedMethods, details, options, data, exceptionState);
}

PaymentRequest::~PaymentRequest()
{
}

ScriptPromise PaymentRequest::show(ScriptState* scriptState)
{
    if (m_showResolver)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Already called show() once"));

    if (!scriptState->domWindow() || !scriptState->domWindow()->frame())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Cannot show the payment request"));

    DCHECK(!m_paymentProvider.is_bound());
    scriptState->domWindow()->frame()->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_paymentProvider));
    m_paymentProvider.set_connection_error_handler(sameThreadBindForMojo(&PaymentRequest::OnError, this));
    m_paymentProvider->SetClient(m_clientBinding.CreateInterfacePtrAndBind());
    m_paymentProvider->Show(std::move(m_supportedMethods), mojom::blink::PaymentDetails::From(m_details), mojom::blink::PaymentOptions::From(m_options), m_stringifiedData.isNull() ? "" : m_stringifiedData);

    m_showResolver = ScriptPromiseResolver::create(scriptState);
    return m_showResolver->promise();
}

void PaymentRequest::abort(ExceptionState& exceptionState)
{
    if (!m_showResolver) {
        exceptionState.throwDOMException(InvalidStateError, "Never called show(), so nothing to abort");
        return;
    }

    m_paymentProvider->Abort();
}

const AtomicString& PaymentRequest::interfaceName() const
{
    return EventTargetNames::PaymentRequest;
}

ExecutionContext* PaymentRequest::getExecutionContext() const
{
    return ContextLifecycleObserver::getExecutionContext();
}

ScriptPromise PaymentRequest::complete(ScriptState* scriptState, bool success)
{
    if (m_completeResolver)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Already called complete() once"));

    // The payment provider should respond in PaymentRequest::OnComplete().
    m_paymentProvider->Complete(success);

    m_completeResolver = ScriptPromiseResolver::create(scriptState);
    return m_completeResolver->promise();
}

void PaymentRequest::onUpdatePaymentDetails(const ScriptValue& detailsScriptValue)
{
    if (!m_showResolver || !m_paymentProvider)
        return;

    PaymentDetails details;
    TrackExceptionState exceptionState;
    V8PaymentDetails::toImpl(detailsScriptValue.isolate(), detailsScriptValue.v8Value(), details, exceptionState);
    if (exceptionState.hadException()) {
        m_showResolver->reject(DOMException::create(SyntaxError, exceptionState.message()));
        stopResolversAndCloseMojoConnection();
        return;
    }

    validatePaymentDetails(details, exceptionState);
    if (exceptionState.hadException()) {
        m_showResolver->reject(DOMException::create(SyntaxError, exceptionState.message()));
        stopResolversAndCloseMojoConnection();
        return;
    }

    // Set the currently selected option if only one option was passed.
    if (details.hasShippingOptions() && details.shippingOptions().size() == 1)
        m_shippingOption = details.shippingOptions().begin()->id();
    else
        m_shippingOption = String();

    m_paymentProvider->UpdateWith(mojom::blink::PaymentDetails::From(details));
}

void PaymentRequest::onUpdatePaymentDetailsFailure(const ScriptValue& error)
{
    if (m_showResolver)
        m_showResolver->reject(error);
    if (m_completeResolver)
        m_completeResolver->reject(error);
    stopResolversAndCloseMojoConnection();
}

DEFINE_TRACE(PaymentRequest)
{
    visitor->trace(m_details);
    visitor->trace(m_options);
    visitor->trace(m_shippingAddress);
    visitor->trace(m_showResolver);
    visitor->trace(m_completeResolver);
    EventTargetWithInlineData::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

PaymentRequest::PaymentRequest(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, const PaymentOptions& options, const ScriptValue& data, ExceptionState& exceptionState)
    : ContextLifecycleObserver(scriptState->getExecutionContext())
    , m_options(options)
    , m_clientBinding(this)
{
    // TODO(rouslan): Also check for a top-level browsing context.
    // https://github.com/w3c/browser-payment-api/issues/2
    if (!scriptState->getExecutionContext()->isSecureContext()) {
        exceptionState.throwSecurityError("Must be in a secure context");
        return;
    }

    if (supportedMethods.isEmpty()) {
        exceptionState.throwTypeError("Must specify at least one payment method identifier");
        return;
    }
    m_supportedMethods = supportedMethods;

    validatePaymentDetails(details, exceptionState);
    if (exceptionState.hadException())
        return;
    m_details = details;

    if (!data.isEmpty()) {
        RefPtr<JSONValue> value = toJSONValue(data.context(), data.v8Value());
        if (!value) {
            exceptionState.throwTypeError("Unable to parse payment method specific data");
            return;
        }
        if (!value->isNull()) {
            if (value->getType() != JSONValue::TypeObject) {
                exceptionState.throwTypeError("Payment method specific data should be a JSON-serializable object");
                return;
            }

            RefPtr<JSONObject> jsonData = JSONObject::cast(value);
            for (const auto& paymentMethodSpecificKeyValue : *jsonData) {
                if (!supportedMethods.contains(paymentMethodSpecificKeyValue.key)) {
                    exceptionState.throwTypeError("'" + paymentMethodSpecificKeyValue.key + "' should match one of the payment method identifiers");
                    return;
                }
                if (paymentMethodSpecificKeyValue.value->getType() != JSONValue::TypeObject) {
                    exceptionState.throwTypeError("Data for '" + paymentMethodSpecificKeyValue.key + "' should be a JSON-serializable object");
                    return;
                }
            }

            m_stringifiedData = jsonData->toJSONString();
        }
    }

    // Set the currently selected option if only one option is passed and shipping is requested.
    if (options.requestShipping() && details.hasShippingOptions() && details.shippingOptions().size() == 1)
        m_shippingOption = details.shippingOptions().begin()->id();
}

void PaymentRequest::contextDestroyed()
{
    stopResolversAndCloseMojoConnection();
}

void PaymentRequest::OnShippingAddressChange(mojom::blink::ShippingAddressPtr address)
{
    DCHECK(m_showResolver);
    DCHECK(!m_completeResolver);

    String errorMessage;
    if (!PaymentsValidators::isValidShippingAddress(address, &errorMessage)) {
        m_showResolver->reject(DOMException::create(SyntaxError, errorMessage));
        stopResolversAndCloseMojoConnection();
        return;
    }

    m_shippingAddress = new ShippingAddress(std::move(address));
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
    event->setTarget(this);
    event->setPaymentDetailsUpdater(this);
    bool success = getExecutionContext()->getEventQueue()->enqueueEvent(event);
    DCHECK(success);
    ALLOW_UNUSED_LOCAL(success);
}

void PaymentRequest::OnShippingOptionChange(const String& shippingOptionId)
{
    DCHECK(m_showResolver);
    DCHECK(!m_completeResolver);
    m_shippingOption = shippingOptionId;
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingoptionchange);
    event->setTarget(this);
    event->setPaymentDetailsUpdater(this);
    bool success = getExecutionContext()->getEventQueue()->enqueueEvent(event);
    DCHECK(success);
    ALLOW_UNUSED_LOCAL(success);
}

void PaymentRequest::OnPaymentResponse(mojom::blink::PaymentResponsePtr response)
{
    DCHECK(m_showResolver);
    DCHECK(!m_completeResolver);

    if (response->shipping_address) {
        String errorMessage;
        if (!PaymentsValidators::isValidShippingAddress(response->shipping_address, &errorMessage)) {
            m_showResolver->reject(DOMException::create(SyntaxError, errorMessage));
            stopResolversAndCloseMojoConnection();
            return;
        }

        m_shippingAddress = new ShippingAddress(std::move(response->shipping_address));
        m_shippingOption = response->shipping_option_id;
    }

    m_showResolver->resolve(new PaymentResponse(std::move(response), this));

    // Do not close the mojo connection here. The merchant website should call
    // PaymentResponse::complete(boolean), which will be forwarded over the mojo
    // connection to display a success or failure message to the user.
    m_showResolver.clear();
}

void PaymentRequest::OnError()
{
    if (m_completeResolver)
        m_completeResolver->reject(DOMException::create(SyntaxError, "Request cancelled"));
    if (m_showResolver)
        m_showResolver->reject(DOMException::create(SyntaxError, "Request cancelled"));
    stopResolversAndCloseMojoConnection();
}

void PaymentRequest::OnComplete()
{
    DCHECK(m_completeResolver);
    m_completeResolver->resolve();
    stopResolversAndCloseMojoConnection();
}

void PaymentRequest::stopResolversAndCloseMojoConnection()
{
    if (m_completeResolver)
        m_completeResolver->stop();
    m_completeResolver.clear();

    if (m_showResolver)
        m_showResolver->stop();
    m_showResolver.clear();

    if (m_clientBinding.is_bound())
        m_clientBinding.Close();
    m_paymentProvider.reset();
}

} // namespace blink
