// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.IBinder;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.text.TextUtils;
import android.util.SparseArray;
import android.widget.RemoteViews;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Utilies for managing the active {@link BrowserSessionContentHandler}. This is an interface owned
 * by the currently focused {@link ChromeActivity} has a linkage to a third party client app through
 * a session.
 */
public class BrowserSessionContentUtils {
    private static final String TAG = "BrowserSession_Utils";

    private static final SparseArray<ContentHandlerData> sTaskIdToHandlerData = new SparseArray<>();

    @Nullable
    private static BrowserSessionContentHandler sActiveContentHandler;

    @Nullable
    private static Callback<CustomTabsSessionToken> sSessionDisconnectCallback;

    /**
     * The data associated with the content handler necessary to pass new intents to it.
     */
    private static class ContentHandlerData {
        public final CustomTabsSessionToken session;

        // Content handlers can reside in Activities of different types, so we need to store the
        // Activity class to be able to route new intents into it.
        public final Class<? extends Activity> activityClass;

        private ContentHandlerData(CustomTabsSessionToken session,
                Class<? extends Activity> activityClass) {
            this.session = session;
            this.activityClass = activityClass;
        }
    }

    /**
     * Sets the currently active {@link BrowserSessionContentHandler} in focus.
     * @param contentHandler {@link BrowserSessionContentHandler} to set.
     */
    public static void setActiveContentHandler(
            @NonNull BrowserSessionContentHandler contentHandler) {
        sActiveContentHandler = contentHandler;
        CustomTabsSessionToken session = sActiveContentHandler.getSession();
        if (session != null) {
            sTaskIdToHandlerData.append(sActiveContentHandler.getTaskId(),
                    new ContentHandlerData(session, sActiveContentHandler.getActivityClass()));
        }
        ensureSessionCleanUpOnDisconnects();
    }

    /**
     * Notifies that given {@link BrowserSessionContentHandler} no longer has focus.
     */
    public static void removeActiveContentHandler(BrowserSessionContentHandler contentHandler) {
        if (sActiveContentHandler == contentHandler) {
            sActiveContentHandler = null;
        } // else this contentHandler has already been replaced.

        // Intentionally not removing from sTaskIdToHandlerData to handle cases when the task is
        // brought to foreground by a new intent - the CCT might not be able to call
        // setActiveContentHandler in time.
    }

    /**
     * Returns the class of Activity with a matching session running in the same task as the given
     * intent is being launched from, or null if no such Activity present.
     */
    @Nullable
    public static Class<? extends Activity> getActiveHandlerClassInCurrentTask(Intent intent,
            Context context) {
        if (!(context instanceof Activity)) return null;
        int taskId = ((Activity) context).getTaskId();
        ContentHandlerData handlerDataInCurrentTask = sTaskIdToHandlerData.get(taskId);
        if (handlerDataInCurrentTask == null || !handlerDataInCurrentTask.session.equals(
                    CustomTabsSessionToken.getSessionTokenFromIntent(intent))) {
            return null;
        }
        return handlerDataInCurrentTask.activityClass;
    }

    /**
     * Called when a Browser Services intent is handled.
     *
     * Used to check whether an incoming intent can be handled by the current
     * {@link BrowserSessionContentHandler}, and to perform action on new Intent.
     *
     * @return Whether the active {@link BrowserSessionContentHandler} has handled the intent.
     */
    public static boolean handleBrowserServicesIntent(Intent intent) {
        String url = IntentHandler.getUrlFromIntent(intent);
        if (TextUtils.isEmpty(url)) return false;

        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        return handleExternalIntent(intent, url, session);
    }

    private static boolean handleExternalIntent(Intent intent, String url,
           @Nullable CustomTabsSessionToken session) {
        if (!sessionMatchesActiveContent(session)) return false;
        if (sActiveContentHandler.shouldIgnoreIntent(intent)) {
            Log.w(TAG, "Incoming intent to Custom Tab was ignored.");
            return false;
        }
        sActiveContentHandler.loadUrlAndTrackFromTimestamp(
                new LoadUrlParams(url), IntentHandler.getTimestampFromIntent(intent));
        return true;
    }

