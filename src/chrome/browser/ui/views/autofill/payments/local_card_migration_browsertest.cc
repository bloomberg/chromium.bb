// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctime>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_dialog_view.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/migratable_card_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"

using base::Bucket;
using testing::ElementsAre;

namespace autofill {

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

constexpr char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
constexpr char kURLMigrateCardRequest[] =
    "https://payments.google.com/payments/apis-secure/chromepaymentsservice/"
    "migratecards"
    "?s7e_suffix=chromewallet";

constexpr char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
constexpr char kResponseGetUploadDetailsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";

constexpr char kResponseMigrateCardSuccess[] =
    "{\"save_result\":[{\"unique_id\":\"0\", \"status\":\"SUCCESS\"}, "
    "{\"unique_id\":\"1\", \"status\":\"SUCCESS\"}], "
    "\"value_prop_display_text\":\"example message.\"}";

constexpr char kCreditCardFormURL[] =
    "/credit_card_upload_form_address_and_cc.html";

constexpr char kFirstCardNumber[] = "5428424047572420";   // Mastercard
constexpr char kSecondCardNumber[] = "4782187095085933";  // Visa
constexpr char kThirdCardNumber[] = "4111111111111111";   // Visa
constexpr char kInvalidCardNumber[] = "4444444444444444";

constexpr double kFakeGeolocationLatitude = 1.23;
constexpr double kFakeGeolocationLongitude = 4.56;

}  // namespace

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() = default;
  ~PersonalDataLoadedObserverMock() = default;

  MOCK_METHOD0(OnPersonalDataChanged, void());
};

