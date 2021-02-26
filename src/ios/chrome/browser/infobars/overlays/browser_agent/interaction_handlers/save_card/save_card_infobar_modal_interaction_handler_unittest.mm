// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_modal_interaction_handler.h"

#include <string>

#include "base/guid.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/infobars/core/infobar_feature.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for SaveCardInfobarModalInteractionHandler.
class SaveCardInfobarModalInteractionHandlerTest : public PlatformTest {
 public:
  SaveCardInfobarModalInteractionHandlerTest()
      : delegate_factory_(),
        prefs_(autofill::test::PrefServiceForTesting()),
        card_(base::GenerateGUID(), "https://www.example.com/") {
    scoped_feature_list_.InitWithFeatures({kIOSInfobarUIReboot},
                                          {kInfobarUIRebootOnlyiOS13});
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveCard,
        MockAutofillSaveCardInfoBarDelegateMobileFactory::
            CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
                false, prefs_.get(), card_));
  }

  MockAutofillSaveCardInfoBarDelegateMobile& mock_delegate() {
    return *static_cast<MockAutofillSaveCardInfoBarDelegateMobile*>(
        infobar_->delegate());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  SaveCardInfobarModalInteractionHandler handler_;
  MockAutofillSaveCardInfoBarDelegateMobileFactory delegate_factory_;
  std::unique_ptr<PrefService> prefs_;
  autofill::CreditCard card_;
  std::unique_ptr<InfoBarIOS> infobar_;
};

TEST_F(SaveCardInfobarModalInteractionHandlerTest, UpdateCredentials) {
  base::string16 cardholder_name = base::SysNSStringToUTF16(@"test name");
  base::string16 expiration_date_month = base::SysNSStringToUTF16(@"06");
  base::string16 expiration_date_year = base::SysNSStringToUTF16(@"2023");
  EXPECT_CALL(mock_delegate(),
              UpdateAndAccept(cardholder_name, expiration_date_month,
                              expiration_date_year));
  handler_.UpdateCredentials(infobar_.get(), cardholder_name,
                             expiration_date_month, expiration_date_year);
}

TEST_F(SaveCardInfobarModalInteractionHandlerTest, LoadURL) {
  GURL url("https://test-example.com");
  EXPECT_CALL(mock_delegate(), OnLegalMessageLinkClicked(url));
  handler_.LoadURL(infobar_.get(), url);
}
