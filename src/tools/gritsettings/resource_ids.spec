# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file is used to assign starting resource ids for resources and strings
# used by Chromium.  This is done to ensure that resource ids are unique
# across all the grd files.  If you are adding a new grd file, please add
# a new entry to this file.
#
# The entries below are organized into sections. When adding new entries,
# please use the right section. Try to keep entries in alphabetical order.
#
# - chrome/app/
# - chrome/browser/
# - chrome/ WebUI
# - chrome/ miscellaneous
# - chromeos/
# - components/
# - ios/ (overlaps with chrome/)
# - content/
# - ios/web/ (overlaps with content/)
# - everything else
#
# The range of ID values, which is used by pak files, is from 0 to 2^16 - 1.
#
# IMPORTANT: Update instructions:
# * If adding items, manually assign draft start IDs so that numerical order is
#   preserved. Usually it suffices to +1 from previous tag.
#   * If updating items with repeated, be sure to add / update
#     "META": {"join": <duplicate count>},
#     for the item following duplicates. Be sure to look for duplicates that
#     may appear earlier than those that immediately precede the item.
{
  # The first entry in the file, SRCDIR, is special: It is a relative path from
  # this file to the base of your checkout.
  "SRCDIR": "../..",

  # START chrome/app section.
  # Previous versions of this file started with resource id 400, so stick with
  # that.
  #
  # chrome/ and ios/chrome/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "chrome/app/chromium_strings.grd": {
    "messages": [400],
  },
  "chrome/app/google_chrome_strings.grd": {
    "messages": [400],
  },

  # Leave lots of space for generated_resources since it has most of our
  # strings.
  "chrome/app/generated_resources.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"join": 2, "align": 200},
    "messages": [600],
  },

  "chrome/app/resources/locale_settings.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 1000},
    "messages": [1000],
  },

  # These each start with the same resource id because we only use one
  # file for each build (chromiumos, google_chromeos, linux, mac, or win).
  "chrome/app/resources/locale_settings_chromiumos.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 100},
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_google_chromeos.grd": {
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_linux.grd": {
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_mac.grd": {
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_win.grd": {
    "messages": [1100],
  },

  "chrome/app/theme/chrome_unscaled_resources.grd": {
    "META": {"join": 5},
    "includes": [1120],
  },

  # Leave space for theme_resources since it has many structures.
  "chrome/app/theme/theme_resources.grd": {
    "structures": [1140],
  },
  # END chrome/app section.

  # START chrome/browser section.
  "chrome/browser/dev_ui_browser_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [1200],
  },
  "chrome/browser/browser_resources.grd": {
    "includes": [1220],
    "structures": [1240],
  },
  "chrome/browser/resources/feedback_webui/feedback_resources.grd": {
    "includes": [1250],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/app_service_internals/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [1251],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/bookmarks/bookmarks_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [1260],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/browser_switch/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [1355],
  },
    "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/enterprise_casting/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [1364],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/emoji_picker/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [1365],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/launcher_internals/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [1366],
  },
  "chrome/browser/resources/chromeos/login/oobe_resources.grd": {
    "META": {"sizes": {"includes": [150], "structures": [300]}},
    "includes": [1367],
    "structures": [1368],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/login/oobe_modulized_resources.grd": {
    "META": {"sizes": {"includes": [150]}},
    "includes": [1369],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/multidevice_internals/resources.grd": {
    "META": {"sizes": {"includes": [35]}},
    "includes": [1370],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/multidevice_setup/multidevice_setup_resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [1380],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/commander/commander_resources.grd": {
    "META": {"sizes": {"includes": [15]}},
    "includes": [1405],
  },
  "chrome/browser/resources/component_extension_resources.grd": {
    "includes": [1420],
    "structures": [1440],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/downloads/downloads_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [1480],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/extensions/extensions_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [1540],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/history/history_resources.grd": {
    "META": {"sizes": {"includes": [40]}},
    "includes": [1580],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [1590],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/management/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [1610],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/new_tab_page_instant/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [1620],
  },
   "chrome/browser/resources/webid/webid_resources.grd": {
    "includes": [1622],
    "structures": [1626],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/nearby_internals/nearby_internals_resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [1630],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/nearby_share/nearby_share_dialog_resources.grd": {
    "META": {"sizes": {"includes": [100]}},
    "includes": [1640],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/media_router/media_router_feedback_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [1650],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/new_tab_page/resources.grd": {
    "META": {"sizes": {"includes": [200]}},
    "includes": [1670],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/new_tab_page_third_party/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [1695],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/ntp4/apps_resources.grd": {
    "META": {"sizes": {"includes": [40]}},
    "includes": [1705],
  },
  "chrome/browser/resources/preinstalled_web_apps/resources.grd": {
    "includes": [1710],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/pdf/resources.grd": {
    "META": {"sizes": {"includes": [200]}},
    "includes": [1715],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/print_preview/print_preview_resources.grd": {
    "META": {"sizes": {"includes": [500],}},
    "includes": [1720],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/read_later/read_later_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [1760],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/settings/chromeos/os_settings_resources.grd": {
    "META": {"sizes": {"includes": [1000],}},
    "includes": [1770],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/settings/settings_resources.grd": {
    "META": {"sizes": {"includes": [500],}},
    "includes": [1830],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/signin/profile_picker/profile_picker_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [1850],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/signin/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [1860],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/tab_search/tab_search_resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [1880],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/tab_strip/tab_strip_resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [1920],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/welcome/welcome_resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [1940],
  },
  "chrome/browser/supervised_user/supervised_user_unscaled_resources.grd": {
    "includes": [1970],
  },
  "chrome/browser/test_dummy/internal/android/resources/resources.grd": {
    "includes": [1980],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/download_shelf/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [1990],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/whats_new/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [2000],
  },
  # END chrome/browser section.

  # START chrome/ WebUI resources section
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/federated_learning/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [2010],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/bluetooth_internals/resources.grd": {
    "META": {"sizes": {"includes": [30],}},
    "includes": [2020],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/audio/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [2025],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/bluetooth_pairing_dialog/bluetooth_pairing_dialog_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2030],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/chromebox_for_meetings/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [2035],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/internet_config_dialog/internet_config_dialog_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2040],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/internet_detail_dialog/internet_detail_dialog_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2050],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/network_ui/network_ui_resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [2065],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/download/resources/download_internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [2070],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/gaia_auth_host/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2080],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/invalidations/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2090],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/media/webrtc_logs_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2100],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/net_internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [2120],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/omnibox/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [2140],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/quota_internals/quota_internals_resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [2160],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/sync_file_system_internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [2180],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/usb_internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [2200],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/webapks/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [2220],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/webui_js_error/webui_js_error_resources.grd": {
   "META": {"sizes": {"includes": [10],}},
   "includes": [2230],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/sync/driver/resources/resources.grd": {
   "META": {"sizes": {"includes": [30],}},
    "includes": [2240],
  },
  "components/resources/dev_ui_components_resources.grd": {
    "includes": [2260],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/media/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2270],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/webrtc/resources/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [2280],
  },
  "content/dev_ui_content_resources.grd": {
    "includes": [2300],
  },
  # END chrome/ WebUI resources section

  # START chrome/ miscellaneous section.
  "chrome/common/common_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [2320],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/common/chromeos/extensions/chromeos_system_extensions_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2330],
  },
  "chrome/credential_provider/gaiacp/gaia_resources.grd": {
    "includes": [2340],
    "messages": [2360],
  },
  "chrome/renderer/resources/renderer_resources.grd": {
    "includes": [2380],
    "structures": [2400],
  },
  "chrome/test/data/webui_test_resources.grd": {
    "includes": [2420],
  },
  "chrome/test/data/chrome_test_resources.grd": {
    "messages": [2440],
  },
  # END chrome/ miscellaneous section.

  # START chromeos/ section.
  "chromeos/chromeos_strings.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "messages": [2500],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chromeos/components/camera_app_ui/chromeos_camera_app_resources.grd": {
    "META": {"sizes": {"includes": [300],}},
    "includes": [2505],
  },
  "chromeos/components/camera_app_ui/resources/strings/camera_strings.grd": {
    "messages": [2515],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chromeos/components/connectivity_diagnostics/resources/connectivity_diagnostics_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2516],
  },
  "ash/webui/diagnostics_ui/resources/diagnostics_app_resources.grd": {
    "includes": [2517],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/file_manager/resources/file_manager_swa_resources.grd": {
    "META": {"sizes": {"includes": [100]}},
    "includes": [2519],
  },
  "chromeos/components/help_app_ui/resources/help_app_resources.grd": {
    "includes": [2520],
  },
  # Both help_app_kids_magazine_bundle_resources.grd and
  # help_app_kids_magazine_bundle_mock_resources.grd start with the same id
  # because only one of them is built depending on if src_internal is available.
  # Lower bound for number of resource ids is the number of files, which is 3 in
  # in this case (HTML, JS and CSS file).
  "chromeos/components/help_app_ui/resources/prod/help_app_kids_magazine_bundle_resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [2530],
  },
  "chromeos/components/help_app_ui/resources/mock/help_app_kids_magazine_bundle_mock_resources.grd": {
    "includes": [2530],
  },
  # Both help_app_bundle_resources.grd and help_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available. Lower bound is that we bundle ~100 images for
  # offline articles with the app, as well as strings in every language (74),
  # and bundled content in the top 25 languages (25 x 2).
  "chromeos/components/help_app_ui/resources/prod/help_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [300],}},  # Relies on src-internal.
    "includes": [2540],
  },
  "chromeos/components/help_app_ui/resources/mock/help_app_bundle_mock_resources.grd": {
    "includes": [2540],
  },
  "chromeos/components/media_app_ui/resources/media_app_resources.grd": {
    "META": {"join": 2},
    "includes": [2560],
  },
  # Both media_app_bundle_resources.grd and media_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available. Lower bound for number of resource ids is number
  # of languages (74).
  "chromeos/components/media_app_ui/resources/prod/media_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [120],}},  # Relies on src-internal.
    "includes": [2580],
  },
  "chromeos/components/media_app_ui/resources/mock/media_app_bundle_mock_resources.grd": {
    "includes": [2580],
  },
  "chromeos/components/print_management/resources/print_management_resources.grd": {
    "META": {"join": 2},
    "includes": [2600],
    "structures": [2620],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/sample_system_web_app_ui/ash_sample_system_web_app_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2640],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/sample_system_web_app_ui/ash_sample_system_web_app_untrusted_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2645],
  },
  "ash/webui/scanning/resources/scanning_app_resources.grd": {
    "includes": [2650],
    "structures": [2655],
  },
  "chromeos/components/telemetry_extension_ui/resources/telemetry_extension_resources.grd": {
    "includes": [2660],
  },
  "chromeos/resources/chromeos_resources.grd": {
    "includes": [2670],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chromeos/components/eche_app_ui/chromeos_eche_app_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2680],
  },
  # Both eche_bundle_resources.grd and eche_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available.
  "chromeos/components/eche_app_ui/resources/prod/eche_bundle_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2690],
  },
  "chromeos/components/eche_app_ui/resources/mock/eche_bundle_mock_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2690],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chromeos/components/personalization_app/resources/chromeos_personalization_app_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2695],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chromeos/components/demo_mode_app_ui/chromeos_demo_mode_app_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
   "includes": [2700],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chromeos/components/projector_app/resources/chromeos_projector_app_untrusted_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2705],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chromeos/components/projector_app/resources/chromeos_projector_app_trusted_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2710],
  },
  # END chromeos/ section.

  # START components/ section.
  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "components/components_chromium_strings.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "messages": [2700],
  },
  "components/components_google_chrome_strings.grd": {
    "messages": [2700],
  },

  "components/components_locale_settings.grd": {
    "META": {"join": 2},
    "includes": [2720],
    "messages": [2740],
  },
  "components/components_strings.grd": {
    "messages": [2760],
  },
  "components/omnibox/resources/omnibox_pedal_synonyms.grd": {
    "messages": [2770],
  },
  "components/omnibox/resources/omnibox_resources.grd": {
    "includes": [2780],
  },
  "components/policy/resources/policy_templates.grd": {
    "structures": [2800],
  },
  "components/resources/components_resources.grd": {
    "includes": [2820],
  },
  "components/resources/components_scaled_resources.grd": {
    "structures": [2840],
  },
  "components/embedder_support/android/java/strings/web_contents_delegate_android_strings.grd": {
    "messages": [2860],
  },
  "components/autofill/core/browser/autofill_address_rewriter_resources.grd":{
    "includes": [2880]
  },
  # END components/ section.

  # START ios/ section.
  #
  # chrome/ and ios/chrome/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  "ios/chrome/app/resources/ios_resources.grd": {
    "includes": [400],
    "structures": [420],
  },

  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "ios/chrome/app/strings/ios_chromium_strings.grd": {
    # Big alignment to make start IDs look nicer.
    "META": {"align": 100},
    "messages": [500],
  },
  "ios/chrome/app/strings/ios_google_chrome_strings.grd": {
    "messages": [500],
  },

  "ios/chrome/app/strings/ios_strings.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"join": 2, "align": 200},
    "messages": [600],
  },
  "ios/chrome/app/theme/ios_theme_resources.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 100},
    "structures": [700],
  },
  "ios/chrome/share_extension/strings/ios_share_extension_strings.grd": {
    "messages": [720],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_strings.grd": {
    "messages": [740],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_chromium_strings.grd": {
    "messages": [760],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_google_chrome_strings.grd": {
    "messages": [760],
  },
  "ios/chrome/content_widget_extension/strings/ios_content_widget_extension_chromium_strings.grd": {
    "META": {"join": 2},
    "messages": [780],
  },
  "ios/chrome/content_widget_extension/strings/ios_content_widget_extension_google_chrome_strings.grd": {
    "messages": [780],
  },
  "ios/chrome/credential_provider_extension/strings/ios_credential_provider_extension_strings.grd": {
    "META": {"join": 2},
    "messages": [800],
  },
  "ios/chrome/widget_kit_extension/strings/ios_widget_kit_extension_strings.grd": {
    "messages": [820],
  },

  # END ios/ section.

  # START content/ section.
  # content/ and ios/web/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  "content/app/resources/content_resources.grd": {
    # Big alignment at start of section.
    "META": {"join": 2, "align": 100},
    "structures": [2900],
  },
  "content/content_resources.grd": {
    "includes": [2920],
  },
  "content/shell/shell_resources.grd": {
    "includes": [2940],
  },
  "content/test/web_ui_mojo_test_resources.grd": {
    "includes": [2950],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [2960],
  },
  # END content/ section.

  # START ios/web/ section.
  # content/ and ios/web/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  "ios/web/ios_web_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [2900],
  },
  "ios/web/test/test_resources.grd": {
    "includes": [2920],
  },
  # END ios/web/ section.

  # START "everything else" section.
  # Everything but chrome/, chromeos/, components/, content/, and ios/
  "android_webview/ui/aw_resources.grd": {
    # Big alignment at start of section.
    "META": {"join": 2, "align": 100},
    "includes": [3000],
  },
  "android_webview/ui/aw_strings.grd": {
    "messages": [3020],
  },

  "ash/app_list/resources/app_list_resources.grd": {
    "structures": [3040],
  },
  "ash/ash_strings.grd": {
    "messages": [3060],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/os_feedback_ui/resources/ash_os_feedback_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3064],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/shortcut_customization_ui/resources/ash_shortcut_customization_app_resources.grd": {
    "META": {"sizes": {"includes": [200],}},
    "includes": [3070],
  },
  "ash/shortcut_viewer/shortcut_viewer_strings.grd": {
    "messages": [3080],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/shimless_rma/resources/ash_shimless_rma_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3090],
  },
  "ash/keyboard/ui/keyboard_resources.grd": {
    "includes": [3100],
  },
  "ash/login/resources/login_resources.grd": {
    "structures": [3120],
  },
  "ash/public/cpp/resources/ash_public_unscaled_resources.grd": {
    "includes": [3140],
  },
  "base/tracing/protos/resources.grd": {
    "includes": [3150],
  },
  "chromecast/renderer/resources/extensions_renderer_resources.grd": {
    "includes": [3160],
  },

  "cloud_print/virtual_driver/win/install/virtual_driver_setup_resources.grd": {
    "includes": [3180],
    "messages": [3200],
  },

  "device/bluetooth/bluetooth_strings.grd": {
    "messages": [3220],
  },

  "device/fido/fido_strings.grd": {
    "messages": [3240],
  },

  "extensions/browser/resources/extensions_browser_resources.grd": {
    "structures": [3260],
  },
  "extensions/extensions_resources.grd": {
    "includes": [3280],
  },
  "extensions/renderer/resources/extensions_renderer_resources.grd": {
    "includes": [3300],
    "structures": [3320],
  },
  "extensions/shell/app_shell_resources.grd": {
    "includes": [3340],
  },
  "extensions/strings/extensions_strings.grd": {
    "messages": [3360],
  },

  "headless/lib/resources/headless_lib_resources.grd": {
    "includes": [3380],
  },

  "mojo/public/js/mojo_bindings_resources.grd": {
    "includes": [3400],
  },

  "net/base/net_resources.grd": {
    "includes": [3420],
  },

  "remoting/resources/remoting_strings.grd": {
    "messages": [3440],
  },

  "services/services_strings.grd": {
    "messages": [3460],
  },
  "skia/skia_resources.grd": {
    "includes": [3470],
  },
  "third_party/blink/public/blink_image_resources.grd": {
    "structures": [3480],
  },
  "third_party/blink/public/blink_resources.grd": {
    "includes": [3500],
  },
  "third_party/blink/renderer/modules/media_controls/resources/media_controls_resources.grd": {
    "includes": [3520],
    "structures": [3540],
  },
  "third_party/blink/public/strings/blink_strings.grd": {
    "messages": [3560],
  },
  "third_party/libaddressinput/chromium/address_input_strings.grd": {
    "messages": [3600],
  },

  "ui/base/test/ui_base_test_resources.grd": {
    "messages": [3620],
  },
  "ui/chromeos/resources/ui_chromeos_resources.grd": {
    "structures": [3640],
  },
  "ui/chromeos/ui_chromeos_strings.grd": {
    "messages": [3660],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ui/file_manager/file_manager_gen_resources.grd": {
    "META": {"sizes": {"includes": [2000]}},
    "includes": [3670],
  },
  "ui/file_manager/file_manager_resources.grd": {
    "includes": [3680],
  },
  "ui/resources/ui_resources.grd": {
    "structures": [3700],
  },
  "ui/resources/ui_unscaled_resources.grd": {
    "includes": [3720],
  },
  "ui/strings/app_locale_settings.grd": {
    "messages": [3740],
  },
  "ui/strings/ui_strings.grd": {
    "messages": [3760],
  },
  "ui/views/examples/views_examples_resources.grd": {
    "messages": [3770],
  },
  "ui/views/resources/views_resources.grd": {
    "structures": [3780],
  },
  "ui/webui/resources/webui_resources.grd": {
    "includes": [3800],
    "structures": [3820],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ui/webui/resources/webui_generated_resources.grd": {
    "META": {"sizes": {"includes": [800]}},
    "includes": [3830],
  },
  "weblayer/weblayer_resources.grd": {
    "includes": [3840],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/third_party/devtools-frontend/src/front_end/devtools_resources.grd": {
    # In debug build, devtools frontend sources are not bundled and therefore
    # includes a lot of individual resources
    "META": {"sizes": {"includes": [2000],}},
    "includes": [3860],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/resources/inspector_overlay/inspector_overlay_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3880],
  },

  # END "everything else" section.
  # Everything but chrome/, components/, content/, and ios/

  # Thinking about appending to the end?
  # Please read the header and find the right section above instead.

  # Resource ids starting at 31000 are reserved for projects built on Chromium.
}
