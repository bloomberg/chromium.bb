// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.io.File;

/**
 * Stores data about an installed web app. Uses SharedPreferences to persist the data to disk.
 * This class must only be accessed via {@link WebappRegistry}, which is used to register and keep
 * track of web app data known to Chrome.
 */
public class WebappDataStorage {
    private static final String TAG = "WebappDataStorage";

    static final String SHARED_PREFS_FILE_PREFIX = "webapp_";
    static final String KEY_SPLASH_ICON = "splash_icon";
    static final String KEY_LAST_USED = "last_used";
    static final String KEY_HAS_BEEN_LAUNCHED = "has_been_launched";
    static final String KEY_URL = "url";
    static final String KEY_SPLASH_SCREEN_URL = "splash_screen_url";
    static final String KEY_SCOPE = "scope";
    static final String KEY_ICON = "icon";
    static final String KEY_NAME = "name";
    static final String KEY_SHORT_NAME = "short_name";
    static final String KEY_DISPLAY_MODE = "display_mode";
    static final String KEY_ORIENTATION = "orientation";
    static final String KEY_THEME_COLOR = "theme_color";
    static final String KEY_BACKGROUND_COLOR = "background_color";
    static final String KEY_SOURCE = "source";
    static final String KEY_ACTION = "action";
    static final String KEY_IS_ICON_GENERATED = "is_icon_generated";
    static final String KEY_IS_ICON_ADAPTIVE = "is_icon_adaptive";
    static final String KEY_VERSION = "version";
    static final String KEY_WEBAPK_PACKAGE_NAME = "webapk_package_name";

    // The completion time of the last check for whether the WebAPK's Web Manifest was updated.
    static final String KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME =
            "last_check_web_manifest_update_time";

    // The last time that the WebAPK update request completed (successfully or unsuccessfully).
    static final String KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME = "last_update_request_complete_time";

    // Whether the last WebAPK update request succeeded.
    static final String KEY_DID_LAST_UPDATE_REQUEST_SUCCEED = "did_last_update_request_succeed";

    // Whether to check updates less frequently.
    static final String KEY_RELAX_UPDATES = "relax_updates";

    // The shell Apk version requested in the last update.
    static final String KEY_LAST_REQUESTED_SHELL_APK_VERSION = "last_requested_shell_apk_version";

    // Whether to show the user the Snackbar disclosure UI.
    static final String KEY_SHOW_DISCLOSURE = "show_disclosure";

    // The path where serialized update data is written before uploading to the WebAPK server.
    static final String KEY_PENDING_UPDATE_FILE_PATH = "pending_update_file_path";

    // Number of milliseconds between checks for whether the WebAPK's Web Manifest has changed.
    public static final long UPDATE_INTERVAL = DateUtils.DAY_IN_MILLIS * 3;

    // Number of milliseconds between checks of updates for a WebAPK that is expected to check
    // updates less frequently. crbug.com/680128.
    public static final long RELAXED_UPDATE_INTERVAL = DateUtils.DAY_IN_MILLIS * 30;

    // Number of milliseconds to wait before re-requesting an updated WebAPK from the WebAPK
    // server if the previous update attempt failed.
    public static final long RETRY_UPDATE_DURATION = DateUtils.DAY_IN_MILLIS;

    // The default shell Apk version of WebAPKs.
    static final int DEFAULT_SHELL_APK_VERSION = 1;

    // Invalid constants for timestamps and URLs. '0' is used as the invalid timestamp as
    // WebappRegistry and WebApkUpdateManager assume that timestamps are always valid.
    static final long TIMESTAMP_INVALID = 0;
    static final String URL_INVALID = "";
    static final int VERSION_INVALID = 0;

    // We use a heuristic to determine whether a web app is still installed on the home screen, as
    // there is no way to do so directly. Any web app which has been opened in the last ten days
    // is considered to be still on the home screen.
    static final long WEBAPP_LAST_OPEN_MAX_TIME = DateUtils.DAY_IN_MILLIS * 10;

    private static Clock sClock = new Clock();
    private static Factory sFactory = new Factory();

    private final String mId;
    private final SharedPreferences mPreferences;

    /**
     * Called after data has been retrieved from storage.
     * @param <T> The type of the data being retrieved.
     */
    public interface FetchCallback<T> {
        public void onDataRetrieved(T readObject);
    }

    /**
     * Factory used to generate WebappDataStorage objects.
     * Overridden in tests to inject mocked objects.
     */
    public static class Factory {
        /**
         * Generates a WebappDataStorage instance for a specified web app.
         */
        public WebappDataStorage create(final String webappId) {
            return new WebappDataStorage(webappId);
        }
    }

