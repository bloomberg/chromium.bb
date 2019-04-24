// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card_save_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/non_client_view.h"
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#endif

using base::Bucket;
using testing::ElementsAre;

namespace {
const char kCreditCardUploadForm[] =
    "/credit_card_upload_form_address_and_cc.html";
const char kCreditCardAndShippingUploadForm[] =
    "/credit_card_upload_form_shipping_address.html";
const char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
const char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
const char kResponsePaymentsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";
const char kURLUploadCardRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/savecard";

const double kFakeGeolocationLatitude = 1.23;
const double kFakeGeolocationLongitude = 4.56;
}  // namespace

namespace autofill {

class SaveCardBubbleViewsFullFormBrowserTest
    : public SyncTest,
      public CreditCardSaveManager::ObserverForTest,
      public SaveCardBubbleControllerImpl::ObserverForTest {
 protected:
  SaveCardBubbleViewsFullFormBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  ~SaveCardBubbleViewsFullFormBrowserTest() override = default;

  // Various events that can be waited on by the DialogEventWaiter.
  enum DialogEvent : int {
    OFFERED_LOCAL_SAVE,
    REQUESTED_UPLOAD_SAVE,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_UPLOAD_CARD_REQUEST,
    RECEIVED_UPLOAD_CARD_RESPONSE,
    STRIKE_CHANGE_COMPLETE,
    BUBBLE_SHOWN,
    BUBBLE_CLOSED
  };

  // SyncTest::SetUpOnMainThread:
  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    // Set up the HTTPS server (uses the embedded_test_server).
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
        browser()->profile())
        ->OverrideNetworkResourcesForTest(
            std::make_unique<fake_server::FakeServerNetworkResources>(
                GetFakeServer()->AsWeakPtr()));

    std::string username;
#if defined(OS_CHROMEOS)
    // In ChromeOS browser tests, the profile may already by authenticated with
    // stub account |user_manager::kStubUserEmail|.
    CoreAccountInfo info =
        IdentityManagerFactory::GetForProfile(browser()->profile())
            ->GetPrimaryAccountInfo();
    username = info.email;
#endif
    if (username.empty())
      username = "user@gmail.com";

