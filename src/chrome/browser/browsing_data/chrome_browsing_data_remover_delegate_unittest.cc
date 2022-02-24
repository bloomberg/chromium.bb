// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"

#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/client_hints/common/client_hints.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/domain_reliability/monitor.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/mock_field_info_store.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/mock_smart_bubble_stats_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/content/mock_payment_manifest_web_data_service.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/site_isolation/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/net_buildflags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/customtabs/origin_verifier.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/android/webapps/webapp_registry.h"
#include "components/feed/buildflags.h"
#else
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "content/public/browser/host_zoom_map.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chromeos/dbus/attestation/fake_attestation_client.h"
#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crashpad.h"
#include "components/upload_list/crash_upload_list.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_utils.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_REPORTING)
#include "base/containers/flat_map.h"
#include "base/unguessable_token.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

using content::BrowsingDataFilterBuilder;
using domain_reliability::DomainReliabilityClearMode;
using domain_reliability::DomainReliabilityMonitor;
using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::FloatEq;
using testing::Invoke;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Return;
using testing::SizeIs;
using testing::WithArgs;

namespace constants = chrome_browsing_data_remover;

namespace {

const char kTestRegisterableDomain1[] = "host1.com";
const char kTestRegisterableDomain3[] = "host3.com";

// For HTTP auth.
const char kTestRealm[] = "TestRealm";

// For Autofill.
const char kWebOrigin[] = "https://www.example.com/";

// Shorthands for origin types.
#if BUILDFLAG(ENABLE_EXTENSIONS)
const uint64_t kExtension = constants::ORIGIN_TYPE_EXTENSION;
#endif
const uint64_t kProtected =
    content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
const uint64_t kUnprotected =
    content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

// Testers --------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)
class TestWebappRegistry : public WebappRegistry {
 public:
  TestWebappRegistry() : WebappRegistry() {}

  void UnregisterWebappsForUrls(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter) override {
    // Mocks out a JNI call.
  }

  void ClearWebappHistoryForUrls(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter) override {
    // Mocks out a JNI call.
  }
};
#endif

class RemoveCookieTester {
 public:
  RemoveCookieTester() = default;
  RemoveCookieTester(const RemoveCookieTester&) = delete;
  RemoveCookieTester& operator=(const RemoveCookieTester&) = delete;

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    bool result = false;
    base::RunLoop run_loop;
    cookie_manager_->GetCookieList(
        cookie_url_, net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection(),
        base::BindLambdaForTesting(
            [&](const net::CookieAccessResultList& cookie_list,
                const net::CookieAccessResultList& excluded_cookies) {
              std::string cookie_line =
                  net::CanonicalCookie::BuildCookieLine(cookie_list);
              if (cookie_line == "A=1") {
                result = true;
              } else {
                EXPECT_EQ("", cookie_line);
                result = false;
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  void AddCookie() {
    base::RunLoop run_loop;
    auto cookie = net::CanonicalCookie::Create(
        cookie_url_, "A=1", base::Time::Now(), absl::nullopt /* server_time */,
        absl::nullopt /* cookie_partition_key */);
    cookie_manager_->SetCanonicalCookie(
        *cookie, cookie_url_, net::CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting([&](net::CookieAccessResult result) {
          EXPECT_TRUE(result.status.IsInclude());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

 protected:
  void SetCookieManager(
      mojo::Remote<network::mojom::CookieManager> cookie_manager) {
    cookie_manager_ = std::move(cookie_manager);
  }

 private:
  const GURL cookie_url_{"http://host1.com:1"};

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

class RemoveSafeBrowsingCookieTester : public RemoveCookieTester {
 public:
  explicit RemoveSafeBrowsingCookieTester(Profile* profile)
      : browser_process_(TestingBrowserProcess::GetGlobal()) {
    // TODO(crbug/925153): Port consumers of the |sb_service| to use the
    // interface in components/safe_browsing, and remove this cast.
    scoped_refptr<safe_browsing::SafeBrowsingService> sb_service =
        static_cast<safe_browsing::SafeBrowsingService*>(
            safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    browser_process_->SetSafeBrowsingService(sb_service.get());
    sb_service->Initialize();
    base::RunLoop().RunUntilIdle();

    // Make sure the safe browsing cookie store has no cookies.
    // TODO(mmenke): Is this really needed?
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    sb_service->GetNetworkContext(profile)->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());
    cookie_manager->DeleteCookies(
        network::mojom::CookieDeletionFilter::New(),
        base::BindLambdaForTesting(
            [&](uint32_t num_deleted) { run_loop.Quit(); }));
    run_loop.Run();

    SetCookieManager(std::move(cookie_manager));
  }

  RemoveSafeBrowsingCookieTester(const RemoveSafeBrowsingCookieTester&) =
      delete;
  RemoveSafeBrowsingCookieTester& operator=(
      const RemoveSafeBrowsingCookieTester&) = delete;

  virtual ~RemoveSafeBrowsingCookieTester() {
    browser_process_->safe_browsing_service()->ShutDown();
    base::RunLoop().RunUntilIdle();
    browser_process_->SetSafeBrowsingService(nullptr);
  }

 private:
  raw_ptr<TestingBrowserProcess> browser_process_;
};

class RemoveHistoryTester {
 public:
  RemoveHistoryTester() = default;

  RemoveHistoryTester(const RemoveHistoryTester&) = delete;
  RemoveHistoryTester& operator=(const RemoveHistoryTester&) = delete;

  [[nodiscard]] bool Init(Profile* profile) {
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history_service_)
      return false;

    return true;
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    bool contains_url = false;

    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service_->QueryURL(
        url, true,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          contains_url = result.success;
          run_loop.Quit();
        }),
        &tracker);
    run_loop.Run();

    return contains_url;
  }

  void AddHistory(const GURL& url, base::Time time) {
    history_service_->AddPage(url, time, nullptr, 0, GURL(),
                              history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false, false);
  }

 private:
  // TestingProfile owns the history service; we shouldn't delete it.
  history::HistoryService* history_service_ = nullptr;
};

class RemoveFaviconTester {
 public:
  RemoveFaviconTester() = default;
  RemoveFaviconTester(const RemoveFaviconTester&) = delete;
  RemoveFaviconTester& operator=(const RemoveFaviconTester&) = delete;

  [[nodiscard]] bool Init(Profile* profile) {
    // Create the history service if it has not been created yet.
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history_service_)
      return false;

    favicon_service_ = FaviconServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!favicon_service_)
      return false;

    return true;
  }

  // Returns true if there is a favicon stored for |page_url| in the favicon
  // database.
  bool HasFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_favicon_;
  }

  // Returns true if:
  // - There is a favicon stored for |page_url| in the favicon database.
  // - The stored favicon is expired.
  bool HasExpiredFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_expired_favicon_;
  }

  // Adds a visit to history and stores an arbitrary favicon bitmap for
  // |page_url|.
  void VisitAndAddFavicon(const GURL& page_url) {
    history_service_->AddPage(page_url, base::Time::Now(), nullptr, 0, GURL(),
                              history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false, false);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
    bitmap.eraseColor(SK_ColorBLUE);
    favicon_service_->SetFavicons({page_url}, page_url,
                                  favicon_base::IconType::kFavicon,
                                  gfx::Image::CreateFrom1xBitmap(bitmap));
  }

 private:
  // Synchronously requests the favicon for |page_url| from the favicon
  // database.
  void RequestFaviconSyncForPageURL(const GURL& page_url) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    favicon_service_->GetRawFaviconForPageURL(
        page_url, {favicon_base::IconType::kFavicon}, gfx::kFaviconSize,
        /*fallback_to_host=*/false,
        base::BindOnce(&RemoveFaviconTester::SaveResultAndQuit,
                       base::Unretained(this)),
        &tracker_);
    run_loop.Run();
  }

  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(const favicon_base::FaviconRawBitmapResult& result) {
    got_favicon_ = result.is_valid();
    got_expired_favicon_ = result.is_valid() && result.expired;
    std::move(quit_closure_).Run();
  }

  // For favicon requests.
  base::CancelableTaskTracker tracker_;
  bool got_favicon_ = false;
  bool got_expired_favicon_ = false;
  base::OnceClosure quit_closure_;

  // Owned by TestingProfile.
  raw_ptr<history::HistoryService> history_service_ = nullptr;
  raw_ptr<favicon::FaviconService> favicon_service_ = nullptr;
};

std::unique_ptr<KeyedService> BuildProtocolHandlerRegistry(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<custom_handlers::ProtocolHandlerRegistry>(
      profile,
      std::make_unique<custom_handlers::TestProtocolHandlerRegistryDelegate>());
}

class ClearDomainReliabilityTester {
 public:
  explicit ClearDomainReliabilityTester(TestingProfile* profile) {
    static_cast<ChromeBrowsingDataRemoverDelegate*>(
        profile->GetBrowsingDataRemoverDelegate())
        ->OverrideDomainReliabilityClearerForTesting(base::BindRepeating(
            &ClearDomainReliabilityTester::Clear, base::Unretained(this)));
  }

  unsigned clear_count() const { return clear_count_; }

  network::mojom::NetworkContext::DomainReliabilityClearMode last_clear_mode()
      const {
    return last_clear_mode_;
  }

  const base::RepeatingCallback<bool(const GURL&)>& last_filter() const {
    return last_filter_;
  }

 private:
  void Clear(
      content::BrowsingDataFilterBuilder* filter_builder,
      network::mojom::NetworkContext_DomainReliabilityClearMode mode,
      network::mojom::NetworkContext::ClearDomainReliabilityCallback callback) {
    ++clear_count_;
    last_clear_mode_ = mode;
    std::move(callback).Run();

    last_filter_ = filter_builder->MatchesAllOriginsAndDomains()
                       ? base::RepeatingCallback<bool(const GURL&)>()
                       : filter_builder->BuildUrlFilter();
  }

  unsigned clear_count_ = 0;
  network::mojom::NetworkContext::DomainReliabilityClearMode last_clear_mode_;
  base::RepeatingCallback<bool(const GURL&)> last_filter_;
};

class RemovePasswordsTester {
 public:
  explicit RemovePasswordsTester(TestingProfile* testing_profile) {
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        testing_profile,
        base::BindRepeating(
            &password_manager::BuildPasswordStoreInterface<
                content::BrowserContext,
                testing::NiceMock<
                    password_manager::MockPasswordStoreInterface>>));

    profile_store_ = static_cast<password_manager::MockPasswordStoreInterface*>(
        PasswordStoreFactory::GetForProfile(testing_profile,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get());

    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
          testing_profile,
          base::BindRepeating(
              &password_manager::BuildPasswordStoreInterface<
                  content::BrowserContext,
                  testing::NiceMock<
                      password_manager::MockPasswordStoreInterface>>));

