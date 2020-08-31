// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time_win_features.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "chrome/browser/sharing/webrtc/webrtc_flags.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/assist_ranker/predictor_config_definitions.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browsing_data/core/features.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_search/core/browser/public.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/download/public/common/download_features.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/favicon/core/features.h"
#include "components/feature_engagement/buildflags.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/games/core/games_features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/lookalikes/core/features.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/page_info/android/features.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/policy/core/common/features.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/query_tiles/switches.h"
#include "components/safe_browsing/core/features.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/features.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/send_tab_to_self/features.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "components/translate/core/common/translate_util.h"
#include "components/ui_devtools/switches.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/fido/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "media/midi/midi_switches.h"
#include "media/webrtc/webrtc_switches.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "services/media_session/public/cpp/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/switches.h"
#include "third_party/blink/public/common/experiments/memory_ablation_experiment.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/leveldatabase/leveldb_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/browser_ui/site_settings/android/features.h"
#include "components/external_intents/android/external_intents_feature_list.h"
#else  // OS_ANDROID
#include "chrome/browser/media/router/media_router_feature.h"
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/arc/arc_features.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/events/ozone/features.h"
#endif  // OS_CHROMEOS

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_dialogs.h"
#endif  // OS_MACOSX

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#endif  // ENABLE_EXTENSIONS

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_features.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif  // USE_OZONE

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/titlebar_config.h"
#endif  // OS_WIN

#if defined(TOOLKIT_VIEWS)
#include "ui/views/animation/installable_ink_drop.h"
#include "ui/views/views_features.h"
#include "ui/views/views_switches.h"
#endif  // defined(TOOLKIT_VIEWS)

using flags_ui::FeatureEntry;
using flags_ui::kDeprecated;
using flags_ui::kOsAndroid;
using flags_ui::kOsCrOS;
using flags_ui::kOsCrOSOwnerOnly;
using flags_ui::kOsLinux;
using flags_ui::kOsMac;
using flags_ui::kOsWin;

namespace about_flags {

namespace {

const unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid;
const unsigned kOsDesktop = kOsMac | kOsWin | kOsLinux | kOsCrOS;

#if defined(USE_AURA) || defined(OS_ANDROID)
const unsigned kOsAura = kOsWin | kOsLinux | kOsCrOS;
#endif  // USE_AURA || OS_ANDROID

const FeatureEntry::Choice kTouchEventFeatureDetectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kTouchEventFeatureDetection,
     switches::kTouchEventFeatureDetectionEnabled},
    {flags_ui::kGenericExperimentChoiceAutomatic,
     switches::kTouchEventFeatureDetection,
     switches::kTouchEventFeatureDetectionAuto}};

#if defined(USE_AURA)
const FeatureEntry::Choice kPullToRefreshChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kPullToRefresh, "0"},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kPullToRefresh, "1"},
    {flag_descriptions::kPullToRefreshEnabledTouchscreen,
     switches::kPullToRefresh, "2"}};
#endif  // USE_AURA

const FeatureEntry::Choice kOverlayStrategiesChoices[] = {
    {flag_descriptions::kOverlayStrategiesDefault, "", ""},
    {flag_descriptions::kOverlayStrategiesNone,
     switches::kEnableHardwareOverlays, ""},
    {flag_descriptions::kOverlayStrategiesUnoccludedFullscreen,
     switches::kEnableHardwareOverlays, "single-fullscreen"},
    {flag_descriptions::kOverlayStrategiesUnoccluded,
     switches::kEnableHardwareOverlays, "single-fullscreen,single-on-top"},
    {flag_descriptions::kOverlayStrategiesOccludedAndUnoccluded,
     switches::kEnableHardwareOverlays,
     "single-fullscreen,single-on-top,underlay"},
};

const FeatureEntry::Choice kTouchTextSelectionStrategyChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kTouchSelectionStrategyCharacter,
     switches::kTouchTextSelectionStrategy, "character"},
    {flag_descriptions::kTouchSelectionStrategyDirection,
     switches::kTouchTextSelectionStrategy, "direction"}};

const FeatureEntry::Choice kTraceUploadURL[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flag_descriptions::kTraceUploadUrlChoiceOther, switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,Other"},
    {flag_descriptions::kTraceUploadUrlChoiceEmloading,
     switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,emloading"},
    {flag_descriptions::kTraceUploadUrlChoiceQa, switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,QA"},
    {flag_descriptions::kTraceUploadUrlChoiceTesting, switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,TestingTeam"}};

const FeatureEntry::Choice kPassiveListenersChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kPassiveEventListenerTrue,
     switches::kPassiveListenersDefault, "true"},
    {flag_descriptions::kPassiveEventListenerForceAllTrue,
     switches::kPassiveListenersDefault, "forcealltrue"},
};

const FeatureEntry::Choice kDataReductionProxyServerExperiment[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kDataReductionProxyServerAlternative1,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative1},
    {flag_descriptions::kDataReductionProxyServerAlternative2,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative2},
    {flag_descriptions::kDataReductionProxyServerAlternative3,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative3},
    {flag_descriptions::kDataReductionProxyServerAlternative4,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative4},
    {flag_descriptions::kDataReductionProxyServerAlternative5,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative5},
    {flag_descriptions::kDataReductionProxyServerAlternative6,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative6},
    {flag_descriptions::kDataReductionProxyServerAlternative7,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative7},
    {flag_descriptions::kDataReductionProxyServerAlternative8,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative8},
    {flag_descriptions::kDataReductionProxyServerAlternative9,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative9},
    {flag_descriptions::kDataReductionProxyServerAlternative10,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative10}};

#if defined(OS_WIN)
const FeatureEntry::Choice kUseAngleChoices[] = {
    {flag_descriptions::kUseAngleDefault, "", ""},
    {flag_descriptions::kUseAngleGL, switches::kUseANGLE,
     gl::kANGLEImplementationOpenGLName},
    {flag_descriptions::kUseAngleD3D11, switches::kUseANGLE,
     gl::kANGLEImplementationD3D11Name},
    {flag_descriptions::kUseAngleD3D9, switches::kUseANGLE,
     gl::kANGLEImplementationD3D9Name},
    {flag_descriptions::kUseAngleD3D11on12, switches::kUseANGLE,
     gl::kANGLEImplementationD3D11on12Name}};
#endif

#if BUILDFLAG(ENABLE_VR)
const FeatureEntry::Choice kWebXrForceRuntimeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kWebXrRuntimeChoiceNone, switches::kWebXrForceRuntime,
     switches::kWebXrRuntimeNone},

#if BUILDFLAG(ENABLE_OPENXR)
    {flag_descriptions::kWebXrRuntimeChoiceOpenXR, switches::kWebXrForceRuntime,
     switches::kWebXrRuntimeOpenXr},
#endif  // ENABLE_OPENXR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    {flag_descriptions::kWebXrRuntimeChoiceWindowsMixedReality,
     switches::kWebXrForceRuntime, switches::kWebXrRuntimeWMR},
#endif  // ENABLE_WINDOWS_MR
};
#endif  // ENABLE_VR

#if defined(OS_ANDROID)
const FeatureEntry::Choice kReaderModeHeuristicsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kReaderModeHeuristicsMarkup,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kOGArticle},
    {flag_descriptions::kReaderModeHeuristicsAdaboost,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kAdaBoost},
    {flag_descriptions::kReaderModeHeuristicsAlwaysOn,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kAlwaysTrue},
    {flag_descriptions::kReaderModeHeuristicsAlwaysOff,
     switches::kReaderModeHeuristics, switches::reader_mode_heuristics::kNone},
    {flag_descriptions::kReaderModeHeuristicsAllArticles,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kAllArticles},
};

const FeatureEntry::Choice kForceUpdateMenuTypeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kUpdateMenuTypeNone, switches::kForceUpdateMenuType,
     "none"},
    {flag_descriptions::kUpdateMenuTypeUpdateAvailable,
     switches::kForceUpdateMenuType, "update_available"},
    {flag_descriptions::kUpdateMenuTypeUnsupportedOSVersion,
     switches::kForceUpdateMenuType, "unsupported_os_version"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateSuccess,
     switches::kForceUpdateMenuType, "inline_update_success"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDialogCanceled,
     switches::kForceUpdateMenuType, "inline_update_dialog_canceled"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDialogFailed,
     switches::kForceUpdateMenuType, "inline_update_dialog_failed"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDownloadFailed,
     switches::kForceUpdateMenuType, "inline_update_download_failed"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDownloadCanceled,
     switches::kForceUpdateMenuType, "inline_update_download_canceled"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateInstallFailed,
     switches::kForceUpdateMenuType, "inline_update_install_failed"},
};
#else  // !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kReaderModeOfferInSettings[] = {
    {switches::kReaderModeDiscoverabilityParamName,
     switches::kReaderModeOfferInSettings}};

const FeatureEntry::FeatureVariation kReaderModeDiscoverabilityVariations[] = {
    {"available in settings", kReaderModeOfferInSettings,
     base::size(kReaderModeOfferInSettings), nullptr}};
#endif  // OS_ANDROID

#if !defined(OS_CHROMEOS)
const FeatureEntry::FeatureParam kForceDark_SimpleHsl[] = {
    {"inversion_method", "hsl_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SimpleCielab[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SimpleRgb[] = {
    {"inversion_method", "rgb_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveImageInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "selective"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveElementInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveGeneralInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "selective"},
    {"text_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

const FeatureEntry::FeatureVariation kForceDarkVariations[] = {
    {"with simple HSL-based inversion", kForceDark_SimpleHsl,
     base::size(kForceDark_SimpleHsl), nullptr},
    {"with simple CIELAB-based inversion", kForceDark_SimpleCielab,
     base::size(kForceDark_SimpleCielab), nullptr},
    {"with simple RGB-based inversion", kForceDark_SimpleRgb,
     base::size(kForceDark_SimpleRgb), nullptr},
    {"with selective image inversion", kForceDark_SelectiveImageInversion,
     base::size(kForceDark_SelectiveImageInversion), nullptr},
    {"with selective inversion of non-image elements",
     kForceDark_SelectiveElementInversion,
     base::size(kForceDark_SelectiveElementInversion), nullptr},
    {"with selective inversion of everything",
     kForceDark_SelectiveGeneralInversion,
     base::size(kForceDark_SelectiveGeneralInversion), nullptr}};
#endif  // !OS_CHROMEOS

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_Immediate[] = {
    {"baseline_tab_suggestions", "true"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_4Hours[] = {
    {"close_tab_suggestions_stale_time_ms", "14400000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_8Hours[] = {
    {"close_tab_suggestions_stale_time_ms", "28800000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_7Days[] = {
    {"close_tab_suggestions_stale_time_ms", "604800000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsTimeSiteEngagement[] = {
    {"close_tab_min_num_tabs", "5"},
    {"close_tab_features_time_last_used_enabled", "true"},
    {"close_tab_features_time_last_used_transform", "MEAN_VARIANCE"},
    {"close_tab_features_time_last_used_threshold", "0.5"},
    {"close_tab_features_site_engagement_enabled", "true"},
    {"close_tab_features_site_engagement_threshold", "90.0"},
};

const FeatureEntry::FeatureVariation kCloseTabSuggestionsStaleVariations[] = {
    {"Immediate", kCloseTabSuggestionsStale_Immediate,
     base::size(kCloseTabSuggestionsStale_Immediate), nullptr},
    {"4 hours", kCloseTabSuggestionsStale_4Hours,
     base::size(kCloseTabSuggestionsStale_4Hours), nullptr},
    {"8 hours", kCloseTabSuggestionsStale_8Hours,
     base::size(kCloseTabSuggestionsStale_8Hours), nullptr},
    {"7 days", kCloseTabSuggestionsStale_7Days,
     base::size(kCloseTabSuggestionsStale_7Days), nullptr},
    {"Time & Site Engagement", kCloseTabSuggestionsTimeSiteEngagement,
     base::size(kCloseTabSuggestionsTimeSiteEngagement), nullptr},
};
#endif  // OS_ANDROID

const FeatureEntry::Choice kEnableGpuRasterizationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableGpuRasterization, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kDisableGpuRasterization, ""},
};

const FeatureEntry::Choice kEnableOopRasterizationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableOopRasterization, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kDisableOopRasterization, ""},
};

const FeatureEntry::Choice kExtensionContentVerificationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kExtensionContentVerificationBootstrap,
     switches::kExtensionContentVerification,
     switches::kExtensionContentVerificationBootstrap},
    {flag_descriptions::kExtensionContentVerificationEnforce,
     switches::kExtensionContentVerification,
     switches::kExtensionContentVerificationEnforce},
    {flag_descriptions::kExtensionContentVerificationEnforceStrict,
     switches::kExtensionContentVerification,
     switches::kExtensionContentVerificationEnforceStrict},
};

const FeatureEntry::Choice kTopChromeTouchUiChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceAutomatic, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiAuto},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiDisabled},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiEnabled}};

#if defined(OS_CHROMEOS)
const char kLacrosSupportInternalName[] = "lacros-support";

const FeatureEntry::Choice kUiShowCompositedLayerBordersChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kUiShowCompositedLayerBordersRenderPass,
     cc::switches::kUIShowCompositedLayerBorders,
     cc::switches::kCompositedRenderPassBorders},
    {flag_descriptions::kUiShowCompositedLayerBordersSurface,
     cc::switches::kUIShowCompositedLayerBorders,
     cc::switches::kCompositedSurfaceBorders},
    {flag_descriptions::kUiShowCompositedLayerBordersLayer,
     cc::switches::kUIShowCompositedLayerBorders,
     cc::switches::kCompositedLayerBorders},
    {flag_descriptions::kUiShowCompositedLayerBordersAll,
     cc::switches::kUIShowCompositedLayerBorders, ""}};

#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kCrosRegionsModeChoices[] = {
    {flag_descriptions::kCrosRegionsModeDefault, "", ""},
    {flag_descriptions::kCrosRegionsModeOverride,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeOverride},
    {flag_descriptions::kCrosRegionsModeHide,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeHide},
};
#endif  // OS_CHROMEOS

const FeatureEntry::Choice kForceUIDirectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kForceDirectionLtr, switches::kForceUIDirection,
     switches::kForceDirectionLTR},
    {flag_descriptions::kForceDirectionRtl, switches::kForceUIDirection,
     switches::kForceDirectionRTL},
};

const FeatureEntry::Choice kForceTextDirectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kForceDirectionLtr, switches::kForceTextDirection,
     switches::kForceDirectionLTR},
    {flag_descriptions::kForceDirectionRtl, switches::kForceTextDirection,
     switches::kForceDirectionRTL},
};

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kSchedulerConfigurationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kSchedulerConfigurationConservative,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationConservative},
    {flag_descriptions::kSchedulerConfigurationPerformance,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationPerformance},
};
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kInterestFeedLargerImagesFeatureVariationConstant[] = {
        {"feed_ui_enabled", "true"},
        {"snippets_enabled", "false"},
        {"undoable_actions_enabled", "false"},
        {"manage_interests_enabled", "false"},
        {"card_menu_tooltip_eligible", "false"}};
const FeatureEntry::FeatureParam
    kInterestFeedSnippetsFeatureVariationConstant[] = {
        {"feed_ui_enabled", "false"},
        {"snippets_enabled", "true"},
        {"undoable_actions_enabled", "false"},
        {"manage_interests_enabled", "false"},
        {"card_menu_tooltip_eligible", "false"}};
const FeatureEntry::FeatureParam
    kInterestFeedLargeImagesAndSnippetsFeatureVariationConstant[] = {
        {"feed_ui_enabled", "true"},
        {"snippets_enabled", "true"},
        {"undoable_actions_enabled", "false"},
        {"manage_interests_enabled", "false"},
        {"card_menu_tooltip_eligible", "false"}};
const FeatureEntry::FeatureParam
    kInterestFeedLargerImagesWithUndoableActionsFeatureVariationConstant[] = {
        {"feed_ui_enabled", "true"},
        {"snippets_enabled", "false"},
        {"undoable_actions_enabled", "true"},
        {"manage_interests_enabled", "true"},
        {"card_menu_tooltip_eligible", "true"}};
const FeatureEntry::FeatureParam
    kInterestFeedSnippetsWithUndoableActionsFeatureVariationConstant[] = {
        {"feed_ui_enabled", "false"},
        {"snippets_enabled", "true"},
        {"undoable_actions_enabled", "true"},
        {"manage_interests_enabled", "true"},
        {"card_menu_tooltip_eligible", "true"}};
const FeatureEntry::FeatureParam
    kInterestFeedLargeImagesAndSnippetsWithUndoableActionsFeatureVariationConstant
        [] = {{"feed_ui_enabled", "true"},
              {"snippets_enabled", "true"},
              {"undoable_actions_enabled", "true"},
              {"manage_interests_enabled", "true"},
              {"card_menu_tooltip_eligible", "true"}};
const FeatureEntry::FeatureVariation kInterestFeedFeatureVariations[] = {
    {"(larger images)", kInterestFeedLargerImagesFeatureVariationConstant,
     base::size(kInterestFeedLargerImagesFeatureVariationConstant), nullptr},
    {"(snippets)", kInterestFeedSnippetsFeatureVariationConstant,
     base::size(kInterestFeedSnippetsFeatureVariationConstant), nullptr},
    {"(larger images and snippets)",
     kInterestFeedLargeImagesAndSnippetsFeatureVariationConstant,
     base::size(kInterestFeedLargeImagesAndSnippetsFeatureVariationConstant),
     nullptr},
    {"(larger images w/ undoable actions)",
     kInterestFeedLargerImagesWithUndoableActionsFeatureVariationConstant,
     base::size(
         kInterestFeedLargerImagesWithUndoableActionsFeatureVariationConstant),
     nullptr},
    {"(snippets w/ undoable actions)",
     kInterestFeedSnippetsWithUndoableActionsFeatureVariationConstant,
     base::size(
         kInterestFeedSnippetsWithUndoableActionsFeatureVariationConstant),
     nullptr},
    {"(larger images and snippets w/ undoable actions)",
     kInterestFeedLargeImagesAndSnippetsWithUndoableActionsFeatureVariationConstant,
     base::size(
         kInterestFeedLargeImagesAndSnippetsWithUndoableActionsFeatureVariationConstant),
     nullptr}};

const FeatureEntry::FeatureParam kCompactSuggestions_SemicompactVariant[] = {
    {"omnibox_compact_suggestions_variant", "semi-compact"}};

const FeatureEntry::FeatureVariation kCompactSuggestionsVariations[] = {
    {"- Semi-compact", kCompactSuggestions_SemicompactVariant,
     base::size(kCompactSuggestions_SemicompactVariant), nullptr},
};
#endif  // OS_ANDROID

const FeatureEntry::Choice kEnableUseZoomForDSFChoices[] = {
    {flag_descriptions::kEnableUseZoomForDsfChoiceDefault, "", ""},
    {flag_descriptions::kEnableUseZoomForDsfChoiceEnabled,
     switches::kEnableUseZoomForDSF, "true"},
    {flag_descriptions::kEnableUseZoomForDsfChoiceDisabled,
     switches::kEnableUseZoomForDSF, "false"},
};

const FeatureEntry::Choice kSiteIsolationOptOutChoices[] = {
    {flag_descriptions::kSiteIsolationOptOutChoiceDefault, "", ""},
    {flag_descriptions::kSiteIsolationOptOutChoiceOptOut,
     switches::kDisableSiteIsolation, ""},
};

const FeatureEntry::Choice kForceColorProfileChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kForceColorProfileSRGB,
     switches::kForceDisplayColorProfile, "srgb"},
    {flag_descriptions::kForceColorProfileP3,
     switches::kForceDisplayColorProfile, "display-p3-d65"},
    {flag_descriptions::kForceColorProfileColorSpin,
     switches::kForceDisplayColorProfile, "color-spin-gamma24"},
    {flag_descriptions::kForceColorProfileSCRGBLinear,
     switches::kForceDisplayColorProfile, "scrgb-linear"},
    {flag_descriptions::kForceColorProfileHDR10,
     switches::kForceDisplayColorProfile, "hdr10"},
};

const FeatureEntry::Choice kForceEffectiveConnectionTypeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kEffectiveConnectionTypeUnknownDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeUnknown},
    {flag_descriptions::kEffectiveConnectionTypeOfflineDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeOffline},
    {flag_descriptions::kEffectiveConnectionTypeSlow2GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeSlow2G},
    {flag_descriptions::kEffectiveConnectionTypeSlow2GOnCellularDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeSlow2GOnCellular},
    {flag_descriptions::kEffectiveConnectionType2GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionType2G},
    {flag_descriptions::kEffectiveConnectionType3GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionType3G},
    {flag_descriptions::kEffectiveConnectionType4GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionType4G},
};

// Ensure that all effective connection types returned by Network Quality
// Estimator (NQE) are also exposed via flags.
static_assert(net::EFFECTIVE_CONNECTION_TYPE_LAST + 2 ==
                  base::size(kForceEffectiveConnectionTypeChoices),
              "ECT enum value is not handled.");
static_assert(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN == 0,
              "ECT enum value is not handled.");
static_assert(net::EFFECTIVE_CONNECTION_TYPE_4G + 1 ==
                  net::EFFECTIVE_CONNECTION_TYPE_LAST,
              "ECT enum value is not handled.");

const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitNone[] = {
    {"max_srp_prefetches", "-1"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitZero[] = {
    {"max_srp_prefetches", "0"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitOne[] = {
    {"max_srp_prefetches", "1"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitTwo[] = {
    {"max_srp_prefetches", "2"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitThree[] = {
    {"max_srp_prefetches", "3"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitFour[] = {
    {"max_srp_prefetches", "4"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitFive[] = {
    {"max_srp_prefetches", "5"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitTen[] = {
    {"max_srp_prefetches", "10"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitFifteen[] = {
    {"max_srp_prefetches", "15"}};

const FeatureEntry::FeatureVariation
    kIsolatedPrerenderFeatureWithPrefetchLimit[] = {
        {"Unlimited Prefetches", kIsolatedPrerenderPrefetchLimitNone,
         base::size(kIsolatedPrerenderPrefetchLimitNone), nullptr},
        {"Zero Prefetches", kIsolatedPrerenderPrefetchLimitZero,
         base::size(kIsolatedPrerenderPrefetchLimitZero), nullptr},
        {"One Prefetch", kIsolatedPrerenderPrefetchLimitOne,
         base::size(kIsolatedPrerenderPrefetchLimitOne), nullptr},
        {"Two Prefetches", kIsolatedPrerenderPrefetchLimitTwo,
         base::size(kIsolatedPrerenderPrefetchLimitTwo), nullptr},
        {"Three Prefetches", kIsolatedPrerenderPrefetchLimitThree,
         base::size(kIsolatedPrerenderPrefetchLimitThree), nullptr},
        {"Four Prefetches", kIsolatedPrerenderPrefetchLimitFour,
         base::size(kIsolatedPrerenderPrefetchLimitFour), nullptr},
        {"Five Prefetches", kIsolatedPrerenderPrefetchLimitFive,
         base::size(kIsolatedPrerenderPrefetchLimitFive), nullptr},
        {"Ten Prefetches", kIsolatedPrerenderPrefetchLimitTen,
         base::size(kIsolatedPrerenderPrefetchLimitTen), nullptr},
        {"Fifteen Prefetches", kIsolatedPrerenderPrefetchLimitFifteen,
         base::size(kIsolatedPrerenderPrefetchLimitFifteen), nullptr},
};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationAnimationDuration[] = {
        {autofill::kAutofillKeyboardAccessoryAnimationDurationKey, "1000"}};

const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth[] = {
        {autofill::kAutofillKeyboardAccessoryLimitLabelWidthKey, "true"}};

const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationShowHint[] = {
        {autofill::kAutofillKeyboardAccessoryHintKey, "true"}};

const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint[] = {
        {autofill::kAutofillKeyboardAccessoryAnimationDurationKey, "1000"},
        {autofill::kAutofillKeyboardAccessoryHintKey, "true"}};

const FeatureEntry::FeatureVariation
    kAutofillKeyboardAccessoryFeatureVariations[] = {
        {"Animate", kAutofillKeyboardAccessoryFeatureVariationAnimationDuration,
         base::size(
             kAutofillKeyboardAccessoryFeatureVariationAnimationDuration),
         nullptr},
        {"Limit label width",
         kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth,
         base::size(kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth),
         nullptr},
        {"Show hint", kAutofillKeyboardAccessoryFeatureVariationShowHint,
         base::size(kAutofillKeyboardAccessoryFeatureVariationShowHint),
         nullptr},
        {"Animate with hint",
         kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint,
         base::size(kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint),
         nullptr}};
#endif  // OS_ANDROID

const FeatureEntry::Choice kMemlogModeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flag_descriptions::kMemlogModeMinimal, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeMinimal},
    {flag_descriptions::kMemlogModeAll, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeAll},
    {flag_descriptions::kMemlogModeBrowser, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeBrowser},
    {flag_descriptions::kMemlogModeGpu, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeGpu},
    {flag_descriptions::kMemlogModeAllRenderers, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeAllRenderers},
    {flag_descriptions::kMemlogModeRendererSampling,
     heap_profiling::kMemlogMode, heap_profiling::kMemlogModeRendererSampling},
};

const FeatureEntry::Choice kMemlogStackModeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kMemlogStackModeNative,
     heap_profiling::kMemlogStackMode, heap_profiling::kMemlogStackModeNative},
    {flag_descriptions::kMemlogStackModeNativeWithThreadNames,
     heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModeNativeWithThreadNames},
    {flag_descriptions::kMemlogStackModePseudo,
     heap_profiling::kMemlogStackMode, heap_profiling::kMemlogStackModePseudo},
    {flag_descriptions::kMemlogStackModeMixed, heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModeMixed},
};

const FeatureEntry::Choice kMemlogSamplingRateChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kMemlogSamplingRate10KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate10KB},
    {flag_descriptions::kMemlogSamplingRate50KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate50KB},
    {flag_descriptions::kMemlogSamplingRate100KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate100KB},
    {flag_descriptions::kMemlogSamplingRate500KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate500KB},
    {flag_descriptions::kMemlogSamplingRate1MB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate1MB},
    {flag_descriptions::kMemlogSamplingRate5MB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate5MB},
};

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
const FeatureEntry::FeatureParam kOmniboxDocumentProviderServerScoring[] = {
    {"DocumentUseServerScore", "true"},
    {"DocumentUseClientScore", "false"},
    {"DocumentCapScorePerRank", "false"},
    {"DocumentBoostOwned", "false"},
};
const FeatureEntry::FeatureParam
    kOmniboxDocumentProviderServerScoringCappedByRank[] = {
        {"DocumentUseServerScore", "true"},
        {"DocumentUseClientScore", "false"},
        {"DocumentCapScorePerRank", "true"},
        {"DocumentBoostOwned", "true"},
};
const FeatureEntry::FeatureParam kOmniboxDocumentProviderClientScoring[] = {
    {"DocumentUseServerScore", "false"},
    {"DocumentUseClientScore", "true"},
    {"DocumentCapScorePerRank", "false"},
    {"DocumentBoostOwned", "false"},
};
const FeatureEntry::FeatureParam
    kOmniboxDocumentProviderServerAndClientScoring[] = {
        {"DocumentUseServerScore", "true"},
        {"DocumentUseClientScore", "true"},
        {"DocumentCapScorePerRank", "false"},
        {"DocumentBoostOwned", "false"},
};

const FeatureEntry::FeatureVariation kOmniboxDocumentProviderVariations[] = {
    {"server scores", kOmniboxDocumentProviderServerScoring,
     base::size(kOmniboxDocumentProviderServerScoring), nullptr},
    {"server scores capped by rank",
     kOmniboxDocumentProviderServerScoringCappedByRank,
     base::size(kOmniboxDocumentProviderServerScoringCappedByRank), nullptr},
    {"client scores", kOmniboxDocumentProviderClientScoring,
     base::size(kOmniboxDocumentProviderClientScoring), nullptr},
    {"server and client scores", kOmniboxDocumentProviderServerAndClientScoring,
     base::size(kOmniboxDocumentProviderServerAndClientScoring), nullptr}};

// The variations include 13 of the 16 possible permutations of "Title UI",
// "2-Line UI", "Title AC", and "Non-Prefix AC". The remaining 3 permutations
// would effectively be no-ops.
// - Title UI: Displays suggestion titles in the omnibox.
//   E.g. en.wikipe | [dia.org/wiki/Space_Shuttle] (Space Shuttle - Wikipedia)
// - 2-Line UI: Stretches the omnibox vertically to fit 2 lines and displays
//   titles on a 2nd line
//   E.g. en.wikipe | [dia.org/wiki/Space_Shuttle]
//        Space Shuttle - Wikipedia
// - Title AC: Autocompletes suggestions when the input matches the title.
//   E.g. Space Sh | [ttle - Wikipedia] (en.wikipedia.org/wiki/Space_Shuttle)
// - Non-Prefix AC: Autocompletes suggestions when the input is not necessarily
//   a prefix.
//   E.g. [en.wikipe dia.org/] wiki/Spac | [e_Shuttle] (Space Shuttle -
//   Wikipedia)
const FeatureEntry::FeatureVariation kOmniboxRichAutocompletionVariations[] = {
    {
        "Title UI",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionShowTitles", "true"}},
        1,
        nullptr,
    },
    // Skipping "2-Line UI" as that would be a no-op
    {
        "Title UI & 2-Line UI",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionShowTitles", "true"},
            {"RichAutocompletionTwoLineOmnibox", "true"}},
        2,
        nullptr,
    },
    {
        "Title AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteTitles", "true"}},
        1,
        nullptr,
    },
    {
        "Title UI & Title AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionShowTitles", "true"},
            {"RichAutocompletionAutocompleteTitles", "true"}},
        2,
        nullptr,
    },
    {
        "2-Line UI & Title AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionTwoLineOmnibox", "true"},
            {"RichAutocompletionAutocompleteTitles", "true"}},
        2,
        nullptr,
    },
    {
        "Title UI , 2-Line UI, & Title AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionShowTitles", "true"},
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionTwoLineOmnibox", "true"}},
        3,
        nullptr,
    },
    {
        "Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteNonPrefix", "true"}},
        1,
        nullptr,
    },
    {
        "Title UI & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionShowTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefix", "true"}},
        2,
        nullptr,
    },
    // Skipping "2-Line UI & Non-Prefix AC" as that would be a no-op
    {
        "Title UI, 2-Line UI, & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionShowTitles", "true"},
            {"RichAutocompletionTwoLineOmnibox", "true"},
            {"RichAutocompletionAutocompleteNonPrefix", "true"}},
        3,
        nullptr,
    },
    {
        "Title AC & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefix", "true"}},
        2,
        nullptr,
    },
    {
        "Title UI, Title AC, & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionShowTitles", "true"},
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefix", "true"}},
        3,
        nullptr,
    },
    {
        "2-Line UI, Title AC, & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionTwoLineOmnibox", "true"},
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefix", "true"}},
        3,
        nullptr,
    },
    {
        "Title UI, 2-Line UI, Title AC, & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteNonPrefix", "true"},
            {"RichAutocompletionShowTitles", "true"},
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionTwoLineOmnibox", "true"}},
        4,
        nullptr,
    }};

