// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

import java.util.Map;

/**
 * Java accessor for base/feature_list.h state.
 *
 * This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in chrome/browser/android/flags/chrome_feature_list.cc and as a constant
 * in this class.
 *
 * This class also provides methods to access values of field trial parameters associated to those
 * flags.
 */
@JNINamespace("chrome::android")
@MainDex
public abstract class ChromeFeatureList {
    /** Prevent instantiation. */
    private ChromeFeatureList() {}

    /**
     * @see FeatureList#setTestCanUseDefaultsForTesting
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @VisibleForTesting
    public static void setTestCanUseDefaultsForTesting() {
        FeatureList.setTestCanUseDefaultsForTesting();
    }

    /**
     * @see FeatureList#resetTestCanUseDefaultsForTesting
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @VisibleForTesting
    public static void resetTestCanUseDefaultsForTesting() {
        FeatureList.resetTestCanUseDefaultsForTesting();
    }

    /**
     * @see FeatureList#setTestFeatures
     * Sets the feature flags to use in JUnit tests, since native calls are not available there.
     * Do not use directly, prefer using the {@link Features} annotation.
     *
     * @see Features
     * @see Features.Processor
     *
     * @deprecated
     * https://crbug.com/1058993
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @VisibleForTesting
    @Deprecated
    public static void setTestFeatures(Map<String, Boolean> features) {
        FeatureList.setTestFeatures(features);
    }

    /**
     * @return Whether the native FeatureList has been initialized. If this method returns false,
     *         none of the methods in this class that require native access should be called (except
     *         in tests if test features have been set).
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @Deprecated
    public static boolean isInitialized() {
        return FeatureList.isInitialized();
    }

    /*
     * Returns whether the specified feature is enabled or not in native.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    private static boolean isEnabledInNative(String featureName) {
        assert FeatureList.isNativeInitialized();
        return ChromeFeatureListJni.get().isEnabled(featureName);
    }

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/android/chrome_feature_list.cc
     *
     * Calling this has the side effect of bucketing this client, which may cause an experiment to
     * be marked as active.
     *
     * Should be called only after native is loaded. If {@link #isInitialized()} return true, this
     * method is safe to call.  In tests, this will return any values set through
     * {@link #setTestFeatures(Map)}, even before native is loaded.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        // FeatureFlags set for testing override the native default value.
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        return isEnabledInNative(featureName);
    }

    /**
     * Returns a field trial param for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @return The parameter value as a String. The string is empty if the feature does not exist or
     *   the specified parameter does not exist.
     */
    public static String getFieldTrialParamByFeature(String featureName, String paramName) {
        if (FeatureList.hasTestFeatures()) return "";
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeature(featureName, paramName);
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The integer value to use if the param is not available.
     * @return The parameter value as an int. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value does not represent an int.
     */
    public static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeatureAsInt(
                featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a double for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The double value to use if the param is not available.
     * @return The parameter value as a double. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value does not represent a double.
     */
    public static double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeatureAsDouble(
                featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The boolean value to use if the param is not available.
     * @return The parameter value as a boolean. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value is neither "true" nor "false".
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeatureAsBoolean(
                featureName, paramName, defaultValue);
    }

    /** Alphabetical: */
    public static final String ALLOW_NEW_INCOGNITO_TAB_INTENTS = "AllowNewIncognitoTabIntents";
    public static final String ALLOW_REMOTE_CONTEXT_FOR_NOTIFICATIONS =
            "AllowRemoteContextForNotifications";
    public static final String AUTOFILL_ALLOW_NON_HTTP_ACTIVATION =
            "AutofillAllowNonHttpActivation";
    public static final String AUTOFILL_CREDIT_CARD_AUTHENTICATION =
            "AutofillCreditCardAuthentication";
    public static final String AUTOFILL_ENABLE_COMPANY_NAME = "AutofillEnableCompanyName";
    public static final String AUTOFILL_ENABLE_GOOGLE_ISSUED_CARD =
            "AutofillEnableGoogleIssuedCard";
    public static final String AUTOFILL_ENABLE_SURFACING_SERVER_CARD_NICKNAME =
            "AutofillEnableSurfacingServerCardNickname";
    public static final String ADJUST_WEBAPK_INSTALLATION_SPACE = "AdjustWebApkInstallationSpace";
    public static final String ANDROID_BLOCK_INTENT_NON_SAFELISTED_HEADERS =
            "AndroidBlockIntentNonSafelistedHeaders";
    public static final String ANDROID_MULTIPLE_DISPLAY = "AndroidMultipleDisplay";
    public static final String ANDROID_NIGHT_MODE_TAB_REPARENTING =
            "AndroidNightModeTabReparenting";
    public static final String ANDROID_PARTNER_CUSTOMIZATION_PHENOTYPE =
            "AndroidPartnerCustomizationPhenotype";
    public static final String ANDROID_PAY_INTEGRATION_V1 = "AndroidPayIntegrationV1";
    public static final String ANDROID_PAY_INTEGRATION_V2 = "AndroidPayIntegrationV2";
    public static final String ANDROID_SEARCH_ENGINE_CHOICE_NOTIFICATION =
            "AndroidSearchEngineChoiceNotification";
    public static final String AUTOFILL_ASSISTANT = "AutofillAssistant";
    public static final String AUTOFILL_ASSISTANT_CHROME_ENTRY = "AutofillAssistantChromeEntry";
    public static final String AUTOFILL_ASSISTANT_DIRECT_ACTIONS = "AutofillAssistantDirectActions";
    public static final String AUTOFILL_MANUAL_FALLBACK_ANDROID = "AutofillManualFallbackAndroid";
    public static final String AUTOFILL_REFRESH_STYLE_ANDROID = "AutofillRefreshStyleAndroid";
    public static final String AUTOFILL_KEYBOARD_ACCESSORY = "AutofillKeyboardAccessory";
    public static final String BACKGROUND_TASK_SCHEDULER_FOR_BACKGROUND_SYNC =
            "BackgroundTaskSchedulerForBackgroundSync";
    public static final String BIOMETRIC_TOUCH_TO_FILL = "BiometricTouchToFill";
    public static final String CAPTIVE_PORTAL_CERTIFICATE_LIST = "CaptivePortalCertificateList";
    public static final String CCT_BACKGROUND_TAB = "CCTBackgroundTab";
    public static final String CCT_CLIENT_DATA_HEADER = "CCTClientDataHeader";
    public static final String CCT_INCOGNITO = "CCTIncognito";
    public static final String CCT_EXTERNAL_LINK_HANDLING = "CCTExternalLinkHandling";
    public static final String CCT_POST_MESSAGE_API = "CCTPostMessageAPI";
    public static final String CCT_REDIRECT_PRECONNECT = "CCTRedirectPreconnect";
    public static final String CCT_RESOURCE_PREFETCH = "CCTResourcePrefetch";
    public static final String CCT_REPORT_PARALLEL_REQUEST_STATUS =
            "CCTReportParallelRequestStatus";
    public static final String CCT_TARGET_TRANSLATE_LANGUAGE = "CCTTargetTranslateLanguage";
    public static final String CLOSE_TAB_SUGGESTIONS = "CloseTabSuggestions";
    public static final String CHROME_DUET = "ChromeDuet";
    public static final String CHROME_DUET_ADAPTIVE = "ChromeDuetAdaptive";
    public static final String DONT_AUTO_HIDE_BROWSER_CONTROLS = "DontAutoHideBrowserControls";
    public static final String CHROME_DUET_LABELED = "ChromeDuetLabeled";
    public static final String CHROME_SHARE_QRCODE = "ChromeShareQRCode";
    public static final String CHROME_SHARE_SCREENSHOT = "ChromeShareScreenshot";
    public static final String CHROME_SHARING_HUB = "ChromeSharingHub";
    public static final String CHROME_SHARING_HUB_V15 = "ChromeSharingHubV15";
    public static final String CHROME_SMART_SELECTION = "ChromeSmartSelection";
    public static final String CLEAR_OLD_BROWSING_DATA = "ClearOldBrowsingData";
    public static final String COMMAND_LINE_ON_NON_ROOTED = "CommandLineOnNonRooted";
    public static final String CONDITIONAL_TAB_STRIP_ANDROID = "ConditionalTabStripAndroid";
    public static final String CONTACTS_PICKER_SELECT_ALL = "ContactsPickerSelectAll";
    public static final String CONTENT_SUGGESTIONS_SCROLL_TO_LOAD =
            "ContentSuggestionsScrollToLoad";
    public static final String CONTENT_INDEXING_NTP = "ContentIndexingNTP";
    public static final String CONTENT_INDEXING_DOWNLOAD_HOME = "ContentIndexingDownloadHome";
    public static final String CONTEXT_MENU_COPY_IMAGE = "ContextMenuCopyImage";
    public static final String CONTEXT_MENU_PERFORMANCE_INFO = "ContextMenuPerformanceInfo";
    public static final String CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS =
            "ContextMenuSearchWithGoogleLens";
    public static final String CONTEXTUAL_SEARCH_DEBUG = "ContextualSearchDebug";
    public static final String CONTEXTUAL_SEARCH_DEFINITIONS = "ContextualSearchDefinitions";
    public static final String CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION =
            "ContextualSearchMlTapSuppression";
    public static final String CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE =
            "ContextualSearchLongpressResolve";
    public static final String CONTEXTUAL_SEARCH_SECOND_TAP = "ContextualSearchSecondTap";
    public static final String CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE =
            "ContextualSearchTapDisableOverride";
    public static final String CONTEXTUAL_SEARCH_TRANSLATIONS = "ContextualSearchTranslations";
    public static final String COOKIES_WITHOUT_SAME_SITE_MUST_BE_SECURE =
            "CookiesWithoutSameSiteMustBeSecure";
    public static final String DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING =
            "DarkenWebsitesCheckboxInThemesSetting";
    public static final String DIRECT_ACTIONS = "DirectActions";
    public static final String DOWNLOAD_FILE_PROVIDER = "DownloadFileProvider";
    public static final String DOWNLOAD_NOTIFICATION_BADGE = "DownloadNotificationBadge";
    public static final String DOWNLOAD_PROGRESS_INFOBAR = "DownloadProgressInfoBar";
    public static final String DOWNLOADS_FOREGROUND = "DownloadsForeground";
    public static final String DOWNLOADS_AUTO_RESUMPTION_NATIVE = "DownloadsAutoResumptionNative";
    public static final String DOWNLOAD_OFFLINE_CONTENT_PROVIDER =
            "UseDownloadOfflineContentProvider";
    public static final String DOWNLOADS_LOCATION_CHANGE = "DownloadsLocationChange";
    public static final String DRAW_VERTICALLY_EDGE_TO_EDGE = "DrawVerticallyEdgeToEdge";
    public static final String DUET_TABSTRIP_INTEGRATION_ANDROID = "DuetTabStripIntegrationAndroid";
    public static final String EPHEMERAL_TAB_USING_BOTTOM_SHEET = "EphemeralTabUsingBottomSheet";
    public static final String EXPLICIT_LANGUAGE_ASK = "ExplicitLanguageAsk";
    public static final String EXPLORE_SITES = "ExploreSites";
    public static final String FOCUS_OMNIBOX_IN_INCOGNITO_TAB_INTENTS =
            "FocusOmniboxInIncognitoTabIntents";
    public static final String GRANT_NOTIFICATIONS_TO_DSE = "GrantNotificationsToDSE";
    public static final String HANDLE_MEDIA_INTENTS = "HandleMediaIntents";
    public static final String HOMEPAGE_LOCATION_POLICY = "HomepageLocationPolicy";
    public static final String HOMEPAGE_PROMO_CARD = "HomepagePromoCard";
    public static final String HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_ENABLED =
            "HomepagePromoSyntheticPromoSeenEnabled";
    public static final String HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_TRACKING =
            "HomepagePromoSyntheticPromoSeenTracking";
    public static final String HOMEPAGE_SETTINGS_UI_CONVERSION = "HomepageSettingsUIConversion";
    public static final String HORIZONTAL_TAB_SWITCHER_ANDROID = "HorizontalTabSwitcherAndroid";
    public static final String IMMERSIVE_UI_MODE = "ImmersiveUiMode";
    public static final String INLINE_UPDATE_FLOW = "InlineUpdateFlow";
    public static final String INSTALLABLE_AMBIENT_BADGE_INFOBAR = "InstallableAmbientBadgeInfoBar";
    public static final String INSTANT_START = "InstantStart";
    public static final String INTEREST_FEED_CONTENT_SUGGESTIONS = "InterestFeedContentSuggestions";
    public static final String INTEREST_FEED_FEEDBACK = "InterestFeedFeedback";
    public static final String INTEREST_FEED_V2 = "InterestFeedV2";
    public static final String KITKAT_SUPPORTED = "KitKatSupported";
    public static final String LOOKALIKE_NAVIGATION_URL_SUGGESTIONS_UI =
            "LookalikeUrlNavigationSuggestionsUI";
    public static final String SEARCH_ENGINE_PROMO_EXISTING_DEVICE =
            "SearchEnginePromo.ExistingDevice";
    public static final String SEARCH_ENGINE_PROMO_NEW_DEVICE = "SearchEnginePromo.NewDevice";
    public static final String MARK_HTTP_AS = "MarkHttpAs";
    public static final String MOBILE_IDENTITY_CONSISTENCY = "MobileIdentityConsistency";
    public static final String MODAL_PERMISSION_PROMPTS = "ModalPermissionPrompts";
    public static final String MODAL_PERMISSION_DIALOG_VIEW = "ModalPermissionDialogView";
    public static final String NEW_PHOTO_PICKER = "NewPhotoPicker";
    public static final String NOTIFICATION_SUSPENDER = "NotificationSuspender";
    public static final String NTP_ARTICLE_SUGGESTIONS = "NTPArticleSuggestions";
    public static final String OFFLINE_INDICATOR = "OfflineIndicator";
    public static final String OFFLINE_INDICATOR_ALWAYS_HTTP_PROBE =
            "OfflineIndicatorAlwaysHttpProbe";
    public static final String OFFLINE_INDICATOR_V2 = "OfflineIndicatorV2";
    public static final String OFFLINE_PAGES_DESCRIPTIVE_FAIL_STATUS =
            "OfflinePagesDescriptiveFailStatus";
    public static final String OFFLINE_PAGES_DESCRIPTIVE_PENDING_STATUS =
            "OfflinePagesDescriptivePendingStatus";
    public static final String OFFLINE_PAGES_LIVE_PAGE_SHARING = "OfflinePagesLivePageSharing";
    public static final String OFFLINE_PAGES_PREFETCHING = "OfflinePagesPrefetching";
    public static final String OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT =
            "OmniboxAdaptiveSuggestionsCount";
    public static final String OMNIBOX_ASSISTANT_VOICE_SEARCH = "OmniboxAssistantVoiceSearch";
    public static final String OMNIBOX_COMPACT_SUGGESTIONS = "OmniboxCompactSuggestions";
    public static final String OMNIBOX_DEFERRED_KEYBOARD_POPUP = "OmniboxDeferredKeyboardPopup";
    public static final String OMNIBOX_ENABLE_CLIPBOARD_PROVIDER_IMAGE_SUGGESTIONS =
            "OmniboxEnableClipboardProviderImageSuggestions";
    public static final String OMNIBOX_HIDE_SCHEME_IN_STEADY_STATE =
            "OmniboxUIExperimentHideSteadyStateUrlScheme";
    public static final String OMNIBOX_HIDE_TRIVIAL_SUBDOMAINS_IN_STEADY_STATE =
            "OmniboxUIExperimentHideSteadyStateUrlTrivialSubdomains";
    public static final String OMNIBOX_SPARE_RENDERER = "OmniboxSpareRenderer";
    public static final String OMNIBOX_SEARCH_ENGINE_LOGO = "OmniboxSearchEngineLogo";
    public static final String OMNIBOX_SUGGESTIONS_RECYCLER_VIEW = "OmniboxSuggestionsRecyclerView";
    public static final String OVERLAY_NEW_LAYOUT = "OverlayNewLayout";
    public static final String OVERSCROLL_HISTORY_NAVIGATION = "OverscrollHistoryNavigation";
    public static final String PAGE_INFO_PERFORMANCE_HINTS = "PageInfoPerformanceHints";
    public static final String PAINT_PREVIEW_CAPTURE_EXPERIMENT = "PaintPreviewCaptureExperiment";
    public static final String PAINT_PREVIEW_DEMO = "PaintPreviewDemo";
    public static final String PAINT_PREVIEW_SHOW_ON_STARTUP = "PaintPreviewShowOnStartup";
    public static final String PASSWORD_EDITING_ANDROID = "PasswordEditingAndroid";
    public static final String PASSWORD_MANAGER_ONBOARDING_ANDROID =
            "PasswordManagerOnboardingAndroid";
    public static final String PAY_WITH_GOOGLE_V1 = "PayWithGoogleV1";
    public static final String PAYMENT_REQUEST_SKIP_TO_GPAY = "PaymentRequestSkipToGPay";
    public static final String PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD =
            "PaymentRequestSkipToGPayIfNoCard";
    public static final String PERMISSION_DELEGATION = "PermissionDelegation";
    public static final String PER_METHOD_CAN_MAKE_PAYMENT_QUOTA =
            "WebPaymentsPerMethodCanMakePaymentQuota";
    public static final String PHOTO_PICKER_VIDEO_SUPPORT = "PhotoPickerVideoSupport";
    public static final String PHOTO_PICKER_ZOOM = "PhotoPickerZoom";
    public static final String PREDICTIVE_PREFETCHING_ALLOWED_ON_ALL_CONNECTION_TYPES =
            "PredictivePrefetchingAllowedOnAllConnectionTypes";
    public static final String PRIORITIZE_BOOTSTRAP_TASKS = "PrioritizeBootstrapTasks";
    public static final String PROBABILISTIC_CRYPTID_RENDERER = "ProbabilisticCryptidRenderer";
    public static final String QUERY_TILES_GEO_FILTER = "QueryTilesGeoFilter";
    public static final String QUERY_TILES = "QueryTiles";
    public static final String QUERY_TILES_ENABLE_QUERY_EDITING = "QueryTilesEnableQueryEditing";
    public static final String QUERY_IN_OMNIBOX = "QueryInOmnibox";
    public static final String QUIET_NOTIFICATION_PROMPTS = "QuietNotificationPrompts";
    public static final String REACHED_CODE_PROFILER = "ReachedCodeProfiler";
    public static final String READER_MODE_IN_CCT = "ReaderModeInCCT";
    public static final String RECOVER_FROM_NEVER_SAVE_ANDROID = "RecoverFromNeverSaveAndroid";
    public static final String REENGAGEMENT_NOTIFICATION = "ReengagementNotification";
    public static final String REMOVE_NAVIGATION_HISTORY = "RemoveNavigationHistory";
    public static final String RELATED_SEARCHES = "RelatedSearches";
    public static final String REPORT_FEED_USER_ACTIONS = "ReportFeedUserActions";
    public static final String REVAMPED_CONTEXT_MENU = "RevampedContextMenu";
    public static final String SAME_SITE_BY_DEFAULT_COOKIES = "SameSiteByDefaultCookies";
    public static final String SCROLL_TO_EXPAND_PAYMENT_HANDLER = "ScrollToExpandPaymentHandler";
    public static final String SEND_TAB_TO_SELF = "SyncSendTabToSelf";
    public static final String SERVICE_MANAGER_FOR_DOWNLOAD = "ServiceManagerForDownload";
    public static final String SERVICE_WORKER_PAYMENT_APPS = "ServiceWorkerPaymentApps";
    public static final String SHARE_BUTTON_IN_TOP_TOOLBAR = "ShareButtonInTopToolbar";
    public static final String SHARED_CLIPBOARD_UI = "SharedClipboardUI";
    public static final String SHARING_QR_CODE_ANDROID = "SharingQrCodeAndroid";
    public static final String SYNC_ERROR_INFOBAR_ANDROID = "SyncErrorInfoBarAndroid";
    public static final String SHOW_TRUSTED_PUBLISHER_URL = "ShowTrustedPublisherURL";
    public static final String SPANNABLE_INLINE_AUTOCOMPLETE = "SpannableInlineAutocomplete";
    public static final String START_SURFACE_ANDROID = "StartSurfaceAndroid";
    public static final String STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT =
            "StrictHasEnrolledAutofillInstrument";
    public static final String SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT =
            "SwapPixelFormatToFixConvertFromTranslucent";
    public static final String SYNC_USE_SESSIONS_UNREGISTER_DELAY =
            "SyncUseSessionsUnregisterDelay";
    public static final String TAB_ENGAGEMENT_REPORTING_ANDROID = "TabEngagementReportingAndroid";
    public static final String TAB_GROUPS_ANDROID = "TabGroupsAndroid";
    public static final String TAB_GROUPS_UI_IMPROVEMENTS_ANDROID =
            "TabGroupsUiImprovementsAndroid";
    public static final String TAB_GROUPS_CONTINUATION_ANDROID = "TabGroupsContinuationAndroid";
    public static final String TAB_GRID_LAYOUT_ANDROID = "TabGridLayoutAndroid";
    public static final String TAB_REPARENTING = "TabReparenting";
    public static final String TAB_SWITCHER_ON_RETURN = "TabSwitcherOnReturn";
    public static final String TAB_TO_GTS_ANIMATION = "TabToGTSAnimation";
    public static final String TEST_DEFAULT_DISABLED = "TestDefaultDisabled";
    public static final String TEST_DEFAULT_ENABLED = "TestDefaultEnabled";
    public static final String TOUCH_TO_FILL_ANDROID = "TouchToFillAndroid";
    public static final String TRUSTED_WEB_ACTIVITY = "TrustedWebActivity";
    public static final String TRUSTED_WEB_ACTIVITY_POST_MESSAGE = "TrustedWebActivityPostMessage";
    public static final String TRUSTED_WEB_ACTIVITY_NOTIFICATION_DELEGATION_ENROLMENT =
            "TrustedWebActivityNotificationDelegationAutoEnrolment";
    public static final String TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE =
            "TrustedWebActivityNewDisclosure";
    public static final String TRUSTED_WEB_ACTIVITY_LOCATION_DELEGATION =
            "TrustedWebActivityLocationDelegation";
    public static final String VIDEO_PERSISTENCE = "VideoPersistence";
    public static final String USAGE_STATS = "UsageStats";
    public static final String VR_BROWSING_FEEDBACK = "VrBrowsingFeedback";
    public static final String WEBAPK_ADAPTIVE_ICON = "WebApkAdaptiveIcon";
    public static final String WEB_AUTH = "WebAuthentication";
    public static final String WEB_AUTH_PHONE_SUPPORT = "WebAuthenticationPhoneSupport";
    public static final String WEB_PAYMENTS = "WebPayments";
    public static final String WEB_PAYMENTS_ALWAYS_ALLOW_JUST_IN_TIME_PAYMENT_APP =
            "AlwaysAllowJustInTimePaymentApp";
    public static final String WEB_PAYMENTS_EXPERIMENTAL_FEATURES =
            "WebPaymentsExperimentalFeatures";
    public static final String WEB_PAYMENTS_METHOD_SECTION_ORDER_V2 =
            "WebPaymentsMethodSectionOrderV2";
    public static final String WEB_PAYMENTS_MINIMAL_UI = "WebPaymentsMinimalUI";
    public static final String WEB_PAYMENTS_MODIFIERS = "WebPaymentsModifiers";
    public static final String WEB_PAYMENTS_REDACT_SHIPPING_ADDRESS =
            "WebPaymentsRedactShippingAddress";
    public static final String WEB_PAYMENTS_RETURN_GOOGLE_PAY_IN_BASIC_CARD =
            "ReturnGooglePayInBasicCard";
    public static final String WEB_PAYMENTS_SINGLE_APP_UI_SKIP = "WebPaymentsSingleAppUiSkip";
    public static final String SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH =
            "ServiceManagerForBackgroundPrefetch";
    public static final String SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY =
            "SplitCacheByNetworkIsolationKey";
    public static final String UPDATE_NOTIFICATION_SCHEDULING_INTEGRATION =
            "UpdateNotificationSchedulingIntegration";
    public static final String UPDATE_NOTIFICATION_IMMEDIATE_SHOW_OPTION =
            "UpdateNotificationScheduleServiceImmediateShowOption";
    public static final String PREFETCH_NOTIFICATION_SCHEDULING_INTEGRATION =
            "PrefetchNotificationSchedulingIntegration";

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
        String getFieldTrialParamByFeature(String featureName, String paramName);
        int getFieldTrialParamByFeatureAsInt(
                String featureName, String paramName, int defaultValue);
        double getFieldTrialParamByFeatureAsDouble(
                String featureName, String paramName, double defaultValue);
        boolean getFieldTrialParamByFeatureAsBoolean(
                String featureName, String paramName, boolean defaultValue);
    }
}
