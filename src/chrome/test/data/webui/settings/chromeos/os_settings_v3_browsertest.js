// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/browser/ash/crostini/fake_crostini_features.h"');
GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "chrome/common/buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/app_restore/features.h"');
GEN('#include "content/public/test/browser_test.h"');

/* eslint-disable no-var */

/** Test fixture for shared Polymer 3 elements. */
var OSSettingsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kEnableHostnameSetting',
      ],
    };
  }
};

var OSSettingsDevicePageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/device_page_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kAllowDisableTouchpadHapticFeedback',
        'features::kAllowTouchpadHapticClickSettings',
      ],
    };
  }
};

TEST_F(
    'OSSettingsDevicePageV3Test', 'All',
    () => mocha.grep('/^((?!arrow_key_arrangement_disabled).)*$/').run());

// TODO(crbug.com/1275568): move this to the generic test lists below after the
// feature is launched.
var OSSettingsPeoplePageAccountManagerV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/people_page_account_manager_test.js';
  }

  /** @override */
  get featureList() {
    return {
      disabled: [
        'chromeos::features::kArcAccountRestrictions',
        'chromeos::features::kLacrosSupport'
      ]
    };
  }
};

TEST_F('OSSettingsPeoplePageAccountManagerV3Test', 'All', () => mocha.run());

var OSSettingsPeoplePageAccountManagerWithArcAccountRestrictionsEnabledV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/people_page_account_manager_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kArcAccountRestrictions',
        'chromeos::features::kLacrosSupport'
      ]
    };
  }
};

TEST_F(
    'OSSettingsPeoplePageAccountManagerWithArcAccountRestrictionsEnabledV3Test',
    'All', () => mocha.run());

var OSSettingsNearbyShareSubPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/nearby_share_subpage_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(['features::kNearbySharing'])
    };
  }
};

TEST_F('OSSettingsNearbyShareSubPageV3Test', 'All', () => mocha.run());

// eslint-disable-next-line no-var
var OSSettingsPeoplePageOsSyncV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_sync_controls_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(
          ['chromeos::features::kSyncSettingsCategorization'])
    };
  }
};

TEST_F('OSSettingsPeoplePageOsSyncV3Test', 'AllJsTests', () => {
  mocha.run();
});

// TODO(crbug.com/1234871) Move this test back into the list of tests below once
// Bluetooth revamp is launched.
var OSSettingsOsSettingsPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_settings_page_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled:
          super.featureList.enabled.concat(['ash::features::kBluetoothRevamp'])
    };
  }
};

TEST_F('OSSettingsOsSettingsPageV3Test', 'AllJsTests', () => {
  mocha.run();
});

// TODO(crbug.com/1234871) Move this test back into the list of tests below once
// Bluetooth revamp is launched.
var OSSettingsOsBluetoothDevicesSubpageV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_bluetooth_devices_subpage_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kBluetoothRevamp',
        'ash::features::kFastPair',
        'ash::features::kFastPairSoftwareScanning',
      ])
    };
  }
};

TEST_F('OSSettingsOsBluetoothDevicesSubpageV3Test', 'AllJsTests', () => {
  mocha.run();
});

// TODO(crbug.com/1234871) Move this test back into the list of tests below once
// Bluetooth revamp is launched.
var OSSettingsOsPairedBluetoothListItemV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_paired_bluetooth_list_item_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled:
          super.featureList.enabled.concat(['ash::features::kBluetoothRevamp'])
    };
  }
};

TEST_F('OSSettingsOsPairedBluetoothListItemV3Test', 'AllJsTests', () => {
  mocha.run();
});

// TODO(crbug.com/1237598) Move this test back into the list of tests below once
// Bluetooth revamp is launched.
var OSSettingsOsBluetoothDeviceDetailSubpageV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_bluetooth_device_detail_subpage_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled:
          super.featureList.enabled.concat(['ash::features::kBluetoothRevamp'])
    };
  }
};

TEST_F('OSSettingsOsBluetoothDeviceDetailSubpageV3Test', 'AllJsTests', () => {
  mocha.run();
});

