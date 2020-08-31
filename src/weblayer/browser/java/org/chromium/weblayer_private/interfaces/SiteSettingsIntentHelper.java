// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

/**
 * A helper class for creating Intents to start the Site Settings UI.
 */
public class SiteSettingsIntentHelper {
    /** Creates an Intent that launches the main category list UI. */
    public static Intent createIntentForCategoryList(Context context, String profileName) {
        Bundle extras = new Bundle();
        extras.putString(SiteSettingsFragmentArgs.PROFILE_NAME, profileName);
        extras.putString(
                SiteSettingsFragmentArgs.FRAGMENT_NAME, SiteSettingsFragmentArgs.CATEGORY_LIST);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the settings UI for a single category. */
    public static Intent createIntentForSingleCategory(
            Context context, String profileName, String categoryType, String categoryTitle) {
        Bundle extras = new Bundle();
        extras.putString(SiteSettingsFragmentArgs.PROFILE_NAME, profileName);
        extras.putString(
                SiteSettingsFragmentArgs.FRAGMENT_NAME, SiteSettingsFragmentArgs.SINGLE_CATEGORY);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TYPE, categoryType);
        fragmentArgs.putString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TITLE, categoryTitle);
        extras.putBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the single website settings UI. */
    public static Intent createIntentForSingleWebsite(
            Context context, String profileName, String url) {
        Bundle extras = new Bundle();
        extras.putString(SiteSettingsFragmentArgs.PROFILE_NAME, profileName);
        extras.putString(
                SiteSettingsFragmentArgs.FRAGMENT_NAME, SiteSettingsFragmentArgs.SINGLE_WEBSITE);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SiteSettingsFragmentArgs.SINGLE_WEBSITE_URL, url);
        extras.putBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    private static Intent createIntentWithExtras(Context context, Bundle extras) {
        Intent intent = new Intent();
        intent.setClassName(context, SiteSettingsFragmentArgs.ACTIVITY_CLASS_NAME);
        intent.putExtras(extras);
        return intent;
    }

    private SiteSettingsIntentHelper() {}
}