    harness_ = ProfileSyncServiceHarness::Create(
        browser()->profile(), username, "password",
        ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

    // Set up the URL loader factory for the payments client so we can intercept
    // those network requests too.
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    ContentAutofillDriver::GetForRenderFrameHost(
        GetActiveWebContents()->GetMainFrame())
        ->autofill_manager()
        ->client()
        ->GetPaymentsClient()
        ->set_url_loader_factory_for_testing(test_shared_loader_factory_);

    // Set up this class as the ObserverForTest implementation.
    CreditCardSaveManager* credit_card_save_manager =
        ContentAutofillDriver::GetForRenderFrameHost(
            GetActiveWebContents()->GetMainFrame())
            ->autofill_manager()
            ->client()
            ->GetFormDataImporter()
            ->credit_card_save_manager_.get();
    credit_card_save_manager->SetEventObserverForTesting(this);

    // Set up the fake geolocation data.
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            kFakeGeolocationLatitude, kFakeGeolocationLongitude);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnOfferLocalSave() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::OFFERED_LOCAL_SAVE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnDecideToRequestUploadSave() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::REQUESTED_UPLOAD_SAVE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnReceivedGetUploadDetailsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnSentUploadCardRequest() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::SENT_UPLOAD_CARD_REQUEST);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnReceivedUploadCardResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_UPLOAD_CARD_RESPONSE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnStrikeChangeComplete() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::STRIKE_CHANGE_COMPLETE);
  }

  // SaveCardBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
  }

  // SaveCardBubbleControllerImpl::ObserverForTest:
  void OnBubbleClosed() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::BUBBLE_CLOSED);
  }

  inline views::Combobox* month_input() {
    return static_cast<views::Combobox*>(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_MONTH));
  }

  inline views::Combobox* year_input() {
    return static_cast<views::Combobox*>(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_YEAR));
  }

  void VerifyExpirationDateDropdownsAreVisible() {
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                    ->visible());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_VIEW)->visible());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_YEAR)
                    ->visible());
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_MONTH)
            ->visible());
    EXPECT_FALSE(FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_LABEL));
  }

  void SetUpForEditableExpirationDate() {
    // Enable the EditableExpirationDate experiment.
    scoped_feature_list_.InitWithFeatures(
        // Enabled
        {features::kAutofillUpstreamEditableExpirationDate,
         features::kAutofillUpstream},
        // Disabled
        {});

    // Start sync.
    harness_->SetupSync();

    // Set up the Payments RPC.
    SetUploadDetailsRpcPaymentsAccepts();

    // Submitting the form should still show the upload save bubble and legal
    // footer, along with a pair of dropdowns specifically requesting the
    // expiration date. (Must wait for response from Payments before accessing
    // the controller.)
    ResetEventWaiterForSequence(
        {DialogEvent::REQUESTED_UPLOAD_SAVE,
         DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
    NavigateTo(kCreditCardUploadForm);
  }

  void NavigateTo(const std::string& file_path) {
    if (file_path.find("data:") == 0U) {
      ui_test_utils::NavigateToURL(browser(), GURL(file_path));
    } else {
      ui_test_utils::NavigateToURL(browser(),
                                   embedded_test_server()->GetURL(file_path));
    }
  }

  void SetAccountFullName(const std::string& full_name) {
    identity::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    CoreAccountInfo core_info = identity_manager->GetPrimaryAccountInfo();
    AccountInfo account_info;
    account_info.account_id = core_info.account_id;
    account_info.gaia = core_info.gaia;
    account_info.email = core_info.email;
    account_info.is_under_advanced_protection =
        core_info.is_under_advanced_protection;
    account_info.full_name = full_name;
    identity::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  void SubmitForm() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitForm() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));
    SubmitForm();
  }

  void FillAndSubmitFormWithCardDetailsOnly() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_card_button_js =
        "(function() { document.getElementById('fill_card_only').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_fill_card_button_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithoutCvc() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_cvc_button_js =
        "(function() { document.getElementById('clear_cvc').click(); })();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_clear_cvc_button_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithInvalidCvc() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_fill_invalid_cvc_button_js =
        "(function() { document.getElementById('fill_invalid_cvc').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_fill_invalid_cvc_button_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithoutName() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_name_button_js =
        "(function() { document.getElementById('clear_name').click(); })();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_clear_name_button_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_shipping_address.html.
  void FillAndSubmitFormWithConflictingName() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_conflicting_name_button_js =
        "(function() { document.getElementById('conflicting_name').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_conflicting_name_button_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithoutExpirationDate() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents,
                                       click_clear_expiration_date_button_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithExpirationMonthOnly(const std::string& month) {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents,
                                       click_clear_expiration_date_button_js));

    std::string set_month_js =
        "(function() { document.getElementById('cc_month_exp_id').value =" +
        month + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_month_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithExpirationYearOnly(const std::string& year) {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents,
                                       click_clear_expiration_date_button_js));

    std::string set_year_js =
        "(function() { document.getElementById('cc_year_exp_id').value =" +
        year + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_year_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithSpecificExpirationDate(const std::string& month,
                                                   const std::string& year) {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    std::string set_month_js =
        "(function() { document.getElementById('cc_month_exp_id').value =" +
        month + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_month_js));

    std::string set_year_js =
        "(function() { document.getElementById('cc_year_exp_id').value =" +
        year + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_year_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillAndSubmitFormWithoutAddress() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_address_button_js =
        "(function() { document.getElementById('clear_address').click(); })();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_clear_address_button_js));

    SubmitForm();
  }

  // Should be called for credit_card_upload_form_shipping_address.html.
  void FillAndSubmitFormWithConflictingPostalCode() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_conflicting_postal_code_button_js =
        "(function() { "
        "document.getElementById('conflicting_postal_code').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(
        web_contents, click_conflicting_postal_code_button_js));

    SubmitForm();
  }

  void SetUploadDetailsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess);
  }

  void SetUploadDetailsRpcPaymentsDeclines() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponsePaymentsFailure);
  }

  void SetUploadDetailsRpcServerError() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess,
                                           net::HTTP_INTERNAL_SERVER_ERROR);
  }

  void SetUploadCardRpcPaymentsFails() {
    test_url_loader_factory()->AddResponse(kURLUploadCardRequest,
                                           kResponsePaymentsFailure);
  }

  void ClickOnView(views::View* view) {
    DCHECK(view);
    ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(released_event);
  }

  void ClickOnDialogView(views::View* view) {
    GetSaveCardBubbleViews()
        ->GetDialogClientView()
        ->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(GetSaveCardBubbleViews()
                                                 ->GetWidget()
                                                 ->non_client_view()
                                                 ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
  }

  void ClickOnDialogViewAndWait(views::View* view) {
    EXPECT_TRUE(GetSaveCardBubbleViews());
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetSaveCardBubbleViews()->GetWidget());
    ClickOnDialogView(view);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetSaveCardBubbleViews());
  }

  void ClickOnDialogViewWithId(DialogViewId view_id) {
    ClickOnDialogView(FindViewInBubbleById(view_id));
  }

  void ClickOnDialogViewWithIdAndWait(DialogViewId view_id) {
    ClickOnDialogViewAndWait(FindViewInBubbleById(view_id));
  }

  views::View* FindViewInBubbleById(DialogViewId view_id) {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    DCHECK(save_card_bubble_views);

    views::View* specified_view =
        save_card_bubble_views->GetViewByID(static_cast<int>(view_id));

    if (!specified_view) {
      // Many of the save card bubble's inner Views are not child views but
      // rather contained by its DialogClientView. If we didn't find what we
      // were looking for, check there as well.
      specified_view =
          save_card_bubble_views->GetDialogClientView()->GetViewByID(
              static_cast<int>(view_id));
    }
    if (!specified_view) {
      // Additionally, the save card bubble's footnote view is not part of its
      // main bubble, and contains elements such as the legal message links.
      // If we didn't find what we were looking for, check there as well.
      views::View* footnote_view =
          save_card_bubble_views->GetFootnoteViewForTesting();
      if (footnote_view) {
        specified_view = footnote_view->GetViewByID(static_cast<int>(view_id));
      }
    }

    return specified_view;
  }

  void ClickOnCancelButton(bool strike_expected = false) {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    DCHECK(save_card_bubble_views);
    if (strike_expected &&
        (base::FeatureList::IsEnabled(
             features::kAutofillSaveCreditCardUsesStrikeSystem) ||
         base::FeatureList::IsEnabled(
             features::kAutofillSaveCreditCardUsesStrikeSystemV2))) {
      ResetEventWaiterForSequence(
          {DialogEvent::STRIKE_CHANGE_COMPLETE, DialogEvent::BUBBLE_CLOSED});
    } else {
      ResetEventWaiterForSequence({DialogEvent::BUBBLE_CLOSED});
    }
    ClickOnDialogViewWithIdAndWait(DialogViewId::CANCEL_BUTTON);
    DCHECK(!GetSaveCardBubbleViews());
  }

  void ClickOnCloseButton() {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    DCHECK(save_card_bubble_views);
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_CLOSED});
    ClickOnDialogViewAndWait(
        save_card_bubble_views->GetBubbleFrameView()->GetCloseButtonForTest());
    DCHECK(!GetSaveCardBubbleViews());
  }

  SaveCardBubbleViews* GetSaveCardBubbleViews() {
    SaveCardBubbleControllerImpl* save_card_bubble_controller_impl =
        SaveCardBubbleControllerImpl::FromWebContents(GetActiveWebContents());
    if (!save_card_bubble_controller_impl)
      return nullptr;
    SaveCardBubbleView* save_card_bubble_view =
        save_card_bubble_controller_impl->save_card_bubble_view();
    if (!save_card_bubble_view)
      return nullptr;
    return static_cast<SaveCardBubbleViews*>(save_card_bubble_view);
  }

  SaveCardIconView* GetSaveCardIconView() {
    if (!browser())
      return nullptr;
    LocationBarView* location_bar_view =
        static_cast<LocationBarView*>(browser()->window()->GetLocationBar());
    DCHECK(location_bar_view->save_credit_card_icon_view());
    return location_bar_view->save_credit_card_icon_view();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void AddEventObserverToController() {
    SaveCardBubbleControllerImpl* save_card_bubble_controller_impl =
        SaveCardBubbleControllerImpl::FromWebContents(GetActiveWebContents());
    DCHECK(save_card_bubble_controller_impl);
    save_card_bubble_controller_impl->SetEventObserverForTesting(this);
  }

  void ReduceAnimationTime() {
    GetSaveCardIconView()->ReduceAnimationTimeForTesting();
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ProfileSyncServiceHarness> harness_;

 private:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleViewsFullFormBrowserTest);
};

