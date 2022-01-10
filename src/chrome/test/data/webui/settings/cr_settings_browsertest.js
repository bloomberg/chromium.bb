// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "build/build_config.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/autofill/core/common/autofill_features.h"');
GEN('#include "content/public/test/browser_test.h"');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)');
GEN('#include "components/language/core/common/language_experiments.h"');
GEN('#endif');

/* eslint-disable no-var */

/** Test fixture for shared Polymer 3 elements. */
var CrSettingsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings';
  }

  /** @override */
  get featureList() {
    if (!this.featureListInternal.enabled &&
        !this.featureListInternal.disabled) {
      return null;
    }
    return this.featureListInternal;
  }

  /** @return {!{enabled: !Array<string>, disabled: !Array<string>}} */
  get featureListInternal() {
    return {};
  }
};

var CrSettingsAboutPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/about_page_tests.js&host=webui-test';
  }
};

TEST_F('CrSettingsAboutPageTest', 'AboutPage', function() {
  mocha.grep('AboutPageTest_AllBuilds').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsAboutPageTest', 'AboutPage_OfficialBuild', function() {
  mocha.grep('AboutPageTest_OfficialBuilds').run();
});
GEN('#endif');

var CrSettingsAvatarIconTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/avatar_icon_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsAvatarIconTest', 'All', function() {
  mocha.run();
});

var CrSettingsBasicPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/basic_page_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsBasicPageTest', 'All', function() {
  runMochaSuite('SettingsBasicPage');
});

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
var CrSettingsLanguagesPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_page_tests.js&host=webui-test';
  }
};

TEST_F('CrSettingsLanguagesPageTest', 'LanguageSettings', function() {
  mocha.grep(languages_page_tests.TestNames.LanguageSettings).run();
});

TEST_F('CrSettingsLanguagesPageTest', 'Spellcheck', function() {
  mocha.grep(languages_page_tests.TestNames.Spellcheck).run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsLanguagesPageTest', 'SpellcheckOfficialBuild', function() {
  mocha.grep(languages_page_tests.TestNames.SpellcheckOfficialBuild).run();
});
GEN('#endif');

GEN('#if !BUILDFLAG(IS_CHROMEOS_LACROS)');
var CrSettingsLanguagesPageRestructuredTest =
    class extends CrSettingsLanguagesPageTest {
  /** @override */
  get featureListInternal() {
    return {enabled: ['language::kDesktopRestructuredLanguageSettings']};
  }
};
TEST_F(
    'CrSettingsLanguagesPageRestructuredTest', 'RestructuredLanguageSettings',
    function() {
      mocha.grep(languages_page_tests.TestNames.RestructuredLanguageSettings)
          .run();
    });
GEN('#endif');

var CrSettingsLanguagesSubpageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_subpage_tests.js&host=webui-test';
  }
};

TEST_F('CrSettingsLanguagesSubpageTest', 'AddLanguagesDialog', function() {
  mocha.grep(languages_subpage_tests.TestNames.AddLanguagesDialog).run();
});

TEST_F('CrSettingsLanguagesSubpageTest', 'LanguageMenu', function() {
  mocha.grep(languages_subpage_tests.TestNames.LanguageMenu).run();
});

GEN('#if !BUILDFLAG(IS_CHROMEOS_LACROS)');
var CrSettingsLanguagesSubpageDetailedTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_subpage_details_tests.js&host=webui-test';
  }
};

TEST_F(
    'CrSettingsLanguagesSubpageDetailedTest', 'AlwaysTranslateDialog',
    function() {
      mocha
          .grep(languages_subpage_details_tests.TestNames.AlwaysTranslateDialog)
          .run();
    });

TEST_F(
    'CrSettingsLanguagesSubpageDetailedTest', 'NeverTranslateDialog',
    function() {
      mocha.grep(languages_subpage_details_tests.TestNames.NeverTranslateDialog)
          .run();
    });
GEN('#endif');

var CrSettingsLanguagesPageMetricsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_page_metrics_test_browser.js&host=webui-test';
  }
};

TEST_F(
    'CrSettingsLanguagesPageMetricsTest', 'LanguagesPageMetricsBrowser',
    function() {
      runMochaSuite('LanguagesPageMetricsBrowser');
    });

GEN('#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)');

var CrSettingsClearBrowsingDataTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/clear_browsing_data_test.js&host=webui-test';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kSearchHistoryLink']};
  }
};

// TODO(crbug.com/1107652): Flaky on Mac.
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_ClearBrowsingDataAllPlatforms DISABLED_ClearBrowsingDataAllPlatforms');
GEN('#else');
GEN('#define MAYBE_ClearBrowsingDataAllPlatforms ClearBrowsingDataAllPlatforms');
GEN('#endif');
TEST_F(
    'CrSettingsClearBrowsingDataTest', 'MAYBE_ClearBrowsingDataAllPlatforms',
    function() {
      runMochaSuite('ClearBrowsingDataAllPlatforms');
    });

