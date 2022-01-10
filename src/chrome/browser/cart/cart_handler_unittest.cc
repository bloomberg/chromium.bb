// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
void GetEvaluationMerchantCarts(
    base::OnceClosure closure,
    std::vector<chrome_cart::mojom::MerchantCartPtr> expected,
    std::vector<chrome_cart::mojom::MerchantCartPtr> found) {
  ASSERT_EQ(expected.size(), found.size());
  for (size_t i = 0; i < expected.size(); i++) {
    ASSERT_EQ(expected[i]->merchant, found[i]->merchant);
    ASSERT_EQ(expected[i]->cart_url, found[i]->cart_url);
    ASSERT_EQ(expected[i]->discount_text, found[i]->discount_text);
    ASSERT_EQ(expected[i]->product_image_urls.size(),
              found[i]->product_image_urls.size());
    for (size_t j = 0; j < expected[i]->product_image_urls.size(); j++) {
      ASSERT_EQ(expected[i]->product_image_urls[i],
                found[i]->product_image_urls[i]);
    }
  }
  std::move(closure).Run();
}

void GetEvaluationMerchantCartWithUtmSource(
    base::OnceClosure closure,
    bool expected_has_utm_source,
    const std::string& expected_utm_source_tag_and_value,
    std::vector<chrome_cart::mojom::MerchantCartPtr> found) {
  EXPECT_EQ(1U, found.size());
  EXPECT_EQ(expected_has_utm_source,
            found[0]->cart_url.spec().find("utm_source") != std::string::npos);
  if (expected_has_utm_source) {
    EXPECT_EQ(expected_has_utm_source,
              found[0]->cart_url.spec().find(
                  expected_utm_source_tag_and_value) != std::string::npos);
  }
  std::move(closure).Run();
}

cart_db::ChromeCartContentProto BuildProto(const char* key,
                                           const char* domain,
                                           const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(base::Time::Now().ToDoubleT());
  return proto;
}

const char kFakeMerchantKey[] = "Fake:foo.com";
const char kFakeMerchant[] = "foo.com";
const char kFakeMerchantURL[] = "https://www.foo.com";
const char kMockMerchantBKey[] = "bar.com";
const char kMockMerchantB[] = "bar.com";
const char kMockMerchantURLB[] = "https://www.bar.com";
const cart_db::ChromeCartContentProto kFakeProto =
    BuildProto(kFakeMerchantKey, kFakeMerchant, kFakeMerchantURL);
const cart_db::ChromeCartContentProto kMockProtoB =
    BuildProto(kMockMerchantBKey, kMockMerchantB, kMockMerchantURLB);
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedFake = {{kFakeMerchant, kFakeProto}};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedAllData = {
        {kFakeMerchant, kFakeProto},
        {kMockMerchantB, kMockProtoB},
};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kEmptyExpected = {};
}  // namespace

class CartHandlerTest : public testing::Test {
 public:
  CartHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    handler_ = std::make_unique<CartHandler>(
        mojo::PendingReceiver<chrome_cart::mojom::CartHandler>(),
        profile_.get());
    service_ = CartServiceFactory::GetForProfile(profile_.get());
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluationCartHiddenStatus(
      base::OnceClosure closure,
      bool isHidden,
      bool result,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(isHidden, found[0].second.is_hidden());
    std::move(closure).Run();
  }

  void GetEvaluationCartRemovedStatus(
      base::OnceClosure closure,
      bool isRemoved,
      bool result,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(isRemoved, found[0].second.is_removed());
    std::move(closure).Run();
  }

  void GetEvaluationBoolResult(base::OnceClosure closure,
                               bool expected_show,
                               bool actual_show) {
    EXPECT_EQ(expected_show, actual_show);
    std::move(closure).Run();
  }

  void TearDown() override {}

 protected:
  // This needs to be declared before |task_environment_|, so that it will be
  // destroyed after |task_environment_| has run all the tasks on other threads
  // that might check if a feature is enabled.
  base::test::ScopedFeatureList feature_list_;
  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CartHandler> handler_;
  raw_ptr<CartService> service_;
  base::HistogramTester histogram_tester_;
};

