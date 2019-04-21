// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {

class PaymentsUtilTest : public testing::Test {
 public:
  PaymentsUtilTest() {}
  ~PaymentsUtilTest() override {}

 protected:
  TestPersonalDataManager personal_data_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsUtilTest);
};

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Normal) {
  base::HistogramTester histogram_tester;

  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_EQ(123456, GetBillingCustomerId(&personal_data_manager_,
                                         /*should_log_validity=*/true));

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::VALID, 1);
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Garbage) {
  base::HistogramTester histogram_tester;

  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"garbage"));

  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_,
                                    /*should_log_validity=*/true));

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::PARSE_ERROR, 1);
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_NoData) {
  base::HistogramTester histogram_tester;

  // Explictly do not set PaymentsCustomerData. Nothing crashes and the returned
  // customer ID is 0.
  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_,
                                    /*should_log_validity=*/true));
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::MISSING, 1);
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_Normal) {
  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_TRUE(HasGooglePaymentsAccount(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_NoData) {
  // Explicitly do not set Prefs data. Nothing crashes and returns false.
  EXPECT_FALSE(HasGooglePaymentsAccount(&personal_data_manager_));
}

}  // namespace payments
}  // namespace autofill
