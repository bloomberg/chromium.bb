// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/accessibility_labels_menu_observer.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"
#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/share_submenu_model.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_context_menu_observer.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_utils.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_features.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/google/core/common/google_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/lens/lens_features.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_prefs/user_prefs.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/escape.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/origin.h"

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
#include "chrome/browser/renderer_context_menu/spelling_options_submenu_observer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_common.h"
#include "components/printing/common/print.mojom.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_context_menu_observer.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/intent_helper/open_with_menu.h"
#include "chrome/browser/ash/arc/intent_helper/start_smart_selection_action_menu.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/aura/window.h"
#endif

using base::UserMetricsAction;
using blink::ContextMenuData;
using blink::ContextMenuDataEditFlags;
using blink::mojom::ContextMenuDataMediaType;
using content::BrowserContext;
using content::ChildProcessSecurityPolicy;
using content::DownloadManager;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;
using download::DownloadUrlParameters;
using extensions::ContextMenuMatcher;
using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;

namespace {

base::OnceCallback<void(RenderViewContextMenu*)>* GetMenuShownCallback() {
  static base::NoDestructor<base::OnceCallback<void(RenderViewContextMenu*)>>
      callback;
  return callback.get();
}

enum class UmaEnumIdLookupType {
  GeneralEnumId,
  ContextSpecificEnumId,
};

const std::map<int, int>& GetIdcToUmaMap(UmaEnumIdLookupType type) {
  // These maps are from IDC_* -> UMA value. Never alter UMA ids. You may remove
  // items, but add a line to keep the old value from being reused.

  // These UMA values are for the the RenderViewContextMenuItem enum, used for
  // the RenderViewContextMenu.Shown and RenderViewContextMenu.Used histograms.
  static const base::NoDestructor<std::map<int, int>> kGeneralMap(
      {// NB: UMA values for 0 and 1 are detected using
       // RenderViewContextMenu::IsContentCustomCommandId() and
       // ContextMenuMatcher::IsExtensionsCustomCommandId()
       {IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST, 2},
       {IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 3},
       {IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 4},
       {IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 5},
       {IDC_CONTENT_CONTEXT_SAVELINKAS, 6},
       {IDC_CONTENT_CONTEXT_SAVEAVAS, 7},
       {IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 8},
       {IDC_CONTENT_CONTEXT_COPYLINKLOCATION, 9},
       {IDC_CONTENT_CONTEXT_COPYIMAGELOCATION, 10},
       {IDC_CONTENT_CONTEXT_COPYAVLOCATION, 11},
       {IDC_CONTENT_CONTEXT_COPYIMAGE, 12},
       {IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB, 13},
       {IDC_CONTENT_CONTEXT_OPENAVNEWTAB, 14},
       {IDC_CONTENT_CONTEXT_PLAYPAUSE, 15},
       {IDC_CONTENT_CONTEXT_MUTE, 16},
       {IDC_CONTENT_CONTEXT_LOOP, 17},
       {IDC_CONTENT_CONTEXT_CONTROLS, 18},
       {IDC_CONTENT_CONTEXT_ROTATECW, 19},
       {IDC_CONTENT_CONTEXT_ROTATECCW, 20},
       {IDC_BACK, 21},
       {IDC_FORWARD, 22},
       {IDC_SAVE_PAGE, 23},
       {IDC_RELOAD, 24},
       {IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP, 25},
       {IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP, 26},
       {IDC_PRINT, 27},
       {IDC_VIEW_SOURCE, 28},
       {IDC_CONTENT_CONTEXT_INSPECTELEMENT, 29},
       {IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE, 30},
       {IDC_CONTENT_CONTEXT_VIEWPAGEINFO, 31},
       {IDC_CONTENT_CONTEXT_TRANSLATE, 32},
       {IDC_CONTENT_CONTEXT_RELOADFRAME, 33},
       {IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE, 34},
       {IDC_CONTENT_CONTEXT_VIEWFRAMEINFO, 35},
       {IDC_CONTENT_CONTEXT_UNDO, 36},
       {IDC_CONTENT_CONTEXT_REDO, 37},
       {IDC_CONTENT_CONTEXT_CUT, 38},
       {IDC_CONTENT_CONTEXT_COPY, 39},
       {IDC_CONTENT_CONTEXT_PASTE, 40},
       {IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE, 41},
       {IDC_CONTENT_CONTEXT_DELETE, 42},
       {IDC_CONTENT_CONTEXT_SELECTALL, 43},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFOR, 44},
       {IDC_CONTENT_CONTEXT_GOTOURL, 45},
       {IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS, 46},
       {IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS, 47},
       {IDC_CONTENT_CONTEXT_OPENLINKWITH, 52},
       {IDC_CHECK_SPELLING_WHILE_TYPING, 53},
       {IDC_SPELLCHECK_MENU, 54},
       {IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, 55},
       {IDC_SPELLCHECK_LANGUAGES_FIRST, 56},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE, 57},
       {IDC_SPELLCHECK_SUGGESTION_0, 58},
       {IDC_SPELLCHECK_ADD_TO_DICTIONARY, 59},
       {IDC_SPELLPANEL_TOGGLE, 60},
       {IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB, 61},
       {IDC_WRITING_DIRECTION_MENU, 62},
       {IDC_WRITING_DIRECTION_DEFAULT, 63},
       {IDC_WRITING_DIRECTION_LTR, 64},
       {IDC_WRITING_DIRECTION_RTL, 65},
       {IDC_CONTENT_CONTEXT_LOAD_IMAGE, 66},
       {IDC_ROUTE_MEDIA, 68},
       {IDC_CONTENT_CONTEXT_COPYLINKTEXT, 69},
       {IDC_CONTENT_CONTEXT_OPENLINKINPROFILE, 70},
       {IDC_OPEN_LINK_IN_PROFILE_FIRST, 71},
       {IDC_CONTENT_CONTEXT_GENERATEPASSWORD, 72},
       {IDC_SPELLCHECK_MULTI_LINGUAL, 73},
       {IDC_CONTENT_CONTEXT_OPEN_WITH1, 74},
       {IDC_CONTENT_CONTEXT_OPEN_WITH2, 75},
       {IDC_CONTENT_CONTEXT_OPEN_WITH3, 76},
       {IDC_CONTENT_CONTEXT_OPEN_WITH4, 77},
       {IDC_CONTENT_CONTEXT_OPEN_WITH5, 78},
       {IDC_CONTENT_CONTEXT_OPEN_WITH6, 79},
       {IDC_CONTENT_CONTEXT_OPEN_WITH7, 80},
       {IDC_CONTENT_CONTEXT_OPEN_WITH8, 81},
       {IDC_CONTENT_CONTEXT_OPEN_WITH9, 82},
       {IDC_CONTENT_CONTEXT_OPEN_WITH10, 83},
       {IDC_CONTENT_CONTEXT_OPEN_WITH11, 84},
       {IDC_CONTENT_CONTEXT_OPEN_WITH12, 85},
       {IDC_CONTENT_CONTEXT_OPEN_WITH13, 86},
       {IDC_CONTENT_CONTEXT_OPEN_WITH14, 87},
       {IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN, 88},
       {IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP, 89},
       {IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS, 90},
       {IDC_CONTENT_CONTEXT_PICTUREINPICTURE, 91},
       {IDC_CONTENT_CONTEXT_EMOJI, 92},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1, 93},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2, 94},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3, 95},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4, 96},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5, 97},
       {IDC_CONTENT_CONTEXT_LOOK_UP, 98},
       {IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE, 99},
       {IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE, 100},
       {IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS, 101},
       {IDC_SEND_TAB_TO_SELF, 102},
       {IDC_CONTENT_LINK_SEND_TAB_TO_SELF, 103},
       {IDC_SEND_TAB_TO_SELF_SINGLE_TARGET, 104},
       {IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET, 105},
       {IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE, 106},
       {IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES, 107},
       {IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE, 108},
       {IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES, 109},
       {IDC_CONTENT_CONTEXT_GENERATE_QR_CODE, 110},
       {IDC_CONTENT_CLIPBOARD_HISTORY_MENU, 111},
       {IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 112},
       {IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE, 113},
       {IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT, 114},
       {IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, 115},
       // To add new items:
       //   - Add one more line above this comment block, using the UMA value
       //     from the line below this comment block.
       //   - Increment the UMA value in that latter line.
       //   - Add the new item to the RenderViewContextMenuItem enum in
       //     tools/metrics/histograms/enums.xml.
       {0, 116}});

  // These UMA values are for the the ContextMenuOptionDesktop enum, used for
  // the ContextMenu.SelectedOptionDesktop histograms.
  static const base::NoDestructor<std::map<int, int>> kSpecificMap(
      {{IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0},
       {IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 1},
       {IDC_CONTENT_CONTEXT_COPYLINKLOCATION, 2},
       {IDC_CONTENT_CONTEXT_COPY, 3},
       {IDC_CONTENT_CONTEXT_SAVELINKAS, 4},
       {IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 5},
       {IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB, 6},
       {IDC_CONTENT_CONTEXT_COPYIMAGE, 7},
       {IDC_CONTENT_CONTEXT_COPYIMAGELOCATION, 8},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE, 9},
       {IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 10},
       {IDC_PRINT, 11},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFOR, 12},
       {IDC_CONTENT_CONTEXT_SAVEAVAS, 13},
       {IDC_SPELLCHECK_SUGGESTION_0, 14},
       {IDC_SPELLCHECK_ADD_TO_DICTIONARY, 15},
       {IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, 16},
       {IDC_CONTENT_CONTEXT_CUT, 17},
       {IDC_CONTENT_CONTEXT_PASTE, 18},
       {IDC_CONTENT_CONTEXT_GOTOURL, 19},
       {IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 20},
       {IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE, 21},
       {IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, 22},
       // To add new items:
       //   - Add one more line above this comment block, using the UMA value
       //     from the line below this comment block.
       //   - Increment the UMA value in that latter line.
       //   - Add the new item to the ContextMenuOptionDesktop enum in
       //     tools/metrics/histograms/enums.xml.
       {0, 23}});

  return *(type == UmaEnumIdLookupType::GeneralEnumId ? kGeneralMap
                                                      : kSpecificMap);
}

int GetUmaValueMax(UmaEnumIdLookupType type) {
  // The IDC_ "value" of 0 is really a sentinel for the UMA max value.
  return GetIdcToUmaMap(type).find(0)->second;
}

// Collapses large ranges of ids before looking for UMA enum.
int CollapseCommandsForUMA(int id) {
  DCHECK(!RenderViewContextMenu::IsContentCustomCommandId(id));
  DCHECK(!ContextMenuMatcher::IsExtensionsCustomCommandId(id));

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  }

  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id <= IDC_SPELLCHECK_LANGUAGES_LAST) {
    return IDC_SPELLCHECK_LANGUAGES_FIRST;
  }

  if (id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      id <= IDC_SPELLCHECK_SUGGESTION_LAST) {
    return IDC_SPELLCHECK_SUGGESTION_0;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    return IDC_OPEN_LINK_IN_PROFILE_FIRST;
  }

  return id;
}

// Returns UMA enum value for command specified by |id| or -1 if not found.
int FindUMAEnumValueForCommand(int id, UmaEnumIdLookupType type) {
  if (RenderViewContextMenu::IsContentCustomCommandId(id))
    return 0;

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return 1;

  id = CollapseCommandsForUMA(id);
  const auto& map = GetIdcToUmaMap(type);
  auto it = map.find(id);
  if (it == map.end())
    return -1;

  return it->second;
}

// Returns true if the command id is for opening a link.
bool IsCommandForOpenLink(int id) {
  return id == IDC_CONTENT_CONTEXT_OPENLINKNEWTAB ||
         id == IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW ||
         id == IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD ||
         (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
          id <= IDC_OPEN_LINK_IN_PROFILE_LAST);
}

// Returns the preference of the profile represented by the |context|.
PrefService* GetPrefs(content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context);
}

bool ExtensionPatternMatch(const extensions::URLPatternSet& patterns,
                           const GURL& url) {
  // No patterns means no restriction, so that implicitly matches.
  if (patterns.is_empty())
    return true;
  return patterns.MatchesURL(url);
}

const GURL& GetDocumentURL(const content::ContextMenuParams& params) {
  return params.frame_url.is_empty() ? params.page_url : params.frame_url;
}

content::Referrer CreateReferrer(const GURL& url,
                                 const content::ContextMenuParams& params) {
  const GURL& referring_url = GetDocumentURL(params);
  return content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(), params.referrer_policy));
}

content::WebContents* GetWebContentsToUse(content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If we're viewing in a MimeHandlerViewGuest, use its embedder WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (guest_view)
    return guest_view->embedder_web_contents();
#endif
  return web_contents;
}

