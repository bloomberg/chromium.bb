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
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/performance_hints/performance_hints_features.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/permissions/abusive_origin_notifications_permission_revocation_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/browser/video_tutorials/switches.h"
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
#include "components/contextual_search/core/browser/public.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/download/public/common/download_features.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_metrics.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/lookalikes/core/features.h"
#include "components/memories/core/memories_features.h"
#include "components/messages/android/messages_feature.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/features.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/permissions/features.h"
#include "components/policy/core/common/features.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/query_tiles/switches.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/core/features.h"
#include "components/search/ntp_features.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/features.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/send_tab_to_self/features.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/site_isolation/features.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "components/translate/core/common/translate_util.h"
#include "components/ui_devtools/switches.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/webapps/common/switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/fido/features.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
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
#include "mojo/core/embedder/features.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/serial/serial_switches.h"
#include "services/media_session/public/cpp/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "storage/browser/quota/quota_features.h"
#include "third_party/blink/public/common/experiments/memory_ablation_experiment.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/blink/public/common/switches.h"
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/allocator/buildflags.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/webapps/android/features.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/browser_ui/site_settings/android/features.h"
#include "components/external_intents/android/external_intents_feature_list.h"
#else  // OS_ANDROID
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#endif  // OS_ANDROID

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_switches.h"
#include "chrome/browser/chromeos/crosapi/browser_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/arc/arc_features.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/events/ozone/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
#include "chrome/browser/ui/browser_dialogs.h"
#endif  // OS_MAC

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

#if defined(USE_AURA)
const unsigned kOsAura = kOsWin | kOsLinux | kOsCrOS;
#endif  // USE_AURA

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
     blink::switches::kTouchTextSelectionStrategy, "character"},
    {flag_descriptions::kTouchSelectionStrategyDirection,
     blink::switches::kTouchTextSelectionStrategy, "direction"}};

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
     blink::switches::kPassiveListenersDefault, "true"},
    {flag_descriptions::kPassiveEventListenerForceAllTrue,
     blink::switches::kPassiveListenersDefault, "forcealltrue"},
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

const FeatureEntry::Choice kLiteVideoDefaultDownlinkBandwidthKbps[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"100", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "100"},
    {"150", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "150"},
    {"200", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "200"},
    {"250", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "250"},
    {"300", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "300"},
    {"350", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "350"},
    {"400", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "400"},
    {"450", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "450"},
    {"500", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "500"},
    {"600", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "600"},
    {"700", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "700"},
    {"800", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "800"},
    {"900", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "900"}};

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
#else   // !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kReaderModeOfferInSettings[] = {
    {switches::kReaderModeDiscoverabilityParamName,
     switches::kReaderModeOfferInSettings}};

const FeatureEntry::FeatureVariation kReaderModeDiscoverabilityVariations[] = {
    {"available in settings", kReaderModeOfferInSettings,
     base::size(kReaderModeOfferInSettings), nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureVariation kAdaptiveButtonInTopToolbarVariations[] = {
    {
        "Always None",
        (FeatureEntry::FeatureParam[]){{"mode", "always-none"}},
        1,
        nullptr,
    },
    {
        "Always New Tab",
        (FeatureEntry::FeatureParam[]){{"mode", "always-new-tab"}},
        1,
        nullptr,
    },
    {
        "Always Share",
        (FeatureEntry::FeatureParam[]){{"mode", "always-share"}},
        1,
        nullptr,
    },
    {
        "Always Voice",
        (FeatureEntry::FeatureParam[]){{"mode", "always-voice"}},
        1,
        nullptr,
    },
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kHideDismissButton[] = {
    {"dismiss_button", "hide"}};

const FeatureEntry::FeatureParam kSuppressBottomSheet[] = {
    {"consecutive_active_dismissal_limit", "3"}};

const FeatureEntry::FeatureVariation kMobileIdentityConsistencyVariations[] = {
    {"Hide Dismiss Button", kHideDismissButton, base::size(kHideDismissButton),
     nullptr},
    {"Suppress Bottom Sheet", kSuppressBottomSheet,
     base::size(kSuppressBottomSheet), nullptr},
};
#endif  // OS_ANDROID

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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

const FeatureEntry::FeatureParam kMBIModeLegacy[] = {{"mode", "legacy"}};
const FeatureEntry::FeatureParam kMBIModeEnabledPerRenderProcessHost[] = {
    {"mode", "per_render_process_host"}};
const FeatureEntry::FeatureParam kMBIModeEnabledPerSiteInstance[] = {
    {"mode", "per_site_instance"}};

const FeatureEntry::FeatureVariation kMBIModeVariations[] = {
    {"legacy mode", kMBIModeLegacy, base::size(kMBIModeLegacy), nullptr},
    {"per render process host", kMBIModeEnabledPerRenderProcessHost,
     base::size(kMBIModeEnabledPerRenderProcessHost), nullptr},
    {"per site instance", kMBIModeEnabledPerSiteInstance,
     base::size(kMBIModeEnabledPerSiteInstance), nullptr}};

const FeatureEntry::FeatureParam
    kDelayCompetingLowPriorityRequestsAggressiveFirstPaint[] = {
        {"until", "first_paint"},
        {"priority_threshold", "medium"}};
const FeatureEntry::FeatureParam
    kDelayCompetingLowPriorityRequestsAggressiveFirstContentfulPaint[] = {
        {"until", "first_contentful_paint"},
        {"priority_threshold", "medium"}};
const FeatureEntry::FeatureParam
    kDelayCompetingLowPriorityRequestsRelaxedFirstPaint[] = {
        {"until", "first_paint"},
        {"priority_threshold", "high"}};
const FeatureEntry::FeatureParam
    kDelayCompetingLowPriorityRequestsRelaxedFirstContentfulPaint[] = {
        {"until", "first_contentful_paint"},
        {"priority_threshold", "high"}};
const FeatureEntry::FeatureParam
    kDelayCompetingLowPriorityRequestsRelaxedAlways[] = {
        {"until", "always"},
        {"priority_threshold", "high"}};

const FeatureEntry::FeatureVariation
    kDelayCompetingLowPriorityRequestsFeatureVariations[] = {
        {"behind medium priority, until first paint",
         kDelayCompetingLowPriorityRequestsAggressiveFirstPaint,
         base::size(kDelayCompetingLowPriorityRequestsAggressiveFirstPaint),
         nullptr},
        {"behind medium priority, until first contentful paint",
         kDelayCompetingLowPriorityRequestsAggressiveFirstContentfulPaint,
         base::size(
             kDelayCompetingLowPriorityRequestsAggressiveFirstContentfulPaint),
         nullptr},
        {"behind high priority, until first paint",
         kDelayCompetingLowPriorityRequestsRelaxedFirstPaint,
         base::size(kDelayCompetingLowPriorityRequestsRelaxedFirstPaint),
         nullptr},
        {"behind high priority, until first contentful paint",
         kDelayCompetingLowPriorityRequestsRelaxedFirstContentfulPaint,
         base::size(
             kDelayCompetingLowPriorityRequestsRelaxedFirstContentfulPaint),
         nullptr},
        {"behind high priority, always",
         kDelayCompetingLowPriorityRequestsRelaxedAlways,
         base::size(kDelayCompetingLowPriorityRequestsRelaxedAlways), nullptr}};

const FeatureEntry::FeatureParam kIntensiveWakeUpThrottlingAfter10Seconds[] = {
    {blink::features::kIntensiveWakeUpThrottling_GracePeriodSeconds_Name,
     "10"}};

const FeatureEntry::FeatureVariation kIntensiveWakeUpThrottlingVariations[] = {
    {"10 seconds after a tab is hidden (facilitates testing)",
     kIntensiveWakeUpThrottlingAfter10Seconds,
     base::size(kIntensiveWakeUpThrottlingAfter10Seconds), nullptr},
};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_Immediate[] = {
    {"baseline_tab_suggestions", "true"},
    {"baseline_close_tab_suggestions", "true"}};
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
const FeatureEntry::FeatureParam kGroupAndCloseTabSuggestions_Immediate[] = {
    {"baseline_tab_suggestions", "true"},
    {"baseline_group_tab_suggestions", "true"},
    {"baseline_close_tab_suggestions", "true"}};

const FeatureEntry::FeatureVariation kCloseTabSuggestionsStaleVariations[] = {
    {"Close Immediate", kCloseTabSuggestionsStale_Immediate,
     base::size(kCloseTabSuggestionsStale_Immediate), nullptr},
    {"Group+Close Immediate", kGroupAndCloseTabSuggestions_Immediate,
     base::size(kGroupAndCloseTabSuggestions_Immediate), nullptr},
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kLacrosSupportInternalName[] = "lacros-support";
const char kLacrosStabilityInternalName[] = "lacros-stability";

const FeatureEntry::Choice kLacrosStabilityChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kLacrosStabilityLessStableDescription,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityLessStable},
    {flag_descriptions::kLacrosStabilityMoreStableDescription,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityMoreStable},
};

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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kCrosRegionsModeChoices[] = {
    {flag_descriptions::kCrosRegionsModeDefault, "", ""},
    {flag_descriptions::kCrosRegionsModeOverride,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeOverride},
    {flag_descriptions::kCrosRegionsModeHide,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeHide},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam
    kDesktopPWAsAttentionBadgingCrOSApiAndNotifications[] = {
        {"badge-source",
         switches::kDesktopPWAsAttentionBadgingCrOSApiAndNotifications}};
const FeatureEntry::FeatureParam kDesktopPWAsAttentionBadgingCrOSApiOnly[] = {
    {"badge-source", switches::kDesktopPWAsAttentionBadgingCrOSApiOnly}};
const FeatureEntry::FeatureParam
    kDesktopPWAsAttentionBadgingCrOSNotificationsOnly[] = {
        {"badge-source",
         switches::kDesktopPWAsAttentionBadgingCrOSNotificationsOnly}};

const FeatureEntry::FeatureVariation
    kDesktopPWAsAttentionBadgingCrOSVariations[] = {
        {flag_descriptions::kDesktopPWAsAttentionBadgingCrOSApiAndNotifications,
         kDesktopPWAsAttentionBadgingCrOSApiAndNotifications,
         base::size(kDesktopPWAsAttentionBadgingCrOSApiAndNotifications),
         nullptr},
        {flag_descriptions::kDesktopPWAsAttentionBadgingCrOSApiOnly,
         kDesktopPWAsAttentionBadgingCrOSApiOnly,
         base::size(kDesktopPWAsAttentionBadgingCrOSApiOnly), nullptr},
        {flag_descriptions::kDesktopPWAsAttentionBadgingCrOSNotificationsOnly,
         kDesktopPWAsAttentionBadgingCrOSNotificationsOnly,
         base::size(kDesktopPWAsAttentionBadgingCrOSNotificationsOnly),
         nullptr},
};

const FeatureEntry::Choice kSchedulerConfigurationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kSchedulerConfigurationConservative,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationConservative},
    {flag_descriptions::kSchedulerConfigurationPerformance,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationPerformance},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
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

// 3 permutations of the 2 rich autocompletion params:
// - Title AC: Autocompletes suggestions when the input matches the title.
//   E.g. Space Sh | [ttle - Wikipedia] (en.wikipedia.org/wiki/Space_Shuttle)
// - Non-Prefix AC: Autocompletes suggestions when the input is not necessarily
//   a prefix.
//   E.g. [en.wikipe dia.org/] wiki/Spac | [e_Shuttle] (Space Shuttle -
//   Wikipedia)
const FeatureEntry::FeatureVariation kOmniboxRichAutocompletionVariations[] = {
    {
        "Title AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteTitles", "true"}},
        1,
        nullptr,
    },
    {
        "Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"}},
        1,
        nullptr,
    },
    {
        "Title AC & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"}},
        2,
        nullptr,
    }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionMinCharVariations[] = {
        {
            "Title 0 / Non Prefix 0",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "0"}},
            2,
            nullptr,
        },
        {
            "Title 0 / Non Prefix 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "3"}},
            2,
            nullptr,
        },
        {
            "Title 0 / Non Prefix 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            2,
            nullptr,
        },
        {
            "Title 3 / Non Prefix 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "3"}},
            2,
            nullptr,
        },
        {
            "Title 3 / Non Prefix 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            2,
            nullptr,
        },
        {
            "Title 5 / Non Prefix 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "5"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            2,
            nullptr,
        }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionShowAdditionalTextVariations[] = {
        {
            "Show Additional Text",
            (FeatureEntry::FeatureParam[]){},
            0,
            nullptr,
        },
        {
            "Hide Additional Text",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteShowAdditionalText", "false"}},
            1,
            nullptr,
        }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionSplitVariations[] = {
        {
            "Titles & URLs, min char 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitUrlCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "5"}},
            3,
            nullptr,
        },
        {
            "Titles & URLs, min char 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitUrlCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "3"}},
            3,
            nullptr,
        },
        {
            "Titles, min char 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "5"}},
            2,
            nullptr,
        },
        {
            "Titles, min char 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "3"}},
            2,
            nullptr,
        }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionPreferUrlsOverPrefixesVariations[] = {
        {
            "Prefer prefixes",
            (FeatureEntry::FeatureParam[]){},
            0,
            nullptr,
        },
        {
            "Prefer URLs",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompletePreferUrlsOverPrefixes",
                 "true"}},
            1,
            nullptr,
        }};

// A limited number of combinations of the above variations that are most
// promising.
const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionPromisingVariations[] = {
        {
            "Aggressive - Title, Non-Prefix, min 0/0",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteNonPrefixAll", "true"}},
            2,
            nullptr,
        },
        {
            "Aggressive Moderate - Title, Non-Prefix, min 3/5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            4,
            nullptr,
        },
        {
            "Conservative Moderate - Title, Shortcut Non-Prefix, min 3/5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteNonPrefixShortcutProvider",
                 "true"},
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            4,
            nullptr,
        },
        {
            "Conservative - Title, min 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"}},
            2,
            nullptr,
        },
};

const FeatureEntry::FeatureVariation kOmniboxBookmarkPathsVariations[] = {
    {
        "Default UI (Title - URL)",
        (FeatureEntry::FeatureParam[]){},
        0,
        nullptr,
    },
    {
        "Replace title (Path/Title - URL)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiReplaceTitle", "true"}},
        1,
        nullptr,
    },
    {
        "Replace URL (Title - Path)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiReplaceUrl", "true"}},
        1,
        nullptr,
    },
    {
        "Append after title (Title : Path - URL)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiAppendAfterTitle", "true"}},
        1,
        nullptr,
    },
    {
        "Dynamic Replace URL (Title - Path|URL)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiDynamicReplaceUrl", "true"}},
        1,
        nullptr,
    },
};

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) ||
        // defined(OS_WIN)

const FeatureEntry::FeatureVariation
    kOmniboxOnFocusSuggestionsContextualWebVariations[] = {
        {"GOC Only", {}, 0, "t3317583"},
        {"pSuggest Only", {}, 0, "t3318055"},
        {"GOC, pSuggest Fallback", {}, 0, "t3317692"},
        {"GOC, pSuggest Backfill", {}, 0, "t3317694"},
        {"GOC, Default Hidden", {}, 0, "t3317834"},
};

const FeatureEntry::FeatureVariation kMaxZeroSuggestMatchesVariations[] = {
    {
        "5",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "5"}},
        1,
        nullptr,
    },
    {
        "6",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "6"}},
        1,
        nullptr,
    },
    {
        "7",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "7"}},
        1,
        nullptr,
    },
    {
        "8",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "8"}},
        1,
        nullptr,
    },
    {
        "9",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "9"}},
        1,
        nullptr,
    },
    {
        "10",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "10"}},
        1,
        nullptr,
    },
    {
        "11",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "11"}},
        1,
        nullptr,
    },
    {
        "12",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "12"}},
        1,
        nullptr,
    },
    {
        "13",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "13"}},
        1,
        nullptr,
    },
    {
        "14",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "14"}},
        1,
        nullptr,
    },
    {
        "15",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "15"}},
        1,
        nullptr,
    },
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

const FeatureEntry::FeatureParam
    kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout3s[] = {
        {omnibox::kDefaultTypedNavigationsToHttpsTimeoutParam, "3s"}};
const FeatureEntry::FeatureParam
    kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout10s[] = {
        {omnibox::kDefaultTypedNavigationsToHttpsTimeoutParam, "10s"}};
const FeatureEntry::FeatureVariation
    kOmniboxDefaultTypedNavigationsToHttpsVariations[] = {
        {"3 second timeout",
         kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout3s,
         base::size(kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout3s),
         nullptr},
        {"10 second timeout",
         kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout10s,
         base::size(kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout10s),
         nullptr},
};

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

const FeatureEntry::FeatureVariation
    kOmniboxDynamicMaxAutocompleteVariations[] = {
        {
            "9 suggestions if 0 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "0"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}},
            2,
            nullptr,
        },
        {
            "9 suggestions if 1 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "1"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}},
            2,
            nullptr,
        },
        {
            "9 suggestions if 2 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "2"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}},
            2,
            nullptr,
        },
        {
            "10 suggestions if 0 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "0"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}},
            2,
            nullptr,
        },
        {
            "10 suggestions if 1 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "1"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}},
            2,
            nullptr,
        },
        {
            "10 suggestions if 2 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "2"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}},
            2,
            nullptr,
        }};

const FeatureEntry::FeatureVariation kOmniboxBubbleUrlSuggestionsVariations[] =
    {{
         "Gap 200, Buffer 100",
         (FeatureEntry::FeatureParam[]){
             {"OmniboxBubbleUrlSuggestionsAbsoluteGap", "200"},
             {"OmniboxBubbleUrlSuggestionsRelativeGap", "1"},
             {"OmniboxBubbleUrlSuggestionsAbsoluteBuffer", "100"},
             {"OmniboxBubbleUrlSuggestionsRelativeBuffer", "1"}},
         4,
         nullptr,
     },
     {
         "Gap 200, Buffer 200",
         (FeatureEntry::FeatureParam[]){
             {"OmniboxBubbleUrlSuggestionsAbsoluteGap", "200"},
             {"OmniboxBubbleUrlSuggestionsRelativeGap", "1"},
             {"OmniboxBubbleUrlSuggestionsAbsoluteBuffer", "200"},
             {"OmniboxBubbleUrlSuggestionsRelativeBuffer", "1"}},
         4,
         nullptr,
     },
     {
         "Gap 400, Buffer 200",
         (FeatureEntry::FeatureParam[]){
             {"OmniboxBubbleUrlSuggestionsAbsoluteGap", "400"},
             {"OmniboxBubbleUrlSuggestionsRelativeGap", "1"},
             {"OmniboxBubbleUrlSuggestionsAbsoluteBuffer", "200"},
             {"OmniboxBubbleUrlSuggestionsRelativeBuffer", "1"}},
         4,
         nullptr,
     }};

// The "Enabled" state for this feature is "0" and representing setting A.
const FeatureEntry::FeatureParam kTabHoverCardsSettingB[] = {
    {features::kTabHoverCardsFeatureParameterName, "1"}};
const FeatureEntry::FeatureParam kTabHoverCardsSettingC[] = {
    {features::kTabHoverCardsFeatureParameterName, "2"}};

const FeatureEntry::FeatureVariation kTabHoverCardsFeatureVariations[] = {
    {"B", kTabHoverCardsSettingB, base::size(kTabHoverCardsSettingB), nullptr},
    {"C", kTabHoverCardsSettingC, base::size(kTabHoverCardsSettingC), nullptr}};

