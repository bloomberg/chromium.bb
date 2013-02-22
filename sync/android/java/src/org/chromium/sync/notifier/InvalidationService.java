// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.accounts.Account;
import android.app.PendingIntent;
import android.content.ContentResolver;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.Lists;
import com.google.common.collect.Sets;
import com.google.ipc.invalidation.external.client.InvalidationListener.RegistrationState;
import com.google.ipc.invalidation.external.client.contrib.AndroidListener;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protos.ipc.invalidation.Types.ClientType;

import org.chromium.base.ActivityStatus;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationController.IntentProtocol;
import org.chromium.sync.notifier.InvalidationPreferences.EditContext;
import org.chromium.sync.signin.AccountManagerHelper;

import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Service that controls notifications for sync.
 * <p>
 * This service serves two roles. On the one hand, it is a client for the notification system
 * used to trigger sync. It receives invalidations and converts them into
 * {@link ContentResolver#requestSync} calls, and it supplies the notification system with the set
 * of desired registrations when requested.
 * <p>
 * On the other hand, this class is controller for the notification system. It starts it and stops
 * it, and it requests that it perform (un)registrations as the set of desired sync types changes.
 * <p>
 * This class is an {@code IntentService}. All methods are assumed to be executing on its single
 * execution thread.
 *
 * @author dsmyers@google.com
 */
public class InvalidationService extends AndroidListener {
    /* This class must be public because it is exposed as a service. */

    /** Notification client typecode. */
    @VisibleForTesting
    static final int CLIENT_TYPE = ClientType.Type.CHROME_SYNC_ANDROID_VALUE;

    private static final String TAG = InvalidationService.class.getSimpleName();

    private static final Random RANDOM = new Random();

    /**
     * Whether the underlying notification client has been started. This boolean is updated when a
     * start or stop intent is issued to the underlying client, not when the intent is actually
     * processed.
     */
    private static boolean sIsClientStarted;

    /**
     * The id of the client in use, if any. May be {@code null} if {@link #sIsClientStarted} is
     * true if the client has not yet gone ready.
     */
    @Nullable private static byte[] sClientId;

    @Override
    public void onHandleIntent(Intent intent) {
        // Ensure that a client is or is not running, as appropriate, and that it is for the
        // correct account. ensureAccount will stop the client if account is non-null and doesn't
        // match the stored account. Then, if a client should be running, ensureClientStartState
        // will start a new one if needed. I.e., these two functions work together to restart the
        // client when the account changes.
        Account account = intent.hasExtra(IntentProtocol.EXTRA_ACCOUNT) ?
                (Account) intent.getParcelableExtra(IntentProtocol.EXTRA_ACCOUNT) : null;
        ensureAccount(account);
        ensureClientStartState();

        // Handle the intent.
        if (IntentProtocol.isStop(intent) && sIsClientStarted) {
            // If the intent requests that the client be stopped, stop it.
            stopClient();
        } else if (IntentProtocol.isRegisteredTypesChange(intent)) {
            // If the intent requests a change in registrations, change them.
            List<String> regTypes =
                    intent.getStringArrayListExtra(IntentProtocol.EXTRA_REGISTERED_TYPES);
            setRegisteredTypes(Sets.newHashSet(regTypes));
        } else {
            // Otherwise, we don't recognize the intent. Pass it to the notification client service.
            super.onHandleIntent(intent);
        }
    }

    @Override
    public void invalidate(Invalidation invalidation, byte[] ackHandle) {
        byte[] payload = invalidation.getPayload();
        String payloadStr = (payload == null) ? null : new String(payload);
        requestSync(invalidation.getObjectId(), invalidation.getVersion(), payloadStr);
        acknowledge(ackHandle);
    }

    @Override
    public void invalidateUnknownVersion(ObjectId objectId, byte[] ackHandle) {
        requestSync(objectId, null, null);
        acknowledge(ackHandle);
    }

    @Override
    public void invalidateAll(byte[] ackHandle) {
        requestSync(null, null, null);
        acknowledge(ackHandle);
    }

    @Override
    public void informRegistrationFailure(
            byte[] clientId, ObjectId objectId, boolean isTransient, String errorMessage) {
        Log.w(TAG, "Registration failure on " + objectId + " ; transient = " + isTransient
                + ": " + errorMessage);
        if (isTransient) {
          // Retry immediately on transient failures. The base AndroidListener will handle
          // exponential backoff if there are repeated failures.
          List<ObjectId> objectIdAsList = Lists.newArrayList(objectId);
          if (readRegistrationsFromPrefs().contains(objectId)) {
              register(clientId, objectIdAsList);
          } else {
              unregister(clientId, objectIdAsList);
          }
        }
    }

    @Override
    public void informRegistrationStatus(
            byte[] clientId, ObjectId objectId, RegistrationState regState) {
        Log.d(TAG, "Registration status for " + objectId + ": " + regState);
        List<ObjectId> objectIdAsList = Lists.newArrayList(objectId);
        boolean registrationisDesired = readRegistrationsFromPrefs().contains(objectId);
        if (regState == RegistrationState.REGISTERED) {
          if (!registrationisDesired) {
            Log.i(TAG, "Unregistering for object we're no longer interested in");
            unregister(clientId, objectIdAsList);
          }
        } else {
          if (registrationisDesired) {
            Log.i(TAG, "Registering for an object");
            register(clientId, objectIdAsList);
          }
        }
    }

    @Override
    public void informError(ErrorInfo errorInfo) {
        Log.w(TAG, "Invalidation client error:" + errorInfo);
        if (!errorInfo.isTransient() && sIsClientStarted) {
            // It is important not to stop the client if it is already stopped. Otherwise, the
            // possibility exists to go into an infinite loop if the stop call itself triggers an
            // error (e.g., because no client actually exists).
            stopClient();
        }
    }

    @Override
    public void ready(byte[] clientId) {
        setClientId(clientId);

        // We might have accumulated some registrations to do while we were waiting for the client
        // to become ready.
        reissueRegistrations(clientId);
    }

    @Override
    public void reissueRegistrations(byte[] clientId) {
        Set<ObjectId> desiredRegistrations = readRegistrationsFromPrefs();
        if (!desiredRegistrations.isEmpty()) {
            register(clientId, desiredRegistrations);
        }
    }

    @Override
    public void requestAuthToken(final PendingIntent pendingIntent,
            @Nullable String invalidAuthToken) {
        @Nullable Account account = SyncStatusHelper.get(this).getSignedInUser();
        if (account == null) {
            // This should never happen, because this code should only be run if a user is
            // signed-in.
            Log.w(TAG, "No signed-in user; cannot send message to data center");
            return;
        }

        // Attempt to retrieve a token for the user. This method will also invalidate
        // invalidAuthToken if it is non-null.
        AccountManagerHelper.get(this).getNewAuthTokenFromForeground(
                account, invalidAuthToken, SyncStatusHelper.AUTH_TOKEN_TYPE_SYNC,
                new AccountManagerHelper.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(String token) {
                        if (token != null) {
                            InvalidationService.setAuthToken(
                                    InvalidationService.this.getApplicationContext(), pendingIntent,
                                    token, SyncStatusHelper.AUTH_TOKEN_TYPE_SYNC);
                        }
                    }
                });
    }

    @Override
    public void writeState(byte[] data) {
        InvalidationPreferences invPreferences = new InvalidationPreferences(this);
        EditContext editContext = invPreferences.edit();
        invPreferences.setInternalNotificationClientState(editContext, data);
        invPreferences.commit(editContext);
    }

    @Override
    @Nullable public byte[] readState() {
        return new InvalidationPreferences(this).getInternalNotificationClientState();
    }

    /**
     * Ensures that the client is running or not running as appropriate, based on the value of
     * {@link #shouldClientBeRunning}.
     */
    private void ensureClientStartState() {
        final boolean shouldClientBeRunning = shouldClientBeRunning();
        if (!shouldClientBeRunning && sIsClientStarted) {
            // Stop the client if it should not be running and is.
            stopClient();
        } else if (shouldClientBeRunning && !sIsClientStarted) {
            // Start the client if it should be running and isn't.
            startClient();
        }
    }

    /**
     * If {@code intendedAccount} is non-{@null} and differs from the account stored in preferences,
     * then stops the existing client (if any) and updates the stored account.
     */
    private void ensureAccount(@Nullable Account intendedAccount) {
        if (intendedAccount == null) {
            return;
        }
        InvalidationPreferences invPrefs = new InvalidationPreferences(this);
        if (!intendedAccount.equals(invPrefs.getSavedSyncedAccount())) {
            if (sIsClientStarted) {
                stopClient();
            }
            setAccount(intendedAccount);
        }
    }

    /**
     * Starts a new client, destroying any existing client. {@code owningAccount} is the account
     * of the user for which the client is being created; it will be persisted using
     * {@link InvalidationPreferences#setAccount}.
     */
    private void startClient() {
        Intent startIntent = AndroidListener.createStartIntent(this, CLIENT_TYPE, getClientName());
        startService(startIntent);
        setIsClientStarted(true);
    }

    /** Stops the notification client. */
    private void stopClient() {
        startService(AndroidListener.createStopIntent(this));
        setIsClientStarted(false);
        setClientId(null);
    }

    /** Sets the saved sync account in {@link InvalidationPreferences} to {@code owningAccount}. */
    private void setAccount(Account owningAccount) {
        InvalidationPreferences invPrefs = new InvalidationPreferences(this);
        EditContext editContext = invPrefs.edit();
        invPrefs.setAccount(editContext, owningAccount);
        invPrefs.commit(editContext);
    }

    /**
     * Reads the saved sync types from storage (if any) and returns a set containing the
     * corresponding object ids.
     */
    @VisibleForTesting
    Set<ObjectId> readRegistrationsFromPrefs() {
        Set<String> savedTypes = new InvalidationPreferences(this).getSavedSyncedTypes();
        if (savedTypes == null) {
            return Collections.emptySet();
        } else {
            Set<ModelType> modelTypes = ModelType.syncTypesToModelTypes(savedTypes);
            Set<ObjectId> objectIds = Sets.newHashSetWithExpectedSize(modelTypes.size());
            for (ModelType modelType : modelTypes) {
                objectIds.add(modelType.toObjectId());
            }
            return objectIds;
        }
    }

    /**
     * Sets the types for which notifications are required to {@code syncTypes}. {@code syncTypes}
     * is either a list of specific types or the special wildcard type
     * {@link ModelType#ALL_TYPES_TYPE}.
     * <p>
     * @param syncTypes
     */
    private void setRegisteredTypes(Set<String> syncTypes) {
        // If we have a ready client and will be making registration change calls on it, then
        // read the current registrations from preferences before we write the new values, so that
        // we can take the diff of the two registration sets and determine which registration change
        // calls to make.
        Set<ObjectId> existingRegistrations = (sClientId == null) ?
                null : readRegistrationsFromPrefs();

        // Write the new sync types to preferences. We do not expand the syncTypes to take into
        // account the ALL_TYPES_TYPE at this point; we want to persist the wildcard unexpanded.
        InvalidationPreferences prefs = new InvalidationPreferences(this);
        EditContext editContext = prefs.edit();
        prefs.setSyncTypes(editContext, syncTypes);
        prefs.commit(editContext);

        // If we do not have a ready invalidation client, we cannot change its registrations, so
        // return. Later, when the client is ready, we will supply the new registrations.
        if (sClientId == null) {
            return;
        }

        // We do have a ready client. Unregister any existing registrations not present in the
        // new set and register any elements in the new set not already present. This call does
        // expansion of the ALL_TYPES_TYPE wildcard.
        // NOTE: syncTypes MUST NOT be used below this line, since it contains an unexpanded
        // wildcard.
        Set<ModelType> newRegisteredTypes = ModelType.syncTypesToModelTypes(syncTypes);

        List<ObjectId> unregistrations = Lists.newArrayList();
        List<ObjectId> registrations = Lists.newArrayList();
        computeRegistrationOps(existingRegistrations,
                ModelType.modelTypesToObjectIds(newRegisteredTypes), registrations,
                unregistrations);
        unregister(sClientId, unregistrations);
        register(sClientId, registrations);
    }

    /**
     * Computes the set of (un)registrations to perform so that the registrations active in the
     * Ticl will be {@code desiredRegs}, given that {@existingRegs} already exist.
     *
     * @param regAccumulator registrations to perform
     * @param unregAccumulator unregistrations to perform.
     */
    @VisibleForTesting
    static void computeRegistrationOps(Set<ObjectId> existingRegs, Set<ObjectId> desiredRegs,
            Collection<ObjectId> regAccumulator, Collection<ObjectId> unregAccumulator) {
        // Registrations to do are elements in the new set but not the old set.
        regAccumulator.addAll(Sets.difference(desiredRegs, existingRegs));

        // Unregistrations to do are elements in the old set but not the new set.
        unregAccumulator.addAll(Sets.difference(existingRegs, desiredRegs));
    }

    /**
     * Requests that the sync system perform a sync.
     *
     * @param objectId the object that changed, if known.
     * @param version the version of the object that changed, if known.
     * @param payload the payload of the change, if known.
     */
    private void requestSync(@Nullable ObjectId objectId, @Nullable Long version,
            @Nullable String payload) {
        // Construct the bundle to supply to the native sync code.
        Bundle bundle = new Bundle();
        if (objectId == null && version == null && payload == null) {
            // Use an empty bundle in this case for compatibility with the v1 implementation.
        } else {
            if (objectId != null) {
                bundle.putString("objectId", new String(objectId.getName()));
            }
            // We use "0" as the version if we have an unknown-version invalidation. This is OK
            // because the native sync code special-cases zero and always syncs for invalidations at
            // that version (Tango defines a special UNKNOWN_VERSION constant with this value).
            bundle.putLong("version", (version == null) ? 0 : version);
            bundle.putString("payload", (payload == null) ? "" : payload);
        }
        Account account = SyncStatusHelper.get(this).getSignedInUser();
        String contractAuthority = InvalidationController.get(this).getContractAuthority();
        requestSyncFromContentResolver(bundle, account, contractAuthority);
    }

    /**
     * Calls {@link ContentResolver#requestSync(Account, String, Bundle)} to trigger a sync. Split
     * into a separate method so that it can be overriden in tests.
     */
    @VisibleForTesting
    void requestSyncFromContentResolver(
            Bundle bundle, Account account, String contractAuthority) {
        Log.d(TAG, "Request sync: " + account + " / " + contractAuthority + " / "
            + bundle.keySet());
        ContentResolver.requestSync(account, contractAuthority, bundle);
    }

    /**
     * Returns whether the notification client should be running, i.e., whether Chrome is in the
     * foreground and sync is enabled.
     */
    @VisibleForTesting
    boolean shouldClientBeRunning() {
        return isSyncEnabled() && isChromeInForeground();
    }

    /** Returns whether sync is enabled. LLocal method so it can be overridden in tests. */
    @VisibleForTesting
    boolean isSyncEnabled() {
        return SyncStatusHelper.get(getApplicationContext()).isSyncEnabled();
    }

    /**
     * Returns whether Chrome is in the foreground. Local method so it can be overridden in tests.
     */
    @VisibleForTesting
    boolean isChromeInForeground() {
        switch (ActivityStatus.getState()) {
            case ActivityStatus.CREATED:
            case ActivityStatus.STARTED:
            case ActivityStatus.RESUMED:
                return true;
            default:
                return false;
        }
    }

    /** Returns whether the notification client has been started, for tests. */
    @VisibleForTesting
    static boolean getIsClientStartedForTest() {
        return sIsClientStarted;
    }

    /** Returns the notification client id, for tests. */
    @VisibleForTesting
    @Nullable static byte[] getClientIdForTest() {
        return sClientId;
    }

    /** Returns the client name used for the notification client. */
    private static byte[] getClientName() {
        // TODO(dsmyers): we should use the same client name as the native sync code.
        // Bug: https://code.google.com/p/chromium/issues/detail?id=172391
        return Long.toString(RANDOM.nextLong()).getBytes();
    }

    private static void setClientId(byte[] clientId) {
        sClientId = clientId;
    }

    private static void setIsClientStarted(boolean isStarted) {
        sIsClientStarted = isStarted;
    }
}