bool g_custom_id_ranges_initialized = false;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void AddAvatarToLastMenuItem(const gfx::Image& icon,
                             ui::SimpleMenuModel* menu) {
  // Don't try to scale too small icons.
  if (icon.Width() < 16 || icon.Height() < 16)
    return;
  int target_dip_width = icon.Width();
  int target_dip_height = icon.Height();
  gfx::CalculateFaviconTargetSize(&target_dip_width, &target_dip_height);
  gfx::Image sized_icon = profiles::GetSizedAvatarIcon(
      icon, true /* is_rectangle */, target_dip_width, target_dip_height,
      profiles::SHAPE_CIRCLE);
  menu->SetIcon(menu->GetItemCount() - 1,
                ui::ImageModel::FromImage(sized_icon));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void OnProfileCreated(const GURL& link_url,
                      const content::Referrer& referrer,
                      Profile* profile,
                      Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    Browser* browser = chrome::FindLastActiveWithProfile(profile);
    NavigateParams nav_params(
        browser, link_url,
        /* |ui::PAGE_TRANSITION_TYPED| is used rather than
           |ui::PAGE_TRANSITION_LINK| since this ultimately opens the link in
           another browser. This parameter is used within the tab strip model of
           the browser it opens in implying a link from the active tab in the
           destination browser which is not correct. */
        ui::PAGE_TRANSITION_TYPED);
    nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    nav_params.referrer = referrer;
    nav_params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&nav_params);
  }
}

bool DoesInputFieldTypeSupportEmoji(
    blink::mojom::ContextMenuDataInputFieldType text_input_type) {
  // Disable emoji for input types that definitely do not support emoji.
  switch (text_input_type) {
    case blink::mojom::ContextMenuDataInputFieldType::kNumber:
    case blink::mojom::ContextMenuDataInputFieldType::kTelephone:
    case blink::mojom::ContextMenuDataInputFieldType::kOther:
      return false;
    default:
      return true;
  }
}

#if BUILDFLAG(ENABLE_PDF)
bool IsPdfPluginURL(const GURL& url) {
  return url.SchemeIs(extensions::kExtensionScheme) &&
         url.host_piece() == extension_misc::kPdfExtensionId;
}
#endif

// If the link points to a system web app (in |profile|), return its type.
// Otherwise nullopt.
absl::optional<web_app::SystemAppType> GetLinkSystemAppType(Profile* profile,
                                                            const GURL& url) {
  absl::optional<web_app::AppId> link_app_id =
      web_app::FindInstalledAppWithUrlInScope(profile, url);

  if (!link_app_id)
    return absl::nullopt;

  return web_app::GetSystemWebAppTypeForAppId(profile, *link_app_id);
}

bool ShouldUseShareMenu() {
  return base::FeatureList::IsEnabled(sharing::kShareMenu);
}

}  // namespace

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeDevToolsScheme);
}

// static
void RenderViewContextMenu::AddSpellCheckServiceItem(ui::SimpleMenuModel* menu,
                                                     bool is_checked) {
  if (is_checked) {
    menu->AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
                                   IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
  } else {
    menu->AddItemWithStringId(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
                              IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
  }
}

RenderViewContextMenu::RenderViewContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenuBase(render_frame_host, params),
      extension_items_(browser_context_,
                       this,
                       &menu_model_,
                       base::BindRepeating(MenuItemMatchesParams, params_)),
      profile_link_submenu_model_(this),
      multiple_profiles_open_(false),
      protocol_handler_submenu_model_(this),
      protocol_handler_registry_(
          ProtocolHandlerRegistryFactory::GetForBrowserContext(GetProfile())),
      accessibility_labels_submenu_model_(this),
      embedder_web_contents_(GetWebContentsToUse(source_web_contents_)) {
  if (!g_custom_id_ranges_initialized) {
    g_custom_id_ranges_initialized = true;
    SetContentCustomCommandIdRange(IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
                                   IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  }
  set_content_type(
      ContextMenuContentTypeFactory::Create(source_web_contents_, params));

  system_app_type_ = GetBrowser() && GetBrowser()->app_controller()
                         ? GetBrowser()->app_controller()->system_app_type()
                         : absl::nullopt;
}

RenderViewContextMenu::~RenderViewContextMenu() = default;

// Menu construction functions -------------------------------------------------

#if BUILDFLAG(ENABLE_EXTENSIONS)
// static
bool RenderViewContextMenu::ExtensionContextAndPatternMatch(
    const content::ContextMenuParams& params,
    const MenuItem::ContextList& contexts,
    const extensions::URLPatternSet& target_url_patterns) {
  const bool has_link = !params.link_url.is_empty();
  const bool has_selection = !params.selection_text.empty();
  const bool in_frame = !params.frame_url.is_empty();

  if (contexts.Contains(MenuItem::ALL) ||
      (has_selection && contexts.Contains(MenuItem::SELECTION)) ||
      (params.is_editable && contexts.Contains(MenuItem::EDITABLE)) ||
      (in_frame && contexts.Contains(MenuItem::FRAME)))
    return true;

  if (has_link && contexts.Contains(MenuItem::LINK) &&
      ExtensionPatternMatch(target_url_patterns, params.link_url))
    return true;

  switch (params.media_type) {
    case ContextMenuDataMediaType::kImage:
      if (contexts.Contains(MenuItem::IMAGE) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case ContextMenuDataMediaType::kVideo:
      if (contexts.Contains(MenuItem::VIDEO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case ContextMenuDataMediaType::kAudio:
      if (contexts.Contains(MenuItem::AUDIO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    default:
      break;
  }

  // PAGE is the least specific context, so we only examine that if none of the
  // other contexts apply (except for FRAME, which is included in PAGE for
  // backwards compatibility).
  if (!has_link && !has_selection && !params.is_editable &&
      params.media_type == ContextMenuDataMediaType::kNone &&
      contexts.Contains(MenuItem::PAGE))
    return true;

  return false;
}

// static
bool RenderViewContextMenu::MenuItemMatchesParams(
    const content::ContextMenuParams& params,
    const extensions::MenuItem* item) {
  bool match = ExtensionContextAndPatternMatch(params, item->contexts(),
                                               item->target_url_patterns());
  if (!match)
    return false;

  const GURL& document_url = GetDocumentURL(params);
  return ExtensionPatternMatch(item->document_url_patterns(), document_url);
}

void RenderViewContextMenu::AppendAllExtensionItems() {
  extension_items_.Clear();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context_);

  MenuManager* menu_manager = MenuManager::Get(browser_context_);
  if (!menu_manager)
    return;

  std::u16string printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  // Get a list of extension id's that have context menu items, and sort by the
  // top level context menu title of the extension.
  std::vector<std::u16string> sorted_menu_titles;
  std::map<std::u16string, std::vector<const Extension*>>
      title_to_extensions_map;
  for (const auto& id : menu_manager->ExtensionIds()) {
    const Extension* extension = registry->GetExtensionById(
        id.extension_id, extensions::ExtensionRegistry::ENABLED);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    if (extension && !extension->is_platform_app()) {
      std::u16string menu_title = extension_items_.GetTopLevelContextMenuTitle(
          id, printable_selection_text);
      title_to_extensions_map[menu_title].push_back(extension);
      sorted_menu_titles.push_back(menu_title);
    }
  }
  if (sorted_menu_titles.empty())
    return;

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  l10n_util::SortStrings16(app_locale, &sorted_menu_titles);
  sorted_menu_titles.erase(
      std::unique(sorted_menu_titles.begin(), sorted_menu_titles.end()),
      sorted_menu_titles.end());

  int index = 0;
  for (const auto& title : sorted_menu_titles) {
    const std::vector<const Extension*>& extensions =
        title_to_extensions_map[title];
    for (const Extension* extension : extensions) {
      MenuItem::ExtensionKey extension_key(extension->id());
      extension_items_.AppendExtensionItems(extension_key,
                                            printable_selection_text, &index,
                                            /*is_action_menu=*/false);
    }
  }
}

void RenderViewContextMenu::AppendCurrentExtensionItems() {
  // Avoid appending extension related items when |extension| is null.
  // For Panel, this happens when the panel is navigated to a url outside of the
  // extension's package.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(source_web_contents_);
  MenuItem::ExtensionKey key;
  if (web_view_guest) {
    key = MenuItem::ExtensionKey(extension->id(),
                                 web_view_guest->owner_web_contents()
                                     ->GetMainFrame()
                                     ->GetProcess()
                                     ->GetID(),
                                 web_view_guest->view_instance_id());
  } else {
    key = MenuItem::ExtensionKey(extension->id());
  }

  // Only add extension items from this extension.
  int index = 0;
  extension_items_.AppendExtensionItems(key, PrintableSelectionText(), &index,
                                        /*is_action_menu=*/false);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::u16string RenderViewContextMenu::FormatURLForClipboard(const GURL& url) {
  DCHECK(!url.is_empty());
  DCHECK(url.is_valid());

  GURL url_to_format = url;
  url_formatter::FormatUrlTypes format_types;
  net::UnescapeRule::Type unescape_rules;
  if (url.SchemeIs(url::kMailToScheme)) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    url_to_format = url.ReplaceComponents(replacements);
    format_types = url_formatter::kFormatUrlOmitMailToScheme;
    unescape_rules =
        net::UnescapeRule::PATH_SEPARATORS |
        net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS;
  } else {
    format_types = url_formatter::kFormatUrlOmitNothing;
    unescape_rules = net::UnescapeRule::NONE;
  }

  return url_formatter::FormatUrl(url_to_format, format_types, unescape_rules,
                                  nullptr, nullptr, nullptr);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  if (url.is_empty() || !url.is_valid())
    return;

  ui::ScopedClipboardWriter scw(
      ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/true));
  scw.WriteText(FormatURLForClipboard(url));
}

void RenderViewContextMenu::InitMenu() {
  RenderViewContextMenuBase::InitMenu();

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PASSWORD)) {
    AppendPasswordItems();
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE))
    AppendPageItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK)) {
    AppendLinkItems();
    if (params_.media_type != ContextMenuDataMediaType::kNone)
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  bool media_image = content_type_->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE);
  if (media_image)
    AppendImageItems();

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCHWEBFORIMAGE)) {
    if (base::FeatureList::IsEnabled(lens::features::kLensStandalone) &&
        search::DefaultSearchProviderIsGoogle(GetProfile())) {
      AppendSearchLensForImageItems();
    } else {
      AppendSearchWebForImageItems();
    }
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO)) {
    AppendVideoItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO)) {
    AppendAudioItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_CANVAS)) {
    AppendCanvasItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    AppendPluginItems();
  }

  // ITEM_GROUP_MEDIA_FILE has no specific items.

  bool editable =
      content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_EDITABLE);
  if (editable)
    AppendEditableItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_COPY)) {
    DCHECK(!editable);
    AppendCopyItem();

    if (base::FeatureList::IsEnabled(features::kCopyLinkToText)) {
      AppendLinkToTextItems();
    }
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_EXISTING_LINK_TO_TEXT)) {
    AppendLinkToTextItems();
  }

  if (!content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK))
    AppendSharingItems();

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER) &&
      params_.misspelled_word.empty()) {
    AppendSearchProvider();
  }

  if (!media_image &&
      content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)) {
    AppendPrintItem();
  }

  // Spell check and writing direction options are not currently supported by
  // pepper plugins.
  if (editable && params_.misspelled_word.empty() &&
      !content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    AppendLanguageSettings();
    AppendPlatformEditableItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    AppendRotationItems();
  }

  bool supports_smart_text_selection = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  supports_smart_text_selection =
      content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SMART_SELECTION) &&
      arc::IsArcPlayStoreEnabledForProfile(GetProfile());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (supports_smart_text_selection)
    AppendSmartSelectionActionItems();

  extension_items_.set_smart_text_selection_enabled(
      supports_smart_text_selection);

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
        ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION));
    AppendAllExtensionItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
        ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION));
    AppendCurrentExtensionItems();
  }

#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_LINUX)
  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_LENS_REGION_SEARCH)) {
    if (IsLensRegionSearchEnabled()) {
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
      AppendLensRegionSearchItem();
    }
  }