const FeatureEntry::FeatureParam kMinimumTabWidthSettingPinned[] = {
    {features::kMinimumTabWidthFeatureParameterName, "54"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingMedium[] = {
    {features::kMinimumTabWidthFeatureParameterName, "72"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingLarge[] = {
    {features::kMinimumTabWidthFeatureParameterName, "140"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingFull[] = {
    {features::kMinimumTabWidthFeatureParameterName, "256"}};

const FeatureEntry::FeatureVariation kTabScrollingVariations[] = {
    {" - tabs shrink to pinned tab width", kMinimumTabWidthSettingPinned,
     base::size(kMinimumTabWidthSettingPinned), nullptr},
    {" - tabs shrink to a medium width", kMinimumTabWidthSettingMedium,
     base::size(kMinimumTabWidthSettingMedium), nullptr},
    {" - tabs shrink to a large width", kMinimumTabWidthSettingLarge,
     base::size(kMinimumTabWidthSettingLarge), nullptr},
    {" - tabs do not shrink", kMinimumTabWidthSettingFull,
     base::size(kMinimumTabWidthSettingFull), nullptr}};

const FeatureEntry::FeatureParam kPromoBrowserCommandUnknownCommandParam[] = {
    {features::kPromoBrowserCommandIdParam, "0"}};
const FeatureEntry::FeatureParam
    kPromoBrowserCommandOpenSafetyCheckCommandParam[] = {
        {features::kPromoBrowserCommandIdParam, "1"}};
const FeatureEntry::FeatureParam
    kPromoBrowserCommandOpenSafeBrowsingSettingsEnhancedProtectionCommandParam
        [] = {{features::kPromoBrowserCommandIdParam, "2"}};
const FeatureEntry::FeatureVariation kPromoBrowserCommandsVariations[] = {
    {"- Unknown Command", kPromoBrowserCommandUnknownCommandParam,
     base::size(kPromoBrowserCommandUnknownCommandParam), nullptr},
    {"- Open Safety Check", kPromoBrowserCommandOpenSafetyCheckCommandParam,
     base::size(kPromoBrowserCommandOpenSafetyCheckCommandParam), nullptr},
    {"- Open Safe Browsing Enhanced Protection Settings",
     kPromoBrowserCommandOpenSafeBrowsingSettingsEnhancedProtectionCommandParam,
     base::size(
         kPromoBrowserCommandOpenSafeBrowsingSettingsEnhancedProtectionCommandParam),
     nullptr}};
#if !defined(OS_ANDROID)

const FeatureEntry::FeatureParam kNtpChromeCartModuleFakeData[] = {
    {ntp_features::kNtpChromeCartModuleDataParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpChromeCartModuleVariations[] = {
    {"- Fake Data", kNtpChromeCartModuleFakeData,
     base::size(kNtpChromeCartModuleFakeData), nullptr},
};

const FeatureEntry::FeatureParam kNtpRecipeTasksModuleFakeData[] = {
    {ntp_features::kNtpStatefulTasksModuleDataParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpRecipeTasksModuleVariations[] = {
    {"- Fake Data", kNtpRecipeTasksModuleFakeData,
     base::size(kNtpRecipeTasksModuleFakeData), nullptr},
};

const FeatureEntry::FeatureParam kNtpShoppingTasksModuleFakeData[] = {
    {ntp_features::kNtpStatefulTasksModuleDataParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpShoppingTasksModuleVariations[] = {
    {"- Fake Data", kNtpShoppingTasksModuleFakeData,
     base::size(kNtpShoppingTasksModuleFakeData),
     "t3329139" /* variation_id */},
};

const FeatureEntry::FeatureParam kNtpRepeatableQueriesInsertPositionStart[] = {
    {ntp_features::kNtpRepeatableQueriesInsertPositionParam, "start"}};
const FeatureEntry::FeatureParam kNtpRepeatableQueriesInsertPositionEnd[] = {
    {ntp_features::kNtpRepeatableQueriesInsertPositionParam, "end"}};
const FeatureEntry::FeatureVariation kNtpRepeatableQueriesVariations[] = {
    {"- Start", kNtpRepeatableQueriesInsertPositionStart,
     base::size(kNtpRepeatableQueriesInsertPositionStart),
     "t3317864" /* variation_id */},
    {"- End", kNtpRepeatableQueriesInsertPositionEnd,
     base::size(kNtpRepeatableQueriesInsertPositionEnd),
     "t3317864" /* variation_id */},
};
#endif  // !defined(OS_ANDROID)

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
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kOverridePrefsForHrefTranslateForceAuto[] = {
    {translate::kForceAutoTranslateKey, "true"}};

const FeatureEntry::FeatureVariation
    kOverrideLanguagePrefsForHrefTranslateVariations[] = {
        {"(Force automatic translation of blocked languages for hrefTranslate)",
         kOverridePrefsForHrefTranslateForceAuto,
         base::size(kOverridePrefsForHrefTranslateForceAuto), nullptr}};

const FeatureEntry::FeatureVariation
    kOverrideSitePrefsForHrefTranslateVariations[] = {
        {"(Force automatic translation of blocked sites for hrefTranslate)",
         kOverridePrefsForHrefTranslateForceAuto,
         base::size(kOverridePrefsForHrefTranslateForceAuto), nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kEphemeralTabOpenPeek[] = {
    {"ephemeral_tab_open_mode", "0"}};
const FeatureEntry::FeatureParam kEphemeralTabOpenHalf[] = {
    {"ephemeral_tab_open_mode", "1"}};
const FeatureEntry::FeatureParam kEphemeralTabOpenFull[] = {
    {"ephemeral_tab_open_mode", "2"}};
const FeatureEntry::FeatureVariation kEphemeralTabOpenVariations[] = {
    {"Open at peek state", kEphemeralTabOpenPeek,
     base::size(kEphemeralTabOpenPeek), nullptr},
    {"Open at half state", kEphemeralTabOpenHalf,
     base::size(kEphemeralTabOpenHalf), nullptr},
    {"Open at full state", kEphemeralTabOpenFull,
     base::size(kEphemeralTabOpenFull), nullptr}};
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
    {"predictor", features::kPredictorNameLsq}};
const FeatureEntry::FeatureParam kResamplingInputEventsKalmanEnabled[] = {
    {"predictor", features::kPredictorNameKalman}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearFirstEnabled[] = {
    {"predictor", features::kPredictorNameLinearFirst}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearSecondEnabled[] = {
    {"predictor", features::kPredictorNameLinearSecond}};
const FeatureEntry::FeatureParam
    kResamplingInputEventsLinearResamplingEnabled[] = {
        {"predictor", features::kPredictorNameLinearResampling}};

const FeatureEntry::FeatureVariation kResamplingInputEventsFeatureVariations[] =
    {{features::kPredictorNameLsq, kResamplingInputEventsLSQEnabled,
      base::size(kResamplingInputEventsLSQEnabled), nullptr},
     {features::kPredictorNameKalman, kResamplingInputEventsKalmanEnabled,
      base::size(kResamplingInputEventsKalmanEnabled), nullptr},
     {features::kPredictorNameLinearFirst,
      kResamplingInputEventsLinearFirstEnabled,
      base::size(kResamplingInputEventsLinearFirstEnabled), nullptr},
     {features::kPredictorNameLinearSecond,
      kResamplingInputEventsLinearSecondEnabled,
      base::size(kResamplingInputEventsLinearSecondEnabled), nullptr},
     {features::kPredictorNameLinearResampling,
      kResamplingInputEventsLinearResamplingEnabled,
      base::size(kResamplingInputEventsLinearResamplingEnabled), nullptr}};

const FeatureEntry::FeatureParam
    kResamplingScrollEventsPredictionTimeBasedEnabled[] = {
        {"mode", features::kPredictionTypeTimeBased},
        {"latency", features::kPredictionTypeDefaultTime}};
const FeatureEntry::FeatureParam
    kResamplingScrollEventsPredictionFramesBasedEnabled[] = {
        {"mode", features::kPredictionTypeFramesBased},
        {"latency", features::kPredictionTypeDefaultFramesRatio}};
const FeatureEntry::FeatureVariation
    kResamplingScrollEventsExperimentalPredictionVariations[] = {
        {features::kPredictionTypeTimeBased,
         kResamplingScrollEventsPredictionTimeBasedEnabled,
         base::size(kResamplingScrollEventsPredictionTimeBasedEnabled),
         nullptr},
        {features::kPredictionTypeFramesBased,
         kResamplingScrollEventsPredictionFramesBasedEnabled,
         base::size(kResamplingScrollEventsPredictionFramesBasedEnabled),
         nullptr}};

const FeatureEntry::FeatureParam kFilteringPredictionEmptyFilterEnabled[] = {
    {"filter", features::kFilterNameEmpty}};
const FeatureEntry::FeatureParam kFilteringPredictionOneEuroFilterEnabled[] = {
    {"filter", features::kFilterNameOneEuro}};

const FeatureEntry::FeatureVariation kFilteringPredictionFeatureVariations[] = {
    {features::kFilterNameEmpty, kFilteringPredictionEmptyFilterEnabled,
     base::size(kFilteringPredictionEmptyFilterEnabled), nullptr},
    {features::kFilterNameOneEuro, kFilteringPredictionOneEuroFilterEnabled,
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
const FeatureEntry::FeatureParam kTabGridLayoutAndroid_NewTabVariation[] = {
    {"tab_grid_layout_android_new_tab", "NewTabVariation"},
    {"allow_to_refetch", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_NewTabTile[] = {
    {"tab_grid_layout_android_new_tab_tile", "NewTabTile"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_TallNTV[] = {
    {"thumbnail_aspect_ratio", "0.85"},
    {"allow_to_refetch", "true"},
    {"tab_grid_layout_android_new_tab", "NewTabVariation"},
    {"enable_launch_polish", "true"},
    {"enable_launch_bug_fix", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_SearchChip[] = {
    {"enable_search_term_chip", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_PriceAlerts[] = {
    {"enable_price_tracking", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_PriceNotifications[] = {
    {"enable_price_notification", "true"}};

const FeatureEntry::FeatureVariation kTabGridLayoutAndroidVariations[] = {
    {"New Tab Variation", kTabGridLayoutAndroid_NewTabVariation,
     base::size(kTabGridLayoutAndroid_NewTabVariation), nullptr},
    {"New Tab Tile", kTabGridLayoutAndroid_NewTabTile,
     base::size(kTabGridLayoutAndroid_NewTabTile), nullptr},
    {"Tall NTV", kTabGridLayoutAndroid_TallNTV,
     base::size(kTabGridLayoutAndroid_TallNTV), nullptr},
    {"Search term chip", kTabGridLayoutAndroid_SearchChip,
     base::size(kTabGridLayoutAndroid_SearchChip), nullptr},
    {"Price alerts", kTabGridLayoutAndroid_PriceAlerts,
     base::size(kTabGridLayoutAndroid_PriceAlerts), nullptr},
    {"Price notifications", kTabGridLayoutAndroid_PriceNotifications,
     base::size(kTabGridLayoutAndroid_PriceNotifications), nullptr},
};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface[] = {
    {"start_surface_variation", "single"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurfaceFinale[] = {
    {"start_surface_variation", "single"},
    {"omnibox_focused_on_new_tab", "true"},
    {"home_button_on_grid_tab_switcher", "true"},
    {"new_home_surface_from_home_button", "hide_tab_switcher_only"},
    {"hide_switch_when_no_incognito_tabs", "true"},
    {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"exclude_mv_tiles", "true"},
    {"open_ntp_instead_of_start", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2Finale[] =
    {{"start_surface_variation", "single"},
     {"show_last_active_tab_only", "true"},
     {"omnibox_focused_on_new_tab", "true"},
     {"home_button_on_grid_tab_switcher", "true"},
     {"new_home_surface_from_home_button", "hide_tab_switcher_only"},
     {"enable_tab_groups_continuation", "true"}};

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
    kStartSurfaceAndroid_SingleSurfaceSingleTabWithoutMvTiles[] = {
        {"start_surface_variation", "single"},
        {"show_last_active_tab_only", "true"},
        {"exclude_mv_tiles", "true"}};

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

const FeatureEntry::FeatureParam kStartSurfaceAndroid_TrendyTerms[] = {
    {"start_surface_variation", "trendyterms"},
    {"trendy_enabled", "true"},
    {"trendy_success_min_period_ms", "30000"},
    {"trendy_failure_min_period_ms", "10000"},
    {"omnibox_scroll_mode", "quick"},
    {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureVariation kStartSurfaceAndroidVariations[] = {
    {"Single Surface", kStartSurfaceAndroid_SingleSurface,
     base::size(kStartSurfaceAndroid_SingleSurface), nullptr},
    {"Single Surface Finale", kStartSurfaceAndroid_SingleSurfaceFinale,
     base::size(kStartSurfaceAndroid_SingleSurfaceFinale), nullptr},
    {"Single Surface V2", kStartSurfaceAndroid_SingleSurface_V2,
     base::size(kStartSurfaceAndroid_SingleSurface_V2), nullptr},
    {"Single Surface V2 Finale", kStartSurfaceAndroid_SingleSurface_V2Finale,
     base::size(kStartSurfaceAndroid_SingleSurface_V2Finale), nullptr},
    {"Single Surface without MV Tiles",
     kStartSurfaceAndroid_SingleSurfaceWithoutMvTiles,
     base::size(kStartSurfaceAndroid_SingleSurfaceWithoutMvTiles), nullptr},
    {"Single Surface + Single Tab", kStartSurfaceAndroid_SingleSurfaceSingleTab,
     base::size(kStartSurfaceAndroid_SingleSurfaceSingleTab), nullptr},
    {"Single Surface + Single Tab without MV Tiles",
     kStartSurfaceAndroid_SingleSurfaceSingleTabWithoutMvTiles,
     base::size(kStartSurfaceAndroid_SingleSurfaceSingleTabWithoutMvTiles),
     nullptr},
    {"Two Panes Surface", kStartSurfaceAndroid_TwoPanesSurface,
     base::size(kStartSurfaceAndroid_TwoPanesSurface), nullptr},
    {"Tasks Only", kStartSurfaceAndroid_TasksOnly,
     base::size(kStartSurfaceAndroid_TasksOnly), nullptr},
    {"Omnibox Only", kStartSurfaceAndroid_OmniboxOnly,
     base::size(kStartSurfaceAndroid_OmniboxOnly), nullptr},
    {"Omnibox Only, Quick", kStartSurfaceAndroid_OmniboxOnly_Quick,
     base::size(kStartSurfaceAndroid_OmniboxOnly_Quick), nullptr},
    {"Omnibox Only, Pinned", kStartSurfaceAndroid_OmniboxOnly_Pinned,
     base::size(kStartSurfaceAndroid_OmniboxOnly_Pinned), nullptr},
    {"Trendy Terms, Quick", kStartSurfaceAndroid_TrendyTerms,
     base::size(kStartSurfaceAndroid_TrendyTerms), nullptr}};

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
const FeatureEntry::FeatureParam kHomepagePromoCardSuppressing[] = {
    {"promo-card-variation", "Compact"},
    {"suppressing_sign_in_promo", "true"}};

const FeatureEntry::FeatureVariation kHomepagePromoCardVariations[] = {
    {"Large", kHomepagePromoCardLarge, base::size(kHomepagePromoCardLarge),
     nullptr},
    {"Compact", kHomepagePromoCardCompact,
     base::size(kHomepagePromoCardCompact), nullptr},
    {"Slim", kHomepagePromoCardSlim, base::size(kHomepagePromoCardSlim),
     nullptr},
    {"Compact_SuppressingSignInPromo", kHomepagePromoCardSuppressing,
     base::size(kHomepagePromoCardSuppressing), nullptr}};
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kContextMenuShopSimilarProducts[] = {
    {"lensShopVariation", "ShopSimilarProducts"}};
const FeatureEntry::FeatureParam kContextMenuShopImageWithGoogleLens[] = {
    {"lensShopVariation", "ShopImageWithGoogleLens"}};
const FeatureEntry::FeatureParam kContextMenuSearchSimilarProducts[] = {
    {"lensShopVariation", "SearchSimilarProducts"}};

const FeatureEntry::FeatureVariation
    kContextMenuShopWithGoogleLensShopVariations[] = {
        {"ShopSimilarProducts", kContextMenuShopSimilarProducts,
         base::size(kContextMenuShopSimilarProducts), nullptr},
        {"ShopImageWithGoogleLens", kContextMenuShopImageWithGoogleLens,
         base::size(kContextMenuShopImageWithGoogleLens), nullptr},
        {"SearchSimilarProducts", kContextMenuSearchSimilarProducts,
         base::size(kContextMenuSearchSimilarProducts), nullptr}};
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
    {"colorful_mic", "false"}};

const FeatureEntry::FeatureParam kOmniboxAssistantVoiceSearchColorfulMic[] = {
    {"min_agsa_version", "10.95"},
    {"colorful_mic", "true"}};

const FeatureEntry::FeatureParam
    kOmniboxAssistantVoiceSearchNoMultiAccountCheck[] = {
        {"min_agsa_version", "10.95"},
        {"colorful_mic", "true"},
        {"enable_multi_account_check", "false"}};

const FeatureEntry::FeatureVariation kOmniboxAssistantVoiceSearchVariations[] =
    {
        {"(grey mic)", kOmniboxAssistantVoiceSearchGreyMic,
         base::size(kOmniboxAssistantVoiceSearchGreyMic), nullptr},
        {"(colorful mic)", kOmniboxAssistantVoiceSearchColorfulMic,
         base::size(kOmniboxAssistantVoiceSearchColorfulMic), nullptr},
        {"(no account check)", kOmniboxAssistantVoiceSearchNoMultiAccountCheck,
         base::size(kOmniboxAssistantVoiceSearchNoMultiAccountCheck), nullptr},
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
    kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails[] = {
        {"animate_thumbnails", "true"}};
const FeatureEntry::FeatureVariation
    kPhotoPickerVideoSupportFeatureVariations[] = {
        {"(with animated thumbnails)",
         kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails,
         base::size(kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails),
         nullptr}};

const FeatureEntry::FeatureParam kTabbedAppOverflowMenuRegroupBackward[] = {
    {"action_bar", "backward_button"}};
const FeatureEntry::FeatureParam kTabbedAppOverflowMenuRegroupShare[] = {
    {"action_bar", "share_button"}};
const FeatureEntry::FeatureVariation kTabbedAppOverflowMenuRegroupVariations[] =
    {{"(backward button)", kTabbedAppOverflowMenuRegroupBackward,
      base::size(kTabbedAppOverflowMenuRegroupBackward), nullptr},
     {"(share button)", kTabbedAppOverflowMenuRegroupShare,
      base::size(kTabbedAppOverflowMenuRegroupShare), nullptr}};
const FeatureEntry::FeatureParam
    kTabbedAppOverflowMenuThreeButtonActionbarAction[] = {
        {"three_button_action_bar", "action_chip_view"}};
const FeatureEntry::FeatureParam
    kTabbedAppOverflowMenuThreeButtonActionbarDestination[] = {
        {"three_button_action_bar", "destination_chip_view"}};
const FeatureEntry::FeatureParam
    kTabbedAppOverflowMenuThreeButtonAddToOption[] = {
        {"three_button_action_bar", "add_to_option"}};
const FeatureEntry::FeatureVariation
    kTabbedAppOverflowMenuThreeButtonActionbarVariations[] = {
        {"(three button with action chip view)",
         kTabbedAppOverflowMenuThreeButtonActionbarAction,
         base::size(kTabbedAppOverflowMenuThreeButtonActionbarAction), nullptr},
        {"(three button with destination chip view)",
         kTabbedAppOverflowMenuThreeButtonActionbarDestination,
         base::size(kTabbedAppOverflowMenuThreeButtonActionbarDestination),
         nullptr},
        {"(three button with add to option)",
         kTabbedAppOverflowMenuThreeButtonAddToOption,
         base::size(kTabbedAppOverflowMenuThreeButtonAddToOption), nullptr}};

// Request Desktop Site on Tablet by default variations.
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets768[] = {
    {"screen_width_dp", "768"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets960[] = {
    {"screen_width_dp", "960"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets1024[] = {
    {"screen_width_dp", "1024"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets1280[] = {
    {"screen_width_dp", "1280"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets1920[] = {
    {"screen_width_dp", "1920"},
    {"enabled", "true"}};
const FeatureEntry::FeatureVariation kRequestDesktopSiteForTabletsVariations[] =
    {{"for 768dp+ screens", kRequestDesktopSiteForTablets768,
      base::size(kRequestDesktopSiteForTablets768), nullptr},
     {"for 960dp+ screens", kRequestDesktopSiteForTablets960,
      base::size(kRequestDesktopSiteForTablets960), nullptr},
     {"for 1024dp+ screens", kRequestDesktopSiteForTablets1024,
      base::size(kRequestDesktopSiteForTablets1024), nullptr},
     {"for 1280dp+ screens", kRequestDesktopSiteForTablets1280,
      base::size(kRequestDesktopSiteForTablets1280), nullptr},
     {"for 1920dp+ screens", kRequestDesktopSiteForTablets1920,
      base::size(kRequestDesktopSiteForTablets1920), nullptr}};
#endif  // OS_ANDROID

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
    {"should_ignore_blocklists", "true"},
    {"enable_same_site", "true"}};

// With this, back-forward cache will be enabled on eligible pages when doing
// same-site navigations (instead of only cross-site navigations).
const FeatureEntry::FeatureParam kBackForwardCache_SameSite[] = {
    {"enable_same_site", "true"}};

const FeatureEntry::FeatureVariation kBackForwardCacheVariations[] = {
    {"same-site support (experimental)", kBackForwardCache_SameSite,
     base::size(kBackForwardCache_SameSite), nullptr},
    {"force caching all pages (experimental)", kBackForwardCache_ForceCaching,
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
const FeatureEntry::Choice kWebOtpBackendChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kWebOtpBackendSmsVerification, switches::kWebOtpBackend,
     switches::kWebOtpBackendSmsVerification},
    {flag_descriptions::kWebOtpBackendUserConsent, switches::kWebOtpBackend,
     switches::kWebOtpBackendUserConsent},
    {flag_descriptions::kWebOtpBackendAuto, switches::kWebOtpBackend,
     switches::kWebOtpBackendAuto},
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kFrameThrottleFpsChoices[] = {
    {flag_descriptions::kFrameThrottleFpsDefault, "", ""},
    {flag_descriptions::kFrameThrottleFps5, ash::switches::kFrameThrottleFps,
     "5"},
    {flag_descriptions::kFrameThrottleFps10, ash::switches::kFrameThrottleFps,
     "10"},
    {flag_descriptions::kFrameThrottleFps15, ash::switches::kFrameThrottleFps,
     "15"},
    {flag_descriptions::kFrameThrottleFps20, ash::switches::kFrameThrottleFps,
     "20"},
    {flag_descriptions::kFrameThrottleFps25, ash::switches::kFrameThrottleFps,
     "25"},
    {flag_descriptions::kFrameThrottleFps30, ash::switches::kFrameThrottleFps,
     "30"}};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
// The variations of --password-change-in-settings.
const FeatureEntry::FeatureParam
    kPasswordChangeInSettingsVariationWithForcedWarningForEverySite[] = {
        {password_manager::features::
             kPasswordChangeInSettingsWithForcedWarningForEverySite,
         "true"}};

const FeatureEntry::FeatureVariation
    kPasswordChangeInSettingsFeatureVariations[] = {
        {"Force leak warnings for every site in settings.",
         kPasswordChangeInSettingsVariationWithForcedWarningForEverySite,
         base::size(
             kPasswordChangeInSettingsVariationWithForcedWarningForEverySite),
         nullptr}};

// The variations of --password-change-support.
const FeatureEntry::FeatureParam
    kPasswordChangeVariationWithForcedDialogAfterEverySuccessfulSubmission[] = {
        {password_manager::features::
             kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission,
         "true"}};

const FeatureEntry::FeatureVariation kPasswordChangeFeatureVariations[] = {
    {"Force dialog after every successful form submission.",
     kPasswordChangeVariationWithForcedDialogAfterEverySuccessfulSubmission,
     base::size(
         kPasswordChangeVariationWithForcedDialogAfterEverySuccessfulSubmission),
     nullptr}};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kAssistantBetterOnboardingInternalName[] =
    "enable-assistant-better-onboarding";
constexpr char kAssistantTimersV2InternalName[] = "enable-assistant-timers-v2";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
const FeatureEntry::FeatureParam
    kSendWebUIJavaScriptErrorReportsVariationSendToStaging[] = {
        {features::kSendWebUIJavaScriptErrorReportsSendToProductionVariation,
         "false"}};

const FeatureEntry::FeatureVariation
    kSendWebUIJavaScriptErrorReportsVariations[] = {
        {"Send reports to staging server.",
         kSendWebUIJavaScriptErrorReportsVariationSendToStaging,
         base::size(kSendWebUIJavaScriptErrorReportsVariationSendToStaging),
         nullptr}};
#endif

#if defined(OS_ANDROID)
// The variations of --metrics-settings-android.
const FeatureEntry::FeatureParam kMetricsSettingsAndroidAlternativeOne[] = {
    {"fre", "1"}};

const FeatureEntry::FeatureParam kMetricsSettingsAndroidAlternativeTwo[] = {
    {"fre", "2"}};

const FeatureEntry::FeatureVariation kMetricsSettingsAndroidVariations[] = {
    {"Alternative FRE 1", kMetricsSettingsAndroidAlternativeOne,
     base::size(kMetricsSettingsAndroidAlternativeOne), nullptr},
    {"Alternative FRE 2", kMetricsSettingsAndroidAlternativeTwo,
     base::size(kMetricsSettingsAndroidAlternativeTwo), nullptr},
};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
// SCT Auditing feature variations.
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateNone[] = {
    {"sampling_rate", "0.0"}};
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateAlternativeOne[] = {
    {"sampling_rate", "0.0001"}};
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateAlternativeTwo[] = {
    {"sampling_rate", "0.001"}};

const FeatureEntry::FeatureVariation kSCTAuditingVariations[] = {
    {"Sampling rate 0%", kSCTAuditingSamplingRateNone,
     base::size(kSCTAuditingSamplingRateNone), nullptr},
    {"Sampling rate 0.01%", kSCTAuditingSamplingRateAlternativeOne,
     base::size(kSCTAuditingSamplingRateAlternativeOne), nullptr},
    {"Sampling rate 0.1%", kSCTAuditingSamplingRateAlternativeTwo,
     base::size(kSCTAuditingSamplingRateAlternativeTwo), nullptr},
};
#endif  // !defined(OS_ANDROID)

const FeatureEntry::FeatureParam kCheckOfflineCapabilityWarnOnly[] = {
    {"check_mode", "warn_only"}};
const FeatureEntry::FeatureParam kCheckOfflineCapabilityEnforce[] = {
    {"check_mode", "enforce"}};

const FeatureEntry::FeatureVariation kCheckOfflineCapabilityVariations[] = {
    {"Warn-only", kCheckOfflineCapabilityWarnOnly,
     base::size(kCheckOfflineCapabilityWarnOnly), nullptr},
    {"Enforce", kCheckOfflineCapabilityEnforce,
     base::size(kCheckOfflineCapabilityEnforce), nullptr},
};

const FeatureEntry::FeatureParam kSubresourceRedirectPublicImageHints[] = {
    {"enable_public_image_hints_based_compression", "true"},
    {"enable_subresource_server_redirect", "true"},
    {"enable_login_robots_based_compression", "false"},
};

const FeatureEntry::FeatureParam
    kSubresourceRedirectLoginRobotsBasedCompression[] = {
        {"enable_login_robots_based_compression", "true"},
        {"enable_subresource_server_redirect", "true"},
        {"enable_public_image_hints_based_compression", "false"},
};

const FeatureEntry::FeatureVariation kSubresourceRedirectVariations[] = {
    {"Public image hints based compression",
     kSubresourceRedirectPublicImageHints,
     base::size(kSubresourceRedirectPublicImageHints), nullptr},
    {"robots.txt allowed image compression in non logged-in pages",
     kSubresourceRedirectLoginRobotsBasedCompression,
     base::size(kSubresourceRedirectLoginRobotsBasedCompression), nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureVariation
    kOmniboxRichEntitiesInLauncherVariations[] = {
        {"with linked Suggest experiment", {}, 0, "t4461027"},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

constexpr FeatureEntry::FeatureParam kPlatformProvidedTrustTokenIssuance[] = {
    {"PlatformProvidedTrustTokenIssuance", "true"}};

constexpr FeatureEntry::FeatureVariation
    kPlatformProvidedTrustTokensVariations[] = {
        {"with platform-provided trust token issuance",
         kPlatformProvidedTrustTokenIssuance,
         base::size(kPlatformProvidedTrustTokenIssuance), nullptr}};

const FeatureEntry::FeatureParam kPasswordsAccountStorage_ProfileStore[] = {
    {password_manager::features::kSaveToProfileStoreByDefault, "true"},
    {password_manager::features::kSaveToAccountStoreOnOptIn, "true"},
};

const FeatureEntry::FeatureVariation kPasswordsAccountStorageVariations[] = {
    {"(save to profile store by default)",
     kPasswordsAccountStorage_ProfileStore,
     base::size(kPasswordsAccountStorage_ProfileStore), nullptr},
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kWallpaperWebUIInternalName[] = "wallpaper-webui";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#include "build/chromeos_buildflags.h"
#include "chrome/browser/unexpire_flags_gen.inc"
    {"ignore-gpu-blocklist", flag_descriptions::kIgnoreGpuBlocklistName,
     flag_descriptions::kIgnoreGpuBlocklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlocklist)},
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
    {"tint-composited-content", flag_descriptions::kTintCompositedContentName,
     flag_descriptions::kTintCompositedContentDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kTintCompositedContent)},
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
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"send-webui-javascript-error-reports",
     flag_descriptions::kSendWebUIJavaScriptErrorReportsName,
     flag_descriptions::kSendWebUIJavaScriptErrorReportsDescription,
     kOsLinux | kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kSendWebUIJavaScriptErrorReports,
         kSendWebUIJavaScriptErrorReportsVariations,
         "SendWebUIJavaScriptErrorReportsVariations")},
#endif
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
    {"extensions-on-chrome-urls",
     flag_descriptions::kExtensionsOnChromeUrlsName,
     flag_descriptions::kExtensionsOnChromeUrlsDescription, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
    {"default-chrome-app-uninstall-sync",
     flag_descriptions::kDefaultChromeAppUninstallSyncName,
     flag_descriptions::kDefaultChromeAppUninstallSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(extensions_features::kDefaultChromeAppUninstallSync)},
#endif  // ENABLE_EXTENSIONS
    {"enable-history-manipulation-intervention",
     flag_descriptions::kHistoryManipulationIntervention,
     flag_descriptions::kHistoryManipulationInterventionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHistoryManipulationIntervention)},
    {"colr-v1-fonts", flag_descriptions::kCOLRV1FontsName,
     flag_descriptions::kCOLRV1FontsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCOLRV1Fonts)},
#if defined(OS_ANDROID)
    {"contextual-search-debug", flag_descriptions::kContextualSearchDebugName,
     flag_descriptions::kContextualSearchDebugDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchDebug)},
    {"contextual-search-force-caption",
     flag_descriptions::kContextualSearchForceCaptionName,
     flag_descriptions::kContextualSearchForceCaptionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchForceCaption)},
    {"contextual-search-literal-search-tap",
     flag_descriptions::kContextualSearchLiteralSearchTapName,
     flag_descriptions::kContextualSearchLiteralSearchTapDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchLiteralSearchTap)},
    {"contextual-search-longpress-panel-help",
     flag_descriptions::kContextualSearchLongpressPanelHelpName,
     flag_descriptions::kContextualSearchLongpressPanelHelpDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchLongpressPanelHelp)},
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
    {"contextual-search-twv-impl",
     flag_descriptions::kContextualSearchThinWebViewImplementationName,
     flag_descriptions::kContextualSearchThinWebViewImplementationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kContextualSearchThinWebViewImplementation)},
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
    {"related-searches-ui", flag_descriptions::kRelatedSearchesUiName,
     flag_descriptions::kRelatedSearchesUiDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kRelatedSearchesUi)},
    {"bento-offline", flag_descriptions::kBentoOfflineName,
     flag_descriptions::kBentoOfflineDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBentoOffline)},
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
    {"sms-receiver-cross-device", flag_descriptions::kWebOTPCrossDeviceName,
     flag_descriptions::kWebOTPCrossDeviceDescription, kOsAll,
     FEATURE_VALUE_TYPE(kWebOTPCrossDevice)},
    {"fractional-scroll-offsets",
     flag_descriptions::kFractionalScrollOffsetsName,
     flag_descriptions::kFractionalScrollOffsetsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFractionalScrollOffsets)},
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
    {"enable-oop-rasterization-ddl",
     flag_descriptions::kOopRasterizationDDLName,
     flag_descriptions::kOopRasterizationDDLDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kOopRasterizationDDL)},
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
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
    {
        "webui-tab-strip-tab-drag-integration",
        flag_descriptions::kWebUITabStripTabDragIntegrationName,
        flag_descriptions::kWebUITabStripTabDragIntegrationDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(ash::features::kWebUITabStripTabDragIntegration),
    },
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
    {"focus-mode", flag_descriptions::kFocusMode,
     flag_descriptions::kFocusModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kFocusMode)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"disable-explicit-dma-fences",
     flag_descriptions::kDisableExplicitDmaFencesName,
     flag_descriptions::kDisableExplicitDmaFencesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableExplicitDmaFences)},
    // TODO(crbug.com/1012846): Remove this flag and provision when HDR is fully
    //  supported on ChromeOS.
    {"use-hdr-transfer-function",
     flag_descriptions::kUseHDRTransferFunctionName,
     flag_descriptions::kUseHDRTransferFunctionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kUseHDRTransferFunction)},
    {"account-management-flows-v2",
     flag_descriptions::kAccountManagementFlowsV2Name,
     flag_descriptions::kAccountManagementFlowsV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAccountManagementFlowsV2)},
    {"dark-light-mode", flag_descriptions::kDarkLightTestName,
     flag_descriptions::kDarkLightTestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDarkLightMode)},
    {"enhanced-desks", flag_descriptions::kBentoName,
     flag_descriptions::kBentoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBento)},
    {"screen-capture", flag_descriptions::kScreenCaptureTestName,
     flag_descriptions::kScreenCaptureTestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCaptureMode)},
    {"ash-limit-alt-tab-to-active-desk",
     flag_descriptions::kLimitAltTabToActiveDeskName,
     flag_descriptions::kLimitAltTabToActiveDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLimitAltTabToActiveDesk)},
    {"ash-limit-shelf-items-to-active-desk",
     flag_descriptions::kLimitShelfItemsToActiveDeskName,
     flag_descriptions::kLimitShelfItemsToActiveDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPerDeskShelf)},
    {"ash-enable-unified-desktop",
     flag_descriptions::kAshEnableUnifiedDesktopName,
     flag_descriptions::kAshEnableUnifiedDesktopDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUnifiedDesktop)},
    {"ash-enable-interactive-window-cycle-list",
     flag_descriptions::kInteractiveWindowCycleList,
     flag_descriptions::kInteractiveWindowCycleListDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kInteractiveWindowCycleList)},
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
    {"bluetooth-wbs-dogfood", flag_descriptions::kBluetoothWbsDogfoodName,
     flag_descriptions::kBluetoothWbsDogfoodDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothWbsDogfood)},
    {"cellular-use-attach-apn", flag_descriptions::kCellularUseAttachApnName,
     flag_descriptions::kCellularUseAttachApnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularUseAttachApn)},
    {"cellular-use-external-euicc",
     flag_descriptions::kCellularUseExternalEuiccName,
     flag_descriptions::kCellularUseExternalEuiccDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularUseExternalEuicc)},
    {"cryptauth-v2-device-activity-status",
     flag_descriptions::kCryptAuthV2DeviceActivityStatusName,
     flag_descriptions::kCryptAuthV2DeviceActivityStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2DeviceActivityStatus)},
    {"cryptauth-v2-device-activity-status-use-connectivity",
     flag_descriptions::kCryptAuthV2DeviceActivityStatusUseConnectivityName,
     flag_descriptions::
         kCryptAuthV2DeviceActivityStatusUseConnectivityDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kCryptAuthV2DeviceActivityStatusUseConnectivity)},
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
    {"disable-idle-sockets-close-on-memory-pressure",
     flag_descriptions::kDisableIdleSocketsCloseOnMemoryPressureName,
     flag_descriptions::kDisableIdleSocketsCloseOnMemoryPressureDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kDisableIdleSocketsCloseOnMemoryPressure)},
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
    {kLacrosStabilityInternalName, flag_descriptions::kLacrosStabilityName,
     flag_descriptions::kLacrosStabilityDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosStabilityChoices)},
    {"list-all-display-modes", flag_descriptions::kListAllDisplayModesName,
     flag_descriptions::kListAllDisplayModesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kListAllDisplayModes)},
    {"enable-hardware_mirror-mode",
     flag_descriptions::kEnableHardwareMirrorModeName,
     flag_descriptions::kEnableHardwareMirrorModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kEnableHardwareMirrorMode)},
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
    {"improved-keyboard-shortcuts",
     flag_descriptions::kImprovedKeyboardShortcutsName,
     flag_descriptions::kImprovedKeyboardShortcutsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kImprovedKeyboardShortcuts)},
    {"shelf-hide-buttons-in-tablet",
     flag_descriptions::kHideShelfControlsInTabletModeName,
     flag_descriptions::kHideShelfControlsInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideShelfControlsInTabletMode)},
    {"shelf-hover-previews", flag_descriptions::kShelfHoverPreviewsName,
     flag_descriptions::kShelfHoverPreviewsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kShelfHoverPreviews)},
    {"show-bluetooth-debug-log-toggle",
     flag_descriptions::kShowBluetoothDebugLogToggleName,
     flag_descriptions::kShowBluetoothDebugLogToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShowBluetoothDebugLogToggle)},
    {"enable-bluetooth-verbose-logs-for-googlers",
     flag_descriptions::kEnableBluetoothVerboseLogsForGooglersName,
     flag_descriptions::kEnableBluetoothVerboseLogsForGooglersDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kEnableBluetoothVerboseLogsForGooglers)},
    {"show-taps", flag_descriptions::kShowTapsName,
     flag_descriptions::kShowTapsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kShowTaps)},
    {"show-touch-hud", flag_descriptions::kShowTouchHudName,
     flag_descriptions::kShowTouchHudDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {"trim-on-memory-pressure", flag_descriptions::kTrimOnMemoryPressureName,
     flag_descriptions::kTrimOnMemoryPressureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kTrimOnMemoryPressure)},
    {"stylus-battery-status", flag_descriptions::kStylusBatteryStatusName,
     flag_descriptions::kStylusBatteryStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kStylusBatteryStatus)},
    {"deprecate-low-usage-codecs",
     flag_descriptions::kDeprecateLowUsageCodecsName,
     flag_descriptions::kDeprecateLowUsageCodecsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kDeprecateLowUsageCodecs)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && !defined(OS_ANDROID)
    {
        "enable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsLinux,
        FEATURE_VALUE_TYPE(media::kVaapiVideoDecodeLinux),
    },
#else
    {
        "disable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
#endif  // (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) &&
        // !defined(OS_ANDROID)
    {
        "disable-accelerated-video-encode",
        flag_descriptions::kAcceleratedVideoEncodeName,
        flag_descriptions::kAcceleratedVideoEncodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoEncode),
    },
    {
        "enable-media-internals",
        flag_descriptions::kEnableMediaInternalsName,
        flag_descriptions::kEnableMediaInternalsDescription,
        kOsAll,
        FEATURE_VALUE_TYPE(media::kEnableMediaInternals),
    },
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_MAC)
    {
        "zero-copy-video-capture",
        flag_descriptions::kZeroCopyVideoCaptureName,
        flag_descriptions::kZeroCopyVideoCaptureDescription,
        kOsMac,
        FEATURE_VALUE_TYPE(media::kAVFoundationCaptureV2),
    },
#endif  // defined(OS_MAC)
#if defined(OS_WIN)
    {
        "zero-copy-video-capture",
        flag_descriptions::kZeroCopyVideoCaptureName,
        flag_descriptions::kZeroCopyVideoCaptureDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kMediaFoundationD3D11VideoCapture),
    },
#endif  // defined(OS_WIN)
    {"debug-packed-apps", flag_descriptions::kDebugPackedAppName,
     flag_descriptions::kDebugPackedAppDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"use-lookalikes-for-navigation-suggestions",
     flag_descriptions::kUseLookalikesForNavigationSuggestionsName,
     flag_descriptions::kUseLookalikesForNavigationSuggestionsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(net::features::kUseLookalikesForNavigationSuggestions)},
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
#if defined(USE_AURA)
    {"overscroll-history-navigation",
     flag_descriptions::kOverscrollHistoryNavigationName,
     flag_descriptions::kOverscrollHistoryNavigationDescription, kOsAura,
     FEATURE_VALUE_TYPE(features::kOverscrollHistoryNavigation)},
    {"pull-to-refresh", flag_descriptions::kPullToRefreshName,
     flag_descriptions::kPullToRefreshDescription, kOsAura,
     MULTI_VALUE_TYPE(kPullToRefreshChoices)},
#endif  // USE_AURA
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-virtual-keyboard", flag_descriptions::kVirtualKeyboardName,
     flag_descriptions::kVirtualKeyboardDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)},
    {"disable-virtual-keyboard",
     flag_descriptions::kVirtualKeyboardDisabledName,
     flag_descriptions::kVirtualKeyboardDisabledDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kDisableVirtualKeyboard)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     flag_descriptions::kDeviceDiscoveryNotificationsName,
     flag_descriptions::kDeviceDiscoveryNotificationsDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications)},
    {"force-enable-privet-printing",
     flag_descriptions::kForceEnablePrivetPrintingName,
     flag_descriptions::kForceEnablePrivetPrintingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kForceEnablePrivetPrinting)},
