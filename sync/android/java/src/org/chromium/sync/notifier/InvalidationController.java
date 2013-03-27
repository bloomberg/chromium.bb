// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.util.Log;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.collect.Lists;

import org.chromium.base.ActivityStatus;
import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.HashSet;
import java.util.Set;

import javax.annotation.Nullable;

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

    /**
     * Name of the manifest application metadata property specifying the name of the class
     * implementing the invalidation client.
     */
    private static final String IMPLEMENTING_CLASS_MANIFEST_PROPERTY =
            "org.chromium.sync.notifier.IMPLEMENTING_CLASS_NAME";

    /**
     * Logging tag.
     */
    private static final String TAG = InvalidationController.class.getSimpleName();

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
        Set<ModelType> typesToRegister = types;
        // Proxy types should never receive notifications.
        typesToRegister.remove(ModelType.PROXY_TABS);
        Intent registerIntent = IntentProtocol.createRegisterIntent(account, allTypes,
                typesToRegister);
        setDestinationClassName(registerIntent);
        mContext.startService(registerIntent);
    }

    /**
     * Reads all stored preferences and calls
     * {@link #setRegisteredTypes(android.accounts.Account, boolean, java.util.Set)} with the stored
     * values. It can be used on startup of Chrome to ensure we always have a consistent set of
     * registrations.
     */
    @Deprecated
    public void refreshRegisteredTypes() {
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        Set<String> savedSyncedTypes = invalidationPreferences.getSavedSyncedTypes();
        Account account = invalidationPreferences.getSavedSyncedAccount();
        boolean allTypes = savedSyncedTypes != null &&
                savedSyncedTypes.contains(ModelType.ALL_TYPES_TYPE);
        Set<ModelType> modelTypes = savedSyncedTypes == null ?
                new HashSet<ModelType>() : ModelType.syncTypesToModelTypes(savedSyncedTypes);
        setRegisteredTypes(account, allTypes, modelTypes);
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
        Intent intent = setDestinationClassName(new Intent());
        mContext.startService(intent);
    }

    /**
     * Stops the invalidation client.
     */
    public void stop() {
        Intent intent = setDestinationClassName(new Intent());
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
     * Returns the singleton instance that will use {@code context} to issue intents.
     *
     * This method is only kept until the downstream callers of this method have been changed to use
     * {@link InvalidationController#get(android.content.Context)}.
     *
     * TODO(nyquist) Remove this method.
     */
    @Deprecated
    public static InvalidationController newInstance(Context context) {
        return get(context);
    }

    /**
     * Creates an instance using {@code context} to send intents.
     */
    @VisibleForTesting
    InvalidationController(Context context) {
        mContext = Preconditions.checkNotNull(context.getApplicationContext());
        ActivityStatus.registerStateListener(this);
    }

    /**
     * Sets the destination class name of {@code intent} to the value given by the manifest
     * property named {@link #IMPLEMENTING_CLASS_MANIFEST_PROPERTY}. If no such property exists or
     * its value is null, takes no action.
     *
     * @return {@code intent}
     */
    private Intent setDestinationClassName(Intent intent) {
        String className = getDestinationClassName(mContext);
        if (className != null) {
            intent.setClassName(mContext, className);
        }
        return intent;
    }

    @VisibleForTesting
    @Nullable static String getDestinationClassName(Context context) {
        ApplicationInfo appInfo;
        try {
            // Fetch application info and read the appropriate metadata element.
            appInfo = context.getPackageManager().getApplicationInfo(context.getPackageName(),
                    PackageManager.GET_META_DATA);
            String className = null;
            if (appInfo.metaData != null) {
                className = appInfo.metaData.getString(IMPLEMENTING_CLASS_MANIFEST_PROPERTY);
            }
            if (className == null) {
                Log.wtf(TAG, "No value for " + IMPLEMENTING_CLASS_MANIFEST_PROPERTY
                        + " in manifest; sync notifications will not work");
            }
            return className;
        } catch (NameNotFoundException exception) {
            Log.wtf(TAG, "Cannot read own application info", exception);
        }
        return null;
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