      account_store_ =
          static_cast<password_manager::MockPasswordStoreInterface*>(
              AccountPasswordStoreFactory::GetForProfile(
                  testing_profile, ServiceAccessType::EXPLICIT_ACCESS)
                  .get());
    }

    OSCryptMocker::SetUp();
  }

  RemovePasswordsTester(const RemovePasswordsTester&) = delete;
  RemovePasswordsTester& operator=(const RemovePasswordsTester&) = delete;

  ~RemovePasswordsTester() { OSCryptMocker::TearDown(); }

  password_manager::MockPasswordStoreInterface* profile_store() {
    return profile_store_;
  }

  password_manager::MockPasswordStoreInterface* account_store() {
    return account_store_;
  }

  password_manager::MockSmartBubbleStatsStore* mock_smart_bubble_stats_store() {
    return &mock_smart_bubble_stats_store_;
  }

  password_manager::MockFieldInfoStore* mock_field_info_store() {
    return &mock_field_info_store_;
  }

 private:
  raw_ptr<password_manager::MockPasswordStoreInterface> profile_store_;
  raw_ptr<password_manager::MockPasswordStoreInterface> account_store_;
  testing::NiceMock<password_manager::MockSmartBubbleStatsStore>
      mock_smart_bubble_stats_store_;
  testing::NiceMock<password_manager::MockFieldInfoStore>
      mock_field_info_store_;
};

class RemoveSecurePaymentConfirmationCredentialsTester {
 public:
  using MockWrapper = testing::NiceMock<payments::MockWebDataServiceWrapper>;
  using MockService =
      testing::NiceMock<payments::MockPaymentManifestWebDataService>;
  explicit RemoveSecurePaymentConfirmationCredentialsTester(
      TestingProfile* testing_profile) {
    webdata_services::WebDataServiceWrapperFactory::GetInstance()
        ->SetTestingFactory(
            testing_profile,
            base::BindRepeating(
                &RemoveSecurePaymentConfirmationCredentialsTester::
                    BuildServiceWapper,
                base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildServiceWapper(
      content::BrowserContext* context) {
    auto wrapper = std::make_unique<MockWrapper>();
    EXPECT_CALL(*wrapper, GetPaymentManifestWebData)
        .WillRepeatedly(Return(service_));
    return std::move(wrapper);
  }

  void ExpectCallClearSecurePaymentConfirmationCredentials(int times) {
    EXPECT_CALL(*service_.get(), ClearSecurePaymentConfirmationCredentials)
        .Times(times)
        .WillRepeatedly(testing::WithArg<2>([](base::OnceClosure completion) {
          base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                        std::move(completion));
        }));
  }

 private:
  scoped_refptr<MockService> service_ = base::MakeRefCounted<MockService>();
};

class RemovePermissionPromptCountsTest {
 public:
  explicit RemovePermissionPromptCountsTest(TestingProfile* profile)
      : autoblocker_(
            PermissionDecisionAutoBlockerFactory::GetForProfile(profile)) {}
  RemovePermissionPromptCountsTest(const RemovePermissionPromptCountsTest&) =
      delete;
  RemovePermissionPromptCountsTest& operator=(
      const RemovePermissionPromptCountsTest&) = delete;

  int GetDismissCount(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->GetDismissCount(url, permission);
  }

  int GetIgnoreCount(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->GetIgnoreCount(url, permission);
  }

  bool RecordIgnoreAndEmbargo(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->RecordIgnoreAndEmbargo(url, permission, false);
  }

  bool RecordDismissAndEmbargo(const GURL& url,
                               ContentSettingsType permission) {
    return autoblocker_->RecordDismissAndEmbargo(url, permission, false);
  }

  void CheckEmbargo(const GURL& url,
                    ContentSettingsType permission,
                    ContentSetting expected_setting) {
    EXPECT_EQ(expected_setting,
              autoblocker_->GetEmbargoResult(url, permission).content_setting);
  }

 private:
  raw_ptr<permissions::PermissionDecisionAutoBlocker> autoblocker_;
};

// Custom matcher to test the equivalence of two URL filters. Since those are
// blackbox predicates, we can only approximate the equivalence by testing
// whether the filter give the same answer for several URLs. This is currently
// good enough for our testing purposes, to distinguish filters that delete or
// preserve origins, empty and non-empty filters and such.
//
// TODO(msramek): BrowsingDataRemover and some of its backends support URL
// filters, but its constructor currently only takes a single URL and constructs
// its own url filter. If an url filter was directly passed to
// BrowsingDataRemover (what should eventually be the case), we can use the same
// instance in the test as well, and thus simply test
// base::RepeatingCallback::Equals() in this matcher.
class ProbablySameFilterMatcher
    : public MatcherInterface<
          const base::RepeatingCallback<bool(const GURL&)>&> {
 public:
  explicit ProbablySameFilterMatcher(
      const base::RepeatingCallback<bool(const GURL&)>& filter)
      : to_match_(filter) {}

  bool MatchAndExplain(const base::RepeatingCallback<bool(const GURL&)>& filter,
                       MatchResultListener* listener) const override {
    if (filter.is_null() && to_match_.is_null())
      return true;
    if (filter.is_null() != to_match_.is_null())
      return false;

    const GURL urls_to_test_[] = {
        GURL("http://host1.com:1"), GURL("http://host2.com:1"),
        GURL("http://host3.com:1"), GURL("invalid spec")};
    for (GURL url : urls_to_test_) {
      if (filter.Run(url) != to_match_.Run(url)) {
        if (listener)
          *listener << "The filters differ on the URL " << url;
        return false;
      }
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "is probably the same url filter as " << &to_match_;
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "is definitely NOT the same url filter as " << &to_match_;
  }

 private:
  const base::RepeatingCallback<bool(const GURL&)>& to_match_;
};

inline Matcher<const base::RepeatingCallback<bool(const GURL&)>&>
ProbablySameFilter(const base::RepeatingCallback<bool(const GURL&)>& filter) {
  return MakeMatcher(new ProbablySameFilterMatcher(filter));
}

bool ProbablySameFilters(
    const base::RepeatingCallback<bool(const GURL&)>& filter1,
    const base::RepeatingCallback<bool(const GURL&)>& filter2) {
  return ProbablySameFilter(filter1).MatchAndExplain(filter2, nullptr);
}

base::Time AnHourAgo() {
  return base::Time::Now() - base::Hours(1);
}

class RemoveDownloadsTester {
 public:
  explicit RemoveDownloadsTester(TestingProfile* testing_profile)
      : download_manager_(new testing::NiceMock<content::MockDownloadManager>) {
    testing_profile->SetDownloadManagerForTesting(
        base::WrapUnique(download_manager_.get()));
    std::unique_ptr<ChromeDownloadManagerDelegate> delegate =
        std::make_unique<ChromeDownloadManagerDelegate>(testing_profile);
    chrome_download_manager_delegate_ = delegate.get();
    service_ =
        DownloadCoreServiceFactory::GetForBrowserContext(testing_profile);
    service_->SetDownloadManagerDelegateForTesting(std::move(delegate));

    EXPECT_CALL(*download_manager_, GetBrowserContext())
        .WillRepeatedly(Return(testing_profile));
    EXPECT_CALL(*download_manager_, Shutdown());
  }

  RemoveDownloadsTester(const RemoveDownloadsTester&) = delete;
  RemoveDownloadsTester& operator=(const RemoveDownloadsTester&) = delete;

  ~RemoveDownloadsTester() {
    service_->SetDownloadManagerDelegateForTesting(nullptr);
  }

  content::MockDownloadManager* download_manager() { return download_manager_; }

 private:
  raw_ptr<DownloadCoreService> service_;
  raw_ptr<content::MockDownloadManager>
      download_manager_;  // Owned by testing profile.
  raw_ptr<ChromeDownloadManagerDelegate> chrome_download_manager_delegate_;
};

}  // namespace

ACTION(QuitMainMessageLoop) {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

class PersonalDataLoadedObserverMock
    : public autofill::PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() = default;
  ~PersonalDataLoadedObserverMock() override = default;
  MOCK_METHOD0(OnPersonalDataChanged, void());
  MOCK_METHOD0(OnPersonalDataFinishedProfileTasks, void());
};

// RemoveAutofillTester is not a part of the anonymous namespace above, as
// PersonalDataManager declares it a friend in an empty namespace.
class RemoveAutofillTester {
 public:
  explicit RemoveAutofillTester(TestingProfile* profile)
      : personal_data_manager_(
            autofill::PersonalDataManagerFactory::GetForProfile(profile)) {
    autofill::test::DisableSystemServices(profile->GetPrefs());
    personal_data_manager_->AddObserver(&personal_data_observer_);
  }

  RemoveAutofillTester(const RemoveAutofillTester&) = delete;
  RemoveAutofillTester& operator=(const RemoveAutofillTester&) = delete;

  ~RemoveAutofillTester() {
    personal_data_manager_->RemoveObserver(&personal_data_observer_);
    autofill::test::ReenableSystemServices();
  }

  // Returns true if there are autofill profiles.
  bool HasProfile() {
    return !personal_data_manager_->GetProfiles().empty() &&
           !personal_data_manager_->GetCreditCards().empty();
  }

  bool HasOrigin(const std::string& origin) {
    const std::vector<autofill::AutofillProfile*>& profiles =
        personal_data_manager_->GetProfiles();
    for (const autofill::AutofillProfile* profile : profiles) {
      if (profile->origin() == origin)
        return true;
    }

    const std::vector<autofill::CreditCard*>& credit_cards =
        personal_data_manager_->GetCreditCards();
    for (const autofill::CreditCard* credit_card : credit_cards) {
      if (credit_card->origin() == origin)
        return true;
    }

    return false;
  }

  // Add two profiles and two credit cards to the database.  In each pair, one
  // entry has a web origin and the other has a Chrome origin.
  void AddProfilesAndCards() {
    std::vector<autofill::AutofillProfile> profiles;
    autofill::AutofillProfile profile;
    profile.set_guid(base::GenerateGUID());
    profile.set_origin(kWebOrigin);
    profile.SetRawInfo(autofill::NAME_FIRST, u"Bob");
    profile.SetRawInfo(autofill::NAME_LAST, u"Smith");
    profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, u"94043");
    profile.SetRawInfo(autofill::EMAIL_ADDRESS, u"sue@example.com");
    profile.SetRawInfo(autofill::COMPANY_NAME, u"Company X");
    profiles.push_back(profile);

    profile.set_guid(base::GenerateGUID());
    profile.set_origin(autofill::kSettingsOrigin);
    profiles.push_back(profile);

    personal_data_manager_->SetProfiles(&profiles);

    WaitForOnPersonalDataFinishedProfileTasks();

    std::vector<autofill::CreditCard> cards;
    autofill::CreditCard card;
    card.set_guid(base::GenerateGUID());
    card.set_origin(kWebOrigin);
    card.SetRawInfo(autofill::CREDIT_CARD_NUMBER, u"1234-5678-9012-3456");
    cards.push_back(card);

    card.set_guid(base::GenerateGUID());
    card.set_origin(autofill::kSettingsOrigin);
    cards.push_back(card);

    personal_data_manager_->SetCreditCards(&cards);
    WaitForOnPersonalDataChanged();
  }

 private:
  void WaitForOnPersonalDataChanged() {
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMainMessageLoop());
    base::RunLoop().Run();
  }

