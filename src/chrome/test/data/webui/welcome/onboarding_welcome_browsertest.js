// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer welcome tests on onboarding-welcome UI. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/webui/welcome/nux_helper.h"');

/**
 * Test fixture for Polymer onboarding welcome elements.
 */
const OnboardingWelcomeBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  }

  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH).concat([
      '../test_browser_proxy.js',
    ]);
  }

  /** @override */
  get featureList() {
    return ['nux::kNuxOnboardingForceEnabled', ''];
  }
};

OnboardingWelcomeEmailChooserTest = class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/shared/app_chooser.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'email_chooser_test.js',
      'test_nux_email_proxy.js',
      'test_metrics_proxy.js',
      'test_bookmark_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeEmailChooserTest', 'All', function() {
  mocha.run();
});

OnboardingWelcomeEmailInterstitialTest =
    class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/email-interstitial?provider=0';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'email_interstitial_test.js',
      'test_email_interstitial_proxy.js',
      'test_nux_email_proxy.js',
      'test_welcome_browser_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeEmailInterstitialTest', 'All', function() {
  mocha.run();
});

OnboardingWelcomeWelcomeAppTest = class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/welcome_app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'welcome_app_test.js',
      'test_bookmark_proxy.js',
      'test_welcome_browser_proxy.js',
      'test_nux_set_as_default_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeWelcomeAppTest', 'All', function() {
  mocha.run();
});

OnboardingWelcomeSigninViewTest = class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/signin_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'signin_view_test.js',
      'test_nux_email_proxy.js',
      'test_welcome_browser_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeSigninViewTest', 'All', function() {
  mocha.run();
});

OnboardingWelcomeNavigationBehaviorTest =
    class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/navigation_behavior.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'navigation_behavior_test.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeNavigationBehaviorTest', 'All', function() {
  mocha.run();
});

OnboardingWelcomeModuleMetricsTest =
    class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/shared/module_metrics_proxy.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'module_metrics_test.js',
      'test_metrics_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeModuleMetricsTest', 'All', function() {
  mocha.run();
});

OnboardingWelcomeSetAsDefaultTest = class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/set_as_default/nux_set_as_default.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nux_set_as_default_test.js',
      'test_nux_set_as_default_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeSetAsDefaultTest', 'All', function() {
  mocha.run();
});

OnboardingWelcomeNtpBackgroundTest =
    class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/ntp_background/nux_ntp_background.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nux_ntp_background_test.js',
      'test_metrics_proxy.js',
      'test_ntp_background_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeNtpBackgroundTest', 'All', function() {
  mocha.run();
});
