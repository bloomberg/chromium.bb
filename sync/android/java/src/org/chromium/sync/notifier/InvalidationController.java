// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.collect.Lists;

import org.chromium.base.ActivityStatus;
import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.Set;

/**
 * Controller used to send start, stop, and registration-change commands to the invalidation
 * client library used by Sync.
 */
public class InvalidationController implements ActivityStatus.StateListener {
    /**
     * Constants and utility methods to create the intents used to communicate between the
     * controller and the invalidation client library.
     */
    public static class IntentProtocol {
        /**
         * Action set on register intents.
         */
        public static final String ACTION_REGISTER =
                "org.chromium.sync.notifier.ACTION_REGISTER_TYPES";

        /**
         * Parcelable-valued intent extra containing the account of the user.
         */
        public static final String EXTRA_ACCOUNT = "account";

        /**
         * String-list-valued intent extra of the syncable types to sync.
         */
        public static final String EXTRA_REGISTERED_TYPES = "registered_types";

        /**
         * Boolean-valued intent extra indicating that the service should be stopped.
         */
        public static final String EXTRA_STOP = "stop";

        /**
         * Create an Intent that will start the invalidation listener service and
         * register for the specified types.
         */
        public static Intent createRegisterIntent(Account account,
                                                  boolean allTypes, Set<ModelType> types) {
            Intent registerIntent = new Intent(ACTION_REGISTER);
            String[] selectedTypesArray;
            if (allTypes) {
                selectedTypesArray = new String[]{ModelType.ALL_TYPES_TYPE};
            } else {
                selectedTypesArray = new String[types.size()];
                int pos = 0;
                for (ModelType type : types) {
                    selectedTypesArray[pos++] = type.name();
                }
            }
            registerIntent.putStringArrayListExtra(EXTRA_REGISTERED_TYPES,
                    Lists.newArrayList(selectedTypesArray));
            registerIntent.putExtra(EXTRA_ACCOUNT, account);
            return registerIntent;
        }

        /** Returns whether {@code intent} is a stop intent. */
        public static boolean isStop(Intent intent) {
            return intent.getBooleanExtra(EXTRA_STOP, false);
        }

        /** Returns whether {@code intent} is a registered types change intent. */
        public static boolean isRegisteredTypesChange(Intent intent) {
            return intent.hasExtra(EXTRA_REGISTERED_TYPES);
        }

        private IntentProtocol() {
            // Disallow instantiation.
        }
    }

    private static final Object LOCK = new Object();

    private static InvalidationController sInstance;

    private final Context mContext;

    /**
     * Sets the types for which the client should register for notifications.
     *
     * @param account  Account of the user.
     * @param allTypes If {@code true}, registers for all types, and {@code types} is ignored
     * @param types    Set of types for which to register. Ignored if {@code allTypes == true}.
     */
    public void setRegisteredTypes(Account account, boolean allTypes, Set<ModelType> types) {
        Intent registerIntent = IntentProtocol.createRegisterIntent(account, allTypes, types);
        registerIntent.setClass(mContext, InvalidationService.class);
        mContext.startService(registerIntent);
    }

    /**
     * Reads all stored preferences and calls
     * {@link #setRegisteredTypes(android.accounts.Account, boolean, java.util.Set)} with the stored
     * values, refreshing the set of types with {@code types}. It can be used on startup of Chrome
     * to ensure we always have a set of registrations consistent with the native code.
     * @param types    Set of types for which to register.
     */
    public void refreshRegisteredTypes(Set<ModelType> types) {
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        Set<String> savedSyncedTypes = invalidationPreferences.getSavedSyncedTypes();
        Account account = invalidationPreferences.getSavedSyncedAccount();
        boolean allTypes = savedSyncedTypes != null &&
                savedSyncedTypes.contains(ModelType.ALL_TYPES_TYPE);
        setRegisteredTypes(account, allTypes, types);
    }

    /**
     * Starts the invalidation client.
     */
    public void start() {
        Intent intent = new Intent(mContext, InvalidationService.class);
        mContext.startService(intent);
    }

    /**
     * Stops the invalidation client.
     */
    public void stop() {
        Intent intent = new Intent(mContext, InvalidationService.class);
        intent.putExtra(IntentProtocol.EXTRA_STOP, true);
        mContext.startService(intent);
    }

    /**
     * Returns the contract authority to use when requesting sync.
     */
    public String getContractAuthority() {
        return mContext.getPackageName();
    }

    /**
     * Returns the instance that will use {@code context} to issue intents.
     *
     * Calling this method will create the instance if it does not yet exist.
     */
    public static InvalidationController get(Context context) {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new InvalidationController(context);
            }
            return sInstance;
        }
    }

    /**
     * Creates an instance using {@code context} to send intents.
     */
    @VisibleForTesting
    InvalidationController(Context context) {
        mContext = Preconditions.checkNotNull(context.getApplicationContext());
        ActivityStatus.registerStateListener(this);
    }

    @Override
    public void onActivityStateChange(int newState) {
        if (SyncStatusHelper.get(mContext).isSyncEnabled()) {
            if (newState == ActivityStatus.PAUSED) {
                stop();
            } else if (newState == ActivityStatus.RESUMED) {
                start();
            }
        }
    }
}
