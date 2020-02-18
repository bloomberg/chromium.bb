// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator_delegate.h"
#include "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class AutofillAddCreditCardMediatorTest : public PlatformTest {
 protected:
  AutofillAddCreditCardMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    chrome_browser_state_->CreateWebDataService();
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());

    add_credit_card_mediator_delegate_mock_ =
        OCMProtocolMock(@protocol(AddCreditCardMediatorDelegate));

    add_credit_card_mediator_ = [[AutofillAddCreditCardMediator alloc]
           initWithDelegate:add_credit_card_mediator_delegate_mock_
        personalDataManager:personal_data_manager_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  autofill::PersonalDataManager* personal_data_manager_;
  AutofillAddCreditCardMediator* add_credit_card_mediator_;
  id add_credit_card_mediator_delegate_mock_;
};

// Test saving a credit card with invalid card number.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestSavingCreditCardWithInvalidNumber) {
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager_);

  // |creditCardMediatorHasInvalidCardNumber|expected to be called by
  // |add_credit_card_mediator_| if the credit card has invalid number.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorHasInvalidCardNumber:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:@"Test"
                       cardNumber:@"4111111111111112"  // This is invalid
                                                       // Card number.
                  expirationMonth:@"11"
                   expirationYear:@"2020"];

  waiter.Wait();  // Wait for completion of the asynchronous operation.

  int number_of_credit_cards = personal_data_manager_->GetCreditCards().size();

  // A credit card with invalid number shouldn't be saved so the number of
  // credit cards has to equal zero.
  EXPECT_EQ(number_of_credit_cards, 0);

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving a credit card with invalid expiration month.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestSavingCreditCardWithInvalidMonth) {
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager_);

  // |creditCardMediatorHasInvalidExpirationDate| expected to be called by
  // |add_credit_card_mediator_| if the credit card has invalid expiration date.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorHasInvalidExpirationDate:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:@"Test"
                       cardNumber:@"4111111111111111"
                  expirationMonth:@"15"  // This is invalid month.
                   expirationYear:@"2020"];

  waiter.Wait();  // Wait for completion of the asynchronous operation.

  //  A credit card with invalid expiration date shouldn't be saved so the
  //  number of credit cards has to equal zero.
  int number_of_credit_cards = personal_data_manager_->GetCreditCards().size();
  EXPECT_EQ(number_of_credit_cards, 0);

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving a credit card with invalid expiration year.
TEST_F(AutofillAddCreditCardMediatorTest, TestSavingCreditCardWithInvalidYear) {
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager_);

  // |creditCardMediatorHasInvalidExpirationDate| expected to be called by
  // |add_credit_card_mediator_| if the credit card has invalid expiration date.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorHasInvalidExpirationDate:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:@"Test"
                       cardNumber:@"4111111111111111"
                  expirationMonth:@"15"
                   expirationYear:@"2010"];  // This is invalid year.

  waiter.Wait();  // Wait for completion of the asynchronous operation.

  // A credit card with invalid expiration date shouldn't be saved so the number
  // of credit cards has to equal zero.
  int number_of_credit_cards = personal_data_manager_->GetCreditCards().size();
  EXPECT_EQ(number_of_credit_cards, 0);

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving a valid credit card.
TEST_F(AutofillAddCreditCardMediatorTest, TestSavingValidCreditCard) {
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager_);

  // |creditCardMediatorDidFinish| expected to be called by
  // |add_credit_card_mediator_| if the credit card has valid data.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorDidFinish:[OCMArg any]]);

  [add_credit_card_mediator_ addCreditCardViewController:nil
                             addCreditCardWithHolderName:@"Test"
                                              cardNumber:@"4111111111111111"
                                         expirationMonth:@"11"
                                          expirationYear:@"2020"];

  waiter.Wait();  // Wait for completion of the asynchronous operation.

  // A valid credit card expected to be savd so the number of credit cards has
  // to equal one.
  int number_of_credit_cards = personal_data_manager_->GetCreditCards().size();
  EXPECT_EQ(number_of_credit_cards, 1);

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving duplicated credit card with the same card number.
TEST_F(AutofillAddCreditCardMediatorTest, TestAlreadyExistsCreditCardNumber) {
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager_);

  // |creditCardMediatorDidFinish| expected to be called by
  // |add_credit_card_mediator_| if the credit card has valid data.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorDidFinish:[OCMArg any]]);

  [add_credit_card_mediator_ addCreditCardViewController:nil
                             addCreditCardWithHolderName:@"Test2"
                                              cardNumber:@"4111111111111111"
                                         expirationMonth:@"12"
                                          expirationYear:@"2022"];

  waiter.Wait();  // Wait for completion of the asynchronous operation.

  // A duplicated credit card expected to be updated not saved as new one so the
  // number of credit cards has to remain eqal one.
  int number_of_credit_cards = personal_data_manager_->GetCreditCards().size();
  EXPECT_EQ(number_of_credit_cards, 1);

  autofill::CreditCard* savedCreditCard =
      personal_data_manager_->GetCreditCardByNumber("4111111111111111");

  // Test if the credit card data was replaced by the new data.
  EXPECT_TRUE(savedCreditCard);

  EXPECT_EQ(savedCreditCard->Expiration2DigitMonthAsString(),
            base::SysNSStringToUTF16(@"12"));

  EXPECT_EQ(savedCreditCard->Expiration2DigitMonthAsString(),
            base::SysNSStringToUTF16(@"12"));

  EXPECT_EQ(savedCreditCard->Expiration4DigitYearAsString(),
            base::SysNSStringToUTF16(@"2022"));

  [add_credit_card_mediator_delegate_mock_ verify];
}