#endif  // BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
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
     ENABLE_DISABLE_VALUE_TYPE(blink::switches::kEnableZeroCopy,
                               blink::switches::kDisableZeroCopy)},
    {"enable-vulkan", flag_descriptions::kEnableVulkanName,
     flag_descriptions::kEnableVulkanDescription,
     kOsWin | kOsLinux | kOsAndroid, FEATURE_VALUE_TYPE(features::kVulkan)},
#if defined(OS_MAC)
    {"disable-hosted-app-shim-creation",
     flag_descriptions::kHostedAppShimCreationName,
     flag_descriptions::kHostedAppShimCreationDescription, kOsMac,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableHostedAppShimCreation)},
    {"enable-hosted-app-quit-notification",
     flag_descriptions::kHostedAppQuitNotificationName,
     flag_descriptions::kHostedAppQuitNotificationDescription, kOsMac,
     SINGLE_VALUE_TYPE(switches::kHostedAppQuitNotification)},
#endif  // OS_MAC
#if defined(OS_ANDROID)
    {"translate-force-trigger-on-english",
     flag_descriptions::kTranslateForceTriggerOnEnglishName,
     flag_descriptions::kTranslateForceTriggerOnEnglishDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(language::kOverrideTranslateTriggerInIndia,
                                    kTranslateForceTriggerOnEnglishVariations,
                                    "OverrideTranslateTriggerInIndia")},
#endif  // OS_ANDROID

    {"override-language-prefs-for-href-translate",
     flag_descriptions::kOverrideLanguagePrefsForHrefTranslateName,
     flag_descriptions::kOverrideLanguagePrefsForHrefTranslateDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         translate::kOverrideLanguagePrefsForHrefTranslate,
         kOverrideLanguagePrefsForHrefTranslateVariations,
         "OverrideLanguagePrefsForHrefTranslate")},
    {"override-site-prefs-for-href-translate",
     flag_descriptions::kOverrideSitePrefsForHrefTranslateName,
     flag_descriptions::kOverrideSitePrefsForHrefTranslateDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         translate::kOverrideSitePrefsForHrefTranslate,
         kOverrideSitePrefsForHrefTranslateVariations,
         "OverrideSitePrefsForHrefTranslate")},

#if BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS) && !BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-system-notifications",
     flag_descriptions::kNotificationsSystemFlagName,
     flag_descriptions::kNotificationsSystemFlagDescription,
     kOsMac | kOsLinux | kOsWin,
     FEATURE_VALUE_TYPE(features::kSystemNotifications)},
#endif  // BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS) && !BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_ANDROID)
    {"adaptive-button-in-top-toolbar",
     flag_descriptions::kAdaptiveButtonInTopToolbarName,
     flag_descriptions::kAdaptiveButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kAdaptiveButtonInTopToolbar,
                                    kAdaptiveButtonInTopToolbarVariations,
                                    "AdaptiveButtonInTopToolbar")},
    {"reader-mode-heuristics", flag_descriptions::kReaderModeHeuristicsName,
     flag_descriptions::kReaderModeHeuristicsDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
    {"voice-button-in-top-toolbar",
     flag_descriptions::kVoiceButtonInTopToolbarName,
     flag_descriptions::kVoiceButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kVoiceButtonInTopToolbar)},
    {"assistant-intent-page-url",
     flag_descriptions::kAssistantIntentPageUrlName,
     flag_descriptions::kAssistantIntentPageUrlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantIntentPageUrl)},
    {"assistant-intent-translate-info",
     flag_descriptions::kAssistantIntentTranslateInfoName,
     flag_descriptions::kAssistantIntentTranslateInfoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantIntentTranslateInfo)},
    {"share-button-in-top-toolbar",
     flag_descriptions::kShareButtonInTopToolbarName,
     flag_descriptions::kShareButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShareButtonInTopToolbar)},
    {"chrome-share-highlights-android",
     flag_descriptions::kChromeShareHighlightsAndroidName,
     flag_descriptions::kChromeShareHighlightsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeShareHighlightsAndroid)},
    {"chrome-share-long-screenshot",
     flag_descriptions::kChromeShareLongScreenshotName,
     flag_descriptions::kChromeShareLongScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeShareLongScreenshot)},
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
    {"share-by-default-in-cct", flag_descriptions::kShareByDefaultInCCTName,
     flag_descriptions::kShareByDefaultInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShareByDefaultInCCT)},
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
     SINGLE_DISABLE_VALUE_TYPE(blink::switches::kDisableThreadedScrolling)},
    {"extension-content-verification",
     flag_descriptions::kExtensionContentVerificationName,
     flag_descriptions::kExtensionContentVerificationDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
    {"preemptive-link-to-text-generation",
     flag_descriptions::kPreemptiveLinkToTextGenerationName,
     flag_descriptions::kPreemptiveLinkToTextGenerationDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kPreemptiveLinkToTextGeneration)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"keyboard-based-display-arrangement-in-settings",
     flag_descriptions::kKeyboardBasedDisplayArrangementInSettingsName,
     flag_descriptions::kKeyboardBasedDisplayArrangementInSettingsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kKeyboardBasedDisplayArrangementInSettings)},
    {"enable-lock-screen-notification",
     flag_descriptions::kLockScreenNotificationName,
     flag_descriptions::kLockScreenNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenNotifications)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"crostini-use-dlc", flag_descriptions::kCrostiniUseDlcName,
     flag_descriptions::kCrostiniUseDlcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseDlc)},
    {"crostini-enable-dlc", flag_descriptions::kCrostiniEnableDlcName,
     flag_descriptions::kCrostiniEnableDlcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniEnableDlc)},
    {"pluginvm-fullscreen", flag_descriptions::kPluginVmFullscreenName,
     flag_descriptions::kPluginVmFullscreenDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPluginVmFullscreen)},
    {"pluginvm-show-camera-permissions",
     flag_descriptions::kPluginVmShowCameraPermissionsName,
     flag_descriptions::kPluginVmShowCameraPermissionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPluginVmShowCameraPermissions)},
    {"pluginvm-show-microphone-permissions",
     flag_descriptions::kPluginVmShowMicrophonePermissionsName,
     flag_descriptions::kPluginVmShowMicrophonePermissionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kPluginVmShowMicrophonePermissions)},
    {"vm-camera-mic-indicators-and-notifications",
     flag_descriptions::kVmCameraMicIndicatorsAndNotificationsName,
     flag_descriptions::kVmCameraMicIndicatorsAndNotificationsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kVmCameraMicIndicatorsAndNotifications)},
    {"crostini-reset-lxd-db", flag_descriptions::kCrostiniResetLxdDbName,
     flag_descriptions::kCrostiniResetLxdDbDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniResetLxdDb)},
#if BUILDFLAG(USE_TCMALLOC)
    {"dynamic-tcmalloc-tuning", flag_descriptions::kDynamicTcmallocName,
     flag_descriptions::kDynamicTcmallocDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kDynamicTcmallocTuning)},
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if (defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_ANDROID)) && \
    !defined(OS_NACL)
    {"mojo-linux-sharedmem", flag_descriptions::kMojoLinuxChannelSharedMemName,
     flag_descriptions::kMojoLinuxChannelSharedMemDescription,
     kOsCrOS | kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(mojo::core::kMojoLinuxChannelSharedMem)},