class LocalCardMigrationBrowserTest
    : public SyncTest,
      public LocalCardMigrationManager::ObserverForTest {
 protected:
  // Various events that can be waited on by the DialogEventWaiter.
  enum class DialogEvent : int {
    REQUESTED_LOCAL_CARD_MIGRATION,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_MIGRATE_CARDS_REQUEST,
    RECEIVED_MIGRATE_CARDS_RESPONSE
  };

  LocalCardMigrationBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  ~LocalCardMigrationBrowserTest() override {}

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    // Set up the HTTPS server (uses the embedded_test_server).
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
        browser()->profile())
        ->OverrideNetworkForTest(
            fake_server::CreateFakeServerHttpPostProviderFactory(
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
    local_card_migration_manager_ =
        ContentAutofillDriver::GetForRenderFrameHost(
            GetActiveWebContents()->GetMainFrame())
            ->autofill_manager()
            ->client()
            ->GetFormDataImporter()
            ->local_card_migration_manager_.get();

    local_card_migration_manager_->SetEventObserverForTesting(this);
    personal_data_ =
        PersonalDataManagerFactory::GetForProfile(browser()->profile());

    // Set up the fake geolocation data.
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            kFakeGeolocationLatitude, kFakeGeolocationLongitude);

    ASSERT_TRUE(harness_->SetupSync());

    // Set the billing_customer_number to designate existence of a Payments
    // account.
    const PaymentsCustomerData data =
        PaymentsCustomerData(/*customer_id=*/"123456");
    SetPaymentsCustomerData(data);

    SetUploadDetailsRpcPaymentsAccepts();
    SetUpMigrateCardsRpcPaymentsAccepts();
  }

  void SetPaymentsCustomerDataOnDBSequence(
      AutofillWebDataService* wds,
      const PaymentsCustomerData& customer_data) {
    DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
    AutofillTable::FromWebDatabase(wds->GetDatabase())
        ->SetPaymentsCustomerData(&customer_data);
  }

  void SetPaymentsCustomerData(const PaymentsCustomerData& customer_data) {
    scoped_refptr<AutofillWebDataService> wds =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    base::RunLoop loop;
    wds->GetDBTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &LocalCardMigrationBrowserTest::SetPaymentsCustomerDataOnDBSequence,
            base::Unretained(this), base::Unretained(wds.get()), customer_data),
        base::BindOnce(&base::RunLoop::Quit, base::Unretained(&loop)));
    loop.Run();
    WaitForOnPersonalDataChanged();
  }

  void WaitForOnPersonalDataChanged() {
    personal_data_->AddObserver(&personal_data_observer_);
    personal_data_->Refresh();

    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&personal_data_observer_);
    personal_data_->RemoveObserver(&personal_data_observer_);
  }

  void NavigateTo(const std::string& file_path) {
    ui_test_utils::NavigateToURL(
        browser(), file_path.find("data:") == 0U
                       ? GURL(file_path)
                       : embedded_test_server()->GetURL(file_path));
  }

  void OnDecideToRequestLocalCardMigration() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION);
  }

  void OnReceivedGetUploadDetailsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
  }

  void OnSentMigrateCardsRequest() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::SENT_MIGRATE_CARDS_REQUEST);
  }

  void OnReceivedMigrateCardsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE);
  }

  CreditCard SaveLocalCard(std::string card_number,
                           bool set_as_expired_card = false) {
    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "John Smith", card_number.c_str(),
                            "12",
                            set_as_expired_card ? test::LastYear().c_str()
                                                : test::NextYear().c_str(),
                            "1");
    local_card.set_guid("00000000-0000-0000-0000-" + card_number.substr(0, 12));
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    AddTestCreditCard(browser(), local_card);
    return local_card;
  }

  CreditCard SaveServerCard(std::string card_number) {
    CreditCard server_card;
    test::SetCreditCardInfo(&server_card, "John Smith", card_number.c_str(),
                            "12", test::NextYear().c_str(), "1");
    server_card.set_guid("00000000-0000-0000-0000-" +
                         card_number.substr(0, 12));
    server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    server_card.set_server_id("full_id_" + card_number);
    AddTestServerCreditCard(browser(), server_card);
    return server_card;
  }

  void UseCardAndWaitForMigrationOffer(std::string card_number) {
    // Reusing a card should show the migration offer bubble.
    ResetEventWaiterForSequence(
        {DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION,
         DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
    FillAndSubmitFormWithCard(card_number);
    WaitForObservedEvent();
    WaitForAnimationToComplete();
  }

  void ClickOnSaveButtonAndWaitForMigrationResults() {
    ResetEventWaiterForSequence({DialogEvent::SENT_MIGRATE_CARDS_REQUEST,
                                 DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE});
    ClickOnOkButton(GetLocalCardMigrationMainDialogView());
    WaitForObservedEvent();
  }

  void FillAndSubmitFormWithCard(std::string card_number) {
    NavigateTo(kCreditCardFormURL);
    content::WebContents* web_contents = GetActiveWebContents();

    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string fill_cc_number_js =
        "(function() { document.getElementsByName(\"cc_number\")[0].value = " +
        card_number + "; })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, fill_cc_number_js));

    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  void SetUploadDetailsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess);
  }

  void SetUploadDetailsRpcPaymentsDeclines() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsFailure);
  }

  void SetUpMigrateCardsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLMigrateCardRequest,
                                           kResponseMigrateCardSuccess);
  }

  void ClickOnView(views::View* view) {
    CHECK(view);
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

  void ClickOnDialogViewAndWait(
      views::View* view,
      views::DialogDelegateView* local_card_migration_view) {
    CHECK(local_card_migration_view);
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        local_card_migration_view->GetWidget());
    local_card_migration_view->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(
            local_card_migration_view->GetWidget()
                ->non_client_view()
                ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
    destroyed_waiter.Wait();
  }

  views::View* FindViewInDialogById(
      DialogViewId view_id,
      views::DialogDelegateView* local_card_migration_view) {
    CHECK(local_card_migration_view);

    views::View* specified_view =
        local_card_migration_view->GetViewByID(static_cast<int>(view_id));

    if (!specified_view) {
      specified_view =
          local_card_migration_view->GetWidget()->GetRootView()->GetViewByID(
              static_cast<int>(view_id));
    }

    return specified_view;
  }

  void ClickOnOkButton(views::DialogDelegateView* local_card_migration_view) {
    views::View* ok_button = local_card_migration_view->GetOkButton();

    ClickOnDialogViewAndWait(ok_button, local_card_migration_view);
  }

  void ClickOnCancelButton(
      views::DialogDelegateView* local_card_migration_view) {
    views::View* cancel_button = local_card_migration_view->GetCancelButton();
    ClickOnDialogViewAndWait(cancel_button, local_card_migration_view);
  }

  LocalCardMigrationBubbleViews* GetLocalCardMigrationOfferBubbleViews() {
    LocalCardMigrationBubbleControllerImpl*
        local_card_migration_bubble_controller_impl =
            LocalCardMigrationBubbleControllerImpl::FromWebContents(
                GetActiveWebContents());
    if (!local_card_migration_bubble_controller_impl)
      return nullptr;
    return static_cast<LocalCardMigrationBubbleViews*>(
        local_card_migration_bubble_controller_impl
            ->local_card_migration_bubble_view());
  }

  views::DialogDelegateView* GetLocalCardMigrationMainDialogView() {
    LocalCardMigrationDialogControllerImpl*
        local_card_migration_dialog_controller_impl =
            LocalCardMigrationDialogControllerImpl::FromWebContents(
                GetActiveWebContents());
    if (!local_card_migration_dialog_controller_impl)
      return nullptr;
    return static_cast<LocalCardMigrationDialogView*>(
        local_card_migration_dialog_controller_impl
            ->local_card_migration_dialog_view());
  }

  PageActionIconView* GetLocalCardMigrationIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kLocalCardMigration);
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableToolbarStatusChip)) {
      EXPECT_TRUE(
          browser_view->toolbar()->toolbar_account_icon_container()->Contains(
              icon));
    } else {
      EXPECT_TRUE(browser_view->GetLocationBarView()->Contains(icon));
    }
    return icon;
  }

  views::View* GetCloseButton() {
    LocalCardMigrationBubbleViews* local_card_migration_bubble_views =
        static_cast<LocalCardMigrationBubbleViews*>(
            GetLocalCardMigrationOfferBubbleViews());
    CHECK(local_card_migration_bubble_views);
    return local_card_migration_bubble_views->GetBubbleFrameView()
        ->GetCloseButtonForTesting();
  }

  views::View* GetCardListView() {
    return static_cast<LocalCardMigrationDialogView*>(
               GetLocalCardMigrationMainDialogView())
        ->card_list_view_;
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void WaitForCardDeletion() { WaitForPersonalDataChange(browser()); }

  void WaitForAnimationToComplete() {
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableToolbarStatusChip)) {
      views::test::WaitForAnimatingLayoutManager(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->toolbar_account_icon_container());
    }
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  LocalCardMigrationManager* local_card_migration_manager_;

  PersonalDataManager* personal_data_;
  PersonalDataLoadedObserverMock personal_data_observer_;

  std::unique_ptr<ProfileSyncServiceHarness> harness_;

 private:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationBrowserTest);
};

