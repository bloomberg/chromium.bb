// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/payments/PaymentDetails.h"
#include "modules/payments/PaymentDetailsTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"

namespace blink {
namespace {

class DetailsTestCase {
public:
    DetailsTestCase(PaymentTestDetailToChange detail, PaymentTestDataToChange data, PaymentTestModificationType modType, const char* valueToUse, bool expectException = false, ExceptionCode expectedExceptionCode = 0)
        : m_detail(detail)
        , m_data(data)
        , m_modType(modType)
        , m_valueToUse(valueToUse)
        , m_expectException(expectException)
        , m_expectedExceptionCode(expectedExceptionCode)
    {
    }

    ~DetailsTestCase() {}

    PaymentDetails buildDetails() const
    {
        return buildPaymentDetailsForTest(m_detail, m_data, m_modType, m_valueToUse);
    }

    bool expectException() const
    {
        return m_expectException;
    }

    ExceptionCode getExpectedExceptionCode() const
    {
        return m_expectedExceptionCode;
    }

private:
    PaymentTestDetailToChange m_detail;
    PaymentTestDataToChange m_data;
    PaymentTestModificationType m_modType;
    const char* m_valueToUse;
    bool m_expectException;
    ExceptionCode m_expectedExceptionCode;
};

class PaymentRequestDetailsTest : public testing::TestWithParam<DetailsTestCase> {
public:
    PaymentRequestDetailsTest()
        : m_page(DummyPageHolder::create())
    {
        setSecurityOrigin("https://www.example.com/");
    }

    ~PaymentRequestDetailsTest() override {}

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExceptionState& getExceptionState() { return m_exceptionState; }

    void setSecurityOrigin(const String& securityOrigin)
    {
        m_page->document().setSecurityOrigin(SecurityOrigin::create(KURL(KURL(), securityOrigin)));
    }

private:
    OwnPtr<DummyPageHolder> m_page;
    TrackExceptionState m_exceptionState;
};

TEST_P(PaymentRequestDetailsTest, ValidatesDetails)
{
    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), GetParam().buildDetails(), getExceptionState());

    EXPECT_EQ(GetParam().expectException(), getExceptionState().hadException());
    if (GetParam().expectException())
        EXPECT_EQ(GetParam().getExpectedExceptionCode(), getExceptionState().code());
}

INSTANTIATE_TEST_CASE_P(MissingData,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataAmount, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataId, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataLabel, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataAmount, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataId, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataLabel, PaymentTestRemoveKey, "", true, V8TypeError)));

INSTANTIATE_TEST_CASE_P(EmptyData,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataId, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataLabel, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataId, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataLabel, PaymentTestOverwriteValue, "", true, V8TypeError)));

INSTANTIATE_TEST_CASE_P(ValidCurrencyCodeFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USD"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USD")));

INSTANTIATE_TEST_CASE_P(InvalidCurrencyCodeFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US1", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USDO", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "usd", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US1", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USDO", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "usd", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError)));

INSTANTIATE_TEST_CASE_P(ValidValueFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "0"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-0"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-3"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10.99"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-3.00"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "0"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-0"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-3"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10.99"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-3.00"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-012345678901234567890123456789")));

INSTANTIATE_TEST_CASE_P(InvalidValueFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "notdigits", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "ALSONOTDIGITS", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, ".99", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1.0.0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1/3", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "notdigits", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "ALSONOTDIGITS", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, ".99", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1.0.0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1/3", true, V8TypeError)));

} // namespace
} // namespace blink
