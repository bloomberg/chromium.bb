// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/content_settings/tab_specific_content_settings_delegate.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_tab_helper.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/media/history/media_history_contents_observer.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"
#include "chrome/browser/metrics/renderer_uptime_web_contents_observer.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/native_file_system/native_file_system_tab_helper.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/performance_hints/performance_hints_observer.h"
#include "chrome/browser/plugins/pdf_plugin_placeholder_observer.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/reputation/reputation_web_contents_observer.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_observer.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/focus_tab_after_navigation_helper.h"
#include "chrome/browser/ui/navigation_correction_tab_observer.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/client_hints/browser/client_hints.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/permissions/permission_request_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/banners/app_banner_manager_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_android.h"
#else
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/zoom_controller.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder_tab_tracker.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/hats/hats_helper.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/media/feeds/media_feeds_contents_observer.h"
#include "chrome/browser/media/feeds/media_feeds_service.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#endif

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
#include "chrome/browser/media/kaleidoscope/internal/kaleidoscope_tab_helper.h"
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

}  // namespace

// static
void TabHelpers::AttachTabHelpers(WebContents* web_contents) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kTabContentsAttachedTabHelpersUserDataKey);
  if (adoption_tag)
    return;

  // Mark as adopted.
  web_contents->SetUserData(&kTabContentsAttachedTabHelpersUserDataKey,
                            std::make_unique<base::SupportsUserData::Data>());

  // Create all the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  CreateSessionServiceTabHelper(web_contents);

#if !defined(OS_ANDROID)
  // ZoomController comes before common tab helpers since ChromeAutofillClient
  // may want to register as a ZoomObserver with it.
  zoom::ZoomController::CreateForWebContents(web_contents);
#endif

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // --- Section 1: Common tab helpers ---

  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents),
      g_browser_process->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));
  if (base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilter)) {
    ChromeSubresourceFilterClient::CreateForWebContents(web_contents);
  }
  ChromeTranslateClient::CreateForWebContents(web_contents);
  PrefService* local_state = g_browser_process->local_state();
  client_hints::ClientHints::CreateForWebContents(
      web_contents, g_browser_process->network_quality_tracker(),
      HostContentSettingsMapFactory::GetForProfile(profile),
      GetUserAgentMetadata(), local_state);
  ConnectionHelpTabHelper::CreateForWebContents(web_contents);
  CoreTabHelper::CreateForWebContents(web_contents);
  DataReductionProxyTabHelper::CreateForWebContents(web_contents);
  ExternalProtocolObserver::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  FindBarState::ConfigureWebContents(web_contents);
  download::DownloadNavigationObserver::CreateForWebContents(
      web_contents,
      download::NavigationMonitorFactory::GetForKey(profile->GetProfileKey()));
  history::WebContentsTopSitesObserver::CreateForWebContents(
      web_contents, TopSitesFactory::GetForProfile(profile).get());
  HistoryTabHelper::CreateForWebContents(web_contents);
  InfoBarService::CreateForWebContents(web_contents);
  InstallableManager::CreateForWebContents(web_contents);
  IsolatedPrerenderTabHelper::CreateForWebContents(web_contents);
  if (MediaEngagementService::IsEnabled())
    MediaEngagementService::CreateWebContentsObserver(web_contents);
  if (base::FeatureList::IsEnabled(media::kUseMediaHistoryStore))
    MediaHistoryContentsObserver::CreateForWebContents(web_contents);
  metrics::RendererUptimeWebContentsObserver::CreateForWebContents(
      web_contents);
  MixedContentSettingsTabHelper::CreateForWebContents(web_contents);
  NativeFileSystemPermissionRequestManager::CreateForWebContents(web_contents);
  NativeFileSystemTabHelper::CreateForWebContents(web_contents);
  NavigationCorrectionTabObserver::CreateForWebContents(web_contents);
  NavigationMetricsRecorder::CreateForWebContents(web_contents);
  NavigationPredictorPreconnectClient::CreateForWebContents(web_contents);
  OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents);
  OutOfMemoryReporter::CreateForWebContents(web_contents);
  chrome::InitializePageLoadMetricsForWebContents(web_contents);
  PDFPluginPlaceholderObserver::CreateForWebContents(web_contents);
  if (auto* performance_manager_registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    performance_manager_registry->CreatePageNodeForWebContents(web_contents);
    performance_manager::TabPropertiesDecorator::SetIsTab(web_contents, true);
  }
  permissions::PermissionRequestManager::CreateForWebContents(web_contents);
  // The PopupBlockerTabHelper has an implicit dependency on
  // ChromeSubresourceFilterClient being available in its constructor.
  PopupBlockerTabHelper::CreateForWebContents(web_contents);
  PopupOpenerTabHelper::CreateForWebContents(
      web_contents, base::DefaultTickClock::GetInstance());
  if (predictors::LoadingPredictorFactory::GetForProfile(profile))
    predictors::LoadingPredictorTabHelper::CreateForWebContents(web_contents);
  PrefsTabHelper::CreateForWebContents(web_contents);
  prerender::PrerenderTabHelper::CreateForWebContents(web_contents);
  PreviewsUITabHelper::CreateForWebContents(web_contents);
  RecentlyAudibleHelper::CreateForWebContents(web_contents);
  // TODO(siggi): Remove this once the Resource Coordinator refactoring is done.
  //     See https://crbug.com/910288.
  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      web_contents);
  ResourceLoadingHintsWebContentsObserver::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents);
  safe_browsing::TriggerCreator::MaybeCreateTriggersForWebContents(
      profile, web_contents);
  ReputationWebContentsObserver::CreateForWebContents(web_contents);
  SearchEngineTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  if (SiteEngagementService::IsEnabled())
    SiteEngagementService::Helper::CreateForWebContents(web_contents);
  SoundContentSettingObserver::CreateForWebContents(web_contents);
  subresource_redirect::SubresourceRedirectObserver::MaybeCreateForWebContents(
      web_contents);
  sync_sessions::SyncSessionsRouterTabHelper::CreateForWebContents(
      web_contents,
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          profile));
  content_settings::TabSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<chrome::TabSpecificContentSettingsDelegate>(
          web_contents));
  TabUIHelper::CreateForWebContents(web_contents);
  tasks::TaskTabHelper::CreateForWebContents(web_contents);
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);
  vr::VrTabHelper::CreateForWebContents(web_contents);

  // NO! Do not just add your tab helper here. This is a large alphabetized
  // block; please insert your tab helper above in alphabetical order.

  // --- Section 2: Platform-specific tab helpers ---