#endif

  // Accessibility label items are appended to all menus when a screen reader
  // is enabled. It can be difficult to open a specific context menu with a
  // screen reader, so this is a UX approved solution.
  bool added_accessibility_labels_items = AppendAccessibilityLabelsItems();

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVELOPER)) {
    AppendDeveloperItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVTOOLS_UNPACKED_EXT)) {
    AppendDevtoolsForUnpackedExtensions();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PRINT_PREVIEW)) {
    AppendPrintPreviewItems();
  }

  // Remove any redundant trailing separator.
  int index = menu_model_.GetItemCount() - 1;
  if (index >= 0 &&
      menu_model_.GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
    menu_model_.RemoveItemAt(index);
  }

  // If there is only one item and it is the Accessibility labels item, remove
  // it. We only show this item when it is not the only item.
  // Note that the separator added in AppendAccessibilityLabelsItems will not
  // actually be added if this is the first item in the list, so we don't need
  // to check for or remove the initial separator.
  if (added_accessibility_labels_items && menu_model_.GetItemCount() == 1) {
    menu_model_.RemoveItemAt(0);
  }

  // Always add Quick Answers view last, as it is rendered next to the context
  // menu, meaning that each menu item added/removed in this function will cause
  // it to visibly jump on the screen (see b/173569669).
  AppendQuickAnswersItems();
}

Profile* RenderViewContextMenu::GetProfile() const {
  return Profile::FromBrowserContext(browser_context_);
}

void RenderViewContextMenu::RecordUsedItem(int id) {
  // Log general ID.

  int enum_id =
      FindUMAEnumValueForCommand(id, UmaEnumIdLookupType::GeneralEnumId);
  if (enum_id == -1) {
    NOTREACHED() << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
    return;
  }

  UMA_HISTOGRAM_EXACT_LINEAR(
      "RenderViewContextMenu.Used", enum_id,
      GetUmaValueMax(UmaEnumIdLookupType::GeneralEnumId));

  // Log a user action for the SEARCHWEBFOR case. This value is used as part of
  // a high-level guiding metric, which is being migrated to user actions.
  if (id == IDC_CONTENT_CONTEXT_SEARCHWEBFOR) {
    base::RecordAction(base::UserMetricsAction(
        "RenderViewContextMenu.Used.IDC_CONTENT_CONTEXT_SEARCHWEBFOR"));
  }

  // Log other situations.

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK) &&
      // Ignore link-related commands that don't actually open a link.
      IsCommandForOpenLink(id) &&
      // Ignore using right click + open in new tab for internal links.
      !params_.link_url.SchemeIs(content::kChromeUIScheme)) {
    const GURL doc_url = GetDocumentURL(params_);
    const GURL history_url = GURL(chrome::kChromeUIHistoryURL);
    if (doc_url == history_url.Resolve(chrome::kChromeUIHistorySyncedTabs)) {
      UMA_HISTOGRAM_ENUMERATION(
          "HistoryPage.OtherDevicesMenu",
          browser_sync::SyncedTabsHistogram::OPENED_LINK_VIA_CONTEXT_MENU,
          browser_sync::SyncedTabsHistogram::LIMIT);
    } else if (doc_url == GURL(chrome::kChromeUIDownloadsURL)) {
      base::RecordAction(base::UserMetricsAction(
          "Downloads_OpenUrlOfDownloadedItemFromContextMenu"));
    } else if (doc_url.GetOrigin() == chrome::kChromeSearchMostVisitedUrl) {
      base::RecordAction(
          base::UserMetricsAction("MostVisited_ClickedFromContextMenu"));
    } else if (doc_url.GetOrigin() == GURL(chrome::kChromeUINewTabPageURL) ||
               doc_url.GetOrigin() ==
                   GURL(chrome::kChromeUIUntrustedNewTabPageUrl)) {
      base::RecordAction(base::UserMetricsAction(
          "NewTabPage.LinkOpenedFromContextMenu.WebUI"));
    }
  }

  // Log UKM for Lens context menu items.
  if ((id == IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH &&
       lens::features::GetEnableUKMLoggingForRegionSearch()) ||
      (id == IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE &&
       lens::features::GetEnableUKMLoggingForImageSearch())) {
    // Enum id should correspond to the RenderViewContextMenuItem enum.
    ukm::SourceId source_id =
        ukm::GetSourceIdForWebContentsDocument(source_web_contents_);
    ukm::builders::RenderViewContextMenu_Used(source_id)
        .SetSelectedMenuItem(enum_id)
        .Record(ukm::UkmRecorder::Get());
  }

  // Log for specific contexts. Note that since the menu is displayed for
  // contexts (all of the ContextMenuContentType::SupportsXXX() functions),
  // these UMA metrics are necessarily best-effort in distilling into a context.

  enum_id = FindUMAEnumValueForCommand(
      id, UmaEnumIdLookupType::ContextSpecificEnumId);
  if (enum_id == -1)
    return;

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO)) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.Video", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if (content_type_->SupportsGroup(
                 ContextMenuContentType::ITEM_GROUP_LINK) &&
             content_type_->SupportsGroup(
                 ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.ImageLink", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if (content_type_->SupportsGroup(
                 ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.Image", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if (!params_.misspelled_word.empty()) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.MisspelledWord", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if ((!params_.selection_text.empty() ||
              params_.opened_from_highlight) &&
             params_.media_type == ContextMenuDataMediaType::kNone) {
    // Probably just text.
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.SelectedText", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.Other", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  }
}

void RenderViewContextMenu::RecordShownItem(int id) {
  int enum_id =
      FindUMAEnumValueForCommand(id, UmaEnumIdLookupType::GeneralEnumId);
  if (enum_id != -1) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "RenderViewContextMenu.Shown", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::GeneralEnumId));
  } else {
    // Just warning here. It's harder to maintain list of all possibly
    // visible items than executable items.
    DLOG(ERROR) << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
  }
}

bool RenderViewContextMenu::IsHTML5Fullscreen() const {
  Browser* browser = chrome::FindBrowserWithWebContents(embedder_web_contents_);
  if (!browser)
    return false;

  FullscreenController* controller =
      browser->exclusive_access_manager()->fullscreen_controller();
  return controller->IsTabFullscreen();
}

bool RenderViewContextMenu::IsPressAndHoldEscRequiredToExitFullscreen() const {
  Browser* browser = chrome::FindBrowserWithWebContents(source_web_contents_);
  if (!browser)
    return false;

  KeyboardLockController* controller =
      browser->exclusive_access_manager()->keyboard_lock_controller();
  return controller->RequiresPressAndHoldEscToExit();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderViewContextMenu::HandleAuthorizeAllPlugins() {
  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      source_web_contents_, false, std::string());
}
#endif

void RenderViewContextMenu::AppendPrintPreviewItems() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!print_preview_menu_observer_) {
    print_preview_menu_observer_ =
        std::make_unique<PrintPreviewContextMenuObserver>(source_web_contents_);
  }

  observers_.AddObserver(print_preview_menu_observer_.get());
#endif
}

const Extension* RenderViewContextMenu::GetExtension() const {
  return extensions::ProcessManager::Get(browser_context_)
      ->GetExtensionForWebContents(source_web_contents_);
}

std::string RenderViewContextMenu::GetTargetLanguage() const {
  std::unique_ptr<translate::TranslatePrefs> prefs(
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefs(browser_context_)));
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(browser_context_)
          ->GetPrimaryModel();
  return translate::TranslateManager::GetTargetLanguage(prefs.get(),
                                                        language_model);
}

void RenderViewContextMenu::AppendDeveloperItems() {
  // Do not Show Inspect Element for DevTools unless DevTools runs with the
  // debugFrontend query param.
  bool hide_developer_items =
      IsDevToolsURL(params_.page_url) &&
      params_.page_url.query().find("debugFrontend=true") == std::string::npos;

  if (hide_developer_items)
    return;

  // In the DevTools popup menu, "developer items" is normally the only
  // section, so omit the separator there.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE))
    menu_model_.AddItemWithStringId(IDC_VIEW_SOURCE,
                                    IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_FRAME)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                                    IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOADFRAME,
                                    IDS_CONTENT_CONTEXT_RELOADFRAME);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                                  IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendDevtoolsForUnpackedExtensions() {
  // Add a separator if there are any items already in the menu.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RELOAD_PACKAGED_APP);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RESTART_APP);
  AppendDeveloperItems();
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE,
                                  IDS_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE);
}

void RenderViewContextMenu::AppendLinkItems() {
  if (!params_.link_url.is_empty()) {
    const Browser* browser = GetBrowser();
    const bool in_app =
        browser && (browser->is_type_app() || browser->is_type_app_popup());
    WebContents* active_web_contents =
        browser ? browser->tab_strip_model()->GetActiveWebContents() : nullptr;

    Profile* profile = GetProfile();
    absl::optional<web_app::SystemAppType> link_system_app_type =
        GetLinkSystemAppType(profile, params_.link_url);
    if (system_app_type_ && link_system_app_type) {
      // Show "Open in new tab" if this link points to the current app, and the
      // app has a tab strip.
      //
      // We don't show "open in tab" for links to a different SWA, because two
      // SWAs can't share the same browser window.
      if (system_app_type_ == link_system_app_type &&
          web_app::WebAppProvider::GetForSystemWebApps(profile)
              ->system_web_app_manager()
              .ShouldHaveTabStrip(system_app_type_.value())) {
        menu_model_.AddItemWithStringId(
            IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
            IDS_CONTENT_CONTEXT_OPENLINKNEWTAB_INAPP);
      }

      // Don't show "open in new window", this is instead handled below in
      // |AppendOpenInWebAppLinkItems| (which includes app's name and icon).
    } else {
      menu_model_.AddItemWithStringId(
          IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
          in_app ? IDS_CONTENT_CONTEXT_OPENLINKNEWTAB_INAPP
                 : IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
      if (!in_app) {
        menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                        IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
      }
    }

    if (params_.link_url.is_valid()) {
      AppendProtocolHandlerSubMenu();
    }

    // Links to system web app can't be opened in incognito / off-the-record.
    if (!link_system_app_type) {
      menu_model_.AddItemWithStringId(
          IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
          in_app ? IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD_INAPP
                 : IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
    }

    AppendOpenInWebAppLinkItems();
    AppendOpenWithLinkItems();

    // While ChromeOS supports multiple profiles, only one can be open at a
    // time.
    // TODO(jochen): Consider adding support for ChromeOS with similar
    // semantics as the profile switcher in the system tray.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // g_browser_process->profile_manager() is null during unit tests.
    if (g_browser_process->profile_manager() &&
        !GetProfile()->IsOffTheRecord()) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      // Find all regular profiles other than the current one which have at
      // least one open window.
      std::vector<ProfileAttributesEntry*> entries =
          profile_manager->GetProfileAttributesStorage()
              .GetAllProfilesAttributesSortedByName();
      std::vector<ProfileAttributesEntry*> target_profiles_entries;
      for (ProfileAttributesEntry* entry : entries) {
        base::FilePath profile_path = entry->GetPath();
        Profile* profile = profile_manager->GetProfileByPath(profile_path);
        if (profile != GetProfile() && !entry->IsOmitted() &&
            !entry->IsSigninRequired()) {
          target_profiles_entries.push_back(entry);
          if (chrome::FindLastActiveWithProfile(profile))
            multiple_profiles_open_ = true;
        }
      }

      if (multiple_profiles_open_ && !target_profiles_entries.empty()) {
        if (target_profiles_entries.size() == 1) {
          int menu_index = static_cast<int>(profile_link_paths_.size());
          ProfileAttributesEntry* entry = target_profiles_entries.front();
          profile_link_paths_.push_back(entry->GetPath());
          menu_model_.AddItem(
              IDC_OPEN_LINK_IN_PROFILE_FIRST + menu_index,
              l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_OPENLINKINPROFILE,
                                         entry->GetName()));
          AddAvatarToLastMenuItem(entry->GetAvatarIcon(), &menu_model_);
        } else {
          for (ProfileAttributesEntry* entry : target_profiles_entries) {
            int menu_index = static_cast<int>(profile_link_paths_.size());
            // In extreme cases, we might have more profiles than available
            // command ids. In that case, just stop creating new entries - the
            // menu is probably useless at this point already.
            if (IDC_OPEN_LINK_IN_PROFILE_FIRST + menu_index >
                IDC_OPEN_LINK_IN_PROFILE_LAST) {
              break;
            }
            profile_link_paths_.push_back(entry->GetPath());
            profile_link_submenu_model_.AddItem(
                IDC_OPEN_LINK_IN_PROFILE_FIRST + menu_index, entry->GetName());
            AddAvatarToLastMenuItem(entry->GetAvatarIcon(),
                                    &profile_link_submenu_model_);
          }
          menu_model_.AddSubMenuWithStringId(
              IDC_CONTENT_CONTEXT_OPENLINKINPROFILE,
              IDS_CONTENT_CONTEXT_OPENLINKINPROFILES,
              &profile_link_submenu_model_);
        }
      }
    }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

    if (ShouldUseShareMenu()) {
      sharing::ShareSubmenuModel::Context context =
          params_.has_image_contents
              ? sharing::ShareSubmenuModel::Context::IMAGE
              : sharing::ShareSubmenuModel::Context::LINK;
      share_submenu_model_ = std::make_unique<sharing::ShareSubmenuModel>(
          GetBrowser(), context, params_.page_url);
      if (share_submenu_model_->GetItemCount() > 0) {
        menu_model_.AddSubMenuWithStringId(IDC_CONTENT_CONTEXT_SHARING_SUBMENU,
                                           IDS_SHARE_MENU_TITLE,
                                           share_submenu_model_.get());
      }
    }

    // Place QR Generator close to send-tab-to-self feature for link images.
    if (!ShouldUseShareMenu() && params_.has_image_contents)
      AppendQRCodeGeneratorItem(/*for_image=*/true, /*draw_icon=*/true);

    if (browser && !ShouldUseShareMenu() &&
        send_tab_to_self::ShouldOfferFeatureForLink(active_web_contents,
                                                    params_.link_url)) {
      if (send_tab_to_self::GetValidDeviceCount(GetBrowser()->profile()) == 1) {
#if defined(OS_MAC)
        menu_model_.AddItem(IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET,
                            l10n_util::GetStringFUTF16(
                                IDS_LINK_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
                                send_tab_to_self::GetSingleTargetDeviceName(
                                    GetBrowser()->profile())));
#else
        menu_model_.AddItemWithIcon(
            IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET,
            l10n_util::GetStringFUTF16(
                IDS_LINK_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
                send_tab_to_self::GetSingleTargetDeviceName(
                    GetBrowser()->profile())),
            ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
      } else {
        send_tab_to_self_sub_menu_model_ =
            std::make_unique<send_tab_to_self::SendTabToSelfSubMenuModel>(
                active_web_contents,
                send_tab_to_self::SendTabToSelfMenuType::kLink,
                params_.link_url);
#if defined(OS_MAC)
        menu_model_.AddSubMenuWithStringId(
            IDC_CONTENT_LINK_SEND_TAB_TO_SELF, IDS_LINK_MENU_SEND_TAB_TO_SELF,
            send_tab_to_self_sub_menu_model_.get());
#else
        menu_model_.AddSubMenuWithStringIdAndIcon(
            IDC_CONTENT_LINK_SEND_TAB_TO_SELF, IDS_LINK_MENU_SEND_TAB_TO_SELF,
            send_tab_to_self_sub_menu_model_.get(),
            ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
      }
    }

#if !defined(OS_FUCHSIA)
    AppendClickToCallItem();
#endif

    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVELINKAS,
                                    IDS_CONTENT_CONTEXT_SAVELINKAS);
  }

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
                                  params_.link_url.SchemeIs(url::kMailToScheme)
                                      ? IDS_CONTENT_CONTEXT_COPYEMAILADDRESS
                                      : IDS_CONTENT_CONTEXT_COPYLINKLOCATION);

  if (params_.source_type == ui::MENU_SOURCE_TOUCH &&
      params_.media_type != ContextMenuDataMediaType::kImage &&
      !params_.link_text.empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKTEXT,
                                    IDS_CONTENT_CONTEXT_COPYLINKTEXT);
  }
}