// Verifies the hide status is flipped by hiding and restoring.
TEST_F(CartHandlerTest, TestHideStatusChange) {
  ASSERT_FALSE(service_->IsHidden());

  handler_->HideCartModule();
  ASSERT_TRUE(service_->IsHidden());

  handler_->RestoreHiddenCartModule();
  ASSERT_FALSE(service_->IsHidden());
}

// Tests hiding a single cart and undoing the hide.
TEST_F(CartHandlerTest, TestHideCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantBKey, kMockProtoB,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  handler_->HideCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  handler_->RestoreHiddenCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests removing a single cart and undoing the remove.
TEST_F(CartHandlerTest, TestRemoveCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantB, kMockProtoB,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  handler_->RemoveCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  handler_->RestoreRemovedCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Override CartHandlerTest so that we can initialize feature_list_ in our
// constructor, before CartHandlerTest::SetUp is called.
class CartHandlerNtpModuleFakeDataTest : public CartHandlerTest {
 public:
  CartHandlerNtpModuleFakeDataTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    feature_list_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{"NtpChromeCartModuleDataParam", "fake"}});
  }
};

// Verifies GetMerchantCarts loads fake data with feature parameter.
TEST_F(CartHandlerNtpModuleFakeDataTest, TestEnableFakeData) {
  // Remove fake data loaded by CartService::CartService.
  service_->DeleteCartsWithFakeData();

  service_->AddCart(kFakeMerchantKey, absl::nullopt, kFakeProto);
  service_->AddCart(kMockMerchantBKey, absl::nullopt, kMockProtoB);
  task_environment_.RunUntilIdle();

  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart1->merchant = kFakeMerchant;
  dummy_cart1->cart_url = GURL(kFakeMerchantURL);
  carts.push_back(std::move(dummy_cart1));

  base::RunLoop run_loop;
  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCarts, run_loop.QuitClosure(), std::move(carts)));
  run_loop.Run();
}

// Override CartHandlerTest so that we can initialize feature_list_ in our
// constructor, before CartHandlerTest::SetUp is called.
class CartHandlerNtpModuleTest : public CartHandlerTest {
 public:
  CartHandlerNtpModuleTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    feature_list_.InitAndEnableFeature(ntp_features::kNtpChromeCartModule);
  }
};

// Verifies GetMerchantCarts loads real data without fake data parameter.
// Flaky, see crbug.com/1185497.
TEST_F(CartHandlerNtpModuleTest, DISABLED_TestDisableFakeData) {
  base::RunLoop run_loop;
  service_->AddCart(kFakeMerchantKey, absl::nullopt, kFakeProto);
  service_->AddCart(kMockMerchantBKey, absl::nullopt, kMockProtoB);
  task_environment_.RunUntilIdle();

  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart1->merchant = kFakeMerchant;
  dummy_cart1->cart_url = GURL(kFakeMerchantURL);
  auto dummy_cart2 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart2->merchant = kMockMerchantB;
  dummy_cart2->cart_url = GURL(kMockMerchantURLB);
  carts.push_back(std::move(dummy_cart2));
  carts.push_back(std::move(dummy_cart1));

  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCarts, run_loop.QuitClosure(), std::move(carts)));
  run_loop.Run();
}