  void WaitForOnPersonalDataFinishedProfileTasks() {
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillRepeatedly(QuitMainMessageLoop());
    base::RunLoop().Run();
  }

  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  testing::NiceMock<PersonalDataLoadedObserverMock> personal_data_observer_;
};

#if BUILDFLAG(ENABLE_REPORTING)
class MockReportingService : public net::ReportingService {
 public:
  MockReportingService() = default;

  MockReportingService(const MockReportingService&) = delete;
  MockReportingService& operator=(const MockReportingService&) = delete;

  ~MockReportingService() override = default;

  // net::ReportingService implementation:

  void SetDocumentReportingEndpoints(
      const base::UnguessableToken& reporting_source,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      const base::flat_map<std::string, std::string>& endpoints) override {
    NOTREACHED();
  }

  void SendReportsAndRemoveSource(
      const base::UnguessableToken& reporting_source) override {
    NOTREACHED();
  }

  void QueueReport(
      const GURL& url,
      const absl::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkIsolationKey& network_isolation_key,
      const std::string& user_agent,
      const std::string& group,
      const std::string& type,
      std::unique_ptr<const base::Value> body,
      int depth) override {
    NOTREACHED();
  }

  void ProcessReportToHeader(
      const url::Origin& origin,
      const net::NetworkIsolationKey& network_isolation_key,
      const std::string& header_value) override {
    NOTREACHED();
  }

  void RemoveBrowsingData(uint64_t data_type_mask,
                          const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    ++remove_calls_;
    last_data_type_mask_ = data_type_mask;
    last_origin_filter_ = origin_filter;
  }

  void RemoveAllBrowsingData(uint64_t data_type_mask) override {
    ++remove_all_calls_;
    last_data_type_mask_ = data_type_mask;
    last_origin_filter_ = base::RepeatingCallback<bool(const GURL&)>();
  }

  void OnShutdown() override {}

  const net::ReportingPolicy& GetPolicy() const override {
    static net::ReportingPolicy dummy_policy_;
    NOTREACHED();
    return dummy_policy_;
  }

  net::ReportingContext* GetContextForTesting() const override {
    NOTREACHED();
    return nullptr;
  }

  std::vector<const net::ReportingReport*> GetReports() const override {
    NOTREACHED();
    return std::vector<const net::ReportingReport*>();
  }

  base::flat_map<url::Origin, std::vector<net::ReportingEndpoint>>
  GetV1ReportingEndpointsByOrigin() const override {
    NOTREACHED();
    return base::flat_map<url::Origin, std::vector<net::ReportingEndpoint>>();
  }

  void AddReportingCacheObserver(
      net::ReportingCacheObserver* observer) override {}

  void RemoveReportingCacheObserver(
      net::ReportingCacheObserver* observer) override {}

  int remove_calls() const { return remove_calls_; }
  int remove_all_calls() const { return remove_all_calls_; }
  uint64_t last_data_type_mask() const { return last_data_type_mask_; }
  const base::RepeatingCallback<bool(const GURL&)>& last_origin_filter() const {
    return last_origin_filter_;
  }

 private:
  int remove_calls_ = 0;
  int remove_all_calls_ = 0;
  uint64_t last_data_type_mask_ = 0;
  base::RepeatingCallback<bool(const GURL&)> last_origin_filter_;
};

class ClearReportingCacheTester {
 public:
  ClearReportingCacheTester(network::NetworkContext* network_context,
                            bool create_service)
      : url_request_context_(network_context->url_request_context()) {
    if (create_service)
      service_ = std::make_unique<MockReportingService>();

    old_service_ = url_request_context_->reporting_service();
    url_request_context_->set_reporting_service(service_.get());
  }

  ~ClearReportingCacheTester() {
    DCHECK_EQ(service_.get(), url_request_context_->reporting_service());
    url_request_context_->set_reporting_service(old_service_);
  }

  const MockReportingService& mock() { return *service_; }

 private:
  raw_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<MockReportingService> service_;
  raw_ptr<net::ReportingService> old_service_;
};

class MockNetworkErrorLoggingService : public net::NetworkErrorLoggingService {
 public:
  MockNetworkErrorLoggingService() = default;

  MockNetworkErrorLoggingService(const MockNetworkErrorLoggingService&) =
      delete;
  MockNetworkErrorLoggingService& operator=(
      const MockNetworkErrorLoggingService&) = delete;

  ~MockNetworkErrorLoggingService() override = default;

  // net::NetworkErrorLoggingService implementation:

  void OnHeader(const net::NetworkIsolationKey& network_isolation_key,
                const url::Origin& origin,
                const net::IPAddress& received_ip_address,
                const std::string& value) override {
    NOTREACHED();
  }

  void OnRequest(RequestDetails details) override { NOTREACHED(); }

  void QueueSignedExchangeReport(SignedExchangeReportDetails details) override {
    NOTREACHED();
  }

  void RemoveBrowsingData(const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    ++remove_calls_;
    last_origin_filter_ = origin_filter;
  }

  void RemoveAllBrowsingData() override {
    ++remove_all_calls_;
    last_origin_filter_ = base::RepeatingCallback<bool(const GURL&)>();
  }

  int remove_calls() const { return remove_calls_; }
  int remove_all_calls() const { return remove_all_calls_; }
  const base::RepeatingCallback<bool(const GURL&)>& last_origin_filter() const {
    return last_origin_filter_;
  }

 private:
  int remove_calls_ = 0;
  int remove_all_calls_ = 0;
  base::RepeatingCallback<bool(const GURL&)> last_origin_filter_;
};

class ClearNetworkErrorLoggingTester {
 public:
  ClearNetworkErrorLoggingTester(network::NetworkContext* network_context,
                                 bool create_service)
      : url_request_context_(network_context->url_request_context()) {
    if (create_service)
      service_ = std::make_unique<MockNetworkErrorLoggingService>();

    url_request_context_->set_network_error_logging_service(service_.get());
  }

  ClearNetworkErrorLoggingTester(const ClearNetworkErrorLoggingTester&) =
      delete;
  ClearNetworkErrorLoggingTester& operator=(
      const ClearNetworkErrorLoggingTester&) = delete;

  ~ClearNetworkErrorLoggingTester() {
    DCHECK_EQ(service_.get(),
              url_request_context_->network_error_logging_service());
    url_request_context_->set_network_error_logging_service(nullptr);
  }

  const MockNetworkErrorLoggingService& mock() { return *service_; }

 private:
  raw_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<MockNetworkErrorLoggingService> service_;
};
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace autofill {

// StrikeDatabaseTester is in the autofill namespace since
// StrikeDatabase declares it as a friend in the autofill namespace.
class StrikeDatabaseTester {
 public:
  explicit StrikeDatabaseTester(Profile* profile)
      : strike_database_(
            autofill::StrikeDatabaseFactory::GetForProfile(profile)) {}

  bool IsEmpty() {
    int num_keys;
    base::RunLoop run_loop;
    strike_database_->LoadKeys(base::BindLambdaForTesting(
        [&](bool success, std::unique_ptr<std::vector<std::string>> keys) {
          num_keys = keys.get()->size();
          run_loop.Quit();
        }));
    run_loop.Run();
    return (num_keys == 0);
  }

 private:
  raw_ptr<autofill::StrikeDatabase> strike_database_;
};

}  // namespace autofill

// Test Class -----------------------------------------------------------------

class ChromeBrowsingDataRemoverDelegateTest : public testing::Test {
 public:
  ChromeBrowsingDataRemoverDelegateTest() = default;
  ChromeBrowsingDataRemoverDelegateTest(
      const ChromeBrowsingDataRemoverDelegateTest&) = delete;
  ChromeBrowsingDataRemoverDelegateTest& operator=(
      const ChromeBrowsingDataRemoverDelegateTest&) = delete;
  ~ChromeBrowsingDataRemoverDelegateTest() override = default;

  void SetUp() override {
    // Make sure the Network Service is started before making a NetworkContext.
    content::GetNetworkService();
    task_environment_.RunUntilIdle();

    // This needs to be done after the test constructor, so that subclasses
    // that initialize a ScopedFeatureList in their constructors can do so
    // before the code below potentially kicks off tasks on other threads that
    // check if a feature is enabled, to avoid tsan data races.
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        StatefulSSLHostStateDelegateFactory::GetInstance(),
        StatefulSSLHostStateDelegateFactory::GetDefaultFactoryForTesting());
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        FaviconServiceFactory::GetInstance(),
        FaviconServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        SpellcheckServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* profile)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<SpellcheckService>(
              static_cast<Profile*>(profile));
        }));
    profile_builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                                      SyncServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    profile_ = profile_builder.Build();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
    web_app_provider_ = web_app::FakeWebAppProvider::Get(profile_.get());
    web_app_provider_->SkipAwaitingExtensionSystem();
    web_app_provider_->StartWithSubsystems();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)

    remover_ = profile_->GetBrowsingDataRemover();

    auto network_context_params = network::mojom::NetworkContextParams::New();
    network_context_params->cert_verifier_params =
        content::GetCertVerifierParams(
            cert_verifier::mojom::CertVerifierCreationParams::New());
    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    network_context_ = std::make_unique<network::NetworkContext>(
        network::NetworkService::GetNetworkServiceForTesting(),
        network_context_remote.InitWithNewPipeAndPassReceiver(),
        std::move(network_context_params));
    profile_->GetDefaultStoragePartition()->SetNetworkContextForTesting(
        std::move(network_context_remote));

    ProtocolHandlerRegistryFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildProtocolHandlerRegistry));

#if BUILDFLAG(IS_ANDROID)
    static_cast<ChromeBrowsingDataRemoverDelegate*>(
        profile_->GetBrowsingDataRemoverDelegate())
        ->OverrideWebappRegistryForTesting(
            std::make_unique<TestWebappRegistry>());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::TpmManagerClient::InitializeFake();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void TearDown() override {
    // Destroying the profile triggers a call to leveldb_proto::
    // ProtoDatabaseProvider::SetSharedDBDeleteObsoleteDelayForTesting, which
    // can race with leveldb_proto::SharedProtoDatabase::OnDatabaseInit
    // on another thread.  Allowing those tasks to complete before we destroy
    // the profile should fix the race.
    content::RunAllTasksUntilIdle();

    // TestingProfile contains a DOMStorageContext.  BrowserContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    base::RunLoop().RunUntilIdle();

    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  // Returns the set of data types for which the deletion failed.
  uint64_t BlockUntilBrowsingDataRemoved(const base::Time& delete_begin,
                                         const base::Time& delete_end,
                                         uint64_t remove_mask,
                                         bool include_protected_origins) {
    uint64_t origin_type_mask =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    if (include_protected_origins) {
      origin_type_mask |=
          content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    }

    content::BrowsingDataRemoverCompletionObserver completion_observer(
        remover_);
    remover_->RemoveAndReply(delete_begin, delete_end, remove_mask,
                             origin_type_mask, &completion_observer);
    base::ThreadPoolInstance::Get()->FlushForTesting();
    completion_observer.BlockUntilCompletion();
    return completion_observer.failed_data_types();
  }

  void ExpectRemoveLoginsByURLAndTime(
      password_manager::MockPasswordStoreInterface* store) {
    EXPECT_CALL(*store, RemoveLoginsByURLAndTime)
        .WillOnce(
            testing::WithArgs<3, 4>([](auto callback, auto sync_callback) {
              std::move(callback).Run();
              if (sync_callback)
                std::move(sync_callback).Run(false);
            }));
  }

  void ExpectRemoveLoginsByURLAndTimeWithFilter(
      password_manager::MockPasswordStoreInterface* store,
      base::RepeatingCallback<bool(const GURL&)> filter) {
    EXPECT_CALL(*store, RemoveLoginsByURLAndTime(ProbablySameFilter(filter), _,
                                                 _, _, _))
        .WillOnce(
            testing::WithArgs<3, 4>([](auto callback, auto sync_callback) {
              std::move(callback).Run();
              if (sync_callback)
                std::move(sync_callback).Run(false);
            }));
  }

  void BlockUntilOriginDataRemoved(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    content::BrowsingDataRemoverCompletionObserver completion_observer(
        remover_);
    remover_->RemoveWithFilterAndReply(
        delete_begin, delete_end, remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    base::ThreadPoolInstance::Get()->FlushForTesting();
    completion_observer.BlockUntilCompletion();
  }

  const base::Time& GetBeginTime() {
    return remover_->GetLastUsedBeginTimeForTesting();
  }

  uint64_t GetRemovalMask() {
    return remover_->GetLastUsedRemovalMaskForTesting();
  }

  uint64_t GetOriginTypeMask() {
    return remover_->GetLastUsedOriginTypeMaskForTesting();
  }

  network::NetworkContext* network_context() { return network_context_.get(); }

  TestingProfile* GetProfile() { return profile_.get(); }

  bool Match(const GURL& origin,
             uint64_t mask,
             storage::SpecialStoragePolicy* policy) {
    return remover_->DoesOriginMatchMaskForTesting(
        mask, url::Origin::Create(origin), policy);
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::FakeWebAppProvider* web_app_provider() { return web_app_provider_; }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)

 protected:
  // |feature_list_| needs to be destroyed after |task_environment_|, to avoid
  // tsan flakes caused by other tasks running while |feature_list_| is
  // destroyed.
  base::test::ScopedFeatureList feature_list_;

 private:
  // Cached pointer to BrowsingDataRemover for access to testing methods.
  raw_ptr<content::BrowsingDataRemover> remover_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<network::NetworkContext> network_context_;
  std::unique_ptr<TestingProfile> profile_;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::FakeWebAppProvider* web_app_provider_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
};

// TODO(crbug.com/812589): Disabled due to flakiness in cookie store
//                         initialization.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemoveSafeBrowsingCookieForever) {
  RemoveSafeBrowsingCookieTester tester(GetProfile());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

// TODO(crbug.com/812589): Disabled due to flakiness in cookie store
//                         initialization.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemoveSafeBrowsingCookieLastHour) {
  RemoveSafeBrowsingCookieTester tester(GetProfile());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  // Removing with time period other than all time should not clear safe
  // browsing cookies.
  EXPECT_TRUE(tester.ContainsCookie());
}

// TODO(crbug.com/812589): Disabled due to flakiness in cookie store
//                         initialization.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemoveSafeBrowsingCookieForeverWithPredicate) {
  RemoveSafeBrowsingCookieTester tester(GetProfile());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_TRUE(tester.ContainsCookie());

  std::unique_ptr<BrowsingDataFilterBuilder> filter2(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  filter2->AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter2));
  EXPECT_FALSE(tester.ContainsCookie());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, ClearWebAppData) {
  auto* provider = web_app_provider();
  ASSERT_TRUE(provider);

  // Make sure WebAppProvider's subsystems are ready.
  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Set-up: add a web app to the registry. Currently, only last_launch_time
  // and last_badging_time fields are being cleared by ClearBrowsingDataCommand.
  // So, we will check if these fields are cleared as a heuristic to
  // ClearBrowsingDataCommand being called.
  auto web_app_id = web_app::test::InstallDummyWebApp(GetProfile(), "Web App",
                                                      GURL("http://some.url"));
  auto last_launch_time = base::Time() + base::Seconds(10);
  provider->sync_bridge().SetAppLastLaunchTime(web_app_id, last_launch_time);
  EXPECT_EQ(provider->registrar().GetAppById(web_app_id)->last_launch_time(),
            last_launch_time);
  auto last_badging_time = base::Time() + base::Seconds(20);
  provider->sync_bridge().SetAppLastBadgingTime(web_app_id, last_badging_time);
  EXPECT_EQ(provider->registrar().GetAppById(web_app_id)->last_badging_time(),
            last_badging_time);

  // Run RemoveEmbedderData, and wait for it to complete.
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);

  // Verify that web app's last launch time is cleared.
  EXPECT_EQ(provider->registrar().GetAppById(web_app_id)->last_launch_time(),
            base::Time());
  // Verify that web app's last badging time is cleared.
  EXPECT_EQ(provider->registrar().GetAppById(web_app_id)->last_badging_time(),
            base::Time());

  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForever) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  const GURL kOrigin1("http://host1.com:1");
  tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForLastHour) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::Hours(2);

  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForOlderThan30Days) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time older_than_29days = base::Time::Now() - base::Days(29);
  base::Time older_than_30days = base::Time::Now() - base::Days(30);
  base::Time older_than_31days = base::Time::Now() - base::Days(31);

  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  const GURL kOrigin3("http://host3.com:1");
  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, older_than_29days);
  tester.AddHistory(kOrigin3, older_than_31days);

  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin3));

  BlockUntilBrowsingDataRemoved(base::Time(), older_than_30days,
                                constants::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin3));
}

// This should crash (DCHECK) in Debug, but death tests don't work properly
// here.
// TODO(msramek): To make this testable, the refusal to delete history should
// be made a part of interface (e.g. a success value) as opposed to a DCHECK.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryProhibited) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  base::Time two_hours_ago = base::Time::Now() - base::Hours(2);

  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Nothing should have been deleted.
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveMultipleTypesHistoryProhibited) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  // Add some history.
  const GURL kOrigin1("http://host1.com:1");
  RemoveHistoryTester history_tester;
  ASSERT_TRUE(history_tester.Init(GetProfile()));
  history_tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  // Expect that passwords will be deleted, as they do not depend
  // on |prefs::kAllowDeletingBrowserHistory|.
  RemovePasswordsTester tester(GetProfile());
  ExpectRemoveLoginsByURLAndTime(tester.profile_store());

  uint64_t removal_mask =
      constants::DATA_TYPE_HISTORY | constants::DATA_TYPE_PASSWORDS;

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(), removal_mask,
                                false);
  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that history was not deleted.
  EXPECT_TRUE(history_tester.HistoryContainsURL(kOrigin1));
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveExternalProtocolData) {
  TestingProfile* profile = GetProfile();
  url::Origin test_origin = url::Origin::Create(GURL("https://example.test"));
  const std::string serialized_test_origin = test_origin.Serialize();
  // Add external protocol data on profile.
  base::Value prefs(base::Value::Type::DICTIONARY);
  prefs.SetKey(serialized_test_origin,
               base::Value(base::Value::Type::DICTIONARY));
  base::Value* allowed_protocols_for_origin =
      prefs.FindDictKey(serialized_test_origin);
  allowed_protocols_for_origin->SetBoolKey("tel", true);
  profile->GetPrefs()->Set(prefs::kProtocolHandlerPerOriginAllowedProtocols,
                           prefs);

  EXPECT_FALSE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kProtocolHandlerPerOriginAllowedProtocols)
          ->DictEmpty());

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_EXTERNAL_PROTOCOL_DATA,
                                false);
  EXPECT_TRUE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kProtocolHandlerPerOriginAllowedProtocols)
          ->DictEmpty());
}

// Check that clearing browsing data (either history or cookies with other site
// data) clears any saved isolated origins.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePersistentIsolatedOrigins) {
  PrefService* prefs = GetProfile()->GetPrefs();

  // Add foo.com to the list of stored user-triggered isolated origins and
  // bar.com to the list of stored web-triggered isolated origins.
  base::Value list(base::Value::Type::LIST);
  list.Append("http://foo.com");
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("https://bar.com", base::TimeToValue(base::Time::Now()));
  prefs->Set(site_isolation::prefs::kWebTriggeredIsolatedOrigins, dict);
  EXPECT_FALSE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());

  // Clear history and ensure the stored isolated origins are cleared.
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  EXPECT_TRUE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());

  // Re-add foo.com and bar.com to stored isolated origins.
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  prefs->Set(site_isolation::prefs::kWebTriggeredIsolatedOrigins, dict);
  EXPECT_FALSE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());

  // Now clear cookies and other site data, and ensure foo.com is cleared.
  // Note that this uses a short time period to document that time ranges are
  // currently ignored by stored isolated origins.
  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_SITE_DATA, false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  EXPECT_TRUE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());

  // Re-add foo.com and bar.com.
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  prefs->Set(site_isolation::prefs::kWebTriggeredIsolatedOrigins, dict);
  EXPECT_FALSE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());

  // Clear the isolated origins data type.
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_ISOLATED_ORIGINS, false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  EXPECT_TRUE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());

  // Re-add foo.com and bar.com.
  prefs->Set(site_isolation::prefs::kUserTriggeredIsolatedOrigins, list);
  EXPECT_FALSE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  prefs->Set(site_isolation::prefs::kWebTriggeredIsolatedOrigins, dict);
  EXPECT_FALSE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());

  // Clear both history and site data, and ensure the stored isolated origins
  // are cleared.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      constants::DATA_TYPE_HISTORY | constants::DATA_TYPE_SITE_DATA, false);
  EXPECT_TRUE(
      prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins)
          ->GetListDeprecated()
          .empty());
  EXPECT_TRUE(
      prefs->GetDictionary(site_isolation::prefs::kWebTriggeredIsolatedOrigins)
          ->DictEmpty());
}