void RenderViewContextMenu::AppendOpenWithLinkItems() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  open_with_menu_observer_ =
      std::make_unique<arc::OpenWithMenu>(browser_context_, this);
  observers_.AddObserver(open_with_menu_observer_.get());
  open_with_menu_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendQuickAnswersItems() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!quick_answers_menu_observer_) {
    quick_answers_menu_observer_ =
        std::make_unique<QuickAnswersMenuObserver>(this);
  }

  observers_.AddObserver(quick_answers_menu_observer_.get());
  quick_answers_menu_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendSmartSelectionActionItems() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  start_smart_selection_action_menu_observer_ =
      std::make_unique<arc::StartSmartSelectionActionMenu>(this);
  observers_.AddObserver(start_smart_selection_action_menu_observer_.get());

  if (menu_model_.GetItemCount())
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  start_smart_selection_action_menu_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendOpenInWebAppLinkItems() {
  Profile* const profile = Profile::FromBrowserContext(browser_context_);
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return;

  absl::optional<web_app::AppId> link_app_id =
      web_app::FindInstalledAppWithUrlInScope(profile, params_.link_url);
  if (!link_app_id)
    return;

  // Don't show "Open link in new app window", if the link points to the
  // current app, and the app is single windowed.
  if (system_app_type_ &&
      system_app_type_ ==
          web_app::GetSystemWebAppTypeForAppId(profile, *link_app_id) &&
      web_app::WebAppProvider::GetForSystemWebApps(GetProfile())
          ->system_web_app_manager()
          .IsSingleWindow(*system_app_type_)) {
    return;
  }

  int open_in_app_string_id;
  const Browser* browser = GetBrowser();
  if (browser && browser->app_name() ==
                     web_app::GenerateApplicationNameFromAppId(*link_app_id)) {
    open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP_SAMEAPP;
  } else {
    open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP;
  }

  auto* const provider = web_app::WebAppProvider::Get(profile);
  menu_model_.AddItem(
      IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
      l10n_util::GetStringFUTF16(
          open_in_app_string_id,
          base::UTF8ToUTF16(
              provider->registrar().GetAppShortName(*link_app_id))));

  gfx::Image icon = gfx::Image::CreateFrom1xBitmap(
      provider->icon_manager().GetFavicon(*link_app_id));
  menu_model_.SetIcon(menu_model_.GetItemCount() - 1,
                      ui::ImageModel::FromImage(icon));
}

void RenderViewContextMenu::AppendImageItems() {
  if (!params_.has_image_contents) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_LOAD_IMAGE,
                                    IDS_CONTENT_CONTEXT_LOAD_IMAGE);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
                                  IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);

  if (ShouldUseShareMenu() && !share_submenu_model_) {
    share_submenu_model_ = std::make_unique<sharing::ShareSubmenuModel>(
        GetBrowser(), sharing::ShareSubmenuModel::Context::IMAGE,
        params_.src_url);
    if (share_submenu_model_->GetItemCount() > 0) {
      menu_model_.AddSubMenuWithStringId(IDC_CONTENT_CONTEXT_SHARING_SUBMENU,
                                         IDS_SHARE_MENU_TITLE,
                                         share_submenu_model_.get());
    }
  }

  // Don't double-add for linked images, which also add the item.
  if (!ShouldUseShareMenu() && params_.link_url.is_empty())
    AppendQRCodeGeneratorItem(/*for_image=*/true, /*draw_icon=*/false);
}

void RenderViewContextMenu::AppendSearchWebForImageItems() {
  if (!params_.has_image_contents)
    return;

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  const TemplateURL* const provider = service->GetDefaultSearchProvider();
  if (!provider || provider->image_url().empty() ||
      !provider->image_url_ref().IsValid(service->search_terms_data())) {
    return;
  }

  menu_model_.AddItem(
      IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
      l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
                                 provider->short_name()));
}

void RenderViewContextMenu::AppendSearchLensForImageItems() {
  if (!params_.has_image_contents)
    return;

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  const TemplateURL* const provider = service->GetDefaultSearchProvider();
  if (!provider || provider->image_url().empty() ||
      !provider->image_url_ref().IsValid(service->search_terms_data())) {
    return;
  }

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE,
                                  IDS_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
}

void RenderViewContextMenu::AppendAudioItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  AppendMediaRouterItem();
}

void RenderViewContextMenu::AppendCanvasItems() {
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
}

void RenderViewContextMenu::AppendVideoItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  AppendPictureInPictureItem();
  AppendMediaRouterItem();
}

void RenderViewContextMenu::AppendMediaItems() {
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_LOOP,
                                       IDS_CONTENT_CONTEXT_LOOP);
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_CONTROLS,
                                       IDS_CONTENT_CONTEXT_CONTROLS);
}

void RenderViewContextMenu::AppendPluginItems() {
  if (params_.page_url == params_.src_url ||
      (guest_view::GuestViewBase::IsGuest(source_web_contents_) &&
       (!embedder_web_contents_ || !embedder_web_contents_->IsSavable()))) {
    // Both full page and embedded plugins are hosted as guest now,
    // the difference is a full page plugin is not considered as savable.
    // For full page plugin, we show page menu items so long as focus is not
    // within an editable text area.
    if (params_.link_url.is_empty() && params_.selection_text.empty() &&
        !params_.is_editable) {
      AppendPageItems();
    }
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                    IDS_CONTENT_CONTEXT_SAVEPAGEAS);
    // The "Print" menu item should always be included for plugins. If
    // content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)
    // is true the item will be added inside AppendPrintItem(). Otherwise we
    // add "Print" here.
    if (!content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT))
      menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }
}

void RenderViewContextMenu::AppendPageItems() {
  AppendExitFullscreenItem();

  menu_model_.AddItemWithStringId(IDC_BACK, IDS_CONTENT_CONTEXT_BACK);
  menu_model_.AddItemWithStringId(IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD);
  menu_model_.AddItemWithStringId(IDC_RELOAD, IDS_CONTENT_CONTEXT_RELOAD);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_SAVE_PAGE,
                                  IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  AppendMediaRouterItem();

  if (ShouldUseShareMenu()) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    share_submenu_model_ = std::make_unique<sharing::ShareSubmenuModel>(
        GetBrowser(), sharing::ShareSubmenuModel::Context::PAGE,
        params_.page_url);
    if (share_submenu_model_->GetItemCount() > 0) {
      menu_model_.AddSubMenuWithStringId(IDC_CONTENT_CONTEXT_SHARING_SUBMENU,
                                         IDS_SHARE_MENU_TITLE,
                                         share_submenu_model_.get());
    }
  }

  // Send-Tab-To-Self (user's other devices), page level.
  bool send_tab_to_self_menu_present = false;
  if (GetBrowser() && !ShouldUseShareMenu() &&
      send_tab_to_self::ShouldOfferFeature(
          GetBrowser()->tab_strip_model()->GetActiveWebContents())) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    send_tab_to_self_menu_present = true;
    if (send_tab_to_self::GetValidDeviceCount(GetBrowser()->profile()) == 1) {
#if defined(OS_MAC)
      menu_model_.AddItem(IDC_SEND_TAB_TO_SELF_SINGLE_TARGET,
                          l10n_util::GetStringFUTF16(
                              IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
                              send_tab_to_self::GetSingleTargetDeviceName(
                                  GetBrowser()->profile())));
#else
      menu_model_.AddItemWithIcon(
          IDC_SEND_TAB_TO_SELF_SINGLE_TARGET,
          l10n_util::GetStringFUTF16(
              IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
              send_tab_to_self::GetSingleTargetDeviceName(
                  GetBrowser()->profile())),
          ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    } else {
      send_tab_to_self_sub_menu_model_ =
          std::make_unique<send_tab_to_self::SendTabToSelfSubMenuModel>(
              GetBrowser()->tab_strip_model()->GetActiveWebContents(),
              send_tab_to_self::SendTabToSelfMenuType::kContent);
#if defined(OS_MAC)
      menu_model_.AddSubMenuWithStringId(
          IDC_SEND_TAB_TO_SELF, IDS_CONTEXT_MENU_SEND_TAB_TO_SELF,
          send_tab_to_self_sub_menu_model_.get());
#else
      menu_model_.AddSubMenuWithStringIdAndIcon(
          IDC_SEND_TAB_TO_SELF, IDS_CONTEXT_MENU_SEND_TAB_TO_SELF,
          send_tab_to_self_sub_menu_model_.get(),
          ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    }
  }

  // Context menu item for QR Code Generator.
  if (!ShouldUseShareMenu() && IsQRCodeGeneratorEnabled()) {
    // This is presented alongside the send-tab-to-self items, though each may
    // be present without the other due to feature experimentation. Therefore we
    // may or may not need to create a new separator.
    if (!send_tab_to_self_menu_present)
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

    AppendQRCodeGeneratorItem(/*for_image=*/false, /*draw_icon=*/true);

    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  } else if (!ShouldUseShareMenu() && send_tab_to_self_menu_present) {
    // Close out sharing section if send-tab-to-self was present but QR
    // generator was not.
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  const bool canTranslate =
      chrome_translate_client &&
      chrome_translate_client->GetTranslateManager()->CanManuallyTranslate(
          true);
  if (canTranslate) {
    language::LanguageModel* language_model =
        LanguageModelManagerFactory::GetForBrowserContext(browser_context_)
            ->GetPrimaryModel();
    std::unique_ptr<translate::TranslatePrefs> prefs(
        ChromeTranslateClient::CreateTranslatePrefs(
            GetPrefs(browser_context_)));
    std::string locale = translate::TranslateManager::GetTargetLanguage(
        prefs.get(), language_model);
    std::u16string language =
        l10n_util::GetDisplayNameForLocale(locale, locale, true);
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_TRANSLATE,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_TRANSLATE, language));
  }
}