#if defined(OS_ANDROID)
  {
    // Remove after fixing https://crbug/905919
    TRACE_EVENT0("browser", "AppBannerManagerAndroid::CreateForWebContents");
    banners::AppBannerManagerAndroid::CreateForWebContents(web_contents);
  }
  ContextMenuHelper::CreateForWebContents(web_contents);
  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          web_contents));
  if (OomInterventionTabHelper::IsEnabled()) {
    OomInterventionTabHelper::CreateForWebContents(web_contents);
  }
  if (base::FeatureList::IsEnabled(
          chrome::android::kPageInfoPerformanceHints) ||
      base::FeatureList::IsEnabled(
          chrome::android::kContextMenuPerformanceInfo) ||
      base::FeatureList::IsEnabled(kPerformanceHintsObserver)) {
    PerformanceHintsObserver::CreateForWebContents(web_contents);
  }
  SearchGeolocationDisclosureTabHelper::CreateForWebContents(web_contents);
  ViewAndroidHelper::CreateForWebContents(web_contents);
#else
  banners::AppBannerManagerDesktop::CreateForWebContents(web_contents);
  BookmarkTabHelper::CreateForWebContents(web_contents);
  BrowserSyncedTabDelegate::CreateForWebContents(web_contents);
  FocusTabAfterNavigationHelper::CreateForWebContents(web_contents);
  FormInteractionTabHelper::CreateForWebContents(web_contents);
  FramebustBlockTabHelper::CreateForWebContents(web_contents);
  IntentPickerTabHelper::CreateForWebContents(web_contents);
  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          web_contents));
  ManagePasswordsUIController::CreateForWebContents(web_contents);
  if (media_feeds::MediaFeedsService::IsEnabled())
    MediaFeedsContentsObserver::CreateForWebContents(web_contents);
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents, std::make_unique<ChromePDFWebContentsHelperClient>());
  SadTabHelper::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(web_contents);
  SearchTabHelper::CreateForWebContents(web_contents);
  if (base::FeatureList::IsEnabled(
          switches::kSyncSupportTrustedVaultPassphrase)) {
    SyncEncryptionKeysTabHelper::CreateForWebContents(web_contents);
  }
  TabDialogs::CreateForWebContents(web_contents);
  if (base::FeatureList::IsEnabled(features::kTabHoverCardImages) ||
      base::FeatureList::IsEnabled(features::kWebUITabStrip)) {
    ThumbnailTabHelper::CreateForWebContents(web_contents);
  }
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
#endif

#if defined(OS_CHROMEOS)
  app_list::CrOSActionRecorderTabTracker::CreateForWebContents(web_contents);
  chromeos::app_time::WebTimeNavigationObserver::MaybeCreateForWebContents(
      web_contents);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  metrics::DesktopSessionDurationObserver::CreateForWebContents(web_contents);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktop) ||
      base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    HatsHelper::CreateForWebContents(web_contents);
  }
#endif

  // --- Section 3: Feature tab helpers behind BUILDFLAGs ---
  // NOT for "if enabled"; put those in section 1.

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      web_contents, CaptivePortalServiceFactory::GetForProfile(profile),
      base::Bind(&ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents,
                 web_contents, false));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_TAB_CONTENTS);

  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  extensions::TabHelper::CreateForWebContents(web_contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  if (web_app::AreWebAppsEnabled(profile))
    web_app::WebAppTabHelper::CreateForWebContents(web_contents);
  if (SiteEngagementService::IsEnabled())
    web_app::WebAppMetrics::Get(profile);
#endif

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  if (base::FeatureList::IsEnabled(media::kKaleidoscope))
    KaleidoscopeTabHelper::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper::CreateForWebContents(web_contents);
  offline_pages::RecentTabHelper::CreateForWebContents(web_contents);
  offline_pages::AutoFetchPageLoadWatcher::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  HungPluginTabHelper::CreateForWebContents(web_contents);
  PluginObserver::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrinting(web_contents);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserNavigationObserver::CreateForWebContents(web_contents);
#endif

  // --- Section 4: The warning ---

  // NONO    NO   NONONO   !
  // NO NO   NO  NO    NO  !
  // NO  NO  NO  NO    NO  !
  // NO   NO NO  NO    NO
  // NO    NONO   NONONO   !

  // Do NOT just drop your tab helpers here! There are three sections above (1.
  // All platforms, 2. Some platforms, 3. Behind BUILDFLAGs). Each is in rough
  // alphabetical order. PLEASE PLEASE PLEASE add your flag to the correct
  // section in the correct order.

  // This is common code for all of us. PLEASE DO YOUR PART to keep it tidy and
  // organized.
}