// Tests the local save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble) {
  // Disable the sign-in promo.
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Clicking [Save] should accept and close it.
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);

  // No bubble should be showing and no sign-in impression should have been
  // recorded.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_FALSE(GetSaveCardIconView()->visible());
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}

// Tests the local save bubble. Ensures that clicking the [No thanks] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingNoThanksClosesBubble) {
  // Enable the updated UI.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillSaveCardImprovedUserConsent},
      // Disabled
      {features::kAutofillSaveCreditCardUsesStrikeSystem,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2});

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton();

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

// Tests the sign in promo bubble. Ensures that clicking the [Save] button
// on the local save bubble successfully causes the sign in promo to show.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingSaveShowsSigninPromo) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Sign-in promo should be showing and user actions should have recorded
  // impression.
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::SIGN_IN_PROMO_VIEW)->visible());
}
#endif

// Tests the sign in promo bubble. Ensures that the sign-in promo
// is not shown when the user is signed-in and syncing, even if the local save
// bubble is shown.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_NoSigninPromoShowsWhenUserIsSyncing) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillSaveCardSignInAfterLocalSave},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Click [Save] should close the offer-to-save bubble
  // but no sign-in promo should show because user is signed in.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // No bubble should be showing and no sign-in impression should have been
  // recorded.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}

// Tests the sign in promo bubble. Ensures that signin impression is recorded
// when promo is shown.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_SigninImpressionSigninPromo) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // User actions should have recorded impression.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}
#endif

// Tests the sign in promo bubble. Ensures that signin action is recorded when
// user accepts promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_AcceptingSigninPromo) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Click on [Sign in] button.
  ClickOnDialogView(static_cast<DiceBubbleSyncPromoView*>(
                        FindViewInBubbleById(DialogViewId::SIGN_IN_VIEW))
                        ->GetSigninButtonForTesting());

  // User actions should have recorded impression and click.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromSaveCardBubble"));
}
#endif