#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

const FeatureEntry::FeatureParam kOmniboxOnFocusSuggestionsParamSERP[] = {
    {"ZeroSuggestVariant:6:*", "RemoteSendUrl"},
    {"ZeroSuggestVariant:9:*", "RemoteSendUrl"}};
#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kOmniboxNTPZPSLocal[] = {
    {"ZeroSuggestVariant:1:*", "Local"},
    {"ZeroSuggestVariant:7:*", "Local"},
    {"ZeroSuggestVariant:8:*", "Local"}};
const FeatureEntry::FeatureParam kOmniboxNTPZPSRemote[] = {
    {"ZeroSuggestVariant:1:*", "RemoteNoUrl"},
    {"ZeroSuggestVariant:7:*", "RemoteNoUrl"},
    {"ZeroSuggestVariant:8:*", "RemoteNoUrl"}};
const FeatureEntry::FeatureParam kOmniboxNTPZPSRemoteLocal[] = {
    {"ZeroSuggestVariant:1:*", "RemoteNoUrl,Local"},
    {"ZeroSuggestVariant:7:*", "RemoteNoUrl,Local"},
    {"ZeroSuggestVariant:8:*", "RemoteNoUrl,Local"}};
#else   // !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kNTPOmniboxZPSRemoteLocal[] = {
    {"ZeroSuggestVariant:1:*", "RemoteNoUrl,Local"},
    {"ZeroSuggestVariant:7:*", "RemoteNoUrl,Local"}};
const FeatureEntry::FeatureParam kNTPOmniboxRealboxZPSRemoteLocal[] = {
    {"ZeroSuggestVariant:1:*", "RemoteNoUrl,Local"},
    {"ZeroSuggestVariant:7:*", "RemoteNoUrl,Local"},
    {"ZeroSuggestVariant:15:*", "RemoteNoUrl,Local"}};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureVariation kOmniboxOnFocusSuggestionsVariations[] = {
    {"SERP - RemoteSendURL", kOmniboxOnFocusSuggestionsParamSERP,
     base::size(kOmniboxOnFocusSuggestionsParamSERP),
     "t3315869" /* variation_id */},
#if defined(OS_ANDROID)
    {"ZPS on NTP: Local History", kOmniboxNTPZPSLocal,
     base::size(kOmniboxNTPZPSLocal), nullptr},
    {"ZPS on NTP: Remote History", kOmniboxNTPZPSRemote,
     base::size(kOmniboxNTPZPSRemote), /* variation_id */ "t3314248"},
    {"ZPS on NTP: Extended Remote History", kOmniboxNTPZPSRemote,
     base::size(kOmniboxNTPZPSRemote), /* variation_id */ "t3317456"},
    {"ZPS on NTP: Onboarding", kOmniboxNTPZPSRemote,
     base::size(kOmniboxNTPZPSRemote), /* variation_id */ "t3316638"},
    {"ZPS on NTP: PZPS, Remote, Local", kOmniboxNTPZPSRemoteLocal,
     base::size(kOmniboxNTPZPSRemoteLocal), /* variation_id */ "t3317569"},
#else   // !defined(OS_ANDROID)
    {"NTP Omnibox - Remote History, Local History", kNTPOmniboxZPSRemoteLocal,
     base::size(kNTPOmniboxZPSRemoteLocal), "t3316133" /* variation_id */},
    {"NTP Omnibox - Remote History + PZPS, Local History",
     kNTPOmniboxZPSRemoteLocal, base::size(kNTPOmniboxZPSRemoteLocal),
     "t3317462" /* variation_id */},
    {"NTP Omnibox/Realbox - Remote History, Local History",
     kNTPOmniboxRealboxZPSRemoteLocal,
     base::size(kNTPOmniboxRealboxZPSRemoteLocal),
     "t3316133" /* variation_id */},
    {"NTP Omnibox/Realbox - Remote History + PZPS, Local History",
     kNTPOmniboxRealboxZPSRemoteLocal,
     base::size(kNTPOmniboxRealboxZPSRemoteLocal),
     "t3317462" /* variation_id */},
#endif  // defined(OS_ANDROID)
};