    private static boolean sessionMatchesActiveContent(@Nullable CustomTabsSessionToken session) {
        return session != null && sActiveContentHandler != null &&
                session.equals(sActiveContentHandler.getSession());
    }

    /**
     * @return Whether the given session is the currently active session.
     */
    public static boolean isActiveSession(CustomTabsSessionToken session) {
        if (sActiveContentHandler == null) return false;
        if (session == null || sActiveContentHandler.getSession() == null) return false;
        return sActiveContentHandler.getSession().equals(session);
    }

    /**
     * Checks whether the given referrer can be used as valid within the Activity launched by the
     * given intent. For this to be true, the intent should be for a {@link CustomTabsSessionToken}
     * that is the currently in focus custom tab and also the related client should have a verified
     * relationship with the referrer origin. This can only be true for https:// origins.
     *
     * @param intent The intent that was used to launch the Activity in question.
     * @param referrer The referrer url that is to be used.
     * @return Whether the given referrer is a valid first party url to the client that launched
     *         the activity.
     */
    public static boolean canActiveContentHandlerUseReferrer(Intent intent, Uri referrer) {
        if (sActiveContentHandler == null) return false;
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        if (session == null || !session.equals(sActiveContentHandler.getSession())) return false;
        String packageName =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(session);
        if (TextUtils.isEmpty(packageName)) return false;
        boolean valid = OriginVerifier.wasPreviouslyVerified(
                packageName, new Origin(referrer), CustomTabsService.RELATION_USE_AS_ORIGIN);

        // OriginVerifier should only be allowing https schemes.
        assert valid == UrlConstants.HTTPS_SCHEME.equals(referrer.getScheme());

        return valid;
    }

    /**
     * @return The url for the page displayed using the current {@link
     * BrowserSessionContentHandler}.
     */
    public static String getCurrentUrlForActiveBrowserSession() {
        if (sActiveContentHandler == null) return null;
        return sActiveContentHandler.getCurrentUrl();
    }

    /**
     * @return The pending url for the page about to be displayed using the current {@link
     * BrowserSessionContentHandler}.
     */
    public static String getPendingUrlForActiveBrowserSession() {
        if (sActiveContentHandler == null) return null;
        return sActiveContentHandler.getPendingUrl();
    }

    /**
     * Checks whether the active {@link BrowserSessionContentHandler} belongs to the given session,
     * and if true, update toolbar's custom button.
     * @param session     The {@link IBinder} that the calling client represents.
     * @param bitmap      The new icon for action button.
     * @param description The new content description for the action button.
     * @return Whether the update is successful.
     */
    public static boolean updateCustomButton(
            CustomTabsSessionToken session, int id, Bitmap bitmap, String description) {
        ThreadUtils.assertOnUiThread();
        // Do nothing if there is no activity or the activity does not belong to this session.
        if (sActiveContentHandler == null || !sActiveContentHandler.getSession().equals(session)) {
            return false;
        }
        return sActiveContentHandler.updateCustomButton(id, bitmap, description);
    }

    /**
     * Checks whether the active {@link BrowserSessionContentHandler} belongs to the given session,
     * and if true, updates {@link RemoteViews} on the secondary toolbar.
     * @return Whether the update is successful.
     */
    public static boolean updateRemoteViews(CustomTabsSessionToken session, RemoteViews remoteViews,
            int[] clickableIDs, PendingIntent pendingIntent) {
        ThreadUtils.assertOnUiThread();
        // Do nothing if there is no activity or the activity does not belong to this session.
        if (sActiveContentHandler == null || !sActiveContentHandler.getSession().equals(session)) {
            return false;
        }
        return sActiveContentHandler.updateRemoteViews(remoteViews, clickableIDs, pendingIntent);
    }

    private static void ensureSessionCleanUpOnDisconnects() {
        if (sSessionDisconnectCallback != null) return;
        sSessionDisconnectCallback = (session) -> {
            if (session == null) {
                return;
            }
            for (int i = 0; i < sTaskIdToHandlerData.size(); i++) {
                if (session.equals(sTaskIdToHandlerData.valueAt(i).session)) {
                    sTaskIdToHandlerData.removeAt(i);
                }
            }
        };
        ChromeApplication.getComponent().resolveCustomTabsConnection()
                .setDisconnectCallback(sSessionDisconnectCallback);
    }
}