// Tests show welcome surface for first three appearances of cart module.
TEST_F(CartHandlerNtpModuleTest, TestShowWelcomeSurface) {
  base::RunLoop run_loop[4 * CartService::kWelcomSurfaceShowLimit + 5];
  int run_loop_index = 0;

  // Never increase appearance count for welcome surface when there is no cart.
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit + 1; i++) {
    std::vector<chrome_cart::mojom::MerchantCartPtr> empty_carts;
    handler_->GetWarmWelcomeVisible(base::BindOnce(
        &CartHandlerTest::GetEvaluationBoolResult, base::Unretained(this),
        run_loop[run_loop_index].QuitClosure(), true));
    run_loop[run_loop_index++].Run();
    handler_->GetMerchantCarts(base::BindOnce(
        &GetEvaluationMerchantCarts, run_loop[run_loop_index].QuitClosure(),
        std::move(empty_carts)));
    run_loop[run_loop_index++].Run();
  }

  // Add a cart with product image.
  CartDB* cart_db_ = service_->GetDB();
  const char image_url[] = "www.image1.com";
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantBKey, kMockMerchantB, kMockMerchantURLB);
  merchant_proto.add_product_image_urls(image_url);
  cart_db_->AddCart(
      kMockMerchantBKey, merchant_proto,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this),
                     run_loop[run_loop_index].QuitClosure(), true));
  run_loop[run_loop_index++].Run();

  // Show welcome surface for the first three appearances.
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit; i++) {
    // Build a callback result without product image.
    auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
    dummy_cart1->merchant = kMockMerchantB;
    dummy_cart1->cart_url = GURL(kMockMerchantURLB);
    std::vector<chrome_cart::mojom::MerchantCartPtr> carts_without_product;
    carts_without_product.push_back(std::move(dummy_cart1));

    handler_->GetWarmWelcomeVisible(base::BindOnce(
        &CartHandlerTest::GetEvaluationBoolResult, base::Unretained(this),
        run_loop[run_loop_index].QuitClosure(), true));
    run_loop[run_loop_index++].Run();
    handler_->GetMerchantCarts(base::BindOnce(
        &GetEvaluationMerchantCarts, run_loop[run_loop_index].QuitClosure(),
        std::move(carts_without_product)));
    run_loop[run_loop_index++].Run();
  }

  // Build a callback result with product image.
  auto dummy_cart2 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart2->merchant = kMockMerchantB;
  dummy_cart2->cart_url = GURL(kMockMerchantURLB);
  dummy_cart2->product_image_urls.emplace_back(image_url);
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts_with_product;
  carts_with_product.push_back(std::move(dummy_cart2));

  // Not show welcome surface afterwards.
  handler_->GetWarmWelcomeVisible(base::BindOnce(
      &CartHandlerTest::GetEvaluationBoolResult, base::Unretained(this),
      run_loop[run_loop_index].QuitClosure(), false));
  run_loop[run_loop_index++].Run();
  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCarts, run_loop[run_loop_index].QuitClosure(),
      std::move(carts_with_product)));
  run_loop[run_loop_index++].Run();
}

// Verifies discount data not showing with RBD disabled.
TEST_F(CartHandlerNtpModuleTest, TestDiscountDataWithoutFeature) {
  base::RunLoop run_loop[7];
  int run_loop_index = 0;
  // Add a cart with discount.
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantBKey, kMockMerchantB, kMockMerchantURLB);
  merchant_proto.mutable_discount_info()->set_discount_text("15% off");
  service_->AddCart(kMockMerchantBKey, absl::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();

  // Skip the welcome surface stage as discount is not showing for welcome
  // surface.
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit; i++) {
    service_->IncreaseWelcomeSurfaceCounter();
  }
  ASSERT_FALSE(service_->ShouldShowWelcomeSurface());

  // Discount should not show in normal cart module with RBD disabled.
  auto expect_cart = chrome_cart::mojom::MerchantCart::New();
  expect_cart->merchant = kMockMerchantB;
  expect_cart->cart_url = GURL(kMockMerchantURLB);
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  carts.push_back(std::move(expect_cart));
  handler_->GetMerchantCarts(
      base::BindOnce(&GetEvaluationMerchantCarts,
                     run_loop[run_loop_index].QuitClosure(), std::move(carts)));
  run_loop[run_loop_index++].Run();
}

// Override CartHandlerTest so that we can initialize feature_list_ in our
// constructor, before CartHandlerTest::SetUp is called.
class CartHandlerNtpModuleDiscountTest : public CartHandlerTest {
 public:
  CartHandlerNtpModuleDiscountTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    feature_list_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{"NtpChromeCartModuleAbandonedCartDiscountParam", "true"},
         {"partner-merchant-pattern", "(foo.com)"}});
  }

  void SetUp() override {
    CartHandlerTest::SetUp();

    // Mock that welcome surface has already finished showing.
    for (int i = 0; i < CartService::kWelcomSurfaceShowLimit; i++) {
      service_->IncreaseWelcomeSurfaceCounter();
    }
    ASSERT_FALSE(service_->ShouldShowWelcomeSurface());
  }
};