const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches7[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "7"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches9[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "9"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

const FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3,
         base::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         base::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         base::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         base::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"7 matches", kOmniboxUIMaxAutocompleteMatches7,
         base::size(kOmniboxUIMaxAutocompleteMatches7), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         base::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"9 matches", kOmniboxUIMaxAutocompleteMatches9,
         base::size(kOmniboxUIMaxAutocompleteMatches9), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         base::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         base::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

const FeatureEntry::FeatureParam kOmniboxMaxURLMatches2[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "2"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches3[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches4[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches5[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches6[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "6"}};

const FeatureEntry::FeatureVariation kOmniboxMaxURLMatchesVariations[] = {
    {"2 matches", kOmniboxMaxURLMatches2, base::size(kOmniboxMaxURLMatches2),
     nullptr},
    {"3 matches", kOmniboxMaxURLMatches3, base::size(kOmniboxMaxURLMatches3),
     nullptr},
    {"4 matches", kOmniboxMaxURLMatches4, base::size(kOmniboxMaxURLMatches4),
     nullptr},
    {"5 matches", kOmniboxMaxURLMatches5, base::size(kOmniboxMaxURLMatches5),
     nullptr},
    {"6 matches", kOmniboxMaxURLMatches6, base::size(kOmniboxMaxURLMatches6),
     nullptr}};

const FeatureEntry::FeatureParam kMarkHttpAsDangerous[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerous}};
const FeatureEntry::FeatureParam kMarkHttpAsWarningAndDangerousOnFormEdits[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::
         kMarkHttpAsParameterWarningAndDangerousOnFormEdits}};
const FeatureEntry::FeatureParam kMarkHttpAsDangerWarning[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerWarning}};

// The "Enabled" state for this feature is "0" and representing setting A.
const FeatureEntry::FeatureParam kTabHoverCardsSettingB[] = {
    {features::kTabHoverCardsFeatureParameterName, "1"}};
const FeatureEntry::FeatureParam kTabHoverCardsSettingC[] = {
    {features::kTabHoverCardsFeatureParameterName, "2"}};

const FeatureEntry::FeatureVariation kTabHoverCardsFeatureVariations[] = {
    {"B", kTabHoverCardsSettingB, base::size(kTabHoverCardsSettingB), nullptr},
    {"C", kTabHoverCardsSettingC, base::size(kTabHoverCardsSettingC), nullptr}};

const FeatureEntry::FeatureVariation kMarkHttpAsFeatureVariations[] = {
    {"(mark as actively dangerous)", kMarkHttpAsDangerous,
     base::size(kMarkHttpAsDangerous), nullptr},
    {"(mark with a Not Secure warning and dangerous on form edits)",
     kMarkHttpAsWarningAndDangerousOnFormEdits,
     base::size(kMarkHttpAsWarningAndDangerousOnFormEdits), nullptr},
    {"(mark with a grey triangle icon)", kMarkHttpAsDangerWarning,
     base::size(kMarkHttpAsDangerWarning), nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishHeuristic[] = {
    {language::kOverrideModelKey, language::kOverrideModelHeuristicValue},
    {language::kEnforceRankerKey, "false"}};
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishGeo[] = {
    {language::kOverrideModelKey, language::kOverrideModelGeoValue},
    {language::kEnforceRankerKey, "false"}};
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishBackoff[] = {
    {language::kOverrideModelKey, language::kOverrideModelDefaultValue},
    {language::kEnforceRankerKey, "false"},
    {language::kBackoffThresholdKey, "0"}};
const FeatureEntry::FeatureVariation
    kTranslateForceTriggerOnEnglishVariations[] = {
        {"(Heuristic model without Ranker)",
         kTranslateForceTriggerOnEnglishHeuristic,
         base::size(kTranslateForceTriggerOnEnglishHeuristic), nullptr},
        {"(Geo model without Ranker)", kTranslateForceTriggerOnEnglishGeo,
         base::size(kTranslateForceTriggerOnEnglishGeo), nullptr},
        {"(Zero threshold)", kTranslateForceTriggerOnEnglishBackoff,
         base::size(kTranslateForceTriggerOnEnglishBackoff), nullptr}};

const FeatureEntry::FeatureParam kOverscrollHistoryNavigationBottomSheet[] = {
    {"overscroll_history_navigation_bottom_sheet", "true"}};
const FeatureEntry::FeatureVariation kOverscrollHistoryNavigationVariations[] =
    {{"Navigation sheet", kOverscrollHistoryNavigationBottomSheet,
      base::size(kOverscrollHistoryNavigationBottomSheet), nullptr}};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTabFreeze_FreezeNoUnfreeze[] = {
    {resource_coordinator::kTabFreeze_ShouldPeriodicallyUnfreezeParam,
     "false"}};
const FeatureEntry::FeatureParam kTabFreeze_FreezeWithUnfreeze[] = {
    {resource_coordinator::kTabFreeze_ShouldPeriodicallyUnfreezeParam, "true"}};
const FeatureEntry::FeatureVariation kTabFreezeVariations[] = {
    {"Freeze - No Unfreeze", kTabFreeze_FreezeNoUnfreeze,
     base::size(kTabFreeze_FreezeNoUnfreeze), nullptr},
    {"Freeze - Unfreeze 10 seconds every 15 minutes",
     kTabFreeze_FreezeWithUnfreeze, base::size(kTabFreeze_FreezeWithUnfreeze),
     nullptr}};
#endif

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kExploreSitesExperimental = {
    chrome::android::explore_sites::kExploreSitesVariationParameterName,
    chrome::android::explore_sites::kExploreSitesVariationExperimental};
const FeatureEntry::FeatureParam kExploreSitesDenseTitleBottom[] = {
    {chrome::android::explore_sites::kExploreSitesDenseVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesDenseVariationDenseTitleBottom},
};
const FeatureEntry::FeatureParam kExploreSitesDenseTitleRight[] = {
    {chrome::android::explore_sites::kExploreSitesDenseVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesDenseVariationDenseTitleRight},
};
const FeatureEntry::FeatureVariation kExploreSitesVariations[] = {
    {"Experimental", &kExploreSitesExperimental, 1, nullptr},
    {"Dense Title Bottom", kExploreSitesDenseTitleBottom,
     base::size(kExploreSitesDenseTitleBottom), nullptr},
    {"Dense Title Right", kExploreSitesDenseTitleRight,
     base::size(kExploreSitesDenseTitleRight), nullptr}};
const FeatureEntry::FeatureParam kLongpressResolvePreserveTap = {
    contextual_search::kLongpressResolveParamName,
    contextual_search::kLongpressResolvePreserveTap};
const FeatureEntry::FeatureVariation kLongpressResolveVariations[] = {
    {"and preserve Tap behavior", &kLongpressResolvePreserveTap, 1, nullptr},
};

#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kResamplingInputEventsLSQEnabled[] = {
    {"predictor", blink::features::kScrollPredictorNameLsq}};
const FeatureEntry::FeatureParam kResamplingInputEventsKalmanEnabled[] = {
    {"predictor", blink::features::kScrollPredictorNameKalman}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearFirstEnabled[] = {
    {"predictor", blink::features::kScrollPredictorNameLinearFirst}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearSecondEnabled[] = {
    {"predictor", blink::features::kScrollPredictorNameLinearSecond}};
const FeatureEntry::FeatureParam
    kResamplingInputEventsLinearResamplingEnabled[] = {
        {"predictor", blink::features::kScrollPredictorNameLinearResampling}};

const FeatureEntry::FeatureVariation kResamplingInputEventsFeatureVariations[] =
    {{blink::features::kScrollPredictorNameLsq,
      kResamplingInputEventsLSQEnabled,
      base::size(kResamplingInputEventsLSQEnabled), nullptr},
     {blink::features::kScrollPredictorNameKalman,
      kResamplingInputEventsKalmanEnabled,
      base::size(kResamplingInputEventsKalmanEnabled), nullptr},
     {blink::features::kScrollPredictorNameLinearFirst,
      kResamplingInputEventsLinearFirstEnabled,
      base::size(kResamplingInputEventsLinearFirstEnabled), nullptr},
     {blink::features::kScrollPredictorNameLinearSecond,
      kResamplingInputEventsLinearSecondEnabled,
      base::size(kResamplingInputEventsLinearSecondEnabled), nullptr},
     {blink::features::kScrollPredictorNameLinearResampling,
      kResamplingInputEventsLinearResamplingEnabled,
      base::size(kResamplingInputEventsLinearResamplingEnabled), nullptr}};

const FeatureEntry::FeatureParam kFilteringPredictionEmptyFilterEnabled[] = {
    {"filter", blink::features::kFilterNameEmpty}};
const FeatureEntry::FeatureParam kFilteringPredictionOneEuroFilterEnabled[] = {
    {"filter", blink::features::kFilterNameOneEuro}};

const FeatureEntry::FeatureVariation kFilteringPredictionFeatureVariations[] = {
    {blink::features::kFilterNameEmpty, kFilteringPredictionEmptyFilterEnabled,
     base::size(kFilteringPredictionEmptyFilterEnabled), nullptr},
    {blink::features::kFilterNameOneEuro,
     kFilteringPredictionOneEuroFilterEnabled,
     base::size(kFilteringPredictionOneEuroFilterEnabled), nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kBottomOfflineIndicatorEnabled[] = {
    {"bottom_offline_indicator", "true"}};

const FeatureEntry::FeatureVariation kOfflineIndicatorFeatureVariations[] = {
    {"(bottom)", kBottomOfflineIndicatorEnabled,
     base::size(kBottomOfflineIndicatorEnabled), nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_Immediate[] = {
    {"tab_switcher_on_return_time_ms", "0"}};
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_1Minute[] = {
    {"tab_switcher_on_return_time_ms", "60000"}};
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_30Minutes[] = {
    {"tab_switcher_on_return_time_ms", "1800000"}};
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_60Minutes[] = {
    {"tab_switcher_on_return_time_ms", "3600000"}};
const FeatureEntry::FeatureVariation kTabSwitcherOnReturnVariations[] = {
    {"Immediate", kTabSwitcherOnReturn_Immediate,
     base::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"1 minute", kTabSwitcherOnReturn_1Minute,
     base::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"30 minutes", kTabSwitcherOnReturn_30Minutes,
     base::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"60 minutes", kTabSwitcherOnReturn_60Minutes,
     base::size(kTabSwitcherOnReturn_60Minutes), nullptr},
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kChromeDuet_HomeSearchTabSwitcher[] = {
    {"chrome_duet_variation", "HomeSearchTabSwitcher"}};

const FeatureEntry::FeatureParam kChromeDuet_HomeSearchShare[] = {
    {"chrome_duet_variation", "HomeSearchShare"}};

const FeatureEntry::FeatureParam kChromeDuet_NewTabSearchShare[] = {
    {"chrome_duet_variation", "NewTabSearchShare"}};

const FeatureEntry::FeatureVariation kChromeDuetVariations[] = {
    {"Home-Search-TabSwitcher Variation", kChromeDuet_HomeSearchTabSwitcher,
     base::size(kChromeDuet_HomeSearchTabSwitcher), nullptr},
    {"Home-Search-Share Variation", kChromeDuet_HomeSearchShare,
     base::size(kChromeDuet_HomeSearchShare), nullptr},
    {"NewTab-Search-Share Variation", kChromeDuet_NewTabSearchShare,
     base::size(kChromeDuet_NewTabSearchShare), nullptr},
};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_NewTabVariation[] = {
    {"tab_grid_layout_android_new_tab", "NewTabVariation"},
    {"allow_to_refetch", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_NewTabTile[] = {
    {"tab_grid_layout_android_new_tab_tile", "NewTabTile"}};

const FeatureEntry::FeatureParam
    kTabGridLayoutAndroid_ThumbnailAspectRatio_2[] = {
        {"thumbnail_aspect_ratio", "2.0"},
        {"allow_to_refetch", "true"}};

const FeatureEntry::FeatureParam
    kTabGridLayoutAndroid_ThumbnailAspectRatio_three_quarter[] = {
        {"thumbnail_aspect_ratio", "0.75"},
        {"allow_to_refetch", "true"}};

const FeatureEntry::FeatureParam
    kTabGridLayoutAndroid_ThumbnailAspectRatio_half[] = {
        {"thumbnail_aspect_ratio", "0.5"},
        {"allow_to_refetch", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_DisableRefetch[] = {
    {"allow_to_refetch", "false"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_SearchChip[] = {
    {"enable_search_term_chip", "true"}};

const FeatureEntry::FeatureVariation kTabGridLayoutAndroidVariations[] = {
    {"New Tab Variation", kTabGridLayoutAndroid_NewTabVariation,
     base::size(kTabGridLayoutAndroid_NewTabVariation), nullptr},
    {"New Tab Tile", kTabGridLayoutAndroid_NewTabTile,
     base::size(kTabGridLayoutAndroid_NewTabTile), nullptr},
    {"thumbnail aspect ratio - 2:1",
     kTabGridLayoutAndroid_ThumbnailAspectRatio_2,
     base::size(kTabGridLayoutAndroid_ThumbnailAspectRatio_2), nullptr},
    {"thumbnail aspect ratio - 1:2",
     kTabGridLayoutAndroid_ThumbnailAspectRatio_half,
     base::size(kTabGridLayoutAndroid_ThumbnailAspectRatio_half), nullptr},
    {"thumbnail aspect ratio - 3:4",
     kTabGridLayoutAndroid_ThumbnailAspectRatio_three_quarter,
     base::size(kTabGridLayoutAndroid_ThumbnailAspectRatio_three_quarter),
     nullptr},
    {"Disable refetch", kTabGridLayoutAndroid_DisableRefetch,
     base::size(kTabGridLayoutAndroid_DisableRefetch), nullptr},
    {"Search term chip", kTabGridLayoutAndroid_SearchChip,
     base::size(kTabGridLayoutAndroid_SearchChip), nullptr},
};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface[] = {
    {"start_surface_variation", "single"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"exclude_mv_tiles", "true"},
    {"show_stack_tab_switcher", "true"},
    {"open_ntp_instead_of_start", "true"}};

const FeatureEntry::FeatureParam
    kStartSurfaceAndroid_SingleSurfaceWithoutMvTiles[] = {
        {"start_surface_variation", "single"},
        {"exclude_mv_tiles", "true"},
        {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurfaceSingleTab[] =
    {{"start_surface_variation", "single"},
     {"show_last_active_tab_only", "true"},
     {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureParam
    kStartSurfaceAndroid_SingleSurfaceSingleTabStack[] = {
        {"start_surface_variation", "single"},
        {"show_last_active_tab_only", "true"},
        {"show_stack_tab_switcher", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_TwoPanesSurface[] = {
    {"start_surface_variation", "twopanes"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_TasksOnly[] = {
    {"start_surface_variation", "tasksonly"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_OmniboxOnly[] = {
    {"start_surface_variation", "omniboxonly"},
    {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_OmniboxOnly_Quick[] = {
    {"start_surface_variation", "omniboxonly"},
    {"omnibox_scroll_mode", "quick"},
    {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_OmniboxOnly_Pinned[] = {
    {"start_surface_variation", "omniboxonly"},
    {"omnibox_scroll_mode", "pinned"},
    {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureVariation kStartSurfaceAndroidVariations[] = {
    {"Single Surface", kStartSurfaceAndroid_SingleSurface,
     base::size(kStartSurfaceAndroid_SingleSurface), nullptr},
    {"Single Surface V2", kStartSurfaceAndroid_SingleSurface_V2,
     base::size(kStartSurfaceAndroid_SingleSurface_V2), nullptr},
    {"Single Surface without MV Tiles",
     kStartSurfaceAndroid_SingleSurfaceWithoutMvTiles,
     base::size(kStartSurfaceAndroid_SingleSurfaceWithoutMvTiles), nullptr},
    {"Single Surface Single Tab", kStartSurfaceAndroid_SingleSurfaceSingleTab,
     base::size(kStartSurfaceAndroid_SingleSurfaceSingleTab), nullptr},
    {"Single Surface Single Tab + Tabs Stack",
     kStartSurfaceAndroid_SingleSurfaceSingleTabStack,
     base::size(kStartSurfaceAndroid_SingleSurfaceSingleTabStack), nullptr},
    {"Two Panes Surface", kStartSurfaceAndroid_TwoPanesSurface,
     base::size(kStartSurfaceAndroid_TwoPanesSurface), nullptr},
    {"Tasks Only", kStartSurfaceAndroid_TasksOnly,
     base::size(kStartSurfaceAndroid_TasksOnly), nullptr},
    {"Omnibox Only", kStartSurfaceAndroid_OmniboxOnly,
     base::size(kStartSurfaceAndroid_OmniboxOnly), nullptr},
    {"Omnibox Only, Quick", kStartSurfaceAndroid_OmniboxOnly_Quick,
     base::size(kStartSurfaceAndroid_OmniboxOnly_Quick), nullptr},
    {"Omnibox Only, Pinned", kStartSurfaceAndroid_OmniboxOnly_Pinned,
     base::size(kStartSurfaceAndroid_OmniboxOnly_Pinned), nullptr}};

const FeatureEntry::FeatureParam kConditionalTabStripAndroid_Immediate[] = {
    {"conditional_tab_strip_session_time_ms", "0"}};
const FeatureEntry::FeatureParam kConditionalTabStripAndroid_60Minutes[] = {
    {"conditional_tab_strip_session_time_ms", "3600000"}};
const FeatureEntry::FeatureVariation kConditionalTabStripAndroidVariations[] = {
    {"Immediate", kConditionalTabStripAndroid_Immediate,
     base::size(kConditionalTabStripAndroid_Immediate), nullptr},
    {"60 minutes", kConditionalTabStripAndroid_60Minutes,
     base::size(kConditionalTabStripAndroid_60Minutes), nullptr},
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowAll[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowAll}};
const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowOne[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowOne}};

const FeatureEntry::FeatureVariation
    kAutofillUseMobileLabelDisambiguationVariations[] = {
        {"(show all)", kAutofillUseMobileLabelDisambiguationShowAll,
         base::size(kAutofillUseMobileLabelDisambiguationShowAll), nullptr},
        {"(show one)", kAutofillUseMobileLabelDisambiguationShowOne,
         base::size(kAutofillUseMobileLabelDisambiguationShowOne), nullptr}};
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kHomepagePromoCardLarge[] = {
    {"promo-card-variation", "Large"}};
const FeatureEntry::FeatureParam kHomepagePromoCardCompact[] = {
    {"promo-card-variation", "Compact"}};
const FeatureEntry::FeatureParam kHomepagePromoCardSlim[] = {
    {"promo-card-variation", "Slim"}};

const FeatureEntry::FeatureVariation kHomepagePromoCardVariations[] = {
    {"Large", kHomepagePromoCardLarge, base::size(kHomepagePromoCardLarge),
     nullptr},
    {"Compact", kHomepagePromoCardCompact,
     base::size(kHomepagePromoCardCompact), nullptr},
    {"Slim", kHomepagePromoCardSlim, base::size(kHomepagePromoCardSlim),
     nullptr}};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kLazyFrameLoadingAutomatic[] = {
    {"automatic-lazy-load-frames-enabled", "true"},
    {"restrict-lazy-load-frames-to-data-saver-only", "false"},
};

const FeatureEntry::FeatureVariation kLazyFrameLoadingVariations[] = {
    {"(Automatically lazily load where safe even if not marked "
     "'loading=lazy')",
     kLazyFrameLoadingAutomatic, base::size(kLazyFrameLoadingAutomatic),
     nullptr}};

const FeatureEntry::FeatureParam kLazyImageLoadingAutomatic[] = {
    {"automatic-lazy-load-images-enabled", "true"},
    {"restrict-lazy-load-images-to-data-saver-only", "false"},
};

const FeatureEntry::FeatureVariation kLazyImageLoadingVariations[] = {
    {"(Automatically lazily load where safe even if not marked "
     "'loading=lazy')",
     kLazyImageLoadingAutomatic, base::size(kLazyImageLoadingAutomatic),
     nullptr}};

const FeatureEntry::Choice kNotificationSchedulerChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::
         kNotificationSchedulerImmediateBackgroundTaskDescription,
     notifications::switches::kNotificationSchedulerImmediateBackgroundTask,
     ""},
};

#if defined(OS_ANDROID)

const FeatureEntry::FeatureParam kOmniboxAssistantVoiceSearchGreyMic[] = {
    {"min_agsa_version", "10.95"},
    {"min_android_sdk", "21"},
    {"min_memory_mb", "1024"},
    {"enabled_locales", ""},
    {"colorful_mic", "false"}};

const FeatureEntry::FeatureParam kOmniboxAssistantVoiceSearchColorfulMic[] = {
    {"min_agsa_version", "10.95"},
    {"min_android_sdk", "21"},
    {"min_memory_mb", "1024"},
    {"enabled_locales", ""},
    {"colorful_mic", "true"}};

const FeatureEntry::FeatureVariation kOmniboxAssistantVoiceSearchVariations[] =
    {
        {"(grey mic)", kOmniboxAssistantVoiceSearchGreyMic,
         base::size(kOmniboxAssistantVoiceSearchGreyMic), nullptr},
        {"(colorful mic)", kOmniboxAssistantVoiceSearchColorfulMic,
         base::size(kOmniboxAssistantVoiceSearchColorfulMic), nullptr},
};

const FeatureEntry::FeatureParam
    kOmniboxSearchEngineLogoRoundedEdgesVariationConstant[] = {
        {"rounded_edges", "true"}};
const FeatureEntry::FeatureParam
    kOmniboxSearchEngineLogoLoupeEverywhereVariationConstant[] = {
        {"loupe_everywhere", "true"}};
const FeatureEntry::FeatureVariation
    kOmniboxSearchEngineLogoFeatureVariations[] = {
        {"(rounded edges)",
         kOmniboxSearchEngineLogoRoundedEdgesVariationConstant,
         base::size(kOmniboxSearchEngineLogoRoundedEdgesVariationConstant),
         nullptr},
        {"(loupe everywhere)",
         kOmniboxSearchEngineLogoLoupeEverywhereVariationConstant,
         base::size(kOmniboxSearchEngineLogoLoupeEverywhereVariationConstant),
         nullptr}};

const FeatureEntry::FeatureParam
    kOmniboxImageSearchSuggestionThumbnailVariationConstant[] = {
        {"ImageSearchSuggestionThumbnail", "true"}};
const FeatureEntry::FeatureVariation
    kOmniboxImageSearchSuggestionThumbnailVariation[] = {
        {"(with thumbnail)",
         kOmniboxImageSearchSuggestionThumbnailVariationConstant,
         base::size(kOmniboxImageSearchSuggestionThumbnailVariationConstant),
         nullptr}};
#endif  // OS_ANDROID

const FeatureEntry::FeatureVariation
    kOmniboxOnDeviceHeadSuggestIncognitoExperimentVariations[] = {{
        "relevance-1000",
        (FeatureEntry::FeatureParam[]){
            {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
             "1000"}},
        1,
        nullptr,
    }};

const FeatureEntry::FeatureVariation
    kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations[] = {
        {
            "relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            2,
            nullptr,
        },
        {
            "request-delay-100ms",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "100"}},
            1,
            nullptr,
        },
        {
            "delay-100ms-relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "100"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            3,
            nullptr,
        },
        {
            "request-delay-200ms",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "200"}},
            1,
            nullptr,
        },
        {
            "delay-200ms-relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "200"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            3,
            nullptr,
        }};

const FeatureEntry::FeatureParam
    kQuietNotificationPromptsWithAdaptiveActivation[] = {
        {QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
         "true"},
        {QuietNotificationPermissionUiConfig::kEnableAbusiveRequestBlocking,
         "true"},
        {QuietNotificationPermissionUiConfig::kEnableAbusiveRequestWarning,
         "true"},
        {QuietNotificationPermissionUiConfig::kEnableCrowdDenyTriggering,
         "true"},
        {QuietNotificationPermissionUiConfig::kCrowdDenyHoldBackChance, "0"}};

// The default "Enabled" option has the semantics of showing the quiet UI
// (animated location bar indicator on Desktop, and mini-infobars on Android),
// but only when the user directly turns it on in Settings. In addition to that,
// expose an option to also enable adaptively turning on the quiet UI after
// three consecutive denies or based on crowd deny verdicts.
const FeatureEntry::FeatureVariation kQuietNotificationPromptsVariations[] = {
    {"(with adaptive activation)",
     kQuietNotificationPromptsWithAdaptiveActivation,
     base::size(kQuietNotificationPromptsWithAdaptiveActivation), nullptr},
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
const FeatureEntry::FeatureParam kExtensionsCheckup_Startup_Performance[] = {
    {extensions_features::kExtensionsCheckupBannerMessageParameter,
     extensions_features::kPerformanceMessage},
    {extensions_features::kExtensionsCheckupEntryPointParameter,
     extensions_features::kStartupEntryPoint}};
const FeatureEntry::FeatureParam kExtensionsCheckup_Startup_Privacy[] = {
    {extensions_features::kExtensionsCheckupBannerMessageParameter,
     extensions_features::kPrivacyMessage},
    {extensions_features::kExtensionsCheckupEntryPointParameter,
     extensions_features::kStartupEntryPoint}};
const FeatureEntry::FeatureParam kExtensionsCheckup_Startup_Neutral[] = {
    {extensions_features::kExtensionsCheckupBannerMessageParameter,
     extensions_features::kNeutralMessage},
    {extensions_features::kExtensionsCheckupEntryPointParameter,
     extensions_features::kStartupEntryPoint}};
const FeatureEntry::FeatureParam kExtensionsCheckup_Promo_Performance[] = {
    {extensions_features::kExtensionsCheckupBannerMessageParameter,
     extensions_features::kPerformanceMessage},
    {extensions_features::kExtensionsCheckupEntryPointParameter,
     extensions_features::kNtpPromoEntryPoint}};
const FeatureEntry::FeatureParam kExtensionsCheckup_Promo_Privacy[] = {
    {extensions_features::kExtensionsCheckupBannerMessageParameter,
     extensions_features::kPrivacyMessage},
    {extensions_features::kExtensionsCheckupEntryPointParameter,
     extensions_features::kNtpPromoEntryPoint}};
const FeatureEntry::FeatureParam kExtensionsCheckup_Promo_Neutral[] = {
    {extensions_features::kExtensionsCheckupBannerMessageParameter,
     extensions_features::kNeutralMessage},
    {extensions_features::kExtensionsCheckupEntryPointParameter,
     extensions_features::kNtpPromoEntryPoint}};

const FeatureEntry::FeatureVariation kExtensionsCheckupVariations[] = {
    {"On Startup - Performance Focused Message",
     kExtensionsCheckup_Startup_Performance,
     base::size(kExtensionsCheckup_Startup_Performance), nullptr},
    {"On Startup - Privacy Focused Message", kExtensionsCheckup_Startup_Privacy,
     base::size(kExtensionsCheckup_Startup_Privacy), nullptr},
    {"On Startup - Neutral Focused Message", kExtensionsCheckup_Startup_Neutral,
     base::size(kExtensionsCheckup_Startup_Neutral), nullptr},
    {"NTP Promo - Performance Focused Message",
     kExtensionsCheckup_Promo_Performance,
     base::size(kExtensionsCheckup_Promo_Performance), nullptr},
    {"NTP Promo - Privacy Focused Message", kExtensionsCheckup_Promo_Privacy,
     base::size(kExtensionsCheckup_Promo_Privacy), nullptr},
    {"NTP Promo - Neutral Focused Message", kExtensionsCheckup_Promo_Neutral,
     base::size(kExtensionsCheckup_Promo_Neutral), nullptr}};
#endif  // ENABLE_EXTENSIONS

// TODO(crbug.com/991082,1015377): Remove after proper support for back-forward
// cache is implemented.
const FeatureEntry::FeatureParam kBackForwardCache_ForceCaching[] = {
    {"TimeToLiveInBackForwardCacheInSeconds", "300"},
    {"should_ignore_blocklists", "true"}};

const FeatureEntry::FeatureVariation kBackForwardCacheVariations[] = {
    {"force caching all pages", kBackForwardCache_ForceCaching,
     base::size(kBackForwardCache_ForceCaching), nullptr},
};

const FeatureEntry::FeatureParam kSharingDeviceExpirationHours_0[] = {
    {"SharingDeviceExpirationHours", "0"}};
const FeatureEntry::FeatureParam kSharingDeviceExpirationHours_12[] = {
    {"SharingDeviceExpirationHours", "12"}};
const FeatureEntry::FeatureParam kSharingDeviceExpirationHours_24[] = {
    {"SharingDeviceExpirationHours", "24"}};
const FeatureEntry::FeatureParam kSharingDeviceExpirationHours_48[] = {
    {"SharingDeviceExpirationHours", "48"}};
const FeatureEntry::FeatureParam kSharingDeviceExpirationHours_96[] = {
    {"SharingDeviceExpirationHours", "96"}};
const FeatureEntry::FeatureParam kSharingDeviceExpirationHours_240[] = {
    {"SharingDeviceExpirationHours", "240"}};
const FeatureEntry::FeatureVariation kSharingDeviceExpirationVariations[] = {
    {"0 hours", kSharingDeviceExpirationHours_0,
     base::size(kSharingDeviceExpirationHours_0), nullptr},
    {"12 hours", kSharingDeviceExpirationHours_12,
     base::size(kSharingDeviceExpirationHours_12), nullptr},
    {"1 day", kSharingDeviceExpirationHours_24,
     base::size(kSharingDeviceExpirationHours_24), nullptr},
    {"2 days", kSharingDeviceExpirationHours_48,
     base::size(kSharingDeviceExpirationHours_48), nullptr},
    {"4 days", kSharingDeviceExpirationHours_96,
     base::size(kSharingDeviceExpirationHours_96), nullptr},
    {"10 days", kSharingDeviceExpirationHours_240,
     base::size(kSharingDeviceExpirationHours_240), nullptr},
};

const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2250[] = {
        {"lcp-limit-in-ms", "2250"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2000[] = {
        {"lcp-limit-in-ms", "2000"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1750[] = {
        {"lcp-limit-in-ms", "1750"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1500[] = {
        {"lcp-limit-in-ms", "1500"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1250[] = {
        {"lcp-limit-in-ms", "1250"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1000[] = {
        {"lcp-limit-in-ms", "1000"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2250[] = {
        {"lcp-limit-in-ms", "2250"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2000[] = {
        {"lcp-limit-in-ms", "2000"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1750[] = {
        {"lcp-limit-in-ms", "1750"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1500[] = {
        {"lcp-limit-in-ms", "1500"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1250[] = {
        {"lcp-limit-in-ms", "1250"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1000[] = {
        {"lcp-limit-in-ms", "1000"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureVariation
    kAlignFontDisplayAutoTimeoutWithLCPGoalVariations[] = {
        {"switch to failure after 2250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2250),
         nullptr},
        {"switch to failure after 2000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2000),
         nullptr},
        {"switch to failure after 1750ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1750,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1750),
         nullptr},
        {"switch to failure after 1500ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1500,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1500),
         nullptr},
        {"switch to failure after 1250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1250),
         nullptr},
        {"switch to failure after 1000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1000),
         nullptr},
        {"switch to swap after 2250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2250), nullptr},
        {"switch to swap after 2000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2000), nullptr},
        {"switch to swap after 1750ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1750,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1750), nullptr},
        {"switch to swap after 1500ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1500,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1500), nullptr},
        {"switch to swap after 1250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1250), nullptr},
        {"switch to swap after 1000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1000), nullptr},
};

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kEnableCrOSActionRecorderChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {ash::switches::kCrOSActionRecorderWithHash,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderWithHash},
    {ash::switches::kCrOSActionRecorderWithoutHash,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderWithoutHash},
    {ash::switches::kCrOSActionRecorderCopyToDownloadDir,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderCopyToDownloadDir},
    {ash::switches::kCrOSActionRecorderDisabled,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderDisabled},
    {ash::switches::kCrOSActionRecorderStructuredDisabled,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderStructuredDisabled},
};
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
const FeatureEntry::Choice kWebOtpBackendChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kWebOtpBackendSmsVerification, switches::kWebOtpBackend,
     switches::kWebOtpBackendSmsVerification},
    {flag_descriptions::kWebOtpBackendUserConsent, switches::kWebOtpBackend,
     switches::kWebOtpBackendUserConsent},
};

const FeatureEntry::Choice kQueryTilesCountryChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kQueryTilesCountryCodeUS,
     query_tiles::switches::kQueryTilesCountryCode, "US"},
    {flag_descriptions::kQueryTilesCountryCodeIndia,
     query_tiles::switches::kQueryTilesCountryCode, "IN"},
    {flag_descriptions::kQueryTilesCountryCodeBrazil,
     query_tiles::switches::kQueryTilesCountryCode, "BR"},
    {flag_descriptions::kQueryTilesCountryCodeNigeria,
     query_tiles::switches::kQueryTilesCountryCode, "NG"},
    {flag_descriptions::kQueryTilesCountryCodeIndonesia,
     query_tiles::switches::kQueryTilesCountryCode, "ID"},
};

#endif  // defined(OS_ANDROID)

// The choices for --enable-experimental-cookie-features. This really should
// just be a SINGLE_VALUE_TYPE, but it is misleading to have the choices be
// labeled "Disabled"/"Enabled". So instead this is made to be a
// MULTI_VALUE_TYPE with choices "Default"/"Enabled".
const FeatureEntry::Choice kEnableExperimentalCookieFeaturesChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableExperimentalCookieFeatures, ""},
};

// RECORDING USER METRICS FOR FLAGS:
// -----------------------------------------------------------------------------
// The first line of the entry is the internal name.
//
// To add a new entry, add to the end of kFeatureEntries. There are two
// distinct types of entries:
// . SINGLE_VALUE: entry is either on or off. Use the SINGLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option). To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// See the documentation of FeatureEntry for details on the fields.
//
// Usage of about:flags is logged on startup via the "Launch.FlagsAtStartup"
// UMA histogram. This histogram shows the number of startups with a given flag
// enabled. If you'd like to see user counts instead, make sure to switch to
// "count users" view on the dashboard. When adding new entries, the enum
// "LoginCustomFlags" must be updated in histograms/enums.xml. See note in
// enums.xml and don't forget to run AboutFlagsHistogramTest unit test to
// calculate and verify checksum.
//
// When adding a new choice, add it to the end of the list.
const FeatureEntry kFeatureEntries[] = {
// Include generated flags for flag unexpiry; see //docs/flag_expiry.md and
// //tools/flags/generate_unexpire_flags.py.
#include "chrome/browser/unexpire_flags_gen.inc"
    {"ignore-gpu-blacklist", flag_descriptions::kIgnoreGpuBlacklistName,
     flag_descriptions::kIgnoreGpuBlacklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)},
    {"disable-accelerated-2d-canvas",
     flag_descriptions::kAccelerated2dCanvasName,
     flag_descriptions::kAccelerated2dCanvasDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAccelerated2dCanvas)},
    {"composited-layer-borders", flag_descriptions::kCompositedLayerBordersName,
     flag_descriptions::kCompositedLayerBordersDescription, kOsAll,
     SINGLE_VALUE_TYPE(cc::switches::kShowCompositedLayerBorders)},
    {"overlay-strategies", flag_descriptions::kOverlayStrategiesName,
     flag_descriptions::kOverlayStrategiesDescription, kOsAll,
     MULTI_VALUE_TYPE(kOverlayStrategiesChoices)},
    {"tint-gl-composited-content",
     flag_descriptions::kTintGlCompositedContentName,
     flag_descriptions::kTintGlCompositedContentDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kTintGlCompositedContent)},
    {"show-overdraw-feedback", flag_descriptions::kShowOverdrawFeedbackName,
     flag_descriptions::kShowOverdrawFeedbackDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kShowOverdrawFeedback)},
    {"ui-disable-partial-swap", flag_descriptions::kUiPartialSwapName,
     flag_descriptions::kUiPartialSwapDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kUIDisablePartialSwap)},
    {"enable-webrtc-capture-multi-channel-audio-processing",
     flag_descriptions::kWebrtcCaptureMultiChannelApmName,
     flag_descriptions::kWebrtcCaptureMultiChannelApmDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcEnableCaptureMultiChannelApm)},
    {"disable-webrtc-hw-decoding", flag_descriptions::kWebrtcHwDecodingName,
     flag_descriptions::kWebrtcHwDecodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding", flag_descriptions::kWebrtcHwEncodingName,
     flag_descriptions::kWebrtcHwEncodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)},
#if !defined(OS_ANDROID)
    {"enable-reader-mode", flag_descriptions::kEnableReaderModeName,
     flag_descriptions::kEnableReaderModeDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(dom_distiller::kReaderMode,
                                    kReaderModeDiscoverabilityVariations,
                                    "ReaderMode")},
#endif  // !defined(OS_ANDROID)
#if defined(WEBRTC_USE_PIPEWIRE)
    {"enable-webrtc-pipewire-capturer",
     flag_descriptions::kWebrtcPipeWireCapturerName,
     flag_descriptions::kWebrtcPipeWireCapturerDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWebRtcPipeWireCapturer)},
#endif  // defined(WEBRTC_USE_PIPEWIRE)
#if !defined(OS_ANDROID)
    {"enable-webrtc-remote-event-log",
     flag_descriptions::kWebRtcRemoteEventLogName,
     flag_descriptions::kWebRtcRemoteEventLogDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebRtcRemoteEventLog)},
#endif
    {"enable-webrtc-srtp-aes-gcm", flag_descriptions::kWebrtcSrtpAesGcmName,
     flag_descriptions::kWebrtcSrtpAesGcmDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcSrtpAesGcm)},
    {"enable-webrtc-stun-origin", flag_descriptions::kWebrtcStunOriginName,
     flag_descriptions::kWebrtcStunOriginDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcStunOrigin)},
    {"enable-webrtc-hybrid-agc", flag_descriptions::kWebrtcHybridAgcName,
     flag_descriptions::kWebrtcHybridAgcDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcHybridAgc)},
    {"enable-webrtc-new-encode-cpu-load-estimator",
     flag_descriptions::kWebrtcNewEncodeCpuLoadEstimatorName,
     flag_descriptions::kWebrtcNewEncodeCpuLoadEstimatorDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kNewEncodeCpuLoadEstimator)},
    {"enable-webrtc-hide-local-ips-with-mdns",
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsName,
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsDecription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebRtcHideLocalIpsWithMdns)},
    {"enable-webrtc-use-min-max-vea-dimensions",
     flag_descriptions::kWebrtcUseMinMaxVEADimensionsName,
     flag_descriptions::kWebrtcUseMinMaxVEADimensionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kWebRtcUseMinMaxVEADimensions)},
#if defined(OS_ANDROID)
    {"clear-old-browsing-data", flag_descriptions::kClearOldBrowsingDataName,
     flag_descriptions::kClearOldBrowsingDataDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kClearOldBrowsingData)},
#endif  // OS_ANDROID
#if BUILDFLAG(ENABLE_NACL)
    {"enable-nacl", flag_descriptions::kNaclName,
     flag_descriptions::kNaclDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
#endif  // ENABLE_NACL
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extension-apis", flag_descriptions::kExperimentalExtensionApisName,
     flag_descriptions::kExperimentalExtensionApisDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExperimentalExtensionApis)},
    {"extension-checkup", flag_descriptions::kExtensionsCheckupName,
     flag_descriptions::kExtensionsCheckupDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(extensions_features::kExtensionsCheckup,
                                    kExtensionsCheckupVariations,
                                    "ExtensionsCheckup")},
    {"extensions-toolbar-menu", flag_descriptions::kExtensionsToolbarMenuName,
     flag_descriptions::kExtensionsToolbarMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kExtensionsToolbarMenu)},
    {"extensions-on-chrome-urls",
     flag_descriptions::kExtensionsOnChromeUrlsName,
     flag_descriptions::kExtensionsOnChromeUrlsDescription, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif  // ENABLE_EXTENSIONS
    {"enable-history-manipulation-intervention",
     flag_descriptions::kHistoryManipulationIntervention,
     flag_descriptions::kHistoryManipulationInterventionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHistoryManipulationIntervention)},
#if defined(OS_ANDROID)
    {"contextual-search-debug", flag_descriptions::kContextualSearchDebugName,
     flag_descriptions::kContextualSearchDebugDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchDebug)},
    {"contextual-search-definitions",
     flag_descriptions::kContextualSearchDefinitionsName,
     flag_descriptions::kContextualSearchDefinitionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchDefinitions)},
    {"contextual-search-longpress-resolve",
     flag_descriptions::kContextualSearchLongpressResolveName,
     flag_descriptions::kContextualSearchLongpressResolveDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kContextualSearchLongpressResolve,
         kLongpressResolveVariations,
         "ContextualSearchLongpressResolve")},
    {"contextual-search-ml-tap-suppression",
     flag_descriptions::kContextualSearchMlTapSuppressionName,
     flag_descriptions::kContextualSearchMlTapSuppressionDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchMlTapSuppression)},
    {"contextual-search-ranker-query",
     flag_descriptions::kContextualSearchRankerQueryName,
     flag_descriptions::kContextualSearchRankerQueryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(assist_ranker::kContextualSearchRankerQuery)},
    {"contextual-search-second-tap",
     flag_descriptions::kContextualSearchSecondTapName,
     flag_descriptions::kContextualSearchSecondTapDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchSecondTap)},
    {"contextual-search-translations",
     flag_descriptions::kContextualSearchTranslationsName,
     flag_descriptions::kContextualSearchTranslationsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchTranslations)},
    {"direct-actions", flag_descriptions::kDirectActionsName,
     flag_descriptions::kDirectActionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDirectActions)},
    {"explore-sites", flag_descriptions::kExploreSitesName,
     flag_descriptions::kExploreSitesDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kExploreSites,
                                    kExploreSitesVariations,
                                    "ExploreSites InitialCountries")},
    {"related-searches", flag_descriptions::kRelatedSearchesName,
     flag_descriptions::kRelatedSearchesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kRelatedSearches)},
#endif  // OS_ANDROID
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillShowTypePredictions)},
    {"smooth-scrolling", flag_descriptions::kSmoothScrollingName,
     flag_descriptions::kSmoothScrollingDescription,
     // Mac has a separate implementation with its own setting to disable.
     kOsLinux | kOsCrOS | kOsWin | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSmoothScrolling,
                               switches::kDisableSmoothScrolling)},
    {"sms-receiver-cross-device",
     flag_descriptions::kSmsReceiverCrossDeviceName,
     flag_descriptions::kSmsReceiverCrossDeviceDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSmsReceiverCrossDevice)},
    {"fractional-scroll-offsets",
     flag_descriptions::kFractionalScrollOffsetsName,
     flag_descriptions::kFractionalScrollOffsetsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFractionalScrollOffsets)},
#if defined(USE_AURA)
    {"overlay-scrollbars", flag_descriptions::kOverlayScrollbarsName,
     flag_descriptions::kOverlayScrollbarsDescription,
     // Uses the system preference on Mac (a different implementation).
     // On Android, this is always enabled.
     kOsAura, FEATURE_VALUE_TYPE(features::kOverlayScrollbar)},
    {"overlay-scrollbars-flash-after-scroll-update",
     flag_descriptions::kOverlayScrollbarsFlashAfterAnyScrollUpdateName,
     flag_descriptions::kOverlayScrollbarsFlashAfterAnyScrollUpdateDescription,
     kOsAura,
     FEATURE_VALUE_TYPE(features::kOverlayScrollbarFlashAfterAnyScrollUpdate)},
    {"overlay-scrollbars-flash-when-mouse-enter",
     flag_descriptions::kOverlayScrollbarsFlashWhenMouseEnterName,
     flag_descriptions::kOverlayScrollbarsFlashWhenMouseEnterDescription,
     kOsAura,
     FEATURE_VALUE_TYPE(features::kOverlayScrollbarFlashWhenMouseEnter)},
#endif  // USE_AURA
    {"enable-quic", flag_descriptions::kQuicName,
     flag_descriptions::kQuicDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableQuic, switches::kDisableQuic)},
    {"disable-javascript-harmony-shipping",
     flag_descriptions::kJavascriptHarmonyShippingName,
     flag_descriptions::kJavascriptHarmonyShippingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableJavaScriptHarmonyShipping)},
    {"enable-javascript-harmony", flag_descriptions::kJavascriptHarmonyName,
     flag_descriptions::kJavascriptHarmonyDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kJavaScriptHarmony)},
    {"enable-experimental-webassembly-features",
     flag_descriptions::kExperimentalWebAssemblyFeaturesName,
     flag_descriptions::kExperimentalWebAssemblyFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebAssemblyFeatures)},
    {"enable-webassembly-baseline", flag_descriptions::kEnableWasmBaselineName,
     flag_descriptions::kEnableWasmBaselineDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyBaseline)},
    {"enable-webassembly-lazy-compilation",
     flag_descriptions::kEnableWasmLazyCompilationName,
     flag_descriptions::kEnableWasmLazyCompilationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyLazyCompilation)},
    {"enable-webassembly-simd", flag_descriptions::kEnableWasmSimdName,
     flag_descriptions::kEnableWasmSimdDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblySimd)},
    {"enable-webassembly-threads", flag_descriptions::kEnableWasmThreadsName,
     flag_descriptions::kEnableWasmThreadsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyThreads)},
    {"enable-webassembly-tiering", flag_descriptions::kEnableWasmTieringName,
     flag_descriptions::kEnableWasmTieringDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyTiering)},
    {"enable-future-v8-vm-features", flag_descriptions::kV8VmFutureName,
     flag_descriptions::kV8VmFutureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8VmFuture)},
    {"enable-gpu-rasterization", flag_descriptions::kGpuRasterizationName,
     flag_descriptions::kGpuRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"enable-oop-rasterization", flag_descriptions::kOopRasterizationName,
     flag_descriptions::kOopRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableOopRasterizationChoices)},
    {"enable-experimental-web-platform-features",
     flag_descriptions::kExperimentalWebPlatformFeaturesName,
     flag_descriptions::kExperimentalWebPlatformFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
#if defined(OS_ANDROID)
    {"enable-app-notification-status-messaging",
     flag_descriptions::kAppNotificationStatusMessagingName,
     flag_descriptions::kAppNotificationStatusMessagingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(browser_ui::kAppNotificationStatusMessaging)},
#endif  // OS_ANDROID
    {"silent-debugger-extension-api",
     flag_descriptions::kSilentDebuggerExtensionApiName,
     flag_descriptions::kSilentDebuggerExtensionApiDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kSilentDebuggerExtensionAPI)},
#if BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_ANDROID)
    {"enable-android-spellchecker",
     flag_descriptions::kEnableAndroidSpellcheckerDescription,
     flag_descriptions::kEnableAndroidSpellcheckerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(spellcheck::kAndroidSpellCheckerNonLowEnd)},
#endif  // ENABLE_SPELLCHECK && OS_ANDROID
    {"top-chrome-touch-ui", flag_descriptions::kTopChromeTouchUiName,
     flag_descriptions::kTopChromeTouchUiDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTopChromeTouchUiChoices)},
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
    {"webui-tab-strip", flag_descriptions::kWebUITabStripName,
     flag_descriptions::kWebUITabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUITabStrip)},
    {"webui-tab-strip-demo-options",
     flag_descriptions::kWebUITabStripDemoOptionsName,
     flag_descriptions::kWebUITabStripDemoOptionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUITabStripDemoOptions)},
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
    {"focus-mode", flag_descriptions::kFocusMode,
     flag_descriptions::kFocusModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kFocusMode)},
    {"touch-events", flag_descriptions::kTouchEventsName,
     flag_descriptions::kTouchEventsDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTouchEventFeatureDetectionChoices)},
    {"disable-touch-adjustment", flag_descriptions::kTouchAdjustmentName,
     flag_descriptions::kTouchAdjustmentDescription,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableTouchAdjustment)},
#if defined(OS_CHROMEOS)
    {"disable-explicit-dma-fences",
     flag_descriptions::kDisableExplicitDmaFencesName,
     flag_descriptions::kDisableExplicitDmaFencesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableExplicitDmaFences)},
    // TODO(crbug.com/1012846): Remove this flag and provision when HDR is fully
    //  supported on ChromeOS.
    {"enable-use-hdr-transfer-function",
     flag_descriptions::kEnableUseHDRTransferFunctionName,
     flag_descriptions::kEnableUseHDRTransferFunctionDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUseHDRTransferFunction)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS)
    {"ash-limit-alt-tab-to-active-desk",
     flag_descriptions::kLimitAltTabToActiveDeskName,
     flag_descriptions::kLimitAltTabToActiveDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLimitAltTabToActiveDesk)},
    {"ash-enable-unified-desktop",
     flag_descriptions::kAshEnableUnifiedDesktopName,
     flag_descriptions::kAshEnableUnifiedDesktopDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUnifiedDesktop)},
    {"bluetooth-aggressive-appearance-filter",
     flag_descriptions::kBluetoothAggressiveAppearanceFilterName,
     flag_descriptions::kBluetoothAggressiveAppearanceFilterDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kBluetoothAggressiveAppearanceFilter)},
    {"bluetooth-fix-a2dp-packet-size",
     flag_descriptions::kBluetoothFixA2dpPacketSizeName,
     flag_descriptions::kBluetoothFixA2dpPacketSizeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothFixA2dpPacketSize)},
    {"bluetooth-next-handsfree-profile",
     flag_descriptions::kBluetoothNextHandsfreeProfileName,
     flag_descriptions::kBluetoothNextHandsfreeProfileDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothNextHandsfreeProfile)},
    {"cryptauth-v2-device-activity-status",
     flag_descriptions::kCryptAuthV2DeviceActivityStatusName,
     flag_descriptions::kCryptAuthV2DeviceActivityStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2DeviceActivityStatus)},
    {"cryptauth-v2-devicesync", flag_descriptions::kCryptAuthV2DeviceSyncName,
     flag_descriptions::kCryptAuthV2DeviceSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2DeviceSync)},
    {"cryptauth-v2-enrollment", flag_descriptions::kCryptAuthV2EnrollmentName,
     flag_descriptions::kCryptAuthV2EnrollmentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2Enrollment)},
    {"disable-cryptauth-v1-devicesync",
     flag_descriptions::kDisableCryptAuthV1DeviceSyncName,
     flag_descriptions::kDisableCryptAuthV1DeviceSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableCryptAuthV1DeviceSync)},
    {"disable-office-editing-component-app",
     flag_descriptions::kDisableOfficeEditingComponentAppName,
     flag_descriptions::kDisableOfficeEditingComponentAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableOfficeEditingComponentApp)},
    {"updated_cellular_activation_ui",
     flag_descriptions::kUpdatedCellularActivationUiName,
     flag_descriptions::kUpdatedCellularActivationUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUpdatedCellularActivationUi)},
    {"use_messages_staging_url", flag_descriptions::kUseMessagesStagingUrlName,
     flag_descriptions::kUseMessagesStagingUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseMessagesStagingUrl)},
    {"use-custom-messages-domain",
     flag_descriptions::kUseCustomMessagesDomainName,
     flag_descriptions::kUseCustomMessagesDomainDescription, kOsCrOS,
     ORIGIN_LIST_VALUE_TYPE(switches::kCustomAndroidMessagesDomain, "")},
    {"disable-cancel-all-touches",
     flag_descriptions::kDisableCancelAllTouchesName,
     flag_descriptions::kDisableCancelAllTouchesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableCancelAllTouches)},
    {
        "enable-background-blur",
        flag_descriptions::kEnableBackgroundBlurName,
        flag_descriptions::kEnableBackgroundBlurDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(ash::features::kEnableBackgroundBlur),
    },
    {"enable-notification-indicator",
     flag_descriptions::kNotificationIndicatorName,
     flag_descriptions::kNotificationIndicatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNotificationIndicator)},
    {"enable-app-list-search-autocomplete",
     flag_descriptions::kEnableAppListSearchAutocompleteName,
     flag_descriptions::kEnableAppListSearchAutocompleteDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppListSearchAutocomplete)},
    {kLacrosSupportInternalName, flag_descriptions::kLacrosSupportName,
     flag_descriptions::kLacrosSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLacrosSupport)},
    {"list-all-display-modes", flag_descriptions::kListAllDisplayModesName,
     flag_descriptions::kListAllDisplayModesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kListAllDisplayModes)},
    {"instant-tethering", flag_descriptions::kTetherName,
     flag_descriptions::kTetherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kInstantTethering)},
    {
        "new-shortcut-mapping",
        flag_descriptions::kEnableNewShortcutMappingName,
        flag_descriptions::kEnableNewShortcutMappingDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(features::kNewShortcutMapping),
    },
    {"shelf-hide-buttons-in-tablet",
     flag_descriptions::kHideShelfControlsInTabletModeName,
     flag_descriptions::kHideShelfControlsInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideShelfControlsInTabletMode)},
    {"shelf-app-scaling", flag_descriptions::kShelfAppScalingName,
     flag_descriptions::kShelfAppScalingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShelfAppScaling)},
    {"shelf-hotseat", flag_descriptions::kShelfHotseatName,
     flag_descriptions::kShelfHotseatDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShelfHotseat)},
    {"shelf-hover-previews", flag_descriptions::kShelfHoverPreviewsName,
     flag_descriptions::kShelfHoverPreviewsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kShelfHoverPreviews)},
    {"show-bluetooth-debug-log-toggle",
     flag_descriptions::kShowBluetoothDebugLogToggleName,
     flag_descriptions::kShowBluetoothDebugLogToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShowBluetoothDebugLogToggle)},
    {"show-bluetooth-device-battery",
     flag_descriptions::kShowBluetoothDeviceBatteryName,
     flag_descriptions::kShowBluetoothDeviceBatteryDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShowBluetoothDeviceBattery)},
    {"show-taps", flag_descriptions::kShowTapsName,
     flag_descriptions::kShowTapsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kShowTaps)},
    {"show-touch-hud", flag_descriptions::kShowTouchHudName,
     flag_descriptions::kShowTouchHudDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {"trim-on-all-frames-frozen", flag_descriptions::kTrimOnFreezeName,
     flag_descriptions::kTrimOnFreezeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kTrimOnFreeze)},
    {"trim-on-memory-pressure", flag_descriptions::kTrimOnMemoryPressureName,
     flag_descriptions::kTrimOnMemoryPressureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kTrimOnMemoryPressure)},
    {"message-center-redesign", flag_descriptions::kMessageCenterRedesignName,
     flag_descriptions::kMessageCenterRedesignDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUnifiedMessageCenterRefactor)},
    {"system-tray-mic-gain", flag_descriptions::kSystemTrayMicGainName,
     flag_descriptions::kSystemTrayMicGainDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSystemTrayMicGainSetting)},
