// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.accountswitcher;

import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.Spinner;

import org.chromium.chromoting.AccountsAdapter;

/**
 * This class implements a basic UI for a user account switcher.
 */
public class AccountSwitcherBasic extends AccountSwitcherBase {
    /** Only accounts of this type will be selectable. */
    private static final String ACCOUNT_TYPE = "com.google";

    private String mSelectedAccount;

    private Spinner mAccountsSpinner;
    private LinearLayout mContainer;

    private Activity mActivity;

    /**
     * The registered callback instance.
     */
    private Callback mCallback;

    /**
     * Constructs an account-switcher, using the given Activity to create any Views. Called from
     * the activity's onCreate() method.
     * @param activity Activity used for creating Views and performing UI operations.
     * @param callback Callback for receiving notifications from the account-switcher.
     */
    public AccountSwitcherBasic(Activity activity, Callback callback) {
        mActivity = activity;
        mCallback = callback;
        mAccountsSpinner = new Spinner(activity);
        mAccountsSpinner.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));
        int padding = (int) (activity.getResources().getDisplayMetrics().density * 16f);
        mAccountsSpinner.setPadding(padding, padding, padding, padding);
        mContainer = new LinearLayout(activity);
        mContainer.setOrientation(LinearLayout.VERTICAL);
        mContainer.addView(mAccountsSpinner);
    }

    @Override
    public View getView() {
        return mContainer;
    }

    @Override
    public void setNavigation(View view) {
        mContainer.removeAllViews();
        mContainer.addView(mAccountsSpinner);
        mContainer.addView(view);
    }

    @Override
    public void setDrawer(View drawerView) {
    }

    @Override
    public void setSelectedAndRecentAccounts(String selected, String[] recents) {
        // This implementation does not support recents.
        mSelectedAccount = selected;
    }

    @Override
    public String[] getRecentAccounts() {
        return new String[0];
    }

    @Override
    public void reloadAccounts() {
        // AccountManager.getAccountsByType() requires the GET_ACCOUNTS permission, which is
        // classed as a dangerous permission. If the permission is not granted, an exception might
        // be thrown or the account-list might wrongly appear to be empty. Check if the permission
        // has been granted, and request it if not, so the user is aware of the cause of this
        // problem.
        if (ContextCompat.checkSelfPermission(mActivity, Manifest.permission.GET_ACCOUNTS)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(mActivity,
                    new String[] {Manifest.permission.GET_ACCOUNTS}, 0);
            return;
        }

        Account[] accounts = AccountManager.get(mActivity).getAccountsByType(ACCOUNT_TYPE);
        if (accounts.length == 0) {
            mCallback.onAccountsListEmpty();
            return;
        }

        // Arbitrarily default to the first account if the currently-selected account is not in the
        // list.
        int selectedIndex = 0;
        for (int i = 0; i < accounts.length; i++) {
            if (accounts[i].name.equals(mSelectedAccount)) {
                selectedIndex = i;
                break;
            }
        }
        mSelectedAccount = accounts[selectedIndex].name;

        AccountsAdapter adapter = new AccountsAdapter(getView().getContext(), accounts);
        mAccountsSpinner.setAdapter(adapter);
        mAccountsSpinner.setSelection(selectedIndex);
        mAccountsSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int itemPosition,
                    long itemId) {
                Account selected = (Account) parent.getItemAtPosition(itemPosition);
                mSelectedAccount = selected.name;
                mCallback.onAccountSelected(mSelectedAccount);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

        mCallback.onAccountSelected(mSelectedAccount);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
    }

    @Override
    public void destroy() {
    }
}
