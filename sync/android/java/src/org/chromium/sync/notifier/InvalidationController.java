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

import com.google.common.base.Preconditions;
import com.google.common.collect.Lists;

import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.Set;

/**
 * Controller used to send start, stop, and registration-change commands to the invalidation
 * client library used by Sync.
 */
public class InvalidationController {
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
         * Special syncable type that lets us know to sync all types.
         */
        public static final String ALL_TYPES_TYPE = "ALL_TYPES";

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
                selectedTypesArray = new String[]{ALL_TYPES_TYPE};
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

    private final Context context;

    /**
     * Sets the types for which the client should register for notifications.
     *
     * @param account  Account of the user.
     * @param allTypes If {@code true}, registers for all types, and {@code types} is ignored
     * @param types    Set of types for which to register. Ignored if {@code allTypes == true}.
     */
    public void setRegisteredTypes(Account account, boolean allTypes, Set<ModelType> types) {
        Intent registerIntent = IntentProtocol.createRegisterIntent(account, allTypes, types);
        setDestinationClassName(registerIntent);
        context.startService(registerIntent);
    }

    /**
     * Starts the invalidation client.
     */
    public void start() {
        Intent intent = setDestinationClassName(new Intent());
        context.startService(intent);
    }

    /**
     * Stops the invalidation client.
     */
    public void stop() {
        Intent intent = setDestinationClassName(new Intent());
        intent.putExtra(IntentProtocol.EXTRA_STOP, true);
        context.startService(intent);
    }

    /**
     * Returns the contract authority to use when requesting sync.
     */
    public String getContractAuthority() {
        return context.getPackageName();
    }

    /**
     * Returns a new instance that will use {@code context} to issue intents.
     */
    public static InvalidationController newInstance(Context context) {
        return new InvalidationController(context);
    }

    /**
     * Creates an instance using {@code context} to send intents.
     */
    private InvalidationController(Context context) {
        this.context = Preconditions.checkNotNull(context.getApplicationContext());
    }

    /**
     * Sets the destination class name of {@code intent} to the value given by the manifest
     * property named {@link #IMPLEMENTING_CLASS_MANIFEST_PROPERTY}. If no such property exists or
     * its value is null, takes no action.
     *
     * @return {@code intent}
     */
    private Intent setDestinationClassName(Intent intent) {
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
            } else {
                intent.setClassName(context, className);
            }
        } catch (NameNotFoundException exception) {
            Log.wtf(TAG, "Cannot read own application info", exception);
        }
        return intent;
    }
}