#endif  // OS_CHROMEOS
    {
        "disable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
    {
        "disable-accelerated-video-encode",
        flag_descriptions::kAcceleratedVideoEncodeName,
        flag_descriptions::kAcceleratedVideoEncodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoEncode),
    },
#if defined(OS_CHROMEOS)
    {
        "zero-copy-video-capture",
        flag_descriptions::kZeroCopyVideoCaptureName,
        flag_descriptions::kZeroCopyVideoCaptureDescription,
        kOsCrOS,
        ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
            switches::kVideoCaptureUseGpuMemoryBuffer,
            "1",
            switches::kDisableVideoCaptureUseGpuMemoryBuffer,
            "1"),
    },
    {
        "ash-debug-shortcuts",
        flag_descriptions::kDebugShortcutsName,
        flag_descriptions::kDebugShortcutsDescription,
        kOsAll,
        SINGLE_VALUE_TYPE(ash::switches::kAshDebugShortcuts),
    },
    {"ui-slow-animations", flag_descriptions::kUiSlowAnimationsName,
     flag_descriptions::kUiSlowAnimationsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kUISlowAnimations)},
    {"ui-show-composited-layer-borders",
     flag_descriptions::kUiShowCompositedLayerBordersName,
     flag_descriptions::kUiShowCompositedLayerBordersDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kUiShowCompositedLayerBordersChoices)},
    {"enable-request-tablet-site", flag_descriptions::kRequestTabletSiteName,
     flag_descriptions::kRequestTabletSiteDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableRequestTabletSite)},
#endif  // OS_CHROMEOS
    {"debug-packed-apps", flag_descriptions::kDebugPackedAppName,
     flag_descriptions::kDebugPackedAppDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"username-first-flow", flag_descriptions::kUsernameFirstFlowName,
     flag_descriptions::kUsernameFirstFlowDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kUsernameFirstFlow)},
    {"enable-show-autofill-signatures",
     flag_descriptions::kShowAutofillSignaturesName,
     flag_descriptions::kShowAutofillSignaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(autofill::switches::kShowAutofillSignatures)},
    {"wallet-service-use-sandbox",
     flag_descriptions::kWalletServiceUseSandboxName,
     flag_descriptions::kWalletServiceUseSandboxDescription,
     kOsAndroid | kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
    {"enable-web-bluetooth-new-permissions-backend",
     flag_descriptions::kWebBluetoothNewPermissionsBackendName,
     flag_descriptions::kWebBluetoothNewPermissionsBackendDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebBluetoothNewPermissionsBackend)},
#if defined(USE_AURA) || defined(OS_ANDROID)
    {"overscroll-history-navigation",
     flag_descriptions::kOverscrollHistoryNavigationName,
     flag_descriptions::kOverscrollHistoryNavigationDescription,
     kOsAura | kOsAndroid,
#if defined(OS_ANDROID)
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kOverscrollHistoryNavigation,
                                    kOverscrollHistoryNavigationVariations,
                                    "OverscrollNavigation")},
#else
     FEATURE_VALUE_TYPE(features::kOverscrollHistoryNavigation)},
    {"pull-to-refresh", flag_descriptions::kPullToRefreshName,
     flag_descriptions::kPullToRefreshDescription, kOsAura,
     MULTI_VALUE_TYPE(kPullToRefreshChoices)},
#endif  // OS_ANDROID
#endif  // USE_AURA || OS_ANDROID
    {"enable-touch-drag-drop", flag_descriptions::kTouchDragDropName,
     flag_descriptions::kTouchDragDropDescription, kOsWin | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTouchDragDrop,
                               switches::kDisableTouchDragDrop)},
    {"touch-selection-strategy", flag_descriptions::kTouchSelectionStrategyName,
     flag_descriptions::kTouchSelectionStrategyDescription,
     kOsAndroid,  // TODO(mfomitchev): Add CrOS/Win/Linux support soon.
     MULTI_VALUE_TYPE(kTouchTextSelectionStrategyChoices)},
    {"trace-upload-url", flag_descriptions::kTraceUploadUrlName,
     flag_descriptions::kTraceUploadUrlDescription, kOsAll,
     MULTI_VALUE_TYPE(kTraceUploadURL)},
    {"enable-suggestions-with-substring-match",
     flag_descriptions::kSuggestionsWithSubStringMatchName,
     flag_descriptions::kSuggestionsWithSubStringMatchDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillTokenPrefixMatching)},
    {"enable-offer-store-unmasked-wallet-cards",
     flag_descriptions::kOfferStoreUnmaskedWalletCardsName,
     flag_descriptions::kOfferStoreUnmaskedWalletCardsDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableOfferStoreUnmaskedWalletCards,
         autofill::switches::kDisableOfferStoreUnmaskedWalletCards)},
#if defined(OS_CHROMEOS)
    {"enable-virtual-keyboard", flag_descriptions::kVirtualKeyboardName,
     flag_descriptions::kVirtualKeyboardDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)},
#endif  // OS_CHROMEOS
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     flag_descriptions::kDeviceDiscoveryNotificationsName,
     flag_descriptions::kDeviceDiscoveryNotificationsDescription, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications,
                               switches::kDisableDeviceDiscoveryNotifications)},
#endif  // BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#if defined(OS_WIN)
    {"enable-cloud-print-xps", flag_descriptions::kCloudPrintXpsName,
     flag_descriptions::kCloudPrintXpsDescription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableCloudPrintXps)},
#endif  // OS_WIN
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"enable-webgl2-compute-context",
     flag_descriptions::kWebGL2ComputeContextName,
     flag_descriptions::kWebGL2ComputeContextDescription,
     kOsWin | kOsLinux | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWebGL2ComputeContext)},
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"enable-webgl-draft-extensions",
     flag_descriptions::kWebglDraftExtensionsName,
     flag_descriptions::kWebglDraftExtensionsDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)},
#if defined(OS_ANDROID)
    {"enable-android-autofill-accessibility",
     flag_descriptions::kAndroidAutofillAccessibilityName,
     flag_descriptions::kAndroidAutofillAccessibilityDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidAutofillAccessibility)},
#endif  // OS_ANDROID
    {"enable-zero-copy", flag_descriptions::kZeroCopyName,
     flag_descriptions::kZeroCopyDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableZeroCopy,
                               switches::kDisableZeroCopy)},
    {"enable-vulkan", flag_descriptions::kEnableVulkanName,
     flag_descriptions::kEnableVulkanDescription,
     kOsWin | kOsLinux | kOsAndroid, FEATURE_VALUE_TYPE(features::kVulkan)},
#if defined(OS_MACOSX)
    {"disable-hosted-app-shim-creation",
     flag_descriptions::kHostedAppShimCreationName,
     flag_descriptions::kHostedAppShimCreationDescription, kOsMac,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableHostedAppShimCreation)},
    {"enable-hosted-app-quit-notification",
     flag_descriptions::kHostedAppQuitNotificationName,
     flag_descriptions::kHostedAppQuitNotificationDescription, kOsMac,
     SINGLE_VALUE_TYPE(switches::kHostedAppQuitNotification)},
#endif  // OS_MACOSX
#if defined(OS_ANDROID)
    {"translate-force-trigger-on-english",
     flag_descriptions::kTranslateForceTriggerOnEnglishName,
     flag_descriptions::kTranslateForceTriggerOnEnglishDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(language::kOverrideTranslateTriggerInIndia,
                                    kTranslateForceTriggerOnEnglishVariations,
                                    "OverrideTranslateTriggerInIndia")},
#endif  // OS_ANDROID

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS) && !defined(OS_CHROMEOS)
    {"enable-native-notifications",
     flag_descriptions::kNotificationsNativeFlagName,
     flag_descriptions::kNotificationsNativeFlagDescription,
     kOsMac | kOsLinux | kOsWin,
     FEATURE_VALUE_TYPE(features::kNativeNotifications)},
#endif  // ENABLE_NATIVE_NOTIFICATIONS
#if defined(OS_ANDROID)
    {"reader-mode-heuristics", flag_descriptions::kReaderModeHeuristicsName,
     flag_descriptions::kReaderModeHeuristicsDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"enable-chrome-duet", flag_descriptions::kChromeDuetName,
     flag_descriptions::kChromeDuetDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kChromeDuetFeature,
                                    kChromeDuetVariations,
                                    "ChromeDuet")},
    {"enable-chrome-duet-labels", flag_descriptions::kChromeDuetLabelsName,
     flag_descriptions::kChromeDuetLabelsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeDuetLabeled)},
    {"share-button-in-top-toolbar",
     flag_descriptions::kShareButtonInTopToolbarName,
     flag_descriptions::kShareButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShareButtonInTopToolbar)},
    {"chrome-share-qr-code", flag_descriptions::kChromeShareQRCodeName,
     flag_descriptions::kChromeShareQRCodeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeShareQRCode)},
    {"chrome-share-screenshot", flag_descriptions::kChromeShareScreenshotName,
     flag_descriptions::kChromeShareScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeShareScreenshot)},
    {"chrome-sharing-hub", flag_descriptions::kChromeSharingHubName,
     flag_descriptions::kChromeSharingHubDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeSharingHub)},
    {"chrome-sharing-hub-v1-5", flag_descriptions::kChromeSharingHubV15Name,
     flag_descriptions::kChromeSharingHubV15Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeSharingHubV15)},
    {"request-unbuffered-dispatch",
     flag_descriptions::kRequestUnbufferedDispatchName,
     flag_descriptions::kRequestUnbufferedDispatchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kRequestUnbufferedDispatch)},
#endif  // OS_ANDROID
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeChoiceName,
     flag_descriptions::kInProductHelpDemoModeChoiceDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"disable-threaded-scrolling", flag_descriptions::kThreadedScrollingName,
     flag_descriptions::kThreadedScrollingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableThreadedScrolling)},
    {"extension-content-verification",
     flag_descriptions::kExtensionContentVerificationName,
     flag_descriptions::kExtensionContentVerificationDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
#if defined(OS_CHROMEOS)
    {"enable-lock-screen-notification",
     flag_descriptions::kLockScreenNotificationName,
     flag_descriptions::kLockScreenNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenNotifications)},
#endif  // OS_CHROMEOS
    {"reduced-referrer-granularity",
     flag_descriptions::kReducedReferrerGranularityName,
     flag_descriptions::kReducedReferrerGranularityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kReducedReferrerGranularity)},
#if defined(OS_CHROMEOS)
    {"crostini-port-forwarding", flag_descriptions::kCrostiniPortForwardingName,
     flag_descriptions::kCrostiniPortForwardingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniPortForwarding)},
    {"crostini-show-mic-setting",
     flag_descriptions::kCrostiniShowMicSettingName,
     flag_descriptions::kCrostiniShowMicSettingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniShowMicSetting)},
    {"terminal-system-app", flag_descriptions::kTerminalSystemAppName,
     flag_descriptions::kTerminalSystemAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTerminalSystemApp)},
    {"terminal-system-app-legacy-settings",
     flag_descriptions::kTerminalSystemAppLegacySettingsName,
     flag_descriptions::kTerminalSystemAppLegacySettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTerminalSystemAppLegacySettings)},
    {"terminal-system-app-splits",
     flag_descriptions::kTerminalSystemAppSplitsName,
     flag_descriptions::kTerminalSystemAppSplitsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTerminalSystemAppSplits)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
    {"dynamic-tcmalloc-tuning", flag_descriptions::kDynamicTcmallocName,
     flag_descriptions::kDynamicTcmallocDescription, kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(performance_manager::features::kDynamicTcmallocTuning)},
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // OS_CHROMEOS || OS_LINUX
#if defined(OS_ANDROID)
    {"enable-credit-card-assist", flag_descriptions::kCreditCardAssistName,
     flag_descriptions::kCreditCardAssistDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardAssist)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"enable-site-isolation-for-password-sites",
     flag_descriptions::kSiteIsolationForPasswordSitesName,
     flag_descriptions::kSiteIsolationForPasswordSitesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSiteIsolationForPasswordSites)},
    {"enable-site-per-process", flag_descriptions::kStrictSiteIsolationName,
     flag_descriptions::kStrictSiteIsolationDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kSitePerProcess)},
    {"enable-process-sharing-with-default-site-instances",
     flag_descriptions::kProcessSharingWithDefaultSiteInstancesName,
     flag_descriptions::kProcessSharingWithDefaultSiteInstancesDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kProcessSharingWithDefaultSiteInstances)},
    {"enable-process-sharing-with-strict-site-instances",
     flag_descriptions::kProcessSharingWithStrictSiteInstancesName,
     flag_descriptions::kProcessSharingWithStrictSiteInstancesDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kProcessSharingWithStrictSiteInstances)},
#endif
    {"isolate-origins", flag_descriptions::kIsolateOriginsName,
     flag_descriptions::kIsolateOriginsDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kIsolateOrigins, "")},
    {"site-isolation-trial-opt-out",
     flag_descriptions::kSiteIsolationOptOutName,
     flag_descriptions::kSiteIsolationOptOutDescription, kOsAll,
     MULTI_VALUE_TYPE(kSiteIsolationOptOutChoices)},
    {"enable-use-zoom-for-dsf", flag_descriptions::kEnableUseZoomForDsfName,
     flag_descriptions::kEnableUseZoomForDsfDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableUseZoomForDSFChoices)},
    {"allow-previews", flag_descriptions::kPreviewsAllowedName,
     flag_descriptions::kPreviewsAllowedDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kPreviews)},
    {"ignore-previews-blocklist",
     flag_descriptions::kIgnorePreviewsBlacklistName,
     flag_descriptions::kIgnorePreviewsBlacklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(previews::switches::kIgnorePreviewsBlacklist)},
    {"enable-data-reduction-proxy-server-experiment",
     flag_descriptions::kEnableDataReductionProxyServerExperimentName,
     flag_descriptions::kEnableDataReductionProxyServerExperimentDescription,
     kOsAll, MULTI_VALUE_TYPE(kDataReductionProxyServerExperiment)},
    {"enable-subresource-redirect",
     flag_descriptions::kEnableSubresourceRedirectName,
     flag_descriptions::kEnableSubresourceRedirectDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kSubresourceRedirect)},
#if defined(OS_ANDROID)
    {"enable-offline-previews", flag_descriptions::kEnableOfflinePreviewsName,
     flag_descriptions::kEnableOfflinePreviewsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(previews::features::kOfflinePreviews)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
    {"enable-save-data", flag_descriptions::kEnableSaveDataName,
     flag_descriptions::kEnableSaveDataDescription, kOsCrOS | kOsLinux,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxy)},
    {"enable-navigation-predictor",
     flag_descriptions::kEnableNavigationPredictorName,
     flag_descriptions::kEnableNavigationPredictorDescription,
     kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(blink::features::kNavigationPredictor)},
#endif  // OS_CHROMEOS || OS_LINUX
    {"enable-preconnect-to-search",
     flag_descriptions::kEnablePreconnectToSearchName,
     flag_descriptions::kEnablePreconnectToSearchDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPreconnectToSearch)},
    {"enable-noscript-previews", flag_descriptions::kEnableNoScriptPreviewsName,
     flag_descriptions::kEnableNoScriptPreviewsDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kNoScriptPreviews)},
    {"enable-resource-loading-hints",
     flag_descriptions::kEnableResourceLoadingHintsName,
     flag_descriptions::kEnableResourceLoadingHintsDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kResourceLoadingHints)},
    {"enable-previews-coin-flip",
     flag_descriptions::kEnablePreviewsCoinFlipName,
     flag_descriptions::kEnablePreviewsCoinFlipDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kCoinFlipHoldback)},
    {"enable-google-srp-isolated-prerender-probing",
     flag_descriptions::kEnableSRPIsolatedPrerenderProbingName,
     flag_descriptions::kEnableSRPIsolatedPrerenderProbingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kIsolatePrerendersMustProbeOrigin)},
    {"enable-google-srp-isolated-prerenders",
     flag_descriptions::kEnableSRPIsolatedPrerendersName,
     flag_descriptions::kEnableSRPIsolatedPrerendersDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kIsolatePrerenders,
                                    kIsolatedPrerenderFeatureWithPrefetchLimit,
                                    "Prefetch Limit")},
    {"allow-insecure-localhost", flag_descriptions::kAllowInsecureLocalhostName,
     flag_descriptions::kAllowInsecureLocalhostDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"bypass-app-banner-engagement-checks",
     flag_descriptions::kBypassAppBannerEngagementChecksName,
     flag_descriptions::kBypassAppBannerEngagementChecksDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kBypassAppBannerEngagementChecks)},
    {"enable-desktop-pwas-local-updating",
     flag_descriptions::kDesktopPWAsLocalUpdatingName,
     flag_descriptions::kDesktopPWAsLocalUpdatingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsLocalUpdating)},
    // TODO(https://crbug.com/1069293): Add macOS and Linux implementations.
    {"enable-desktop-pwas-app-icon-shortcuts-menu",
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuName,
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsAppIconShortcutsMenu)},
    {"enable-desktop-pwas-tab-strip",
     flag_descriptions::kDesktopPWAsTabStripName,
     flag_descriptions::kDesktopPWAsTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStrip)},
    {"enable-desktop-pwas-tab-strip-link-capturing",
     flag_descriptions::kDesktopPWAsTabStripLinkCapturingName,
     flag_descriptions::kDesktopPWAsTabStripLinkCapturingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStripLinkCapturing)},
    {"enable-desktop-pwas-without-extensions",
     flag_descriptions::kDesktopPWAsWithoutExtensionsName,
     flag_descriptions::kDesktopPWAsWithoutExtensionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsWithoutExtensions)},
    {"enable-system-webapps", flag_descriptions::kEnableSystemWebAppsName,
     flag_descriptions::kEnableSystemWebAppsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemWebApps)},
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, kOsAll,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
#if !defined(OS_ANDROID)
    {"load-media-router-component-extension",
     flag_descriptions::kLoadMediaRouterComponentExtensionName,
     flag_descriptions::kLoadMediaRouterComponentExtensionDescription,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         switches::kLoadMediaRouterComponentExtension,
         "1",
         switches::kLoadMediaRouterComponentExtension,
         "0")},
    {"media-router-cast-allow-all-ips",
     flag_descriptions::kMediaRouterCastAllowAllIPsName,
     flag_descriptions::kMediaRouterCastAllowAllIPsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastAllowAllIPsFeature)},
    {"cast-media-route-provider",
     flag_descriptions::kCastMediaRouteProviderName,
     flag_descriptions::kCastMediaRouteProviderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastMediaRouteProvider)},
#endif  // !OS_ANDROID
#if defined(OS_ANDROID)
    {"autofill-keyboard-accessory-view",
     flag_descriptions::kAutofillAccessoryViewName,
     flag_descriptions::kAutofillAccessoryViewDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillKeyboardAccessory,
         kAutofillKeyboardAccessoryFeatureVariations,
         "AutofillKeyboardAccessoryVariations")},
#endif  // OS_ANDROID
#if defined(OS_WIN)
    {"try-supported-channel-layouts",
     flag_descriptions::kTrySupportedChannelLayoutsName,
     flag_descriptions::kTrySupportedChannelLayoutsDescription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kTrySupportedChannelLayouts)},
#endif  // OS_WIN
#if defined(OS_MACOSX)
    {"mac-syscall-sandbox", flag_descriptions::kMacSyscallSandboxName,
     flag_descriptions::kMacSyscallSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSyscallSandbox)},
    {"mac-v2-gpu-sandbox", flag_descriptions::kMacV2GPUSandboxName,
     flag_descriptions::kMacV2GPUSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacV2GPUSandbox)},
#endif  // OS_MACOSX
#if BUILDFLAG(ENABLE_VR)
    {"webxr-incubations", flag_descriptions::kWebXrIncubationsName,
     flag_descriptions::kWebXrIncubationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrIncubations)},
    {"webxr-runtime", flag_descriptions::kWebXrForceRuntimeName,
     flag_descriptions::kWebXrForceRuntimeDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kWebXrForceRuntimeChoices)},
#endif  // ENABLE_VR
#if defined(OS_CHROMEOS)
    {"disable-accelerated-mjpeg-decode",
     flag_descriptions::kAcceleratedMjpegDecodeName,
     flag_descriptions::kAcceleratedMjpegDecodeDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // OS_CHROMEOS
    {"system-keyboard-lock", flag_descriptions::kSystemKeyboardLockName,
     flag_descriptions::kSystemKeyboardLockDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemKeyboardLock)},
#if defined(OS_ANDROID)
    {"offline-pages-load-signal-collecting",
     flag_descriptions::kOfflinePagesLoadSignalCollectingName,
     flag_descriptions::kOfflinePagesLoadSignalCollectingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesLoadSignalCollectingFeature)},
    {"offline-pages-live-page-sharing",
     flag_descriptions::kOfflinePagesLivePageSharingName,
     flag_descriptions::kOfflinePagesLivePageSharingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesLivePageSharingFeature)},
    {"offline-pages-prefetching",
     flag_descriptions::kOfflinePagesPrefetchingName,
     flag_descriptions::kOfflinePagesPrefetchingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kPrefetchingOfflinePagesFeature)},
    {"offline-pages-failed-download",
     flag_descriptions::kOfflinePagesDescriptiveFailStatusName,
     flag_descriptions::kOfflinePagesDescriptiveFailStatusDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesDescriptiveFailStatusFeature)},
    {"offline-pages-pending-download",
     flag_descriptions::kOfflinePagesDescriptivePendingStatusName,
     flag_descriptions::kOfflinePagesDescriptivePendingStatusDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesDescriptivePendingStatusFeature)},
    {"offline-pages-resource-based-snapshot",
     flag_descriptions::kOfflinePagesResourceBasedSnapshotName,
     flag_descriptions::kOfflinePagesResourceBasedSnapshotDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesResourceBasedSnapshotFeature)},
    {"offline-pages-renovations",
     flag_descriptions::kOfflinePagesRenovationsName,
     flag_descriptions::kOfflinePagesRenovationsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesRenovationsFeature)},
    {"offline-pages-in-downloads-home-open-in-cct",
     flag_descriptions::kOfflinePagesInDownloadHomeOpenInCctName,
     flag_descriptions::kOfflinePagesInDownloadHomeOpenInCctDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesInDownloadHomeOpenInCctFeature)},
    {"offline-pages-alternate-dino-page",
     flag_descriptions::kOfflinePagesShowAlternateDinoPageName,
     flag_descriptions::kOfflinePagesShowAlternateDinoPageDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesShowAlternateDinoPageFeature)},
    {"offline-indicator-choice", flag_descriptions::kOfflineIndicatorChoiceName,
     flag_descriptions::kOfflineIndicatorChoiceDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(offline_pages::kOfflineIndicatorFeature,
                                    kOfflineIndicatorFeatureVariations,
                                    "OfflineIndicator")},
    {"offline-indicator-always-http-probe",
     flag_descriptions::kOfflineIndicatorAlwaysHttpProbeName,
     flag_descriptions::kOfflineIndicatorAlwaysHttpProbeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflineIndicatorAlwaysHttpProbeFeature)},
    {"content-indexing-download-home",
     flag_descriptions::kContentIndexingDownloadHomeName,
     flag_descriptions::kContentIndexingDownloadHomeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContentIndexingDownloadHome)},
    {"content-indexing-ntp", flag_descriptions::kContentIndexingNTPName,
     flag_descriptions::kContentIndexingNTPDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContentIndexingNTP)},
    {"offline-indicator-v2", flag_descriptions::kOfflineIndicatorV2Name,
     flag_descriptions::kOfflineIndicatorV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOfflineIndicatorV2)},
    {"on-the-fly-mhtml-hash-computation",
     flag_descriptions::kOnTheFlyMhtmlHashComputationName,
     flag_descriptions::kOnTheFlyMhtmlHashComputationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOnTheFlyMhtmlHashComputationFeature)},
    {"query-tiles", flag_descriptions::kQueryTilesName,
     flag_descriptions::kQueryTilesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTiles)},
    {"query-tiles-omnibox", flag_descriptions::kQueryTilesOmniboxName,
     flag_descriptions::kQueryTilesOmniboxDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesInOmnibox)},
    {"query-tiles-single-tier", flag_descriptions::kQueryTilesSingleTierName,
     flag_descriptions::kQueryTilesSingleTierDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesSingleTier)},
    {"query-tiles-enable-query-editing",
     flag_descriptions::kQueryTilesEnableQueryEditingName,
     flag_descriptions::kQueryTilesEnableQueryEditingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesEnableQueryEditing)},
    {"query-tiles-country-code", flag_descriptions::kQueryTilesCountryCode,
     flag_descriptions::kQueryTilesCountryCodeDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kQueryTilesCountryChoices)},
    {"query-tiles-instant-fetch",
     flag_descriptions::kQueryTilesInstantFetchName,
     flag_descriptions::kQueryTilesInstantFetchDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(
         query_tiles::switches::kQueryTilesInstantBackgroundTask)},
    {"android-picture-in-picture-api",
     flag_descriptions::kAndroidPictureInPictureAPIName,
     flag_descriptions::kAndroidPictureInPictureAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(media::kPictureInPictureAPI)},
    {"reengagement-notification",
     flag_descriptions::kReengagementNotificationName,
     flag_descriptions::kReengagementNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReengagementNotification)},
