// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.ItemType;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * The coordinator of account picker is the only public class in the account_picker package.
 *
 * It is responsible for setting up the account list's view and model and it serves as an access
 * point for users of this package.
 */
public class AccountPickerCoordinator {
    /**
     * Listener for account picker.
     */
    public interface Listener {
        /**
         * Notifies that the user has selected an account.
         * @param accountName The email of the selected account.
         * @param isDefaultAccount Whether the selected account is the first in the account list.
         */
        void onAccountSelected(String accountName, boolean isDefaultAccount);

        /**
         * Notifies when the user clicked the "add account" button.
         */
        void addAccount();
    }

    private final AccountPickerMediator mMediator;

    /**
     * Constructs an AccountPickerCoordinator object.
     *
     * @param view The account list recycler view.
     * @param listener Listener to notify when an account is selected or the user wants to add an
     *                 account.
     * @param selectedAccountName The name of the account that should be marked as selected.
     */
    public AccountPickerCoordinator(
            RecyclerView view, Listener listener, @Nullable String selectedAccountName) {
        assert listener != null : "The argument AccountPickerCoordinator.Listener cannot be null!";

        MVCListAdapter.ModelList listModel = new MVCListAdapter.ModelList();

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listModel);
        adapter.registerType(ItemType.ADD_ACCOUNT_ROW, AddAccountRowViewBinder::buildView,
                AddAccountRowViewBinder::bindView);
        adapter.registerType(ItemType.EXISTING_ACCOUNT_ROW, ExistingAccountRowViewBinder::buildView,
                ExistingAccountRowViewBinder::bindView);

        view.setAdapter(adapter);

        mMediator = new AccountPickerMediator(
                view.getContext(), listModel, listener, selectedAccountName);
    }

    /**
     * Destroys the resources used by the coordinator.
     */
    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Sets the selected account name. The UI should be updated in this call.
     * @param selectedAccountName The name of the account that should be marked as selected.
     */
    public void setSelectedAccountName(String selectedAccountName) {
        mMediator.setSelectedAccountName(selectedAccountName);
    }
}