// Test that clearing history deletes favicons not associated with bookmarks.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveFaviconsForever) {
  GURL page_url("http://a");

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(page_url);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(page_url));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_FALSE(favicon_tester.HasFaviconForPageURL(page_url));
}

// Test that a bookmark's favicon is expired and not deleted when clearing
// history. Expiring the favicon causes the bookmark's favicon to be updated
// when the user next visits the bookmarked page. Expiring the bookmark's
// favicon is useful when the bookmark's favicon becomes incorrect (See
// crbug.com/474421 for a sample bug which causes this).
TEST_F(ChromeBrowsingDataRemoverDelegateTest, ExpireBookmarkFavicons) {
  GURL bookmarked_page("http://a");

  TestingProfile* profile = GetProfile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"a",
                         bookmarked_page);

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(bookmarked_page);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(bookmarked_page));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_TRUE(favicon_tester.HasExpiredFaviconForPageURL(bookmarked_page));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DeleteBookmarks) {
  GURL bookmarked_page("http://a");

  TestingProfile* profile = GetProfile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"a",
                         bookmarked_page);
  EXPECT_EQ(1u, bookmark_model->bookmark_bar_node()->children().size());
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_BOOKMARKS, false);
  EXPECT_EQ(0u, bookmark_model->bookmark_bar_node()->children().size());
}

// Verifies deleting does not crash if BookmarkModel has not been loaded.
// Regression test for: https://crbug.com/1207632.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DeleteBookmarksDoesNothingWhenModelNotLoaded) {
  TestingProfile* profile = GetProfile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  // For this test to exercise the code path that lead to the crash the
  // model must not be loaded yet.
  EXPECT_FALSE(bookmark_model->loaded());
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_BOOKMARKS, false);
  // No crash means test passes.
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_TimeBasedHistoryRemoval) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::Hours(2);

  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                              constants::DATA_TYPE_HISTORY, std::move(builder));

  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// Verify that clearing autofill form data works.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalLastHour) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  SyncServiceFactory::GetForProfile(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_FORM_DATA, false);

  EXPECT_EQ(constants::DATA_TYPE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

// Verify the clearing of autofill profiles added / modified more than 30 days
// ago.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalOlderThan30Days) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  SyncServiceFactory::GetForProfile(GetProfile());

  const base::Time k32DaysOld = base::Time::Now();
  task_environment()->AdvanceClock(base::Days(1));
  const base::Time k31DaysOld = base::Time::Now();
  task_environment()->AdvanceClock(base::Days(1));
  const base::Time k30DaysOld = base::Time::Now();
  task_environment()->AdvanceClock(base::Days(30));

  // Add profiles and cards with modification date as 31 days old from now.
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(k31DaysOld);

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), k32DaysOld,
                                constants::DATA_TYPE_FORM_DATA, false);
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(k30DaysOld, base::Time::Max(),
                                constants::DATA_TYPE_FORM_DATA, false);
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), k30DaysOld,
                                constants::DATA_TYPE_FORM_DATA, false);
  EXPECT_EQ(constants::DATA_TYPE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  SyncServiceFactory::GetForProfile(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_FORM_DATA, false);

  EXPECT_EQ(constants::DATA_TYPE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       StrikeDatabaseEmptyOnAutofillRemoveEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  SyncServiceFactory::GetForProfile(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  autofill::StrikeDatabaseTester strike_database_tester(GetProfile());
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_FORM_DATA, false);

  // StrikeDatabase should be empty when DATA_TYPE_FORM_DATA browsing data
  // gets deleted.
  ASSERT_TRUE(strike_database_tester.IsEmpty());
  EXPECT_EQ(constants::DATA_TYPE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

// Verify that clearing autofill form data works.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       AutofillOriginsRemovedWithHistory) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());
  // Initialize sync service so that PersonalDatabaseHelper::server_database_
  // gets initialized:
  SyncServiceFactory::GetForProfile(GetProfile());

  tester.AddProfilesAndCards();
  EXPECT_FALSE(tester.HasOrigin(std::string()));
  EXPECT_TRUE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(constants::DATA_TYPE_HISTORY, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_TRUE(tester.HasOrigin(std::string()));
  EXPECT_FALSE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ZeroSuggestCacheClear) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults,
                   "[\"\", [\"foo\", \"bar\"]]");
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  // Expect the prefs to be cleared when cookies are removed.
  EXPECT_TRUE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ContentProtectionPlatformKeysRemoval) {
  ash::MockUserManager* mock_user_manager =
      new testing::NiceMock<ash::MockUserManager>();
  mock_user_manager->SetActiveUser(
      AccountId::FromUserEmail("test@example.com"));
  user_manager::ScopedUserManager user_manager_enabler(
      base::WrapUnique(mock_user_manager));

  chromeos::AttestationClient::InitializeFake();
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES, false);

  const std::vector<::attestation::DeleteKeysRequest>& history =
      chromeos::AttestationClient::Get()
          ->GetTestInterface()
          ->delete_keys_history();
  EXPECT_EQ(history.size(), 1);

  chromeos::AttestationClient::Shutdown();
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Null) {
  ClearDomainReliabilityTester tester(GetProfile());

  EXPECT_EQ(0u, tester.clear_count());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Beacons) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(
      network::mojom::NetworkContext::DomainReliabilityClearMode::CLEAR_BEACONS,
      tester.last_clear_mode());
  EXPECT_TRUE(tester.last_filter().is_null());
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_DomainReliability_Beacons_WithFilter) {
  ClearDomainReliabilityTester tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              constants::DATA_TYPE_HISTORY, builder->Copy());
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(
      network::mojom::NetworkContext::DomainReliabilityClearMode::CLEAR_BEACONS,
      tester.last_clear_mode());
  EXPECT_TRUE(
      ProbablySameFilters(builder->BuildUrlFilter(), tester.last_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Contexts) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
  EXPECT_TRUE(tester.last_filter().is_null());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DomainReliability_Contexts_WithFilter) {
  ClearDomainReliabilityTester tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              builder->Copy());
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
  EXPECT_TRUE(
      ProbablySameFilters(builder->BuildUrlFilter(), tester.last_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_ContextsWin) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      constants::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DomainReliability_ProtectedOrigins) {
  ClearDomainReliabilityTester tester(GetProfile());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                true);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(network::mojom::NetworkContext::DomainReliabilityClearMode::
                CLEAR_CONTEXTS,
            tester.last_clear_mode());
}

// TODO(juliatuttle): This isn't actually testing the no-monitor case, since
// BrowsingDataRemoverTest now creates one unconditionally, since it's needed
// for some unrelated test cases. This should be fixed so it tests the no-
// monitor case again.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_DomainReliability_NoMonitor) {
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      constants::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      false);
}

// Tests that the deletion of downloads completes successfully and that
// ChromeDownloadManagerDelegate is correctly created and shut down.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveDownloads) {
  RemoveDownloadsTester tester(GetProfile());
  EXPECT_CALL(*tester.download_manager(), RemoveDownloadsByURLAndTime(_, _, _));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePasswordStatistics) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter;

  ON_CALL(*tester.profile_store(), GetSmartBubbleStatsStore)
      .WillByDefault(Return(tester.mock_smart_bubble_stats_store()));
  ON_CALL(*tester.profile_store(), GetFieldInfoStore)
      .WillByDefault(Return(tester.mock_field_info_store()));
  EXPECT_CALL(
      *tester.mock_smart_bubble_stats_store(),
      RemoveStatisticsByOriginAndTime(ProbablySameFilter(empty_filter),
                                      base::Time(), base::Time::Max(), _))
      .WillOnce(testing::WithArg<3>([](base::OnceClosure completion) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(completion));
      }));
  EXPECT_CALL(*tester.mock_field_info_store(),
              RemoveFieldInfoByTime(base::Time(), base::Time::Max(), _))
      .WillOnce(testing::WithArg<2>([](base::OnceClosure completion) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(completion));
      }));
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemovePasswordStatisticsByOrigin) {
  RemovePasswordsTester tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::RepeatingCallback<bool(const GURL&)> filter = builder->BuildUrlFilter();

  ON_CALL(*tester.profile_store(), GetSmartBubbleStatsStore)
      .WillByDefault(Return(tester.mock_smart_bubble_stats_store()));
  EXPECT_CALL(
      *tester.mock_smart_bubble_stats_store(),
      RemoveStatisticsByOriginAndTime(ProbablySameFilter(filter), base::Time(),
                                      base::Time::Max(), _))
      .WillOnce(testing::WithArg<3>([](base::OnceClosure completion) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(completion));
      }));

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              constants::DATA_TYPE_HISTORY, std::move(builder));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePasswordsByTimeOnly) {
  RemovePasswordsTester tester(GetProfile());

  ExpectRemoveLoginsByURLAndTime(tester.profile_store());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_PASSWORDS, false);
}