#endif  // OS_ANDROID
    {"disallow-doc-written-script-loads",
     flag_descriptions::kDisallowDocWrittenScriptsUiName,
     flag_descriptions::kDisallowDocWrittenScriptsUiDescription, kOsAll,
     // NOTE: if we want to add additional experiment entries for other
     // features controlled by kBlinkSettings, we'll need to add logic to
     // merge the flag values.
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=true",
         switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=false")},
#if defined(OS_WIN)
    {"use-winrt-midi-api", flag_descriptions::kUseWinrtMidiApiName,
     flag_descriptions::kUseWinrtMidiApiDescription, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
#endif  // OS_WIN
#if defined(OS_CHROMEOS)
    {"cros-regions-mode", flag_descriptions::kCrosRegionsModeName,
     flag_descriptions::kCrosRegionsModeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kCrosRegionsModeChoices)},
#endif  // OS_CHROMEOS
#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
#endif  // TOOLKIT_VIEWS || OS_ANDROID
    {"force-ui-direction", flag_descriptions::kForceUiDirectionName,
     flag_descriptions::kForceUiDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceUIDirectionChoices)},
    {"force-text-direction", flag_descriptions::kForceTextDirectionName,
     flag_descriptions::kForceTextDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceTextDirectionChoices)},
#if defined(OS_ANDROID)
    {"force-update-menu-type", flag_descriptions::kUpdateMenuTypeName,
     flag_descriptions::kUpdateMenuTypeDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kForceUpdateMenuTypeChoices)},
    {"enable-inline-update-flow", flag_descriptions::kInlineUpdateFlowName,
     flag_descriptions::kInlineUpdateFlowDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kInlineUpdateFlow)},
    {"update-menu-item-custom-summary",
     flag_descriptions::kUpdateMenuItemCustomSummaryName,
     flag_descriptions::kUpdateMenuItemCustomSummaryDescription, kOsAndroid,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kForceShowUpdateMenuItemCustomSummary,
         "Custom Summary")},
    {"force-show-update-menu-badge", flag_descriptions::kUpdateMenuBadgeName,
     flag_descriptions::kUpdateMenuBadgeDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kForceShowUpdateMenuBadge)},
    {"set-market-url-for-testing",
     flag_descriptions::kSetMarketUrlForTestingName,
     flag_descriptions::kSetMarketUrlForTestingDescription, kOsAndroid,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kMarketUrlForTesting,
                                 "https://play.google.com/store/apps/"
                                 "details?id=com.android.chrome")},
#endif  // OS_ANDROID
    {"tls13-hardening-for-local-anchors",
     flag_descriptions::kTLS13HardeningForLocalAnchorsName,
     flag_descriptions::kTLS13HardeningForLocalAnchorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kTLS13HardeningForLocalAnchors)},
    {"enable-tls13-early-data", flag_descriptions::kEnableTLS13EarlyDataName,
     flag_descriptions::kEnableTLS13EarlyDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnableTLS13EarlyData)},
    {"post-quantum-cecpq2", flag_descriptions::kPostQuantumCECPQ2Name,
     flag_descriptions::kPostQuantumCECPQ2Description, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kPostQuantumCECPQ2)},
#if defined(OS_ANDROID)
    {"interest-feed-content-suggestions",
     flag_descriptions::kInterestFeedContentSuggestionsName,
     flag_descriptions::kInterestFeedContentSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(feed::kInterestFeedContentSuggestions,
                                    kInterestFeedFeatureVariations,
                                    "InterestFeedContentSuggestions")},
    {"interest-feed-feedback", flag_descriptions::kInterestFeedFeedbackName,
     flag_descriptions::kInterestFeedFeedbackDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedFeedback)},
    {"interest-feed-notifications",
     flag_descriptions::kInterestFeedNotificationsName,
     flag_descriptions::kInterestFeedNotificationsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedNotifications)},
    {"interest-feed-v2", flag_descriptions::kInterestFeedV2Name,
     flag_descriptions::kInterestFeedV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2)},
    {"report-feed-user-actions", flag_descriptions::kReportFeedUserActionsName,
     flag_descriptions::kReportFeedUserActionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kReportFeedUserActions)},
    {"offlining-recent-pages", flag_descriptions::kOffliningRecentPagesName,
     flag_descriptions::kOffliningRecentPagesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOffliningRecentPagesFeature)},
    {"offline-pages-ct", flag_descriptions::kOfflinePagesCtName,
     flag_descriptions::kOfflinePagesCtDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesCTFeature)},
    {"offline-pages-ct-v2", flag_descriptions::kOfflinePagesCtV2Name,
     flag_descriptions::kOfflinePagesCtV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesCTV2Feature)},
    {"offline-pages-ct-suppress-completed-notification",
     flag_descriptions::kOfflinePagesCTSuppressNotificationsName,
     flag_descriptions::kOfflinePagesCTSuppressNotificationsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesCTSuppressNotificationsFeature)},
#endif  // OS_ANDROID
    {"PasswordImport", flag_descriptions::kPasswordImportName,
     flag_descriptions::kPasswordImportDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordImport)},
#if defined(OS_ANDROID)
    {"password-editing-android", flag_descriptions::kPasswordEditingAndroidName,
     flag_descriptions::kPasswordEditingAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordEditingAndroid)},
#endif  // OS_ANDROID
#if !defined(OS_CHROMEOS)
    // TODO(https://crbug.com/1011696): Investigate crash reports and re-enable
    // for ChromeOS.
    {"enable-force-dark", flag_descriptions::kForceWebContentsDarkModeName,
     flag_descriptions::kForceWebContentsDarkModeDescription,
     kOsWin | kOsLinux | kOsMac | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kForceWebContentsDarkMode,
                                    kForceDarkVariations,
                                    "ForceDarkVariations")},
#endif  // !OS_CHROMEOS
#if defined(OS_ANDROID)
    {"enable-android-dark-search", flag_descriptions::kAndroidDarkSearchName,
     flag_descriptions::kAndroidDarkSearchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidDarkSearch)},

    {"enable-android-night-mode-tab-reparenting",
     flag_descriptions::kAndroidNightModeTabReparentingName,
     flag_descriptions::kAndroidNightModeTabReparentingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidNightModeTabReparenting)},
#endif  // OS_ANDROID
    {"enable-experimental-accessibility-language-detection",
     flag_descriptions::kExperimentalAccessibilityLanguageDetectionName,
     flag_descriptions::kExperimentalAccessibilityLanguageDetectionDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityLanguageDetection)},
    {"enable-experimental-accessibility-language-detection-dynamic",
     flag_descriptions::kExperimentalAccessibilityLanguageDetectionDynamicName,
     flag_descriptions::
         kExperimentalAccessibilityLanguageDetectionDynamicDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic)},
#if defined(OS_CHROMEOS)
    {"enable-encryption-migration",
     flag_descriptions::kEnableEncryptionMigrationName,
     flag_descriptions::kEnableEncryptionMigrationDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnableEncryptionMigration,
         chromeos::switches::kDisableEncryptionMigration)},
    {"enable-cros-ime-assist-autocorrect",
     flag_descriptions::kImeAssistAutocorrectName,
     flag_descriptions::kImeAssistAutocorrectDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistAutoCorrect)},
    {"enable-cros-ime-assist-personal-info",
     flag_descriptions::kImeAssistPersonalInfoName,
     flag_descriptions::kImeAssistPersonalInfoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistPersonalInfo)},
    {"enable-cros-ime-emoji-suggest-addition",
     flag_descriptions::kImeEmojiSuggestAdditionName,
     flag_descriptions::kImeEmojiSuggestAdditionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEmojiSuggestAddition)},
    {"enable-cros-ime-input-logic-fst",
     flag_descriptions::kImeInputLogicFstName,
     flag_descriptions::kImeInputLogicFstDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeInputLogicFst)},
    {"enable-cros-ime-input-logic-hmm",
     flag_descriptions::kImeInputLogicHmmName,
     flag_descriptions::kImeInputLogicHmmDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeInputLogicHmm)},
    {"enable-cros-ime-input-logic-mozc",
     flag_descriptions::kImeInputLogicMozcName,
     flag_descriptions::kImeInputLogicMozcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeInputLogicMozc)},
    {"enable-cros-ime-mozc-proto", flag_descriptions::kImeMozcProtoName,
     flag_descriptions::kImeMozcProtoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeMozcProto)},
    {"enable-cros-ime-native-decoder", flag_descriptions::kImeNativeDecoderName,
     flag_descriptions::kImeNativeDecoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeDecoderWithSandbox)},
    {"enable-cros-virtual-keyboard-bordered-key",
     flag_descriptions::kVirtualKeyboardBorderedKeyName,
     flag_descriptions::kVirtualKeyboardBorderedKeyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardBorderedKey)},
    {"enable-cros-virtual-keyboard-floating-resizable",
     flag_descriptions::kVirtualKeyboardFloatingResizableName,
     flag_descriptions::kVirtualKeyboardFloatingResizableDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardFloatingResizable)},
    {"enable-experimental-accessibility-switch-access",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccess)},
    {"enable-experimental-accessibility-switch-access-text",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessText)},
    {"enable-experimental-accessibility-chromevox-annotations",
     flag_descriptions::kExperimentalAccessibilityChromeVoxAnnotationsName,
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxAnnotationsDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityChromeVoxAnnotations)},
    {"disable-experimental-accessibility-chromevox-language-switching",
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxLanguageSwitchingName,
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxLanguageSwitchingDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::
             kDisableExperimentalAccessibilityChromeVoxLanguageSwitching)},
    {"enable-experimental-kernel-vm-support",
     flag_descriptions::kKernelnextVMsName,
     flag_descriptions::kKernelnextVMsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kKernelnextVMs)},
    {"disable-experimental-accessibility-chromevox-search-menus",
     flag_descriptions::kExperimentalAccessibilityChromeVoxSearchMenusName,
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxSearchMenusDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kDisableExperimentalAccessibilityChromeVoxSearchMenus)},
    {"enable-experimental-accessibility-chromevox-tutorial",
     flag_descriptions::kExperimentalAccessibilityChromeVoxTutorialName,
     flag_descriptions::kExperimentalAccessibilityChromeVoxTutorialDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityChromeVoxTutorial)},
#endif  // OS_CHROMEOS
#if defined(OS_MACOSX)
    {"enable-immersive-fullscreen-toolbar",
     flag_descriptions::kImmersiveFullscreenName,
     flag_descriptions::kImmersiveFullscreenDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kImmersiveFullscreen)},
#endif  // OS_MACOSX
    {"rewrite-leveldb-on-deletion",
     flag_descriptions::kRewriteLevelDBOnDeletionName,
     flag_descriptions::kRewriteLevelDBOnDeletionDescription, kOsAll,
     FEATURE_VALUE_TYPE(leveldb::kLevelDBRewriteFeature)},
    {"passive-listener-default",
     flag_descriptions::kPassiveEventListenerDefaultName,
     flag_descriptions::kPassiveEventListenerDefaultDescription, kOsAll,
     MULTI_VALUE_TYPE(kPassiveListenersChoices)},
    {"enable-experimental-fling-animation",
     flag_descriptions::kExperimentalFlingAnimationName,
     flag_descriptions::kExperimentalFlingAnimationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExperimentalFlingAnimation)},
    {"per-method-can-make-payment-quota",
     flag_descriptions::kPerMethodCanMakePaymentQuotaName,
     flag_descriptions::kPerMethodCanMakePaymentQuotaDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         payments::features::kWebPaymentsPerMethodCanMakePaymentQuota)},
    {"enable-web-payments-experimental-features",
     flag_descriptions::kWebPaymentsExperimentalFeaturesName,
     flag_descriptions::kWebPaymentsExperimentalFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsExperimentalFeatures)},
    {"enable-web-payments-minimal-ui",
     flag_descriptions::kWebPaymentsMinimalUIName,
     flag_descriptions::kWebPaymentsMinimalUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebPaymentsMinimalUI)},
    {"fill-on-account-select", flag_descriptions::kFillOnAccountSelectName,
     flag_descriptions::kFillOnAccountSelectDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
#if defined(OS_CHROMEOS)
    {"arc-custom-tabs-experiment",
     flag_descriptions::kArcCustomTabsExperimentName,
     flag_descriptions::kArcCustomTabsExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kCustomTabsExperimentFeature)},
    {"arc-documents-provider", flag_descriptions::kArcDocumentsProviderName,
     flag_descriptions::kArcDocumentsProviderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableDocumentsProviderInFilesAppFeature)},
    {"arc-file-picker-experiment",
     flag_descriptions::kArcFilePickerExperimentName,
     flag_descriptions::kArcFilePickerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kFilePickerExperimentFeature)},
    {"arc-native-bridge-toggle", flag_descriptions::kArcNativeBridgeToggleName,
     flag_descriptions::kArcNativeBridgeToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeToggleFeature)},
    {"arc-print-spooler-experiment",
     flag_descriptions::kArcPrintSpoolerExperimentName,
     flag_descriptions::kArcPrintSpoolerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kPrintSpoolerExperimentFeature)},
    {"arc-usb-host", flag_descriptions::kArcUsbHostName,
     flag_descriptions::kArcUsbHostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUsbHostFeature)},
    {"arc-usb-storage-ui", flag_descriptions::kArcUsbStorageUIName,
     flag_descriptions::kArcUsbStorageUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUsbStorageUIFeature)},
#endif  // OS_CHROMEOS
#if defined(OS_WIN)
    {"enable-winrt-sensor-implementation",
     flag_descriptions::kWinrtSensorsImplementationName,
     flag_descriptions::kWinrtSensorsImplementationDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWinrtSensorsImplementation)},
#endif
    {"enable-generic-sensor-extra-classes",
     flag_descriptions::kEnableGenericSensorExtraClassesName,
     flag_descriptions::kEnableGenericSensorExtraClassesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGenericSensorExtraClasses)},
    {"expensive-background-timer-throttling",
     flag_descriptions::kExpensiveBackgroundTimerThrottlingName,
     flag_descriptions::kExpensiveBackgroundTimerThrottlingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExpensiveBackgroundTimerThrottling)},

#if defined(OS_CHROMEOS)
    {ui_devtools::switches::kEnableUiDevTools,
     flag_descriptions::kUiDevToolsName,
     flag_descriptions::kUiDevToolsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ui_devtools::switches::kEnableUiDevTools)},
#endif  // defined(OS_CHROMEOS)

    {"enable-autofill-credit-card-ablation-experiment",
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDisplayName,
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillCreditCardAblationExperiment)},
    {"enable-autofill-credit-card-upload-editable-expiration-date",
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEditableExpirationDate)},

#if defined(OS_ANDROID)
    {"enable-autofill-manual-fallback",
     flag_descriptions::kAutofillManualFallbackAndroidName,
     flag_descriptions::kAutofillManualFallbackAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillManualFallbackAndroid)},

    {"enable-autofill-refresh-style",
     flag_descriptions::kEnableAutofillRefreshStyleName,
     flag_descriptions::kEnableAutofillRefreshStyleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRefreshStyleAndroid)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-touchscreen-calibration",
     flag_descriptions::kTouchscreenCalibrationName,
     flag_descriptions::kTouchscreenCalibrationDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchCalibrationSetting)},
#endif  // defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
    {"android-files-in-files-app",
     flag_descriptions::kShowAndroidFilesInFilesAppName,
     flag_descriptions::kShowAndroidFilesInFilesAppDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kShowAndroidFilesInFilesApp,
         chromeos::switches::kHideAndroidFilesInFilesApp)},
    {"camera-system-web-app", flag_descriptions::kCameraSystemWebAppName,
     flag_descriptions::kCameraSystemWebAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCameraSystemWebApp)},
    {"crostini-gpu-support", flag_descriptions::kCrostiniGpuSupportName,
     flag_descriptions::kCrostiniGpuSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniGpuSupport)},
    {"crostini-usb-allow-unsupported",
     flag_descriptions::kCrostiniUsbAllowUnsupportedName,
     flag_descriptions::kCrostiniUsbAllowUnsupportedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUsbAllowUnsupported)},
    {"files-ng", flag_descriptions::kFilesNGName,
     flag_descriptions::kFilesNGDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesNG)},
    {"files-zip-no-nacl", flag_descriptions::kFilesZipNoNaClName,
     flag_descriptions::kFilesZipNoNaClDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesZipNoNaCl)},
    {"files-unified-media-view", flag_descriptions::kUnifiedMediaViewName,
     flag_descriptions::kUnifiedMediaViewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUnifiedMediaView)},
    {"rar2fs", flag_descriptions::kRar2FsName,
     flag_descriptions::kRar2FsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kRar2Fs)},
    {"smbfs-file-shares", flag_descriptions::kSmbfsFileSharesName,
     flag_descriptions::kSmbfsFileSharesName, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSmbFs)},
#endif  // OS_CHROMEOS

#if defined(OS_WIN)
    {"gdi-text-printing", flag_descriptions::kGdiTextPrinting,
     flag_descriptions::kGdiTextPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kGdiTextPrinting)},
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
    {"new-usb-backend", flag_descriptions::kNewUsbBackendName,
     flag_descriptions::kNewUsbBackendDescription, kOsWin,
     FEATURE_VALUE_TYPE(device::kNewUsbBackend)},
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
    {"omnibox-adaptive-suggestions-count",
     flag_descriptions::kOmniboxAdaptiveSuggestionsCountName,
     flag_descriptions::kOmniboxAdaptiveSuggestionsCountDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kAdaptiveSuggestionsCount)},
    {"omnibox-assistant-voice-search",
     flag_descriptions::kOmniboxAssistantVoiceSearchName,
     flag_descriptions::kOmniboxAssistantVoiceSearchDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxAssistantVoiceSearch,
                                    kOmniboxAssistantVoiceSearchVariations,
                                    "OmniboxAssistantVoiceSearch")},
    {"omnibox-compact-suggestions",
     flag_descriptions::kOmniboxCompactSuggestionsName,
     flag_descriptions::kOmniboxCompactSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kCompactSuggestions,
                                    kCompactSuggestionsVariations,
                                    "OmniboxCompactSuggestions")},
    {"omnibox-deferred-keyboard-popup",
     flag_descriptions::kOmniboxDeferredKeyboardPopupName,
     flag_descriptions::kOmniboxDeferredKeyboardPopupDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kDeferredKeyboardPopup)},
    {"omnibox-search-engine-logo",
     flag_descriptions::kOmniboxSearchEngineLogoName,
     flag_descriptions::kOmniboxSearchEngineLogoDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxSearchEngineLogo,
                                    kOmniboxSearchEngineLogoFeatureVariations,
                                    "OmniboxSearchEngineLogo")},
    {"omnibox-suggestions-recycler-view",
     flag_descriptions::kOmniboxSuggestionsRecyclerViewName,
     flag_descriptions::kOmniboxSuggestionsRecyclerViewDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxSuggestionsRecyclerView)},
#endif  // defined(OS_ANDROID)

    {"omnibox-on-device-head-suggestions-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnDeviceHeadProviderIncognito,
         kOmniboxOnDeviceHeadSuggestIncognitoExperimentVariations,
         "OmniboxOnDeviceHeadSuggestIncognito")},

    {"omnibox-on-device-head-suggestions-non-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnDeviceHeadProviderNonIncognito,
         kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations,
         "OmniboxOnDeviceHeadSuggestNonIncognito")},

    {"omnibox-on-focus-suggestions",
     flag_descriptions::kOmniboxOnFocusSuggestionsName,
     flag_descriptions::kOmniboxOnFocusSuggestionsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOnFocusSuggestions,
                                    kOmniboxOnFocusSuggestionsVariations,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-on-focus-suggestions-contextual-web",
     flag_descriptions::kOmniboxOnFocusSuggestionsContextualWebName,
     flag_descriptions::kOmniboxOnFocusSuggestionsContextualWebDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kOnFocusSuggestionsContextualWeb)},

    {"omnibox-preserve-default-match-against-async-update",
     flag_descriptions::kOmniboxPreserveDefaultMatchAgainstAsyncUpdateName,
     flag_descriptions::
         kOmniboxPreserveDefaultMatchAgainstAsyncUpdateDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         omnibox::kOmniboxPreserveDefaultMatchAgainstAsyncUpdate)},

    {"omnibox-local-entity-suggestions",
     flag_descriptions::kOmniboxLocalEntitySuggestionsName,
     flag_descriptions::kOmniboxLocalEntitySuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxLocalEntitySuggestions)},

    {"omnibox-experimental-suggest-scoring",
     flag_descriptions::kOmniboxExperimentalSuggestScoringName,
     flag_descriptions::kOmniboxExperimentalSuggestScoringDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxExperimentalSuggestScoring)},

    {"omnibox-history-quick-provider-allow-but-do-not-score-midword-terms",
     flag_descriptions::
         kOmniboxHistoryQuickProviderAllowButDoNotScoreMidwordTermsName,
     flag_descriptions::
         kOmniboxHistoryQuickProviderAllowButDoNotScoreMidwordTermsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         omnibox::kHistoryQuickProviderAllowButDoNotScoreMidwordTerms)},
    {"omnibox-history-quick-provider-allow-midword-continuations",
     flag_descriptions::
         kOmniboxHistoryQuickProviderAllowMidwordContinuationsName,
     flag_descriptions::
         kOmniboxHistoryQuickProviderAllowMidwordContinuationsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         omnibox::kHistoryQuickProviderAllowMidwordContinuations)},

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    {"omnibox-experimental-keyword-mode",
     flag_descriptions::kOmniboxExperimentalKeywordModeName,
     flag_descriptions::kOmniboxExperimentalKeywordModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kExperimentalKeywordMode)},
    {"omnibox-loose-max-limit-on-dedicated-rows",
     flag_descriptions::kOmniboxLooseMaxLimitOnDedicatedRowsName,
     flag_descriptions::kOmniboxLooseMaxLimitOnDedicatedRowsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxLooseMaxLimitOnDedicatedRows)},
    {"omnibox-reverse-answers", flag_descriptions::kOmniboxReverseAnswersName,
     flag_descriptions::kOmniboxReverseAnswersDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxReverseAnswers)},
    {"omnibox-short-bookmark-suggestions",
     flag_descriptions::kOmniboxShortBookmarkSuggestionsName,
     flag_descriptions::kOmniboxShortBookmarkSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxShortBookmarkSuggestions)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"omnibox-tab-switch-suggestions-dedicated-row",
     flag_descriptions::kOmniboxTabSwitchSuggestionsDedicatedRowName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDedicatedRowDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestionsDedicatedRow)},
    {"omnibox-pedal-suggestions",
     flag_descriptions::kOmniboxPedalSuggestionsName,
     flag_descriptions::kOmniboxPedalSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalSuggestions)},
    {"omnibox-suggestion-button-row",
     flag_descriptions::kOmniboxSuggestionButtonRowName,
     flag_descriptions::kOmniboxSuggestionButtonRowDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxSuggestionButtonRow)},
    {"omnibox-suggestion-transparency-options",
     flag_descriptions::kOmniboxSuggestionTransparencyOptionsName,
     flag_descriptions::kOmniboxSuggestionTransparencyOptionsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxSuggestionTransparencyOptions)},
    {"omnibox-drive-suggestions",
     flag_descriptions::kOmniboxDriveSuggestionsName,
     flag_descriptions::kOmniboxDriveSuggestionsDescriptions, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kDocumentProvider,
                                    kOmniboxDocumentProviderVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-autocomplete-titles",
     flag_descriptions::kOmniboxAutocompleteTitlesName,
     flag_descriptions::kOmniboxAutocompleteTitlesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kAutocompleteTitles)},
    {"omnibox-rich-autocompletion",
     flag_descriptions::kOmniboxRichAutocompletionName,
     flag_descriptions::kOmniboxRichAutocompletionDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kOmniboxRichAutocompletionVariations,
                                    "OmniboxBundledExperimentV1")},
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

    {"enable-speculative-service-worker-start-on-query-input",
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputName,
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kSpeculativeServiceWorkerStartOnQueryInput)},

    {"enable-service-worker-on-ui", flag_descriptions::kServiceWorkerOnUIName,
     flag_descriptions::kServiceWorkerOnUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kServiceWorkerOnUI)},

#if defined(OS_CHROMEOS)
    {"scheduler-configuration", flag_descriptions::kSchedulerConfigurationName,
     flag_descriptions::kSchedulerConfigurationDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kSchedulerConfigurationChoices)},
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
    {"enable-command-line-on-non-rooted-devices",
     flag_descriptions::kEnableCommandLineOnNonRootedName,
     flag_descriptions::kEnableCommandLineOnNoRootedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCommandLineOnNonRooted)},
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
    {"enable-revamped-context-menu",
     flag_descriptions::kEnableRevampedContextMenuName,
     flag_descriptions::kEnableRevampedContextMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kRevampedContextMenu)},
#endif  // OS_ANDROID

    {"omnibox-display-title-for-current-url",
     flag_descriptions::kOmniboxDisplayTitleForCurrentUrlName,
     flag_descriptions::kOmniboxDisplayTitleForCurrentUrlDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kDisplayTitleForCurrentUrl)},

    {"force-color-profile", flag_descriptions::kForceColorProfileName,
     flag_descriptions::kForceColorProfileDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceColorProfileChoices)},

    {"force-effective-connection-type",
     flag_descriptions::kForceEffectiveConnectionTypeName,
     flag_descriptions::kForceEffectiveConnectionTypeDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceEffectiveConnectionTypeChoices)},

    {"forced-colors", flag_descriptions::kForcedColorsName,
     flag_descriptions::kForcedColorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kForcedColors)},

#if defined(OS_ANDROID)
    {"dynamic-color-gamut", flag_descriptions::kDynamicColorGamutName,
     flag_descriptions::kDynamicColorGamutDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDynamicColorGamut)},
#endif

    {"memlog", flag_descriptions::kMemlogName,
     flag_descriptions::kMemlogDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogModeChoices)},

    {"memlog-sampling-rate", flag_descriptions::kMemlogSamplingRateName,
     flag_descriptions::kMemlogSamplingRateDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogSamplingRateChoices)},

    {"memlog-stack-mode", flag_descriptions::kMemlogStackModeName,
     flag_descriptions::kMemlogStackModeDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogStackModeChoices)},

    {"omnibox-ui-hide-steady-state-url-scheme",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlSchemeName,
     flag_descriptions::kOmniboxUIHideSteadyStateUrlSchemeDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kHideSteadyStateUrlScheme)},

    {"omnibox-ui-hide-steady-state-url-trivial-subdomains",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName,
     flag_descriptions::
         kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kHideSteadyStateUrlTrivialSubdomains)},

    {"omnibox-ui-hide-steady-state-url-path-query-and-ref",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlPathQueryAndRefName,
     flag_descriptions::kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kHideSteadyStateUrlPathQueryAndRef)},

    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxBundledExperimentV1")},

    {"omnibox-max-url-matches", flag_descriptions::kOmniboxMaxURLMatchesName,
     flag_descriptions::kOmniboxMaxURLMatchesDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxMaxURLMatches,
                                    kOmniboxMaxURLMatchesVariations,
                                    "OmniboxMaxURLMatchesVariations")},

    {"omnibox-ui-swap-title-and-url",
     flag_descriptions::kOmniboxUISwapTitleAndUrlName,
     flag_descriptions::kOmniboxUISwapTitleAndUrlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentSwapTitleAndUrl)},

    {"omnibox-zero-suggestions-on-ntp",
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPName,
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestionsOnNTP)},

    {"omnibox-zero-suggestions-on-ntp-realbox",
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPRealboxName,
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPRealboxDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(omnibox::kZeroSuggestionsOnNTPRealbox)},

    {"omnibox-zero-suggestions-on-serp",
     flag_descriptions::kOmniboxZeroSuggestionsOnSERPName,
     flag_descriptions::kOmniboxZeroSuggestionsOnSERPDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestionsOnSERP)},

    {"omnibox-disable-instant-extended-limit",
     flag_descriptions::kOmniboxDisableInstantExtendedLimitName,
     flag_descriptions::kOmniboxDisableInstantExtendedLimitDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxDisableInstantExtendedLimit)},

    {"use-new-accept-language-header",
     flag_descriptions::kUseNewAcceptLanguageHeaderName,
     flag_descriptions::kUseNewAcceptLanguageHeaderDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUseNewAcceptLanguageHeader)},

#if defined(OS_CHROMEOS)
    {"handwriting-gesture", flag_descriptions::kHandwritingGestureName,
     flag_descriptions::kHandwritingGestureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kHandwritingGesture)},
#endif  // OS_CHROMEOS

    {"network-service-in-process",
     flag_descriptions::kEnableNetworkServiceInProcessName,
     flag_descriptions::kEnableNetworkServiceInProcessDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kNetworkServiceInProcess)},

    {"cors-for-content-scripts", flag_descriptions::kCorsForContentScriptsName,
     flag_descriptions::kCorsForContentScriptsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(network::features::kCorbAllowlistAlsoAppliesToOorCors)},

    {"cross-origin-opener-policy",
     flag_descriptions::kCrossOriginOpenerPolicyName,
     flag_descriptions::kCrossOriginOpenerPolicyDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kCrossOriginOpenerPolicy)},

    {"cross-origin-opener-policy-reporting",
     flag_descriptions::kCrossOriginOpenerPolicyReportingName,
     flag_descriptions::kCrossOriginOpenerPolicyReportingDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kCrossOriginOpenerPolicyReporting)},

    {"disable-keepalive-fetch", flag_descriptions::kDisableKeepaliveFetchName,
     flag_descriptions::kDisableKeepaliveFetchDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kDisableKeepaliveFetch)},

    {"prefetch-privacy-changes", flag_descriptions::kPrefetchPrivacyChangesName,
     flag_descriptions::kPrefetchPrivacyChangesDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPrefetchPrivacyChanges)},

#if defined(OS_ANDROID)
    {"omnibox-spare-renderer", flag_descriptions::kOmniboxSpareRendererName,
     flag_descriptions::kOmniboxSpareRendererDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOmniboxSpareRenderer)},
#endif

#if defined(OS_CHROMEOS)
    {"double-tap-to-zoom-in-tablet-mode",
     flag_descriptions::kDoubleTapToZoomInTabletModeName,
     flag_descriptions::kDoubleTapToZoomInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDoubleTapToZoomInTabletMode)},
#endif  // defined(OS_CHROMEOS)

    {"tab-groups", flag_descriptions::kTabGroupsName,
     flag_descriptions::kTabGroupsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroups)},

    {"tab-groups-collapse", flag_descriptions::kTabGroupsCollapseName,
     flag_descriptions::kTabGroupsCollapseDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsCollapse)},

    {"tab-groups-feedback", flag_descriptions::kTabGroupsFeedbackName,
     flag_descriptions::kTabGroupsFeedbackDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsFeedback)},

    {"new-tabstrip-animation", flag_descriptions::kNewTabstripAnimationName,
     flag_descriptions::kNewTabstripAnimationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNewTabstripAnimation)},

    {"scrollable-tabstrip", flag_descriptions::kScrollableTabStripName,
     flag_descriptions::kScrollableTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kScrollableTabStrip)},

    {"tab-outlines-in-low-contrast-themes",
     flag_descriptions::kTabOutlinesInLowContrastThemesName,
     flag_descriptions::kTabOutlinesInLowContrastThemesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabOutlinesInLowContrastThemes)},

    {"prominent-dark-mode-active-tab-title",
     flag_descriptions::kProminentDarkModeActiveTabTitleName,
     flag_descriptions::kProminentDarkModeActiveTabTitleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kProminentDarkModeActiveTabTitle)},

#if defined(OS_ANDROID)
    {"enable-reader-mode-in-cct", flag_descriptions::kReaderModeInCCTName,
     flag_descriptions::kReaderModeInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReaderModeInCCT)},
#endif  // !defined(OS_ANDROID)

    {"click-to-open-pdf", flag_descriptions::kClickToOpenPDFName,
     flag_descriptions::kClickToOpenPDFDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kClickToOpenPDFPlaceholder)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"direct-manipulation-stylus",
     flag_descriptions::kDirectManipulationStylusName,
     flag_descriptions::kDirectManipulationStylusDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(features::kDirectManipulationStylus)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if !defined(OS_ANDROID)
    {"ntp-confirm-suggestion-removals",
     flag_descriptions::kNtpConfirmSuggestionRemovalsName,
     flag_descriptions::kNtpConfirmSuggestionRemovalsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kConfirmSuggestionRemovals)},

    {"ntp-dismiss-promos", flag_descriptions::kNtpDismissPromosName,
     flag_descriptions::kNtpDismissPromosDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kDismissPromos)},

    {"ntp-iframe-one-google-bar", flag_descriptions::kNtpIframeOneGoogleBarName,
     flag_descriptions::kNtpIframeOneGoogleBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kIframeOneGoogleBar)},

    {"ntp-realbox", flag_descriptions::kNtpRealboxName,
     flag_descriptions::kNtpRealboxDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealbox)},

    {"ntp-realbox-match-omnibox-theme",
     flag_descriptions::kNtpRealboxMatchOmniboxThemeName,
     flag_descriptions::kNtpRealboxMatchOmniboxThemeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealboxMatchOmniboxTheme)},

    {"ntp-webui", flag_descriptions::kNtpWebUIName,
     flag_descriptions::kNtpWebUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kWebUI)},

    {"webui-a11y-enhancements", flag_descriptions::kWebUIA11yEnhancementsName,
     flag_descriptions::kWebUIA11yEnhancementsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUIA11yEnhancements)},
#endif  // !defined(OS_ANDROID)

#if defined(DCHECK_IS_CONFIGURABLE)
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, kOsWin,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // defined(DCHECK_IS_CONFIGURABLE)

    {"enable-pixel-canvas-recording",
     flag_descriptions::kEnablePixelCanvasRecordingName,
     flag_descriptions::kEnablePixelCanvasRecordingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnablePixelCanvasRecording)},

    {"enable-parallel-downloading", flag_descriptions::kParallelDownloadingName,
     flag_descriptions::kParallelDownloadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(download::features::kParallelDownloading)},

    {"enable-pointer-lock-options", flag_descriptions::kPointerLockOptionsName,
     flag_descriptions::kPointerLockOptionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPointerLockOptions)},

#if defined(OS_ANDROID)
    {"enable-async-dns", flag_descriptions::kAsyncDnsName,
     flag_descriptions::kAsyncDnsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAsyncDns)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"download-auto-resumption-native",
     flag_descriptions::kDownloadAutoResumptionNativeName,
     flag_descriptions::kDownloadAutoResumptionNativeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kDownloadAutoResumptionNative)},

    {"download-later", flag_descriptions::kDownloadLaterName,
     flag_descriptions::kDownloadLaterDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kDownloadLater)},
#endif

    {"enable-new-download-backend",
     flag_descriptions::kEnableNewDownloadBackendName,
     flag_descriptions::kEnableNewDownloadBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         download::features::kUseDownloadOfflineContentProvider)},

#if defined(OS_ANDROID)
    {"update-notification-scheduling-integration",
     flag_descriptions::kUpdateNotificationSchedulingIntegrationName,
     flag_descriptions::kUpdateNotificationSchedulingIntegrationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kUpdateNotificationSchedulingIntegration)},
#endif

#if defined(OS_ANDROID)
    {"prefetch-notification-scheduling-integration",
     flag_descriptions::kPrefetchNotificationSchedulingIntegrationName,
     flag_descriptions::kPrefetchNotificationSchedulingIntegrationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kPrefetchNotificationSchedulingIntegration)},
#endif

#if defined(OS_ANDROID)
    {"update-notification-scheduling-show-immediately",
     flag_descriptions::kUpdateNotificationServiceImmediateShowOptionName,
     flag_descriptions::
         kUpdateNotificationServiceImmediateShowOptionDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::
             kUpdateNotificationScheduleServiceImmediateShowOption)},
#endif

    {"tab-hover-cards", flag_descriptions::kTabHoverCardsName,
     flag_descriptions::kTabHoverCardsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kTabHoverCards,
                                    kTabHoverCardsFeatureVariations,
                                    "TabHoverCards")},

    {"tab-hover-card-images", flag_descriptions::kTabHoverCardImagesName,
     flag_descriptions::kTabHoverCardImagesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabHoverCardImages)},

    {"stop-non-timers-in-background",
     flag_descriptions::kStopNonTimersInBackgroundName,
     flag_descriptions::kStopNonTimersInBackgroundDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kStopNonTimersInBackground)},

    {"stop-in-background", flag_descriptions::kStopInBackgroundName,
     flag_descriptions::kStopInBackgroundDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kStopInBackground)},

    {"enable-storage-pressure-ui", flag_descriptions::kStoragePressureUIName,
     flag_descriptions::kStoragePressureUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kStoragePressureUI)},

    {"installed-apps-in-cbd", flag_descriptions::kInstalledAppsInCbdName,
     flag_descriptions::kInstalledAppsInCbdDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kInstalledAppsInCbd)},

    {"enable-network-logging-to-file",
     flag_descriptions::kEnableNetworkLoggingToFileName,
     flag_descriptions::kEnableNetworkLoggingToFileDescription, kOsAll,
     SINGLE_VALUE_TYPE(network::switches::kLogNetLog)},

    {"enable-mark-http-as", flag_descriptions::kMarkHttpAsName,
     flag_descriptions::kMarkHttpAsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         security_state::features::kMarkHttpAsFeature,
         kMarkHttpAsFeatureVariations,
         "HTTPReallyBadFinal")},

    {"enable-web-authentication-cable-v2-support",
     flag_descriptions::kEnableWebAuthenticationCableV2SupportName,
     flag_descriptions::kEnableWebAuthenticationCableV2SupportDescription,
     kOsDesktop | kOsAndroid, FEATURE_VALUE_TYPE(device::kWebAuthPhoneSupport)},

    {"use-preferred-interval-for-video",
     flag_descriptions::kUsePreferredIntervalForVideoName,
     flag_descriptions::kUsePreferredIntervalForVideoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kUsePreferredIntervalForVideo)},

    {"split-partially-occluded-quads",
     flag_descriptions::kSplitPartiallyOccludedQuadsName,
     flag_descriptions::kSplitPartiallyOccludedQuadsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSplitPartiallyOccludedQuads)},

    {"enable-query-in-omnibox", flag_descriptions::kQueryInOmniboxName,
     flag_descriptions::kQueryInOmniboxDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kQueryInOmnibox)},

#if BUILDFLAG(ENABLE_PDF)
#if defined(OS_CHROMEOS)
    {"pdf-annotations", flag_descriptions::kPdfAnnotations,
     flag_descriptions::kPdfAnnotationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPDFAnnotations)},
#endif  // defined(OS_CHROMEOS)

    {"pdf-form-save", flag_descriptions::kPdfFormSaveName,
     flag_descriptions::kPdfFormSaveDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kSaveEditedPDFForm)},

    {"pdf-two-up-view", flag_descriptions::kPdfTwoUpViewName,
     flag_descriptions::kPdfTwoUpViewDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPDFTwoUpView)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINTING)
#if defined(OS_MACOSX)
    {"cups-ipp-printing-backend",
     flag_descriptions::kCupsIppPrintingBackendName,
     flag_descriptions::kCupsIppPrintingBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(printing::features::kCupsIppPrintingBackend)},
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
    {"print-with-reduced-rasterization",
     flag_descriptions::kPrintWithReducedRasterizationName,
     flag_descriptions::kPrintWithReducedRasterizationDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kPrintWithReducedRasterization)},

    {"use-xps-for-printing", flag_descriptions::kUseXpsForPrintingName,
     flag_descriptions::kUseXpsForPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kUseXpsForPrinting)},

    {"use-xps-for-printing-from-pdf",
     flag_descriptions::kUseXpsForPrintingFromPdfName,
     flag_descriptions::kUseXpsForPrintingFromPdfDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kUseXpsForPrintingFromPdf)},
#endif  // defined(OS_WIN)
#endif  // BUILDFLAG(ENABLE_PRINTING)

    {"autofill-profile-client-validation",
     flag_descriptions::kAutofillProfileClientValidationName,
     flag_descriptions::kAutofillProfileClientValidationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillProfileClientValidation)},

    {"autofill-profile-server-validation",
     flag_descriptions::kAutofillProfileServerValidationName,
     flag_descriptions::kAutofillProfileServerValidationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillProfileServerValidation)},

    {"autofill-reject-company-birthyear",
     flag_descriptions::kAutofillRejectCompanyBirthyearName,
     flag_descriptions::kAutofillRejectCompanyBirthyearDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRejectCompanyBirthyear)},

    {"autofill-restrict-formless-form-extraction",
     flag_descriptions::kAutofillRestrictUnownedFieldsToFormlessCheckoutName,
     flag_descriptions::
         kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout)},

#if defined(OS_ANDROID)
    {"enable-start-surface", flag_descriptions::kStartSurfaceAndroidName,
     flag_descriptions::kStartSurfaceAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kStartSurfaceAndroid,
                                    kStartSurfaceAndroidVariations,
                                    "ChromeStart")},

    {"enable-instant-start", flag_descriptions::kInstantStartName,
     flag_descriptions::kInstantStartDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kInstantStart)},

    {"enable-close-tab-suggestions",
     flag_descriptions::kCloseTabSuggestionsName,
     flag_descriptions::kCloseTabSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kCloseTabSuggestions,
                                    kCloseTabSuggestionsStaleVariations,
                                    "CloseSuggestionsTab")},

    {"enable-horizontal-tab-switcher",
     flag_descriptions::kHorizontalTabSwitcherAndroidName,
     flag_descriptions::kHorizontalTabSwitcherAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHorizontalTabSwitcherAndroid)},

    {"enable-tab-grid-layout", flag_descriptions::kTabGridLayoutAndroidName,
     flag_descriptions::kTabGridLayoutAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabGridLayoutAndroid,
                                    kTabGridLayoutAndroidVariations,
                                    "TabGridLayoutAndroid")},

    {"enable-tab-groups", flag_descriptions::kTabGroupsAndroidName,
     flag_descriptions::kTabGroupsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabGroupsAndroid)},

    {"enable-tab-groups-continuation",
     flag_descriptions::kTabGroupsContinuationAndroidName,
     flag_descriptions::kTabGroupsContinuationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabGroupsContinuationAndroid)},

    {"enable-tab-groups-ui-improvements",
     flag_descriptions::kTabGroupsUiImprovementsAndroidName,
     flag_descriptions::kTabGroupsUiImprovementsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabGroupsUiImprovementsAndroid)},

    {"enable-tab-switcher-on-return",
     flag_descriptions::kTabSwitcherOnReturnName,
     flag_descriptions::kTabSwitcherOnReturnDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabSwitcherOnReturn,
                                    kTabSwitcherOnReturnVariations,
                                    "ChromeStart")},

    {"enable-tab-to-gts-animation",
     flag_descriptions::kTabToGTSAnimationAndroidName,
     flag_descriptions::kTabToGTSAnimationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabToGTSAnimation)},

    {"enable-tab-engagement-reporting",
     flag_descriptions::kTabEngagementReportingName,
     flag_descriptions::kTabEngagementReportingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabEngagementReportingAndroid)},

    {"enable-duet-tabstrip-integration",
     flag_descriptions::kDuetTabStripIntegrationAndroidName,
     flag_descriptions::kDuetTabStripIntegrationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDuetTabStripIntegrationAndroid)},

    {"enable-conditional-tabstrip",
     flag_descriptions::kConditionalTabStripAndroidName,
     flag_descriptions::kConditionalTabStripAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kConditionalTabStripAndroid,
         kConditionalTabStripAndroidVariations,
         "ConditioanlTabStrip")},
#endif  // OS_ANDROID

    {"enable-layout-ng", flag_descriptions::kEnableLayoutNGName,
     flag_descriptions::kEnableLayoutNGDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kLayoutNG)},

    {"enable-lazy-image-loading",
     flag_descriptions::kEnableLazyImageLoadingName,
     flag_descriptions::kEnableLazyImageLoadingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kLazyImageLoading,
                                    kLazyImageLoadingVariations,
                                    "LazyLoad")},

    {"enable-lazy-frame-loading",
     flag_descriptions::kEnableLazyFrameLoadingName,
     flag_descriptions::kEnableLazyFrameLoadingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kLazyFrameLoading,
                                    kLazyFrameLoadingVariations,
                                    "LazyLoad")},

    {"autofill-cache-query-responses",
     flag_descriptions::kAutofillCacheQueryResponsesName,
     flag_descriptions::kAutofillCacheQueryResponsesDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheQueryResponses)},

    {"autofill-enable-company-name",
     flag_descriptions::kAutofillEnableCompanyNameName,
     flag_descriptions::kAutofillEnableCompanyNameDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCompanyName)},

    {"autofill-enable-toolbar-status-chip",
     flag_descriptions::kAutofillEnableToolbarStatusChipName,
     flag_descriptions::kAutofillEnableToolbarStatusChipDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableToolbarStatusChip)},

    {"autofill-enforce-min-required-fields-for-heuristics",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForHeuristicsName,
     flag_descriptions::
         kAutofillEnforceMinRequiredFieldsForHeuristicsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics)},
    {"autofill-enforce-min-required-fields-for-query",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForQuery)},
    {"autofill-enforce-min-required-fields-for-upload",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForUpload)},
    {"autofill-rich-metadata-queries",
     flag_descriptions::kAutofillRichMetadataQueriesName,
     flag_descriptions::kAutofillRichMetadataQueriesDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRichMetadataQueries)},
    {"enable-experimental-productivity-features",
     flag_descriptions::kExperimentalProductivityFeaturesName,
     flag_descriptions::kExperimentalProductivityFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExperimentalProductivityFeatures)},

#if defined(USE_AURA)
    {"touchpad-overscroll-history-navigation",
     flag_descriptions::kTouchpadOverscrollHistoryNavigationName,
     flag_descriptions::kTouchpadOverscrollHistoryNavigationDescription,
     kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTouchpadOverscrollHistoryNavigation)},
#endif

    {"unsafely-treat-insecure-origin-as-secure",
     flag_descriptions::kTreatInsecureOriginAsSecureName,
     flag_descriptions::kTreatInsecureOriginAsSecureDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(
         network::switches::kUnsafelyTreatInsecureOriginAsSecure,
         "")},

    {"treat-unsafe-downloads-as-active-content",
     flag_descriptions::kTreatUnsafeDownloadsAsActiveName,
     flag_descriptions::kTreatUnsafeDownloadsAsActiveDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kTreatUnsafeDownloadsAsActive)},

    {"detect-target-embedding-lookalikes",
     flag_descriptions::kDetectTargetEmbeddingLookalikesName,
     flag_descriptions::kDetectTargetEmbeddingLookalikesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         lookalikes::features::kDetectTargetEmbeddingLookalikes)},

#if defined(OS_CHROMEOS)
    {"enable-play-store-search", flag_descriptions::kEnablePlayStoreSearchName,
     flag_descriptions::kEnablePlayStoreSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnablePlayStoreAppSearch)},

    {"enable-app-data-search", flag_descriptions::kEnableAppDataSearchName,
     flag_descriptions::kEnableAppDataSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppDataSearch)},

    {"enable-app-grid-ghost", flag_descriptions::kEnableAppGridGhostName,
     flag_descriptions::kEnableAppGridGhostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppGridGhost)},
#endif  // OS_CHROMEOS

    {"enable-accessibility-expose-aria-annotations",
     flag_descriptions::kAccessibilityExposeARIAAnnotationsName,
     flag_descriptions::kAccessibilityExposeARIAAnnotationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAccessibilityExposeARIAAnnotations)},

    {"enable-accessibility-expose-display-none",
     flag_descriptions::kAccessibilityExposeDisplayNoneName,
     flag_descriptions::kAccessibilityExposeDisplayNoneDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAccessibilityExposeDisplayNone)},

#if !defined(OS_ANDROID)
    {"enable-accessibility-live-captions",
     flag_descriptions::kEnableAccessibilityLiveCaptionsName,
     flag_descriptions::kEnableAccessibilityLiveCaptionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kLiveCaption)},
#endif  // !defined(OS_ANDROID)

    {"enable-accessibility-object-model",
     flag_descriptions::kEnableAccessibilityObjectModelName,
     flag_descriptions::kEnableAccessibilityObjectModelDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableAccessibilityObjectModel)},

#if defined(OS_ANDROID)
    {"cct-incognito", flag_descriptions::kCCTIncognitoName,
     flag_descriptions::kCCTIncognitoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognito)},
#endif

#if !defined(OS_ANDROID)
    {"proactive-tab-freeze", flag_descriptions::kTabFreezeName,
     flag_descriptions::kTabFreezeDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kTabFreeze,
         kTabFreezeVariations,
         resource_coordinator::kTabFreezeFeatureName)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-arc-cups-api", flag_descriptions::kArcCupsApiName,
     flag_descriptions::kArcCupsApiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kArcCupsApi)},
#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
    {"enable-native-controls",
     flag_descriptions::kEnableVideoPlayerNativeControlsName,
     flag_descriptions::kEnableVideoPlayerNativeControlsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVideoPlayerNativeControls)},
#endif

#if defined(OS_ANDROID)
    {"background-task-component-update",
     flag_descriptions::kBackgroundTaskComponentUpdateName,
     flag_descriptions::kBackgroundTaskComponentUpdateDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBackgroundTaskComponentUpdate)},
#endif

#if defined(OS_CHROMEOS)
    {"smart-text-selection", flag_descriptions::kSmartTextSelectionName,
     flag_descriptions::kSmartTextSelectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kSmartTextSelectionFeature)},
#endif  // OS_CHROMEOS

    {"allow-sxg-certs-without-extension",
     flag_descriptions::kAllowSignedHTTPExchangeCertsWithoutExtensionName,
     flag_descriptions::
         kAllowSignedHTTPExchangeCertsWithoutExtensionDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kAllowSignedHTTPExchangeCertsWithoutExtension)},

    {"enable-sxg-subresource-prefetching",
     flag_descriptions::kEnableSignedExchangeSubresourcePrefetchName,
     flag_descriptions::kEnableSignedExchangeSubresourcePrefetchDescription,
     kOsAll, FEATURE_VALUE_TYPE(features::kSignedExchangeSubresourcePrefetch)},

    {"enable-sxg-prefetch-cache-for-navigations",
     flag_descriptions::kEnableSignedExchangePrefetchCacheForNavigationsName,
     flag_descriptions::
         kEnableSignedExchangePrefetchCacheForNavigationsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kSignedExchangePrefetchCacheForNavigations)},

    {"enable-autofill-account-wallet-storage",
     flag_descriptions::kEnableAutofillAccountWalletStorageName,
     flag_descriptions::kEnableAutofillAccountWalletStorageDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableAccountWalletStorage)},

#if defined(OS_CHROMEOS)
    {"enable-zero-state-suggestions",
     flag_descriptions::kEnableZeroStateSuggestionsName,
     flag_descriptions::kEnableZeroStateSuggestionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableZeroStateSuggestions)},
    {"enable-zero-state-app-reinstall-suggestions",
     flag_descriptions::kEnableAppReinstallZeroStateName,
     flag_descriptions::kEnableAppReinstallZeroStateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppReinstallZeroState)},
#endif  // OS_CHROMEOS

    {"enable-sync-device-info-in-transport-mode",
     flag_descriptions::kSyncDeviceInfoInTransportModeName,
     flag_descriptions::kSyncDeviceInfoInTransportModeDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncDeviceInfoInTransportMode)},

    {"enable-lookalike-url-navigation-suggestions",
     flag_descriptions::kLookalikeUrlNavigationSuggestionsName,
     flag_descriptions::kLookalikeUrlNavigationSuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLookalikeUrlNavigationSuggestionsUI)},

    {"enable-resampling-input-events",
     flag_descriptions::kEnableResamplingInputEventsName,
     flag_descriptions::kEnableResamplingInputEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kResamplingInputEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingInputEvents")},

    {"enable-resampling-scroll-events",
     flag_descriptions::kEnableResamplingScrollEventsName,
     flag_descriptions::kEnableResamplingScrollEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kResamplingScrollEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingScrollEvents")},

    {"enable-filtering-scroll-events",
     flag_descriptions::kFilteringScrollPredictionName,
     flag_descriptions::kFilteringScrollPredictionDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kFilteringScrollPrediction,
                                    kFilteringPredictionFeatureVariations,
                                    "FilteringScrollPrediction")},

    {"enable-first-scroll-latency-measurement",
     flag_descriptions::kFirstScrollLatencyMeasurementName,
     flag_descriptions::kFirstScrollLatencyMeasurementDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFirstScrollLatencyMeasurement)},

    {"compositor-threaded-scrollbar-scrolling",
     flag_descriptions::kCompositorThreadedScrollbarScrollingName,
     flag_descriptions::kCompositorThreadedScrollbarScrollingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kCompositorThreadedScrollbarScrolling)},

#if defined(OS_CHROMEOS)
    {"enable-vaapi-jpeg-image-decode-acceleration",
     flag_descriptions::kVaapiJpegImageDecodeAccelerationName,
     flag_descriptions::kVaapiJpegImageDecodeAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kVaapiJpegImageDecodeAcceleration)},

    {"enable-vaapi-webp-image-decode-acceleration",
     flag_descriptions::kVaapiWebPImageDecodeAccelerationName,
     flag_descriptions::kVaapiWebPImageDecodeAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kVaapiWebPImageDecodeAcceleration)},
#endif

#if defined(OS_WIN)
    {"calculate-native-win-occlusion",
     flag_descriptions::kCalculateNativeWinOcclusionName,
     flag_descriptions::kCalculateNativeWinOcclusionDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kCalculateNativeWinOcclusion)},
#endif  // OS_WIN

#if !defined(OS_ANDROID)
    {"happiness-tracking-surveys-for-desktop",
     flag_descriptions::kHappinessTrackingSurveysForDesktopName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktop)},

    {"happiness-tracking-surveys-for-desktop-demo",
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktopDemo)},

    {"happiness-tracking-surveys-for-desktop-migration",
     flag_descriptions::kHappinessTrackingSurveysForDesktopMigrationName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopMigrationDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         features::kHappinessTrackingSurveysForDesktopMigration)},

    {"happiness-tracking-surveys-for-desktop-settings",
     flag_descriptions::kHappinessTrackingSurveysForDesktopSettingsName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopSettingsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktopSettings)},

    {"happiness-tracking-surveys-for-desktop-settings-privacy",
     flag_descriptions::kHappinessTrackingSurveysForDesktopSettingsPrivacyName,
     flag_descriptions::
         kHappinessTrackingSurveysForDesktopSettingsPrivacyDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         features::kHappinessTrackingSurveysForDesktopSettingsPrivacy)},
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"use-multilogin-endpoint", flag_descriptions::kUseMultiloginEndpointName,
     flag_descriptions::kUseMultiloginEndpointDescription,
     kOsMac | kOsWin | kOsLinux, FEATURE_VALUE_TYPE(kUseMultiloginEndpoint)},

    {"enable-new-profile-picker", flag_descriptions::kNewProfilePickerName,
     flag_descriptions::kNewProfilePickerDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kNewProfilePicker)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-usbguard", flag_descriptions::kUsbguardName,
     flag_descriptions::kUsbguardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kUsbguard)},
    {"enable-fs-nosymfollow", flag_descriptions::kFsNosymfollowName,
     flag_descriptions::kFsNosymfollowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFsNosymfollow)},
    {"enable-arc-unified-audio-focus",
     flag_descriptions::kEnableArcUnifiedAudioFocusName,
     flag_descriptions::kEnableArcUnifiedAudioFocusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableUnifiedAudioFocusFeature)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescription, kOsWin,
     MULTI_VALUE_TYPE(kUseAngleChoices)},
#endif
#if defined(OS_ANDROID)
    {"draw-vertically-edge-to-edge",
     flag_descriptions::kDrawVerticallyEdgeToEdgeName,
     flag_descriptions::kDrawVerticallyEdgeToEdgeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDrawVerticallyEdgeToEdge)},
#endif
#if defined(OS_ANDROID)
    {"enable-ephemeral-tab-bottom-sheet",
     flag_descriptions::kEphemeralTabUsingBottomSheetName,
     flag_descriptions::kEphemeralTabUsingBottomSheetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEphemeralTabUsingBottomSheet)},
    {"overlay-new-layout", flag_descriptions::kOverlayNewLayoutName,
     flag_descriptions::kOverlayNewLayoutDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOverlayNewLayout)},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"enable-assistant-dsp", flag_descriptions::kEnableGoogleAssistantDspName,
     flag_descriptions::kEnableGoogleAssistantDspDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kEnableDspHotword)},

    {"enable-assistant-app-support",
     flag_descriptions::kEnableAssistantAppSupportName,
     flag_descriptions::kEnableAssistantAppSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantAppSupport)},

    {"enable-assistant-media-session-integration",
     flag_descriptions::kEnableAssistantMediaSessionIntegrationName,
     flag_descriptions::kEnableAssistantMediaSessionIntegrationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kEnableMediaSessionIntegration)},

    {"enable-quick-answers", flag_descriptions::kEnableQuickAnswersName,
     flag_descriptions::kEnableQuickAnswersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswers)},

    {"enable-quick-answers-rich-ui",
     flag_descriptions::kEnableQuickAnswersRichUiName,
     flag_descriptions::kEnableQuickAnswersRichUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersRichUi)},

    {"enable-quick-answers-text-annotator",
     flag_descriptions::kEnableQuickAnswersTextAnnotatorName,
     flag_descriptions::kEnableQuickAnswersTextAnnotatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersTextAnnotator)},