#endif
#if defined(OS_ANDROID)
    {"enable-site-isolation-for-password-sites",
     flag_descriptions::kSiteIsolationForPasswordSitesName,
     flag_descriptions::kSiteIsolationForPasswordSitesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         site_isolation::features::kSiteIsolationForPasswordSites)},
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
    {"isolation-by-default", flag_descriptions::kIsolationByDefaultName,
     flag_descriptions::kIsolationByDefaultDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIsolationByDefault)},
    {"enable-use-zoom-for-dsf", flag_descriptions::kEnableUseZoomForDsfName,
     flag_descriptions::kEnableUseZoomForDsfDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableUseZoomForDSFChoices)},
    {"allow-previews", flag_descriptions::kPreviewsAllowedName,
     flag_descriptions::kPreviewsAllowedDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kPreviews)},
    {"ignore-previews-blocklist",
     flag_descriptions::kIgnorePreviewsBlocklistName,
     flag_descriptions::kIgnorePreviewsBlocklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(previews::switches::kIgnorePreviewsBlocklist)},
    {"enable-data-reduction-proxy-server-experiment",
     flag_descriptions::kEnableDataReductionProxyServerExperimentName,
     flag_descriptions::kEnableDataReductionProxyServerExperimentDescription,
     kOsAll, MULTI_VALUE_TYPE(kDataReductionProxyServerExperiment)},
    {"enable-subresource-redirect",
     flag_descriptions::kEnableSubresourceRedirectName,
     flag_descriptions::kEnableSubresourceRedirectDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kSubresourceRedirect,
                                    kSubresourceRedirectVariations,
                                    "SubresourceRedirect")},
    {"enable-login-detection", flag_descriptions::kEnableLoginDetectionName,
     flag_descriptions::kEnableLoginDetectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(login_detection::kLoginDetection)},
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
    {"enable-navigation-predictor-renderer-warmup",
     flag_descriptions::kEnableNavigationPredictorRendererWarmupName,
     flag_descriptions::kEnableNavigationPredictorRendererWarmupDescription,
     kOsAll, FEATURE_VALUE_TYPE(features::kNavigationPredictorRendererWarmup)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || OS_LINUX
    {"enable-preconnect-to-search",
     flag_descriptions::kEnablePreconnectToSearchName,
     flag_descriptions::kEnablePreconnectToSearchDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPreconnectToSearch)},
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
    {"enable-google-srp-isolated-prerender-nsp",
     flag_descriptions::kEnableSRPIsolatedPrerendersNSPName,
     flag_descriptions::kEnableSRPIsolatedPrerendersNSPDescription, kOsAll,
     SINGLE_VALUE_TYPE(kIsolatedPrerenderEnableNSPCmdLineFlag)},
    {"allow-insecure-localhost", flag_descriptions::kAllowInsecureLocalhostName,
     flag_descriptions::kAllowInsecureLocalhostDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"bypass-app-banner-engagement-checks",
     flag_descriptions::kBypassAppBannerEngagementChecksName,
     flag_descriptions::kBypassAppBannerEngagementChecksDescription, kOsAll,
     SINGLE_VALUE_TYPE(webapps::switches::kBypassAppBannerEngagementChecks)},
    // TODO(https://crbug.com/1069293): Add macOS and Linux implementations.
    {"enable-desktop-pwas-app-icon-shortcuts-menu",
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuName,
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsAppIconShortcutsMenu)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desktop-pwas-attention-badging-cros",
     flag_descriptions::kDesktopPWAsAttentionBadgingCrOSName,
     flag_descriptions::kDesktopPWAsAttentionBadgingCrOSDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kDesktopPWAsAttentionBadgingCrOS,
                                    kDesktopPWAsAttentionBadgingCrOSVariations,
                                    "DesktopPWAsAttentionBadgingCrOS")},
#endif
    {"enable-desktop-pwas-remove-status-bar",
     flag_descriptions::kDesktopPWAsRemoveStatusBarName,
     flag_descriptions::kDesktopPWAsRemoveStatusBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRemoveStatusBarInWebApps)},
    {"enable-desktop-pwas-elided-extensions-menu",
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuName,
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsElidedExtensionsMenu)},
    {"enable-desktop-pwas-flash-app-name-instead-of-origin",
     flag_descriptions::kDesktopPWAsFlashAppNameInsteadOfOriginName,
     flag_descriptions::kDesktopPWAsFlashAppNameInsteadOfOriginDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsFlashAppNameInsteadOfOrigin)},
    {"enable-desktop-pwas-tab-strip",
     flag_descriptions::kDesktopPWAsTabStripName,
     flag_descriptions::kDesktopPWAsTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStrip)},
    {"enable-desktop-pwas-tab-strip-link-capturing",
     flag_descriptions::kDesktopPWAsTabStripLinkCapturingName,
     flag_descriptions::kDesktopPWAsTabStripLinkCapturingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStripLinkCapturing)},
    {"enable-desktop-pwas-link-capturing",
     flag_descriptions::kDesktopPWAsLinkCapturingName,
     flag_descriptions::kDesktopPWAsLinkCapturingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableLinkCapturing)},
    {"enable-desktop-pwas-run-on-os-login",
     flag_descriptions::kDesktopPWAsRunOnOsLoginName,
     flag_descriptions::kDesktopPWAsRunOnOsLoginDescription,
     kOsWin | kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsRunOnOsLogin)},
    {"enable-desktop-pwas-url-handling",
     flag_descriptions::kDesktopPWAsUrlHandlingName,
     flag_descriptions::kDesktopPWAsUrlHandlingDescription,
     kOsWin | kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableUrlHandlers)},
    {"record-web-app-debug-info", flag_descriptions::kRecordWebAppDebugInfoName,
     flag_descriptions::kRecordWebAppDebugInfoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRecordWebAppDebugInfo)},
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
    {"global-media-controls-cast-start-stop",
     flag_descriptions::kGlobalMediaControlsCastStartStopName,
     flag_descriptions::kGlobalMediaControlsCastStartStopDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media_router::kGlobalMediaControlsCastStartStop)},
    {"allow-all-sites-to-initiate-mirroring",
     flag_descriptions::kAllowAllSitesToInitiateMirroringName,
     flag_descriptions::kAllowAllSitesToInitiateMirroringDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kAllowAllSitesToInitiateMirroring)},
    {"enable-migrate-default-chrome-app-to-web-apps-gsuite",
     flag_descriptions::kEnableMigrateDefaultChromeAppToWebAppsGSuiteName,
     flag_descriptions::
         kEnableMigrateDefaultChromeAppToWebAppsGSuiteDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(web_app::kMigrateDefaultChromeAppToWebAppsGSuite)},
    {"enable-migrate-default-chrome-app-to-web-apps-non-gsuite",
     flag_descriptions::kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteName,
     flag_descriptions::
         kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(web_app::kMigrateDefaultChromeAppToWebAppsNonGSuite)},
#endif  // !OS_ANDROID
#if defined(OS_ANDROID)
    {"autofill-keyboard-accessory-view",
     flag_descriptions::kAutofillAccessoryViewName,
     flag_descriptions::kAutofillAccessoryViewDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillKeyboardAccessory)},
#endif  // OS_ANDROID
#if defined(OS_MAC)
    {"mac-syscall-sandbox", flag_descriptions::kMacSyscallSandboxName,
     flag_descriptions::kMacSyscallSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSyscallSandbox)},
    {"mac-v2-gpu-sandbox", flag_descriptions::kMacV2GPUSandboxName,
     flag_descriptions::kMacV2GPUSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacV2GPUSandbox)},
#endif  // OS_MAC
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN) || defined(OS_MAC)
    {"web-share", flag_descriptions::kWebShareName,
     flag_descriptions::kWebShareDescription, kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebShare)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || OS_WIN || OS_MAC
#if BUILDFLAG(ENABLE_VR)
    {"webxr-incubations", flag_descriptions::kWebXrIncubationsName,
     flag_descriptions::kWebXrIncubationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrIncubations)},
    {"webxr-runtime", flag_descriptions::kWebXrForceRuntimeName,
     flag_descriptions::kWebXrForceRuntimeDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kWebXrForceRuntimeChoices)},
#endif  // ENABLE_VR
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"disable-accelerated-mjpeg-decode",
     flag_descriptions::kAcceleratedMjpegDecodeName,
     flag_descriptions::kAcceleratedMjpegDecodeDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {"system-keyboard-lock", flag_descriptions::kSystemKeyboardLockName,
     flag_descriptions::kSystemKeyboardLockDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemKeyboardLock)},
#if defined(OS_ANDROID)
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
    {"offline-indicator-v2", flag_descriptions::kOfflineIndicatorV2Name,
     flag_descriptions::kOfflineIndicatorV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOfflineIndicatorV2)},
    {"query-tiles", flag_descriptions::kQueryTilesName,
     flag_descriptions::kQueryTilesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTiles)},
    {"query-tiles-ntp", flag_descriptions::kQueryTilesNTPName,
     flag_descriptions::kQueryTilesNTPDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesInNTP)},
    {"query-tiles-omnibox", flag_descriptions::kQueryTilesOmniboxName,
     flag_descriptions::kQueryTilesOmniboxDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesInOmnibox)},
    {"query-tiles-local-ordering",
     flag_descriptions::kQueryTilesLocalOrderingName,
     flag_descriptions::kQueryTilesLocalOrderingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesLocalOrdering)},
    {"query-tiles-single-tier", flag_descriptions::kQueryTilesSingleTierName,
     flag_descriptions::kQueryTilesSingleTierDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesSingleTier)},
    {"query-tiles-enable-query-editing",
     flag_descriptions::kQueryTilesEnableQueryEditingName,
     flag_descriptions::kQueryTilesEnableQueryEditingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesEnableQueryEditing)},
    {"query-tiles-enable-trending",
     flag_descriptions::kQueryTilesEnableTrendingName,
     flag_descriptions::kQueryTilesEnableTrendingDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesEnableTrending)},
    {"query-tiles-country-code", flag_descriptions::kQueryTilesCountryCode,
     flag_descriptions::kQueryTilesCountryCodeDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kQueryTilesCountryChoices)},
    {"query-tiles-instant-fetch",
     flag_descriptions::kQueryTilesInstantFetchName,
     flag_descriptions::kQueryTilesInstantFetchDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(
         query_tiles::switches::kQueryTilesInstantBackgroundTask)},
    {"query-tiles-more-trending",
     flag_descriptions::kQueryTilesMoreTrendingName,
     flag_descriptions::kQueryTilesMoreTrendingDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesMoreTrending)},
    {"query-tiles-swap-trending",
     flag_descriptions::kQueryTilesSwapTrendingName,
     flag_descriptions::kQueryTilesSwapTrendingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         query_tiles::features::kQueryTilesRemoveTrendingTilesAfterInactivity)},
    {"video-tutorials", flag_descriptions::kVideoTutorialsName,
     flag_descriptions::kVideoTutorialsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(video_tutorials::features::kVideoTutorials)},
    {"video-tutorials-instant-fetch",
     flag_descriptions::kVideoTutorialsInstantFetchName,
     flag_descriptions::kVideoTutorialsInstantFetchDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(video_tutorials::switches::kVideoTutorialsInstantFetch)},
    {"android-picture-in-picture-api",
     flag_descriptions::kAndroidPictureInPictureAPIName,
     flag_descriptions::kAndroidPictureInPictureAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(media::kPictureInPictureAPI)},
    {"reengagement-notification",
     flag_descriptions::kReengagementNotificationName,
     flag_descriptions::kReengagementNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReengagementNotification)},
    {"toolbar-iph-android", flag_descriptions::kToolbarIphAndroidName,
     flag_descriptions::kToolbarIphAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kToolbarIphAndroid)},
    {"theme-refactor-android", flag_descriptions::kThemeRefactorAndroidName,
     flag_descriptions::kThemeRefactorAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kThemeRefactorAndroid)},
#endif  // OS_ANDROID
    {"disallow-doc-written-script-loads",
     flag_descriptions::kDisallowDocWrittenScriptsUiName,
     flag_descriptions::kDisallowDocWrittenScriptsUiDescription, kOsAll,
     // NOTE: if we want to add additional experiment entries for other
     // features controlled by kBlinkSettings, we'll need to add logic to
     // merge the flag values.
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         blink::switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=true",
         blink::switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=false")},
#if defined(OS_WIN)
    {"use-winrt-midi-api", flag_descriptions::kUseWinrtMidiApiName,
     flag_descriptions::kUseWinrtMidiApiDescription, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
#endif  // OS_WIN
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-regions-mode", flag_descriptions::kCrosRegionsModeName,
     flag_descriptions::kCrosRegionsModeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kCrosRegionsModeChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
    {"enable-tls13-early-data", flag_descriptions::kEnableTLS13EarlyDataName,
     flag_descriptions::kEnableTLS13EarlyDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnableTLS13EarlyData)},
    {"post-quantum-cecpq2", flag_descriptions::kPostQuantumCECPQ2Name,
     flag_descriptions::kPostQuantumCECPQ2Description, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kPostQuantumCECPQ2)},
#if defined(OS_ANDROID)
    {"interest-feed-v2", flag_descriptions::kInterestFeedV2Name,
     flag_descriptions::kInterestFeedV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2)},
    {"feed-v2-hearts", flag_descriptions::kInterestFeedV2HeartsName,
     flag_descriptions::kInterestFeedV2HeartsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2Hearts)},
    {"feed-share", flag_descriptions::kFeedShareName,
     flag_descriptions::kFeedShareDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedShare)},
    {"web-feed", flag_descriptions::kWebFeedName,
     flag_descriptions::kWebFeedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kWebFeed)},
    {"xsurface-metrics-reporting",
     flag_descriptions::kXsurfaceMetricsReportingName,
     flag_descriptions::kXsurfaceMetricsReportingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kWebFeed)},
    {"report-feed-user-actions", flag_descriptions::kReportFeedUserActionsName,
     flag_descriptions::kReportFeedUserActionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kReportFeedUserActions)},
    {"interest-feed-v1-clicks-and-views-cond-upload",
     flag_descriptions::kInterestFeedV1ClickAndViewActionsConditionalUploadName,
     flag_descriptions::
         kInterestFeedV1ClickAndViewActionsConditionalUploadDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV1ClicksAndViewsConditionalUpload)},
    {"interest-feed-v2-clicks-and-views-cond-upload",
     flag_descriptions::kInterestFeedV2ClickAndViewActionsConditionalUploadName,
     flag_descriptions::
         kInterestFeedV2ClickAndViewActionsConditionalUploadDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2ClicksAndViewsConditionalUpload)},
    {"interest-feed-notice-card-auto-dismiss",
     flag_descriptions::kInterestFeedNoticeCardAutoDismissName,
     flag_descriptions::kInterestFeedNoticeCardAutoDismissDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(feed::kInterestFeedNoticeCardAutoDismiss)},
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
    {"enable-force-dark", flag_descriptions::kForceWebContentsDarkModeName,
     flag_descriptions::kForceWebContentsDarkModeDescription, kOsAll,
#if BUILDFLAG(IS_CHROMEOS_ASH)
     // TODO(https://crbug.com/1011696): Investigate crash reports and
     // re-enable variations for ChromeOS.
     FEATURE_VALUE_TYPE(blink::features::kForceWebContentsDarkMode)},
#else
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kForceWebContentsDarkMode,
                                    kForceDarkVariations,
                                    "ForceDarkVariations")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_ANDROID)
    {"enable-android-dark-search", flag_descriptions::kAndroidDarkSearchName,
     flag_descriptions::kAndroidDarkSearchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidDarkSearch)},

    {"enable-android-layout-change-tab-reparenting",
     flag_descriptions::kAndroidLayoutChangeTabReparentingName,
     flag_descriptions::kAndroidLayoutChangeTabReparentingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidLayoutChangeTabReparenting)},
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
    {"enable-aria-element-reflection",
     flag_descriptions::kAriaElementReflectionName,
     flag_descriptions::kAriaElementReflectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAriaElementReflection)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    {"enable-cros-ime-assist-multi-word",
     flag_descriptions::kImeAssistMultiWordName,
     flag_descriptions::kImeAssistMultiWordDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistMultiWord)},
    {"enable-cros-ime-assist-personal-info",
     flag_descriptions::kImeAssistPersonalInfoName,
     flag_descriptions::kImeAssistPersonalInfoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistPersonalInfo)},
    {"enable-cros-ime-emoji-suggest-addition",
     flag_descriptions::kImeEmojiSuggestAdditionName,
     flag_descriptions::kImeEmojiSuggestAdditionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEmojiSuggestAddition)},
    {"enable-cros-ime-mozc-proto", flag_descriptions::kImeMozcProtoName,
     flag_descriptions::kImeMozcProtoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeMozcProto)},
    {"enable-cros-ime-service-decoder",
     flag_descriptions::kImeServiceDecoderName,
     flag_descriptions::kImeServiceDecoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeMojoDecoder)},
    {"enable-cros-ime-system-emoji-picker",
     flag_descriptions::kImeSystemEmojiPickerName,
     flag_descriptions::kImeSystemEmojiPickerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPicker)},
    {"enable-cros-language-settings-update",
     flag_descriptions::kCrosLanguageSettingsUpdateName,
     flag_descriptions::kCrosLanguageSettingsUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLanguageSettingsUpdate)},
    {"enable-cros-language-settings-update-2",
     flag_descriptions::kCrosLanguageSettingsUpdate2Name,
     flag_descriptions::kCrosLanguageSettingsUpdate2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLanguageSettingsUpdate2)},
    {"enable-cros-multilingual-typing",
     flag_descriptions::kMultilingualTypingName,
     flag_descriptions::kMultilingualTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMultilingualTyping)},
    {"enable-cros-on-device-grammar-check",
     flag_descriptions::kCrosOnDeviceGrammarCheckName,
     flag_descriptions::kCrosOnDeviceGrammarCheckDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOnDeviceGrammarCheck)},
    {"enable-cros-system-latin-physical-typing",
     flag_descriptions::kSystemLatinPhysicalTypingName,
     flag_descriptions::kSystemLatinPhysicalTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSystemLatinPhysicalTyping)},
    {"enable-cros-virtual-keyboard-bordered-key",
     flag_descriptions::kVirtualKeyboardBorderedKeyName,
     flag_descriptions::kVirtualKeyboardBorderedKeyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardBorderedKey)},
    {"enable-cros-virtual-keyboard-multipaste",
     flag_descriptions::kVirtualKeyboardMultipasteName,
     flag_descriptions::kVirtualKeyboardMultipasteDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardMultipaste)},
    {"enable-experimental-accessibility-dictation-extension",
     flag_descriptions::kExperimentalAccessibilityDictationExtensionName,
     flag_descriptions::kExperimentalAccessibilityDictationExtensionDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityDictationExtension)},
    {"enable-experimental-accessibility-dictation-offline",
     flag_descriptions::kExperimentalAccessibilityDictationOfflineName,
     flag_descriptions::kExperimentalAccessibilityDictationOfflineDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityDictationOffline)},
    {"enable-experimental-accessibility-switch-access-text",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessText)},
    {"enable-switch-access-point-scanning",
     flag_descriptions::kSwitchAccessPointScanningName,
     flag_descriptions::kSwitchAccessPointScanningDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(::switches::kEnableSwitchAccessPointScanning)},
    {"enable-experimental-accessibility-switch-access-setup-guide",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessSetupGuideName,
     flag_descriptions::
         kExperimentalAccessibilitySwitchAccessSetupGuideDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessSetupGuide)},
    {"enable-experimental-kernel-vm-support",
     flag_descriptions::kKernelnextVMsName,
     flag_descriptions::kKernelnextVMsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kKernelnextVMs)},
    {"enable-magnifier-new-focus-following",
     flag_descriptions::kMagnifierNewFocusFollowingName,
     flag_descriptions::kMagnifierNewFocusFollowingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kMagnifierNewFocusFollowing)},
    {"enable-magnifier-panning-improvements",
     flag_descriptions::kMagnifierPanningImprovementsName,
     flag_descriptions::kMagnifierPanningImprovementsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kMagnifierPanningImprovements)},
    {"enable-magnifier-continuous-mouse-following-mode-setting",
     flag_descriptions::kMagnifierContinuousMouseFollowingModeSettingName,
     flag_descriptions::
         kMagnifierContinuousMouseFollowingModeSettingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kMagnifierContinuousMouseFollowingModeSetting)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_MAC)
    {"enable-immersive-fullscreen-toolbar",
     flag_descriptions::kImmersiveFullscreenName,
     flag_descriptions::kImmersiveFullscreenDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kImmersiveFullscreen)},
#endif  // OS_MAC
    {"rewrite-leveldb-on-deletion",
     flag_descriptions::kRewriteLevelDBOnDeletionName,
     flag_descriptions::kRewriteLevelDBOnDeletionDescription, kOsAll,
     FEATURE_VALUE_TYPE(leveldb::kLevelDBRewriteFeature)},
    {"passive-listener-default",
     flag_descriptions::kPassiveEventListenerDefaultName,
     flag_descriptions::kPassiveEventListenerDefaultDescription, kOsAll,
     MULTI_VALUE_TYPE(kPassiveListenersChoices)},
    {"enable-web-payments-experimental-features",
     flag_descriptions::kWebPaymentsExperimentalFeaturesName,
     flag_descriptions::kWebPaymentsExperimentalFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsExperimentalFeatures)},
    {"enable-web-payments-minimal-ui",
     flag_descriptions::kWebPaymentsMinimalUIName,
     flag_descriptions::kWebPaymentsMinimalUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebPaymentsMinimalUI)},
    {"enable-debug-for-store-billing",
     flag_descriptions::kAppStoreBillingDebugName,
     flag_descriptions::kAppStoreBillingDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kAppStoreBillingDebug)},
    {"enable-debug-for-secure-payment-confirmation",
     flag_descriptions::kSecurePaymentConfirmationDebugName,
     flag_descriptions::kSecurePaymentConfirmationDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSecurePaymentConfirmationDebug)},
    {"fill-on-account-select", flag_descriptions::kFillOnAccountSelectName,
     flag_descriptions::kFillOnAccountSelectDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"arc-custom-tabs-experiment",
     flag_descriptions::kArcCustomTabsExperimentName,
     flag_descriptions::kArcCustomTabsExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kCustomTabsExperimentFeature)},
    {"arc-documents-provider", flag_descriptions::kArcDocumentsProviderName,
     flag_descriptions::kArcDocumentsProviderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableDocumentsProviderInFilesAppFeature)},
    {"arc-enable-usap", flag_descriptions::kArcEnableUsapName,
     flag_descriptions::kArcEnableUsapDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableUsap)},
    {"arc-file-picker-experiment",
     flag_descriptions::kArcFilePickerExperimentName,
     flag_descriptions::kArcFilePickerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kFilePickerExperimentFeature)},
    {"arc-native-bridge-toggle", flag_descriptions::kArcNativeBridgeToggleName,
     flag_descriptions::kArcNativeBridgeToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeToggleFeature)},
    {"arc-native-bridge-64bit-support-experiment",
     flag_descriptions::kArcNativeBridge64BitSupportExperimentName,
     flag_descriptions::kArcNativeBridge64BitSupportExperimentDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridge64BitSupportExperimentFeature)},
    {"arc-rt-vcpu-dual-core", flag_descriptions::kArcRtVcpuDualCoreName,
     flag_descriptions::kArcRtVcpuDualCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuDualCore)},
    {"arc-rt-vcpu-quad-core", flag_descriptions::kArcRtVcpuQuadCoreName,
     flag_descriptions::kArcRtVcpuQuadCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuQuadCore)},
    {"arc-use-high-memory-dalvik-profile",
     flag_descriptions::kArcUseHighMemoryDalvikProfileName,
     flag_descriptions::kArcUseHighMemoryDalvikProfileDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUseHighMemoryDalvikProfile)},
    {"arc-usb-host", flag_descriptions::kArcUsbHostName,
     flag_descriptions::kArcUsbHostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUsbHostFeature)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {ui_devtools::switches::kEnableUiDevTools,
     flag_descriptions::kUiDevToolsName,
     flag_descriptions::kUiDevToolsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ui_devtools::switches::kEnableUiDevTools)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-autofill-credit-card-ablation-experiment",
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDisplayName,
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillCreditCardAblationExperiment)},

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-touchscreen-calibration",
     flag_descriptions::kTouchscreenCalibrationName,
     flag_descriptions::kTouchscreenCalibrationDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchCalibrationSetting)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"prefer-constant-frame-rate",
     flag_descriptions::kPreferConstantFrameRateName,
     flag_descriptions::kPreferConstantFrameRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPreferConstantFrameRate)},
    {"crostini-gpu-support", flag_descriptions::kCrostiniGpuSupportName,
     flag_descriptions::kCrostiniGpuSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniGpuSupport)},
    {"disable-camera-frame-rotation-at-source",
     flag_descriptions::kDisableCameraFrameRotationAtSourceName,
     flag_descriptions::kDisableCameraFrameRotationAtSourceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::features::kDisableCameraFrameRotationAtSource)},
    {"drive-fs-bidirectional-native-messaging",
     flag_descriptions::kDriveFsBidirectionalNativeMessagingName,
     flag_descriptions::kDriveFsBidirectionalNativeMessagingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kDriveFsBidirectionalNativeMessaging)},
    {"files-app-copy-image", flag_descriptions::kFilesAppCopyImageName,
     flag_descriptions::kFilesAppCopyImageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableFilesAppCopyImage)},
    {"files-camera-folder", flag_descriptions::kFilesCameraFolderName,
     flag_descriptions::kFilesCameraFolderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesCameraFolder)},
    {"files-filters-in-recents", flag_descriptions::kFiltersInRecentsName,
     flag_descriptions::kFiltersInRecentsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFiltersInRecents)},
    {"files-js-modules", flag_descriptions::kFilesJsModulesName,
     flag_descriptions::kFilesJsModulesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesJsModules)},
    {"audio-player-js-modules", flag_descriptions::kAudioPlayerJsModulesName,
     flag_descriptions::kAudioPlayerJsModulesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAudioPlayerJsModules)},
    {"video-player-js-modules", flag_descriptions::kVideoPlayerJsModulesName,
     flag_descriptions::kVideoPlayerJsModulesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVideoPlayerJsModules)},
    {"files-ng", flag_descriptions::kFilesNGName,
     flag_descriptions::kFilesNGDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesNG)},
    {"files-single-partition-format",
     flag_descriptions::kFilesSinglePartitionFormatName,
     flag_descriptions::kFilesSinglePartitionFormatDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesSinglePartitionFormat)},
    {"files-swa", flag_descriptions::kFilesSWAName,
     flag_descriptions::kFilesSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesSWA)},
    {"files-trash", flag_descriptions::kFilesTrashName,
     flag_descriptions::kFilesTrashDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesTrash)},
    {"files-zip-mount", flag_descriptions::kFilesZipMountName,
     flag_descriptions::kFilesZipMountDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesZipMount)},
    {"files-zip-pack", flag_descriptions::kFilesZipPackName,
     flag_descriptions::kFilesZipPackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesZipPack)},
    {"files-zip-unpack", flag_descriptions::kFilesZipUnpackName,
     flag_descriptions::kFilesZipUnpackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesZipUnpack)},
    {"files-unified-media-view", flag_descriptions::kUnifiedMediaViewName,
     flag_descriptions::kUnifiedMediaViewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUnifiedMediaView)},
    {"force-spectre-v2-mitigation",
     flag_descriptions::kForceSpectreVariant2MitigationName,
     flag_descriptions::kForceSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         sandbox::policy::features::kForceSpectreVariant2Mitigation)},
    {"smbfs-file-shares", flag_descriptions::kSmbfsFileSharesName,
     flag_descriptions::kSmbfsFileSharesName, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSmbFs)},
    {"spectre-v2-mitigation", flag_descriptions::kSpectreVariant2MitigationName,
     flag_descriptions::kSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(sandbox::policy::features::kSpectreVariant2Mitigation)},
    {"eche-swa", flag_descriptions::kEcheSWAName,
     flag_descriptions::kEcheSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWA)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"gdi-text-printing", flag_descriptions::kGdiTextPrinting,
     flag_descriptions::kGdiTextPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kGdiTextPrinting)},
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MAC)
    {"new-usb-backend", flag_descriptions::kNewUsbBackendName,
     flag_descriptions::kNewUsbBackendDescription, kOsWin | kOsMac,
     FEATURE_VALUE_TYPE(device::kNewUsbBackend)},