// Tests the manage cards bubble. Ensures that it shows up by clicking the
// credit card icon.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingIconShowsManageCards) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Bubble should be showing.
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MANAGE_CARDS_VIEW)->visible());
  histogram_tester.ExpectUniqueSample("Autofill.ManageCardsPrompt.Local",
                                      AutofillMetrics::MANAGE_CARDS_SHOWN, 1);
}

// Tests the manage cards bubble. Ensures that clicking the [Manage cards]
// button redirects properly.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ManageCardsButtonRedirects) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on the redirect button.
  ClickOnDialogViewWithId(DialogViewId::MANAGE_CARDS_BUTTON);

#if defined(OS_CHROMEOS)
  // ChromeOS should have opened up the settings window.
  EXPECT_TRUE(
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          browser()->profile()));
#else
  // Otherwise, another tab should have opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
#endif

  // Metrics should have been recorded correctly.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, 1)));
}

// Tests the manage cards bubble. Ensures that clicking the [Done]
// button closes the bubble.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ManageCardsDoneButtonClosesBubble) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on the [Done] button.
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // No bubble should be showing now and metrics should be recorded correctly.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_DONE, 1)));
}

// Tests the manage cards bubble. Ensures that sign-in impression is recorded
// correctly.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_SigninImpressionManageCards) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Close promo.
  ClickOnCloseButton();

  // Open up Manage Cards prompt.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // User actions should have recorded impression.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromManageCardsBubble"));
}
#endif

// Tests the Manage Cards bubble. Ensures that signin action is recorded when
// user accepts footnote promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_AcceptingFootnotePromoManageCards) {
  // Enable the sign-in promo.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCardSignInAfterLocalSave);

  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Close promo.
  ClickOnCloseButton();

  // Open up Manage Cards prompt.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on [Sign in] button in footnote.
  ClickOnDialogView(static_cast<DiceBubbleSyncPromoView*>(
                        FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW))
                        ->GetSigninButtonForTesting());

  // User actions should have recorded impression and click.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromManageCardsBubble"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Signin_FromManageCardsBubble"));
}
#endif

// Tests the local save bubble. Ensures that the bubble does not have a
// [No thanks] button (it has an [X] Close button instead.)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ShouldNotHaveNoThanksButton) {
  // Disable the updated UI.
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillSaveCardImprovedUserConsent);
  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Assert that the cancel button cannot be found.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CANCEL_BUTTON));
}

// Tests the local save bubble. Ensures that the bubble behaves correctly if
// dismissed and then immediately torn down (e.g. by closing browser window)
// before the asynchronous close completes. Regression test for
// https://crbug.com/842577 .
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_SynchronousCloseAfterAsynchronousClose) {
  // Submitting the form without signed in user should show the local save
  // bubble.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();

  SaveCardBubbleViews* bubble = GetSaveCardBubbleViews();
  EXPECT_TRUE(bubble);
  views::Widget* bubble_widget = bubble->GetWidget();
  EXPECT_TRUE(bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());
  bubble->Hide();
  EXPECT_FALSE(bubble_widget->IsVisible());

  // The bubble is immediately hidden, but it can still receive events here.
  // Simulate an OS event arriving to destroy the Widget.
  bubble_widget->CloseNow();
  // |bubble| and |bubble_widget| now point to deleted objects.

  // Simulate closing the browser window.
  browser()->tab_strip_model()->CloseAllTabs();

  // Process the asynchronous close (which should do nothing).
  base::RunLoop().RunUntilIdle();
}

// Tests the upload save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away and sends an UploadCardRequest RPC
// to Google Payments.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ClickingSaveClosesBubble) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking [Save] should accept and close it, then send an UploadCardRequest
  // to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

// Tests the upload save bubble. Ensures that clicking the [No thanks] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ClickingNoThanksClosesBubble) {
  // Enable the updated UI.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillSaveCardImprovedUserConsent,
       features::kAutofillUpstream},
      // Disabled
      {features::kAutofillSaveCreditCardUsesStrikeSystem,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton();

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

// Tests the upload save bubble. Ensures that the bubble does not have a
// [No thanks] button (it has an [X] Close button instead.)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ShouldNotHaveNoThanksButton) {
  // Disable the updated UI.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream},
      // Disable the updated UI.
      {features::kAutofillSaveCardImprovedUserConsent});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Assert that the cancel button cannot be found.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CANCEL_BUTTON));
}

// Tests the upload save bubble. Ensures that clicking the top-right [X] close
// button successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ClickingCloseClosesBubble) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();
}

// Tests the upload save bubble. Ensures that the bubble does not surface the
// cardholder name textfield if it is not needed.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ShouldNotRequestCardholderNameInHappyPath) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Assert that cardholder name was not explicitly requested in the bubble.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a textfield
// requesting cardholder name if cardholder name is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithMissingNamesRequestsCardholderNameIfExpOn) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a textfield
// requesting cardholder name if cardholder name is conflicting.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithConflictingNamesRequestsCardholderNameIfExpOn) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submit first shipping address form with a conflicting name.
  NavigateTo(kCreditCardAndShippingUploadForm);
  FillAndSubmitFormWithConflictingName();

  // Submitting the second form should still show the upload save bubble and
  // legal footer, along with a textfield requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitForm();

  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that if the cardholder name textfield