// Test discount consent card visibility aligns with CartService.
// Flaky on multiple platforms: crbug.com/1256745
TEST_F(CartHandlerNtpModuleDiscountTest,
       DISABLED_TestGetDiscountConsentCardVisible) {
  CartDB* cart_db = service_->GetDB();
  base::RunLoop run_loop[5];
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  run_loop[0].Run();
  handler_->GetDiscountConsentCardVisible(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  // Add a partner cart.
  cart_db->AddCart(
      kFakeMerchant, kFakeProto,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();
  handler_->GetDiscountConsentCardVisible(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();
}

// Test OnDiscountConsentAcknowledged can update status in CartService.
// Flaky on multiple platforms: crbug.com/1256745
TEST_F(CartHandlerNtpModuleDiscountTest,
       DISABLED_TestOnDiscountConsentAcknowledged) {
  // Update fetch timestamp to avoid fetching triggered by consent
  // acknowledgement.
  profile_->GetPrefs()->SetTime(prefs::kCartDiscountLastFetchedTime,
                                base::Time::Now());
  CartDB* cart_db = service_->GetDB();
  base::RunLoop run_loop[4];
  // Add a partner cart.
  cart_db->AddCart(
      kFakeMerchant, kFakeProto,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  ASSERT_FALSE(service_->IsCartDiscountEnabled());

  handler_->OnDiscountConsentAcknowledged(true);
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[2].QuitClosure(), false));
  run_loop[2].Run();
  ASSERT_TRUE(service_->IsCartDiscountEnabled());

  handler_->OnDiscountConsentAcknowledged(false);
  service_->ShouldShowDiscountConsent(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[3].QuitClosure(), false));
  run_loop[3].Run();
  ASSERT_FALSE(service_->IsCartDiscountEnabled());
}

// Test GetDiscountEnabled returns whether rule-based discount feature is
// enabled.
TEST_F(CartHandlerNtpModuleDiscountTest, TestGetDiscountEnabled) {
  base::RunLoop run_loop[2];
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  ASSERT_TRUE(service_->IsCartDiscountEnabled());
  handler_->GetDiscountEnabled(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, false);
  ASSERT_FALSE(service_->IsCartDiscountEnabled());
  handler_->GetDiscountEnabled(
      base::BindOnce(&CartHandlerTest::GetEvaluationBoolResult,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();
}

// Test SetDiscountEnabled updates whether rule-based discount is enabled.
TEST_F(CartHandlerNtpModuleDiscountTest, TestSetDiscountEnabled) {
  ASSERT_FALSE(service_->IsCartDiscountEnabled());
  handler_->SetDiscountEnabled(true);
  ASSERT_TRUE(service_->IsCartDiscountEnabled());
  handler_->SetDiscountEnabled(false);
  ASSERT_FALSE(service_->IsCartDiscountEnabled());
}

// Verifies discount data showing with RBD enabled.
TEST_F(CartHandlerNtpModuleDiscountTest, TestDiscountDataWithFeature) {
  base::RunLoop run_loop[7];
  int run_loop_index = 0;
  // Add a cart with discount. Mock that welcome surface hasn't shown and RBD is
  // enabled.
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantBKey, kMockMerchantB, kMockMerchantURLB);
  merchant_proto.mutable_discount_info()->set_discount_text("15% off");
  cart_db::RuleDiscountInfoProto* rule_discount_info =
      merchant_proto.mutable_discount_info()->add_rule_discount_info();
  rule_discount_info->set_rule_id("123");
  service_->AddCart(kMockMerchantBKey, absl::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   0);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  // Discount should not show in welcome surface.
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit; i++) {
    // Build a callback result without discount.
    auto expect_cart = chrome_cart::mojom::MerchantCart::New();
    expect_cart->merchant = kMockMerchantB;
    expect_cart->cart_url = GURL(kMockMerchantURLB);
    std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
    carts.push_back(std::move(expect_cart));
    handler_->GetWarmWelcomeVisible(base::BindOnce(
        &CartHandlerTest::GetEvaluationBoolResult, base::Unretained(this),
        run_loop[run_loop_index].QuitClosure(), true));
    run_loop[run_loop_index++].Run();
    handler_->GetMerchantCarts(base::BindOnce(
        &GetEvaluationMerchantCarts, run_loop[run_loop_index].QuitClosure(),
        std::move(carts)));
    run_loop[run_loop_index++].Run();
  }

  // Discount should show in normal cart module with RBD enabled.
  auto expect_cart = chrome_cart::mojom::MerchantCart::New();
  expect_cart->merchant = kMockMerchantB;
  expect_cart->cart_url = GURL(kMockMerchantURLB);
  expect_cart->discount_text = "15% off";
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  carts.push_back(std::move(expect_cart));
  handler_->GetMerchantCarts(
      base::BindOnce(&GetEvaluationMerchantCarts,
                     run_loop[run_loop_index].QuitClosure(), std::move(carts)));
  run_loop[run_loop_index++].Run();
}

// Verifies discount data showing when coupons is available.
TEST_F(CartHandlerNtpModuleDiscountTest, TestDiscountDataShows) {
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   3);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

  base::RunLoop run_loop;

  // Add a cart with discount.
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantBKey, kMockMerchantB, kMockMerchantURLB);
  merchant_proto.mutable_discount_info()->set_discount_text("15% off");
  merchant_proto.mutable_discount_info()->set_has_coupons(true);
  service_->AddCart(kMockMerchantBKey, absl::nullopt, merchant_proto);
  task_environment_.RunUntilIdle();

  // Discount should show.
  auto expect_cart = chrome_cart::mojom::MerchantCart::New();
  expect_cart->merchant = kMockMerchantB;
  expect_cart->cart_url = GURL(kMockMerchantURLB);
  expect_cart->discount_text = "15% off";
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  carts.push_back(std::move(expect_cart));
  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCarts, run_loop.QuitClosure(), std::move(carts)));
  run_loop.Run();
}