TEST_F('CrSettingsClearBrowsingDataTest', 'InstalledApps', () => {
  runMochaSuite('InstalledApps');
});

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
TEST_F(
    'CrSettingsClearBrowsingDataTest', 'ClearBrowsingDataDesktop', function() {
      runMochaSuite('ClearBrowsingDataDesktop');
    });
GEN('#endif');


var CrSettingsMainPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_main_test.js&host=webui-test';
  }
};

// Copied from Polymer 2 version of tests:
// Times out on Windows Tests (dbg). See https://crbug.com/651296.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/667882
// Flaky everywhere crbug.com/1197768
TEST_F('CrSettingsMainPageTest', 'DISABLED_MainPage', function() {
  mocha.run();
});

var CrSettingsAutofillPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_page_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsAutofillPageTest', 'All', function() {
  mocha.run();
});

var CrSettingsAutofillSectionCompanyEnabledTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_section_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsAutofillSectionCompanyEnabledTest', 'All', function() {
  // Use 'EnableCompanyName' to inform tests that the feature is enabled.
  const loadTimeDataOverride = {};
  loadTimeDataOverride['EnableCompanyName'] = true;
  loadTimeDataOverride['showHonorific'] = true;
  loadTimeData.overrideValues(loadTimeDataOverride);
  mocha.run();
});

var CrSettingsPasswordsSectionTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_section_test.js&host=webui-test';
  }
};

// Flaky on Debug builds https://crbug.com/1090931
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrSettingsPasswordsSectionTest', 'MAYBE_All', function() {
  mocha.run();
});
GEN('#undef MAYBE_All');

var CrSettingsMultiStorePasswordUiEntryTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/multi_store_password_ui_entry_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsMultiStorePasswordUiEntryTest', 'All', function() {
  mocha.run();
});

var CrSettingsPasswordsDeviceSectionTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_device_section_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsPasswordsDeviceSectionTest', 'All', function() {
  mocha.run();
});

var CrSettingsPasswordEditDialogTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_edit_dialog_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsPasswordEditDialogTest', 'All', function() {
  mocha.run();
});

var CrSettingsMultiStoreExceptionEntryTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/multi_store_exception_entry_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsMultiStoreExceptionEntryTest', 'All', function() {
  mocha.run();
});

var CrSettingsPasswordsCheckTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_check_test.js&host=webui-test';
  }
};

// Flaky on Mac builds https://crbug.com/1143801
GEN('#if (defined(OS_MAC)) || (defined(OS_LINUX))');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrSettingsPasswordsCheckTest', 'MAYBE_All', function() {
  mocha.run();
});
GEN('#undef MAYBE_All');

var CrSettingsSafetyCheckPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_page_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsSafetyCheckPageTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');
var CrSettingsSafetyCheckChromeCleanerTest =
    class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_chrome_cleaner_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsSafetyCheckChromeCleanerTest', 'All', function() {
  mocha.run();
});
GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

var CrSettingsSiteListTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_list_tests.js&host=webui-test';
  }
};

// Copied from Polymer 2 test:
// TODO(crbug.com/929455): flaky, fix.
TEST_F('CrSettingsSiteListTest', 'DISABLED_SiteList', function() {
  runMochaSuite('SiteList');
});

// TODO(crbug.com/929455): When the bug is fixed, merge
// SiteListEmbargoedOrigin into SiteList
TEST_F('CrSettingsSiteListTest', 'SiteListEmbargoedOrigin', function() {
  runMochaSuite('SiteListEmbargoedOrigin');
});

TEST_F('CrSettingsSiteListTest', 'EditExceptionDialog', function() {
  runMochaSuite('EditExceptionDialog');
});

TEST_F('CrSettingsSiteListTest', 'AddExceptionDialog', function() {
  runMochaSuite('AddExceptionDialog');
});

var CrSettingsSiteDetailsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_details_tests.js&host=webui-test';
  }
};

// Disabling on debug due to flaky timeout on Win7 Tests (dbg)(1) bot.
// https://crbug.com/825304 - later for other platforms in crbug.com/1021219.
// Disabling on Linux CFI due to flaky timeout (crbug.com/1031960).
GEN('#if (!defined(NDEBUG)) || ((defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(IS_CFI))');
GEN('#define MAYBE_SiteDetails DISABLED_SiteDetails');
GEN('#else');
GEN('#define MAYBE_SiteDetails SiteDetails');
GEN('#endif');

TEST_F('CrSettingsSiteDetailsTest', 'MAYBE_SiteDetails', function() {
  mocha.run();
});

var CrSettingsPersonalizationOptionsTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/personalization_options_test.js&host=webui-test';
  }
};

TEST_F('CrSettingsPersonalizationOptionsTest', 'AllBuilds', function() {
  runMochaSuite('PersonalizationOptionsTests_AllBuilds');
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsPersonalizationOptionsTest', 'OfficialBuild', function() {
  runMochaSuite('PersonalizationOptionsTests_OfficialBuild');
});
GEN('#endif');

var CrSettingsPrivacyPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_page_test.js&host=webui-test';
  }

  /** @override */
  get featureListInternal() {
    return {enabled: ['features::kPrivacyReview']};
  }
};

// TODO(crbug.com/1263420): Flaky on Linux Tests(dbg).
GEN('#if defined(OS_LINUX)');
GEN('#define MAYBE_PrivacyPageTests DISABLED_PrivacyPageTests');
GEN('#else');
GEN('#define MAYBE_PrivacyPageTests PrivacyPageTests');
GEN('#endif');
TEST_F('CrSettingsPrivacyPageTest', 'PrivacyPageTests', function() {
  runMochaSuite('PrivacyPage');
});

TEST_F('CrSettingsPrivacyPageTest', 'PrivacyReviewEnabled', function() {
  runMochaSuite('PrivacyReviewEnabled');
});

// TODO(crbug.com/1043665): flaky crash on Linux Tests (dbg).
TEST_F(
    'CrSettingsPrivacyPageTest', 'DISABLED_PrivacyPageSoundTests', function() {
      runMochaSuite('PrivacyPageSound');
    });

// TODO(crbug.com/1113912): flaky failure on multiple platforms
TEST_F(
    'CrSettingsPrivacyPageTest', 'DISABLED_HappinessTrackingSurveysTests',
    function() {
      runMochaSuite('HappinessTrackingSurveys');
    });

GEN('#if defined(OS_MAC) || defined(OS_WIN)');
// TODO(crbug.com/1043665): disabling due to failures on several builders.
TEST_F(
    'CrSettingsPrivacyPageTest', 'DISABLED_CertificateManagerTests',
    function() {
      runMochaSuite('NativeCertificateManager');
    });
GEN('#endif');

var CrSettingsPrivacyReviewPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_review_page_test.js&host=webui-test';
  }

  /** @override */
  get featureListInternal() {
    return {enabled: ['features::kPrivacyReview']};
  }
};

TEST_F('CrSettingsPrivacyReviewPageTest', 'PrivacyReviewPageTests', function() {
  runMochaSuite('PrivacyReviewPage');
});


TEST_F(
    'CrSettingsPrivacyReviewPageTest', 'HistorySyncFragmentTests', function() {
      runMochaSuite('HistorySyncFragment');
    });

var CrSettingsRouteTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/route_tests.js&host=webui-test';
  }
};

TEST_F('CrSettingsRouteTest', 'Basic', function() {
  runMochaSuite('route');
});

TEST_F('CrSettingsRouteTest', 'DynamicParameters', function() {
  runMochaSuite('DynamicParameters');
});

// Copied from Polymer 2 test:
// Failing on ChromiumOS dbg. https://crbug.com/709442
GEN('#if (defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)) && !defined(NDEBUG)');
GEN('#define MAYBE_NonExistentRoute DISABLED_NonExistentRoute');
GEN('#else');
GEN('#define MAYBE_NonExistentRoute NonExistentRoute');
GEN('#endif');

TEST_F('CrSettingsRouteTest', 'MAYBE_NonExistentRoute', function() {
  runMochaSuite('NonExistentRoute');
});

var CrSettingsAdvancedPageTest = class extends CrSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/advanced_page_test.js&host=webui-test';
  }
};

// Copied from Polymer 2 test:
// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
GEN('#endif');
TEST_F('CrSettingsAdvancedPageTest', 'MAYBE_Load', function() {
  mocha.run();
});