#endif  // defined(OS_WIN) || defined(OS_MAC)

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
    {"omnibox-most-visited-tiles",
     flag_descriptions::kOmniboxMostVisitedTilesName,
     flag_descriptions::kOmniboxMostVisitedTilesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTiles)},
    {"omnibox-native-voice-suggestions-provider",
     flag_descriptions::kOmniboxNativeVoiceSuggestProviderName,
     flag_descriptions::kOmniboxNativeVoiceSuggestProviderDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(omnibox::kNativeVoiceSuggestProvider)},
    {"omnibox-search-engine-logo",
     flag_descriptions::kOmniboxSearchEngineLogoName,
     flag_descriptions::kOmniboxSearchEngineLogoDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxSearchEngineLogo,
                                    kOmniboxSearchEngineLogoFeatureVariations,
                                    "OmniboxSearchEngineLogo")},
    {"omnibox-search-ready-incognito",
     flag_descriptions::kOmniboxSearchReadyIncognitoName,
     flag_descriptions::kOmniboxSearchReadyIncognitoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxSearchReadyIncognito)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
#endif  // defined(OS_ANDROID)

    {"omnibox-clobber-triggers-contextual-web-zero-suggest",
     flag_descriptions::kOmniboxClobberTriggersContextualWebZeroSuggestName,
     flag_descriptions::
         kOmniboxClobberTriggersContextualWebZeroSuggestDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kClobberTriggersContextualWebZeroSuggest,
         // On-clobber has the same variations and forcing IDs as on-focus.
         kOmniboxOnFocusSuggestionsContextualWebVariations,
         "OmniboxGoogleOnContent")},

    {"omnibox-on-device-head-suggestions-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kOnDeviceHeadProviderIncognito)},

    {"omnibox-on-device-head-suggestions-non-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnDeviceHeadProviderNonIncognito,
         kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations,
         "OmniboxOnDeviceHeadSuggestNonIncognito")},

    {"omnibox-on-focus-suggestions-contextual-web",
     flag_descriptions::kOmniboxOnFocusSuggestionsContextualWebName,
     flag_descriptions::kOmniboxOnFocusSuggestionsContextualWebDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnFocusSuggestionsContextualWeb,
         kOmniboxOnFocusSuggestionsContextualWebVariations,
         "OmniboxGoogleOnContent")},

    {"omnibox-local-zero-suggest-frecency-ranking",
     flag_descriptions::kOmniboxLocalZeroSuggestFrecencyRankingName,
     flag_descriptions::kOmniboxLocalZeroSuggestFrecencyRankingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxLocalZeroSuggestFrecencyRanking)},

    {"omnibox-experimental-suggest-scoring",
     flag_descriptions::kOmniboxExperimentalSuggestScoringName,
     flag_descriptions::kOmniboxExperimentalSuggestScoringDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxExperimentalSuggestScoring)},

    {"omnibox-trending-zero-prefix-suggestions-on-ntp",
     flag_descriptions::kOmniboxTrendingZeroPrefixSuggestionsOnNTPName,
     flag_descriptions::kOmniboxTrendingZeroPrefixSuggestionsOnNTPDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTrendingZeroPrefixSuggestionsOnNTP)},

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
    {"omnibox-experimental-keyword-mode",
     flag_descriptions::kOmniboxExperimentalKeywordModeName,
     flag_descriptions::kOmniboxExperimentalKeywordModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kExperimentalKeywordMode)},
    {"omnibox-short-bookmark-suggestions",
     flag_descriptions::kOmniboxShortBookmarkSuggestionsName,
     flag_descriptions::kOmniboxShortBookmarkSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kShortBookmarkSuggestions)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"omnibox-suggestion-button-row",
     flag_descriptions::kOmniboxSuggestionButtonRowName,
     flag_descriptions::kOmniboxSuggestionButtonRowDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxSuggestionButtonRow)},
    {"omnibox-pedal-suggestions",
     flag_descriptions::kOmniboxPedalSuggestionsName,
     flag_descriptions::kOmniboxPedalSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalSuggestions)},
    {"omnibox-keyword-search-button",
     flag_descriptions::kOmniboxKeywordSearchButtonName,
     flag_descriptions::kOmniboxKeywordSearchButtonDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxKeywordSearchButton)},
    {"omnibox-refined-focus-state",
     flag_descriptions::kOmniboxRefinedFocusStateName,
     flag_descriptions::kOmniboxRefinedFocusStateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxRefinedFocusState)},
    {"omnibox-drive-suggestions",
     flag_descriptions::kOmniboxDriveSuggestionsName,
     flag_descriptions::kOmniboxDriveSuggestionsDescriptions, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kDocumentProvider,
         kOmniboxDocumentProviderVariations,
         "OmniboxDocumentProviderNonDogfoodExperiments")},
    {"omnibox-rich-autocompletion",
     flag_descriptions::kOmniboxRichAutocompletionName,
     flag_descriptions::kOmniboxRichAutocompletionDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kOmniboxRichAutocompletionVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-min-char",
     flag_descriptions::kOmniboxRichAutocompletionMinCharName,
     flag_descriptions::kOmniboxRichAutocompletionMinCharDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kOmniboxRichAutocompletionMinCharVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-show-additional-text",
     flag_descriptions::kOmniboxRichAutocompletionShowAdditionalTextName,
     flag_descriptions::kOmniboxRichAutocompletionShowAdditionalTextDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionShowAdditionalTextVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-split",
     flag_descriptions::kOmniboxRichAutocompletionSplitName,
     flag_descriptions::kOmniboxRichAutocompletionSplitDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kOmniboxRichAutocompletionSplitVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-prefer-urls-over-prefixes",
     flag_descriptions::kOmniboxRichAutocompletionPreferUrlsOverPrefixesName,
     flag_descriptions::
         kOmniboxRichAutocompletionPreferUrlsOverPrefixesDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionPreferUrlsOverPrefixesVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-promising",
     flag_descriptions::kOmniboxRichAutocompletionPromisingName,
     flag_descriptions::kOmniboxRichAutocompletionPromisingDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionPromisingVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-bookmark-paths", flag_descriptions::kOmniboxBookmarkPathsName,
     flag_descriptions::kOmniboxBookmarkPathsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kBookmarkPaths,
                                    kOmniboxBookmarkPathsVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-disable-cgi-param-matching",
     flag_descriptions::kOmniboxDisableCGIParamMatchingName,
     flag_descriptions::kOmniboxDisableCGIParamMatchingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDisableCGIParamMatching)},
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) ||
        // defined(OS_WIN)

    {"enable-speculative-service-worker-start-on-query-input",
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputName,
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kSpeculativeServiceWorkerStartOnQueryInput)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"scheduler-configuration", flag_descriptions::kSchedulerConfigurationName,
     flag_descriptions::kSchedulerConfigurationDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kSchedulerConfigurationChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"enable-command-line-on-non-rooted-devices",
     flag_descriptions::kEnableCommandLineOnNonRootedName,
     flag_descriptions::kEnableCommandLineOnNoRootedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCommandLineOnNonRooted)},
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
    {"tabbed-app-overflow-menu-icons",
     flag_descriptions::kTabbedAppOverflowMenuIconsName,
     flag_descriptions::kTabbedAppOverflowMenuIconsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabbedAppOverflowMenuIcons)},
    {"tabbed-app-overflow-menu-regroup",
     flag_descriptions::kTabbedAppOverflowMenuRegroupName,
     flag_descriptions::kTabbedAppOverflowMenuRegroupDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kTabbedAppOverflowMenuRegroup,
         kTabbedAppOverflowMenuRegroupVariations,
         "AndroidAppMenuUiRework")},
    {"tabbed-app-overflow-menu-three-button-actionbar",
     flag_descriptions::kTabbedAppOverflowMenuThreeButtonActionbarName,
     flag_descriptions::kTabbedAppOverflowMenuThreeButtonActionbarDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kTabbedAppOverflowMenuThreeButtonActionbar,
         kTabbedAppOverflowMenuThreeButtonActionbarVariations,
         "AndroidAppMenuUiReworkPhase4And5")},
    {"request-desktop-site-for-tablets",
     flag_descriptions::kRequestDesktopSiteForTabletsName,
     flag_descriptions::kRequestDesktopSiteForTabletsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kRequestDesktopSiteForTablets,
                                    kRequestDesktopSiteForTabletsVariations,
                                    "RequestDesktopSiteForTablets")},
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

    {"omnibox-default-typed-navigations-to-https",
     flag_descriptions::kOmniboxDefaultTypedNavigationsToHttpsName,
     flag_descriptions::kOmniboxDefaultTypedNavigationsToHttpsDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kDefaultTypedNavigationsToHttps,
         kOmniboxDefaultTypedNavigationsToHttpsVariations,
         "OmniboxDefaultTypedNavigationsToHttps")},

    {"omnibox-ui-sometimes-elide-to-registrable-domain",
     flag_descriptions::kOmniboxUIMaybeElideToRegistrableDomainName,
     flag_descriptions::kOmniboxUIMaybeElideToRegistrableDomainDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(omnibox::kMaybeElideToRegistrableDomain)},

    {"omnibox-ui-reveal-steady-state-url-path-query-and-ref-on-hover",
     flag_descriptions::
         kOmniboxUIRevealSteadyStateUrlPathQueryAndRefOnHoverName,
     flag_descriptions::
         kOmniboxUIRevealSteadyStateUrlPathQueryAndRefOnHoverDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover)},

    {"omnibox-ui-hide-steady-state-url-path-query-and-ref-on-interaction",
     flag_descriptions::
         kOmniboxUIHideSteadyStateUrlPathQueryAndRefOnInteractionName,
     flag_descriptions::
         kOmniboxUIHideSteadyStateUrlPathQueryAndRefOnInteractionDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         omnibox::kHideSteadyStateUrlPathQueryAndRefOnInteraction)},

    {"omnibox-max-zero-suggest-matches",
     flag_descriptions::kOmniboxMaxZeroSuggestMatchesName,
     flag_descriptions::kOmniboxMaxZeroSuggestMatchesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMaxZeroSuggestMatches,
                                    kMaxZeroSuggestMatchesVariations,
                                    "OmniboxBundledExperimentV1")},

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

    {"omnibox-dynamic-max-autocomplete",
     flag_descriptions::kOmniboxDynamicMaxAutocompleteName,
     flag_descriptions::kOmniboxDynamicMaxAutocompleteDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kDynamicMaxAutocomplete,
                                    kOmniboxDynamicMaxAutocompleteVariations,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-bubble-url-suggestions",
     flag_descriptions::kOmniboxBubbleUrlSuggestionsName,
     flag_descriptions::kOmniboxBubbleUrlSuggestionsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kBubbleUrlSuggestions,
                                    kOmniboxBubbleUrlSuggestionsVariations,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-ui-swap-title-and-url",
     flag_descriptions::kOmniboxUISwapTitleAndUrlName,
     flag_descriptions::kOmniboxUISwapTitleAndUrlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentSwapTitleAndUrl)},

    {"omnibox-webui-omnibox-popup",
     flag_descriptions::kOmniboxWebUIOmniboxPopupName,
     flag_descriptions::kOmniboxWebUIOmniboxPopupDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kWebUIOmniboxPopup)},

    {"memories", flag_descriptions::kMemoriesName,
     flag_descriptions::kMemoriesDescription, kOsAll,
     FEATURE_VALUE_TYPE(memories::kMemories)},

    {"memories-debug", flag_descriptions::kMemoriesDebugName,
     flag_descriptions::kMemoriesDebugDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(memories::kDebug)},

    {"search-prefetch", flag_descriptions::kEnableSearchPrefetchName,
     flag_descriptions::kEnableSearchPrefetchDescription, kOsAll,
     SINGLE_VALUE_TYPE(kSearchPrefetchServiceCommandLineFlag)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"handwriting-gesture", flag_descriptions::kHandwritingGestureName,
     flag_descriptions::kHandwritingGestureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kHandwritingGesture)},

    {"handwriting-gesture-editing",
     flag_descriptions::kHandwritingGestureEditingName,
     flag_descriptions::kHandwritingGestureEditingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHandwritingGestureEditing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"block-insecure-private-network-requests",
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsName,
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBlockInsecurePrivateNetworkRequests)},

    {"cross-origin-opener-policy-reporting",
     flag_descriptions::kCrossOriginOpenerPolicyReportingName,
     flag_descriptions::kCrossOriginOpenerPolicyReportingDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kCrossOriginOpenerPolicyReporting)},

    {"cross-origin-opener-policy-access-reporting",
     flag_descriptions::kCrossOriginOpenerPolicyAccessReportingName,
     flag_descriptions::kCrossOriginOpenerPolicyAccessReportingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         network::features::kCrossOriginOpenerPolicyAccessReporting)},

    {"cross-origin-isolated", flag_descriptions::kCrossOriginIsolatedName,
     flag_descriptions::kCrossOriginIsolatedDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kCrossOriginIsolated)},

    {"disable-keepalive-fetch", flag_descriptions::kDisableKeepaliveFetchName,
     flag_descriptions::kDisableKeepaliveFetchDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kDisableKeepaliveFetch)},

    {"mbi-mode", flag_descriptions::kMBIModeName,
     flag_descriptions::kMBIModeDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kMBIMode,
                                    kMBIModeVariations,
                                    "MBIMode")},

    {"delay-competing-low-priority-requests",
     flag_descriptions::kDelayCompetingLowPriorityRequestsName,
     flag_descriptions::kDelayCompetingLowPriorityRequestsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kDelayCompetingLowPriorityRequests,
         kDelayCompetingLowPriorityRequestsFeatureVariations,
         "DelayCompetingLowPriorityRequests")},

    {"intensive-wake-up-throttling",
     flag_descriptions::kIntensiveWakeUpThrottlingName,
     flag_descriptions::kIntensiveWakeUpThrottlingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kIntensiveWakeUpThrottling,
                                    kIntensiveWakeUpThrottlingVariations,
                                    "IntensiveWakeUpThrottling")},

#if defined(OS_ANDROID)
    {"omnibox-spare-renderer", flag_descriptions::kOmniboxSpareRendererName,
     flag_descriptions::kOmniboxSpareRendererDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOmniboxSpareRenderer)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"double-tap-to-zoom-in-tablet-mode",
     flag_descriptions::kDoubleTapToZoomInTabletModeName,
     flag_descriptions::kDoubleTapToZoomInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDoubleTapToZoomInTabletMode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {flag_descriptions::kReadLaterFlagId, flag_descriptions::kReadLaterName,
     flag_descriptions::kReadLaterDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(reading_list::switches::kReadLater)},

#ifdef OS_ANDROID
    {"read-later-reminder-notification",
     flag_descriptions::kReadLaterReminderNotificationName,
     flag_descriptions::kReadLaterReminderNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         reading_list::switches::kReadLaterReminderNotification)},

    {"bookmark-bottom-sheet", flag_descriptions::kBookmarkBottomSheetName,
     flag_descriptions::kBookmarkBottomSheetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBookmarkBottomSheet)},
#endif

    {"tab-groups-auto-create", flag_descriptions::kTabGroupsAutoCreateName,
     flag_descriptions::kTabGroupsAutoCreateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsAutoCreate)},

    {"tab-groups-collapse-freezing",
     flag_descriptions::kTabGroupsCollapseFreezingName,
     flag_descriptions::kTabGroupsCollapseFreezingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsCollapseFreezing)},

    {"tab-groups-feedback", flag_descriptions::kTabGroupsFeedbackName,
     flag_descriptions::kTabGroupsFeedbackDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsFeedback)},

    {"tab-groups-new-badge-promo",
     flag_descriptions::kTabGroupsNewBadgePromoName,
     flag_descriptions::kTabGroupsNewBadgePromoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsNewBadgePromo)},

    {"new-tabstrip-animation", flag_descriptions::kNewTabstripAnimationName,
     flag_descriptions::kNewTabstripAnimationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNewTabstripAnimation)},

    {flag_descriptions::kScrollableTabStripFlagId,
     flag_descriptions::kScrollableTabStripName,
     flag_descriptions::kScrollableTabStripDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kScrollableTabStrip,
                                    kTabScrollingVariations,
                                    "TabScrolling")},

    {"scrollable-tabstrip-buttons",
     flag_descriptions::kScrollableTabStripButtonsName,
     flag_descriptions::kScrollableTabStripButtonsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kScrollableTabStripButtons)},

    {"side-panel", flag_descriptions::kSidePanelName,
     flag_descriptions::kSidePanelDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSidePanel)},

    {"tab-outlines-in-low-contrast-themes",
     flag_descriptions::kTabOutlinesInLowContrastThemesName,
     flag_descriptions::kTabOutlinesInLowContrastThemesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabOutlinesInLowContrastThemes)},

    {"prominent-dark-mode-active-tab-title",
     flag_descriptions::kProminentDarkModeActiveTabTitleName,
     flag_descriptions::kProminentDarkModeActiveTabTitleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kProminentDarkModeActiveTabTitle)},

    {"promo-browser-commands", flag_descriptions::kPromoBrowserCommandsName,
     flag_descriptions::kPromoBrowserCommandsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kPromoBrowserCommands,
                                    kPromoBrowserCommandsVariations,
                                    "PromoBrowserCommands")},
#if defined(OS_ANDROID)
    {"enable-reader-mode-in-cct", flag_descriptions::kReaderModeInCCTName,
     flag_descriptions::kReaderModeInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReaderModeInCCT)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"direct-manipulation-stylus",
     flag_descriptions::kDirectManipulationStylusName,
     flag_descriptions::kDirectManipulationStylusDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(features::kDirectManipulationStylus)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
    {"ntp-cache-one-google-bar", flag_descriptions::kNtpCacheOneGoogleBarName,
     flag_descriptions::kNtpCacheOneGoogleBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCacheOneGoogleBar)},

    {"ntp-iframe-one-google-bar", flag_descriptions::kNtpIframeOneGoogleBarName,
     flag_descriptions::kNtpIframeOneGoogleBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kIframeOneGoogleBar)},

    {"ntp-one-google-bar-modal-overlays",
     flag_descriptions::kNtpOneGoogleBarModalOverlaysName,
     flag_descriptions::kNtpOneGoogleBarModalOverlaysDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kOneGoogleBarModalOverlays)},

    {"ntp-repeatable-queries", flag_descriptions::kNtpRepeatableQueriesName,
     flag_descriptions::kNtpRepeatableQueriesDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpRepeatableQueries,
                                    kNtpRepeatableQueriesVariations,
                                    "NtpRepeatableQueries")},

    {"ntp-webui", flag_descriptions::kNtpWebUIName,
     flag_descriptions::kNtpWebUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kWebUI)},

    {"ntp-modules", flag_descriptions::kNtpModulesName,
     flag_descriptions::kNtpModulesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kModules)},

    {"ntp-drive-module", flag_descriptions::kNtpDriveModuleName,
     flag_descriptions::kNtpDriveModuleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpDriveModule)},

    {"ntp-recipe-tasks-module", flag_descriptions::kNtpRecipeTasksModuleName,
     flag_descriptions::kNtpRecipeTasksModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpRecipeTasksModule,
                                    kNtpRecipeTasksModuleVariations,
                                    "NtpRecipeTasksModule")},

    {"ntp-shopping-tasks-module",
     flag_descriptions::kNtpShoppingTasksModuleName,
     flag_descriptions::kNtpShoppingTasksModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpShoppingTasksModule,
                                    kNtpShoppingTasksModuleVariations,
                                    "NtpShoppingTasksModule")},

    {"ntp-chrome-cart-module", flag_descriptions::kNtpChromeCartModuleName,
     flag_descriptions::kNtpChromeCartModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpChromeCartModule,
                                    kNtpChromeCartModuleVariations,
                                    "NtpChromeCartModule")},
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

    {"download-later-debug-on-wifi",
     flag_descriptions::kDownloadLaterDebugOnWifiName,
     flag_descriptions::kDownloadLaterDebugOnWifiNameDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(download::switches::kDownloadLaterDebugOnWifi)},
#endif

    {"enable-new-download-backend",
     flag_descriptions::kEnableNewDownloadBackendName,
     flag_descriptions::kEnableNewDownloadBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         download::features::kUseDownloadOfflineContentProvider)},

#if defined(OS_ANDROID)
    {"screen-capture-android", flag_descriptions::kUserMediaScreenCapturingName,
     flag_descriptions::kUserMediaScreenCapturingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kUserMediaScreenCapturing)},