void RenderViewContextMenu::AppendExitFullscreenItem() {
  Browser* browser = GetBrowser();
  if (!browser)
    return;

  // Only show item if in fullscreen mode.
  if (!browser->exclusive_access_manager()
           ->fullscreen_controller()
           ->IsControllerInitiatedFullscreen()) {
    return;
  }

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN,
                                  IDS_CONTENT_CONTEXT_EXIT_FULLSCREEN);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendCopyItem() {
  if (menu_model_.GetItemCount())
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendLinkToTextItems() {
  if (link_to_text_menu_observer_)
    return;

  link_to_text_menu_observer_ =
      LinkToTextMenuObserver::Create(this, GetRenderFrameHost());
  if (link_to_text_menu_observer_) {
    observers_.AddObserver(link_to_text_menu_observer_.get());
    link_to_text_menu_observer_->InitMenu(params_);
  }
}

void RenderViewContextMenu::AppendPrintItem() {
  if (GetPrefs(browser_context_)->GetBoolean(prefs::kPrintingEnabled) &&
      (params_.media_type == ContextMenuDataMediaType::kNone ||
       params_.media_flags & ContextMenuData::kMediaCanPrint) &&
      params_.misspelled_word.empty()) {
    menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }
}

void RenderViewContextMenu::AppendMediaRouterItem() {
  if (media_router::MediaRouterEnabled(browser_context_)) {
    menu_model_.AddItemWithStringId(IDC_ROUTE_MEDIA,
                                    IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
  }
}

void RenderViewContextMenu::AppendRotationItems() {
  if (params_.media_flags & ContextMenuData::kMediaCanRotate) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECW,
                                    IDS_CONTENT_CONTEXT_ROTATECW);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECCW,
                                    IDS_CONTENT_CONTEXT_ROTATECCW);
  }
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(browser_context_);

  base::TrimWhitespace(params_.selection_text, base::TRIM_ALL,
                       &params_.selection_text);
  if (params_.selection_text.empty())
    return;

  base::ReplaceChars(params_.selection_text, AutocompleteMatch::kInvalidChars,
                     u" ", &params_.selection_text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(GetProfile())
      ->Classify(params_.selection_text, false, false,
                 metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);
  selection_navigation_url_ = match.destination_url;
  if (!selection_navigation_url_.is_valid())
    return;

  std::u16string printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  if (AutocompleteMatch::IsSearchType(match.type)) {
    const TemplateURL* const default_provider =
        TemplateURLServiceFactory::GetForProfile(GetProfile())
            ->GetDefaultSearchProvider();
    if (!default_provider)
      return;

    if (!base::Contains(
            params_.properties,
            prefs::kDefaultSearchProviderContextMenuAccessAllowed)) {
      return;
    }

    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_SEARCHWEBFOR,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                                   default_provider->short_name(),
                                   printable_selection_text));
  } else {
    if ((selection_navigation_url_ != params_.link_url) &&
        ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
            selection_navigation_url_.scheme())) {
      menu_model_.AddItem(
          IDC_CONTENT_CONTEXT_GOTOURL,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_GOTOURL,
                                     printable_selection_text));
    }
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  const bool use_spelling = !chrome::IsRunningInForcedAppMode();
  if (use_spelling)
    AppendSpellingSuggestionItems();

  if (!params_.misspelled_word.empty()) {
    AppendSearchProvider();
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
  if (params_.misspelled_word.empty() &&
      DoesInputFieldTypeSupportEmoji(params_.input_field_type) &&
      ui::IsEmojiPanelSupported()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_EMOJI,
                                    IDS_CONTENT_CONTEXT_EMOJI);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

// 'Undo' and 'Redo' for text input with no suggestions and no text selected.
// We make an exception for OS X as context clicking will select the closest
// word. In this case both items are always shown.
#if defined(OS_MAC)
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO,
                                  IDS_CONTENT_CONTEXT_UNDO);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_REDO,
                                  IDS_CONTENT_CONTEXT_REDO);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
#else
  // Also want to show 'Undo' and 'Redo' if 'Emoji' is the only item in the menu
  // so far.
  if (!IsDevToolsURL(params_.page_url) &&
      !content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT) &&
      (!menu_model_.GetItemCount() ||
       menu_model_.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_EMOJI) != -1)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO,
                                    IDS_CONTENT_CONTEXT_UNDO);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_REDO,
                                    IDS_CONTENT_CONTEXT_REDO);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
#endif

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_CUT,
                                  IDS_CONTENT_CONTEXT_CUT);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE,
                                  IDS_CONTENT_CONTEXT_PASTE);

  const bool has_misspelled_word = !params_.misspelled_word.empty();
  if (!has_misspelled_word) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE,
                                    IDS_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::features::IsClipboardHistoryEnabled()) {
    menu_model_.AddItemWithStringId(
        IDC_CONTENT_CLIPBOARD_HISTORY_MENU,
        IDS_CONTEXT_MENU_SHOW_CLIPBOARD_HISTORY_MENU);
    ash::ClipboardHistoryController* clipboard_history_controller =
        ash::ClipboardHistoryController::Get();
    if (clipboard_history_controller &&
        clipboard_history_controller->ShouldShowNewFeatureBadge()) {
      menu_model_.SetIsNewFeatureAt(
          menu_model_.GetIndexOfCommandId(IDC_CONTENT_CLIPBOARD_HISTORY_MENU),
          true);
      clipboard_history_controller->MarkNewFeatureBadgeShown();
    }
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* service = chromeos::LacrosService::Get();
  if (service && service->IsAvailable<crosapi::mojom::ClipboardHistory>()) {
    menu_model_.AddItemWithStringId(
        IDC_CONTENT_CLIPBOARD_HISTORY_MENU,
        IDS_CONTEXT_MENU_SHOW_CLIPBOARD_HISTORY_MENU);
  }
#endif

  if (!has_misspelled_word) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SELECTALL,
                                    IDS_CONTENT_CONTEXT_SELECTALL);
  }

  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendLanguageSettings() {
  const bool use_spelling = !chrome::IsRunningInForcedAppMode();
  if (!use_spelling)
    return;

#if defined(OS_MAC)
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS,
                                  IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);
#else
  if (!spelling_options_submenu_observer_) {
    const int kLanguageRadioGroup = 1;
    spelling_options_submenu_observer_ =
        std::make_unique<SpellingOptionsSubMenuObserver>(this, this,
                                                         kLanguageRadioGroup);
  }

  spelling_options_submenu_observer_->InitMenu(params_);
  observers_.AddObserver(spelling_options_submenu_observer_.get());
#endif
}

void RenderViewContextMenu::AppendSpellingSuggestionItems() {
  if (!spelling_suggestions_menu_observer_) {
    spelling_suggestions_menu_observer_ =
        std::make_unique<SpellingMenuObserver>(this);
  }
  observers_.AddObserver(spelling_suggestions_menu_observer_.get());
  spelling_suggestions_menu_observer_->InitMenu(params_);
}

bool RenderViewContextMenu::AppendAccessibilityLabelsItems() {
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  if (!accessibility_labels_menu_observer_) {
    accessibility_labels_menu_observer_ =
        std::make_unique<AccessibilityLabelsMenuObserver>(this);
  }
  observers_.AddObserver(accessibility_labels_menu_observer_.get());
  accessibility_labels_menu_observer_->InitMenu(params_);
  return accessibility_labels_menu_observer_->ShouldShowLabelsItem();
}

void RenderViewContextMenu::AppendProtocolHandlerSubMenu() {
  const ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty())
    return;

  protocol_handler_registry_observation_.Observe(protocol_handler_registry_);
  is_protocol_submenu_valid_ = true;

  size_t max = IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST -
               IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  for (size_t i = 0; i < handlers.size() && i <= max; i++) {
    protocol_handler_submenu_model_.AddItem(
        IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST + i,
        base::UTF8ToUTF16(handlers[i].url().host()));
  }
  protocol_handler_submenu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  protocol_handler_submenu_model_.AddItem(
      IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH_CONFIGURE));

  menu_model_.AddSubMenu(
      IDC_CONTENT_CONTEXT_OPENLINKWITH,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH),
      &protocol_handler_submenu_model_);
}

void RenderViewContextMenu::AppendPasswordItems() {
  bool add_separator = false;

  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          GetRenderFrameHost());
  // Don't show the item for guest or incognito profiles and also when the
  // automatic generation feature is disabled.
  if (password_manager_util::ManualPasswordGenerationEnabled(driver)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_GENERATEPASSWORD,
                                    IDS_CONTENT_CONTEXT_GENERATEPASSWORD);
    add_separator = true;
  }
  if (password_manager_util::ShowAllSavedPasswordsContextMenuEnabled(driver)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS,
                                    IDS_AUTOFILL_SHOW_ALL_SAVED_FALLBACK);
    add_separator = true;
  }

  if (add_separator)
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendPictureInPictureItem() {
  if (base::FeatureList::IsEnabled(media::kPictureInPicture))
    menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_PICTUREINPICTURE,
                                         IDS_CONTENT_CONTEXT_PICTUREINPICTURE);
}

void RenderViewContextMenu::AppendSharingItems() {
  int items_initial = menu_model_.GetItemCount();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  // Check if the starting separator got added.
  int items_before_sharing = menu_model_.GetItemCount();
  bool starting_separator_added = items_before_sharing > items_initial;

#if !defined(OS_FUCHSIA)
  AppendClickToCallItem();
#endif
  AppendSharedClipboardItem();

  // Add an ending separator if there are sharing items, otherwise remove the
  // starting separator iff we added one above.
  int sharing_items = menu_model_.GetItemCount() - items_before_sharing;
  if (sharing_items > 0)
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  else if (starting_separator_added)
    menu_model_.RemoveItemAt(items_initial);
}

#if !defined(OS_FUCHSIA)
void RenderViewContextMenu::AppendClickToCallItem() {
  SharingClickToCallEntryPoint entry_point;
  absl::optional<std::string> phone_number;
  std::string selection_text;
  if (ShouldOfferClickToCallForURL(browser_context_, params_.link_url)) {
    entry_point = SharingClickToCallEntryPoint::kRightClickLink;
    phone_number = params_.link_url.GetContent();
  } else if (!params_.selection_text.empty()) {
    entry_point = SharingClickToCallEntryPoint::kRightClickSelection;
    selection_text = base::UTF16ToUTF8(params_.selection_text);
    phone_number =
        ExtractPhoneNumberForClickToCall(browser_context_, selection_text);
  }

  if (!phone_number || phone_number->empty())
    return;

  if (!click_to_call_context_menu_observer_) {
    click_to_call_context_menu_observer_ =
        std::make_unique<ClickToCallContextMenuObserver>(this);
    observers_.AddObserver(click_to_call_context_menu_observer_.get());
  }

  click_to_call_context_menu_observer_->BuildMenu(*phone_number, selection_text,
                                                  entry_point);
}
#endif  // !defined(OS_FUCHSIA)

void RenderViewContextMenu::AppendSharedClipboardItem() {
  if (!ShouldOfferSharedClipboard(browser_context_, params_.selection_text))
    return;

  if (!shared_clipboard_context_menu_observer_) {
    shared_clipboard_context_menu_observer_ =
        std::make_unique<SharedClipboardContextMenuObserver>(this);
    observers_.AddObserver(shared_clipboard_context_menu_observer_.get());
  }
  shared_clipboard_context_menu_observer_->InitMenu(params_);
}