// Disabled, since passwords are not yet marked as a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemovePasswordsByOrigin) {
  RemovePasswordsTester tester(GetProfile());
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::RepeatingCallback<bool(const GURL&)> filter = builder->BuildUrlFilter();

  ExpectRemoveLoginsByURLAndTimeWithFilter(tester.profile_store(), filter);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              constants::DATA_TYPE_PASSWORDS,
                              std::move(builder));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DisableAutoSignIn) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.profile_store(),
              DisableAutoSignInForOrigins(ProbablySameFilter(empty_filter), _))
      .WillOnce(testing::WithArg<1>([](base::OnceClosure completion) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(completion));
      }));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DisableAutoSignInAfterRemovingPasswords) {
  RemovePasswordsTester tester(GetProfile());
  base::RepeatingCallback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  ExpectRemoveLoginsByURLAndTime(tester.profile_store());
  EXPECT_CALL(*tester.profile_store(),
              DisableAutoSignInForOrigins(ProbablySameFilter(empty_filter), _))
      .WillOnce(testing::WithArg<1>([](base::OnceClosure completion) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(completion));
      }));
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          constants::DATA_TYPE_PASSWORDS,
      false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DisableAutoSignInCrossSiteClearSiteData) {
  RemovePasswordsTester tester(GetProfile());
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  filter->AddRegisterableDomain("cookie.com");
  filter->SetCookiePartitionKeyCollection(net::CookiePartitionKeyCollection(
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://notcookie.com"))));
  EXPECT_CALL(*tester.profile_store(), DisableAutoSignInForOrigins).Times(0);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveContentSettingsWithPreserveFilter) {
  // This test relies on async loading to complete. RunUntilIdle() should be
  // removed and an explicit wait should be added.
  task_environment()->RunUntilIdle();

  // Add our settings.
  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  const GURL kOrigin3("http://host3.com:1");
  const GURL kOrigin4("https://host3.com:1");
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin1, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(base::Value::Type::DICTIONARY));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin2, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(base::Value::Type::DICTIONARY));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin3, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(base::Value::Type::DICTIONARY));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin4, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(base::Value::Type::DICTIONARY));

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                              constants::DATA_TYPE_SITE_USAGE_DATA,
                              std::move(filter));

  EXPECT_EQ(constants::DATA_TYPE_SITE_USAGE_DATA, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify we only have true, and they're origin1, origin3, and origin4.
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, &host_settings);
  EXPECT_EQ(3u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin1),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin4),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin3),
            host_settings[2].primary_pattern)
      << host_settings[2].primary_pattern.ToString();
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveContentSettings) {
  // This test relies on async loading to complete. RunUntilIdle() should be
  // removed and an explicit wait should be added.
  task_environment()->RunUntilIdle();

  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  const GURL kOrigin3("http://host3.com:1");
  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingDefaultScope(kOrigin1, kOrigin1,
                                     ContentSettingsType::GEOLOCATION,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(kOrigin2, kOrigin2,
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(
      kOrigin3, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  map->SetContentSettingCustomScope(pattern, ContentSettingsPattern::Wildcard(),
                                    ContentSettingsType::COOKIES,
                                    CONTENT_SETTING_BLOCK);
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);

  ContentSettingsForOneType host_settings;
  map->GetSettingsForOneType(ContentSettingsType::GEOLOCATION, &host_settings);

  ASSERT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[0].GetContentSetting());

  map->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS,
                             &host_settings);

  ASSERT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[0].GetContentSetting());

  map->GetSettingsForOneType(ContentSettingsType::COOKIES, &host_settings);
  ASSERT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveProtocolHandler) {
  // This test relies on async loading to complete. RunUntilIdle() should be
  // removed and an explicit wait should be added.
  task_environment()->RunUntilIdle();

  auto* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(GetProfile());
  const GURL kOrigin("https://host3.com:1");
  base::Time one_hour_ago = base::Time::Now() - base::Hours(1);
  base::Time yesterday = base::Time::Now() - base::Days(1);
  registry->OnAcceptRegisterProtocolHandler(
      content::ProtocolHandler::CreateProtocolHandler("news", kOrigin));
  registry->OnAcceptRegisterProtocolHandler(
      content::ProtocolHandler("mailto", kOrigin, yesterday,
                               blink::ProtocolHandlerSecurityLevel::kStrict));
  EXPECT_TRUE(registry->IsHandledProtocol("news"));
  EXPECT_TRUE(registry->IsHandledProtocol("mailto"));
  EXPECT_EQ(
      2U,
      registry->GetUserDefinedHandlers(base::Time(), base::Time::Max()).size());
  // Delete last hour.
  BlockUntilBrowsingDataRemoved(one_hour_ago, base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_FALSE(registry->IsHandledProtocol("news"));
  EXPECT_TRUE(registry->IsHandledProtocol("mailto"));
  EXPECT_EQ(
      1U,
      registry->GetUserDefinedHandlers(base::Time(), base::Time::Max()).size());
  // Delete everything.
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_FALSE(registry->IsHandledProtocol("news"));
  EXPECT_FALSE(registry->IsHandledProtocol("mailto"));
  EXPECT_EQ(
      0U,
      registry->GetUserDefinedHandlers(base::Time(), base::Time::Max()).size());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveSelectedClientHints) {
  // This test relies on async loading to complete. RunUntilIdle() should be
  // removed and an explicit wait should be added.
  task_environment()->RunUntilIdle();

  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  base::Value client_hints_list(base::Value::Type::LIST);
  client_hints_list.Append(0);
  client_hints_list.Append(2);

  base::Value client_hints_dictionary(base::Value::Type::DICTIONARY);
  client_hints_dictionary.SetKey(client_hints::kClientHintsSettingKey,
                                 std::move(client_hints_list));

  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  const GURL kOrigin3("http://host3.com:1");
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin1, GURL(), ContentSettingsType::CLIENT_HINTS,
      client_hints_dictionary.Clone());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin2, GURL(), ContentSettingsType::CLIENT_HINTS,
      client_hints_dictionary.Clone());

  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin3, GURL(), ContentSettingsType::CLIENT_HINTS,
      client_hints_dictionary.Clone());

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));

  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &host_settings);

  ASSERT_EQ(2u, host_settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin1),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin3),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();

  for (const auto& setting : host_settings) {
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), setting.secondary_pattern);
    EXPECT_EQ(client_hints_dictionary, setting.setting_value);
  }
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveAllClientHints) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  base::Value client_hints_list(base::Value::Type::LIST);
  client_hints_list.Append(0);
  client_hints_list.Append(2);

  base::Value client_hints_dictionary(base::Value::Type::DICTIONARY);
  client_hints_dictionary.SetKey(client_hints::kClientHintsSettingKey,
                                 std::move(client_hints_list));

  host_content_settings_map->SetWebsiteSettingDefaultScope(
      GURL("http://host1.com:1"), GURL(), ContentSettingsType::CLIENT_HINTS,
      client_hints_dictionary.Clone());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      GURL("http://host2.com:1"), GURL(), ContentSettingsType::CLIENT_HINTS,
      client_hints_dictionary.Clone());

  host_content_settings_map->SetWebsiteSettingDefaultScope(
      GURL("http://host3.com:1"), GURL(), ContentSettingsType::CLIENT_HINTS,
      client_hints_dictionary.Clone());

  // Clear all.
  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &host_settings);

  ASSERT_EQ(0u, host_settings.size());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveZoomLevel) {
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(GetProfile());
  EXPECT_EQ(0u, zoom_map->GetAllZoomLevels().size());

  base::SimpleTestClock test_clock;
  zoom_map->SetClockForTesting(&test_clock);

  base::Time now = base::Time::Now();
  zoom_map->InitializeZoomLevelForHost(kTestRegisterableDomain1, 1.5,
                                       now - base::Hours(5));
  test_clock.SetNow(now - base::Hours(2));
  zoom_map->SetZoomLevelForHost(kTestRegisterableDomain3, 2.0);
  EXPECT_EQ(2u, zoom_map->GetAllZoomLevels().size());

  // Remove everything created during the last hour.
  BlockUntilBrowsingDataRemoved(now - base::Hours(1), base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);

  // Nothing should be deleted as the zoomlevels were created earlier.
  EXPECT_EQ(2u, zoom_map->GetAllZoomLevels().size());

  test_clock.SetNow(now);
  zoom_map->SetZoomLevelForHost(kTestRegisterableDomain3, 2.0);

  // Remove everything changed during the last hour (domain3).
  BlockUntilBrowsingDataRemoved(now - base::Hours(1), base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);

  // Verify we still have the zoom_level for domain1.
  auto levels = zoom_map->GetAllZoomLevels();
  EXPECT_EQ(1u, levels.size());
  EXPECT_EQ(kTestRegisterableDomain1, levels[0].host);

  zoom_map->SetZoomLevelForHostAndScheme("chrome", "print", 4.0);
  // Remove everything.
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);

  // Host and scheme zoomlevels should not be affected.
  levels = zoom_map->GetAllZoomLevels();
  EXPECT_EQ(1u, levels.size());
  EXPECT_EQ("chrome", levels[0].scheme);
  EXPECT_EQ("print", levels[0].host);
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveTranslateBlocklist) {
  auto translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetProfile()->GetPrefs());
  translate_prefs->AddSiteToNeverPromptList("google.com");
  task_environment()->AdvanceClock(base::Days(1));
  base::Time t = base::Time::Now();
  translate_prefs->AddSiteToNeverPromptList("maps.google.com");

  EXPECT_TRUE(translate_prefs->IsSiteOnNeverPromptList("google.com"));
  EXPECT_TRUE(translate_prefs->IsSiteOnNeverPromptList("maps.google.com"));

  BlockUntilBrowsingDataRemoved(t, base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_TRUE(translate_prefs->IsSiteOnNeverPromptList("google.com"));
  EXPECT_FALSE(translate_prefs->IsSiteOnNeverPromptList("maps.google.com"));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);
  EXPECT_FALSE(translate_prefs->IsSiteOnNeverPromptList("google.com"));
  EXPECT_FALSE(translate_prefs->IsSiteOnNeverPromptList("maps.google.com"));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveDurablePermission) {
  // Add our settings.
  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  DurableStoragePermissionContext durable_permission(GetProfile());
  durable_permission.UpdateContentSetting(
      kOrigin1, GURL(), CONTENT_SETTING_ALLOW, /*is_one_time=*/false);
  durable_permission.UpdateContentSetting(
      kOrigin2, GURL(), CONTENT_SETTING_ALLOW, /*is_one_time=*/false);

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                              constants::DATA_TYPE_DURABLE_PERMISSION,
                              std::move(filter));

  EXPECT_EQ(constants::DATA_TYPE_DURABLE_PERMISSION, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify we only have allow for the first origin.
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE, &host_settings);

  ASSERT_EQ(2u, host_settings.size());
  // Only the first should should have a setting.
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin1),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());

  // And our wildcard.
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[1].GetContentSetting());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DurablePermissionIsPartOfEmbedderDOMStorage) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  DurableStoragePermissionContext durable_permission(GetProfile());
  durable_permission.UpdateContentSetting(GURL("http://host1.com:1"), GURL(),
                                          CONTENT_SETTING_ALLOW,
                                          /*is_one_time=*/false);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE, &host_settings);
  EXPECT_EQ(2u, host_settings.size());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE, false);

  // After the deletion, only the wildcard should remain.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE, &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
}