#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
    {"click-to-call-ui", flag_descriptions::kClickToCallUIName,
     flag_descriptions::kClickToCallUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kClickToCallUI)},
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"remote-copy-receiver", flag_descriptions::kRemoteCopyReceiverName,
     flag_descriptions::kRemoteCopyReceiverDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kRemoteCopyReceiver)},
    {"remote-copy-image-notification",
     flag_descriptions::kRemoteCopyImageNotificationName,
     flag_descriptions::kRemoteCopyImageNotificationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kRemoteCopyImageNotification)},
    {"remote-copy-persistent-notification",
     flag_descriptions::kRemoteCopyPersistentNotificationName,
     flag_descriptions::kRemoteCopyPersistentNotificationDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(kRemoteCopyPersistentNotification)},
    {"remote-copy-progress-notification",
     flag_descriptions::kRemoteCopyProgressNotificationName,
     flag_descriptions::kRemoteCopyProgressNotificationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kRemoteCopyProgressNotification)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

    {"shared-clipboard-ui", flag_descriptions::kSharedClipboardUIName,
     flag_descriptions::kSharedClipboardUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharedClipboardUI)},

    {"enable-ambient-authentication-in-incognito",
     flag_descriptions::kEnableAmbientAuthenticationInIncognitoName,
     flag_descriptions::kEnableAmbientAuthenticationInIncognitoDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAmbientAuthenticationInIncognito)},

    {"enable-ambient-authentication-in-guest-session",
     flag_descriptions::kEnableAmbientAuthenticationInGuestSessionName,
     flag_descriptions::kEnableAmbientAuthenticationInGuestSessionDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAmbientAuthenticationInGuestSession)},

    {"enable-send-tab-to-self-omnibox-sending-animation",
     flag_descriptions::kSendTabToSelfOmniboxSendingAnimationName,
     flag_descriptions::kSendTabToSelfOmniboxSendingAnimationDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         send_tab_to_self::kSendTabToSelfOmniboxSendingAnimation)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"sharing-peer-connection-receiver",
     flag_descriptions::kSharingPeerConnectionReceiverName,
     flag_descriptions::kSharingPeerConnectionReceiverDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kSharingPeerConnectionReceiver)},

    {"sharing-peer-connection-sender",
     flag_descriptions::kSharingPeerConnectionSenderName,
     flag_descriptions::kSharingPeerConnectionSenderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kSharingPeerConnectionSender)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

    {"sharing-prefer-vapid", flag_descriptions::kSharingPreferVapidName,
     flag_descriptions::kSharingPreferVapidDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingPreferVapid)},

    {"sharing-qr-code-generator",
     flag_descriptions::kSharingQRCodeGeneratorName,
     flag_descriptions::kSharingQRCodeGeneratorDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kSharingQRCodeGenerator)},

    {"sharing-send-via-sync", flag_descriptions::kSharingSendViaSyncName,
     flag_descriptions::kSharingSendViaSyncDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingSendViaSync)},

    {"sharing-device-expiration",
     flag_descriptions::kSharingDeviceExpirationName,
     flag_descriptions::kSharingDeviceExpirationDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kSharingDeviceExpiration,
                                    kSharingDeviceExpirationVariations,
                                    "SharingDeviceExpiration")},

#if defined(OS_CHROMEOS)
    {"discover-app", flag_descriptions::kEnableDiscoverAppName,
     flag_descriptions::kEnableDiscoverAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDiscoverApp)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
    {"ash-enable-pip-rounded-corners",
     flag_descriptions::kAshEnablePipRoundedCornersName,
     flag_descriptions::kAshEnablePipRoundedCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipRoundedCorners)},

    {"ash-swap-side-volume-buttons-for-orientation",
     flag_descriptions::kAshSwapSideVolumeButtonsForOrientationName,
     flag_descriptions::kAshSwapSideVolumeButtonsForOrientationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSwapSideVolumeButtonsForOrientation)},
#endif  // defined(OS_CHROMEOS)
    {"google-password-manager", flag_descriptions::kGooglePasswordManagerName,
     flag_descriptions::kGooglePasswordManagerDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kGooglePasswordManager)},

    {"enable-implicit-root-scroller",
     flag_descriptions::kEnableImplicitRootScrollerName,
     flag_descriptions::kEnableImplicitRootScrollerDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kImplicitRootScroller)},

    {"enable-cssom-view-scroll-coordinates",
     flag_descriptions::kEnableCSSOMViewScrollCoordinatesName,
     flag_descriptions::kEnableCSSOMViewScrollCoordinatesDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCSSOMViewScrollCoordinates)},

    {"enable-text-fragment-anchor",
     flag_descriptions::kEnableTextFragmentAnchorName,
     flag_descriptions::kEnableTextFragmentAnchorDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kTextFragmentAnchor)},

#if defined(OS_CHROMEOS)
    {"enable-assistant-stereo-input",
     flag_descriptions::kEnableGoogleAssistantStereoInputName,
     flag_descriptions::kEnableGoogleAssistantStereoInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kEnableStereoAudioInput)},
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_MACOSX)
    {"enable-custom-mac-paper-sizes",
     flag_descriptions::kEnableCustomMacPaperSizesName,
     flag_descriptions::kEnableCustomMacPaperSizesDescription, kOsMac,
     FEATURE_VALUE_TYPE(printing::features::kEnableCustomMacPaperSizes)},
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"enable-reopen-tab-in-product-help",
     flag_descriptions::kReopenTabInProductHelpName,
     flag_descriptions::kReopenTabInProductHelpDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHReopenTabFeature)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

    {"enable-audio-focus-enforcement",
     flag_descriptions::kEnableAudioFocusEnforcementName,
     flag_descriptions::kEnableAudioFocusEnforcementDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_session::features::kAudioFocusEnforcement)},
    {"enable-media-session-service",
     flag_descriptions::kEnableMediaSessionServiceName,
     flag_descriptions::kEnableMediaSessionServiceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_session::features::kMediaSessionService)},
    {"enable-gpu-service-logging",
     flag_descriptions::kEnableGpuServiceLoggingName,
     flag_descriptions::kEnableGpuServiceLoggingDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableGPUServiceLogging)},

#if !defined(OS_ANDROID)
    {"hardware-media-key-handling",
     flag_descriptions::kHardwareMediaKeyHandling,
     flag_descriptions::kHardwareMediaKeyHandlingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kHardwareMediaKeyHandling)},
#endif

    {"enable-paint-holding", flag_descriptions::kPaintHoldingName,
     flag_descriptions::kPaintHoldingDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPaintHolding)},

#if defined(OS_CHROMEOS)
    {"app-service-instance-registry",
     flag_descriptions::kAppServiceInstanceRegistryName,
     flag_descriptions::kAppServiceInstanceRegistryDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceInstanceRegistry)},

    {"app-service-intent-handling",
     flag_descriptions::kAppServiceIntentHandlingName,
     flag_descriptions::kAppServiceIntentHandlingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceIntentHandling)},

    {"ash-swiping-from-left-edge-to-go-back",
     flag_descriptions::kAshSwipingFromLeftEdgeToGoBackName,
     flag_descriptions::kAshSwipingFromLeftEdgeToGoBackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSwipingFromLeftEdgeToGoBack)},

    {"use-fake-device-for-media-stream",
     flag_descriptions::kUseFakeDeviceForMediaStreamName,
     flag_descriptions::kUseFakeDeviceForMediaStreamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseFakeDeviceForMediaStream)},

    {"ash-drag-window-from-shelf",
     flag_descriptions::kAshDragWindowFromShelfName,
     flag_descriptions::kAshDragWindowFromShelfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragFromShelfToHomeOrOverview)},

    {"intent-picker-pwa-persistence",
     flag_descriptions::kIntentPickerPWAPersistenceName,
     flag_descriptions::kIntentPickerPWAPersistenceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kIntentPickerPWAPersistence)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    {"d3d11-video-decoder", flag_descriptions::kD3D11VideoDecoderName,
     flag_descriptions::kD3D11VideoDecoderDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kD3D11VideoDecoder)},
#elif defined(OS_CHROMEOS)
    {"chromeos-video-decoder", flag_descriptions::kChromeosVideoDecoderName,
     flag_descriptions::kChromeosVideoDecoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kChromeosVideoDecoder)},
#endif

#if defined(OS_ANDROID)
    {"autofill-assistant-chrome-entry",
     flag_descriptions::kAutofillAssistantChromeEntryName,
     flag_descriptions::kAutofillAssistantChromeEntryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill_assistant::features::kAutofillAssistantChromeEntry)},

    {"autofill-assistant-direct-actions",
     flag_descriptions::kAutofillAssistantDirectActionsName,
     flag_descriptions::kAutofillAssistantDirectActionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill_assistant::features::kAutofillAssistantDirectActions)},
#endif  // defined(OS_ANDROID)

    {"disable-best-effort-tasks",
     flag_descriptions::kDisableBestEffortTasksName,
     flag_descriptions::kDisableBestEffortTasksDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableBestEffortTasks)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
    {"web-contents-occlusion", flag_descriptions::kWebContentsOcclusionName,
     flag_descriptions::kWebContentsOcclusionDescription,
     kOsWin | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebContentsOcclusion)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
    {"mobile-identity-consistency",
     flag_descriptions::kMobileIdentityConsistencyName,
     flag_descriptions::kMobileIdentityConsistencyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(signin::kMobileIdentityConsistency)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"sync-error-infobar", flag_descriptions::kSyncErrorInfoBarName,
     flag_descriptions::kSyncErrorInfoBarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kSyncErrorInfoBarAndroid)},
#endif  // defined(OS_ANDROID)

    {"autofill-use-improved-label-disambiguation",
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationName,
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUseImprovedLabelDisambiguation)},

#if defined(OS_ANDROID)
    {"cct-target-translate-language",
     flag_descriptions::kCCTTargetTranslateLanguageName,
     flag_descriptions::kCCTTargetTranslateLanguageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTTargetTranslateLanguage)},
#endif

    {"native-file-system-api", flag_descriptions::kNativeFileSystemAPIName,
     flag_descriptions::kNativeFileSystemAPIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kNativeFileSystemAPI)},

    {"file-handling-api", flag_descriptions::kFileHandlingAPIName,
     flag_descriptions::kFileHandlingAPIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kFileHandlingAPI)},

#if defined(TOOLKIT_VIEWS)
    {"installable-ink-drop", flag_descriptions::kInstallableInkDropName,
     flag_descriptions::kInstallableInkDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::kInstallableInkDropFeature)},
#endif  // defined(TOOLKIT_VIEWS)

#if defined(OS_CHROMEOS)
    {"enable-assistant-launcher-integration",
     flag_descriptions::kEnableAssistantLauncherIntegrationName,
     flag_descriptions::kEnableAssistantLauncherIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAssistantSearch)},
#endif  // OS_CHROMEOS

#if defined(TOOLKIT_VIEWS)
    {"enable-md-rounded-corners-on-dialogs",
     flag_descriptions::kEnableMDRoundedCornersOnDialogsName,
     flag_descriptions::kEnableMDRoundedCornersOnDialogsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::features::kEnableMDRoundedCornersOnDialogs)},
#endif  // defined(TOOLKIT_VIEWS)

    {"strict-origin-isolation", flag_descriptions::kStrictOriginIsolationName,
     flag_descriptions::kStrictOriginIsolationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kStrictOriginIsolation)},

    {"autofill-no-local-save-on-unmask-success",
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessName,
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillNoLocalSaveOnUnmaskSuccess)},

#if defined(OS_ANDROID)
    {"enable-logging-js-console-messages",
     flag_descriptions::kLogJsConsoleMessagesName,
     flag_descriptions::kLogJsConsoleMessagesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kLogJsConsoleMessages)},
#endif  // OS_ANDROID

    {"enable-skia-renderer", flag_descriptions::kSkiaRendererName,
     flag_descriptions::kSkiaRendererDescription,
     kOsLinux | kOsWin | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kUseSkiaRenderer)},

#if defined(OS_CHROMEOS)
    {"allow-disable-mouse-acceleration",
     flag_descriptions::kAllowDisableMouseAccelerationName,
     flag_descriptions::kAllowDisableMouseAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAllowDisableMouseAcceleration)},

    {"allow-scroll-settings", flag_descriptions::kAllowScrollSettingsName,
     flag_descriptions::kAllowScrollSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAllowScrollSettings)},

    {"enable-streamlined-usb-printer-setup",
     flag_descriptions::kStreamlinedUsbPrinterSetupName,
     flag_descriptions::kStreamlinedUsbPrinterSetupDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kStreamlinedUsbPrinterSetup)},

    {"enable-media-session-notifications",
     flag_descriptions::kMediaSessionNotificationsName,
     flag_descriptions::kMediaSessionNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMediaSessionNotification)},

    {"enable-neural-stylus-palm-rejection",
     flag_descriptions::kEnableNeuralStylusPalmRejectionName,
     flag_descriptions::kEnableNeuralStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableNeuralPalmDetectionFilter)},

    {"enable-palm-max-touch-major",
     flag_descriptions::kEnablePalmOnMaxTouchMajorName,
     flag_descriptions::kEnablePalmOnMaxTouchMajorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmOnMaxTouchMajor)},

    {"enable-palm-tool-type-palm",
     flag_descriptions::kEnablePalmOnToolTypePalmName,
     flag_descriptions::kEnablePalmOnToolTypePalmName, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmOnToolTypePalm)},

    {"enable-heuristic-stylus-palm-rejection",
     flag_descriptions::kEnableHeuristicStylusPalmRejectionName,
     flag_descriptions::kEnableHeuristicStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHeuristicPalmDetectionFilter)},

    {"enable-hide-arc-media-notifications",
     flag_descriptions::kHideArcMediaNotificationsName,
     flag_descriptions::kHideArcMediaNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideArcMediaNotifications)},

    {"reduce-display-notifications",
     flag_descriptions::kReduceDisplayNotificationsName,
     flag_descriptions::kReduceDisplayNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kReduceDisplayNotifications)},

    {"use-search-click-for-right-click",
     flag_descriptions::kUseSearchClickForRightClickName,
     flag_descriptions::kUseSearchClickForRightClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseSearchClickForRightClick)},

    {"enable-print-server-ui", flag_descriptions::kPrintServerUiName,
     flag_descriptions::kPrintServerUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kPrintServerUi)},

    {"display-change-modal", flag_descriptions::kDisplayChangeModalName,
     flag_descriptions::kDisplayChangeModalDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisplayChangeModal)},

    {"corner-shortcuts", flag_descriptions::kCornerShortcutsName,
     flag_descriptions::kCornerShortcutsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCornerShortcuts)},

    {"printer-status", flag_descriptions::kPrinterStatusName,
     flag_descriptions::kPrinterStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPrinterStatus)},

    {"print-job-management-app", flag_descriptions::kPrintJobManagementAppName,
     flag_descriptions::kPrintJobManagementAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPrintJobManagementApp)},

    {"display-identification", flag_descriptions::kDisplayIdentificationName,
     flag_descriptions::kDisplayIdentificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisplayIdentification)},
#endif  // OS_CHROMEOS

    {"autofill-off-no-server-data",
     flag_descriptions::kAutofillOffNoServerDataName,
     flag_descriptions::kAutofillOffNoServerDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillOffNoServerData)},

    {"enable-portals", flag_descriptions::kEnablePortalsName,
     flag_descriptions::kEnablePortalsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPortals)},
    {"enable-portals-cross-origin",
     flag_descriptions::kEnablePortalsCrossOriginName,
     flag_descriptions::kEnablePortalsCrossOriginDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPortalsCrossOrigin)},
    {"enable-autofill-credit-card-authentication",
     flag_descriptions::kEnableAutofillCreditCardAuthenticationName,
     flag_descriptions::kEnableAutofillCreditCardAuthenticationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardAuthentication)},

#if defined(OS_MACOSX)
    {"mac-system-media-permissions-info-ui",
     flag_descriptions::kMacSystemMediaPermissionsInfoUiName,
     flag_descriptions::kMacSystemMediaPermissionsInfoUiDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSystemMediaPermissionsInfoUi)},
#endif  // defined(OS_MACOSX)

    {"storage-access-api", flag_descriptions::kStorageAccessAPIName,
     flag_descriptions::kStorageAccessAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kStorageAccessAPI)},

    {"same-site-by-default-cookies",
     flag_descriptions::kSameSiteByDefaultCookiesName,
     flag_descriptions::kSameSiteByDefaultCookiesDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kSameSiteByDefaultCookies)},

    {"enable-removing-all-third-party-cookies",
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesName,
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         browsing_data::features::kEnableRemovingAllThirdPartyCookies)},

    {"cookies-without-same-site-must-be-secure",
     flag_descriptions::kCookiesWithoutSameSiteMustBeSecureName,
     flag_descriptions::kCookiesWithoutSameSiteMustBeSecureDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kCookiesWithoutSameSiteMustBeSecure)},

#if defined(OS_CHROMEOS)
    {"enterprise-reporting-in-chromeos",
     flag_descriptions::kEnterpriseReportingInChromeOSName,
     flag_descriptions::kEnterpriseReportingInChromeOSDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnterpriseReportingInChromeOS)},
#endif  // !defined(OS_CHROMEOS)

    {"enable-unsafe-webgpu", flag_descriptions::kUnsafeWebGPUName,
     flag_descriptions::kUnsafeWebGPUDescription, kOsMac | kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeWebGPU)},

#if defined(OS_ANDROID)
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
#endif  // defined(OS_ANDROID)

    {"autofill-prune-suggestions",
     flag_descriptions::kAutofillPruneSuggestionsName,
     flag_descriptions::kAutofillPruneSuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPruneSuggestions)},

    {"allow-popups-during-page-unload",
     flag_descriptions::kAllowPopupsDuringPageUnloadName,
     flag_descriptions::kAllowPopupsDuringPageUnloadDescription,
     kOsAll | kDeprecated,
     FEATURE_VALUE_TYPE(features::kAllowPopupsDuringPageUnload)},

#if defined(OS_CHROMEOS)
    {"enable-advanced-ppd-attributes",
     flag_descriptions::kEnableAdvancedPpdAttributesName,
     flag_descriptions::kEnableAdvancedPpdAttributesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(printing::features::kAdvancedPpdAttributes)},
#endif  // defined(OS_CHROMEOS)

    {"allow-sync-xhr-in-page-dismissal",
     flag_descriptions::kAllowSyncXHRInPageDismissalName,
     flag_descriptions::kAllowSyncXHRInPageDismissalDescription,
     kOsAll | kDeprecated,
     FEATURE_VALUE_TYPE(blink::features::kAllowSyncXHRInPageDismissal)},

    {"form-controls-refresh", flag_descriptions::kFormControlsRefreshName,
     flag_descriptions::kFormControlsRefreshDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFormControlsRefresh)},

#if defined(OS_CHROMEOS)
    {"auto-screen-brightness", flag_descriptions::kAutoScreenBrightnessName,
     flag_descriptions::kAutoScreenBrightnessDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAutoScreenBrightness)},
    {"sync-wifi-configurations", flag_descriptions::kSyncWifiConfigurationsName,
     flag_descriptions::kSyncWifiConfigurationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(switches::kSyncWifiConfigurations)},
#endif  // defined(OS_CHROMEOS)

    {"audio-worklet-realtime-thread",
     flag_descriptions::kAudioWorkletRealtimeThreadName,
     flag_descriptions::kAudioWorkletRealtimeThreadDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAudioWorkletRealtimeThread)},

#if defined(OS_CHROMEOS)
    {"release-notes", flag_descriptions::kReleaseNotesName,
     flag_descriptions::kReleaseNotesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kReleaseNotes)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
    {"smart-dim-model-v3", flag_descriptions::kSmartDimModelV3Name,
     flag_descriptions::kSmartDimModelV3Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSmartDimModelV3)},
    {"smart-dim-new-ml-agent", flag_descriptions::kSmartDimNewMlAgentName,
     flag_descriptions::kSmartDimNewMlAgentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSmartDimNewMlAgent)},
#endif  // defined(OS_CHROMEOS)

    {"privacy-settings-redesign",
     flag_descriptions::kPrivacySettingsRedesignName,
     flag_descriptions::kPrivacySettingsRedesignDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPrivacySettingsRedesign)},

#if defined(OS_ANDROID)
    {"safety-check-android", flag_descriptions::kSafetyCheckAndroidName,
     flag_descriptions::kSafetyCheckAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSafetyCheckAndroid)},
#endif

#if defined(OS_CHROMEOS)
    {"gesture-properties-dbus-service",
     flag_descriptions::kEnableGesturePropertiesDBusServiceName,
     flag_descriptions::kEnableGesturePropertiesDBusServiceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGesturePropertiesDBusService)},
#endif  // defined(OS_CHROMEOS)

    {"cookie-deprecation-messages",
     flag_descriptions::kCookieDeprecationMessagesName,
     flag_descriptions::kCookieDeprecationMessagesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCookieDeprecationMessages)},

    {"ev-details-in-page-info", flag_descriptions::kEvDetailsInPageInfoName,
     flag_descriptions::kEvDetailsInPageInfoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEvDetailsInPageInfo)},

    {"enable-autofill-credit-card-upload-feedback",
     flag_descriptions::kEnableAutofillCreditCardUploadFeedbackName,
     flag_descriptions::kEnableAutofillCreditCardUploadFeedbackDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardUploadFeedback)},

    {"font-access", flag_descriptions::kFontAccessAPIName,
     flag_descriptions::kFontAccessAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFontAccess)},

#if defined(OS_CHROMEOS)
    {"enable-parental-controls-settings",
     flag_descriptions::kEnableParentalControlsSettingsName,
     flag_descriptions::kEnableParentalControlsSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kParentalControlsSettings)},
#endif  // defined(OS_CHROMEOS)

    {"mouse-subframe-no-implicit-capture",
     flag_descriptions::kMouseSubframeNoImplicitCaptureName,
     flag_descriptions::kMouseSubframeNoImplicitCaptureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kMouseSubframeNoImplicitCapture)},

#if defined(OS_ANDROID)
    {"touch-to-fill", flag_descriptions::kAutofillTouchToFillName,
     flag_descriptions::kAutofillTouchToFillDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillTouchToFill)},
#endif  // defined(OS_ANDROID)

    {"enable-sync-trusted-vault",
     flag_descriptions::kEnableSyncTrustedVaultName,
     flag_descriptions::kEnableSyncTrustedVaultDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncSupportTrustedVaultPassphrase)},

    {"enable-sync-uss-nigori", flag_descriptions::kEnableSyncUSSNigoriName,
     flag_descriptions::kEnableSyncUSSNigoriDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUSSNigori)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"global-media-controls", flag_descriptions::kGlobalMediaControlsName,
     flag_descriptions::kGlobalMediaControlsDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControls)},

    {"global-media-controls-for-cast",
     flag_descriptions::kGlobalMediaControlsForCastName,
     flag_descriptions::kGlobalMediaControlsForCastDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsForCast)},

    {"global-media-controls-picture-in-picture",
     flag_descriptions::kGlobalMediaControlsPictureInPictureName,
     flag_descriptions::kGlobalMediaControlsPictureInPictureDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsPictureInPicture)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_WIN)
    {"win-use-native-spellchecker",
     flag_descriptions::kWinUseBrowserSpellCheckerName,
     flag_descriptions::kWinUseBrowserSpellCheckerDescription, kOsWin,
     FEATURE_VALUE_TYPE(spellcheck::kWinUseBrowserSpellChecker)},

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
    {"win-use-hybrid-spellchecker",
     flag_descriptions::kWinUseHybridSpellCheckerName,
     flag_descriptions::kWinUseHybridSpellCheckerDescription, kOsWin,
     FEATURE_VALUE_TYPE(spellcheck::kWinUseHybridSpellChecker)},
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
#endif  // BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_WIN)

    {"safety-tips", flag_descriptions::kSafetyTipName,
     flag_descriptions::kSafetyTipDescription, kOsAll,
     FEATURE_VALUE_TYPE(security_state::features::kSafetyTipUI)},

#if defined(OS_CHROMEOS)
    {"crostini-webui-upgrader", flag_descriptions::kCrostiniWebUIUpgraderName,
     flag_descriptions::kCrostiniWebUIUpgraderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniWebUIUpgrader)},
#endif  // OS_CHROMEOS

    {"turn-off-streaming-media-caching",
     flag_descriptions::kTurnOffStreamingMediaCachingName,
     flag_descriptions::kTurnOffStreamingMediaCachingDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTurnOffStreamingMediaCaching)},

#if defined(OS_ANDROID)
    {"password-manager-onboarding-android",
     flag_descriptions::kPasswordManagerOnboardingAndroidName,
     flag_descriptions::kPasswordManagerOnboardingAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordManagerOnboardingAndroid)},
#endif  // defined(OS_ANDROID)

    {"enable-cooperative-scheduling",
     flag_descriptions::kCooperativeSchedulingName,
     flag_descriptions::kCooperativeSchedulingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCooperativeScheduling)},

    {"enable-defer-all-script", flag_descriptions::kEnableDeferAllScriptName,
     flag_descriptions::kEnableDeferAllScriptDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kDeferAllScriptPreviews)},
    {"enable-defer-all-script-without-optimization-hints",
     flag_descriptions::kEnableDeferAllScriptWithoutOptimizationHintsName,
     flag_descriptions::
         kEnableDeferAllScriptWithoutOptimizationHintsDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         previews::switches::kEnableDeferAllScriptWithoutOptimizationHints)},

#if defined(OS_CHROMEOS)
    {"enable-edu-coexistence", flag_descriptions::kEnableEduCoexistenceName,
     flag_descriptions::kEnableEduCoexistenceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEduCoexistence)},
#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
    {"enable-edu-coexistence-consent-log",
     flag_descriptions::kEnableEduCoexistenceConsentLogName,
     flag_descriptions::kEnableEduCoexistenceConsentLogDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEduCoexistenceConsentLog)},
#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
    {"enable-assistant-routines",
     flag_descriptions::kEnableAssistantRoutinesName,
     flag_descriptions::kEnableAssistantRoutinesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantRoutines)},
#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
    {"gaia-action-buttons", flag_descriptions::kGaiaActionButtonsName,
     flag_descriptions::kGaiaActionButtonsDescription, kOsCrOSOwnerOnly,
     FEATURE_VALUE_TYPE(chromeos::features::kGaiaActionButtons)},
#endif  // defined(OS_CHROMEOS)

    {"notification-scheduler-debug-options",
     flag_descriptions::kNotificationSchedulerDebugOptionName,
     flag_descriptions::kNotificationSchedulerDebugOptionDescription,
     kOsAndroid, MULTI_VALUE_TYPE(kNotificationSchedulerChoices)},

#if defined(OS_ANDROID)
    {"usage-stats", flag_descriptions::kUsageStatsName,
     flag_descriptions::kUsageStatsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kUsageStatsFeature)},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"lock-screen-media-controls",
     flag_descriptions::kLockScreenMediaControlsName,
     flag_descriptions::kLockScreenMediaControlsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenMediaControls)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
    {"contextual-nudges", flag_descriptions::kContextualNudgesName,
     flag_descriptions::kContextualNudgesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kContextualNudges)},
#endif  // defined(OS_CHROMEOS)

    {"policy-atomic-group-enabled",
     flag_descriptions::kPolicyAtomicGroupsEnabledName,
     flag_descriptions::kPolicyAtomicGroupsEnabledDescription, kOsAll,
     FEATURE_VALUE_TYPE(policy::features::kPolicyAtomicGroup)},

    {"decode-jpeg-images-to-yuv",
     flag_descriptions::kDecodeJpeg420ImagesToYUVName,
     flag_descriptions::kDecodeJpeg420ImagesToYUVDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDecodeJpeg420ImagesToYUV)},

    {"decode-webp-images-to-yuv",
     flag_descriptions::kDecodeLossyWebPImagesToYUVName,
     flag_descriptions::kDecodeLossyWebPImagesToYUVDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDecodeLossyWebPImagesToYUV)},

    {"dns-httpssvc", flag_descriptions::kDnsHttpssvcName,
     flag_descriptions::kDnsHttpssvcDescription,
     kOsMac | kOsWin | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kDnsHttpssvc)},

    {"dns-over-https", flag_descriptions::kDnsOverHttpsName,
     flag_descriptions::kDnsOverHttpsDescription,
     kOsMac | kOsWin | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDnsOverHttps)},

    {"web-bundles", flag_descriptions::kWebBundlesName,
     flag_descriptions::kWebBundlesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebBundles)},

#if defined(OS_ANDROID)
    {"web-otp-backend", flag_descriptions::kWebOtpBackendName,
     flag_descriptions::kWebOtpBackendDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kWebOtpBackendChoices)},

    {"darken-websites-checkbox-in-themes-setting",
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingName,
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kDarkenWebsitesCheckboxInThemesSetting)},
#endif  // defined(OS_ANDROID)

    {"enable-autofill-upi-vpa", flag_descriptions::kAutofillSaveAndFillVPAName,
     flag_descriptions::kAutofillSaveAndFillVPADescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSaveAndFillVPA)},

    {"quiet-notification-prompts",
     flag_descriptions::kQuietNotificationPromptsName,
     flag_descriptions::kQuietNotificationPromptsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kQuietNotificationPrompts,
                                    kQuietNotificationPromptsVariations,
                                    "QuietNotificationPrompts")},

