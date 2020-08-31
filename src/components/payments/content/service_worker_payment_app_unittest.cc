// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/mock_payment_request_delegate.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class ServiceWorkerPaymentAppTest : public testing::Test,
                                    public PaymentRequestSpec::Observer {
 public:
  ServiceWorkerPaymentAppTest() {}
  ~ServiceWorkerPaymentAppTest() override {}

 protected:
  const SkBitmap* icon_bitmap() const { return icon_bitmap_; }

  void OnSpecUpdated() override {}

  void SetUp() override {
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    mojom::PaymentItemPtr total = mojom::PaymentItem::New();
    mojom::PaymentCurrencyAmountPtr amount =
        mojom::PaymentCurrencyAmount::New();
    amount->value = "5.00";
    amount->currency = "USD";
    total->amount = std::move(amount);
    details->total = std::move(total);
    details->id = base::Optional<std::string>("123456");
    details->modifiers = std::vector<mojom::PaymentDetailsModifierPtr>();

    mojom::PaymentDetailsModifierPtr modifier_1 =
        mojom::PaymentDetailsModifier::New();
    modifier_1->total = mojom::PaymentItem::New();
    modifier_1->total->amount = mojom::PaymentCurrencyAmount::New();
    modifier_1->total->amount->currency = "USD";
    modifier_1->total->amount->value = "4.00";
    modifier_1->method_data = mojom::PaymentMethodData::New();
    modifier_1->method_data->supported_method = "basic-card";
    details->modifiers->push_back(std::move(modifier_1));

    mojom::PaymentDetailsModifierPtr modifier_2 =
        mojom::PaymentDetailsModifier::New();
    modifier_2->total = mojom::PaymentItem::New();
    modifier_2->total->amount = mojom::PaymentCurrencyAmount::New();
    modifier_2->total->amount->currency = "USD";
    modifier_2->total->amount->value = "3.00";
    modifier_2->method_data = mojom::PaymentMethodData::New();
    modifier_2->method_data->supported_method = "https://bobpay.com";
    details->modifiers->push_back(std::move(modifier_2));

    mojom::PaymentDetailsModifierPtr modifier_3 =
        mojom::PaymentDetailsModifier::New();
    modifier_3->total = mojom::PaymentItem::New();
    modifier_3->total->amount = mojom::PaymentCurrencyAmount::New();
    modifier_3->total->amount->currency = "USD";
    modifier_3->total->amount->value = "2.00";
    modifier_3->method_data = mojom::PaymentMethodData::New();
    modifier_3->method_data->supported_method = "https://alicepay.com";
    details->modifiers->push_back(std::move(modifier_3));

    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentMethodDataPtr entry_1 = mojom::PaymentMethodData::New();
    entry_1->supported_method = "basic-card";
    entry_1->supported_networks.push_back(mojom::BasicCardNetwork::UNIONPAY);
    entry_1->supported_networks.push_back(mojom::BasicCardNetwork::JCB);
    entry_1->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
    method_data.push_back(std::move(entry_1));

    mojom::PaymentMethodDataPtr entry_2 = mojom::PaymentMethodData::New();
    entry_2->supported_method = "https://bobpay.com";
    method_data.push_back(std::move(entry_2));

    spec_ = std::make_unique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), std::move(details),
        std::move(method_data), this, "en-US");
  }

  void TearDown() override {}

  void CreateServiceWorkerPaymentApp(bool with_url_method) {
    constexpr int kBitmapDimension = 16;

    std::unique_ptr<content::StoredPaymentApp> stored_app =
        std::make_unique<content::StoredPaymentApp>();
    stored_app->registration_id = 123456;
    stored_app->scope = GURL("https://bobpay.com");
    stored_app->name = "bobpay";
    stored_app->icon = std::make_unique<SkBitmap>();
    stored_app->icon->allocN32Pixels(kBitmapDimension, kBitmapDimension);
    stored_app->icon->eraseColor(SK_ColorRED);
    stored_app->enabled_methods.emplace_back("basic-card");
    if (with_url_method)
      stored_app->enabled_methods.emplace_back("https://bobpay.com");
    stored_app->capabilities.emplace_back(content::StoredCapabilities());
    stored_app->capabilities.back().supported_card_networks.emplace_back(
        static_cast<int32_t>(mojom::BasicCardNetwork::UNIONPAY));
    stored_app->capabilities.back().supported_card_networks.emplace_back(
        static_cast<int32_t>(mojom::BasicCardNetwork::JCB));
    stored_app->user_hint = "Visa 4012 ... 1881";
    stored_app->prefer_related_applications = false;

    icon_bitmap_ = stored_app->icon.get();
    app_ = std::make_unique<ServiceWorkerPaymentApp>(
        &browser_context_, GURL("https://testmerchant.com"),
        GURL("https://testmerchant.com/bobpay"), spec_.get(),
        std::move(stored_app), &delegate_,
        base::BindRepeating(
            [](const url::Origin& origin,
               int64_t registration_id) { /* Intentionally left blank. */ }));
  }

  ServiceWorkerPaymentApp* GetApp() { return app_.get(); }

  mojom::PaymentRequestEventDataPtr CreatePaymentRequestEventData() {
    return app_->CreatePaymentRequestEventData();
  }

  mojom::CanMakePaymentEventDataPtr CreateCanMakePaymentEventData() {
    return app_->CreateCanMakePaymentEventData();
  }

 private:
  MockPaymentRequestDelegate delegate_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;

  std::unique_ptr<PaymentRequestSpec> spec_;
  std::unique_ptr<ServiceWorkerPaymentApp> app_;
  const SkBitmap* icon_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPaymentAppTest);
};