void RenderViewContextMenu::AppendLensRegionSearchItem() {
  int resource_id = IDS_CONTENT_CONTEXT_LENS_REGION_SEARCH;
  if (lens::features::kRegionSearchUseMenuItemAltText1.Get()) {
    resource_id = IDS_CONTENT_CONTEXT_LENS_REGION_SEARCH_ALT1;
  } else if (lens::features::kRegionSearchUseMenuItemAltText2.Get()) {
    resource_id = IDS_CONTENT_CONTEXT_LENS_REGION_SEARCH_ALT2;
  } else if (lens::features::kRegionSearchUseMenuItemAltText3.Get()) {
    resource_id = IDS_CONTENT_CONTEXT_LENS_REGION_SEARCH_ALT3;
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH,
                                  resource_id);
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsCommandIdEnabled(int id) const {
  // Disable context menu in locked fullscreen mode (the menu is not really
  // disabled as the user can still open it, but all the individual context menu
  // entries are disabled / greyed out).
  if (GetBrowser() && platform_util::IsBrowserLockedFullscreen(GetBrowser()))
    return false;

  {
    bool enabled = false;
    if (RenderViewContextMenuBase::IsCommandIdKnown(id, &enabled))
      return enabled;
  }

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  int content_restrictions = 0;
  if (core_tab_helper)
    content_restrictions = core_tab_helper->content_restrictions();
  if (id == IDC_PRINT && (content_restrictions & CONTENT_RESTRICTION_PRINT))
    return false;

  if (id == IDC_SAVE_PAGE &&
      (content_restrictions & CONTENT_RESTRICTION_SAVE)) {
    return false;
  }

  PrefService* prefs = GetPrefs(browser_context_);

  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable);
  }

  // Extension items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdEnabled(id);

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return true;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    return params_.link_url.is_valid();
  }

  switch (id) {
    case IDC_BACK:
      return embedder_web_contents_->GetController().CanGoBack();

    case IDC_FORWARD:
      return embedder_web_contents_->GetController().CanGoForward();

    case IDC_RELOAD:
      return IsReloadEnabled();

    case IDC_VIEW_SOURCE:
    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      return IsViewSourceEnabled();

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      return IsDevCommandEnabled(id);

    case IDC_CONTENT_CONTEXT_TRANSLATE:
      return IsTranslateEnabled();

    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
    case IDC_CONTENT_CONTEXT_OPENLINKINPROFILE:
    case IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP:
      return params_.link_url.is_valid();

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.unfiltered_link_url.is_valid();

    case IDC_CONTENT_CONTEXT_COPYLINKTEXT:
      return true;

    case IDC_CONTENT_CONTEXT_SAVELINKAS:
      return IsSaveLinkAsEnabled();

    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS:
      return IsSaveImageAsEnabled();

    // The images shown in the most visited thumbnails can't be opened or
    // searched for conventionally.
    case IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB:
    case IDC_CONTENT_CONTEXT_LOAD_IMAGE:
    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
    case IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE:
      return params_.src_url.is_valid() &&
             (params_.src_url.scheme() != content::kChromeUIScheme);

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      return params_.has_image_contents;

    // Media control commands should all be disabled if the player is in an
    // error state.
    case IDC_CONTENT_CONTEXT_PLAYPAUSE:
      return (params_.media_flags & ContextMenuData::kMediaInError) == 0;

    // Loop command should be disabled if the player is in an error state.
    case IDC_CONTENT_CONTEXT_LOOP:
      return (params_.media_flags & ContextMenuData::kMediaCanLoop) != 0 &&
             (params_.media_flags & ContextMenuData::kMediaInError) == 0;

    // Mute and unmute should also be disabled if the player has no audio.
    case IDC_CONTENT_CONTEXT_MUTE:
      return (params_.media_flags & ContextMenuData::kMediaHasAudio) != 0 &&
             (params_.media_flags & ContextMenuData::kMediaInError) == 0;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      return (params_.media_flags & ContextMenuData::kMediaCanToggleControls) !=
             0;

    case IDC_CONTENT_CONTEXT_ROTATECW:
    case IDC_CONTENT_CONTEXT_ROTATECCW: {
      bool is_pdf_viewer_fullscreen = false;
#if BUILDFLAG(ENABLE_PDF)
      // Rotate commands should be disabled when in PDF Viewer's Presentation
      // mode.
      is_pdf_viewer_fullscreen =
          IsPdfPluginURL(GetDocumentURL(params_)) && IsHTML5Fullscreen();
#endif
      return !is_pdf_viewer_fullscreen &&
             (params_.media_flags & ContextMenuData::kMediaCanRotate) != 0;
    }

    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.src_url.is_valid();

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
      return IsSaveAsEnabled();

    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      // Currently, a media element can be opened in a new tab iff it can
      // be saved. So rather than duplicating the MediaCanSave flag, we rely
      // on that here.
      return !!(params_.media_flags & ContextMenuData::kMediaCanSave);

    case IDC_SAVE_PAGE:
      return IsSavePageEnabled();

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      return params_.frame_url.is_valid() &&
             params_.frame_url.GetOrigin() != chrome::kChromeUIPrintURL;

    case IDC_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanUndo);

    case IDC_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanRedo);

    case IDC_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanCut);

    case IDC_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanCopy);

    case IDC_CONTENT_CONTEXT_PASTE:
      return IsPasteEnabled();

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      return IsPasteAndMatchStyleEnabled();

    case IDC_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanDelete);

    case IDC_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanSelectAll);

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return IsOpenLinkOTREnabled();

    case IDC_PRINT:
      return IsPrintPreviewEnabled();

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
      return IsSearchWebForEnabled();

    case IDC_CONTENT_CONTEXT_GOTOURL:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
    case IDC_SEND_TAB_TO_SELF:
    case IDC_SEND_TAB_TO_SELF_SINGLE_TARGET:
    case IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH:
      return true;

    case IDC_CONTENT_CONTEXT_GENERATE_QR_CODE:
      return IsQRCodeGeneratorEnabled();

    case IDC_CONTENT_CONTEXT_SHARING_SUBMENU:
      return true;

    case IDC_CONTENT_LINK_SEND_TAB_TO_SELF:
    case IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET:
      return send_tab_to_self::AreContentRequirementsMet(
          params_.link_url, GetBrowser()->profile());

    case IDC_CHECK_SPELLING_WHILE_TYPING:
      return prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable);

#if !defined(OS_MAC) && defined(OS_POSIX)
    // TODO(suzhe): this should not be enabled for password fields.
    case IDC_INPUT_METHODS_MENU:
      return true;
#endif

    case IDC_SPELLCHECK_MENU:
    case IDC_CONTENT_CONTEXT_OPENLINKWITH:
    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS:
    case IDC_CONTENT_CONTEXT_GENERATEPASSWORD:
    case IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS:
      return true;

    case IDC_ROUTE_MEDIA:
      return IsRouteMediaEnabled();

    case IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN:
      return true;

    case IDC_CONTENT_CONTEXT_PICTUREINPICTURE:
      return !!(params_.media_flags &
                ContextMenuData::kMediaCanPictureInPicture);

    case IDC_CONTENT_CONTEXT_EMOJI:
      return params_.is_editable;

    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5:
      return true;

    case IDC_CONTENT_CLIPBOARD_HISTORY_MENU:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (chromeos::features::IsClipboardHistoryEnabled())
        return ash::ClipboardHistoryController::Get()->CanShowMenu();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    {
      auto* service = chromeos::LacrosService::Get();
      return service &&
             service->IsAvailable<crosapi::mojom::ClipboardHistory>();
    }
#else
      NOTREACHED();
#endif
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool RenderViewContextMenu::IsCommandIdChecked(int id) const {
  if (RenderViewContextMenuBase::IsCommandIdChecked(id))
    return true;

  // See if the video is set to looping.
  if (id == IDC_CONTENT_CONTEXT_LOOP)
    return (params_.media_flags & ContextMenuData::kMediaLoop) != 0;

  if (id == IDC_CONTENT_CONTEXT_CONTROLS)
    return (params_.media_flags & ContextMenuData::kMediaControls) != 0;

  if (id == IDC_CONTENT_CONTEXT_PICTUREINPICTURE)
    return (params_.media_flags & ContextMenuData::kMediaPictureInPicture) != 0;

  if (id == IDC_CONTENT_CONTEXT_EMOJI)
    return false;

  // Extension items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdChecked(id);

  return false;
}

bool RenderViewContextMenu::IsCommandIdVisible(int id) const {
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdVisible(id);
  return RenderViewContextMenuBase::IsCommandIdVisible(id);
}

void RenderViewContextMenu::ExecuteCommand(int id, int event_flags) {
  RenderViewContextMenuBase::ExecuteCommand(id, event_flags);
  if (command_executed_)
    return;
  command_executed_ = true;

  // Process extension menu items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id)) {
    RenderFrameHost* render_frame_host = GetRenderFrameHost();
    if (render_frame_host) {
      extension_items_.ExecuteCommand(id, source_web_contents_,
                                      render_frame_host, params_);
    }
    return;
  }

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    ExecProtocolHandler(event_flags,
                        id - IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST);
    return;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    ExecOpenLinkInProfile(id - IDC_OPEN_LINK_IN_PROFILE_FIRST);
    return;
  }

  switch (id) {
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_LINK, "" /* extra_headers */,
                              true /* started_from_context_menu */);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              WindowOpenDisposition::NEW_WINDOW,
                              ui::PAGE_TRANSITION_LINK, "" /* extra_headers */,
                              true /* started_from_context_menu */);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      // Pass along the |referring_url| so we can show it in browser UI. Note
      // that this won't and shouldn't be sent via the referrer header.
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              WindowOpenDisposition::OFF_THE_RECORD,
                              ui::PAGE_TRANSITION_LINK, "" /* extra_headers */,
                              true /* started_from_context_menu */);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP:
      ExecOpenWebApp();
      break;

    case IDC_CONTENT_CONTEXT_SAVELINKAS:
      ExecSaveLinkAs();
      break;

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS:
      ExecSaveAs();
      break;

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.unfiltered_link_url);
      break;

    case IDC_CONTENT_CONTEXT_COPYLINKTEXT:
      ExecCopyLinkText();
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
      WriteURLToClipboard(params_.src_url);
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      ExecCopyImageAt();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      ExecSearchWebForImage();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE:
      ExecSearchLensForImage();
      break;

    case IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH:
      ExecLensRegionSearch();
      break;

    case IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB:
      OpenURLWithExtraHeaders(params_.src_url, GetDocumentURL(params_),
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_LINK, std::string(), false);
      break;

    case IDC_CONTENT_CONTEXT_LOAD_IMAGE:
      ExecLoadImage();
      break;

    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      OpenURL(params_.src_url, GetDocumentURL(params_),
              WindowOpenDisposition::NEW_BACKGROUND_TAB,
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_PLAYPAUSE:
      ExecPlayPause();
      break;

    case IDC_CONTENT_CONTEXT_MUTE:
      ExecMute();
      break;

    case IDC_CONTENT_CONTEXT_LOOP:
      ExecLoop();
      break;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      ExecControls();
      break;

    case IDC_CONTENT_CONTEXT_ROTATECW:
      ExecRotateCW();
      break;

    case IDC_CONTENT_CONTEXT_ROTATECCW:
      ExecRotateCCW();
      break;

    case IDC_BACK:
      embedder_web_contents_->GetController().GoBack();
      break;

    case IDC_FORWARD:
      embedder_web_contents_->GetController().GoForward();
      break;

    case IDC_SAVE_PAGE:
      embedder_web_contents_->OnSavePage();
      break;

    case IDC_SEND_TAB_TO_SELF_SINGLE_TARGET:
      send_tab_to_self::ShareToSingleTarget(
          GetBrowser()->tab_strip_model()->GetActiveWebContents());
      send_tab_to_self::RecordDeviceClicked(
          send_tab_to_self::ShareEntryPoint::kContentMenu);
      break;

    case IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET:
      send_tab_to_self::ShareToSingleTarget(
          GetBrowser()->tab_strip_model()->GetActiveWebContents(),
          params_.link_url);
      send_tab_to_self::RecordDeviceClicked(
          send_tab_to_self::ShareEntryPoint::kLinkMenu);
      break;

    case IDC_CONTENT_CONTEXT_GENERATE_QR_CODE: {
      auto* web_contents =
          GetBrowser()->tab_strip_model()->GetActiveWebContents();
      auto* bubble_controller =
          qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
      if (params_.media_type == ContextMenuDataMediaType::kImage) {
        base::RecordAction(
            UserMetricsAction("SharingQRCode.DialogLaunched.ContextMenuImage"));
        bubble_controller->ShowBubble(params_.src_url);
      } else {
        base::RecordAction(
            UserMetricsAction("SharingQRCode.DialogLaunched.ContextMenuPage"));
        NavigationEntry* entry =
            embedder_web_contents_->GetController().GetLastCommittedEntry();
        bubble_controller->ShowBubble(entry->GetURL());
      }
      break;
    }

    case IDC_RELOAD:
      chrome::Reload(GetBrowser(), WindowOpenDisposition::CURRENT_TAB);
      break;

    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
      ExecReloadPackagedApp();
      break;

    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      ExecRestartPackagedApp();
      break;

    case IDC_PRINT:
      ExecPrint();
      break;

    case IDC_ROUTE_MEDIA:
      ExecRouteMedia();
      break;

    case IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN:
      ExecExitFullscreen();
      break;

    case IDC_VIEW_SOURCE:
      embedder_web_contents_->GetMainFrame()->ViewSource();
      break;

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      ExecInspectElement();
      break;

    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
      ExecInspectBackgroundPage();
      break;

    case IDC_CONTENT_CONTEXT_TRANSLATE:
      ExecTranslate();
      break;

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      source_web_contents_->ReloadFocusedFrame();
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      if (GetRenderFrameHost())
        GetRenderFrameHost()->ViewSource();
      break;

    case IDC_CONTENT_CONTEXT_UNDO:
      source_web_contents_->Undo();
      break;

    case IDC_CONTENT_CONTEXT_REDO:
      source_web_contents_->Redo();
      break;

    case IDC_CONTENT_CONTEXT_CUT:
      source_web_contents_->Cut();
      break;

    case IDC_CONTENT_CONTEXT_COPY:
      source_web_contents_->Copy();
      break;

    case IDC_CONTENT_CONTEXT_PASTE:
      source_web_contents_->Paste();
      break;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      source_web_contents_->PasteAndMatchStyle();
      break;

    case IDC_CONTENT_CONTEXT_DELETE:
      source_web_contents_->Delete();
      break;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      source_web_contents_->SelectAll();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL:
      OpenURL(selection_navigation_url_, GURL(),
              ui::DispositionFromEventFlags(
                  event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB),
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      ExecLanguageSettings(event_flags);
      break;

    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS:
      ExecProtocolHandlerSettings(event_flags);
      break;

    case IDC_CONTENT_CONTEXT_GENERATEPASSWORD:
      password_manager_util::UserTriggeredManualGenerationFromContextMenu(
          ChromePasswordManagerClient::FromWebContents(source_web_contents_));
      break;

    case IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS:
      NavigateToManagePasswordsPage(
          GetBrowser(),
          password_manager::ManagePasswordsReferrer::kPasswordContextMenu);
      break;

    case IDC_CONTENT_CONTEXT_PICTUREINPICTURE:
      ExecPictureInPicture();
      break;

    case IDC_CONTENT_CONTEXT_EMOJI: {
      // The emoji dialog is UI that can interfere with the fullscreen bubble,
      // so drop fullscreen when it is shown. https://crbug.com/1170584
      // TODO(avi): Do we need to attach the fullscreen block to the emoji
      // panel?
      source_web_contents_->ForSecurityDropFullscreen().RunAndReset();

      Browser* browser = GetBrowser();
      if (browser) {
        browser->window()->ShowEmojiPanel();
      } else {
        // TODO(https://crbug.com/919167): Ensure this is called in the correct
        // process. This fails in print preview for PWA windows on Mac.
        ui::ShowEmojiPanel();
      }
      break;
    }

    case IDC_CONTENT_CLIPBOARD_HISTORY_MENU: {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      auto* host_native_view = GetRenderFrameHost()
                                   ? GetRenderFrameHost()->GetNativeView()
                                   : nullptr;
      if (!host_native_view)
        break;

      // Calculate the anchor point in screen coordinates.
      gfx::Point anchor_point_in_screen =
          host_native_view->GetBoundsInScreen().origin();
      anchor_point_in_screen.Offset(params_.x, params_.y);

      // Calculate the menu source type from `event_flags`.
      ui::MenuSourceType source_type;
      if (event_flags & ui::EF_LEFT_MOUSE_BUTTON)
        source_type = ui::MENU_SOURCE_MOUSE;
      else if (event_flags & ui::EF_FROM_TOUCH)
        source_type = ui::MENU_SOURCE_TOUCH;
      else
        source_type = ui::MENU_SOURCE_KEYBOARD;

#if BUILDFLAG(IS_CHROMEOS_ASH)
      ash::ClipboardHistoryController::Get()->ShowMenu(
          gfx::Rect(anchor_point_in_screen, gfx::Size()), source_type,
          crosapi::mojom::ClipboardHistoryControllerShowSource::
              kRenderViewContextMenu);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
      {
        auto* service = chromeos::LacrosService::Get();
        if (service &&
            service->IsAvailable<crosapi::mojom::ClipboardHistory>()) {
          service->GetRemote<crosapi::mojom::ClipboardHistory>()->ShowClipboard(
              gfx::Rect(anchor_point_in_screen, gfx::Size()), source_type,
              crosapi::mojom::ClipboardHistoryControllerShowSource::
                  kRenderViewContextMenu);
        }
      }
#endif
#else
      NOTREACHED();
#endif
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void RenderViewContextMenu::AddSpellCheckServiceItem(bool is_checked) {
  AddSpellCheckServiceItem(&menu_model_, is_checked);
}

void RenderViewContextMenu::AddAccessibilityLabelsServiceItem(bool is_checked) {
  if (is_checked) {
    menu_model_.AddCheckItemWithStringId(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE,
        IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_MENU_OPTION);
  } else {
    // Add the submenu if the whole feature is not enabled.
    accessibility_labels_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE,
        IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_SEND);
    accessibility_labels_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE,
        IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_SEND_ONCE);
    menu_model_.AddSubMenu(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_MENU_OPTION),
        &accessibility_labels_submenu_model_);
  }
}