// Test that removing passwords clears HTTP auth data.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ClearHttpAuthCache_RemovePasswords) {
  net::HttpNetworkSession* http_session = network_context()
                                              ->url_request_context()
                                              ->http_transaction_factory()
                                              ->GetSession();
  DCHECK(http_session);

  net::HttpAuthCache* http_auth_cache = http_session->http_auth_cache();
  const url::SchemeHostPort kSchemeHostPort(GURL("http://host1.com:1"));
  http_auth_cache->Add(kSchemeHostPort, net::HttpAuth::AUTH_SERVER, kTestRealm,
                       net::HttpAuth::AUTH_SCHEME_BASIC,
                       net::NetworkIsolationKey(), "test challenge",
                       net::AuthCredentials(u"foo", u"bar"), "/");
  CHECK(http_auth_cache->Lookup(kSchemeHostPort, net::HttpAuth::AUTH_SERVER,
                                kTestRealm, net::HttpAuth::AUTH_SCHEME_BASIC,
                                net::NetworkIsolationKey()));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_PASSWORDS, false);

  EXPECT_EQ(nullptr,
            http_auth_cache->Lookup(
                kSchemeHostPort, net::HttpAuth::AUTH_SERVER, kTestRealm,
                net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveFledgeJoinSettings) {
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(GetProfile());

  const std::string etld_one = "example.com";
  base::Time setting_time_one = base::Time::Now();
  privacy_sandbox_settings->SetFledgeJoiningAllowed(etld_one, false);
  task_environment()->AdvanceClock(base::Days(1));

  const std::string etld_two = "another-example.com";
  base::Time setting_time_two = base::Time::Now();
  privacy_sandbox_settings->SetFledgeJoiningAllowed(etld_two, false);
  task_environment()->AdvanceClock(base::Days(1));

  const std::string etld_three = "different-example.com";
  base::Time setting_time_three = base::Time::Now();
  privacy_sandbox_settings->SetFledgeJoiningAllowed(etld_three, false);

  EXPECT_FALSE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://www.example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://another-example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://different-example.com"))));

  // Apply a deletion targeting the second setting.
  BlockUntilBrowsingDataRemoved(setting_time_two - base::Seconds(1),
                                setting_time_two + base::Seconds(1),
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);

  EXPECT_FALSE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://www.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://another-example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://different-example.com"))));

  // Apply a deletion targeting the remaining settings.
  BlockUntilBrowsingDataRemoved(setting_time_one, setting_time_three,
                                constants::DATA_TYPE_CONTENT_SETTINGS, false);

  EXPECT_TRUE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://www.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://another-example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://different-example.com"))));
}

class ChromeBrowsingDataRemoverDelegateBlockPromptsTest
    : public ChromeBrowsingDataRemoverDelegateTest {
 public:
  ChromeBrowsingDataRemoverDelegateBlockPromptsTest() {
    // This needs to be done before SetUp, to avoid tsan flakes due to tasks
    // running on other threads checking if a feature is enabled.
    feature_list_.InitWithFeatures(
        {permissions::features::kBlockPromptsIfDismissedOften}, {});
  }
};

TEST_F(ChromeBrowsingDataRemoverDelegateBlockPromptsTest,
       ClearPermissionPromptCounts) {
  RemovePermissionPromptCountsTest tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_1(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  filter_builder_1->AddRegisterableDomain(kTestRegisterableDomain1);

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_2(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  filter_builder_2->AddRegisterableDomain(kTestRegisterableDomain1);

  const GURL kOrigin1("http://host1.com:1");
  const GURL kOrigin2("http://host2.com:1");
  {
    // Test REMOVE_HISTORY.
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, ContentSettingsType::NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin1, ContentSettingsType::MIDI_SYSEX));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin2, ContentSettingsType::DURABLE_STORAGE));
    tester.CheckEmbargo(kOrigin2, ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_ASK);
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin2, ContentSettingsType::NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin2, ContentSettingsType::NOTIFICATIONS));
    EXPECT_TRUE(tester.RecordDismissAndEmbargo(
        kOrigin2, ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin2, ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_BLOCK);

    BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_SITE_USAGE_DATA,
                                std::move(filter_builder_1));

    // kOrigin1 should be gone, but kOrigin2 remains.
    EXPECT_EQ(
        0, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(
        0, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        0, tester.GetDismissCount(kOrigin1, ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(1, tester.GetIgnoreCount(kOrigin2,
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(3, tester.GetDismissCount(kOrigin2,
                                        ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin2, ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_BLOCK);

    BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                  constants::DATA_TYPE_HISTORY, false);

    // Everything should be gone.
    EXPECT_EQ(
        0, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(
        0, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        0, tester.GetDismissCount(kOrigin1, ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin2,
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin2,
                                        ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin2, ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_ASK);
  }
  {
    // Test REMOVE_SITE_DATA.
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, ContentSettingsType::NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin1, ContentSettingsType::MIDI_SYSEX));
    tester.CheckEmbargo(kOrigin1, ContentSettingsType::MIDI_SYSEX,
                        CONTENT_SETTING_ASK);
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin2, ContentSettingsType::DURABLE_STORAGE));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin2, ContentSettingsType::NOTIFICATIONS));

    BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_SITE_USAGE_DATA,
                                std::move(filter_builder_2));

    // kOrigin2 should be gone, but kOrigin1 remains.
    EXPECT_EQ(
        2, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(
        1, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        1, tester.GetDismissCount(kOrigin1, ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin2,
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin2,
                                        ContentSettingsType::NOTIFICATIONS));

    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin1, ContentSettingsType::MIDI_SYSEX));
    EXPECT_TRUE(tester.RecordDismissAndEmbargo(
        kOrigin1, ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(
        3, tester.GetDismissCount(kOrigin1, ContentSettingsType::MIDI_SYSEX));
    tester.CheckEmbargo(kOrigin1, ContentSettingsType::MIDI_SYSEX,
                        CONTENT_SETTING_BLOCK);

    BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                  constants::DATA_TYPE_SITE_USAGE_DATA, false);

    // Everything should be gone.
    EXPECT_EQ(
        0, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(
        0, tester.GetIgnoreCount(kOrigin1, ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        0, tester.GetDismissCount(kOrigin1, ContentSettingsType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin2,
                                       ContentSettingsType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin2,
                                        ContentSettingsType::NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin1, ContentSettingsType::MIDI_SYSEX,
                        CONTENT_SETTING_ASK);
  }
}

// Test that the remover clears language model data (normally added by the
// LanguageDetectionDriver).
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       LanguageHistogramClearedOnClearingCompleteHistory) {
  language::UrlLanguageHistogram* language_histogram =
      UrlLanguageHistogramFactory::GetForBrowserContext(GetProfile());

  // Simulate browsing.
  for (int i = 0; i < 100; i++) {
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("es");
  }

  // Clearing a part of the history has no effect.
  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);

  EXPECT_THAT(language_histogram->GetTopLanguages(), SizeIs(2));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("en"), FloatEq(0.75));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("es"), FloatEq(0.25));

  // Clearing the full history does the trick.
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);

  EXPECT_THAT(language_histogram->GetTopLanguages(), SizeIs(0));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("en"), FloatEq(0.0));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("es"), FloatEq(0.0));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, OriginTypeMasks) {
  const GURL kOriginProtected("http://protected.com");
  const GURL kOriginUnprotected("http://unprotected.com");
  const GURL kOriginExtension("chrome-extension://abcdefghijklmnopqrstuvwxyz");
  const GURL kOriginDevTools("devtools://abcdefghijklmnopqrstuvw");

  auto mock_policy = base::MakeRefCounted<MockExtensionSpecialStoragePolicy>();
  // Protect |kOriginProtected|.
  mock_policy->AddProtected(kOriginProtected.DeprecatedGetOriginAsURL());

  EXPECT_FALSE(Match(kOriginProtected, kUnprotected, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginUnprotected, kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExtension, kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kUnprotected, mock_policy.get()));

  EXPECT_TRUE(Match(kOriginProtected, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginUnprotected, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExtension, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kProtected, mock_policy.get()));

  EXPECT_FALSE(Match(kOriginProtected, kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginUnprotected, kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExtension, kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kExtension, mock_policy.get()));

  EXPECT_TRUE(
      Match(kOriginProtected, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_TRUE(
      Match(kOriginUnprotected, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginExtension, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kUnprotected | kProtected, mock_policy.get()));

  EXPECT_FALSE(
      Match(kOriginProtected, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(
      Match(kOriginUnprotected, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(
      Match(kOriginExtension, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kUnprotected | kExtension, mock_policy.get()));

  EXPECT_TRUE(
      Match(kOriginProtected, kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginUnprotected, kProtected | kExtension, mock_policy.get()));
  EXPECT_TRUE(
      Match(kOriginExtension, kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kProtected | kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(kOriginProtected, kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_TRUE(Match(kOriginUnprotected, kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExtension, kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kUnprotected | kProtected | kExtension,
                     mock_policy.get()));
}
#endif

// If extensions are disabled, there is no policy.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, OriginTypeMasksNoPolicy) {
  const GURL kOriginStandard("http://test.com");
  const GURL kOriginExtension("chrome-extension://abcdefghijklmnopqrstuvwxyz");
  const GURL kOriginDevTools("devtools://abcdefghijklmnopqrstuvw");

  EXPECT_TRUE(Match(kOriginStandard, kUnprotected, nullptr));
  EXPECT_FALSE(Match(kOriginExtension, kUnprotected, nullptr));
  EXPECT_FALSE(Match(kOriginDevTools, kUnprotected, nullptr));

  EXPECT_FALSE(Match(kOriginStandard, kProtected, nullptr));
  EXPECT_FALSE(Match(kOriginExtension, kProtected, nullptr));
  EXPECT_FALSE(Match(kOriginDevTools, kProtected, nullptr));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_FALSE(Match(kOriginStandard, kExtension, nullptr));
  EXPECT_TRUE(Match(kOriginExtension, kExtension, nullptr));
  EXPECT_FALSE(Match(kOriginDevTools, kExtension, nullptr));
#endif
}

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, ReportingCache_NoService) {
  ClearReportingCacheTester tester(network_context(), false);

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, true);

  // Nothing to check, since there's no mock service; we're just making sure
  // nothing crashes without a service.
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ReportingCache) {
  ClearReportingCacheTester tester(network_context(), true);

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, true);

  EXPECT_EQ(0, tester.mock().remove_calls());
  EXPECT_EQ(1, tester.mock().remove_all_calls());
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
            tester.mock().last_data_type_mask());
  EXPECT_TRUE(ProbablySameFilters(base::RepeatingCallback<bool(const GURL&)>(),
                                  tester.mock().last_origin_filter()));
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_ReportingCache_WithFilter) {
  ClearReportingCacheTester tester(network_context(), true);

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              constants::DATA_TYPE_HISTORY, builder->Copy());

  EXPECT_EQ(1, tester.mock().remove_calls());
  EXPECT_EQ(0, tester.mock().remove_all_calls());
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
            tester.mock().last_data_type_mask());
  EXPECT_TRUE(ProbablySameFilters(builder->BuildUrlFilter(),
                                  tester.mock().last_origin_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, NetworkErrorLogging_NoDelegate) {
  ClearNetworkErrorLoggingTester tester(network_context(), false);

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, true);

  // Nothing to check, since there's no mock service; we're just making sure
  // nothing crashes without a service.
}

// This would use an origin filter, but history isn't yet filterable.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, NetworkErrorLogging_History) {
  ClearNetworkErrorLoggingTester tester(network_context(), true);

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, true);

  EXPECT_EQ(0, tester.mock().remove_calls());
  EXPECT_EQ(1, tester.mock().remove_all_calls());
  EXPECT_TRUE(ProbablySameFilters(base::RepeatingCallback<bool(const GURL&)>(),
                                  tester.mock().last_origin_filter()));
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

// Test that all WebsiteSettings are getting deleted by creating a
// value for each of them and removing data.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, AllTypesAreGettingDeleted) {
  TestingProfile* profile = GetProfile();
  ASSERT_TRUE(SubresourceFilterProfileContextFactory::GetForProfile(profile));

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  auto* registry = content_settings::WebsiteSettingsRegistry::GetInstance();
  auto* content_setting_registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  auto* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  // Create a safe_browsing::VerdictCacheManager that will handle deletion of
  // ContentSettingsType::PASSWORD_PROTECTION entries.
  safe_browsing::VerdictCacheManager sb_cache_manager(history_service, map);

  GURL url("https://example.com");

  // List of types that don't have to be deletable.
  static const ContentSettingsType non_deletable_types[] = {
      // Doesn't allow any values.
      ContentSettingsType::PROTOCOL_HANDLERS,
      // Doesn't allow any values.
      ContentSettingsType::MIXEDSCRIPT,
      // Only policy provider sets exceptions for this type.
      ContentSettingsType::AUTO_SELECT_CERTIFICATE,

      // TODO(710873): Make sure that these get fixed:
      // Not deleted but should be deleted with history?
      ContentSettingsType::IMPORTANT_SITE_INFO,
  };

  // Set a value for every WebsiteSetting.
  for (const content_settings::WebsiteSettingsInfo* info : *registry) {
    if (base::Contains(non_deletable_types, info->type()))
      continue;
    base::Value some_value;
    auto* content_setting = content_setting_registry->Get(info->type());
    if (content_setting) {
      // Content Settings only allow integers.
      if (content_setting->IsSettingValid(CONTENT_SETTING_ALLOW)) {
        some_value = base::Value(CONTENT_SETTING_ALLOW);
      } else {
        ASSERT_TRUE(content_setting->IsSettingValid(CONTENT_SETTING_ASK));
        some_value = base::Value(CONTENT_SETTING_ASK);
      }
      ASSERT_TRUE(content_setting->IsDefaultSettingValid(CONTENT_SETTING_BLOCK))
          << info->name();
      // Set default to BLOCK to be able to differentiate an exception from the
      // default.
      map->SetDefaultContentSetting(info->type(), CONTENT_SETTING_BLOCK);
    } else {
      // Other website settings only allow dictionaries.
      base::Value dict(base::Value::Type::DICTIONARY);
      dict.SetKey("foo", base::Value(42));
      some_value = std::move(dict);
    }
    // Create an exception.
    map->SetWebsiteSettingDefaultScope(url, url, info->type(),
                                       some_value.Clone());

    // Check that the exception was created.
    base::Value value = map->GetWebsiteSetting(url, url, info->type(), nullptr);
    EXPECT_FALSE(value.is_none()) << "Not created: " << info->name();
    EXPECT_EQ(some_value, value) << "Not created: " << info->name();
  }

  // Delete all data types that trigger website setting deletions.
  uint64_t mask = constants::DATA_TYPE_HISTORY |
                  constants::DATA_TYPE_SITE_DATA |
                  constants::DATA_TYPE_CONTENT_SETTINGS;

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(), mask, false);

  // All settings should be deleted now.
  for (const content_settings::WebsiteSettingsInfo* info : *registry) {
    if (base::Contains(non_deletable_types, info->type()))
      continue;
    base::Value value = map->GetWebsiteSetting(url, url, info->type(), nullptr);

    if (value.is_int()) {
      EXPECT_EQ(CONTENT_SETTING_BLOCK, value.GetInt())
          << "Not deleted: " << info->name() << " value: " << value;
    } else {
      EXPECT_TRUE(value.is_none())
          << "Not deleted: " << info->name() << " value: " << value;
    }
  }
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, WipeOriginVerifierData) {
  int before =
      customtabs::OriginVerifier::GetClearBrowsingDataCallCountForTesting();
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(
      before + 1,
      customtabs::OriginVerifier::GetClearBrowsingDataCallCountForTesting());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, WipeCrashData) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // This test applies only when using a logfile of Crash uploads. Chrome Linux
  // will use Crashpad's database instead of the logfile. Chrome Chrome OS
  // continues to use the logfile even when Crashpad is enabled.
  if (crash_reporter::IsCrashpadEnabled()) {
    GTEST_SKIP();
  }