// TODO(crbug.com/932818): Remove this class after experiment flag is cleaned
// up. Otherwise we need it because the toolbar is init-ed before each test is
// set up. Thus need to enable the feature in the general browsertest SetUp().
class LocalCardMigrationBrowserTestForStatusChip
    : public LocalCardMigrationBrowserTest {
 protected:
  LocalCardMigrationBrowserTestForStatusChip()
      : LocalCardMigrationBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillCreditCardUploadFeedback,
                              features::kAutofillEnableToolbarStatusChip,
                              features::kAutofillUpstream},
        /*disabled_features=*/{});
  }

  ~LocalCardMigrationBrowserTestForStatusChip() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensures that migration is not offered when user saves a new card.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       UsingNewCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kSecondCardNumber);

  // No migration bubble should be showing, because the single card upload
  // bubble should be displayed instead.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that migration is not offered when payments declines the cards.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    IntermediateMigrationOfferDoesNotShowWhenPaymentsDeclines) {
  base::HistogramTester histogram_tester;
  SetUploadDetailsRpcPaymentsDeclines();

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is not shown after reusing
// a saved server card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingServerCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveServerCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is shown after reusing
// a saved server card, if there is at least one card to migrate.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    ReusingServerCardWithMigratableLocalCardShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveServerCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // The intermediate migration bubble should show.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationOrigin.UseOfServerCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
}

// Ensures that the intermediate migration bubble is not shown after reusing
// a previously saved local card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingLocalCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No migration bubble should be showing, because the single card upload
  // bubble should be displayed instead.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is triggered after reusing
// a saved local card, if there are multiple local cards available to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingLocalCardShowsIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // The intermediate migration bubble should show.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
}

// Ensures that clicking [X] on the offer bubble makes the bubble disappear.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingCloseClosesBubble) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
}

// Ensures that the credit card icon will show in location bar.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       CreditCardIconShownInLocationBar) {
  SaveServerCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
}

// Ensures that clicking on the credit card icon in the omnibox reopens the
// offer bubble after closing it.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingOmniboxIconReshowsBubble) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());
  ClickOnView(GetLocalCardMigrationIconView());

  // Clicking the icon should reshow the bubble.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.Reshows"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

// Ensures that accepting the intermediate migration offer opens up the main
// migration dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingContinueOpensDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  // Dialog should be visible.
  EXPECT_TRUE(FindViewInDialogById(
                  DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_OFFER_DIALOG,
                  GetLocalCardMigrationMainDialogView())
                  ->GetVisible());
  // Intermediate bubble should be gone.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_ACCEPTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationDialogOffer",
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_SHOWN, 1);
}