    /**
     * Clock used to generate the current time in millseconds for setting last used time.
     */
    public static class Clock {
        /**
         * @return Current time in milliseconds.
         */
        public long currentTimeMillis() {
            return System.currentTimeMillis();
        }
    }

    /**
     * Opens an instance of WebappDataStorage for the web app specified.
     * @param webappId The ID of the web app.
     */
    static WebappDataStorage open(String webappId) {
        return sFactory.create(webappId);
    }

    /**
     * Sets the clock used to get the current time.
     */
    @VisibleForTesting
    public static void setClockForTests(Clock clock) {
        sClock = clock;
    }

    /**
     * Sets the factory used to generate WebappDataStorage objects.
     */
    @VisibleForTesting
    public static void setFactoryForTests(Factory factory) {
        sFactory = factory;
    }

    /**
     * Asynchronously retrieves the splash screen image associated with the web app. The work is
     * performed on a background thread as it requires a potentially expensive image decode.
     * @param callback Called when the splash screen image has been retrieved.
     *                 The bitmap result will be null if no image was found.
     */
    public void getSplashScreenImage(final FetchCallback<Bitmap> callback) {
        new AsyncTask<Bitmap>() {
            @Override
            protected final Bitmap doInBackground() {
                return ShortcutHelper.decodeBitmapFromString(
                        mPreferences.getString(KEY_SPLASH_ICON, null));
            }

            @Override
            protected final void onPostExecute(Bitmap result) {
                assert callback != null;
                callback.onDataRetrieved(result);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Update the splash screen image associated with the web app with the specified data. The image
     * must have been encoded using {@link ShortcutHelper#encodeBitmapAsString}.
     * @param splashScreenImage The image which should be shown on the splash screen of the web app.
     */
    public void updateSplashScreenImage(String splashScreenImage) {
        mPreferences.edit().putString(KEY_SPLASH_ICON, splashScreenImage).apply();
    }

    /**
     * Creates and returns a web app launch intent from the data stored in this object.
     * @return The web app launch intent.
     */
    public Intent createWebappLaunchIntent() {
        // Assume that all of the data is invalid if the version isn't set, so return a null intent.
        int version = mPreferences.getInt(KEY_VERSION, VERSION_INVALID);
        if (version == VERSION_INVALID) return null;

        // Use "standalone" as the default display mode as this was the original assumed default for
        // all web apps.
        return ShortcutHelper.createWebappShortcutIntent(mId,
                mPreferences.getString(KEY_ACTION, null), mPreferences.getString(KEY_URL, null),
                mPreferences.getString(KEY_SCOPE, null), mPreferences.getString(KEY_NAME, null),
                mPreferences.getString(KEY_SHORT_NAME, null),
                mPreferences.getString(KEY_ICON, null),
                version,
                mPreferences.getInt(KEY_DISPLAY_MODE, WebDisplayMode.STANDALONE),
                mPreferences.getInt(KEY_ORIENTATION, ScreenOrientationValues.DEFAULT),
                mPreferences.getLong(
                        KEY_THEME_COLOR, ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING),
                mPreferences.getLong(
                        KEY_BACKGROUND_COLOR, ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING),
                mPreferences.getString(KEY_SPLASH_SCREEN_URL, ""),
                mPreferences.getBoolean(KEY_IS_ICON_GENERATED, false),
                mPreferences.getBoolean(KEY_IS_ICON_ADAPTIVE, false));
    }

    /**
     * Updates the data stored in this object to match that in the supplied intent.
     * @param shortcutIntent The intent to pull web app data from.
     */
    public void updateFromShortcutIntent(Intent shortcutIntent) {
        if (shortcutIntent == null) return;

        SharedPreferences.Editor editor = mPreferences.edit();
        boolean updated = false;

        // The URL and scope may have been deleted by the user clearing their history. Check whether
        // they are present, and update if necessary.
        String url = mPreferences.getString(KEY_URL, URL_INVALID);
        if (url.equals(URL_INVALID)) {
            url = IntentUtils.safeGetStringExtra(shortcutIntent, ShortcutHelper.EXTRA_URL);
            editor.putString(KEY_URL, url);
            updated = true;
        }

        if (mPreferences.getString(KEY_SCOPE, URL_INVALID).equals(URL_INVALID)) {
            String scope = IntentUtils.safeGetStringExtra(
                    shortcutIntent, ShortcutHelper.EXTRA_SCOPE);
            if (scope == null) {
                scope = ShortcutHelper.getScopeFromUrl(url);
            }
            editor.putString(KEY_SCOPE, scope);
            updated = true;
        }

        // For all other fields, assume that if the version key is present and equal to
        // ShortcutHelper.WEBAPP_SHORTCUT_VERSION, then all fields are present and do not need to be
        // updated. All fields except for the last used time, scope, and URL are either set or
        // cleared together.
        if (mPreferences.getInt(KEY_VERSION, VERSION_INVALID)
                != ShortcutHelper.WEBAPP_SHORTCUT_VERSION) {
            editor.putString(KEY_SPLASH_SCREEN_URL,
                    IntentUtils.safeGetStringExtra(
                            shortcutIntent, ShortcutHelper.EXTRA_SPLASH_SCREEN_URL));
            editor.putString(KEY_NAME, IntentUtils.safeGetStringExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_NAME));
            editor.putString(KEY_SHORT_NAME, IntentUtils.safeGetStringExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_SHORT_NAME));
            editor.putString(KEY_ICON, IntentUtils.safeGetStringExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_ICON));
            editor.putInt(KEY_VERSION, ShortcutHelper.WEBAPP_SHORTCUT_VERSION);

            // "Standalone" was the original assumed default for all web apps.
            editor.putInt(KEY_DISPLAY_MODE,
                    IntentUtils.safeGetIntExtra(shortcutIntent, ShortcutHelper.EXTRA_DISPLAY_MODE,
                            WebDisplayMode.STANDALONE));
            editor.putInt(KEY_ORIENTATION, IntentUtils.safeGetIntExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_ORIENTATION,
                        ScreenOrientationValues.DEFAULT));
            editor.putLong(KEY_THEME_COLOR, IntentUtils.safeGetLongExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_THEME_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING));
            editor.putLong(KEY_BACKGROUND_COLOR, IntentUtils.safeGetLongExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_BACKGROUND_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING));
            editor.putBoolean(KEY_IS_ICON_GENERATED, IntentUtils.safeGetBooleanExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_IS_ICON_GENERATED, false));
            editor.putBoolean(KEY_IS_ICON_ADAPTIVE, IntentUtils.safeGetBooleanExtra(
                        shortcutIntent, ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE, false));
            editor.putString(KEY_ACTION, shortcutIntent.getAction());

            String webApkPackageName = IntentUtils.safeGetStringExtra(
                    shortcutIntent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
            editor.putString(KEY_WEBAPK_PACKAGE_NAME, webApkPackageName);

            if (TextUtils.isEmpty(webApkPackageName)) {
                editor.putInt(KEY_SOURCE,
                        IntentUtils.safeGetIntExtra(shortcutIntent, ShortcutHelper.EXTRA_SOURCE,
                                ShortcutSource.UNKNOWN));
            }
            updated = true;
        }
        if (updated) editor.apply();
    }

    /**
     * Returns true if this web app recently (within WEBAPP_LAST_OPEN_MAX_TIME milliseconds) was
     * either:
     * - registered with WebappRegistry
     * - launched from the homescreen
     */
    public boolean wasUsedRecently() {
        // WebappRegistry.register sets the last used time, so that counts as a 'launch'.
        return (sClock.currentTimeMillis() - getLastUsedTimeMs() < WEBAPP_LAST_OPEN_MAX_TIME);
    }

    /**
     * Deletes the data for a web app by clearing all the information inside the SharedPreferences
     * file. This does NOT delete the file itself but the file is left empty.
     */
    void delete() {
        deletePendingUpdateRequestFile();
        mPreferences.edit().clear().apply();
    }

    /**
     * Deletes the URL and scope, and sets all timestamps to 0 in SharedPreferences.
     * This does not remove the stored splash screen image (if any) for the app.
     */
    void clearHistory() {
        deletePendingUpdateRequestFile();

        SharedPreferences.Editor editor = mPreferences.edit();

        editor.remove(KEY_LAST_USED);
        editor.remove(KEY_HAS_BEEN_LAUNCHED);
        editor.remove(KEY_URL);
        editor.remove(KEY_SCOPE);
        editor.remove(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME);
        editor.remove(KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME);
        editor.remove(KEY_DID_LAST_UPDATE_REQUEST_SUCCEED);
        editor.remove(KEY_RELAX_UPDATES);
        editor.remove(KEY_SHOW_DISCLOSURE);
        editor.apply();
    }

    /**
     * Returns the scope stored in this object, or URL_INVALID if it is not stored.
     */
    public String getScope() {
        return mPreferences.getString(KEY_SCOPE, URL_INVALID);
    }

    /**
     * Returns the URL stored in this object, or URL_INVALID if it is not stored.
     */
    public String getUrl() {
        return mPreferences.getString(KEY_URL, URL_INVALID);
    }

    /** Returns the source stored in this object, or ShortcutSource.UNKNOWN if it is not stored. */
    public int getSource() {
        return mPreferences.getInt(KEY_SOURCE, ShortcutSource.UNKNOWN);
    }

    /** Updates the source. */
    public void updateSource(int source) {
        mPreferences.edit().putInt(KEY_SOURCE, source).apply();
    }

    /** Returns the id stored in this object. */
    public String getId() {
        return mId;
    }

    /**
     * Returns the last used time, in milliseconds, of this object, or -1 if it is not stored.
     */
    public long getLastUsedTimeMs() {
        return mPreferences.getLong(KEY_LAST_USED, TIMESTAMP_INVALID);
    }

    /**
     * Update the information associated with the web app with the specified data. Used for testing.
     * @param splashScreenImage The image encoded as a string which should be shown on the splash
     *                          screen of the web app.
     */
    @VisibleForTesting
    void updateSplashScreenImageForTests(String splashScreenImage) {
        mPreferences.edit().putString(KEY_SPLASH_ICON, splashScreenImage).apply();
    }

    /**
     * Updates the last used time of this object.
     */
    void updateLastUsedTime() {
        mPreferences.edit().putLong(KEY_LAST_USED, sClock.currentTimeMillis()).apply();
    }

    /** Returns true if the web app has been launched at least once from the home screen. */
    boolean hasBeenLaunched() {
        return mPreferences.getBoolean(KEY_HAS_BEEN_LAUNCHED, false);
    }

    /** Sets whether the web app was launched at least once from the home screen. */
    void setHasBeenLaunched() {
        mPreferences.edit().putBoolean(KEY_HAS_BEEN_LAUNCHED, true).apply();
    }

    /**
     * Returns the package name if the data is for a WebAPK, null otherwise.
     */
    public String getWebApkPackageName() {
        return mPreferences.getString(KEY_WEBAPK_PACKAGE_NAME, null);
    }

    /**
     * Updates the time of the completion of the last check for whether the WebAPK's Web Manifest
     * was updated.
     */
    void updateTimeOfLastCheckForUpdatedWebManifest() {
        mPreferences.edit()
                .putLong(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME, sClock.currentTimeMillis())
                .apply();
    }

    /**
     * Returns the completion time, in milliseconds, of the last check for whether the WebAPK's Web
     * Manifest was updated. This time needs to be set when the WebAPK is registered.
     */
    public long getLastCheckForWebManifestUpdateTimeMs() {
        return mPreferences.getLong(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME, TIMESTAMP_INVALID);
    }

    /**
     * Updates when the last WebAPK update request finished (successfully or unsuccessfully).
     */
    void updateTimeOfLastWebApkUpdateRequestCompletion() {
        mPreferences.edit()
                .putLong(KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME, sClock.currentTimeMillis())
                .apply();
    }

    /**
     * Returns the time, in milliseconds, that the last WebAPK update request completed
     * (successfully or unsuccessfully). This time needs to be set when the WebAPK is registered.
     */
    long getLastWebApkUpdateRequestCompletionTimeMs() {
        return mPreferences.getLong(KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME, TIMESTAMP_INVALID);
    }

    /**
     * Updates whether the last update request to WebAPK Server succeeded.
     */
    void updateDidLastWebApkUpdateRequestSucceed(boolean success) {
        mPreferences.edit().putBoolean(KEY_DID_LAST_UPDATE_REQUEST_SUCCEED, success).apply();
    }

    /**
     * Returns whether the last update request to WebAPK Server succeeded.
     */
    boolean getDidLastWebApkUpdateRequestSucceed() {
        return mPreferences.getBoolean(KEY_DID_LAST_UPDATE_REQUEST_SUCCEED, false);
    }

    /**
     * Returns whether to show the user a privacy disclosure (used for TWAs and unbound WebAPKs).
     * This is not cleared until the user explicitly acknowledges it.
     */
    boolean shouldShowDisclosure() {
        return mPreferences.getBoolean(KEY_SHOW_DISCLOSURE, false);
    }

    /**
     * Clears the show disclosure bit, this stops TWAs and unbound WebAPKs from showing a privacy
     * disclosure on every resume of the Webapp. This should be called when the user has
     * acknowledged the disclosure.
     */
    void clearShowDisclosure() {
        mPreferences.edit().putBoolean(KEY_SHOW_DISCLOSURE, false).apply();
    }

    /**
     * Sets the disclosure bit which causes TWAs and unbound WebAPKs to show a privacy disclosure.
     * This is set the first time an app is opened without storage (either right after install or
     * after Chrome's storage is cleared).
     */
    void setShowDisclosure() {
        mPreferences.edit().putBoolean(KEY_SHOW_DISCLOSURE, true).apply();
    }

    /** Updates the shell Apk version requested in the last update. */
    void updateLastRequestedShellApkVersion(int shellApkVersion) {
        mPreferences.edit().putInt(KEY_LAST_REQUESTED_SHELL_APK_VERSION, shellApkVersion).apply();
    }

    /** Returns the shell Apk version requested in last update. */
    int getLastRequestedShellApkVersion() {
        return mPreferences.getInt(KEY_LAST_REQUESTED_SHELL_APK_VERSION, DEFAULT_SHELL_APK_VERSION);
    }

    /**
     * Returns whether the previous WebAPK update attempt succeeded. Returns true if there has not
     * been any update attempts.
     */
    boolean didPreviousUpdateSucceed() {
        long lastUpdateCompletionTime = getLastWebApkUpdateRequestCompletionTimeMs();
        if (lastUpdateCompletionTime == TIMESTAMP_INVALID) {
            return true;
        }
        return getDidLastWebApkUpdateRequestSucceed();
    }

    /** Sets whether we should check for updates less frequently. */
    void setRelaxedUpdates(boolean relaxUpdates) {
        mPreferences.edit().putBoolean(KEY_RELAX_UPDATES, relaxUpdates).apply();
    }

    /** Returns whether we should check for updates less frequently. */
    public boolean shouldRelaxUpdates() {
        return mPreferences.getBoolean(KEY_RELAX_UPDATES, false);
    }

    /**
     * Returns file where WebAPK update data should be stored and stores the file name in
     * SharedPreferences.
     */
    String createAndSetUpdateRequestFilePath(WebApkInfo info) {
        String filePath = WebappDirectoryManager.getWebApkUpdateFilePathForStorage(this).getPath();
        mPreferences.edit().putString(KEY_PENDING_UPDATE_FILE_PATH, filePath).apply();
        return filePath;
    }

    /** Returns the path of the file which contains data to update the WebAPK. */
    @Nullable
    String getPendingUpdateRequestPath() {
        return mPreferences.getString(KEY_PENDING_UPDATE_FILE_PATH, null);
    }

    /**
     * Deletes the file which contains data to update the WebAPK. The file is large (> 1Kb) and
     * should be deleted when the update completes.
     */
    void deletePendingUpdateRequestFile() {
        final String pendingUpdateFilePath = getPendingUpdateRequestPath();
        if (pendingUpdateFilePath == null) return;

        mPreferences.edit().remove(KEY_PENDING_UPDATE_FILE_PATH).apply();
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
            if (!new File(pendingUpdateFilePath).delete()) {
                Log.d(TAG, "Failed to delete file " + pendingUpdateFilePath);
            }
        });
    }

    /**
     * Returns whether a check for whether the Web Manifest needs to be updated has occurred in the
     * last {@link numMillis} milliseconds.
     */
    boolean wasCheckForUpdatesDoneInLastMs(long numMillis) {
        return (sClock.currentTimeMillis() - getLastCheckForWebManifestUpdateTimeMs()) < numMillis;
    }

    /** Returns whether we should check for update. */
    boolean shouldCheckForUpdate() {
        long checkUpdatesInterval =
                shouldRelaxUpdates() ? RELAXED_UPDATE_INTERVAL : UPDATE_INTERVAL;
        long now = sClock.currentTimeMillis();
        long sinceLastCheckDurationMs = now - getLastCheckForWebManifestUpdateTimeMs();
        if (sinceLastCheckDurationMs >= checkUpdatesInterval) return true;

        long sinceLastUpdateRequestDurationMs = now - getLastWebApkUpdateRequestCompletionTimeMs();
        return sinceLastUpdateRequestDurationMs >= RETRY_UPDATE_DURATION
                && !didPreviousUpdateSucceed();
    }

    protected WebappDataStorage(String webappId) {
        mId = webappId;
        mPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                SHARED_PREFS_FILE_PREFIX + webappId, Context.MODE_PRIVATE);
    }
}