// static
void RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
    base::OnceCallback<void(RenderViewContextMenu*)> cb) {
  *GetMenuShownCallback() = std::move(cb);
}

ProtocolHandlerRegistry::ProtocolHandlerList
RenderViewContextMenu::GetHandlersForLinkUrl() {
  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      protocol_handler_registry_->GetHandlersFor(params_.link_url.scheme());
  std::sort(handlers.begin(), handlers.end());
  return handlers;
}

void RenderViewContextMenu::OnProtocolHandlerRegistryChanged() {
  is_protocol_submenu_valid_ = false;
}

void RenderViewContextMenu::NotifyMenuShown() {
  auto* cb = GetMenuShownCallback();
  if (!cb->is_null())
    std::move(*cb).Run(this);
}

std::u16string RenderViewContextMenu::PrintableSelectionText() {
  return gfx::TruncateString(params_.selection_text, kMaxSelectionTextLength,
                             gfx::WORD_BREAK);
}

void RenderViewContextMenu::EscapeAmpersands(std::u16string* text) {
  base::ReplaceChars(*text, u"&", u"&&", text);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const policy::DlpRulesManager* RenderViewContextMenu::GetDlpRulesManager()
    const {
  return policy::DlpRulesManagerFactory::GetForPrimaryProfile();
}
#endif

// Controller functions --------------------------------------------------------

bool RenderViewContextMenu::IsReloadEnabled() const {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(embedder_web_contents_);
  return core_tab_helper && chrome::CanReload(GetBrowser());
}

bool RenderViewContextMenu::IsViewSourceEnabled() const {
  if (!!extensions::MimeHandlerViewGuest::FromWebContents(
          source_web_contents_)) {
    return false;
  }
  return (params_.media_type != ContextMenuDataMediaType::kPlugin) &&
         embedder_web_contents_->GetController().CanViewSource();
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  if (id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
      id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE) {
    PrefService* prefs = GetPrefs(browser_context_);
    if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled))
      return false;

    // Don't enable the web inspector if the developer tools are disabled via
    // the preference dev-tools-disabled.
    if (!DevToolsWindow::AllowDevToolsFor(GetProfile(), source_web_contents_))
      return false;
  }

  return true;
}

bool RenderViewContextMenu::IsTranslateEnabled() const {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  // If no |chrome_translate_client| attached with this WebContents or we're
  // viewing in a MimeHandlerViewGuest translate will be disabled.
  if (!chrome_translate_client ||
      !!extensions::MimeHandlerViewGuest::FromWebContents(
          source_web_contents_)) {
    return false;
  }
  std::string source_lang =
      chrome_translate_client->GetLanguageState().source_language();
  // Note that we intentionally enable the menu even if the source and
  // target languages are identical.  This is to give a way to user to
  // translate a page that might contains text fragments in a different
  // language.
  return ((params_.edit_flags & ContextMenuDataEditFlags::kCanTranslate) !=
          0) &&
         !source_lang.empty() &&  // Did we receive the page language yet?
         // Disable on the Instant Extended NTP.
         !search::IsInstantNTP(embedder_web_contents_);
}

bool RenderViewContextMenu::IsSaveLinkAsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(browser_context_);
  if (service->GetURLBlocklistState(params_.link_url) ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return false;
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  Profile* const profile = Profile::FromBrowserContext(browser_context_);
  if (profile->IsChild()) {
    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    if (url_filter->GetFilteringBehaviorForURL(params_.link_url) !=
        SupervisedUserURLFilter::FilteringBehavior::ALLOW)
      return false;
  }
#endif

  return params_.link_url.is_valid() &&
         ProfileIOData::IsHandledProtocol(params_.link_url.scheme());
}

bool RenderViewContextMenu::IsSaveImageAsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  return params_.has_image_contents;
}

bool RenderViewContextMenu::IsSaveAsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  const GURL& url = params_.src_url;
  bool can_save = (params_.media_flags & ContextMenuData::kMediaCanSave) &&
                  url.is_valid() &&
                  ProfileIOData::IsHandledProtocol(url.scheme());
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Do not save the preview PDF on the print preview page.
  can_save = can_save &&
             !(printing::PrintPreviewDialogController::IsPrintPreviewURL(url));
#endif
  return can_save;
}

bool RenderViewContextMenu::IsSavePageEnabled() const {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(embedder_web_contents_);
  if (!core_tab_helper)
    return false;

  Browser* browser = GetBrowser();
  if (browser && !browser->CanSaveContents(embedder_web_contents_))
    return false;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  // We save the last committed entry (which the user is looking at), as
  // opposed to any pending URL that hasn't committed yet.
  NavigationEntry* entry =
      embedder_web_contents_->GetController().GetLastCommittedEntry();
  return content::IsSavableURL(entry ? entry->GetURL() : GURL());
}

bool RenderViewContextMenu::IsPasteEnabled() const {
  if (!(params_.edit_flags & ContextMenuDataEditFlags::kCanPaste))
    return false;

  std::vector<std::u16string> types;
  ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
      ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/false).get(), &types);
  return !types.empty();
}

bool RenderViewContextMenu::IsSearchWebForEnabled() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const policy::DlpRulesManager* dlp_rules_manager = GetDlpRulesManager();
  if (!dlp_rules_manager) {
    return true;
  }
  policy::DlpRulesManager::Level level =
      dlp_rules_manager->IsRestrictedDestination(
          params_.page_url, selection_navigation_url_,
          policy::DlpRulesManager::Restriction::kClipboard,
          /*out_source_pattern=*/nullptr, /*out_destination_pattern=*/nullptr);
  // TODO(crbug.com/1222057): show a warning if the level is kWarn
  return level != policy::DlpRulesManager::Level::kBlock;
#else
  return true;
#endif
}

bool RenderViewContextMenu::IsPasteAndMatchStyleEnabled() const {
  if (!(params_.edit_flags & ContextMenuDataEditFlags::kCanPaste))
    return false;

  return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
      ui::ClipboardFormatType::PlainTextType(), ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/false).get());
}

bool RenderViewContextMenu::IsPrintPreviewEnabled() const {
  if (params_.media_type != ContextMenuDataMediaType::kNone &&
      !(params_.media_flags & ContextMenuData::kMediaCanPrint)) {
    return false;
  }

  Browser* browser = GetBrowser();
  return browser && chrome::CanPrint(browser);
}

bool RenderViewContextMenu::IsQRCodeGeneratorEnabled() const {
  if (!GetBrowser())
    return false;

  NavigationEntry* entry =
      embedder_web_contents_->GetController().GetLastCommittedEntry();
  if (!entry)
    return false;

  return qrcode_generator::QRCodeGeneratorBubbleController::
      IsGeneratorAvailable(entry->GetURL());
}

bool RenderViewContextMenu::IsLensRegionSearchEnabled() const {
  return base::FeatureList::IsEnabled(lens::features::kLensRegionSearch) &&
         search::DefaultSearchProviderIsGoogle(GetProfile()) &&
         GetPrefs(browser_context_)
             ->GetBoolean(prefs::kLensRegionSearchEnabled);
}

void RenderViewContextMenu::AppendQRCodeGeneratorItem(bool for_image,
                                                      bool draw_icon) {
  if (!IsQRCodeGeneratorEnabled())
    return;
  auto string_id = for_image ? IDS_CONTEXT_MENU_GENERATE_QR_CODE_IMAGE
                             : IDS_CONTEXT_MENU_GENERATE_QR_CODE_PAGE;
#if defined(OS_MAC)
  draw_icon = false;
#endif
  if (draw_icon) {
    menu_model_.AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_GENERATE_QR_CODE, string_id,
        ui::ImageModel::FromVectorIcon(kQrcodeGeneratorIcon));
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_GENERATE_QR_CODE,
                                    string_id);
  }
}