// Ensures that the migration dialog contains all the valid card stored in
// Chrome browser local storage.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       DialogContainsAllValidMigratableCard) {
  base::HistogramTester histogram_tester;

  CreditCard first_card = SaveLocalCard(kFirstCardNumber);
  CreditCard second_card = SaveLocalCard(kSecondCardNumber);
  SaveLocalCard(kThirdCardNumber, /*set_as_expired_card=*/true);
  SaveLocalCard(kInvalidCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  views::View* card_list_view = GetCardListView();
  EXPECT_TRUE(card_list_view->GetVisible());
  ASSERT_EQ(2u, card_list_view->children().size());
  // Cards will be added to database in a reversed order.
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->children()[0])
                ->GetNetworkAndLastFourDigits(),
            second_card.NetworkAndLastFourDigits());
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->children()[1])
                ->GetNetworkAndLastFourDigits(),
            first_card.NetworkAndLastFourDigits());
}

// Ensures that rejecting the main migration dialog closes the dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingCancelClosesDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Cancel] button.
  ClickOnCancelButton(GetLocalCardMigrationMainDialogView());

  // No dialog should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationMainDialogView());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationDialogUserInteraction",
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED,
      1);
}

// Ensures that accepting the main migration dialog closes the dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingSaveClosesDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button in the bubble.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button in the dialog.
  ClickOnSaveButtonAndWaitForMigrationResults();

  // No dialog should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationMainDialogView());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_SHOWN, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_ACCEPTED, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationDialogUserInteraction",
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED,
      1);
}

// Ensures local cards will be deleted from browser local storage after being
// successfully migrated.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       DeleteSuccessfullyMigratedCardsFromLocal) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button in the bubble.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button in the dialog.
  ClickOnSaveButtonAndWaitForMigrationResults();
  WaitForCardDeletion();

  EXPECT_EQ(nullptr, personal_data_->GetCreditCardByNumber(kFirstCardNumber));
  EXPECT_EQ(nullptr, personal_data_->GetCreditCardByNumber(kSecondCardNumber));
}

// Ensures that accepting the main migration dialog adds strikes.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       AcceptingDialogAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button, should add and log strikes.
  ClickOnSaveButtonAndWaitForMigrationResults();

  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(Bucket(
          LocalCardMigrationStrikeDatabase::kStrikesToAddWhenDialogClosed, 1)));
}

// Ensures that rejecting the main migration dialog adds strikes.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       RejectingDialogAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Cancel] button, should add and log strikes.
  ClickOnCancelButton(GetLocalCardMigrationMainDialogView());

  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(Bucket(
          LocalCardMigrationStrikeDatabase::kStrikesToAddWhenDialogClosed, 1)));
}

// Ensures that rejecting the migration bubble adds strikes.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClosingBubbleAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(Bucket(
          LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed, 1)));
}

// Ensures that rejecting the migration bubble repeatedly adds strikes every
// time, even for the same tab. Currently, it adds 3 strikes (out of 6), so this
// test can reliably test it being added twice.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClosingBubbleAgainAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());
  // Do it again for the same tab.
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics: Added 3 strikes each time, for totals of 3 then 6.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(
          Bucket(
              LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed,
              1),
          Bucket(
              LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed *
                  2,
              1)));
}

// Ensures that reshowing and closing bubble after previously closing it does
// not add strikes.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReshowingBubbleDoesNotAddStrikes) {
  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());
  base::HistogramTester histogram_tester;
  ClickOnView(GetLocalCardMigrationIconView());

  // Clicking the icon should reshow the bubble.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());

  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  // Metrics
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// TODO(crbug.com/932818): Remove the condition once the experiment is enabled
// on ChromeOS.
#if !defined(OS_CHROMEOS)
// Ensures that the credit card icon will show in status chip.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForStatusChip,
                       CreditCardIconShownInStatusChip) {
  SaveServerCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
}

// TODO(crbug.com/999510): Crashes flakily on Linux.
#if defined(OS_LINUX)
#define MAYBE_ClickingOmniboxIconReshowsBubble \
  DISABLED_ClickingOmniboxIconReshowsBubble
#else
#define MAYBE_ClickingOmniboxIconReshowsBubble ClickingOmniboxIconReshowsBubble
#endif
// Ensures that clicking on the credit card icon in the status chip reopens the
// offer bubble after closing it.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForStatusChip,
                       MAYBE_ClickingOmniboxIconReshowsBubble) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());
  ClickOnView(GetLocalCardMigrationIconView());

  // Clicking the icon should reshow the bubble.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.Reshows"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

#if defined(OS_MACOSX)
// TODO(crbug.com/823543): Widget activation doesn't work on Mac.
#define MAYBE_ActivateFirstInactiveBubbleForAccessibility \
  DISABLED_ActivateFirstInactiveBubbleForAccessibility