// Test app info and status are correct.
TEST_F(ServiceWorkerPaymentAppTest, AppInfo) {
  CreateServiceWorkerPaymentApp(true);

  EXPECT_TRUE(GetApp()->IsCompleteForPayment());

  EXPECT_EQ(base::UTF16ToUTF8(GetApp()->GetLabel()), "bobpay");
  EXPECT_EQ(base::UTF16ToUTF8(GetApp()->GetSublabel()), "bobpay.com");

  const gfx::Size expected_size{icon_bitmap()->width(),
                                icon_bitmap()->height()};
  EXPECT_EQ(GetApp()->icon_image_skia().size(), expected_size);
}

// Test payment request event data can be correctly constructed for invoking
// InvokePaymentApp.
TEST_F(ServiceWorkerPaymentAppTest, CreatePaymentRequestEventData) {
  CreateServiceWorkerPaymentApp(true);

  mojom::PaymentRequestEventDataPtr event_data =
      CreatePaymentRequestEventData();

  EXPECT_EQ(event_data->top_origin.spec(), "https://testmerchant.com/");
  EXPECT_EQ(event_data->payment_request_origin.spec(),
            "https://testmerchant.com/bobpay");

  EXPECT_EQ(event_data->method_data.size(), 2U);
  EXPECT_EQ(event_data->method_data[0]->supported_method, "basic-card");
  EXPECT_EQ(event_data->method_data[0]->supported_networks.size(), 3U);
  EXPECT_EQ(event_data->method_data[1]->supported_method, "https://bobpay.com");

  EXPECT_EQ(event_data->total->currency, "USD");
  EXPECT_EQ(event_data->total->value, "5.00");
  EXPECT_EQ(event_data->payment_request_id, "123456");

  EXPECT_EQ(event_data->modifiers.size(), 2U);
  EXPECT_EQ(event_data->modifiers[0]->total->amount->value, "4.00");
  EXPECT_EQ(event_data->modifiers[0]->total->amount->currency, "USD");
  EXPECT_EQ(event_data->modifiers[0]->method_data->supported_method,
            "basic-card");
  EXPECT_EQ(event_data->modifiers[1]->total->amount->value, "3.00");
  EXPECT_EQ(event_data->modifiers[1]->total->amount->currency, "USD");
  EXPECT_EQ(event_data->modifiers[1]->method_data->supported_method,
            "https://bobpay.com");
}

// Test CanMakePaymentEventData can be correctly constructed for invoking
// Validate.
TEST_F(ServiceWorkerPaymentAppTest, CreateCanMakePaymentEvent) {
  CreateServiceWorkerPaymentApp(false);
  mojom::CanMakePaymentEventDataPtr event_data =
      CreateCanMakePaymentEventData();
  EXPECT_TRUE(event_data.is_null());

  CreateServiceWorkerPaymentApp(true);
  event_data = CreateCanMakePaymentEventData();
  EXPECT_FALSE(event_data.is_null());

  EXPECT_EQ(event_data->top_origin.spec(), "https://testmerchant.com/");
  EXPECT_EQ(event_data->payment_request_origin.spec(),
            "https://testmerchant.com/bobpay");

  EXPECT_EQ(event_data->method_data.size(), 1U);
  EXPECT_EQ(event_data->method_data[0]->supported_method, "https://bobpay.com");

  EXPECT_EQ(event_data->modifiers.size(), 1U);
  EXPECT_EQ(event_data->modifiers[0]->total->amount->value, "3.00");
  EXPECT_EQ(event_data->modifiers[0]->total->amount->currency, "USD");
  EXPECT_EQ(event_data->modifiers[0]->method_data->supported_method,
            "https://bobpay.com");
}

// Test the case when CanMakePaymentEvent cannot be fired. The app should be
// considered valid, but not ready for payment.
TEST_F(ServiceWorkerPaymentAppTest, ValidateCanMakePayment) {
  // CanMakePaymentEvent is not fired because this test app does not have any
  // explicitly verified methods.
  CreateServiceWorkerPaymentApp(/*with_url_method=*/true);
  GetApp()->ValidateCanMakePayment(base::BindOnce(
      [](ServiceWorkerPaymentApp*, bool result) { EXPECT_TRUE(result); }));
  EXPECT_FALSE(GetApp()->HasEnrolledInstrument());
}

// Test modifiers can be matched based on capabilities.
TEST_F(ServiceWorkerPaymentAppTest, IsValidForModifier) {
  CreateServiceWorkerPaymentApp(true);

  EXPECT_TRUE(GetApp()->IsValidForModifier(
      /*method=*/"basic-card", /*supported_networks_specified=*/false,
      /*supported_networks=*/{}));

  EXPECT_TRUE(GetApp()->IsValidForModifier(
      /*method=*/"https://bobpay.com", /*supported_networks_specified=*/true,
      /*supported_networks=*/{}));

  EXPECT_FALSE(GetApp()->IsValidForModifier(
      /*method=*/"basic-card", /*supported_networks_specified=*/true,
      /*supported_networks=*/{"mastercard"}));

  EXPECT_TRUE(GetApp()->IsValidForModifier(
      /*method=*/"basic-card", /*supported_networks_specified=*/true,
      /*supported_networks=*/{"unionpay"}));
}

}  // namespace payments
