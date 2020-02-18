// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;

import androidx.annotation.IntDef;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Allows user to pick an account and sign in. Started from Settings and various sign-in promos.
 */
// TODO(https://crbug.com/820491): extend AsyncInitializationActivity.
public class SigninActivity extends ChromeBaseAppCompatActivity {
    private static final String ARGUMENT_FRAGMENT_ARGS = "SigninActivity.FragmentArgs";

    @IntDef({SigninAccessPoint.SETTINGS, SigninAccessPoint.BOOKMARK_MANAGER,
            SigninAccessPoint.RECENT_TABS, SigninAccessPoint.SIGNIN_PROMO,
            SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, SigninAccessPoint.AUTOFILL_DROPDOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AccessPoint {}

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as it might recreate
        // SigninFragment that currently depends on native. See https://crbug.com/983730.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        super.onCreate(savedInstanceState);
        setContentView(R.layout.signin_activity);

        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment fragment = fragmentManager.findFragmentById(R.id.fragment_container);
        if (fragment == null) {
            Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
            fragment = new SigninFragment();
            fragment.setArguments(fragmentArgs);
            fragmentManager.beginTransaction().add(R.id.fragment_container, fragment).commit();
        }
    }

    /**
     * Create a new intent to start the SigninActivity.
     *
     * @param fragmentArgs arguments to create an Sign-in Fragment.
     */
    static Intent createIntent(Context context, Bundle fragmentArgs) {
        Intent intent = new Intent(context, SigninActivity.class);
        intent.putExtra(ARGUMENT_FRAGMENT_ARGS, fragmentArgs);
        return intent;
    }
}
