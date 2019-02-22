// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill_credit_card_collection_view_controller.h"

#include "base/guid.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/browser/ui/settings/personal_data_manager_data_changed_observer.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsRootCollectionViewController (ExposedForTesting)
- (void)editButtonPressed;
@end

namespace {

class AutofillCreditCardCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  AutofillCreditCardCollectionViewControllerTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    chrome_browser_state_->CreateWebDataService();
  }

  CollectionViewController* InstantiateController() override {
    return [[AutofillCreditCardCollectionViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  void AddCreditCard(const std::string& origin,
                     const std::string& card_holder_name,
                     const std::string& card_number) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    PersonalDataManagerDataChangedObserver observer(personal_data_manager);

    autofill::CreditCard credit_card(base::GenerateGUID(), origin);
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::ASCIIToUTF16(card_holder_name));
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                           base::ASCIIToUTF16(card_number));
    personal_data_manager->OnAcceptedLocalCreditCardSave(credit_card);
    observer.Wait();  // Wait for completion of the asynchronous operation.
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Default test case of no credit cards.
TEST_F(AutofillCreditCardCollectionViewControllerTest, TestInitialization) {
  CreateController();
  CheckController();

  // Expect one header section and one subtitle section.
  EXPECT_EQ(2, NumberOfSections());
  // Expect header section to contain one row (the credit card Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // Expect subtitle section to contain one row (the credit card Autofill toggle
  // subtitle).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Adding a single credit card results in a credit card section.
TEST_F(AutofillCreditCardCollectionViewControllerTest, TestOneCreditCard) {
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005");
  CreateController();
  CheckController();

  // Expect three sections (header, subtitle, and credit card section).
  EXPECT_EQ(3, NumberOfSections());
  // Expect credit card section to contain one row (the credit card itself).
  EXPECT_EQ(1, NumberOfItemsInSection(2));
}

// Deleting the only credit card results in item deletion and section deletion.
TEST_F(AutofillCreditCardCollectionViewControllerTest,
       TestOneCreditCardItemDeleted) {
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005");
  CreateController();
  CheckController();

  // Expect three sections (header, subtitle, and credit card section).
  EXPECT_EQ(3, NumberOfSections());
  // Expect credit card section to contain one row (the credit card itself).
  EXPECT_EQ(1, NumberOfItemsInSection(2));

  AutofillCreditCardCollectionViewController* view_controller =
      base::mac::ObjCCastStrict<AutofillCreditCardCollectionViewController>(
          controller());
  // Put the collectionView in 'edit' mode.
  [view_controller editButtonPressed];

  // This is a bit of a shortcut, since actually clicking on the 'delete'
  // button would be tough.
  void (^delete_item_with_wait)(int, int) = ^(int i, int j) {
    __block BOOL completion_called = NO;
    this->DeleteItem(i, j, ^{
      completion_called = YES;
    });
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool() {
          return completion_called;
        }));
  };

  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_browser_state_.get());
  PersonalDataManagerDataChangedObserver observer(personal_data_manager);

  // This call cause a modification of the PersonalDataManager, so wait until
  // the asynchronous task complete in addition to waiting for the UI update.
  delete_item_with_wait(2, 0);
  observer.Wait();  // Wait for completion of the asynchronous operation.

  // Exit 'edit' mode.
  [view_controller editButtonPressed];

  // Expect credit card section to have been removed.
  EXPECT_EQ(2, NumberOfSections());
}

}  // namespace