#endif

#if defined(OS_ANDROID)
    {"prefetch-notification-scheduling-integration",
     flag_descriptions::kPrefetchNotificationSchedulingIntegrationName,
     flag_descriptions::kPrefetchNotificationSchedulingIntegrationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kPrefetchNotificationSchedulingIntegration)},
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"chrome-tips-in-main-menu", flag_descriptions::kChromeTipsInMainMenuName,
     flag_descriptions::kChromeTipsInMainMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kChromeTipsInMainMenu)},
#endif

    {"tab-hover-cards", flag_descriptions::kTabHoverCardsName,
     flag_descriptions::kTabHoverCardsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kTabHoverCards,
                                    kTabHoverCardsFeatureVariations,
                                    "TabHoverCards")},

    {"tab-hover-card-images", flag_descriptions::kTabHoverCardImagesName,
     flag_descriptions::kTabHoverCardImagesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabHoverCardImages)},

    {"stop-in-background", flag_descriptions::kStopInBackgroundName,
     flag_descriptions::kStopInBackgroundDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kStopInBackground)},

    {"enable-storage-pressure-event",
     flag_descriptions::kStoragePressureEventName,
     flag_descriptions::kStoragePressureEventDescription, kOsAll,
     FEATURE_VALUE_TYPE(storage::features::kStoragePressureEvent)},

    {"installed-apps-in-cbd", flag_descriptions::kInstalledAppsInCbdName,
     flag_descriptions::kInstalledAppsInCbdDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kInstalledAppsInCbd)},

    {"enable-network-logging-to-file",
     flag_descriptions::kEnableNetworkLoggingToFileName,
     flag_descriptions::kEnableNetworkLoggingToFileDescription, kOsAll,
     SINGLE_VALUE_TYPE(network::switches::kLogNetLog)},

    {"enable-web-authentication-cable-v2-support",
     flag_descriptions::kEnableWebAuthenticationCableV2SupportName,
     flag_descriptions::kEnableWebAuthenticationCableV2SupportDescription,
     kOsDesktop | kOsAndroid, FEATURE_VALUE_TYPE(device::kWebAuthPhoneSupport)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-web-authentication-chromeos-authenticator",
     flag_descriptions::kEnableWebAuthenticationChromeOSAuthenticatorName,
     flag_descriptions::
         kEnableWebAuthenticationChromeOSAuthenticatorDescription,
     kOsCrOS, FEATURE_VALUE_TYPE(device::kWebAuthCrosPlatformAuthenticator)},
#endif
#if BUILDFLAG(ENABLE_PDF)
    {"accessible-pdf-form", flag_descriptions::kAccessiblePDFFormName,
     flag_descriptions::kAccessiblePDFFormDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kAccessiblePDFForm)},

    {"pdf-viewer-document-properties",
     flag_descriptions::kPdfViewerDocumentPropertiesName,
     flag_descriptions::kPdfViewerDocumentPropertiesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfViewerDocumentProperties)},

    {"pdf-viewer-presentation-mode",
     flag_descriptions::kPdfViewerPresentationModeName,
     flag_descriptions::kPdfViewerPresentationModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfViewerPresentationMode)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINTING)
#if defined(OS_MAC)
    {"cups-ipp-printing-backend",
     flag_descriptions::kCupsIppPrintingBackendName,
     flag_descriptions::kCupsIppPrintingBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(printing::features::kCupsIppPrintingBackend)},
#endif  // defined(OS_MAC)

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

    {"autofill-restrict-formless-form-extraction",
     flag_descriptions::kAutofillRestrictUnownedFieldsToFormlessCheckoutName,
     flag_descriptions::
         kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout)},

#if defined(OS_WIN)
    {"enable-windows-gaming-input-data-fetcher",
     flag_descriptions::kEnableWindowsGamingInputDataFetcherName,
     flag_descriptions::kEnableWindowsGamingInputDataFetcherDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableWindowsGamingInputDataFetcher)},
#endif

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

    {"enable-critical-persisted-tab-data",
     flag_descriptions::kCriticalPersistedTabDataName,
     flag_descriptions::kCriticalPersistedTabDataDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCriticalPersistedTabData)},

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

    {"enable-table-ng", flag_descriptions::kEnableLayoutNGTableName,
     flag_descriptions::kEnableLayoutNGTableDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kLayoutNGTable)},

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

    {"autofill-enable-toolbar-status-chip",
     flag_descriptions::kAutofillEnableToolbarStatusChipName,
     flag_descriptions::kAutofillEnableToolbarStatusChipDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableToolbarStatusChip)},

    {"autofill-rich-metadata-queries",
     flag_descriptions::kAutofillRichMetadataQueriesName,
     flag_descriptions::kAutofillRichMetadataQueriesDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRichMetadataQueries)},

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-app-data-search", flag_descriptions::kEnableAppDataSearchName,
     flag_descriptions::kEnableAppDataSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppDataSearch)},

    {"enable-app-grid-ghost", flag_descriptions::kEnableAppGridGhostName,
     flag_descriptions::kEnableAppGridGhostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppGridGhost)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
    {"enable-accessibility-live-caption",
     flag_descriptions::kEnableAccessibilityLiveCaptionName,
     flag_descriptions::kEnableAccessibilityLiveCaptionDescription, kOsDesktop,
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

#if defined(OS_ANDROID)
    {"cct-incognito-available-to-third-party",
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyName,
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognitoAvailableToThirdParty)},
#endif

#if defined(OS_ANDROID)
    {"background-task-component-update",
     flag_descriptions::kBackgroundTaskComponentUpdateName,
     flag_descriptions::kBackgroundTaskComponentUpdateDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBackgroundTaskComponentUpdate)},
#endif

#if defined(OS_ANDROID)
    {"enable-use-aaudio-driver", flag_descriptions::kEnableUseAaudioDriverName,
     flag_descriptions::kEnableUseAaudioDriverDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kUseAAudioDriver)},
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-zero-state-app-reinstall-suggestions",
     flag_descriptions::kEnableAppReinstallZeroStateName,
     flag_descriptions::kEnableAppReinstallZeroStateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppReinstallZeroState)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-resampling-input-events",
     flag_descriptions::kEnableResamplingInputEventsName,
     flag_descriptions::kEnableResamplingInputEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kResamplingInputEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingInputEvents")},

    {"enable-resampling-scroll-events",
     flag_descriptions::kEnableResamplingScrollEventsName,
     flag_descriptions::kEnableResamplingScrollEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kResamplingScrollEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingScrollEvents")},

    // Should only be available if kResamplingScrollEvents is on, and using
    // linear resampling.
    {"enable-resampling-scroll-events-experimental-prediction",
     flag_descriptions::kEnableResamplingScrollEventsExperimentalPredictionName,
     flag_descriptions::
         kEnableResamplingScrollEventsExperimentalPredictionDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ::features::kResamplingScrollEventsExperimentalPrediction,
         kResamplingScrollEventsExperimentalPredictionVariations,
         "ResamplingScrollEventsExperimentalLatency")},

    {"enable-filtering-scroll-events",
     flag_descriptions::kFilteringScrollPredictionName,
     flag_descriptions::kFilteringScrollPredictionDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kFilteringScrollPrediction,
                                    kFilteringPredictionFeatureVariations,
                                    "FilteringScrollPrediction")},

    {"compositor-threaded-scrollbar-scrolling",
     flag_descriptions::kCompositorThreadedScrollbarScrollingName,
     flag_descriptions::kCompositorThreadedScrollbarScrollingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kCompositorThreadedScrollbarScrolling)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

    {"happiness-tracking-surveys-for-desktop-devtools-issues-cookies-same-site",
     flag_descriptions::
         kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSiteName,
     flag_descriptions::
         kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSiteDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         features::
             kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite)},

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

    {"enable-sign-in-profile-creation",
     flag_descriptions::kSignInProfileCreationName,
     flag_descriptions::kSignInProfileCreationDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kSignInProfileCreation)},

    {"enable-sign-in-profile-creation-enterprise",
     flag_descriptions::kSignInProfileCreationEnterpriseName,
     flag_descriptions::kSignInProfileCreationEnterpriseDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kSignInProfileCreationEnterprise)},
#endif

    {"destroy-profile-on-browser-close",
     flag_descriptions::kDestroyProfileOnBrowserCloseName,
     flag_descriptions::kDestroyProfileOnBrowserCloseDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kDestroyProfileOnBrowserClose)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-fs-nosymfollow", flag_descriptions::kFsNosymfollowName,
     flag_descriptions::kFsNosymfollowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFsNosymfollow)},
    {"enable-arc-unified-audio-focus",
     flag_descriptions::kEnableArcUnifiedAudioFocusName,
     flag_descriptions::kEnableArcUnifiedAudioFocusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableUnifiedAudioFocusFeature)},
    {"enable-kerberos-settings-section",
     flag_descriptions::kKerberosSettingsSectionName,
     flag_descriptions::kKerberosSettingsSectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kKerberosSettingsSection)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescription, kOsWin,
     MULTI_VALUE_TYPE(kUseAngleChoices)},
#endif
#if defined(OS_ANDROID)
    {"enable-ephemeral-tab-bottom-sheet",
     flag_descriptions::kEphemeralTabUsingBottomSheetName,
     flag_descriptions::kEphemeralTabUsingBottomSheetDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kEphemeralTabUsingBottomSheet,
         kEphemeralTabOpenVariations,
         "EphemeralTabOpenMode")},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

    {"enable-quick-answers-on-editable-text",
     flag_descriptions::kEnableQuickAnswersOnEditableTextName,
     flag_descriptions::kEnableQuickAnswersOnEditableTextDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersOnEditableText)},

    {"enable-quick-answers-text-annotator",
     flag_descriptions::kEnableQuickAnswersTextAnnotatorName,
     flag_descriptions::kEnableQuickAnswersTextAnnotatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersTextAnnotator)},

    {"enable-quick-answers-translation",
     flag_descriptions::kEnableQuickAnswersTranslationName,
     flag_descriptions::kEnableQuickAnswersTranslationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersTranslation)},

    {"enable-quick-answers-translation-cloud-api",
     flag_descriptions::kEnableQuickAnswersTranslationCloudAPIName,
     flag_descriptions::kEnableQuickAnswersTranslationCloudAPIDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersTranslationCloudAPI)},

    {kAssistantBetterOnboardingInternalName,
     flag_descriptions::kEnableAssistantBetterOnboardingName,
     flag_descriptions::kEnableAssistantBetterOnboardingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kAssistantBetterOnboarding)},

    {kAssistantTimersV2InternalName,
     flag_descriptions::kEnableAssistantTimersV2Name,
     flag_descriptions::kEnableAssistantTimersV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantTimersV2)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
    {"click-to-call-ui", flag_descriptions::kClickToCallUIName,
     flag_descriptions::kClickToCallUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kClickToCallUI)},
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
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
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

    {"restrict-gamepad-access", flag_descriptions::kRestrictGamepadAccessName,
     flag_descriptions::kRestrictGamepadAccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kRestrictGamepadAccess)},

    {"shared-clipboard-ui", flag_descriptions::kSharedClipboardUIName,
     flag_descriptions::kSharedClipboardUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharedClipboardUI)},

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"ash-enable-pip-rounded-corners",
     flag_descriptions::kAshEnablePipRoundedCornersName,
     flag_descriptions::kAshEnablePipRoundedCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipRoundedCorners)},

    {"ash-swap-side-volume-buttons-for-orientation",
     flag_descriptions::kAshSwapSideVolumeButtonsForOrientationName,
     flag_descriptions::kAshSwapSideVolumeButtonsForOrientationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSwapSideVolumeButtonsForOrientation)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-stereo-input",
     flag_descriptions::kEnableGoogleAssistantStereoInputName,
     flag_descriptions::kEnableGoogleAssistantStereoInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kEnableStereoAudioInput)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"app-service-adaptive-icon",
     flag_descriptions::kAppServiceAdaptiveIconName,
     flag_descriptions::kAppServiceAdaptiveIconDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceAdaptiveIcon)},

    {"app-service-external-protocol",
     flag_descriptions::kAppServiceExternalProtocolName,
     flag_descriptions::kAppServiceExternalProtocolDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceExternalProtocol)},

    {"full-restore", flag_descriptions::kFullRestoreName,
     flag_descriptions::kFullRestoreDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFullRestore)},

    {"use-fake-device-for-media-stream",
     flag_descriptions::kUseFakeDeviceForMediaStreamName,
     flag_descriptions::kUseFakeDeviceForMediaStreamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseFakeDeviceForMediaStream)},

    {"intent-picker-pwa-persistence",
     flag_descriptions::kIntentPickerPWAPersistenceName,
     flag_descriptions::kIntentPickerPWAPersistenceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kIntentPickerPWAPersistence)},

    {"intent-handling-sharing", flag_descriptions::kIntentHandlingSharingName,
     flag_descriptions::kIntentHandlingSharingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kIntentHandlingSharing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"d3d11-video-decoder", flag_descriptions::kD3D11VideoDecoderName,
     flag_descriptions::kD3D11VideoDecoderDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kD3D11VideoDecoder)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    {"chromeos-direct-video-decoder",
     flag_descriptions::kChromeOSDirectVideoDecoderName,
     flag_descriptions::kChromeOSDirectVideoDecoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseChromeOSDirectVideoDecoder)},
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

    {"autofill-assistant-proactive-help",
     flag_descriptions::kAutofillAssistantProactiveHelpName,
     flag_descriptions::kAutofillAssistantProactiveHelpDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill_assistant::features::kAutofillAssistantProactiveHelp)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"deprecate-menagerie-api", flag_descriptions::kDeprecateMenagerieAPIName,
     flag_descriptions::kDeprecateMenagerieAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kDeprecateMenagerieAPI)},
    {"mobile-identity-consistency",
     flag_descriptions::kMobileIdentityConsistencyName,
     flag_descriptions::kMobileIdentityConsistencyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(signin::kMobileIdentityConsistency)},
    {"mobile-identity-consistency-var",
     flag_descriptions::kMobileIdentityConsistencyVarName,
     flag_descriptions::kMobileIdentityConsistencyVarDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(signin::kMobileIdentityConsistencyVar,
                                    kMobileIdentityConsistencyVariations,
                                    "MobileIdentityConsistencyVar")},
#endif  // defined(OS_ANDROID)

    {"autofill-use-improved-label-disambiguation",
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationName,
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUseImprovedLabelDisambiguation)},

    {"file-handling-api", flag_descriptions::kFileHandlingAPIName,
     flag_descriptions::kFileHandlingAPIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kFileHandlingAPI)},

#if defined(TOOLKIT_VIEWS)
    {"installable-ink-drop", flag_descriptions::kInstallableInkDropName,
     flag_descriptions::kInstallableInkDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::kInstallableInkDropFeature)},
#endif  // defined(TOOLKIT_VIEWS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-launcher-integration",
     flag_descriptions::kEnableAssistantLauncherIntegrationName,
     flag_descriptions::kEnableAssistantLauncherIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAssistantSearch)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(TOOLKIT_VIEWS)

    {"enable-new-badge-on-menu-items",
     flag_descriptions::kEnableNewBadgeOnMenuItemsName,
     flag_descriptions::kEnableNewBadgeOnMenuItemsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::features::kEnableNewBadgeOnMenuItems)},
#endif  // defined(TOOLKIT_VIEWS)

    {"strict-origin-isolation", flag_descriptions::kStrictOriginIsolationName,
     flag_descriptions::kStrictOriginIsolationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kStrictOriginIsolation)},

#if defined(OS_ANDROID)
    {"enable-logging-js-console-messages",
     flag_descriptions::kLogJsConsoleMessagesName,
     flag_descriptions::kLogJsConsoleMessagesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kLogJsConsoleMessages)},
#endif  // OS_ANDROID

    {"enable-skia-renderer", flag_descriptions::kSkiaRendererName,
     flag_descriptions::kSkiaRendererDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUseSkiaRenderer)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"allow-disable-mouse-acceleration",
     flag_descriptions::kAllowDisableMouseAccelerationName,
     flag_descriptions::kAllowDisableMouseAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAllowDisableMouseAcceleration)},

    {"allow-repeated-updates", flag_descriptions::kAllowRepeatedUpdatesName,
     flag_descriptions::kAllowRepeatedUpdatesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAllowRepeatedUpdates)},

    {"allow-scroll-settings", flag_descriptions::kAllowScrollSettingsName,
     flag_descriptions::kAllowScrollSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAllowScrollSettings)},

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

    {"enable-pci-guard-ui", flag_descriptions::kEnablePciguardUiName,
     flag_descriptions::kEnablePciguardUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnablePciguardUi)},

    {"enable-heuristic-stylus-palm-rejection",
     flag_descriptions::kEnableHeuristicStylusPalmRejectionName,
     flag_descriptions::kEnableHeuristicStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHeuristicPalmDetectionFilter)},

    {"enable-hide-arc-media-notifications",
     flag_descriptions::kHideArcMediaNotificationsName,
     flag_descriptions::kHideArcMediaNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideArcMediaNotifications)},

    {"media-notifications-counter",
     flag_descriptions::kMediaNotificationsCounterName,
     flag_descriptions::kMediaNotificationsCounterDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMediaNotificationsCounter)},

    {"reduce-display-notifications",
     flag_descriptions::kReduceDisplayNotificationsName,
     flag_descriptions::kReduceDisplayNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kReduceDisplayNotifications)},

    {"use-search-click-for-right-click",
     flag_descriptions::kUseSearchClickForRightClickName,
     flag_descriptions::kUseSearchClickForRightClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseSearchClickForRightClick)},

    {"show-metered-toggle", flag_descriptions::kMeteredShowToggleName,
     flag_descriptions::kMeteredShowToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kMeteredShowToggle)},

    {"printer-status", flag_descriptions::kPrinterStatusName,
     flag_descriptions::kPrinterStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPrinterStatus)},

    {"printer-status-dialog", flag_descriptions::kPrinterStatusDialogName,
     flag_descriptions::kPrinterStatusDialogDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPrinterStatusDialog)},

    {"enable-phone-hub", flag_descriptions::kPhoneHubName,
     flag_descriptions::kPhoneHubDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPhoneHub)},

    {"wifi-sync-android", flag_descriptions::kWifiSyncAndroidName,
     flag_descriptions::kWifiSyncAndroidDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kWifiSyncAndroid)},

    {"display-identification", flag_descriptions::kDisplayIdentificationName,
     flag_descriptions::kDisplayIdentificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisplayIdentification)},

    {"display-alignment-assistance",
     flag_descriptions::kDisplayAlignmentAssistanceName,
     flag_descriptions::kDisplayAlignmentAssistanceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisplayAlignAssist)},

    {"print-save-to-drive", flag_descriptions::kPrintSaveToDriveName,
     flag_descriptions::kPrintSaveToDriveDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPrintSaveToDrive)},

    {"diagnostics-app", flag_descriptions::kDiagnosticsAppName,
     flag_descriptions::kDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDiagnosticsApp)},

    {"enable-hostname-setting", flag_descriptions::kEnableHostnameSettingName,
     flag_descriptions::kEnableHostnameSettingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableHostnameSetting)},

    {"webui-dark-mode", flag_descriptions::kWebuiDarkModeName,
     flag_descriptions::kWebuiDarkModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebUIDarkMode)},

    {"select-to-speak-navigation-control",
     flag_descriptions::kSelectToSpeakNavigationControlName,
     flag_descriptions::kSelectToSpeakNavigationControlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSelectToSpeakNavigationControl)},

    {"print-server-scaling", flag_descriptions::kPrintServerScalingName,
     flag_descriptions::kPrintServerScalingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPrintServerScaling)},

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if defined(OS_MAC)
    {"enterprise-reporting-api-keychain-recreation",
     flag_descriptions::kEnterpriseReportingApiKeychainRecreationName,
     flag_descriptions::kEnterpriseReportingApiKeychainRecreationDescription,
     kOsMac,
     FEATURE_VALUE_TYPE(features::kEnterpriseReportingApiKeychainRecreation)},
#endif  // defined(OS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enterprise-reporting-in-chromeos",
     flag_descriptions::kEnterpriseReportingInChromeOSName,
     flag_descriptions::kEnterpriseReportingInChromeOSDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnterpriseReportingInChromeOS)},
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
    {"enterprise-realtime-extension-request",
     flag_descriptions::kEnterpriseRealtimeExtensionRequestName,
     flag_descriptions::kEnterpriseRealtimeExtensionRequestDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnterpriseRealtimeExtensionRequest)},
#endif  // !defined(OS_ANDROID)

    {"enable-unsafe-webgpu", flag_descriptions::kUnsafeWebGPUName,
     flag_descriptions::kUnsafeWebGPUDescription, kOsMac | kOsLinux | kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeWebGPU)},

    {"enable-unsafe-fast-js-calls", flag_descriptions::kUnsafeFastJSCallsName,
     flag_descriptions::kUnsafeFastJSCallsDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeFastJSCalls)},

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

    {"allow-sync-xhr-in-page-dismissal",
     flag_descriptions::kAllowSyncXHRInPageDismissalName,
     flag_descriptions::kAllowSyncXHRInPageDismissalDescription,
     kOsAll | kDeprecated,
     FEATURE_VALUE_TYPE(blink::features::kAllowSyncXHRInPageDismissal)},

    {"enable-sync-requires-policies-loaded",
     flag_descriptions::kEnableSyncRequiresPoliciesLoadedName,
     flag_descriptions::kEnableSyncRequiresPoliciesLoadedDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncRequiresPoliciesLoaded)},

    {"enable-policy-blocklist-throttle-requires-policies-loaded",
     flag_descriptions::
         kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedName,
     flag_descriptions::
         kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         policy::features::kPolicyBlocklistThrottleRequiresPoliciesLoaded)},

#if !defined(OS_ANDROID)
    {"form-controls-dark-mode", flag_descriptions::kFormControlsDarkModeName,
     flag_descriptions::kFormControlsDarkModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCSSColorSchemeUARendering)},
#endif  // !defined(OS_ANDROID)

    {"form-controls-refresh", flag_descriptions::kFormControlsRefreshName,
     flag_descriptions::kFormControlsRefreshDescription,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kFormControlsRefresh)},

    {"color-picker-eye-dropper", flag_descriptions::kColorPickerEyeDropperName,
     flag_descriptions::kColorPickerEyeDropperDescription, kOsWin | kOsMac,
     FEATURE_VALUE_TYPE(features::kEyeDropper)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"auto-screen-brightness", flag_descriptions::kAutoScreenBrightnessName,
     flag_descriptions::kAutoScreenBrightnessDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAutoScreenBrightness)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"audio-worklet-realtime-thread",
     flag_descriptions::kAudioWorkletRealtimeThreadName,
     flag_descriptions::kAudioWorkletRealtimeThreadDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAudioWorkletRealtimeThread)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"smart-dim-model-v3", flag_descriptions::kSmartDimModelV3Name,
     flag_descriptions::kSmartDimModelV3Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSmartDimModelV3)},
    {"smart-dim-new-ml-agent", flag_descriptions::kSmartDimNewMlAgentName,
     flag_descriptions::kSmartDimNewMlAgentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSmartDimNewMlAgent)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"privacy-reordered-android",
     flag_descriptions::kPrivacyReorderedAndroidName,
     flag_descriptions::kPrivacyReorderedAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kPrivacyReorderedAndroid)},
#endif

    {"privacy-sandbox-settings", flag_descriptions::kPrivacySandboxSettingsName,
     flag_descriptions::kPrivacySandboxSettingsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kPrivacySandboxSettings)},

