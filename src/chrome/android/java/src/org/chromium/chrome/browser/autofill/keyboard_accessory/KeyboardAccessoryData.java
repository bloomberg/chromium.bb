// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.annotation.LayoutRes;
import android.support.annotation.Nullable;
import android.support.annotation.Px;
import android.view.ViewGroup;

import org.chromium.base.Callback;

import java.util.ArrayList;
import java.util.List;

/**
 * Interfaces in this class are used to pass data into keyboard accessory component.
 */
public class KeyboardAccessoryData {
    /**
     * A provider notifies all registered {@link Observer}s about a changed object.
     * @param <T> The object this provider provides.
     */
    public interface Provider<T> {
        /**
         * Every observer added by this needs to be notified whenever the object changes.
         * @param observer The observer to be notified.
         */
        void addObserver(Observer<T> observer);

        /**
         * Passes the given item to all subscribed {@link Observer}s.
         * @param item The item to be passed to the {@link Observer}s.
         */
        void notifyObservers(T item);
    }

    /**
     * An observer receives notifications from an {@link Provider} it is subscribed to.
     * @param <T> Any object that this instance observes.
     */
    public interface Observer<T> {
        int DEFAULT_TYPE = Integer.MIN_VALUE;

        /**
         * A provider calls this function with an item that should be available in the keyboard
         * accessory.
         * @param typeId Specifies which type of item this update affects.
         * @param item An item to be displayed in the Accessory.
         */
        void onItemAvailable(int typeId, T item);
    }

    /**
     * Describes a tab which should be displayed as a small icon at the start of the keyboard
     * accessory. Typically, a tab is responsible to change the accessory sheet below the accessory.
     */
    public final static class Tab {
        private final Drawable mIcon;
        private final @Nullable String mOpeningAnnouncement;
        private final String mContentDescription;
        private final int mTabLayout;
        private final @AccessoryTabType int mRecordingType;
        private final @Nullable Listener mListener;

        /**
         * A Tab's Listener get's notified when e.g. the Tab was assigned a view.
         */
        public interface Listener {
            /**
             * Triggered when the tab was successfully created.
             * @param view The newly created accessory sheet of the tab.
             */
            void onTabCreated(ViewGroup view);

            /**
             * Triggered when the tab becomes visible to the user.
             */
            void onTabShown();
        }

        public Tab(Drawable icon, String contentDescription, @LayoutRes int tabLayout,
                @AccessoryTabType int recordingType, @Nullable Listener listener) {
            this(icon, contentDescription, null, tabLayout, recordingType, listener);
        }

        public Tab(Drawable icon, String contentDescription, @Nullable String openingAnnouncement,
                @LayoutRes int tabLayout, @AccessoryTabType int recordingType,
                @Nullable Listener listener) {
            mIcon = icon;
            mContentDescription = contentDescription;
            mOpeningAnnouncement = openingAnnouncement;
            mTabLayout = tabLayout;
            mListener = listener;
            mRecordingType = recordingType;
        }

        /**
         * Provides the icon that will be displayed in the {@link KeyboardAccessoryCoordinator}.
         * @return The small icon that identifies this tab uniquely.
         */
        public Drawable getIcon() {
            return mIcon;
        }

        /**
         * The description for this tab. It will become the content description of the icon.
         * @return A short string describing the name of this tab.
         */
        public String getContentDescription() {
            return mContentDescription;
        }

        /**
         * An optional announcement triggered when the Tab is opened.
         * @return A string describing the contents of this tab.
         */
        public String getOpeningAnnouncement() {
            return mOpeningAnnouncement;
        }

        /**
         * Recording type of this tab. Used to sort it into the correct UMA bucket.
         * @return A {@link AccessoryTabType}.
         */
        public @AccessoryTabType int getRecordingType() {
            return mRecordingType;
        }