#endif
  base::ScopedPathOverride override_crash_dumps(chrome::DIR_CRASH_DUMPS);
  base::FilePath crash_dir_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);

  constexpr char kCrashEntry1[] = "12345,abc\n";
  constexpr char kCrashEntry2[] = "67890,def\n";
  std::string initial_contents = kCrashEntry1;
  initial_contents.append(kCrashEntry2);
  ASSERT_TRUE(base::WriteFile(upload_log_path, initial_contents));

  BlockUntilBrowsingDataRemoved(base::Time::FromTimeT(67890u),
                                base::Time::Max(), constants::DATA_TYPE_HISTORY,
                                false);

  std::string contents;
  base::ReadFileToString(upload_log_path, &contents);
  EXPECT_EQ(kCrashEntry1, contents);

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_HISTORY, false);

  EXPECT_FALSE(base::PathExists(upload_log_path));
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, WipeCustomDictionaryData) {
  base::FilePath dict_path =
      GetProfile()->GetPath().Append(chrome::kCustomDictionaryFileName);
  base::FilePath backup_path = dict_path.AddExtensionASCII("backup");

  auto* spellcheck = SpellcheckServiceFactory::GetForContext(GetProfile());
  ASSERT_NE(nullptr, spellcheck);
  auto* dict = spellcheck->GetCustomDictionary();
  ASSERT_NE(nullptr, dict);

  auto change1 = std::make_unique<SpellcheckCustomDictionary::Change>();
  change1->AddWord("wug");
  dict->UpdateDictionaryFile(std::move(change1), dict_path);

  auto change2 = std::make_unique<SpellcheckCustomDictionary::Change>();
  change2->AddWord("spowing");
  dict->UpdateDictionaryFile(std::move(change2), dict_path);

  EXPECT_TRUE(base::PathExists(dict_path));
  EXPECT_TRUE(base::PathExists(backup_path));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_LOCAL_CUSTOM_DICTIONARY,
                                false);

  std::string contents;
  base::ReadFileToString(dict_path, &contents);
  EXPECT_EQ(std::string::npos, contents.find("wug"));
  EXPECT_EQ(std::string::npos, contents.find("spowing"));
  EXPECT_FALSE(base::PathExists(backup_path));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       WipeNotificationPermissionPromptOutcomesData) {
  PrefService* prefs = GetProfile()->GetPrefs();
  base::Time first_recorded_time = base::Time::Now();

  auto* action_history =
      PermissionActionsHistoryFactory::GetForProfile(GetProfile());
  action_history->RecordAction(
      permissions::PermissionAction::DENIED,
      permissions::RequestType::kNotifications,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
  task_environment()->AdvanceClock(base::Days(1));
  action_history->RecordAction(
      permissions::PermissionAction::DENIED,
      permissions::RequestType::kNotifications,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
  task_environment()->AdvanceClock(base::Days(1));
  base::Time third_recorded_time = base::Time::Now();
  action_history->RecordAction(
      permissions::PermissionAction::DENIED,
      permissions::RequestType::kNotifications,
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);

  constexpr char kPermissionActionsPrefPath[] =
      "profile.content_settings.permission_actions";

  EXPECT_EQ(3u, prefs->GetDictionary(kPermissionActionsPrefPath)
                    ->FindKey("notifications")
                    ->GetListDeprecated()
                    .size());
  // Remove the first and the second element.
  BlockUntilBrowsingDataRemoved(first_recorded_time, third_recorded_time,
                                constants::DATA_TYPE_SITE_USAGE_DATA, false);
  // There is only one element left.
  EXPECT_EQ(1u, prefs->GetDictionary(kPermissionActionsPrefPath)
                    ->FindKey("notifications")
                    ->GetListDeprecated()
                    .size());
  EXPECT_EQ((base::ValueToTime(prefs->GetDictionary(kPermissionActionsPrefPath)
                                   ->FindKey("notifications")
                                   ->GetListDeprecated()
                                   .begin()
                                   ->FindKey("time")))
                .value_or(base::Time()),
            third_recorded_time);

  // Test we wiped all the elements left.
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_SITE_USAGE_DATA, false);
  EXPECT_TRUE(prefs->GetDictionary(kPermissionActionsPrefPath)->DictEmpty());
}

class ChromeBrowsingDataRemoverDelegateEnabledPasswordsTest
    : public ChromeBrowsingDataRemoverDelegateTest {
 public:
  ChromeBrowsingDataRemoverDelegateEnabledPasswordsTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);
  }
};

TEST_F(ChromeBrowsingDataRemoverDelegateEnabledPasswordsTest,
       RemovePasswordsByTimeOnly_WithAccountStore) {
  RemovePasswordsTester tester(GetProfile());

  ExpectRemoveLoginsByURLAndTime(tester.profile_store());
  // Only DATA_TYPE_PASSWORDS is cleared. Accounts passwords are not affected.
  EXPECT_CALL(*tester.account_store(), RemoveLoginsByURLAndTime).Times(0);

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_PASSWORDS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateEnabledPasswordsTest,
       RemoveAccountPasswordsByTimeOnly_WithAccountStore) {
  RemovePasswordsTester tester(GetProfile());

  EXPECT_CALL(*tester.profile_store(), RemoveLoginsByURLAndTime).Times(0);
  ExpectRemoveLoginsByURLAndTime(tester.account_store());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                constants::DATA_TYPE_ACCOUNT_PASSWORDS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateEnabledPasswordsTest,
       RemoveAccountPasswordsByTimeOnly_WithAccountStore_Failure) {
  RemovePasswordsTester tester(GetProfile());

  EXPECT_CALL(*tester.profile_store(), RemoveLoginsByURLAndTime).Times(0);
  ExpectRemoveLoginsByURLAndTime(tester.account_store());

  uint64_t failed_data_types = BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(), constants::DATA_TYPE_ACCOUNT_PASSWORDS,
      false);
  EXPECT_EQ(failed_data_types, constants::DATA_TYPE_ACCOUNT_PASSWORDS);
}

TEST_F(ChromeBrowsingDataRemoverDelegateEnabledPasswordsTest,
       GetDomainsForDeferredCookieDeletion) {
  auto* delegate = GetProfile()->GetBrowsingDataRemoverDelegate();

  auto domains = delegate->GetDomainsForDeferredCookieDeletion(
      constants::DATA_TYPE_ACCOUNT_PASSWORDS);
  EXPECT_EQ(domains.size(), 1u);
  EXPECT_EQ(domains[0], "google.com");

  domains = delegate->GetDomainsForDeferredCookieDeletion(
      constants::DATA_TYPE_PASSWORDS);
  EXPECT_EQ(domains.size(), 0u);

  domains =
      delegate->GetDomainsForDeferredCookieDeletion(constants::ALL_DATA_TYPES);
  EXPECT_EQ(domains.size(), 0u);
}

// Verify that clearing secure payment confirmation credentials data works.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveSecurePaymentConfirmationCredentials) {
  GetProfile()->CreateWebDataService();
  RemoveSecurePaymentConfirmationCredentialsTester tester(GetProfile());
  tester.ExpectCallClearSecurePaymentConfirmationCredentials(1);

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                constants::DATA_TYPE_PASSWORDS, false);
}