// TODO(crbug.com/1237598) Move this test back into the list of tests below once
// Bluetooth revamp is launched.
var OSSettingsOsBluetoothTrueWirelessImagesV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_bluetooth_true_wireless_images_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled:
          super.featureList.enabled.concat(['ash::features::kBluetoothRevamp'])
    };
  }
};

TEST_F('OSSettingsOsBluetoothTrueWirelessImagesV3Test', 'AllJsTests', () => {
  mocha.run();
});


var OSSettingsSearchEngineV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/search_engine_test.js';
  }

  /** @override */
  get featureList() {
    return {disabled: ['chromeos::features::kSyncSettingsCategorization']};
  }
};

TEST_F('OSSettingsSearchEngineV3Test', 'AllJsTests', () => {
  mocha.run();
});

var OSSettingsAppManagementAppDetailsV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/app_management/app_details_item_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(
          ['features::kAppManagementAppDetails'])
    };
  }
};

function crostiniTestGenPreamble() {
  GEN('crostini::FakeCrostiniFeatures fake_crostini_features;');
  GEN('fake_crostini_features.SetAll(true);');
}

TEST_F('OSSettingsAppManagementAppDetailsV3Test', 'AllJsTests', () => {
  mocha.run();
});

var OSSettingsCrostiniPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/crostini_page_test.js';
  }

  /** @override */
  testGenPreamble() {
    return crostiniTestGenPreamble();
  }
};

TEST_F('OSSettingsCrostiniPageV3Test', 'AllJsTests', () => {
  mocha.run();
});

var OSSettingsCrostiniExtraContainerPageV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/crostini_extra_containers_subpage_test.js';
  }

  /** @override */
  testGenPreamble() {
    return crostiniTestGenPreamble();
  }
};

TEST_F('OSSettingsCrostiniExtraContainerPageV3Test', 'AllJsTests', () => {
  mocha.run();
});