#if defined(OS_ANDROID)
    {"safety-check-android", flag_descriptions::kSafetyCheckAndroidName,
     flag_descriptions::kSafetyCheckAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSafetyCheckAndroid)},
#endif

#if defined(OS_ANDROID)
    {"metrics-settings-android", flag_descriptions::kMetricsSettingsAndroidName,
     flag_descriptions::kMetricsSettingsAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kMetricsSettingsAndroid,
                                    kMetricsSettingsAndroidVariations,
                                    "MetricsSettingsAndroid")},
#endif

#if defined(OS_ANDROID)
    {"safe-browsing-client-side-detection-android",
     flag_descriptions::kSafeBrowsingClientSideDetectionAndroidName,
     flag_descriptions::kSafeBrowsingClientSideDetectionAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(safe_browsing::kClientSideDetectionForAndroid)},

    {"safe-browsing-enhanced-protection-android",
     flag_descriptions::kSafeBrowsingEnhancedProtectionAndroidName,
     flag_descriptions::kSafeBrowsingEnhancedProtectionAndroidDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(safe_browsing::kEnhancedProtection)},

    {"safe-browsing-enhanced-protection-promo-android",
     flag_descriptions::kEnhancedProtectionPromoAndroidName,
     flag_descriptions::kEnhancedProtectionPromoAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEnhancedProtectionPromoCard)},

    {"safe-browsing-security-section-ui-android",
     flag_descriptions::kSafeBrowsingSectionUiAndroidName,
     flag_descriptions::kSafeBrowsingSectionUiAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(safe_browsing::kSafeBrowsingSectionUIAndroid)},
#endif

    {"safe-browsing-enhanced-protection-message-in-interstitials",
     flag_descriptions::
         kSafeBrowsingEnhancedProtectionMessageInInterstitialsName,
     flag_descriptions::
         kSafeBrowsingEnhancedProtectionMessageInInterstitialsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         safe_browsing::kEnhancedProtectionMessageInInterstitials)},

    {"safe-browsing-real-time-url-lookup-enterprise-ga-endpoint",
     flag_descriptions::kSafeBrowsingRealTimeUrlLookupEnterpriseGaEndpointName,
     flag_descriptions::
         kSafeBrowsingRealTimeUrlLookupEnterpriseGaEndpointDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(safe_browsing::kRealTimeUrlLookupEnterpriseGaEndpoint)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"gesture-properties-dbus-service",
     flag_descriptions::kEnableGesturePropertiesDBusServiceName,
     flag_descriptions::kEnableGesturePropertiesDBusServiceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGesturePropertiesDBusService)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

    {"font-access-persistent", flag_descriptions::kFontAccessPersistentName,
     flag_descriptions::kFontAccessPersistentDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFontAccessPersistent)},

    {"compute-pressure", flag_descriptions::kComputePressureAPIName,
     flag_descriptions::kComputePressureAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kComputePressure)},

    {"mouse-subframe-no-implicit-capture",
     flag_descriptions::kMouseSubframeNoImplicitCaptureName,
     flag_descriptions::kMouseSubframeNoImplicitCaptureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kMouseSubframeNoImplicitCapture)},

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"global-media-controls", flag_descriptions::kGlobalMediaControlsName,
     flag_descriptions::kGlobalMediaControlsDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControls)},

    {"global-media-controls-for-cast",
     flag_descriptions::kGlobalMediaControlsForCastName,
     flag_descriptions::kGlobalMediaControlsForCastDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsForCast)},

    {"global-media-controls-for-chromeos",
     flag_descriptions::kGlobalMediaControlsForChromeOSName,
     flag_descriptions::kGlobalMediaControlsForChromeOSDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsForChromeOS)},

    {"global-media-controls-modern-ui",
     flag_descriptions::kGlobalMediaControlsModernUIName,
     flag_descriptions::kGlobalMediaControlsModernUIDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsModernUI)},

    {"global-media-controls-picture-in-picture",
     flag_descriptions::kGlobalMediaControlsPictureInPictureName,
     flag_descriptions::kGlobalMediaControlsPictureInPictureDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsPictureInPicture)},

    {"global-media-controls-seamless-transfer",
     flag_descriptions::kGlobalMediaControlsSeamlessTransferName,
     flag_descriptions::kGlobalMediaControlsSeamlessTransferDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsSeamlessTransfer)},

    {"global-media-controls-overlay-controls",
     flag_descriptions::kGlobalMediaControlsOverlayControlsName,
     flag_descriptions::kGlobalMediaControlsOverlayControlsDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsOverlayControls)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_WIN)
    {"win-use-native-spellchecker",
     flag_descriptions::kWinUseBrowserSpellCheckerName,
     flag_descriptions::kWinUseBrowserSpellCheckerDescription, kOsWin,
     FEATURE_VALUE_TYPE(spellcheck::kWinUseBrowserSpellChecker)},
#endif  // BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_WIN)

    {"safety-tips", flag_descriptions::kSafetyTipName,
     flag_descriptions::kSafetyTipDescription, kOsAll,
     FEATURE_VALUE_TYPE(security_state::features::kSafetyTipUI)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"crostini-webui-upgrader", flag_descriptions::kCrostiniWebUIUpgraderName,
     flag_descriptions::kCrostiniWebUIUpgraderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniWebUIUpgrader)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"turn-off-streaming-media-caching-on-battery",
     flag_descriptions::kTurnOffStreamingMediaCachingOnBatteryName,
     flag_descriptions::kTurnOffStreamingMediaCachingOnBatteryDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTurnOffStreamingMediaCachingOnBattery)},

    {"turn-off-streaming-media-caching-always",
     flag_descriptions::kTurnOffStreamingMediaCachingAlwaysName,
     flag_descriptions::kTurnOffStreamingMediaCachingAlwaysDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTurnOffStreamingMediaCachingAlways)},

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-routines",
     flag_descriptions::kEnableAssistantRoutinesName,
     flag_descriptions::kEnableAssistantRoutinesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantRoutines)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"notification-scheduler", flag_descriptions::kNotificationSchedulerName,
     flag_descriptions::kNotificationSchedulerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(notifications::features::kNotificationScheduleService)},

    {"notification-scheduler-debug-options",
     flag_descriptions::kNotificationSchedulerDebugOptionName,
     flag_descriptions::kNotificationSchedulerDebugOptionDescription,
     kOsAndroid, MULTI_VALUE_TYPE(kNotificationSchedulerChoices)},

#if defined(OS_ANDROID)

    {"debug-chime-notification",
     flag_descriptions::kChimeAlwaysShowNotificationName,
     flag_descriptions::kChimeAlwaysShowNotificationDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(notifications::switches::kDebugChimeNotification)},

    {"use-chime-android-sdk", flag_descriptions::kChimeAndroidSdkName,
     flag_descriptions::kChimeAndroidSdkDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(notifications::features::kUseChimeAndroidSdk)},

#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"lock-screen-media-controls",
     flag_descriptions::kLockScreenMediaControlsName,
     flag_descriptions::kLockScreenMediaControlsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenMediaControls)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"contextual-nudges", flag_descriptions::kContextualNudgesName,
     flag_descriptions::kContextualNudgesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kContextualNudges)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    {"abusive-notification-permission-revocation",
     flag_descriptions::kAbusiveNotificationPermissionRevocationName,
     flag_descriptions::kAbusiveNotificationPermissionRevocationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kAbusiveNotificationPermissionRevocation)},

#if defined(OS_ANDROID)
    {"context-menu-google-lens-chip",
     flag_descriptions::kContextMenuGoogleLensChipName,
     flag_descriptions::kContextMenuGoogleLensChipDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuGoogleLensChip)},

    {"context-menu-search-with-google-lens",
     flag_descriptions::kContextMenuSearchWithGoogleLensName,
     flag_descriptions::kContextMenuSearchWithGoogleLensDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuSearchWithGoogleLens)},

    {"context-menu-shop-with-google-lens",
     flag_descriptions::kContextMenuShopWithGoogleLensName,
     flag_descriptions::kContextMenuShopWithGoogleLensDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kContextMenuShopWithGoogleLens,
         kContextMenuShopWithGoogleLensShopVariations,
         "ContextMenuShopWithGoogleLens")},

    {"context-menu-search-and-shop-with-google-lens",
     flag_descriptions::kContextMenuSearchAndShopWithGoogleLensName,
     flag_descriptions::kContextMenuSearchAndShopWithGoogleLensDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kContextMenuSearchAndShopWithGoogleLens)},

    {"context-menu-translate-with-google-lens",
     flag_descriptions::kContextMenuTranslateWithGoogleLensName,
     flag_descriptions::kContextMenuTranslateWithGoogleLensDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuTranslateWithGoogleLens)},

    {"lens-camera-assisted-search",
     flag_descriptions::kLensCameraAssistedSearchName,
     flag_descriptions::kLensCameraAssistedSearchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kLensCameraAssistedSearch)},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-suggested-files", flag_descriptions::kEnableSuggestedFilesName,
     flag_descriptions::kEnableSuggestedFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableSuggestedFiles)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"passwords-account-storage",
     flag_descriptions::kEnablePasswordsAccountStorageName,
     flag_descriptions::kEnablePasswordsAccountStorageDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kEnablePasswordsAccountStorage,
         kPasswordsAccountStorageVariations,
         "ButterForPasswords")},

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"passwords-account-storage-iph",
     flag_descriptions::kEnablePasswordsAccountStorageIPHName,
     flag_descriptions::kEnablePasswordsAccountStorageIPHDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHPasswordsAccountStorageFeature)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

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

    {"impulse-scroll-animations",
     flag_descriptions::kImpulseScrollAnimationsName,
     flag_descriptions::kImpulseScrollAnimationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kImpulseScrollAnimations)},

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

#if defined(OS_MAC)
    {"enable-core-location-backend",
     flag_descriptions::kMacCoreLocationBackendName,
     flag_descriptions::kMacCoreLocationBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacCoreLocationBackend)},
#endif

#if defined(OS_MAC)
    {"enable-core-location-implementation",
     flag_descriptions::kMacCoreLocationImplementationName,
     flag_descriptions::kMacCoreLocationImplementationDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacCoreLocationImplementation)},
#endif

#if defined(OS_MAC)
    {"enable-new-mac-notification-api",
     flag_descriptions::kNewMacNotificationAPIName,
     flag_descriptions::kNewMacNotificationAPIDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kNewMacNotificationAPI)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"exo-gamepad-vibration", flag_descriptions::kExoGamepadVibrationName,
     flag_descriptions::kExoGamepadVibrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGamepadVibration)},
    {"exo-lock-notification", flag_descriptions::kExoLockNotificationName,
     flag_descriptions::kExoLockNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoLockNotification)},
    {"exo-ordinal-motion", flag_descriptions::kExoOrdinalMotionName,
     flag_descriptions::kExoOrdinalMotionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoOrdinalMotion)},
    {"exo-pointer-lock", flag_descriptions::kExoPointerLockName,
     flag_descriptions::kExoPointerLockDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoPointerLock)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
    {"metal", flag_descriptions::kMetalName,
     flag_descriptions::kMetalDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMetal)},
#endif

    {"enable-de-jelly", flag_descriptions::kEnableDeJellyName,
     flag_descriptions::kEnableDeJellyDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableDeJelly)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-cros-action-recorder",
     flag_descriptions::kEnableCrOSActionRecorderName,
     flag_descriptions::kEnableCrOSActionRecorderDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kEnableCrOSActionRecorderChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
     FEATURE_VALUE_TYPE(blink::features::kFtpProtocol)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"crostini-use-buster-image",
     flag_descriptions::kCrostiniUseBusterImageName,
     flag_descriptions::kCrostiniUseBusterImageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseBusterImage)},
    {"crostini-disk-resizing", flag_descriptions::kCrostiniDiskResizingName,
     flag_descriptions::kCrostiniDiskResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniDiskResizing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"homepage-promo-card", flag_descriptions::kHomepagePromoCardName,
     flag_descriptions::kHomepagePromoCardDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kHomepagePromoCard,
                                    kHomepagePromoCardVariations,
                                    "HomepagePromoAndroid")},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"split-settings-sync", flag_descriptions::kSplitSettingsSyncName,
     flag_descriptions::kSplitSettingsSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSplitSettingsSync)},
    {"os-settings-deep-linking", flag_descriptions::kOsSettingsDeepLinkingName,
     flag_descriptions::kOsSettingsDeepLinkingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOsSettingsDeepLinking)},
    {"help-app-search-service-integration",
     flag_descriptions::kHelpAppSearchServiceIntegrationName,
     flag_descriptions::kHelpAppSearchServiceIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHelpAppSearchServiceIntegration)},
    {"media-app", flag_descriptions::kMediaAppName,
     flag_descriptions::kMediaAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaApp)},
    {"media-app-annotation", flag_descriptions::kMediaAppAnnotationName,
     flag_descriptions::kMediaAppAnnotationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppAnnotation)},
    {"media-app-pdf-in-ink", flag_descriptions::kMediaAppPdfInInkName,
     flag_descriptions::kMediaAppPdfInInkDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppPdfInInk)},
    {"os-settings-polymer3", flag_descriptions::kOsSettingsPolymer3Name,
     flag_descriptions::kOsSettingsPolymer3Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOsSettingsPolymer3)},
    {"release-notes-notification",
     flag_descriptions::kReleaseNotesNotificationName,
     flag_descriptions::kReleaseNotesNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kReleaseNotesNotification)},
    {"release-notes-notification-all-channels",
     flag_descriptions::kReleaseNotesNotificationAllChannelsName,
     flag_descriptions::kReleaseNotesNotificationAllChannelsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kReleaseNotesNotificationAllChannels)},
    {"use-wallpaper-staging-url",
     flag_descriptions::kUseWallpaperStagingUrlName,
     flag_descriptions::kUseWallpaperStagingUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseWallpaperStagingUrl)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-virtual-card",
     flag_descriptions::kAutofillEnableVirtualCardName,
     flag_descriptions::kAutofillEnableVirtualCardDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableVirtualCard)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"account-id-migration", flag_descriptions::kAccountIdMigrationName,
     flag_descriptions::kAccountIdMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(switches::kAccountIdMigration)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // TODO(https://crbug.com/1032161): Implement and enable for ChromeOS.
    {"raw-clipboard", flag_descriptions::kRawClipboardName,
     flag_descriptions::kRawClipboardDescription, kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(blink::features::kRawClipboard)},

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
    {"paint-preview-demo", flag_descriptions::kPaintPreviewDemoName,
     flag_descriptions::kPaintPreviewDemoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(paint_preview::kPaintPreviewDemo)},
    {"paint-preview-startup", flag_descriptions::kPaintPreviewStartupName,
     flag_descriptions::kPaintPreviewStartupDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(paint_preview::kPaintPreviewShowOnStartup)},
#endif  // ENABLE_PAINT_PREVIEW && defined(OS_ANDROID)

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
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         photo_picker::features::kPhotoPickerVideoSupport,
         kPhotoPickerVideoSupportFeatureVariations,
         "PhotoPickerVideoSupportFeatureVariations")},
#endif  // defined(OS_ANDROID)

    {"freeze-user-agent", flag_descriptions::kFreezeUserAgentName,
     flag_descriptions::kFreezeUserAgentDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kFreezeUserAgent)},

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
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

    {"double-buffer-compositing",
     flag_descriptions::kDoubleBufferCompositingName,
     flag_descriptions::kDoubleBufferCompositingDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDoubleBufferCompositing)},

#if defined(OS_ANDROID)
    {"password-change-in-settings",
     flag_descriptions::kPasswordChangeInSettingsName,
     flag_descriptions::kPasswordChangeInSettingsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kPasswordChangeInSettings,
         kPasswordChangeInSettingsFeatureVariations,
         "PasswordChangeInSettings")},
    {"password-scripts-fetching",
     flag_descriptions::kPasswordScriptsFetchingName,
     flag_descriptions::kPasswordScriptsFetchingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordScriptsFetching)},
    {"password-change-support", flag_descriptions::kPasswordChangeName,
     flag_descriptions::kPasswordChangeDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(password_manager::features::kPasswordChange,
                                    kPasswordChangeFeatureVariations,
                                    "PasswordChangeFeatureVariations")},

    {"context-menu-performance-info-and-remote-hints-fetching",
     flag_descriptions::kContextMenuPerformanceInfoAndRemoteHintFetchingName,
     flag_descriptions::
         kContextMenuPerformanceInfoAndRemoteHintFetchingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(optimization_guide::features::
                            kContextMenuPerformanceInfoAndRemoteHintFetching)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"page-info-performance-hints",
     flag_descriptions::kPageInfoPerformanceHintsName,
     flag_descriptions::kPageInfoPerformanceHintsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         performance_hints::features::kPageInfoPerformanceHints)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"page-info-version-2", flag_descriptions::kPageInfoV2Name,
     flag_descriptions::kPageInfoV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoV2)},
    {"page-info-discoverability",
     flag_descriptions::kPageInfoDiscoverabilityName,
     flag_descriptions::kPageInfoDiscoverabilityDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoDiscoverability)},
    {"page-info-history", flag_descriptions::kPageInfoHistoryName,
     flag_descriptions::kPageInfoHistoryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHistory)},
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enhanced_clipboard", flag_descriptions::kEnhancedClipboardName,
     flag_descriptions::kEnhancedClipboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kClipboardHistory)},
    {"enhanced_clipboard_nudge_session_reset",
     flag_descriptions::kEnhancedClipboardNudgeSessionResetName,
     flag_descriptions::kEnhancedClipboardNudgeSessionResetDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kClipboardHistoryNudgeSessionReset)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"enable-media-foundation-video-capture",
     flag_descriptions::kEnableMediaFoundationVideoCaptureName,
     flag_descriptions::kEnableMediaFoundationVideoCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kMediaFoundationVideoCapture)},
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"scanning-ui", flag_descriptions::kScanningUIName,
     flag_descriptions::kScanningUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanningUI)},
    {"avatar-toolbar-button", flag_descriptions::kAvatarToolbarButtonName,
     flag_descriptions::kAvatarToolbarButtonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAvatarToolbarButton)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"color-provider-redirection",
     flag_descriptions::kColorProviderRedirectionName,
     flag_descriptions::kColorProviderRedirectionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kColorProviderRedirection)},

    {"trust-tokens", flag_descriptions::kTrustTokensName,
     flag_descriptions::kTrustTokensDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(network::features::kTrustTokens,
                                    kPlatformProvidedTrustTokensVariations,
                                    "TrustTokenOriginTrial")},

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
    {"copy-link-to-text", flag_descriptions::kCopyLinkToTextName,
     flag_descriptions::kCopyLinkToTextDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCopyLinkToText)},
#endif  // !defined(OS_ANDROID)

    {"shared-highlighting-use-blocklist",
     flag_descriptions::kSharedHighlightingUseBlocklistName,
     flag_descriptions::kSharedHighlightingUseBlocklistDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingUseBlocklist)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"nearby-sharing", flag_descriptions::kNearbySharingName,
     flag_descriptions::kNearbySharingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharing)},
    {"nearby-sharing-device-contacts",
     flag_descriptions::kNearbySharingDeviceContactsName,
     flag_descriptions::kNearbySharingDeviceContactsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingDeviceContacts)},
    {"nearby-sharing-webrtc", flag_descriptions::kNearbySharingWebRtcName,
     flag_descriptions::kNearbySharingWebRtcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingWebRtc)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"android-default-browser-promo",
     flag_descriptions::kAndroidDefaultBrowserPromoName,
     flag_descriptions::kAndroidDefaultBrowserPromoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidDefaultBrowserPromo)},

    {"android-managed-by-menu-item",
     flag_descriptions::kAndroidManagedByMenuItemName,
     flag_descriptions::kAndroidManagedByMenuItemDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidManagedByMenuItem)},
#endif  // defined(OS_ANDROID)

    {"app-cache", flag_descriptions::kAppCacheName,
     flag_descriptions::kAppCacheDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAppCache)},

    {"align-font-display-auto-lcp",
     flag_descriptions::kAlignFontDisplayAutoTimeoutWithLCPGoalName,
     flag_descriptions::kAlignFontDisplayAutoTimeoutWithLCPGoalDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kAlignFontDisplayAutoTimeoutWithLCPGoal,
         kAlignFontDisplayAutoTimeoutWithLCPGoalVariations,
         "AlignFontDisplayAutoTimeoutWithLCPGoal")},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-palm-suppression", flag_descriptions::kEnablePalmSuppressionName,
     flag_descriptions::kEnablePalmSuppressionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmSuppression)},

    {"movable-partial-screenshot-region",
     flag_descriptions::kMovablePartialScreenshotName,
     flag_descriptions::kMovablePartialScreenshotDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMovablePartialScreenshot)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
     FEATURE_VALUE_TYPE(permissions::features::kPermissionChip)},

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

#if !defined(OS_ANDROID)
    {"enable-media-feeds", flag_descriptions::kEnableMediaFeedsName,
     flag_descriptions::kEnableMediaFeedsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kMediaFeeds)},

    {"enable-media-feeds-background-fetch",
     flag_descriptions::kEnableMediaFeedsBackgroundFetchName,
     flag_descriptions::kEnableMediaFeedsBackgroundFetchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kMediaFeedsBackgroundFetching)},
#endif  // !defined(OS_ANDROID)

    {"conversion-measurement-api",
     flag_descriptions::kConversionMeasurementApiName,
     flag_descriptions::kConversionMeasurementApiDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kConversionMeasurement)},
    {"conversion-measurement-debug-mode",
     flag_descriptions::kConversionMeasurementDebugModeName,
     flag_descriptions::kConversionMeasurementDebugModeDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kConversionsDebugMode)},

    {"client-storage-access-context-auditing",
     flag_descriptions::kClientStorageAccessContextAuditingName,
     flag_descriptions::kClientStorageAccessContextAuditingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kClientStorageAccessContextAuditing)},

    {"clipboard-filenames", flag_descriptions::kClipboardFilenamesName,
     flag_descriptions::kClipboardFilenamesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kClipboardFilenames)},

#if defined(OS_WIN)
    {"safety-check-chrome-cleaner-child",
     flag_descriptions::kSafetyCheckChromeCleanerChildName,
     flag_descriptions::kSafetyCheckChromeCleanerChildDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kSafetyCheckChromeCleanerChild)},
#endif  // !defined(OS_WIN)

#if defined(OS_WIN)
    {"chrome-cleanup-scan-completed-notification",
     flag_descriptions::kChromeCleanupScanCompletedNotificationName,
     flag_descriptions::kChromeCleanupScanCompletedNotificationDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(features::kChromeCleanupScanCompletedNotification)},