#if defined(OS_ANDROID)
    {"context-menu-search-with-google-lens",
     flag_descriptions::kContextMenuSearchWithGoogleLensName,
     flag_descriptions::kContextMenuSearchWithGoogleLensDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuSearchWithGoogleLens)},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"launcher-settings-search", flag_descriptions::kLauncherSettingsSearchName,
     flag_descriptions::kLauncherSettingsSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kLauncherSettingsSearch)},

    {"enable-suggested-files", flag_descriptions::kEnableSuggestedFilesName,
     flag_descriptions::kEnableSuggestedFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableSuggestedFiles)},

    {"zero-state-files", flag_descriptions::kZeroStateFilesName,
     flag_descriptions::kZeroStateFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableZeroStateMixedTypesRanker)},

    {"aggregated-ml-app-ranking",
     flag_descriptions::kAggregatedMlAppRankingName,
     flag_descriptions::kAggregatedMlAppRankingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAggregatedMlAppRanking)},

    {"fuzzy-app-search", flag_descriptions::kFuzzyAppSearchName,
     flag_descriptions::kFuzzyAppSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableFuzzyAppSearch)},

    {"aggregated-ml-search-ranking",
     flag_descriptions::kAggregatedMlSearchRankingName,
     flag_descriptions::kAggregatedMlSearchRankingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAggregatedMlSearchRanking)},

#endif  // defined(OS_CHROMEOS)

    {"passwords-account-storage",
     flag_descriptions::kEnablePasswordsAccountStorageName,
     flag_descriptions::kEnablePasswordsAccountStorageDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnablePasswordsAccountStorage)},

    {"improved-cookie-controls", flag_descriptions::kImprovedCookieControlsName,
     flag_descriptions::kImprovedCookieControlsDescription, kOsAll,
     FEATURE_VALUE_TYPE(content_settings::kImprovedCookieControls)},

    {"improved-cookie-controls-for-third-party-cookie-blocking",
     flag_descriptions::kImprovedCookieControlsForThirdPartyCookieBlockingName,
     flag_descriptions::
         kImprovedCookieControlsForThirdPartyCookieBlockingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         content_settings::kImprovedCookieControlsForThirdPartyCookieBlocking)},

    {"autofill-always-return-cloud-tokenized-card",
     flag_descriptions::kAutofillAlwaysReturnCloudTokenizedCardName,
     flag_descriptions::kAutofillAlwaysReturnCloudTokenizedCardDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAlwaysReturnCloudTokenizedCard)},

    {"back-forward-cache", flag_descriptions::kBackForwardCacheName,
     flag_descriptions::kBackForwardCacheDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kBackForwardCache,
                                    kBackForwardCacheVariations,
                                    "BackForwardCache")},

#if defined(OS_ANDROID)
    {"context-menu-copy-image", flag_descriptions::kContextMenuCopyImageName,
     flag_descriptions::kContextMenuCopyImageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuCopyImage)},
    {"enable-clipboard-provider-image-suggestions",
     flag_descriptions::kEnableClipboardProviderImageSuggestionsName,
     flag_descriptions::kEnableClipboardProviderImageSuggestionsDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kEnableClipboardProviderImageSuggestions,
         kOmniboxImageSearchSuggestionThumbnailVariation,
         "OmniboxEnableClipboardProviderImageSuggestions")},
#endif  // defined(OS_ANDROID)

    {"impulse-scroll-animations",
     flag_descriptions::kImpulseScrollAnimationsName,
     flag_descriptions::kImpulseScrollAnimationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kImpulseScrollAnimations)},
    {"texture-layer-skip-wait-for-activation",
     flag_descriptions::kTextureLayerSkipWaitForActivationName,
     flag_descriptions::kTextureLayerSkipWaitForActivationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kTextureLayerSkipWaitForActivation)},
    {"percent-based-scrolling", flag_descriptions::kPercentBasedScrollingName,
     flag_descriptions::kPercentBasedScrollingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPercentBasedScrolling)},

    {"scroll-unification", flag_descriptions::kScrollUnificationName,
     flag_descriptions::kScrollUnificationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kScrollUnification)},

#if defined(OS_WIN)
    {"elastic-overscroll-win", flag_descriptions::kElasticOverscrollWinName,
     flag_descriptions::kElasticOverscrollWinDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kElasticOverscrollWin)},
#endif

#if !defined(OS_ANDROID)
    {"show-legacy-tls-warnings", flag_descriptions::kLegacyTLSWarningsName,
     flag_descriptions::kLegacyTLSWarningsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(security_state::features::kLegacyTLSWarnings)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-assistant-aec", flag_descriptions::kEnableGoogleAssistantAecName,
     flag_descriptions::kEnableGoogleAssistantAecDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantAudioEraser)},
#endif

#if defined(OS_WIN)
    {"enable-winrt-geolocation-implementation",
     flag_descriptions::kWinrtGeolocationImplementationName,
     flag_descriptions::kWinrtGeolocationImplementationDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWinrtGeolocationImplementation)},
#endif

#if defined(OS_MACOSX)
    {"enable-core-location-implementation",
     flag_descriptions::kMacCoreLocationImplementationName,
     flag_descriptions::kMacCoreLocationImplementationDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacCoreLocationImplementation)},
#endif

#if defined(OS_CHROMEOS)
    {"exo-pointer-lock", flag_descriptions::kExoPointerLockName,
     flag_descriptions::kExoPointerLockDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoPointerLock)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
    {"metal", flag_descriptions::kMetalName,
     flag_descriptions::kMetalDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMetal)},
#endif

    {"enable-de-jelly", flag_descriptions::kEnableDeJellyName,
     flag_descriptions::kEnableDeJellyDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableDeJelly)},

#if defined(OS_CHROMEOS)
    {"enable-cros-action-recorder",
     flag_descriptions::kEnableCrOSActionRecorderName,
     flag_descriptions::kEnableCrOSActionRecorderDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kEnableCrOSActionRecorderChoices)},
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
    {"mix-browser-type-tabs", flag_descriptions::kMixBrowserTypeTabsName,
     flag_descriptions::kMixBrowserTypeTabsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMixBrowserTypeTabs)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"enable-games-hub", flag_descriptions::kGamesHubName,
     flag_descriptions::kGamesHubDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(games::features::kGamesHub)},
#endif  // defined(OS_ANDROID)

    {"enable-heavy-ad-intervention",
     flag_descriptions::kHeavyAdInterventionName,
     flag_descriptions::kHeavyAdInterventionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHeavyAdIntervention)},

    {"heavy-ad-privacy-mitigations",
     flag_descriptions::kHeavyAdPrivacyMitigationsName,
     flag_descriptions::kHeavyAdPrivacyMitigationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHeavyAdPrivacyMitigations)},

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    {"enable-ftp", flag_descriptions::kEnableFtpName,
     flag_descriptions::kEnableFtpDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFtpProtocol)},
#endif

#if defined(OS_CHROMEOS)
    {"crostini-use-buster-image",
     flag_descriptions::kCrostiniUseBusterImageName,
     flag_descriptions::kCrostiniUseBusterImageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseBusterImage)},
    {"arc-application-zoom", flag_descriptions::kArcApplicationZoomName,
     flag_descriptions::kArcApplicationZoomDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableApplicationZoomFeature)},
    {"crostini-disk-resizing", flag_descriptions::kCrostiniDiskResizingName,
     flag_descriptions::kCrostiniDiskResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniDiskResizing)},
    {"crostini-username", flag_descriptions::kCrostiniUsernameName,
     flag_descriptions::kCrostiniUsernameDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUsername)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
    {"enable-home-page-location-policy",
     flag_descriptions::kHomepageLocationName,
     flag_descriptions::kHomepageLocationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHomepageLocation)},
    {"homepage-settings-ui-conversion",
     flag_descriptions::kHomepageSettingsUIConversionName,
     flag_descriptions::kHomepageSettingsUIConversionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHomepageSettingsUIConversion)},
    {"homepage-promo-card", flag_descriptions::kHomepagePromoCardName,
     flag_descriptions::kHomepagePromoCardDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kHomepagePromoCard,
                                    kHomepagePromoCardVariations,
                                    "HomepagePromoCard")},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"split-settings-sync", flag_descriptions::kSplitSettingsSyncName,
     flag_descriptions::kSplitSettingsSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSplitSettingsSync)},
    {"new-os-settings-search", flag_descriptions::kNewOsSettingsSearchName,
     flag_descriptions::kNewOsSettingsSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kNewOsSettingsSearch)},
    {"dlc-settings-ui", flag_descriptions::kDlcSettingsUiName,
     flag_descriptions::kDlcSettingsUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDlcSettingsUi)},
    {"help-app", flag_descriptions::kHelpAppName,
     flag_descriptions::kHelpAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHelpAppV2)},
    {"media-app", flag_descriptions::kMediaAppName,
     flag_descriptions::kMediaAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaApp)},
#endif  // defined(OS_CHROMEOS)

    {"passive-mixed-content-warning",
     flag_descriptions::kPassiveMixedContentWarningName,
     flag_descriptions::kPassiveMixedContentWarningDescription, kOsAll,
     FEATURE_VALUE_TYPE(security_state::features::kPassiveMixedContentWarning)},

    {"autofill-enable-virtual-card",
     flag_descriptions::kAutofillEnableVirtualCardName,
     flag_descriptions::kAutofillEnableVirtualCardDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableVirtualCard)},

#if defined(OS_CHROMEOS)
    {"account-id-migration", flag_descriptions::kAccountIdMigrationName,
     flag_descriptions::kAccountIdMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(switches::kAccountIdMigration)},
#endif  // defined(OS_CHROMEOS)

    // TODO(https://crbug.com/1032161): Implement and enable for ChromeOS.
    {"raw-clipboard", flag_descriptions::kRawClipboardName,
     flag_descriptions::kRawClipboardDescription, kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(blink::features::kRawClipboard)},

#if defined(OS_WIN)
    {"slow-dc-timer-interrupts-win",
     flag_descriptions::kSlowDCTimerInterruptsWinName,
     flag_descriptions::kSlowDCTimerInterruptsWinDescription, kOsWin,
     FEATURE_VALUE_TYPE(base::kSlowDCTimerInterruptsWin)},
#endif  // OS_WIN

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
    {"paint-preview-demo", flag_descriptions::kPaintPreviewDemoName,
     flag_descriptions::kPaintPreviewDemoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(paint_preview::kPaintPreviewDemo)},
#endif  // ENABLE_PAINT_PREVIEW && defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"sync-setup-friendly-settings",
     flag_descriptions::kSyncSetupFriendlySettingsName,
     flag_descriptions::kSyncSetupFriendlySettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSyncSetupFriendlySettings)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"block-external-form-redirects-no-gesture",
     flag_descriptions::kIntentBlockExternalFormRedirectsNoGestureName,
     flag_descriptions::kIntentBlockExternalFormRedirectsNoGestureDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         external_intents::kIntentBlockExternalFormRedirectsNoGesture)},
    {"recover-from-never-save-android",
     flag_descriptions::kRecoverFromNeverSaveAndroidName,
     flag_descriptions::kRecoverFromNeverSaveAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kRecoverFromNeverSaveAndroid)},
    {"photo-picker-video-support",
     flag_descriptions::kPhotoPickerVideoSupportName,
     flag_descriptions::kPhotoPickerVideoSupportDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPhotoPickerVideoSupport)},
#endif  // defined(OS_ANDROID)

    {"freeze-user-agent", flag_descriptions::kFreezeUserAgentName,
     flag_descriptions::kFreezeUserAgentDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kFreezeUserAgent)},

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
    {"enable-user-data-snapshot", flag_descriptions::kUserDataSnapshotName,
     flag_descriptions::kUserDataSnapshotDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kUserDataSnapshot)},
#endif

#if defined(OS_WIN)
    {"run-video-capture-service-in-browser",
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessName,
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(features::kRunVideoCaptureServiceInBrowserProcess)},
#endif  // defined(OS_WIN)

    {"legacy-tls-enforced", flag_descriptions::kLegacyTLSEnforcedName,
     flag_descriptions::kLegacyTLSEnforcedDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kLegacyTLSEnforced)},

#if !defined(OS_ANDROID)
    {"password-check", flag_descriptions::kPasswordCheckName,
     flag_descriptions::kPasswordCheckDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordCheck)},
#endif  // !defined(OS_ANDROID)

    {"export-tagged-pdf", flag_descriptions::kExportTaggedPDFName,
     flag_descriptions::kExportTaggedPDFDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kExportTaggedPDF)},
    {"double-buffer-compositing",
     flag_descriptions::kDoubleBufferCompositingName,
     flag_descriptions::kDoubleBufferCompositingDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDoubleBufferCompositing)},

#if defined(OS_CHROMEOS)
    {"enable-ambient-mode", flag_descriptions::kEnableAmbientModeName,
     flag_descriptions::kEnableAmbientModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAmbientModeFeature)},
#endif  // defined(OS_CHROMEOS)

    {"password-change-support", flag_descriptions::kPasswordChangeName,
     flag_descriptions::kPasswordChangeDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordChange)},

#if defined(OS_ANDROID)
    {"context-menu-performance-info",
     flag_descriptions::kContextMenuPerformanceInfoName,
     flag_descriptions::kContextMenuPerformanceInfoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuPerformanceInfo)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"page-info-performance-hints",
     flag_descriptions::kPageInfoPerformanceHintsName,
     flag_descriptions::kPageInfoPerformanceHintsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPageInfoPerformanceHints)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"page-info-version-2", flag_descriptions::kPageInfoV2Name,
     flag_descriptions::kPageInfoV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoV2)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"drag-to-snap-in-clamshell-mode",
     flag_descriptions::kDragToSnapInClamshellModeName,
     flag_descriptions::kDragToSnapInClamshellModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragToSnapInClamshellMode)},
    {"multi-display-overview-and-split-view",
     flag_descriptions::kMultiDisplayOverviewAndSplitViewName,
     flag_descriptions::kMultiDisplayOverviewAndSplitViewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMultiDisplayOverviewAndSplitView)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    {"enable-media-foundation-video-capture",
     flag_descriptions::kEnableMediaFoundationVideoCaptureName,
     flag_descriptions::kEnableMediaFoundationVideoCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kMediaFoundationVideoCapture)},
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
    {"scanning-ui", flag_descriptions::kScanningUIName,
     flag_descriptions::kScanningUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanningUI)},
    {"avatar-toolbar-button", flag_descriptions::kAvatarToolbarButtonName,
     flag_descriptions::kAvatarToolbarButtonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAvatarToolbarButton)},
#endif  // defined(OS_CHROMEOS)

    {"omnibox-context-menu-show-full-urls",
     flag_descriptions::kOmniboxContextMenuShowFullUrlsName,
     flag_descriptions::kOmniboxContextMenuShowFullUrlsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxContextMenuShowFullUrls)},

    {"color-provider-redirection",
     flag_descriptions::kColorProviderRedirectionName,
     flag_descriptions::kColorProviderRedirectionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kColorProviderRedirection)},

    {"trust-tokens", flag_descriptions::kTrustTokensName,
     flag_descriptions::kTrustTokensDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kTrustTokens)},

#if defined(OS_ANDROID)
    {"android-partner-customization-phenotype",
     flag_descriptions::kAndroidPartnerCustomizationPhenotypeName,
     flag_descriptions::kAndroidPartnerCustomizationPhenotypeDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kAndroidPartnerCustomizationPhenotype)},
#endif  // defined(OS_ANDROID)

    {"media-history", flag_descriptions::kMediaHistoryName,
     flag_descriptions::kMediaHistoryDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kUseMediaHistoryStore)},

#if !defined(OS_ANDROID)
    {"nearby-sharing", flag_descriptions::kNearbySharingName,
     flag_descriptions::kNearbySharingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNearbySharing)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"android-multiple-display", flag_descriptions::kAndroidMultipleDisplayName,
     flag_descriptions::kAndroidMultipleDisplayDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidMultipleDisplay)},
#endif  // defined(OS_ANDROID)
    {"autofill-enable-surfacing-server-card-nickname",
     flag_descriptions::kAutofillEnableSurfacingServerCardNicknameName,
     flag_descriptions::kAutofillEnableSurfacingServerCardNicknameDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSurfacingServerCardNickname)},

    {"app-cache", flag_descriptions::kAppCacheName,
     flag_descriptions::kAppCacheDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAppCache)},
    {"enable-autofill-cache-server-card-info",
     flag_descriptions::kEnableAutofillCacheServerCardInfoName,
     flag_descriptions::kEnableAutofillCacheServerCardInfoDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheServerCardInfo)},

    {"autofill-enable-sticky-payments-bubble",
     flag_descriptions::kAutofillEnableStickyPaymentsBubbleName,
     flag_descriptions::kAutofillEnableStickyPaymentsBubbleDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableStickyPaymentsBubble)},

    {"align-font-display-auto-lcp",
     flag_descriptions::kAlignFontDisplayAutoTimeoutWithLCPGoalName,
     flag_descriptions::kAlignFontDisplayAutoTimeoutWithLCPGoalDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kAlignFontDisplayAutoTimeoutWithLCPGoal,
         kAlignFontDisplayAutoTimeoutWithLCPGoalVariations,
         "AlignFontDisplayAutoTimeoutWithLCPGoalVariations")},

#if defined(OS_CHROMEOS)
    {"enable-palm-suppression", flag_descriptions::kEnablePalmSuppressionName,
     flag_descriptions::kEnablePalmSuppressionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmSuppression)},

    {"enable-high-resolution-mouse-scrolling",
     flag_descriptions::kEnableHighResolutionMouseScrollingName,
     flag_descriptions::kEnableHighResolutionMouseScrollingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHighResolutionMouseScrolling)},

    {"movable-partial-screenshot-region",
     flag_descriptions::kMovablePartialScreenshotName,
     flag_descriptions::kMovablePartialScreenshotDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMovablePartialScreenshot)},
#endif  // defined(OS_CHROMEOS)

    {"enable-experimental-cookie-features",
     flag_descriptions::kEnableExperimentalCookieFeaturesName,
     flag_descriptions::kEnableExperimentalCookieFeaturesDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableExperimentalCookieFeaturesChoices)},

    {"autofill-enable-google-issued-card",
     flag_descriptions::kAutofillEnableGoogleIssuedCardName,
     flag_descriptions::kAutofillEnableGoogleIssuedCardDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableGoogleIssuedCard)},

#if defined(TOOLKIT_VIEWS)
    {"textfield-focus-on-tap-up", flag_descriptions::kTextfieldFocusOnTapUpName,
     flag_descriptions::kTextfieldFocusOnTapUpDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::features::kTextfieldFocusOnTapUp)},
#endif  // defined(TOOLKIT_VIEWS)

    {"permission-chip", flag_descriptions::kPermissionChipName,
     flag_descriptions::kPermissionChipDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPermissionChip)},

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"dice-web-signin-interception",
     flag_descriptions::kDiceWebSigninInterceptionName,
     flag_descriptions::kDiceWebSigninInterceptionDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(kDiceWebSigninInterceptionFeature)},
#endif  // ENABLE_DICE_SUPPORT
    {"new-canvas-2d-api", flag_descriptions::kNewCanvas2DAPIName,
     flag_descriptions::kNewCanvas2DAPIDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNewCanvas2DAPI)},

    {"enable-translate-sub-frames",
     flag_descriptions::kEnableTranslateSubFramesName,
     flag_descriptions::kEnableTranslateSubFramesDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTranslateSubFrames)},

#if defined(OS_CHROMEOS)
    {"suggested-content-toggle", flag_descriptions::kSuggestedContentToggleName,
     flag_descriptions::kSuggestedContentToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSuggestedContentToggle)},
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
    {"enable-media-feeds", flag_descriptions::kEnableMediaFeedsName,
     flag_descriptions::kEnableMediaFeedsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kMediaFeeds)},
#endif  // !defined(OS_ANDROID)

    {"autofill-enable-card-nickname-management",
     flag_descriptions::kAutofillEnableCardNicknameManagementName,
     flag_descriptions::kAutofillEnableCardNicknameManagementDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardNicknameManagement)},

    {"conversion-measurement-api",
     flag_descriptions::kConversionMeasurementApiName,
     flag_descriptions::kConversionMeasurementApiDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kConversionMeasurement)},
    {"conversion-measurement-debug-mode",
     flag_descriptions::kConversionMeasurementDebugModeName,
     flag_descriptions::kConversionMeasurementDebugModeDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kConversionsDebugMode)},

    // NOTE: Adding a new flag requires adding a corresponding entry to enum
    // "LoginCustomFlags" in tools/metrics/histograms/enums.xml. See "Flag
    // Histograms" in tools/metrics/histograms/README.md (run the
    // AboutFlagsHistogramTest unit test to verify this process).
};

class FlagsStateSingleton : public flags_ui::FlagsState::Delegate {
 public:
  FlagsStateSingleton()
      : flags_state_(
            std::make_unique<flags_ui::FlagsState>(kFeatureEntries, this)) {}
  ~FlagsStateSingleton() override = default;

  static FlagsStateSingleton* GetInstance() {
    return base::Singleton<FlagsStateSingleton>::get();
  }

  static flags_ui::FlagsState* GetFlagsState() {
    return GetInstance()->flags_state_.get();
  }

  void RebuildState(const std::vector<flags_ui::FeatureEntry>& entries) {
    flags_state_ = std::make_unique<flags_ui::FlagsState>(entries, this);
  }

 private:
  // flags_ui::FlagsState::Delegate:
  bool ShouldExcludeFlag(const FeatureEntry& entry) override {
    return flags::IsFlagExpired(entry.internal_name);
  }

  std::unique_ptr<flags_ui::FlagsState> flags_state_;

  DISALLOW_COPY_AND_ASSIGN(FlagsStateSingleton);
};

bool ShouldSkipNonDeprecatedFeatureEntry(const FeatureEntry& entry) {
  return ~entry.supported_platforms & kDeprecated;
}

bool SkipConditionalFeatureEntry(const FeatureEntry& entry) {
  version_info::Channel channel = chrome::GetChannel();
#if defined(OS_CHROMEOS)
  // enable-ui-devtools is only available on for non Stable channels.
  if (!strcmp(ui_devtools::switches::kEnableUiDevTools, entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }

  if (!strcmp(kLacrosSupportInternalName, entry.internal_name) &&
      channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }
#endif  // defined(OS_CHROMEOS)

  // data-reduction-proxy-lo-fi and enable-data-reduction-proxy-lite-page
  // are only available for Chromium builds and the Canary/Dev/Beta channels.
  if ((!strcmp("data-reduction-proxy-lo-fi", entry.internal_name) ||
       !strcmp("enable-data-reduction-proxy-lite-page", entry.internal_name)) &&
      channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

  // enable-unsafe-webgpu is only available on Dev/Canary channels.
  if (!strcmp("enable-unsafe-webgpu", entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

#if defined(OS_WIN)
  // HDR mode works, but displays everything horribly wrong prior to windows 10.
  if (!strcmp("enable-hdr", entry.internal_name) &&
      base::win::GetVersion() < base::win::Version::WIN10) {
    return true;
  }
#endif  // OS_WIN

  if (!strcmp("dns-over-https", entry.internal_name) &&
      (SystemNetworkContextManager::GetStubResolverConfigReader()
           ->ShouldDisableDohForManaged() ||
       features::kDnsOverHttpsShowUiParam.Get())) {
    return true;
  }

  if (flags::IsFlagExpired(entry.internal_name))
    return true;

  return false;
}

// Records a set of feature switches (prefixed with "--").
void ReportAboutFlagsHistogramSwitches(const std::string& uma_histogram_name,
                                       const std::set<std::string>& switches) {
  for (const std::string& flag : switches) {
    int uma_id = about_flags::testing::kBadSwitchFormatHistogramId;
    if (base::StartsWith(flag, "--", base::CompareCase::SENSITIVE)) {
      // Skip '--' before switch name.
      std::string switch_name(flag.substr(2));

      // Kill value, if any.
      const size_t value_pos = switch_name.find('=');
      if (value_pos != std::string::npos)
        switch_name.resize(value_pos);

      uma_id = GetSwitchUMAId(switch_name);
    } else {
      NOTREACHED() << "ReportAboutFlagsHistogram(): flag '" << flag
                   << "' has incorrect format.";
    }
    DVLOG(1) << "ReportAboutFlagsHistogram(): histogram='" << uma_histogram_name
             << "' '" << flag << "', uma_id=" << uma_id;
    base::UmaHistogramSparse(uma_histogram_name, uma_id);
  }
}

// Records a set of FEATURE_VALUE_TYPE features (suffixed with ":enabled" or
// "disabled", depending on their state).
void ReportAboutFlagsHistogramFeatures(const std::string& uma_histogram_name,
                                       const std::set<std::string>& features) {
  for (const std::string& feature : features) {
    int uma_id = GetSwitchUMAId(feature);
    DVLOG(1) << "ReportAboutFlagsHistogram(): histogram='" << uma_histogram_name
             << "' '" << feature << "', uma_id=" << uma_id;
    base::UmaHistogramSparse(uma_histogram_name, uma_id);
  }
}

}  // namespace

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            flags_ui::SentinelsMode sentinels) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  FlagsStateSingleton::GetFlagsState()->ConvertFlagsToSwitches(
      flags_storage, command_line, sentinels, switches::kEnableFeatures,
      switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  return FlagsStateSingleton::GetFlagsState()
      ->RegisterAllFeatureVariationParameters(flags_storage, feature_list);
}

bool AreSwitchesIdenticalToCurrentCommandLine(
    const base::CommandLine& new_cmdline,
    const base::CommandLine& active_cmdline,
    std::set<base::CommandLine::StringType>* out_difference) {
  const char* extra_flag_sentinel_begin_flag_name = nullptr;
  const char* extra_flag_sentinel_end_flag_name = nullptr;
#if defined(OS_CHROMEOS)
  // Put the flags between --policy-switches--begin and --policy-switches-end on
  // ChromeOS.
  extra_flag_sentinel_begin_flag_name =
      chromeos::switches::kPolicySwitchesBegin;
  extra_flag_sentinel_end_flag_name = chromeos::switches::kPolicySwitchesEnd;
#endif  // OS_CHROMEOS
  return flags_ui::FlagsState::AreSwitchesIdenticalToCurrentCommandLine(
      new_cmdline, active_cmdline, out_difference,
      extra_flag_sentinel_begin_flag_name, extra_flag_sentinel_end_flag_name);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&SkipConditionalFeatureEntry));
}

void GetFlagFeatureEntriesForDeprecatedPage(
    flags_ui::FlagsStorage* flags_storage,
    flags_ui::FlagAccess access,
    base::ListValue* supported_entries,
    base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&ShouldSkipNonDeprecatedFeatureEntry));
}

bool IsRestartNeededToCommitChanges() {
  return FlagsStateSingleton::GetFlagsState()->IsRestartNeededToCommitChanges();
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  FlagsStateSingleton::GetFlagsState()->SetFeatureEntryEnabled(
      flags_storage, internal_name, enable);
}

void SetOriginListFlag(const std::string& internal_name,
                       const std::string& value,
                       flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->SetOriginListFlag(internal_name, value,
                                                          flags_storage);
}

void RemoveFlagsSwitches(base::CommandLine::SwitchMap* switch_list) {
  FlagsStateSingleton::GetFlagsState()->RemoveFlagsSwitches(switch_list);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->ResetAllFlags(flags_storage);
}

void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage) {
  std::set<std::string> switches;
  std::set<std::string> features;
  FlagsStateSingleton::GetFlagsState()->GetSwitchesAndFeaturesFromFlags(
      flags_storage, &switches, &features);
  ReportAboutFlagsHistogram("Launch.FlagsAtStartup", switches, features);
}

base::HistogramBase::Sample GetSwitchUMAId(const std::string& switch_name) {
  return static_cast<base::HistogramBase::Sample>(
      base::HashMetricName(switch_name));
}

void ReportAboutFlagsHistogram(const std::string& uma_histogram_name,
                               const std::set<std::string>& switches,
                               const std::set<std::string>& features) {
  ReportAboutFlagsHistogramSwitches(uma_histogram_name, switches);
  ReportAboutFlagsHistogramFeatures(uma_histogram_name, features);
}

namespace testing {

const base::HistogramBase::Sample kBadSwitchFormatHistogramId = 0;

std::vector<FeatureEntry>* GetEntriesForTesting() {
  static base::NoDestructor<std::vector<FeatureEntry>> entries;
  return entries.get();
}

const FeatureEntry* GetFeatureEntries(size_t* count) {
  if (!GetEntriesForTesting()->empty()) {
    *count = GetEntriesForTesting()->size();
    return GetEntriesForTesting()->data();
  }
  *count = base::size(kFeatureEntries);
  return kFeatureEntries;
}

void SetFeatureEntries(const std::vector<FeatureEntry>& entries) {
  GetEntriesForTesting()->clear();
  for (const auto& entry : entries)
    GetEntriesForTesting()->push_back(entry);
  FlagsStateSingleton::GetInstance()->RebuildState(*GetEntriesForTesting());
}

}  // namespace testing

}  // namespace about_flags