        /**
         * Returns the tab layout which allows to create the tab's view on demand.
         * @return The layout resource that allows to create the view necessary for this tab.
         */
        public @LayoutRes int getTabLayout() {
            return mTabLayout;
        }

        /**
         * Returns the listener which might need to react on changes to this tab.
         * @return A {@link Listener} to be called, e.g. when the tab is created.
         */
        public @Nullable Listener getListener() {
            return mListener;
        }
    }

    /**
     * This describes an action that can be invoked from the keyboard accessory.
     * The most prominent example hereof is the "Generate Password" action.
     */
    public static final class Action {
        private final String mCaption;
        private final Callback<Action> mActionCallback;
        private @AccessoryAction int mType;

        public Action(String caption, @AccessoryAction int type, Callback<Action> actionCallback) {
            mCaption = caption;
            mActionCallback = actionCallback;
            mType = type;
        }

        public String getCaption() {
            return mCaption;
        }

        public Callback<Action> getCallback() {
            return mActionCallback;
        }

        public @AccessoryAction int getActionType() {
            return mType;
        }
    }

    /**
     * This describes an item in a accessory sheet. They are usually list items that were created
     * natively.
     *
     * TODO(crbug.com/902425): Remove this class once all code in the frontend uses
     *                         AccessorySheetData.
     */
    public final static class Item {
        private final int mType;
        private final String mCaption;
        private final String mContentDescription;
        private final boolean mIsObfuscated;
        private final @Nullable Callback<Item> mItemSelectedCallback;
        private final @Nullable FaviconProvider mFaviconProvider;

        /**
         * Items will call a class that implements this interface to request a favicon.
         *
         * TODO(crbug.com/902425): Move this to UserInfo.Field once Item is deprecated and
         *                         generalize it so it can handle static assets for credit card
         *                         items.
         */
        interface FaviconProvider {
            /**
             * Starts a request for a favicon. The callback can be called either asynchronously or
             * synchronously (depending on whether the icon was cached).
             * @param favicon The icon to be used for this Item. If null, use the default icon.
             */
            void fetchFavicon(@Px int desiredSize, Callback<Bitmap> favicon);
        }

        /**
         * Creates a new Item of type {@link ItemType#LABEL}. It is not interactive.
         * @param caption The text of the displayed item.
         * @param contentDescription The description of this item (i.e. used for accessibility).
         */
        public static Item createLabel(String caption, String contentDescription) {
            return new Item(ItemType.LABEL, caption, contentDescription, false, null, null);
        }

        /**
         * Creates a new Item of type {@link ItemType#SUGGESTION} if has a callback, otherwise, it
         * will be {@link ItemType#NON_INTERACTIVE_SUGGESTION}. It usually is part of a list of
         * suggestions and can have a callback that is triggered on selection.
         * @param caption The text of the displayed item. Only plain text if |isObfuscated| is
         * false.
         * @param contentDescription The description of this item (i.e. used for accessibility).
         * @param isObfuscated If true, the displayed caption is transformed into stars.
         * @param itemSelectedCallback A click on this item will invoke this callback. Optional.
         */
        public static Item createSuggestion(String caption, String contentDescription,
                boolean isObfuscated, @Nullable Callback<Item> itemSelectedCallback,
                @Nullable FaviconProvider faviconProvider) {
            if (itemSelectedCallback == null) {
                return new Item(ItemType.NON_INTERACTIVE_SUGGESTION, caption, contentDescription,
                        isObfuscated, null, faviconProvider);
            }
            return new Item(ItemType.SUGGESTION, caption, contentDescription, isObfuscated,
                    itemSelectedCallback, faviconProvider);
        }

        /**
         * Creates an Item of type {@link ItemType#DIVIDER}. Basically, it's a horizontal line.
         */
        public static Item createDivider() {
            return new Item(ItemType.DIVIDER, null, null, false, null, null);
        }

