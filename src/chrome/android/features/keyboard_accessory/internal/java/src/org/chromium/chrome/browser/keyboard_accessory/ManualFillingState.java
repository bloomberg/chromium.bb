// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.support.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.keyboard_accessory.data.CachedProviderAdapter;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AddressAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.CreditCardAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.PasswordAccessorySheetCoordinator;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.ArrayList;

/**
 * This class holds all data that is necessary to restore the state of the Keyboard accessory
 * and its sheet for the {@link WebContents} it is attached to.
 */
class ManualFillingState {
    private final WebContents mWebContents;
    private boolean mWebContentsShowing;
    private @Nullable CachedProviderAdapter<KeyboardAccessoryData.Action[]> mActionsProvider;
    private @Nullable CachedProviderAdapter<AccessorySheetData> mPasswordSheetDataProvider;
    private @Nullable CachedProviderAdapter<AccessorySheetData> mAddressSheetDataProvider;
    private @Nullable PasswordAccessorySheetCoordinator mPasswordAccessorySheet;
    private @Nullable AddressAccessorySheetCoordinator mAddressAccessorySheet;
    private @Nullable CreditCardAccessorySheetCoordinator mCreditCardAccessorySheet;

    private class Observer extends WebContentsObserver {
        public Observer(WebContents webContents) {
            super(webContents);
        }

        @Override
        public void wasShown() {
            super.wasShown();
            mWebContentsShowing = true;
            if (mActionsProvider != null) mActionsProvider.notifyAboutCachedItems();
            if (mPasswordSheetDataProvider != null)
                mPasswordSheetDataProvider.notifyAboutCachedItems();
        }

        @Override
        public void wasHidden() {
            super.wasHidden();
            mWebContentsShowing = false;
        }
    };

    private final WebContentsObserver mWebContentsObserver;

    /**
     * Creates a new set of user data that is bound to and observing the given web contents.
     * @param webContents Some {@link WebContents} which are assumed to be shown right now.
     */
    ManualFillingState(@Nullable WebContents webContents) {
        mWebContents = webContents;
        if (webContents == null) {
            mWebContentsObserver = null;
            return;
        }
        mWebContentsShowing = true;
        mWebContentsObserver = new Observer(mWebContents);
        mWebContents.addObserver(mWebContentsObserver);
    }

    /**
     * Repeats the latest data that known {@link CachedProviderAdapter}s cached to all
     * {@link Provider.Observer}s.
     */
    void notifyObservers() {
        if (mActionsProvider != null) mActionsProvider.notifyAboutCachedItems();
        if (mPasswordSheetDataProvider != null) mPasswordSheetDataProvider.notifyAboutCachedItems();
    }

    KeyboardAccessoryData.Tab[] getTabs() {
        ArrayList<KeyboardAccessoryData.Tab> tabs = new ArrayList<>();
        if (mPasswordAccessorySheet != null) tabs.add(mPasswordAccessorySheet.getTab());
        if (mCreditCardAccessorySheet != null) tabs.add(mCreditCardAccessorySheet.getTab());
        if (mAddressAccessorySheet != null) tabs.add(mAddressAccessorySheet.getTab());
        return tabs.toArray(new KeyboardAccessoryData.Tab[0]);
    }

    void destroy() {
        if (mWebContents != null) mWebContents.removeObserver(mWebContentsObserver);
        mActionsProvider = null;
        mPasswordSheetDataProvider = null;
        mPasswordAccessorySheet = null;
        mCreditCardAccessorySheet = null;
        mAddressAccessorySheet = null;
        mWebContentsShowing = false;
    }

    /**
     * Wraps the given ActionProvider in a {@link CachedProviderAdapter} and stores it.
     * @param provider A {@link PropertyProvider} providing actions.
     * @param defaultActions A default set of actions to prepopulate the adapter's cache.
     */
    void wrapActionsProvider(PropertyProvider<KeyboardAccessoryData.Action[]> provider,
            KeyboardAccessoryData.Action[] defaultActions) {
        mActionsProvider = new CachedProviderAdapter<>(
                provider, defaultActions, this::onAdapterReceivedNewData);
    }

    /**
     * Returns the wrapped provider set with {@link #wrapActionsProvider}.
     * @return A {@link CachedProviderAdapter} wrapping a {@link PropertyProvider}.
     */
    Provider<KeyboardAccessoryData.Action[]> getActionsProvider() {
        return mActionsProvider;
    }

    /**
     * Wraps the given provider for password data in a {@link CachedProviderAdapter} and stores it.
     * @param provider A {@link PropertyProvider} providing password sheet data.
     */
    void wrapPasswordSheetDataProvider(PropertyProvider<AccessorySheetData> provider) {
        mPasswordSheetDataProvider =
                new CachedProviderAdapter<>(provider, null, this::onAdapterReceivedNewData);
    }

    /**
     * Returns the wrapped provider set with {@link #wrapPasswordSheetDataProvider}.
     * @return A {@link CachedProviderAdapter} wrapping a {@link PropertyProvider}.
     */
    Provider<AccessorySheetData> getPasswordSheetDataProvider() {
        return mPasswordSheetDataProvider;
    }

    void setPasswordAccessorySheet(@Nullable PasswordAccessorySheetCoordinator sheet) {
        mPasswordAccessorySheet = sheet;
    }

    @Nullable
    PasswordAccessorySheetCoordinator getPasswordAccessorySheet() {
        return mPasswordAccessorySheet;
    }

    /**
     * Wraps the given provider for address data in a {@link CachedProviderAdapter} and stores it.
     * @param provider A {@link PropertyProvider} providing password sheet data.
     */
    void wrapAddressSheetDataProvider(PropertyProvider<AccessorySheetData> provider) {
        mAddressSheetDataProvider =
                new CachedProviderAdapter<>(provider, null, this::onAdapterReceivedNewData);
    }

    /**
     * Returns the wrapped provider set with {@link #wrapAddressSheetDataProvider}.
     * @return A {@link CachedProviderAdapter} wrapping a {@link PropertyProvider}.
     */
    Provider<AccessorySheetData> getAddressSheetDataProvider() {
        return mAddressSheetDataProvider;
    }

    void setAddressAccessorySheet(@Nullable AddressAccessorySheetCoordinator sheet) {
        mAddressAccessorySheet = sheet;
    }

    @Nullable
    AddressAccessorySheetCoordinator getAddressAccessorySheet() {
        return mAddressAccessorySheet;
    }

    void setCreditCardAccessorySheet(@Nullable CreditCardAccessorySheetCoordinator sheet) {
        mCreditCardAccessorySheet = sheet;
    }

    @Nullable
    CreditCardAccessorySheetCoordinator getCreditCardAccessorySheet() {
        return mCreditCardAccessorySheet;
    }

    private void onAdapterReceivedNewData(CachedProviderAdapter adapter) {
        if (mWebContentsShowing) adapter.notifyAboutCachedItems();
    }

    @VisibleForTesting
    WebContentsObserver getWebContentsObserverForTesting() {
        return mWebContentsObserver;
    }
}