// is empty, the user is not allowed to click [Save] and close the dialog.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SaveButtonIsDisabledIfNoCardholderNameAndCardholderNameRequested) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clearing out the cardholder name textfield should disable the [Save]
  // button.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(base::ASCIIToUTF16(""));
  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  // Setting a cardholder name should enable the [Save] button.
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("John Smith"));
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_NORMAL);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, filling it and clicking [Save] closes the dialog.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_EnteringCardholderNameAndClickingSaveClosesBubbleIfCardholderNameRequested) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Entering a cardholder name and clicking [Save] should accept and close
  // the bubble, then send an UploadCardRequest to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("John Smith"));
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance for both regular save UMA and
  // the ".RequestingCardholderName" subhistogram.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, it is prefilled with the name from the user's Google Account.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_RequestedCardholderNameTextfieldIsPrefilledWithFocusName) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  base::HistogramTester histogram_tester;
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be prefilled with the name on the user's Google
  // Account, and UMA should have logged its value's existence. Because the
  // textfield has a value, the tooltip explaining that the name came from the
  // user's Google Account should also be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_EQ(cardholder_name_textfield->text(),
            base::ASCIIToUTF16("John Smith"));
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", true, 1);
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested but the name on the user's Google Account is unable to be fetched
// for any reason, the textfield is left blank.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_RequestedCardholderNameTextfieldIsNotPrefilledWithFocusNameIfMissing) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Don't sign the user in. In this case, the user's Account cannot be fetched
  // and their name is not available.

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  base::HistogramTester histogram_tester;
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be blank, and UMA should have logged its value's
  // absence. Because the textfield is blank, the tooltip explaining that the
  // name came from the user's Google Account should NOT be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_TRUE(cardholder_name_textfield->text().empty());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", false, 1);
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog without changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_CardholderNameRequested_SubmittingPrefilledValueLogsUneditedMetric) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clicking [Save] should accept and close the bubble, logging that the name
  // was not edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", false, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog after changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_CardholderNameRequested_SubmittingChangedValueLogsEditedMetric) {
  // Enable the EditableCardholderName experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Changing the name then clicking [Save] should accept and close the bubble,
  // logging that the name was edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("Jane Doe"));
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", true, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested but the AutofillUpstreamBlankCardholderNameField experiment is
// active, the textfield is NOT prefilled even though the user's Google Account
// name is available.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_CardholderNameNotPrefilledIfBlankNameExperimentEnabled) {
  // Enable the EditableCardholderName and BlankCardholderNameField experiments.
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillUpstream,
       features::kAutofillUpstreamEditableCardholderName,
       features::kAutofillUpstreamBlankCardholderNameField},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble, along with a
  // textfield specifically requesting the cardholder name.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  base::HistogramTester histogram_tester;
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be blank, and UMA should have logged its value's
  // absence. Because the textfield is blank, the tooltip explaining that the
  // name came from the user's Google Account should NOT be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_TRUE(cardholder_name_textfield->text().empty());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", false, 1);
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// TODO(jsaul): Only *part* of the legal message StyledLabel is clickable, and
//              the NOTREACHED() in SaveCardBubbleViews::StyledLabelLinkClicked
//              prevents us from being able to click it unless we know the exact
//              gfx::Range of the link. When/if that can be worked around,
//              create an Upload_ClickingTosLinkClosesBubble test.

// Tests the upload save logic. Ensures that Chrome offers a local save when the
// data is complete, even if Payments rejects the data.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldOfferLocalSaveIfPaymentsDeclines) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());
}

// Tests the upload save logic. Ensures that Chrome offers a local save when the
// data is complete, even if the Payments upload fails unexpectedly.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldOfferLocalSaveIfPaymentsFails) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcServerError();

  // Submitting the form and having the call to Payments fail should show the
  // local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());
}

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and offers to upload save the card if Payments allows it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Logic_CanOfferToSaveEvenIfNothingFoundIfPaymentsAccepts) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form, even with only card number and expiration date, should
  // start the flow of asking Payments if Chrome should offer to save the card
  // to Google. In this case, Payments says yes, and the offer to save bubble
  // should be shown.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithCardDetailsOnly();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
}

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and still does not surface the offer to upload save dialog
// if Payments declines it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Logic_ShouldNotOfferToSaveIfNothingFoundAndPaymentsDeclines) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form, even with only card number and expiration date, should
  // start the flow of asking Payments if Chrome should offer to save the card
  // to Google. In this case, Payments says no, so the offer to save bubble
  // should not be shown.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithCardDetailsOnly();
  WaitForObservedEvent();
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if CVC is not detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfCvcNotFound) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though CVC is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutCvc();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if the detected CVC is invalid.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfInvalidCvcFound) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the provided
  // CVC is invalid.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithInvalidCvc();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if address/cardholder name is not
// detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfNameNotFound) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though name is
  // missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutName();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting names are
// detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfNamesConflict) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Submit first shipping address form with a conflicting name.
  NavigateTo(kCreditCardAndShippingUploadForm);
  FillAndSubmitFormWithConflictingName();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the name
  // conflicts with the previous form.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if billing address is not detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfAddressNotFound) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though billing address
  // is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutAddress();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting billing address
// postal codes are detected.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Logic_ShouldAttemptToOfferToSaveIfPostalCodesConflict) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Submit first shipping address form with a conflicting postal code.
  NavigateTo(kCreditCardAndShippingUploadForm);
  FillAndSubmitFormWithConflictingPostalCode();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the postal code
  // conflicts with the previous form.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillAndSubmitForm();
  WaitForObservedEvent();
}

// Tests UMA logging for the upload save bubble. Ensures that if the user
// declines upload, Autofill.UploadAcceptedCardOrigin is not logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_DecliningUploadDoesNotLogUserAcceptedCardOriginUMA) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();

  // Ensure that UMA was logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.UploadOfferedCardOrigin",
      AutofillMetrics::OFFERING_UPLOAD_OF_NEW_CARD, 1);
  histogram_tester.ExpectTotalCount("Autofill.UploadAcceptedCardOrigin", 0);
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithMissingExpirationDateRequestsExpirationDate) {
  SetUpForEditableExpirationDate();
  FillAndSubmitFormWithoutExpirationDate();
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date is expired.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithExpiredExpirationDateRequestsExpirationDate) {
  SetUpForEditableExpirationDate();
  FillAndSubmitFormWithSpecificExpirationDate("08", "2000");
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();
}

// Tests the upload save bubble. Ensures that the bubble is not shown when
// expiration date is passed, but the flag is disabled.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Logic_ShouldNotOfferToSaveIfSubmittingExpiredExpirationDateAndExpOff) {
  // Disable the EditableExpirationDate experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream},
      // Disabled
      {features::kAutofillUpstreamEditableExpirationDate});

  // The credit card will not be imported if the expiration date is expired and
  // experiment is off.
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithSpecificExpirationDate("08", "2000");
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

// Tests the upload save bubble. Ensures that the bubble is not shown when
// expiration date is missing, but the flag is disabled.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Logic_ShouldNotOfferToSaveIfMissingExpirationDateAndExpOff) {
  // Disable the EditableExpirationDate experiment.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream},
      // Disabled
      {features::kAutofillUpstreamEditableExpirationDate});

  // The credit card will not be imported if there is no expiration date and
  // experiment is off.
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitFormWithoutExpirationDate();
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

// Tests the upload save bubble. Ensures that the bubble does not surface the
// expiration date dropdowns if it is not needed.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Upload_ShouldNotRequestExpirationDateInHappyPath) {
  SetUpForEditableExpirationDate();
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_LABEL)->visible());

  // Assert that expiration date was not explicitly requested in the bubble.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_VIEW));
  EXPECT_FALSE(
      FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_YEAR));
  EXPECT_FALSE(
      FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_MONTH));
}

// Tests the upload save bubble. Ensures that if the expiration date drop down
// box is changing, [Save] button will change status correctly.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SaveButtonStatusResetBetweenExpirationDateSelectionChanges) {
  SetUpForEditableExpirationDate();
  FillAndSubmitFormWithoutExpirationDate();
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();

  // [Save] button is disabled by default when requesting expiration date,
  // because there are no preselected values in the dropdown lists.
  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  // Selecting only month or year will disable [Save] button.
  year_input()->SetSelectedRow(2);
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  year_input()->SetSelectedRow(0);
  month_input()->SetSelectedRow(2);
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_DISABLED);

  // Selecting both month and year will enable [Save] button.
  month_input()->SetSelectedRow(2);
  year_input()->SetSelectedRow(2);
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_NORMAL);
}

// Tests the upload save bubble. Ensures that if the user is selecting an
// expired expiration date, it is not allowed to click [Save].
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SaveButtonIsDisabledIfExpiredExpirationDateAndExpirationDateRequested) {
  SetUpForEditableExpirationDate();
  FillAndSubmitFormWithoutExpirationDate();
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();

  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));

  // Set now to next month. Setting test_clock will not affect the dropdown to
  // be selected, so selecting the current January will always be expired.
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());
  test_clock.Advance(base::TimeDelta::FromDays(40));
  // Selecting expired date will disable [Save] button.
  month_input()->SetSelectedRow(1);
  year_input()->SetSelectedRow(1);
  EXPECT_EQ(save_button->state(),
            views::LabelButton::ButtonState::STATE_DISABLED);
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date with year pre-populated if year is valid
// but month is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithMissingExpirationDateMonthAndWithValidYear) {
  SetUpForEditableExpirationDate();
  // Submit the form with a year value, but not a month value.
  FillAndSubmitFormWithExpirationYearOnly(test::NextYear());
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure the next year is pre-populated but month is not checked.
  EXPECT_EQ(0, month_input()->selected_index());
  EXPECT_EQ(base::ASCIIToUTF16(test::NextYear()),
            year_input()->GetTextForRow(year_input()->selected_index()));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date with month pre-populated if month is
