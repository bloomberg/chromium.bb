// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.ui.UiUtils;

import java.lang.ref.WeakReference;

/**
 * The class provides the WindowAndroid's implementation which requires
 * Activity Instance.
 * Only instantiate this class when you need the implemented features.
 */
public class ActivityWindowAndroid
        extends WindowAndroid
        implements ApplicationStatus.ActivityStateListener, View.OnLayoutChangeListener {
    // Constants used for intent request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;
    private static final String TAG = "ActivityWindowAndroid";

    private final WeakReference<Activity> mActivityRef;
    private int mNextRequestCode = 0;

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * TODO(jdduke): Remove this overload when all callsites have been updated to
     * indicate their activity state listening preference.
     * @param activity The activity associated with the WindowAndroid.
     */
    public ActivityWindowAndroid(Activity activity) {
        this(activity, true);
    }

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * @param activity The activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     */
    public ActivityWindowAndroid(Activity activity, boolean listenToActivityState) {
        super(activity.getApplicationContext());
        mActivityRef = new WeakReference<Activity>(activity);
        activity.findViewById(android.R.id.content).addOnLayoutChangeListener(this);
        if (listenToActivityState) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
        }
    }

    @Override
    public int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        Activity activity = mActivityRef.get();
        if (activity == null) return START_INTENT_FAILURE;

        int requestCode = generateNextRequestCode();

        try {
            activity.startIntentSenderForResult(
                    intent.getIntentSender(), requestCode, new Intent(), 0, 0, 0);
        } catch (SendIntentException e) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
        Activity activity = mActivityRef.get();
        if (activity == null) return START_INTENT_FAILURE;

        int requestCode = generateNextRequestCode();

        try {
            activity.startActivityForResult(intent, requestCode);
        } catch (ActivityNotFoundException e) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public void cancelIntent(int requestCode) {
        Activity activity = mActivityRef.get();
        if (activity == null) return;
        activity.finishActivity(requestCode);
    }

    @Override
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        IntentCallback callback = mOutstandingIntents.get(requestCode);
        mOutstandingIntents.delete(requestCode);
        String errorMessage = mIntentErrors.remove(requestCode);

        if (callback != null) {
            callback.onIntentCompleted(this, resultCode,
                    mApplicationContext.getContentResolver(), data);
            return true;
        } else {
            if (errorMessage != null) {
                showCallbackNonExistentError(errorMessage);
                return true;
            }
        }
        return false;
    }

    @Override
    public WeakReference<Activity> getActivity() {
        // Return a new WeakReference to prevent clients from releasing our internal WeakReference.
        return new WeakReference<Activity>(mActivityRef.get());
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.PAUSED) {
            onActivityPaused();
        } else if (newState == ActivityState.RESUMED) {
            onActivityResumed();
        }
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        keyboardVisibilityPossiblyChanged(UiUtils.isKeyboardShowing(mActivityRef.get(), v));
    }

    private int generateNextRequestCode() {
        int requestCode = REQUEST_CODE_PREFIX + mNextRequestCode;
        mNextRequestCode = (mNextRequestCode + 1) % REQUEST_CODE_RANGE_SIZE;
        return requestCode;
    }

    private void storeCallbackData(int requestCode, IntentCallback callback, Integer errorId) {
        mOutstandingIntents.put(requestCode, callback);
        mIntentErrors.put(
                requestCode, errorId == null ? null : mApplicationContext.getString(errorId));
    }
}