#else
#define MAYBE_ActivateFirstInactiveBubbleForAccessibility \
  ActivateFirstInactiveBubbleForAccessibility
#endif
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForStatusChip,
                       MAYBE_ActivateFirstInactiveBubbleForAccessibility) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ToolbarView* toolbar_view = browser_view->toolbar();
  EXPECT_FALSE(toolbar_view->toolbar_account_icon_container()
                   ->page_action_icon_controller()
                   ->ActivateFirstInactiveBubbleForAccessibility());

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // Ensures the bubble's widget is visible, but inactive. Active widgets are
  // focused by accessibility, so not of concern.
  views::Widget* widget = GetLocalCardMigrationOfferBubbleViews()->GetWidget();
  widget->Deactivate();
  widget->ShowInactive();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->IsActive());

  EXPECT_TRUE(toolbar_view->toolbar_account_icon_container()
                  ->page_action_icon_controller()
                  ->ActivateFirstInactiveBubbleForAccessibility());

  // Ensure the bubble's widget refreshed appropriately.
  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->IsActive());
}

// Ensures the credit card icon updates its visibility when switching between
// tabs.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForStatusChip,
                       IconAndBubbleVisibilityAfterTabSwitching) {
  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // Ensures flow is triggered, and bubble and icon view are visible.
  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
  EXPECT_TRUE(GetLocalCardMigrationOfferBubbleViews()->GetVisible());

  AddTabAtIndex(1, GURL("http://example.com/"), ui::PAGE_TRANSITION_TYPED);
  TabStripModel* tab_model = browser()->tab_strip_model();
  tab_model->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  WaitForAnimationToComplete();

  // Ensures bubble and icon go away if user navigates to another tab.
  EXPECT_FALSE(GetLocalCardMigrationIconView()->GetVisible());
  EXPECT_FALSE(GetLocalCardMigrationOfferBubbleViews());

  tab_model->ActivateTabAt(0, {TabStripModel::GestureType::kOther});
  WaitForAnimationToComplete();

  // If the user navigates back, shows only the icon not the bubble.
  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
  EXPECT_FALSE(GetLocalCardMigrationOfferBubbleViews());
}

IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForStatusChip,
                       Feedback_CardSavingAnimation) {
  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button in the bubble.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  test_url_loader_factory()->ClearResponses();

  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
  EXPECT_FALSE(GetLocalCardMigrationIconView()
                   ->loading_indicator_for_testing()
                   ->IsAnimating());

  // Click the [Save] button in the dialog.
  ResetEventWaiterForSequence({DialogEvent::SENT_MIGRATE_CARDS_REQUEST});
  ClickOnOkButton(GetLocalCardMigrationMainDialogView());
  WaitForObservedEvent();

  // No dialog should be showing, but icon should display throbber animation.
  EXPECT_EQ(nullptr, GetLocalCardMigrationMainDialogView());
  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
  EXPECT_TRUE(GetLocalCardMigrationIconView()
                  ->loading_indicator_for_testing()
                  ->IsAnimating());

  SetUpMigrateCardsRpcPaymentsAccepts();
  ResetEventWaiterForSequence({DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE});
  WaitForObservedEvent();

  // Icon animation should stop. Dialog stays hidden.
  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
  EXPECT_FALSE(GetLocalCardMigrationIconView()
                   ->loading_indicator_for_testing()
                   ->IsAnimating());
}

#endif  // !defined(OS_CHROMEOS)

class LocalCardMigrationBrowserTestForFixedLogging
    : public LocalCardMigrationBrowserTest {
 protected:
  LocalCardMigrationBrowserTestForFixedLogging() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableFixedPaymentsBubbleLogging);
  }

  ~LocalCardMigrationBrowserTestForFixedLogging() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForFixedLogging,
                       ClosedReason_BubbleAccepted) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED, 1);
}

IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForFixedLogging,
                       ClosedReason_BubbleClosed) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED, 1);
}

IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForFixedLogging,
                       ClosedReason_BubbleNotInteracted) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetLocalCardMigrationOfferBubbleViews()->GetWidget());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_waiter.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
}

IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTestForFixedLogging,
                       ClosedReason_BubbleLostFocus) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetLocalCardMigrationOfferBubbleViews()->GetWidget());
  GetLocalCardMigrationOfferBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);
  destroyed_waiter.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS, 1);
}

// TODO(crbug.com/897998):
// - Add more tests for feedback dialog.

}  // namespace autofill