// detected but year is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithMissingExpirationDateYearAndWithMonth) {
  SetUpForEditableExpirationDate();
  // Submit the form with a month value, but not a year value.
  FillAndSubmitFormWithExpirationMonthOnly("12");
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure the December is pre-populated but year is not checked.
  EXPECT_EQ(base::ASCIIToUTF16("12"),
            month_input()->GetTextForRow(month_input()->selected_index()));
  EXPECT_EQ(0, year_input()->selected_index());
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if month is missing and year is detected
// but out of the range of dropdown.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithExpirationDateMonthAndWithYearIsOutOfRange) {
  SetUpForEditableExpirationDate();
  // Fill form but with an expiration year ten years in the future which is out
  // of the range of |year_input_dropdown_|.
  FillAndSubmitFormWithExpirationYearOnly(test::TenYearsFromNow());
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure no pre-populated expiration date.
  EXPECT_EQ(0, month_input()->selected_index());
  EXPECT_EQ(0, year_input()->GetSelectedRow());
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date month is missing and
// year is detected but passed.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithExpirationDateMonthAndYearExpired) {
  SetUpForEditableExpirationDate();
  // Fill form with a valid month but a passed year.
  FillAndSubmitFormWithSpecificExpirationDate("08", "2000");
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure no pre-populated expiration date.
  EXPECT_EQ(base::ASCIIToUTF16("08"),
            month_input()->GetTextForRow(month_input()->selected_index()));
  EXPECT_EQ(0, year_input()->GetSelectedRow());
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date is expired but is
// current year.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    Upload_SubmittingFormWithExpirationDateMonthAndCurrentYear) {
  SetUpForEditableExpirationDate();
  const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);
  // Fill form with a valid month but a passed year.
  FillAndSubmitFormWithSpecificExpirationDate("03", "2017");
  WaitForObservedEvent();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure pre-populated expiration date.
  EXPECT_EQ(base::ASCIIToUTF16("03"),
            month_input()->GetTextForRow(month_input()->selected_index()));
  EXPECT_EQ(base::ASCIIToUTF16("2017"),
            year_input()->GetTextForRow(year_input()->selected_index()));
}

// TODO(crbug.com/884817): Investigate combining local vs. upload tests using a
//                         boolean to branch local vs. upload logic.

// Tests StrikeDatabase interaction with the local save bubble. Ensures that no
// strikes are added if the feature flag is disabled.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_StrikeNotAddedIfExperimentFlagOff) {
  // Disable the the SaveCreditCardUsesStrikeSystem experiments.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillSaveCardImprovedUserConsent},
      // Disabled
      {features::kAutofillSaveCreditCardUsesStrikeSystem,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2});

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  base::HistogramTester histogram_tester;
  ClickOnCancelButton();

  // Ensure that no strike was added because the feature is disabled.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave", 0);
}

// Tests StrikeDatabase interaction with the upload save bubble. Ensures that no
// strikes are added if the feature flag is disabled.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTest,
    StrikeDatabase_Upload_StrikeNotAddedIfExperimentFlagOff) {
  // Disable the the SaveCreditCardUsesStrikeSystem experiments.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillSaveCardImprovedUserConsent},
      // Disabled
      {features::kAutofillSaveCreditCardUsesStrikeSystem,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  base::HistogramTester histogram_tester;

  ClickOnCancelButton();

  // Ensure that no strike was added because the feature is disabled.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave", 0);
}

// Tests StrikeDatabase interaction with the local save bubble. Ensures that a
// strike is added if the bubble is ignored.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_AddStrikeIfBubbleIgnored) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystemV2);

  TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();

  // Add an event observer to the controller to detect strike changes.
  AddEventObserverToController();

  base::HistogramTester histogram_tester;

  // Wait long enough to avoid bubble stickiness, then navigate away from the
  // page.
  test_clock.Advance(kCardBubbleSurviveNavigationTime);
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  NavigateTo(kCreditCardUploadForm);
  WaitForObservedEvent();

  // Ensure that a strike was added due to the bubble being ignored.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/1, /*count=*/1);
}

// Tests StrikeDatabase interaction with the upload save bubble. Ensures that a
// strike is added if the bubble is ignored.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Upload_AddStrikeIfBubbleIgnored) {
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillUpstream,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2},
      // Disabled
      {});

  TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();

  // Add an event observer to the controller to detect strike changes.
  AddEventObserverToController();

  base::HistogramTester histogram_tester;

  // Wait long enough to avoid bubble stickiness, then navigate away from the
  // page.
  test_clock.Advance(kCardBubbleSurviveNavigationTime);
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  NavigateTo(kCreditCardUploadForm);
  WaitForObservedEvent();

  // Ensure that a strike was added due to the bubble being ignored.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/1, /*count=*/1);
}

