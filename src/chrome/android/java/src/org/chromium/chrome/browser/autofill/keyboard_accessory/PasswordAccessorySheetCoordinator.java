// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.CREDENTIALS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.SCROLL_LISTENER;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetViewBinder.ItemViewHolder;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.modelutil.SimpleRecyclerViewMcp;

/**
 * This component is a tab that can be added to the {@link ManualFillingCoordinator} which shows it
 * as bottom sheet below the keyboard accessory.
 */
public class PasswordAccessorySheetCoordinator implements KeyboardAccessoryData.Tab.Listener {
    private final PropertyModel mModel = new PropertyModel.Builder(CREDENTIALS, SCROLL_LISTENER)
                                                 .with(CREDENTIALS, new ListModel<>())
                                                 .with(SCROLL_LISTENER, null)
                                                 .build();
    private final PasswordAccessorySheetMediator mMediator;
    private final KeyboardAccessoryData.Tab mTab;

    @VisibleForTesting
    static class IconProvider {
        private final static IconProvider sInstance = new IconProvider();
        private Drawable mIcon;
        private IconProvider() {}

        public static IconProvider getInstance() {
            return sInstance;
        }

        /**
         * Loads and remembers the icon used for this class. Used to mock icons in unit tests.
         * @param context The context containing the icon resources.
         * @return The icon as {@link Drawable}.
         */
        public Drawable getIcon(Context context) {
            if (mIcon != null) return mIcon;
            mIcon = AppCompatResources.getDrawable(context, R.drawable.ic_vpn_key_grey);
            return mIcon;
        }

        @VisibleForTesting
        void setIconForTesting(Drawable icon) {
            mIcon = icon;
        }
    }

    interface FaviconProvider {
        @Nullable
        Bitmap getFavicon();
    }

    /**
     * Creates the passwords tab.
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     * @param scrollListener An optional listener that will be bound to the inflated recycler view.
     */
    public PasswordAccessorySheetCoordinator(
            Context context, @Nullable RecyclerView.OnScrollListener scrollListener) {
        mMediator = new PasswordAccessorySheetMediator(mModel, scrollListener);
        mTab = new KeyboardAccessoryData.Tab(IconProvider.getInstance().getIcon(context),
                context.getString(R.string.password_accessory_sheet_toggle),
                context.getString(R.string.password_accessory_sheet_opened),
                R.layout.password_accessory_sheet, AccessoryTabType.PASSWORDS, this);
    }

    @Override
    public void onTabCreated(ViewGroup view) {
        PasswordAccessorySheetViewBinder.initializeView((RecyclerView) view, mModel);
    }

    @Override
    public void onTabShown() {
        mMediator.onTabShown();
    }

    /**
     * Creates an adapter to an {@link PasswordAccessorySheetViewBinder} that is wired
     * up to the model change processor which listens to the {@link ListModel <Item>}.
     * @param model the {@link ListModel <Item>} the adapter gets its data from.
     * @return Returns a fully initialized and wired adapter to a PasswordAccessorySheetViewBinder.
     */
    static RecyclerViewAdapter<ItemViewHolder, Void> createAdapter(ListModel<Item> model) {
        return new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, Item::getType, ItemViewHolder::bind),
                ItemViewHolder::create);
    }

    /**
     * Registered item providers can replace the currently shown data in the password sheet.
     * @param itemProvider The provider this component will listen to.
     */
    public void registerItemProvider(KeyboardAccessoryData.Provider<Item[]> itemProvider) {
        itemProvider.addObserver(mMediator);
    }

    /**
     * Returns the Tab object that describes the appearance of this class in the keyboard accessory
     * or its accessory sheet. The returned object doesn't change for this instance.
     * @return Returns a stable {@link KeyboardAccessoryData.Tab} that is connected to this sheet.
     */
    public KeyboardAccessoryData.Tab getTab() {
        return mTab;
    }

    @VisibleForTesting
    ListModel<Item> getModelForTesting() {
        return mMediator.getModelForTesting();
    }
}
