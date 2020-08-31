// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/**
 * A controller that triggers an auto sign-in snackbar. Auto sign-in snackbar is
 * triggered on a request credentials call of a Credential Manager API.
 */
public class AutoSigninSnackbarController implements SnackbarManager.SnackbarController {
    private final SnackbarManager mSnackbarManager;
    private final TabObserver mTabObserver;
    private final Tab mTab;

    /**
     * Displays Auto sign-in snackbar, which communicates to the users that they
     * were signed in to the web site.
     */
    @CalledByNative
    private static void showSnackbar(Tab tab, String text) {
        ChromeActivity activity = (ChromeActivity) TabUtils.getActivity(tab);
        if (activity == null) return;
        SnackbarManager snackbarManager = activity.getSnackbarManager();
        AutoSigninSnackbarController snackbarController =
                new AutoSigninSnackbarController(snackbarManager, tab);
        Snackbar snackbar = Snackbar.make(
                text, snackbarController, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_AUTO_LOGIN);
        Context context = tab.getWindowAndroid().getActivity().get();
        int backgroundColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.default_control_color_active);
        Drawable icon = AppCompatResources.getDrawable(context, R.drawable.logo_avatar_anonymous);
        snackbar.setSingleLine(false)
                .setBackgroundColor(backgroundColor)
                .setProfileImage(icon)
                .setTextAppearance(R.style.TextAppearance_TextMedium_Primary_Light);
        snackbarManager.showSnackbar(snackbar);
    }

    /**
     * Creates an instance of a {@link AutoSigninSnackbarController}.
     * @param snackbarManager The manager that helps to show up snackbar.
     */
    private AutoSigninSnackbarController(SnackbarManager snackbarManager, Tab tab) {
        mTab = tab;
        mSnackbarManager = snackbarManager;
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                AutoSigninSnackbarController.this.dismissAutoSigninSnackbar();
            }

            @Override
            public void onDestroyed(Tab tab) {
                AutoSigninSnackbarController.this.dismissAutoSigninSnackbar();
            }

            @Override
            public void onCrash(Tab tab) {
                AutoSigninSnackbarController.this.dismissAutoSigninSnackbar();
            }
        };
        mTab.addObserver(mTabObserver);
    }

    /**
     * Dismisses the snackbar.
     */
    public void dismissAutoSigninSnackbar() {
        if (mSnackbarManager.isShowing()) {
            mSnackbarManager.dismissSnackbars(this);
        }
    }

    @Override
    public void onAction(Object actionData) {}

    @Override
    public void onDismissNoAction(Object actionData) {
        mTab.removeObserver(mTabObserver);
    }
}