// Override CartHandlerTest so that we can initialize feature_list_ in our
// constructor, before CartHandlerTest::SetUp is called.
class CartHandlerNtpModuleDiscountFastPathTest : public CartHandlerTest {
 public:
  CartHandlerNtpModuleDiscountFastPathTest() {
    // This needs to be called before any tasks that run on other threads check
    // if a feature is enabled.
    feature_list_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{"NtpChromeCartModuleAbandonedCartDiscountParam", "true"},
         {"partner-merchant-pattern", "(foo.com)"},
         {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountUseUtmParam,
          "true"}});
  }
};

// Verifies utm_source tag is appended to partner merchant's cart.
TEST_F(CartHandlerNtpModuleDiscountFastPathTest,
       TestAppendUtmSourceToPartnerMerchant) {
  base::RunLoop run_loop[2];
  service_->AddCart(kFakeMerchantKey, absl::nullopt, kFakeProto);
  task_environment_.RunUntilIdle();

  // Verifies utm_source tag for discount is disabled.
  ASSERT_FALSE(service_->IsCartDiscountEnabled());
  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCartWithUtmSource, run_loop[0].QuitClosure(), true,
      "utm_source=chrome_cart_no_rbd"));
  run_loop[0].Run();

  // Verifies utm_source tag for discount is enabled.
  handler_->SetDiscountEnabled(true);
  ASSERT_TRUE(service_->IsCartDiscountEnabled());
  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCartWithUtmSource, run_loop[1].QuitClosure(), true,
      "utm_source=chrome_cart_rbd"));
  run_loop[1].Run();
}

// Verifies utm_source tag is not appended to non partner merchant's cart.
TEST_F(CartHandlerNtpModuleDiscountFastPathTest,
       TestNotAppendUtmSourceToNonPartnerMerchant) {
  base::RunLoop run_loop;
  service_->AddCart(kMockMerchantBKey, absl::nullopt, kMockProtoB);
  task_environment_.RunUntilIdle();

  // Verifies utm_source tag is not appended.
  ASSERT_FALSE(service_->IsCartDiscountEnabled());
  handler_->GetMerchantCarts(
      base::BindOnce(&GetEvaluationMerchantCartWithUtmSource,
                     run_loop.QuitClosure(), false, ""));
  run_loop.Run();
}