        /**
         * Creates a new Item of type {@link ItemType#OPTION}. They are normally independent items
         * that trigger a unique action (e.g. generate a password or navigate to an overview).
         * @param caption The text of the displayed option.
         * @param contentDescription The description of this option (i.e. used for accessibility).
         * @param callback A click on this item will invoke this callback.
         */
        public static Item createOption(
                String caption, String contentDescription, Callback<Item> callback) {
            return new Item(ItemType.OPTION, caption, contentDescription, false, callback, null);
        }

        /**
         * Creates an Item of type {@link ItemType#TOP_DIVIDER}. A horizontal line meant to be
         * displayed at the top of the accessory sheet.
         */
        public static Item createTopDivider() {
            return new Item(ItemType.TOP_DIVIDER, null, null, false, null, null);
        }

        /**
         * Creates a new item.
         * @param type Type of the item (e.g. non-clickable LABEL or clickable SUGGESTION).
         * @param caption The text of the displayed item. Only plain text if |isObfuscated| is
         * false.
         * @param contentDescription The description of this item (i.e. used for accessibility).
         * @param isObfuscated If true, the displayed caption is transformed into stars.
         * @param itemSelectedCallback If the Item is interactive, a click on it will trigger this.
         * @param faviconProvider
         */
        private Item(@ItemType int type, String caption, String contentDescription,
                boolean isObfuscated, @Nullable Callback<Item> itemSelectedCallback,
                @Nullable FaviconProvider faviconProvider) {
            mType = type;
            mCaption = caption;
            mContentDescription = contentDescription;
            mIsObfuscated = isObfuscated;
            mItemSelectedCallback = itemSelectedCallback;
            mFaviconProvider = faviconProvider;
        }

        /**
         * Returns the type of the item.
         * @return Returns a {@link ItemType}.
         */
        public @ItemType int getType() {
            return mType;
        }

        /**
         * Returns a human-readable, translated string that will appear as text of the item.
         * @return A short descriptive string of the item.
         */
        public String getCaption() {
            return mCaption;
        }

        /**
         * Returns a translated description that can be used for accessibility.
         * @return A short description of the displayed item.
         */
        public String getContentDescription() {
            return mContentDescription;
        }

        /**
         * Returns whether obfuscation should be applied to the item's caption, for example to hide
         * passwords.
         * @return Returns true if obfuscation should be applied to the caption.
         */
        public boolean isObfuscated() {
            return mIsObfuscated;
        }

        /**
         * The delegate is called when the Item is selected by a user.
         */
        public Callback<Item> getItemSelectedCallback() {
            return mItemSelectedCallback;
        }

        public void fetchFavicon(@Px int desiredSize, Callback<Bitmap> faviconCallback) {
            if (mFaviconProvider == null) {
                faviconCallback.onResult(null); // Use default icon without provider.
                return;
            }
            mFaviconProvider.fetchFavicon(desiredSize, faviconCallback);
        }
    }

    /**
     * Represents a Profile, or a Credit Card, or the credentials for a website
     * (username + password), to be shown on the manual fallback UI.
     */
    public final static class UserInfo {
        private final List<Field> mFields = new ArrayList<>();

        /**
         * Represents an item (either selectable or not) presented on the UI, such as the username
         * or a credit card number.
         */
        public final static class Field {
            private final String mDisplayText;
            private final String mA11yDescription;
            private final boolean mIsObfuscated;
            private final Callback<Field> mCallback;

            /**
             * Creates a new Field.
             * @param displayText The text to display. Plain text if |isObfuscated| is false.
             * @param a11yDescription The description used for accessibility.
             * @param isObfuscated If true, the displayed caption is transformed into stars.
             * @param callback Called when the user taps the suggestions.
             */
            public Field(String displayText, String a11yDescription, boolean isObfuscated,
                    Callback<Field> callback) {
                mDisplayText = displayText;
                mA11yDescription = a11yDescription;
                mIsObfuscated = isObfuscated;
                mCallback = callback;
            }

