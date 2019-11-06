// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

/**
 * Basic shared functionality for partner customization integration tests.
 */
public class BasePartnerBrowserCustomizationIntegrationTestRule
        extends ChromeActivityTestRule<ChromeActivity> {
    BasePartnerBrowserCustomizationIntegrationTestRule() {
        super(ChromeActivity.class);
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
                PartnerBrowserCustomizations.setProviderAuthorityForTests(
                        TestPartnerBrowserCustomizationsProvider.class.getName());
                base.evaluate();
            }
        }, desc);
    }
}
