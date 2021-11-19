// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.base.BaseSwitches;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.cc.base.CcSwitches;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.components.power_scheduler.PowerSchedulerFeatures;
import org.chromium.components.viz.common.VizFeatures;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.gpu.config.GpuFeatures;
import org.chromium.gpu.config.GpuSwitches;
import org.chromium.services.network.NetworkServiceFeatures;

/**
 * List of experimental features/flags supported for user devices. Add features/flags to this list
 * with scrutiny: any feature/flag in this list can be enabled for production Android devices, and
 * so it must not compromise the Android security model (i.e., WebView must still protect the app's
 * private data from being user visible).
 *
 * <p>
 * This lives in the common package so it can be shared by dev UI (to know which features/flags to
 * display) as well as the WebView implementation (so it knows which features/flags are safe to
 * honor).
 */
public final class ProductionSupportedFlagList {
    // Do not instantiate this class.
    private ProductionSupportedFlagList() {}

    /**
     * A list of commandline flags supported on user devices. If updating this list, please also
     * update enums.xml. See android_webview/docs/developer-ui.md
     * (https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/developer-ui.md#Adding-your-flags-and-features-to-the-UI).
     */
    public static final Flag[] sFlagList = {
            Flag.commandLine(AwSwitches.HIGHLIGHT_ALL_WEBVIEWS,
                    "Highlight the contents (including web contents) of all WebViews with a yellow "
                            + "tint. This is useful for identifying WebViews in an Android "
                            + "application."),
            Flag.commandLine(AwSwitches.WEBVIEW_VERBOSE_LOGGING,
                    "WebView will log additional debugging information to logcat, such as "
                            + "variations and commandline state."),
            Flag.commandLine(CcSwitches.SHOW_COMPOSITED_LAYER_BORDERS,
                    "Renders a border around compositor layers to help debug and study layer "
                            + "compositing."),
            Flag.commandLine(CcSwitches.ANIMATED_IMAGE_RESUME,
                    "Resumes animated images from where they were."),
            Flag.commandLine(AwSwitches.FINCH_SEED_EXPIRATION_AGE,
                    "Forces all variations seeds to be considered stale.", "0"),
            Flag.commandLine(AwSwitches.FINCH_SEED_IGNORE_PENDING_DOWNLOAD,
                    "Forces the WebView service to reschedule a variations seed download job even "
                            + "if one is already pending."),
            Flag.commandLine(AwSwitches.FINCH_SEED_NO_CHARGING_REQUIREMENT,
                    "Forces WebView's service to always schedule a new variations seed download "
                            + "job, even if the device is not charging. Note this switch may be "
                            + "necessary for testing on Android emulators as these are not always "
                            + "considered to be charging."),
            Flag.commandLine(AwSwitches.FINCH_SEED_MIN_DOWNLOAD_PERIOD,
                    "Disables throttling of variations seed download jobs.", "0"),
            Flag.commandLine(AwSwitches.FINCH_SEED_MIN_UPDATE_PERIOD,
                    "Disables throttling of new variations seed requests to the WebView service.",
                    "0"),
            Flag.commandLine(MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING,
                    "Forces WebView's metrics reporting to be enabled. This overrides user "
                            + "settings and capacity sampling, but does not override the app's "
                            + "choice to opt-out."),
            Flag.commandLine(AwSwitches.WEBVIEW_LOG_JS_CONSOLE_MESSAGES,
                    "Mirrors JavaScript console messages to system logs."),
            Flag.commandLine(BaseSwitches.ENABLE_CRASH_REPORTER_FOR_TESTING,
                    "Used for turning on Breakpad crash reporting in a debug environment where "
                            + "crash reporting is typically compiled but disabled."),
            Flag.commandLine(GpuSwitches.DISABLE_GPU_RASTERIZATION,
                    "Disables GPU rasterization, i.e. rasterizes on the CPU only."),
            Flag.commandLine(GpuSwitches.IGNORE_GPU_BLOCKLIST,
                    "Overrides the built-in software rendering list and enables "
                            + "GPU acceleration on unsupported device configurations."),
            Flag.commandLine(AwSwitches.WEBVIEW_ENABLE_MODERN_COOKIE_SAME_SITE,
                    "Enables modern SameSite cookie behavior: 1) SameSite=Lax by default "
                            + "(cookies without a SameSite attribute are treated as SameSite=Lax); "
                            + "2) Schemeful Same-Site (site boundaries include the URL scheme)."),
            Flag.commandLine(ContentSwitches.SITE_PER_PROCESS,
                    "Security mode that enables site isolation for all sites inside WebView. In "
                            + "this mode, each renderer process will contain pages from at most "
                            + "one site, using out-of-process iframes when needed. Highly "
                            + "experimental."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_VULKAN,
                    "Use Vulkan for composite. Requires Android device and OS support. May crash "
                            + "if enabled on unsupported device."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_SURFACE_CONTROL,
                    "Use SurfaceControl. Requires WebViewZeroCopyVideo and Android device and OS "
                            + "support."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_ZERO_COPY_VIDEO,
                    "Avoid extra copy for video frames when possible"),
            Flag.baseFeature(GpuFeatures.WEBVIEW_THREAD_SAFE_MEDIA,
                    "Use thread-safe media path, requires Android P."),
            Flag.baseFeature(
                    VizFeatures.WEBVIEW_VULKAN_INTERMEDIATE_BUFFER, "For debugging vulkan"),
            Flag.baseFeature(
                    GpuFeatures.USE_GLES2_FOR_OOP_R, "Force Skia context to use es2 only."),
            Flag.baseFeature(AwFeatures.WEBVIEW_CONNECTIONLESS_SAFE_BROWSING,
                    "Uses GooglePlayService's 'connectionless' APIs for Safe Browsing "
                            + "security checks."),
            Flag.baseFeature(AwFeatures.WEBVIEW_BROTLI_SUPPORT,
                    "Enables brotli compression support in WebView."),
            Flag.baseFeature(BlinkFeatures.APP_CACHE,
                    "Controls AppCache to facilitate testing against future removal."),
            Flag.baseFeature(AwFeatures.WEBVIEW_EXTRA_HEADERS_SAME_ORIGIN_ONLY,
                    "Only allow extra headers added via loadUrl() to be sent to the same origin "
                            + "as the original request."),
            Flag.baseFeature(AwFeatures.WEBVIEW_MEASURE_SCREEN_COVERAGE,
                    "Measure the number of pixels occupied by one or more WebViews as a proportion "
                            + "of the total screen size. Depending on the number of WebViews and "
                            + "the size of the screen this might be expensive so hidden behind a "
                            + "feature flag until the true runtime cost can be measured."),
            Flag.baseFeature(AwFeatures.WEBVIEW_DISPLAY_CUTOUT,
                    "Enables display cutout (notch) support in WebView for Android P and above."),
            Flag.baseFeature(PowerSchedulerFeatures.WEBVIEW_CPU_AFFINITY_RESTRICT_TO_LITTLE_CORES,
                    "Forces WebView to do rendering work on LITTLE CPU cores on big.LITTLE "
                            + "architectures"),
            Flag.baseFeature(PowerSchedulerFeatures.WEBVIEW_POWER_SCHEDULER_THROTTLE_IDLE,
                    "Restricts all of WebView's out-of-process renderer threads to use only LITTLE "
                            + "CPU cores on big.LITTLE architectures when the power mode is idle. "
                            + "WebViewCpuAffinityRestrictToLittleCores, if set, takes precedence "
                            + "over this flag."),
            Flag.baseFeature(PowerSchedulerFeatures.POWER_SCHEDULER,
                    "Enables the Power Scheduler. Defaults to throttling when idle or in no-op "
                            + "animations, if at least 250ms of CPU time were spent "
                            + "in the first 500ms after entering idle/no-op animation mode. "
                            + "Can be further configured via field trial parameters, "
                            + "see power_scheduler.h/cc for details."),
            Flag.baseFeature(BlinkFeatures.WEBVIEW_ACCELERATE_SMALL_CANVASES,
                    "Accelerate all canvases in webview."),
            Flag.baseFeature(AwFeatures.WEBVIEW_MIXED_CONTENT_AUTOUPGRADES,
                    "Enables autoupgrades for audio/video/image mixed content when mixed content "
                            + "mode is set to MIXED_CONTENT_COMPATIBILITY_MODE"),
            Flag.baseFeature(AwFeatures.WEBVIEW_JAVA_JS_BRIDGE_MOJO,
                    "Enables the new Java/JS Bridge code path with mojo implementation."),
            Flag.baseFeature(
                    AwFeatures.WEBVIEW_ORIGIN_TRIALS, "Enables Origin Trials support on WebView."),
            Flag.baseFeature(BlinkFeatures.GMS_CORE_EMOJI,
                    "Enables retrieval of the emoji font through GMS Core "
                            + "improving emoji glyph coverage."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ACROSS_IFRAMES,
                    "Enable Autofill for frame-transcending forms (forms whose fields live in "
                            + "different frames)."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_MORE_STRUCTURE_IN_NAMES,
                    "Enables support for names with a rich structure including multiple last "
                            + "names."),
            Flag.baseFeature(
                    AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_MORE_STRUCTURE_IN_ADDRESSES,
                    "Enables support for address with a rich structure including separate street "
                            + "names and house numberse."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_FIX_FILLABLE_FIELD_TYPES,
                    "Fix how it is determined if a field type is fillable with Autofill"),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENABLE_MERCHANT_BOUND_VIRTUAL_CARDS,
                    "When enabled, merchant bound virtual cards will be offered when users "
                            + "interact with a payment form."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENABLE_AUGMENTED_PHONE_COUNTRY_CODE,
                    "Enables support for phone code number fields with additional text."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_USE_UNASSOCIATED_LISTED_ELEMENTS,
                    "Caches unowned listed elements in the document."),
            Flag.baseFeature(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE,
                    "When enabled, merchant bound virtual cards will be offered in the keyboard "
                            + "accessory."),
            Flag.baseFeature(
                    NetworkServiceFeatures.TRUST_TOKENS, "Enables the prototype Trust Tokens API."),
            Flag.baseFeature(AwFeatures.WEBVIEW_APPS_PACKAGE_NAMES_ALLOWLIST,
                    "Enables using a server-defined allowlist of apps whose name can be recorded "
                            + "in UMA logs. The allowlist is downloaded and fetched via component "
                            + "updater services in WebView."),
            Flag.commandLine(AwSwitches.WEBVIEW_DISABLE_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT,
                    "Disable downloading the apps package names allowlist component by the "
                            + "component updater."),
            Flag.commandLine(AwSwitches.WEBVIEW_SELECTIVE_IMAGE_INVERSION_DARKENING,
                    "Enables use selective image inversion to automatically darken page, it will be"
                            + " used when WebView is in dark mode, but website doesn't provide dark"
                            + " style."),
            Flag.baseFeature(AwFeatures.WEBVIEW_DARK_MODE_MATCH_THEME,
                    "Set prefers-color-theme according to the app's theme unless the app specifies"
                            + " FORCE_DARK_OFF or DARK_STRATEGY_USER_AGENT_DARKENING_ONLY."),
            Flag.baseFeature(AwFeatures.WEBVIEW_FORCE_DARK_MODE_MATCH_THEME,
                    "Automatically darken page if"
                            + " WebView is set to FORCE_DARK_AUTO and the app has dark theme"),
            Flag.baseFeature(ContentFeatures.VERIFY_DID_COMMIT_PARAMS,
                    "Enables reporting of browser and renderer navigation inconsistencies on"
                            + "navigations"),
            Flag.baseFeature(ContentFeatures.USER_MEDIA_CAPTURE_ON_FOCUS,
                    "Enables GetUserMedia API will only resolve when the document calling it has"
                            + "focus"),
            Flag.baseFeature(ContentFeatures.COMPOSITE_BG_COLOR_ANIMATION,
                    "When enabled, the background-color animation runs on the compositor thread."),
            Flag.baseFeature(AwFeatures.WEBVIEW_USE_METRICS_UPLOAD_SERVICE,
                    "Upload UMA metrics logs through MetricsUploadService not via GMS-core"
                            + " directly."),
            Flag.baseFeature(AwFeatures.WEBVIEW_LOG_FIRST_PARTY_PAGE_TIME_SPENT,
                    "Enables logging whether it was a first party page when logging"
                            + " PageTimeSpent."),
            Flag.baseFeature(BlinkFeatures.FORCE_MAJOR_VERSION100_IN_USER_AGENT,
                    "Force the Chrome major version number to 100 in the User-Agent string."),
            Flag.baseFeature(NetworkServiceFeatures.URL_LOADER_SYNC_CLIENT,
                    "Optimizes communication between URLLoader and CorsURLLoader."),
            Flag.baseFeature(BlinkFeatures.SET_TIMEOUT_WITHOUT_CLAMP,
                    "Enables faster setTimeout(,0) by removing the 1 ms clamping."),
            Flag.baseFeature(BlinkFeatures.PAINT_HOLDING_CROSS_ORIGIN,
                    "Defers the first commit until FCP or timeout for cross-origin navigations."),
    };
}