[['AllSites', 'all_sites_tests.js'],
 ['AppearanceFontsPage', 'appearance_fonts_page_test.js'],
 ['AppearancePage', 'appearance_page_test.js'],
 [
   'SettingsCategoryDefaultRadioGroup',
   'settings_category_default_radio_group_tests.js'
 ],
 ['CategoryDefaultSetting', 'category_default_setting_tests.js'],
 ['CategorySettingExceptions', 'category_setting_exceptions_tests.js'],
 ['Checkbox', 'checkbox_tests.js'],
 ['ChooserExceptionList', 'chooser_exception_list_tests.js'],
 ['ChooserExceptionListEntry', 'chooser_exception_list_entry_tests.js'],
 ['CollapseRadioButton', 'collapse_radio_button_tests.js'],
 ['ControlledButton', 'controlled_button_tests.js'],
 ['ControlledRadioButton', 'controlled_radio_button_tests.js'],
 ['DoNotTrackToggle', 'do_not_track_toggle_test.js'],
 ['DownloadsPage', 'downloads_page_test.js'],
 ['DropdownMenu', 'dropdown_menu_tests.js'],
 ['ExtensionControlledIndicator', 'extension_controlled_indicator_tests.js'],
 ['HelpPage', 'help_page_test.js'],
 ['Menu', 'settings_menu_test.js'],
 ['PaymentsSection', 'payments_section_test.js'],
 ['PeoplePage', 'people_page_test.js'],
 ['PeoplePageSyncControls', 'people_page_sync_controls_test.js'],
 ['PeoplePageSyncPage', 'people_page_sync_page_test.js'],
 ['Prefs', 'prefs_tests.js'],
 ['PrefUtil', 'pref_util_tests.js'],
 ['ProtocolHandlers', 'protocol_handlers_tests.js'],
 ['RecentSitePermissions', 'recent_site_permissions_test.js'],
 // Flaky on all OSes. TODO(crbug.com/1127733): Enable the test.
 ['ResetPage', 'reset_page_test.js', 'DISABLED_All'],
 ['ResetProfileBanner', 'reset_profile_banner_test.js'],
 ['SearchEngines', 'search_engines_page_test.js'],
 ['SearchPage', 'search_page_test.js'],
 ['Search', 'search_settings_test.js'],
 ['SecurityKeysSubpage', 'security_keys_subpage_test.js'],
 ['SecureDns', 'secure_dns_test.js'],
 ['SiteData', 'site_data_test.js'],
 ['SiteDataDetails', 'site_data_details_subpage_tests.js'],
 ['SiteDetailsPermission', 'site_details_permission_tests.js'],
 ['SiteEntry', 'site_entry_tests.js'],
 ['SiteFavicon', 'site_favicon_test.js'],
 ['SiteListEntry', 'site_list_entry_tests.js'],
 ['SiteSettingsPage', 'site_settings_page_test.js'],
 ['Slider', 'settings_slider_tests.js'],
 ['StartupUrlsPage', 'startup_urls_page_test.js'],
 ['Subpage', 'settings_subpage_test.js'],
 ['SyncAccountControl', 'sync_account_control_test.js'],
 ['Textarea', 'settings_textarea_tests.js'],
 ['ToggleButton', 'settings_toggle_button_tests.js'],
 ['ZoomLevels', 'zoom_levels_tests.js'],
].forEach(test => registerTest(...test));

// Timeout on Linux and MacOS dbg bots: https://crbug.com/1133412
// Fails on Mac/Arm: https://crbug.com/1222886
GEN('#if (!defined(OS_LINUX) && !defined(OS_MAC)) || ' +
    '(defined(NDEBUG) && !defined(ARCH_CPU_ARM64))');
[['SecurityPage', 'security_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');


// Flaky on MacOS bots and times out on Linux Dbg: https://crbug.com/1240747
GEN('#if (!defined(OS_MAC)) && (!defined(OS_LINUX) || defined(NDEBUG))');
[['CookiesPage', 'cookies_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif');

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)');
[['PasswordsSectionCros', 'passwords_section_test_cros.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // BUILDFLAG(IS_CHROMEOS_ASH)) || BUILDFLAG(IS_CHROMEOS_LACROS)');

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
[['PeoplePageChromeOS', 'people_page_test_cros.js'],
 // Copied from Polymer 2 test. TODO(crbug.com/929455): flaky, fix.
 ['SiteListChromeOS', 'site_list_tests_cros.js', 'DISABLED_AndroidSmsInfo'],
].forEach(test => registerTest(...test));
GEN('#endif  // BUILDFLAG(IS_CHROMEOS_ASH)');

GEN('#if !defined(OS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH)');
[['EditDictionaryPage', 'edit_dictionary_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // !defined(OS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH)');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)');
[['DefaultBrowser', 'default_browser_test.js'],
 ['ImportDataDialog', 'import_data_dialog_test.js'],
 ['SystemPage', 'system_page_tests.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)');

GEN('#if !BUILDFLAG(IS_CHROMEOS_ASH)');
[['PeoplePageManageProfile', 'people_page_manage_profile_test.js'],
 ['Languages', 'languages_tests.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)');

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');
[['ChromeCleanupPage', 'chrome_cleanup_page_test.js'],
 ['IncompatibleApplicationsPage', 'incompatible_applications_page_test.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)');
registerTest('MetricsReporting', 'metrics_reporting_tests.js');
GEN('#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) ' +
    '&& !BUILDFLAG(IS_CHROMEOS_ASH)');

function registerTest(testName, module, caseName) {
  const className = `CrSettings${testName}Test`;
  this[className] = class extends CrSettingsBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://settings/test_loader.html?module=settings/${module}&host=webui-test`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