#endif  // !defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-launcher-app-paging",
     flag_descriptions::kNewDragSpecInLauncherName,
     flag_descriptions::kNewDragSpecInLauncherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kNewDragSpecInLauncher)},
    {"cdm-factory-daemon", flag_descriptions::kCdmFactoryDaemonName,
     flag_descriptions::kCdmFactoryDaemonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCdmFactoryDaemon)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-offers-in-downstream",
     flag_descriptions::kAutofillEnableOffersInDownstreamName,
     flag_descriptions::kAutofillEnableOffersInDownstreamDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableOffersInDownstream)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-sharesheet", flag_descriptions::kSharesheetName,
     flag_descriptions::kSharesheetDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSharesheet)},

    {"enable-sharesheet-content-previews",
     flag_descriptions::kSharesheetContentPreviewsName,
     flag_descriptions::kSharesheetContentPreviewsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSharesheetContentPreviews)},

    {"chromeos-sharing-hub", flag_descriptions::kChromeOSSharingHubName,
     flag_descriptions::kChromeOSSharingHubDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kChromeOSSharingHub)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"schemeful-same-site", flag_descriptions::kSchemefulSameSiteName,
     flag_descriptions::kSchemefulSameSiteDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kSchemefulSameSite)},

    {"enable-bluetooth-spp-in-serial-api",
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiName,
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiDescription,
     kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableBluetoothSerialPortProfileInSerialApi)},

    {"enable-lite-video", flag_descriptions::kLiteVideoName,
     flag_descriptions::kLiteVideoDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLiteVideo)},

    {"lite-video-default-downlink-bandwidth-kbps",
     flag_descriptions::kLiteVideoDownlinkBandwidthKbpsName,
     flag_descriptions::kLiteVideoDownlinkBandwidthKbpsDescription, kOsAll,
     MULTI_VALUE_TYPE(kLiteVideoDefaultDownlinkBandwidthKbps)},

    {"lite-video-force-override-decision",
     flag_descriptions::kLiteVideoForceOverrideDecisionName,
     flag_descriptions::kLiteVideoForceOverrideDecisionDescription, kOsAll,
     SINGLE_VALUE_TYPE(lite_video::switches::kLiteVideoForceOverrideDecision)},

    {"edit-passwords-in-settings",
     flag_descriptions::kEditPasswordsInSettingsName,
     flag_descriptions::kEditPasswordsInSettingsDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kEditPasswordsInSettings)},

    {"mixed-forms-disable-autofill",
     flag_descriptions::kMixedFormsDisableAutofillName,
     flag_descriptions::kMixedFormsDisableAutofillDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPreventMixedFormsFilling)},

    {"mixed-forms-interstitial", flag_descriptions::kMixedFormsInterstitialName,
     flag_descriptions::kMixedFormsInterstitialDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         security_interstitials::kInsecureFormSubmissionInterstitial)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"frame-throttle-fps", flag_descriptions::kFrameThrottleFpsName,
     flag_descriptions::kFrameThrottleFpsDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kFrameThrottleFpsChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"filling-passwords-from-any-origin",
     flag_descriptions::kFillingPasswordsFromAnyOriginName,
     flag_descriptions::kFillingPasswordsFromAnyOriginDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingPasswordsFromAnyOrigin)},
#endif  // OS_ANDROID

#if defined(OS_WIN)
    {"enable-incognito-shortcut-on-desktop",
     flag_descriptions::kEnableIncognitoShortcutOnDesktopName,
     flag_descriptions::kEnableIncognitoShortcutOnDesktopDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableIncognitoShortcutOnDesktop)},
#endif  // defined(OS_WIN)

    {"content-settings-redesign",
     flag_descriptions::kContentSettingsRedesignName,
     flag_descriptions::kContentSettingsRedesignDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kContentSettingsRedesign)},

    {flag_descriptions::kEnableTabSearchFlagId,
     flag_descriptions::kEnableTabSearchName,
     flag_descriptions::kEnableTabSearchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabSearch)},

#if defined(OS_ANDROID)
    {"cpu-affinity-restrict-to-little-cores",
     flag_descriptions::kCpuAffinityRestrictToLittleCoresName,
     flag_descriptions::kCpuAffinityRestrictToLittleCoresDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kCpuAffinityRestrictToLittleCores)},

    {"enable-surface-control", flag_descriptions::kAndroidSurfaceControlName,
     flag_descriptions::kAndroidSurfaceControlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidSurfaceControl)},

    {"enable-image-reader", flag_descriptions::kAImageReaderName,
     flag_descriptions::kAImageReaderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAImageReader)},
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
    {"enable-autofill-credit-card-cvc-prompt-google-logo",
     flag_descriptions::kEnableAutofillCreditCardCvcPromptGoogleLogoName,
     flag_descriptions::kEnableAutofillCreditCardCvcPromptGoogleLogoDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillDownstreamCvcPromptUseGooglePayLogo)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-auto-select", flag_descriptions::kEnableAutoSelectName,
     flag_descriptions::kEnableAutoSelectDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(blink::features::kCrOSAutoSelect)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"smart-suggestion-for-large-downloads",
     flag_descriptions::kSmartSuggestionForLargeDownloadsName,
     flag_descriptions::kSmartSuggestionForLargeDownloadsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kSmartSuggestionForLargeDownloads)},
#endif  // defined(OS_ANDROID)

    {"window-naming", flag_descriptions::kWindowNamingName,
     flag_descriptions::kWindowNamingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWindowNaming)},

#if defined(OS_ANDROID)
    {"messages-for-android-infrastructure",
     flag_descriptions::kMessagesForAndroidInfrastructureName,
     flag_descriptions::kMessagesForAndroidInfrastructureDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidInfrastructure)},
    {"messages-for-android-passwords",
     flag_descriptions::kMessagesForAndroidPasswordsName,
     flag_descriptions::kMessagesForAndroidPasswordsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidPasswords)},
    {"messages-for-android-popup-blocked",
     flag_descriptions::kMessagesForAndroidPopupBlockedName,
     flag_descriptions::kMessagesForAndroidPopupBlockedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidPopupBlocked)},
#endif

#if defined(OS_ANDROID)
    {"android-detailed-language-settings",
     flag_descriptions::kAndroidDetailedLanguageSettingsName,
     flag_descriptions::kAndroidDetailedLanguageSettingsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kDetailedLanguageSettings)},
#endif

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
    {"commander", flag_descriptions::kCommanderName,
     flag_descriptions::kCommanderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCommander)},

    {"desktop-restructured-language-settings",
     flag_descriptions::kDesktopRestructuredLanguageSettingsName,
     flag_descriptions::kDesktopRestructuredLanguageSettingsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(language::kDesktopRestructuredLanguageSettings)},

    {"desktop-detailed-language-settings",
     flag_descriptions::kDesktopDetailedLanguageSettingsName,
     flag_descriptions::kDesktopDetailedLanguageSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(language::kDesktopDetailedLanguageSettings)},
#endif

    {"sync-autofill-wallet-offer-data",
     flag_descriptions::kSyncAutofillWalletOfferDataName,
     flag_descriptions::kSyncAutofillWalletOfferDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncAutofillWalletOfferData)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-holding-space", flag_descriptions::kHoldingSpaceName,
     flag_descriptions::kHoldingSpaceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTemporaryHoldingSpace)},

    {"enable-holding-space-previews",
     flag_descriptions::kHoldingSpacePreviewsName,
     flag_descriptions::kHoldingSpacePreviewsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTemporaryHoldingSpacePreviews)},

    {"enhanced-desk-animations", flag_descriptions::kEnhancedDeskAnimationsName,
     flag_descriptions::kEnhancedDeskAnimationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnhancedDeskAnimations)},
#endif

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"enable-oop-print-drivers", flag_descriptions::kEnableOopPrintDriversName,
     flag_descriptions::kEnableOopPrintDriversDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(printing::features::kEnableOopPrintDrivers)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"omnibox-rich-entities-in-launcher",
     flag_descriptions::kOmniboxRichEntitiesInLauncherName,
     flag_descriptions::kOmniboxRichEntitiesInLauncherDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         app_list_features::kEnableOmniboxRichEntities,
         kOmniboxRichEntitiesInLauncherVariations,
         "OmniboxRichEntitiesInLauncher")},

    {"separate-pointing-stick-settings",
     flag_descriptions::kSeparatePointingStickSettingsName,
     flag_descriptions::kSeparatePointingStickSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSeparatePointingStickSettings)},
#endif

#if !defined(OS_ANDROID)
    {"mute-notifications-during-screen-share",
     flag_descriptions::kMuteNotificationsDuringScreenShareName,
     flag_descriptions::kMuteNotificationsDuringScreenShareDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMuteNotificationsDuringScreenShare)},
#endif  // !defined(OS_ANDROID)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC)
    {"enable-ephemeral-guest-profiles-on-desktop",
     flag_descriptions::kEnableEphemeralGuestProfilesOnDesktopName,
     flag_descriptions::kEnableEphemeralGuestProfilesOnDesktopDescription,
     kOsWin | kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(features::kEnableEphemeralGuestProfilesOnDesktop)},
#endif  // defined(OS_WIN) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS)) || defined(OS_MAC)

#if defined(OS_ANDROID)
    {"decouple-sync-from-android-auto-sync",
     flag_descriptions::kDecoupleSyncFromAndroidAutoSyncName,
     flag_descriptions::kDecoupleSyncFromAndroidAutoSyncDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kDecoupleSyncFromAndroidMasterSync)},
#endif  // defined(OS_ANDROID)

    {"enable-browsing-data-lifetime-manager",
     flag_descriptions::kEnableBrowsingDataLifetimeManagerName,
     flag_descriptions::kEnableBrowsingDataLifetimeManagerDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         browsing_data::features::kEnableBrowsingDataLifetimeManager)},

#if defined(OS_ANDROID)
    {"wallet-requires-first-sync-setup",
     flag_descriptions::kWalletRequiresFirstSyncSetupCompleteName,
     flag_descriptions::kWalletRequiresFirstSyncSetupCompleteDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kWalletRequiresFirstSyncSetupComplete)},
#endif  // defined(OS_ANDROID)

    {"privacy-advisor", flag_descriptions::kPrivacyAdvisorName,
     flag_descriptions::kPrivacyAdvisorDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPrivacyAdvisor)},

#if defined(TOOLKIT_VIEWS)
    {"desktop-in-product-help-snooze",
     flag_descriptions::kDesktopInProductHelpSnoozeName,
     flag_descriptions::kDesktopInProductHelpSnoozeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHDesktopSnoozeFeature)},
#endif  // defined(TOOLKIT_VIEWS)

    {"animated-image-resume", flag_descriptions::kAnimatedImageResumeName,
     flag_descriptions::kAnimatedImageResumeDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kAnimatedImageResume)},

#if !defined(OS_ANDROID)
    {"sct-auditing", flag_descriptions::kSCTAuditingName,
     flag_descriptions::kSCTAuditingDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kSCTAuditing,
                                    kSCTAuditingVariations,
                                    "SCTAuditingVariations")},
#endif  // !defined(OS_ANDROID)

    {"kaleidoscope-ntp-module", flag_descriptions::kKaleidoscopeModuleName,
     flag_descriptions::kKaleidoscopeModuleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kKaleidoscopeModule)},

    {"insert-key-toggle-mode", flag_descriptions::kInsertKeyToggleModeName,
     flag_descriptions::kInsertKeyToggleModeDescription,
     kOsWin | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(blink::features::kInsertKeyToggleMode)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"connectivity-diagnostics-webui",
     flag_descriptions::kConnectivityDiagnosticsWebUiName,
     flag_descriptions::kConnectivityDiagnosticsWebUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kConnectivityDiagnosticsWebUi)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"enable-autofill-password-account-indicator-footer",
     flag_descriptions::
         kEnableAutofillPasswordInfoBarAccountIndicationFooterName,
     flag_descriptions::
         kEnableAutofillPasswordInfoBarAccountIndicationFooterDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnablePasswordInfoBarAccountIndicationFooter)},

    {"incognito-screenshot", flag_descriptions::kIncognitoScreenshotName,
     flag_descriptions::kIncognitoScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kIncognitoScreenshot)},
#endif
    {"use-first-party-set", flag_descriptions::kUseFirstPartySetName,
     flag_descriptions::kUseFirstPartySetDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(network::switches::kUseFirstPartySet, "")},

    {"check-offline-capability", flag_descriptions::kCheckOfflineCapabilityName,
     flag_descriptions::kCheckOfflineCapabilityDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kCheckOfflineCapability,
                                    kCheckOfflineCapabilityVariations,
                                    "CheckOfflineCapability")},
#if defined(OS_ANDROID)
    {"enable-autofill-save-card-info-bar-account-indication-footer",
     flag_descriptions::
         kEnableAutofillSaveCardInfoBarAccountIndicationFooterName,
     flag_descriptions::
         kEnableAutofillSaveCardInfoBarAccountIndicationFooterDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableSaveCardInfoBarAccountIndicationFooter)},
#endif
    {"detect-form-submission-on-form-clear",
     flag_descriptions::kDetectFormSubmissionOnFormClearName,
     flag_descriptions::kDetectFormSubmissionOnFormClearDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kDetectFormSubmissionOnFormClear)},

#if defined(OS_ANDROID)
    {"enable-autofill-infobar-account-indication-footer-for-single-account-"
     "users",
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersName,
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers)},
    {"enable-autofill-infobar-account-indication-footer-for-sync-users",
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersName,
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableInfoBarAccountIndicationFooterForSyncUsers)},
#endif

    {"permission-predictions", flag_descriptions::kPermissionPredictionsName,
     flag_descriptions::kPermissionPredictionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPermissionPredictions)},

    {"show-performance-metrics-hud",
     flag_descriptions::kShowPerformanceMetricsHudName,
     flag_descriptions::kShowPerformanceMetricsHudDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHudDisplayForPerformanceMetrics)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"disable-buffer-bw-compression",
     flag_descriptions::kDisableBufferBWCompressionName,
     flag_descriptions::kDisableBufferBWCompressionDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableBufferBWCompression)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-prerender2", flag_descriptions::kPrerender2Name,
     flag_descriptions::kPrerender2Description, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPrerender2)},

#if defined(OS_ANDROID)
    {"enable-swipe-to-move-cursor", flag_descriptions::kSwipeToMoveCursorName,
     flag_descriptions::kSwipeToMoveCursorDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSwipeToMoveCursor)},
#endif  // defined(OS_ANDROID)

    {"change-password-affiliation",
     flag_descriptions::kChangePasswordAffiliationInfoName,
     flag_descriptions::kChangePasswordAffiliationInfoDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kChangePasswordAffiliationInfo)},

    {"use-of-hash-affiliation-fetcher",
     flag_descriptions::kUseOfHashAffiliationFetcherName,
     flag_descriptions::kUseOfHashAffiliationFetcherDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kUseOfHashAffiliationFetcher)},

    {"safety-check-weak-passwords",
     flag_descriptions::kSafetyCheckWeakPasswordsName,
     flag_descriptions::kSafetyCheckWeakPasswordsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSafetyCheckWeakPasswords)},

#if defined(OS_ANDROID)
    {"continuous-search", flag_descriptions::kContinuousSearchName,
     flag_descriptions::kContinuousSearchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kContinuousSearch)},

    {"enable-experimental-accessibility-labels",
     flag_descriptions::kExperimentalAccessibilityLabelsName,
     flag_descriptions::kExperimentalAccessibilityLabelsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kExperimentalAccessibilityLabels)},
#endif  // defined(OS_ANDROID)

    // TODO(crbug.com/1155358): Enable Chrome Labs for ChromeOS
    {"chrome-labs", flag_descriptions::kChromeLabsName,
     flag_descriptions::kChromeLabsDescription, kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(features::kChromeLabs)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"launcher-search-normalization",
     flag_descriptions::kEnableLauncherSearchNormalizationName,
     flag_descriptions::kEnableLauncherSearchNormalizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableLauncherSearchNormalization)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-first-party-sets", flag_descriptions::kEnableFirstPartySetsName,
     flag_descriptions::kEnableFirstPartySetsDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kFirstPartySets)},

#if defined(OS_ANDROID)
    {"autofill-enable-offers-in-clank-keyboard-accessory",
     flag_descriptions::kAutofillEnableOffersInClankKeyboardAccessoryName,
     flag_descriptions::
         kAutofillEnableOffersInClankKeyboardAccessoryDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableOffersInClankKeyboardAccessory)},
#endif

#if BUILDFLAG(ENABLE_PDF)
    {"pdf-xfa-forms", flag_descriptions::kPdfXfaFormsName,
     flag_descriptions::kPdfXfaFormsDescription, kOsAll,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfXfaSupport)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if defined(OS_ANDROID)
    {"actionable-content-settings",
     flag_descriptions::kActionableContentSettingsName,
     flag_descriptions::kActionableContentSettingsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(browser_ui::kActionableContentSettings)},
#endif

    {"send-tab-to-self-when-signed-in",
     flag_descriptions::kSendTabToSelfWhenSignedInName,
     flag_descriptions::kSendTabToSelfWhenSignedInDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfWhenSignedIn)},

#if defined(OS_ANDROID)
    {"mobile-pwa-install-use-bottom-sheet",
     flag_descriptions::kMobilePwaInstallUseBottomSheetName,
     flag_descriptions::kMobilePwaInstallUseBottomSheetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(webapps::features::kPwaInstallUseBottomSheet)},
#endif

    {"text-fragment-color-change",
     flag_descriptions::kTextFragmentColorChangeName,
     flag_descriptions::kTextFragmentColorChangeDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kTextFragmentColorChange)},

#if defined(OS_WIN)
    {"raw-audio-capture", flag_descriptions::kRawAudioCaptureName,
     flag_descriptions::kRawAudioCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kWasapiRawAudioCapture)},
#endif  // defined(OS_MAC)

    {"enable-restricted-web-apis",
     flag_descriptions::kEnableRestrictedWebApisName,
     flag_descriptions::kEnableRestrictedWebApisDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableRestrictedWebApis)},

    {"clear-cross-browsing-context-group-main-frame-name",
     flag_descriptions::kClearCrossBrowsingContextGroupMainFrameNameName,
     flag_descriptions::kClearCrossBrowsingContextGroupMainFrameNameDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kClearCrossBrowsingContextGroupMainFrameName)},

    {"sync-compromised-credentials",
     flag_descriptions::kSyncingCompromisedCredentialsName,
     flag_descriptions::kSyncingCompromisedCredentialsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSyncingCompromisedCredentials)},

    {"autofill-enable-offer-notification",
     flag_descriptions::kAutofillEnableOfferNotificationName,
     flag_descriptions::kAutofillEnableOfferNotificationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableOfferNotification)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kWallpaperWebUIInternalName, flag_descriptions::kWallpaperWebUIName,
     flag_descriptions::kWallpaperWebUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperWebUI)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(b/177462291): make flag available on LaCrOS.
    {"enable-vaapi-av1-decode-acceleration",
     flag_descriptions::kVaapiAV1DecoderName,
     flag_descriptions::kVaapiAV1DecoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kVaapiAV1Decoder)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC)
    {
        "ui-debug-tools",
        flag_descriptions::kUIDebugToolsName,
        flag_descriptions::kUIDebugToolsDescription,
        kOsWin | kOsLinux | kOsMac,
        FEATURE_VALUE_TYPE(features::kUIDebugTools),
    },
#endif
    {"http-cache-partitioning",
     flag_descriptions::kSplitCacheByNetworkIsolationKeyName,
     flag_descriptions::kSplitCacheByNetworkIsolationKeyDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kSplitCacheByNetworkIsolationKey)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-scalable-status-area", flag_descriptions::kScalableStatusAreaName,
     flag_descriptions::kScalableStatusAreaDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kScalableStatusArea)},

    {"enable-show-date-in-tray", flag_descriptions::kShowDateInTrayName,
     flag_descriptions::kShowDateInTrayDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShowDateInTrayButton)},
#endif

    {"autofill-address-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptName,
     flag_descriptions::kEnableAutofillAddressSavePromptDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAddressProfileSavePrompt)},

    {"detected-source-language-option",
     flag_descriptions::kDetectedSourceLanguageOptionName,
     flag_descriptions::kDetectedSourceLanguageOptionDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(language::kDetectedSourceLanguageOption)},

#if defined(OS_ANDROID)
    {"content-languages-in-language-picker",
     flag_descriptions::kContentLanguagesInLanguagePickerName,
     flag_descriptions::kContentLanguagesInLanguagePickerName, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kContentLanguagesInLanguagePicker)},
#endif

    {"filling-across-affiliated-websites",
     flag_descriptions::kFillingAcrossAffiliatedWebsitesName,
     flag_descriptions::kFillingAcrossAffiliatedWebsitesDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingAcrossAffiliatedWebsites)},

    {"enable-tflite-language-detection",
     flag_descriptions::kTFLiteLanguageDetectionName,
     flag_descriptions::kTFLiteLanguageDetectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTFLiteLanguageDetectionEnabled)},

    {"optimization-guide-model-downloading",
     flag_descriptions::kOptimizationGuideModelDownloadingName,
     flag_descriptions::kOptimizationGuideModelDownloadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         optimization_guide::features::kOptimizationGuideModelDownloading)},

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
  FlagsStateSingleton(const FlagsStateSingleton&) = delete;
  FlagsStateSingleton& operator=(const FlagsStateSingleton&) = delete;
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
  bool ShouldExcludeFlag(const flags_ui::FlagsStorage* storage,
                         const FeatureEntry& entry) override {
    return flags::IsFlagExpired(storage, entry.internal_name);
  }

  std::unique_ptr<flags_ui::FlagsState> flags_state_;
};

bool ShouldSkipNonDeprecatedFeatureEntry(const FeatureEntry& entry) {
  return ~entry.supported_platforms & kDeprecated;
}

bool SkipConditionalFeatureEntry(const flags_ui::FlagsStorage* storage,
                                 const FeatureEntry& entry) {
  version_info::Channel channel = chrome::GetChannel();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // enable-ui-devtools is only available on for non Stable channels.
  if (!strcmp(ui_devtools::switches::kEnableUiDevTools, entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }

  if (!strcmp(kLacrosSupportInternalName, entry.internal_name) ||
      !strcmp(kLacrosStabilityInternalName, entry.internal_name)) {
    if (!base::FeatureList::IsEnabled(
            crosapi::browser_util::kLacrosAllowOnStableChannel) &&
        channel == version_info::Channel::STABLE) {
      return true;
    }
  }

  // The following flags are only available to teamfooders.
  if (!strcmp(kAssistantBetterOnboardingInternalName, entry.internal_name) ||
      !strcmp(kAssistantTimersV2InternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kTeamfoodFlags);
  }

  // enable-bloom and wallpaper-webui are only available for Unknown/Canary/Dev
  // channels.
  if ((!strcmp("enable-bloom", entry.internal_name) ||
       !strcmp(kWallpaperWebUIInternalName, entry.internal_name)) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
      SystemNetworkContextManager::GetInstance() &&
      (SystemNetworkContextManager::GetStubResolverConfigReader()
           ->ShouldDisableDohForManaged() ||
       features::kDnsOverHttpsShowUiParam.Get())) {
    return true;
  }

#if defined(OS_ANDROID)
  if (!strcmp("password-change-support", entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kTeamfoodFlags);
  }
#endif  // OS_ANDROID

  if (flags::IsFlagExpired(storage, entry.internal_name))
    return true;

  return false;
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Put the flags between --policy-switches--begin and --policy-switches-end on
  // ChromeOS.
  extra_flag_sentinel_begin_flag_name =
      chromeos::switches::kPolicySwitchesBegin;
  extra_flag_sentinel_end_flag_name = chromeos::switches::kPolicySwitchesEnd;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
      base::BindRepeating(&SkipConditionalFeatureEntry,
                          // Unretained: this callback doesn't outlive this
                          // stack frame.
                          base::Unretained(flags_storage)));
}

void GetFlagFeatureEntriesForDeprecatedPage(
    flags_ui::FlagsStorage* flags_storage,
    flags_ui::FlagAccess access,
    base::ListValue* supported_entries,
    base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::BindRepeating(&ShouldSkipNonDeprecatedFeatureEntry));
}

flags_ui::FlagsState* GetCurrentFlagsState() {
  return FlagsStateSingleton::GetFlagsState();
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
  flags_ui::ReportAboutFlagsHistogram("Launch.FlagsAtStartup", switches,
                                      features);
}

namespace testing {

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