[['AccessibilityPage', 'os_a11y_page_tests.js'],
 ['AboutPage', 'os_about_page_tests.js'],
 ['AccountsPage', 'add_users_tests.js'],
 ['AmbientModePage', 'ambient_mode_page_test.js'],
 ['AmbientModePhotosPage', 'ambient_mode_photos_page_test.js'],
 ['AppsPage', 'apps_page_test.js'],
 ['AppNotificationsSubpage', 'app_notifications_subpage_tests.js'],
 ['AppManagementAppDetailsItem', 'app_management/app_details_item_test.js'],
 ['AppManagementAppDetailView', 'app_management/app_detail_view_test.js'],
 ['AppManagementAppItem', 'app_management/app_item_test.js'],
 ['AppManagementArcDetailView', 'app_management/arc_detail_view_test.js'],
 [
   'AppManagementBorealisDetailView',
   'app_management/borealis_detail_view_test.js'
 ],
 [
   'AppManagementChromeAppDetailView',
   'app_management/chrome_app_detail_view_test.js'
 ],
 ['AppManagementDomSwitch', 'app_management/dom_switch_test.js'],
 ['AppManagementFileHandlingItem', 'app_management/file_handling_item_test.js'],
 ['AppManagementMainView', 'app_management/main_view_test.js'],
 ['AppManagementManagedApp', 'app_management/managed_apps_test.js'],
 ['AppManagementPage', 'app_management/app_management_page_tests.js'],
 ['AppManagementPinToShelfItem', 'app_management/pin_to_shelf_item_test.js'],
 [
   'AppManagementPluginVmDetailView',
   'app_management/plugin_vm_detail_view_test.js'
 ],
 ['AppManagementPwaDetailView', 'app_management/pwa_detail_view_test.js'],
 ['AppManagementReducers', 'app_management/reducers_test.js'],
 ['AppManagementResizeLockItem', 'app_management/resize_lock_item_test.js'],
 [
   'AppManagementSupportedLinksItem',
   'app_management/supported_links_item_test.js'
 ],
 ['AppManagementToggleRow', 'app_management/toggle_row_test.js'],
 ['AppManagementUninstallButton', 'app_management/uninstall_button_test.js'],
 ['BluetoothPage', 'bluetooth_page_tests.js'],
 ['CellularNetworksList', 'cellular_networks_list_test.js'],
 ['CellularRoamingToggleButton', 'cellular_roaming_toggle_button_test.js'],
 ['CellularSetupDialog', 'cellular_setup_dialog_test.js'],
 [
   'DictationChangeLanguageLocaleDialogTest',
   'change_dictation_locale_dialog_test.js'
 ],
 ['CupsPrinterEntry', 'cups_printer_entry_tests.js'],
 ['CupsPrinterLandingPage', 'cups_printer_landing_page_tests.js'],
 // TODO(crbug/1240970): Re-enable once flakiness is fixed.
 // ['CupsPrinterPage', 'cups_printer_page_tests.js'],
 ['DarkModeSubpage', 'dark_mode_subpage_tests.js'],
 ['DateTimePage', 'date_time_page_tests.js'],
 ['EsimInstallErrorDialog', 'esim_install_error_dialog_test.js'],
 ['EsimRemoveProfileDialog', 'esim_remove_profile_dialog_test.js'],
 ['EsimRenameDialog', 'esim_rename_dialog_test.js'],
 ['FilesPage', 'os_files_page_test.js'],
 ['FingerprintPage', 'fingerprint_browsertest_chromeos.js'],
 ['GoogleAssistantPage', 'google_assistant_page_test.js'],
 ['GuestOsSharedPaths', 'guest_os_shared_paths_test.js'],
 ['GuestOsSharedUsbDevices', 'guest_os_shared_usb_devices_test.js'],
 ['InputMethodOptionPage', 'input_method_options_page_test.js'],
 ['InputPage', 'input_page_test.js'],
 ['InternetConfig', 'internet_config_test.js'],
 ['InternetDetailMenu', 'internet_detail_menu_test.js'],
 ['InternetDetailPage', 'internet_detail_page_tests.js'],
 ['InternetKnownNetworksPage', 'internet_known_networks_page_tests.js'],
 ['InternetSubpage', 'internet_subpage_tests.js'],
 ['InternetPage', 'internet_page_tests.js'],
 ['KerberosAccounts', 'kerberos_accounts_test.js'],
 ['KerberosPage', 'kerberos_page_test.js'],
 ['KeyboardShortcutBanner', 'keyboard_shortcut_banner_test.js'],
 ['LockScreenPage', 'lock_screen_tests.js'],
 ['ManageAccessibilityPage', 'manage_accessibility_page_tests.js'],
 ['MultideviceCombinedSetupItem', 'multidevice_combined_setup_item_tests.js'],
 // TODO(crbug.com/1227116): Re-enable once flakiness is fixed.
 //  ['MultideviceFeatureItem', 'multidevice_feature_item_tests.js'],
 ['MultideviceFeatureToggle', 'multidevice_feature_toggle_tests.js'],
 [
   'MultideviceNotificationAccessSetupDialog',
   'multidevice_notification_access_setup_dialog_tests.js'
 ],
 ['MultidevicePage', 'multidevice_page_tests.js'],
 [
   'MultidevicePermissionsSetupDialog',
   'multidevice_permissions_setup_dialog_tests.js'
 ],
 ['MultideviceSmartLockItem', 'multidevice_smartlock_item_test.js'],
 ['MultideviceSmartLockSubPage', 'multidevice_smartlock_subpage_test.js'],
 ['MultideviceSubPage', 'multidevice_subpage_tests.js'],
 [
   'MultideviceTaskContinuationItem',
   'multidevice_task_continuation_item_tests.js'
 ],
 [
   'MultideviceTaskContinuationDisabledLink',
   'multidevice_task_continuation_disabled_link_tests.js'
 ],
 [
   'MultideviceWifiSyncDisabledLink',
   'multidevice_wifi_sync_disabled_link_tests.js'
 ],
 ['MultideviceWifiSyncItem', 'multidevice_wifi_sync_item_tests.js'],
 ['NearbyShareConfirmPage', 'nearby_share_confirm_page_test.js'],
 ['NearbyShareReceiveDialog', 'nearby_share_receive_dialog_tests.js'],
 ['NetworkAlwaysOnVpn', 'network_always_on_vpn_test.js'],
 ['NetworkProxySection', 'network_proxy_section_test.js'],
 ['NetworkSummary', 'network_summary_test.js'],
 ['NetworkSummaryItem', 'network_summary_item_test.js'],
 ['OncMojoTest', 'onc_mojo_test.js'],
 ['OsBluetoothPage', 'os_bluetooth_page_tests.js'],
 ['OsBluetoothPairingDialog', 'os_bluetooth_pairing_dialog_tests.js'],
 ['OsBluetoothSummary', 'os_bluetooth_summary_tests.js'],
 [
   'OsBluetoothChangeDeviceNameDialog',
   'os_bluetooth_change_device_name_dialog_tests.js'
 ],
 ['OsEditDictionaryPage', 'os_edit_dictionary_page_test.js'],
 ['OsLanguagesPageV2', 'os_languages_page_v2_tests.js'],
 ['OsPairedBluetoothList', 'os_paired_bluetooth_list_tests.js'],
 ['OsSettingsUi', 'os_settings_ui_test.js'],
 ['OsSettingsUi2', 'os_settings_ui_test_2.js'],
 ['OsSettingsMain', 'os_settings_main_test.js'],
 ['OsSearchPage', 'os_search_page_test.js'],
 ['OsSettingsSearchBox', 'os_settings_search_box_test.js'],
 ['OSSettingsMenu', 'os_settings_menu_test.js'],
 ['ParentalControlsPage', 'parental_controls_page_test.js'],
 ['PeoplePage', 'os_people_page_test.js'],
 ['PeoplePageChangePicture', 'people_page_change_picture_test.js'],
 ['PeoplePageQuickUnlock', 'quick_unlock_authenticate_browsertest_chromeos.js'],
 ['PersonalizationPage', 'personalization_page_test.js'],
 ['PrintingPage', 'os_printing_page_tests.js'],
 ['PrivacyPage', 'os_privacy_page_test.js'],
 ['ResetPage', 'os_reset_page_test.js'],
 ['SettingsSchedulerSlider', 'settings_scheduler_slider_test.js'],
 ['SearchSubpage', 'search_subpage_test.js'],
 ['SettingsTrafficCounters', 'settings_traffic_counters_test.js'],
 ['SmartInputsPage', 'smart_inputs_page_test.js'],
 ['SmbPage', 'smb_shares_page_tests.js'],
 ['SmartPrivacySubpage', 'smart_privacy_subpage_tests.js'],
 [
   'SwitchAccessActionAssignmentDialog',
   'switch_access_action_assignment_dialog_test.js'
 ],
 ['SwitchAccessSetupGuideDialog', 'switch_access_setup_guide_dialog_test.js'],
 ['SwitchAccessSubpage', 'switch_access_subpage_tests.js'],
 ['TetherConnectionDialog', 'tether_connection_dialog_test.js'],
 ['TextToSpeechSubpage', 'text_to_speech_subpage_tests.js'],
 ['TimezoneSelector', 'timezone_selector_test.js'],
 ['TimezoneSubpage', 'timezone_subpage_test.js'],
 ['TtsSubpage', 'tts_subpage_test.js'],
 ['UserPage', 'user_page_tests.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `OSSettings${testName}V3Test`;
  this[className] = class extends OSSettingsV3BrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/${
          module}`;
    }
  };

  // AboutPage has a test suite that can only succeed on official builds where
  // the is_chrome_branded build flag is enabled.
  if (testName === 'AboutPage') {
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!AboutPageTest_OfficialBuild).*$/').run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('AboutPageTest_OfficialBuild').run();
    });
    GEN('#endif');
  } else if (testName === 'PrivacyPage') {
    // PrivacyPage has a test suite that can only succeed on official builds
    // where the is_chrome_branded build flag is enabled.
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!PrivacePageTest_OfficialBuild).*$/').run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('PrivacePageTest_OfficialBuild').run();
    });
    GEN('#endif');
  } else {
    TEST_F(className, caseName || 'All', () => mocha.run());
  }
}