            /**
             * Returns the text to be displayed on the UI.
             */
            public String getDisplayText() {
                return mDisplayText;
            }

            /**
             * Returns a translated description that can be used for accessibility.
             */
            public String getA11yDescription() {
                return mA11yDescription;
            }

            /**
             * Returns whether the user can interact with the selected suggestion. For example,
             * this is false if this is a password suggestion on a non-password input field.
             */
            public boolean isSelectable() {
                return mCallback != null;
            }

            /**
             * Returns true if obfuscation should be applied to the item's caption, for example to
             * hide passwords.
             */
            public boolean isObfuscated() {
                return mIsObfuscated;
            }

            /**
             * The delegate is called when the Item is selected by a user.
             */
            public void triggerSelection() {
                if (mCallback != null) mCallback.onResult(this);
            }
        }

        public UserInfo() {}

        /**
         * Adds a new field to the group.
         * @param field The field to be added.
         */
        public void addField(Field field) {
            mFields.add(field);
        }

        /**
         * Returns the list of fields in this group.
         */
        public List<Field> getFields() {
            return mFields;
        }
    }

    /**
     * Represents a command below the suggestions, such as "Manage password...".
     */
    public final static class FooterCommand {
        private final String mDisplayText;
        private final Callback<FooterCommand> mCallback;

        /**
         * Creates a new FooterCommand.
         * @param displayText The text to be displayed on the footer.
         * @param callback Called when the user taps the suggestions.
         */
        public FooterCommand(String displayText, Callback<FooterCommand> callback) {
            mDisplayText = displayText;
            mCallback = callback;
        }

        /**
         * Returns the translated text to be shown on the UI for this footer command. This text is
         * used for accessibility.
         */
        public String getDisplayText() {
            return mDisplayText;
        }

        /**
         * Returns the translated text to be shown on the UI for this footer command. This text is
         * used for accessibility.
         */
        public void execute() {
            mCallback.onResult(this);
        }
    }

    /**
     * Represents the contents of a accessory sheet tab below the keyboard accessory, which can
     * correspond to passwords, credit cards, or profiles data. Created natively.
     *
     * TODO(crbug.com/902425): Add a field to indicate if this corresponds to password, profile, or
     *                         credit card data.
     */
    public final static class AccessorySheetData {
        private final String mTitle;
        private final List<UserInfo> mUserInfoList = new ArrayList<>();
        private final List<FooterCommand> mFooterCommands = new ArrayList<>();

        /**
         * Creates the AccessorySheetData object.
         * @param title The title of accessory sheet tab.
         */
        public AccessorySheetData(String title) {
            mTitle = title;
        }

        /**
         * Returns the title of the accessory sheet. This text is also used for accessibility.
         */
        public String getTitle() {
            return mTitle;
        }

        /**
         * Returns the list of {@link UserInfo} to be shown on the accessory sheet.
         */
        public List<UserInfo> getUserInfoList() {
            return mUserInfoList;
        }

        /** Returns the list of {@link FooterCommand} to be shown on the accessory sheet.
         */
        public List<FooterCommand> getFooterCommands() {
            return mFooterCommands;
        }
    }

    /**
     * A simple class that holds a list of {@link Observer}s which can be notified about new data by
     * directly passing that data into {@link PropertyProvider#notifyObservers(T)}.
     * @param <T> The object this provider provides.
     */
    public static class PropertyProvider<T> implements Provider<T> {
        private final List<Observer<T>> mObservers = new ArrayList<>();
        protected int mType;

        public PropertyProvider() {
            this(Observer.DEFAULT_TYPE);
        }

        public PropertyProvider(int type) {
            mType = type;
        }

        @Override
        public void addObserver(Observer<T> observer) {
            mObservers.add(observer);
        }

        @Override
        public void notifyObservers(T item) {
            for (Observer<T> observer : mObservers) {
                observer.onItemAvailable(mType, item);
            }
        }
    }

    private KeyboardAccessoryData() {}
}