std::unique_ptr<ui::DataTransferEndpoint>
RenderViewContextMenu::CreateDataEndpoint(bool notify_if_restricted) const {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (render_frame_host &&
      !render_frame_host->GetBrowserContext()->IsOffTheRecord()) {
    return std::make_unique<ui::DataTransferEndpoint>(
        render_frame_host->GetLastCommittedOrigin(), notify_if_restricted);
  }
  return nullptr;
}

bool RenderViewContextMenu::IsRouteMediaEnabled() const {
  if (!media_router::MediaRouterEnabled(browser_context_))
    return false;

  Browser* browser = GetBrowser();
  if (!browser)
    return false;

  // Disable the command if there is an active modal dialog.
  // We don't use |source_web_contents_| here because it could be the
  // WebContents for something that's not the current tab (e.g., WebUI
  // modal dialog).
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  return !manager || !manager->IsDialogActive();
}

bool RenderViewContextMenu::IsOpenLinkOTREnabled() const {
  if (browser_context_->IsOffTheRecord() || !params_.link_url.is_valid())
    return false;

  if (!IsURLAllowedInIncognito(params_.link_url, browser_context_))
    return false;

  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(GetPrefs(browser_context_));
  return incognito_avail != IncognitoModePrefs::DISABLED;
}

void RenderViewContextMenu::ExecOpenWebApp() {
  absl::optional<web_app::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(
          Profile::FromBrowserContext(browser_context_), params_.link_url);
  // |app_id| could be nullopt if it has been uninstalled since the user
  // opened the context menu.
  if (!app_id)
    return;

  apps::AppLaunchParams launch_params(
      *app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB,
      apps::mojom::AppLaunchSource::kSourceContextMenu);
  launch_params.override_url = params_.link_url;
  apps::AppServiceProxyFactory::GetForProfile(GetProfile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParams(std::move(launch_params));
}

void RenderViewContextMenu::ExecProtocolHandler(int event_flags,
                                                int handler_index) {
  if (!is_protocol_submenu_valid_) {
    // A protocol was changed since the time that the menu was built, so the
    // index passed in isn't valid. The only thing that can be done now safely
    // is to bail.
    return;
  }

  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty())
    return;

  base::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.ContextMenu_Open"));
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  OpenURL(handlers[handler_index].TranslateUrl(params_.link_url),
          GetDocumentURL(params_), disposition, ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecOpenLinkInProfile(int profile_index) {
  DCHECK_GE(profile_index, 0);
  DCHECK_LE(profile_index, static_cast<int>(profile_link_paths_.size()));

  base::FilePath profile_path = profile_link_paths_[profile_index];
  profiles::SwitchToProfile(
      profile_path, false,
      base::BindRepeating(OnProfileCreated, params_.link_url,
                          CreateReferrer(params_.link_url, params_)));
}

void RenderViewContextMenu::ExecInspectElement() {
  base::RecordAction(UserMetricsAction("DevTools_InspectElement"));
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  DevToolsWindow::InspectElement(render_frame_host, params_.x, params_.y);
}

void RenderViewContextMenu::ExecInspectBackgroundPage() {
  const Extension* platform_app = GetExtension();
  DCHECK(platform_app);
  DCHECK(platform_app->is_platform_app());

  extensions::devtools_util::InspectBackgroundPage(platform_app, GetProfile());
}

void RenderViewContextMenu::ExecSaveLinkAs() {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;

  RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);

  const GURL& url = params_.link_url;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("render_view_context_menu", R"(
        semantics {
          sender: "Save Link As"
          description: "Saving url to local file."
          trigger:
            "The user selects the 'Save link as...' command in the context "
            "menu."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled by settings. The request is made "
            "only if user chooses 'Save link as...' in the context menu."
          policy_exception_justification: "Not implemented."
        })");

  auto dl_params = std::make_unique<DownloadUrlParameters>(
      url, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), traffic_annotation);
  content::Referrer referrer = CreateReferrer(url, params_);
  dl_params->set_referrer(referrer.url);
  dl_params->set_referrer_policy(
      content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  dl_params->set_referrer_encoding(params_.frame_charset);
  dl_params->set_initiator(url::Origin::Create(GetDocumentURL(params_)));
  dl_params->set_suggested_name(params_.suggested_filename);
  dl_params->set_prompt(true);
  dl_params->set_download_source(download::DownloadSource::CONTEXT_MENU);

  browser_context_->GetDownloadManager()->DownloadUrl(std::move(dl_params));
}

void RenderViewContextMenu::ExecSaveAs() {
  bool is_large_data_url =
      params_.has_image_contents && params_.src_url.is_empty();
  if (params_.media_type == ContextMenuDataMediaType::kCanvas ||
      (params_.media_type == ContextMenuDataMediaType::kImage &&
       is_large_data_url)) {
    RenderFrameHost* frame_host = GetRenderFrameHost();
    if (frame_host)
      frame_host->SaveImageAt(params_.x, params_.y);
  } else {
    RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
    const GURL& url = params_.src_url;
    content::Referrer referrer = CreateReferrer(url, params_);
    std::string headers;

    RenderFrameHost* frame_host =
        (params_.media_type == ContextMenuDataMediaType::kPlugin)
            ? source_web_contents_->GetOuterWebContentsFrame()
            : GetRenderFrameHost();
    if (frame_host) {
      source_web_contents_->SaveFrameWithHeaders(
          url, referrer, headers, params_.suggested_filename, frame_host);
    }
  }
}

void RenderViewContextMenu::ExecExitFullscreen() {
  Browser* browser = GetBrowser();
  if (!browser) {
    NOTREACHED();
    return;
  }

  browser->exclusive_access_manager()->ExitExclusiveAccess();
}

void RenderViewContextMenu::ExecCopyLinkText() {
  ui::ScopedClipboardWriter scw(
      ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/true));
  scw.WriteText(params_.link_text);
}

void RenderViewContextMenu::ExecCopyImageAt() {
  RenderFrameHost* frame_host = GetRenderFrameHost();
  if (frame_host)
    frame_host->CopyImageAt(params_.x, params_.y);
}

void RenderViewContextMenu::ExecSearchLensForImage() {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper)
    return;
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;

  core_tab_helper->SearchWithLensInNewTab(
      render_frame_host, params().src_url,
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM);
}

void RenderViewContextMenu::ExecLensRegionSearch() {
#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_LINUX)
  if (!lens_region_search_controller_)
    lens_region_search_controller_ =
        std::make_unique<lens::LensRegionSearchController>(
            source_web_contents_);
  lens_region_search_controller_->Start();
#endif
}

void RenderViewContextMenu::ExecSearchWebForImage() {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper)
    return;
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  core_tab_helper->SearchByImageInNewTab(render_frame_host, params().src_url);
}

void RenderViewContextMenu::ExecLoadImage() {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  chrome_render_frame->RequestReloadImageForContextNode();
}

void RenderViewContextMenu::ExecPlayPause() {
  bool play = !!(params_.media_flags & ContextMenuData::kMediaPaused);
  if (play)
    base::RecordAction(UserMetricsAction("MediaContextMenu_Play"));
  else
    base::RecordAction(UserMetricsAction("MediaContextMenu_Pause"));

  MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                      blink::mojom::MediaPlayerAction(
                          blink::mojom::MediaPlayerActionType::kPlay, play));
}

void RenderViewContextMenu::ExecMute() {
  bool mute = !(params_.media_flags & ContextMenuData::kMediaMuted);
  if (mute)
    base::RecordAction(UserMetricsAction("MediaContextMenu_Mute"));
  else
    base::RecordAction(UserMetricsAction("MediaContextMenu_Unmute"));

  MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                      blink::mojom::MediaPlayerAction(
                          blink::mojom::MediaPlayerActionType::kMute, mute));
}

void RenderViewContextMenu::ExecLoop() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_Loop"));
  MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                      blink::mojom::MediaPlayerAction(
                          blink::mojom::MediaPlayerActionType::kLoop,
                          !IsCommandIdChecked(IDC_CONTENT_CONTEXT_LOOP)));
}

void RenderViewContextMenu::ExecControls() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_Controls"));
  MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                      blink::mojom::MediaPlayerAction(
                          blink::mojom::MediaPlayerActionType::kControls,
                          !IsCommandIdChecked(IDC_CONTENT_CONTEXT_CONTROLS)));
}

void RenderViewContextMenu::ExecRotateCW() {
  base::RecordAction(UserMetricsAction("PluginContextMenu_RotateClockwise"));
  PluginActionAt(gfx::Point(params_.x, params_.y),
                 blink::mojom::PluginActionType::kRotate90Clockwise);
}

void RenderViewContextMenu::ExecRotateCCW() {
  base::RecordAction(
      UserMetricsAction("PluginContextMenu_RotateCounterclockwise"));
  PluginActionAt(gfx::Point(params_.x, params_.y),
                 blink::mojom::PluginActionType::kRotate90Counterclockwise);
}

void RenderViewContextMenu::ExecReloadPackagedApp() {
  const Extension* platform_app = GetExtension();
  DCHECK(platform_app);
  DCHECK(platform_app->is_platform_app());

  extensions::ExtensionSystem::Get(browser_context_)
      ->extension_service()
      ->ReloadExtension(platform_app->id());
}

void RenderViewContextMenu::ExecRestartPackagedApp() {
  const Extension* platform_app = GetExtension();
  DCHECK(platform_app);
  DCHECK(platform_app->is_platform_app());

  apps::AppLoadService::Get(GetProfile())
      ->RestartApplication(platform_app->id());
}

void RenderViewContextMenu::ExecPrint() {
#if BUILDFLAG(ENABLE_PRINTING)
  if (params_.media_type != ContextMenuDataMediaType::kNone) {
    RenderFrameHost* rfh = GetRenderFrameHost();
    if (rfh) {
      mojo::AssociatedRemote<printing::mojom::PrintRenderFrame> remote;
      rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote);
      remote->PrintNodeUnderContextMenu();
    }
    return;
  }

  printing::StartPrint(
      source_web_contents_, mojo::NullAssociatedRemote(),
      GetPrefs(browser_context_)->GetBoolean(prefs::kPrintPreviewDisabled),
      !params_.selection_text.empty());
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

void RenderViewContextMenu::ExecRouteMedia() {
  media_router::MediaRouterDialogController* dialog_controller =
      media_router::MediaRouterDialogController::GetOrCreateForWebContents(
          embedder_web_contents_);
  if (!dialog_controller)
    return;

  dialog_controller->ShowMediaRouterDialog(
      media_router::MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU);
  media_router::MediaRouterMetrics::RecordMediaRouterDialogOrigin(
      media_router::MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU);
}

void RenderViewContextMenu::ExecTranslate() {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  if (!chrome_translate_client)
    return;

  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  DCHECK(manager);
  manager->ShowTranslateUI(/*auto_translate=*/true,
                           /*triggered_from_menu=*/true);
}

void RenderViewContextMenu::ExecLanguageSettings(int event_flags) {
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
  OpenURL(url, GURL(), disposition, ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecProtocolHandlerSettings(int event_flags) {
  base::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.ContextMenu_Settings"));
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  GURL url = chrome::GetSettingsUrl(chrome::kHandlerSettingsSubPage);
  OpenURL(url, GURL(), disposition, ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecPictureInPicture() {
  if (!base::FeatureList::IsEnabled(media::kPictureInPicture))
    return;

  bool picture_in_picture_active =
      IsCommandIdChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE);

  if (picture_in_picture_active) {
    base::RecordAction(
        UserMetricsAction("MediaContextMenu_ExitPictureInPicture"));
  } else {
    base::RecordAction(
        UserMetricsAction("MediaContextMenu_EnterPictureInPicture"));
  }

  MediaPlayerActionAt(
      gfx::Point(params_.x, params_.y),
      blink::mojom::MediaPlayerAction(
          blink::mojom::MediaPlayerActionType::kPictureInPicture,
          !picture_in_picture_active));
}

void RenderViewContextMenu::MediaPlayerActionAt(
    const gfx::Point& location,
    const blink::mojom::MediaPlayerAction& action) {
  RenderFrameHost* frame_host = GetRenderFrameHost();
  if (frame_host)
    frame_host->ExecuteMediaPlayerActionAtLocation(location, action);
}

void RenderViewContextMenu::PluginActionAt(
    const gfx::Point& location,
    blink::mojom::PluginActionType plugin_action) {
  source_web_contents_->GetMainFrame()
      ->GetRenderViewHost()
      ->ExecutePluginActionAtLocation(location, plugin_action);
}

Browser* RenderViewContextMenu::GetBrowser() const {
  return chrome::FindBrowserWithWebContents(embedder_web_contents_);
}
