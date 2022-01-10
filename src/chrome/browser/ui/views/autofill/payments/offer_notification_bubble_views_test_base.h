// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_TEST_BASE_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_event_waiter.h"

class CouponService;

namespace autofill {

namespace {
const int64_t kCreditCardInstrumentId = 0x4444;
}  // namespace

// Test base class for the OfferNotificationBubbleViews related tests. Provides
// helper function and common setups.
class OfferNotificationBubbleViewsTestBase
    : public InProcessBrowserTest,
      public OfferNotificationBubbleControllerImpl::ObserverForTest {
 public:
  // Various events that can be waited on by the DialogEventWaiter.
  enum class DialogEvent : int {
    BUBBLE_SHOWN,
  };

  explicit OfferNotificationBubbleViewsTestBase(
      bool promo_code_flag_enabled = true);
  ~OfferNotificationBubbleViewsTestBase() override;
  OfferNotificationBubbleViewsTestBase(
      const OfferNotificationBubbleViewsTestBase&) = delete;
  OfferNotificationBubbleViewsTestBase& operator=(
      const OfferNotificationBubbleViewsTestBase&) = delete;

  // InProcessBrowserTest::SetUpOnMainThread:
  void SetUpOnMainThread() override;

  // OfferNotificationBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override;

  // Also creates a credit card for the offer.
  std::unique_ptr<AutofillOfferData> CreateCardLinkedOfferDataWithDomains(
      const std::vector<GURL>& domains);

  std::unique_ptr<AutofillOfferData> CreatePromoCodeOfferDataWithDomains(
      const std::vector<GURL>& domains);

  void DeleteFreeListingCouponForUrl(const GURL& url);

  void SetUpOfferDataWithDomains(AutofillOfferData::OfferType offer_type,
                                 const std::vector<GURL>& domains);

  // Also creates a credit card for the offer.
  void SetUpCardLinkedOfferDataWithDomains(const std::vector<GURL>& domains);

  void SetUpFreeListingCouponOfferDataWithDomains(
      const std::vector<GURL>& domains);

  void SetUpFreeListingCouponOfferDataForCouponService(
      std::unique_ptr<AutofillOfferData> offer);

  void NavigateTo(const std::string& file_path);

  OfferNotificationBubbleViews* GetOfferNotificationBubbleViews();

  OfferNotificationIconView* GetOfferNotificationIconView();

  bool IsIconVisible();

  content::WebContents* GetActiveWebContents();

  void AddEventObserverToController();

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence);

  void UpdateFreeListingCouponDisplayTime(
      std::unique_ptr<AutofillOfferData> offer);

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  PersonalDataManager* personal_data() { return personal_data_; }

 protected:
  // Returns the string used for the default test promo code data, so that it
  // can be expected on UI elements if desired.
  std::string GetDefaultTestPromoCode() const;

 private:
  raw_ptr<PersonalDataManager> personal_data_;
  raw_ptr<CouponService> coupon_service_;
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_TEST_BASE_H_