// Tests the local save bubble. Ensures that clicking the [No thanks] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_AddStrikeIfBubbleDeclined) {
  // Enable the updated UI.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillSaveCardImprovedUserConsent,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2},
      // Disabled
      {});

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton(/*strike_expected=*/true);

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*count=*/1);
}

// Tests the upload save bubble. Ensures that clicking the [No thanks] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Upload_AddStrikeIfBubbleDeclined) {
  // Enable the updated UI.
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillSaveCardImprovedUserConsent,
       features::kAutofillUpstream,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2},
      // Disabled
      {});

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the form should show the upload save bubble and legal footer.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton(/*strike_expected=*/true);

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*count=*/1);
}

// Tests overall StrikeDatabase interaction with the local save bubble. Runs an
// example of declining the prompt three times and ensuring that the
// offer-to-save bubble does not appear on the fourth try. Then, ensures that no
// strikes are added if the card already has max strikes.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_FullFlowTest) {
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillSaveCardImprovedUserConsent,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2},
      // Disabled
      {});

  bool controller_observer_set = false;

  // Show and ignore the bubble kMaxStrikesToPreventPoppingUpOfferToSavePrompt
  // times in order to accrue maximum strikes.
  for (int i = 0; i < kMaxStrikesToPreventPoppingUpOfferToSavePrompt; i++) {
    // Submitting the form and having Payments decline offering to save should
    // show the local save bubble.
    // (Must wait for response from Payments before accessing the controller.)
    ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
    NavigateTo(kCreditCardUploadForm);
    FillAndSubmitForm();
    WaitForObservedEvent();
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

    if (!controller_observer_set) {
      // Add an event observer to the controller.
      AddEventObserverToController();
      ReduceAnimationTime();
      controller_observer_set = true;
    }

    base::HistogramTester histogram_tester;
    ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
    ClickOnCancelButton(/*strike_expected=*/true);
    WaitForObservedEvent();

    // Ensure that a strike was added due to the bubble being declined.
    // The sample logged is the Nth strike added, or (i+1).
    histogram_tester.ExpectUniqueSample(
        "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
        /*sample=*/(i + 1), /*count=*/1);
  }

  base::HistogramTester histogram_tester;

  // Submit the form a fourth time. Since the card now has maximum strikes (3),
  // the icon should be shown but the bubble should not.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(GetSaveCardIconView()->visible());
  EXPECT_FALSE(GetSaveCardBubbleViews());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)->visible());

  ClickOnCancelButton();

  // Ensure that no strike was added because the card already had max strikes.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave", 0);

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, 1);
}

// Tests overall StrikeDatabase interaction with the upload save bubble. Runs an
// example of declining the prompt three times and ensuring that the
// offer-to-save bubble does not appear on the fourth try. Then, ensures that no
// strikes are added if the card already has max strikes.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Upload_FullFlowTest) {
  scoped_feature_list_.InitWithFeatures(
      // Enabled
      {features::kAutofillSaveCardImprovedUserConsent,
       features::kAutofillUpstream,
       features::kAutofillSaveCreditCardUsesStrikeSystemV2},
      // Disabled
      {});

  bool controller_observer_set = false;

  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Show and ignore the bubble kMaxStrikesToPreventPoppingUpOfferToSavePrompt
  // times in order to accrue maximum strikes.
  for (int i = 0; i < kMaxStrikesToPreventPoppingUpOfferToSavePrompt; i++) {
    // Submitting the form should show the upload save bubble and legal footer.
    // (Must wait for response from Payments before accessing the controller.)
    ResetEventWaiterForSequence(
        {DialogEvent::REQUESTED_UPLOAD_SAVE,
         DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
    NavigateTo(kCreditCardUploadForm);
    FillAndSubmitForm();
    WaitForObservedEvent();
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                    ->visible());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

    if (!controller_observer_set) {
      // Add an event observer to the controller.
      AddEventObserverToController();
      ReduceAnimationTime();
      controller_observer_set = true;
    }

    base::HistogramTester histogram_tester;

    ClickOnCancelButton(/*strike_expected=*/true);
    WaitForObservedEvent();

    // Ensure that a strike was added due to the bubble being declined.
    // The sample logged is the Nth strike added, or (i+1).
    histogram_tester.ExpectUniqueSample(
        "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
        /*sample=*/(i + 1), /*count=*/1);
  }

  base::HistogramTester histogram_tester;

  // Submit the form a fourth time. Since the card now has maximum strikes (3),
  // the icon should be shown but the bubble should not.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardUploadForm);
  FillAndSubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(GetSaveCardIconView()->visible());
  EXPECT_FALSE(GetSaveCardBubbleViews());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)->visible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->visible());

  ClickOnCancelButton();

  // Ensure that no strike was added because the card already had max strikes.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave", 0);

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::SERVER, 1);

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, 1);
}

}  // namespace autofill
