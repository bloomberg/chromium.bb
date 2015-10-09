// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.accountswitcher;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
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

    /**
     * The registered callback instance.
     */
    private Callback mCallback;

    /**
     * Constructs an account-switcher, using the given Context to create any Views. Called from
     * the activity's onCreate() method.
     * @param context Context used for creating Views and performing UI operations.
     * @param callback Callback for receiving notifications from the account-switcher.
     */
    public AccountSwitcherBasic(Context context, Callback callback) {
        mCallback = callback;
        mAccountsSpinner = new Spinner(context);
        mAccountsSpinner.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));
        int padding = (int) (context.getResources().getDisplayMetrics().density * 16f);
        mAccountsSpinner.setPadding(padding, padding, padding, padding);
        mContainer = new LinearLayout(context);
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
        Context context = getView().getContext();
        Account[] accounts = AccountManager.get(context).getAccountsByType(ACCOUNT_TYPE);
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
