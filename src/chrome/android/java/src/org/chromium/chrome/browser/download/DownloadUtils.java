// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeIntentUtil;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.offlinepages.DownloadUiActionFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.components.download.DownloadState;
import org.chromium.components.download.ResumeMode;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.io.File;

/**
 * A class containing some utility static methods.
 */
public class DownloadUtils {
    private static final String TAG = "download";

    private static final String EXTRA_IS_OFF_THE_RECORD =
            "org.chromium.chrome.browser.download.IS_OFF_THE_RECORD";
    private static final String MIME_TYPE_ZIP = "application/zip";
    private static final String DOCUMENTS_UI_PACKAGE_NAME = "com.android.documentsui";
    public static final String EXTRA_SHOW_PREFETCHED_CONTENT =
            "org.chromium.chrome.browser.download.SHOW_PREFETCHED_CONTENT";

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @param activity The current activity is available.
     * @param tab The current tab if it exists.
     * @param source The source where the user action is coming from.
     * @return Whether the UI was shown.
     */
    public static boolean showDownloadManager(
            @Nullable Activity activity, @Nullable Tab tab, @DownloadOpenSource int source) {
        return showDownloadManager(activity, tab, source, false);
    }

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @param activity The current activity is available.
     * @param tab The current tab if it exists.
     * @param source The source where the user action is coming from.
     * @param showPrefetchedContent Whether the manager should start with prefetched content section
     * expanded.
     * @return Whether the UI was shown.
     */
    @CalledByNative
    public static boolean showDownloadManager(@Nullable Activity activity, @Nullable Tab tab,
            @DownloadOpenSource int source, boolean showPrefetchedContent) {
        // Figure out what tab was last being viewed by the user.
        if (activity == null) activity = ApplicationStatus.getLastTrackedFocusedActivity();
        Context appContext = ContextUtils.getApplicationContext();
        boolean isTablet;

        if (tab == null && activity instanceof ChromeTabbedActivity) {
            ChromeTabbedActivity chromeActivity = ((ChromeTabbedActivity) activity);
            tab = chromeActivity.getActivityTab();
            isTablet = chromeActivity.isTablet();
        } else {
            Context displayContext = activity != null ? activity : appContext;
            isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(displayContext);
        }

        if (isTablet) {
            // Download Home shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.DOWNLOADS_URL);
            if (tab == null || !tab.isInitialized()) {
                // Open a new tab, which pops Chrome into the foreground.
                TabDelegate delegate = new TabDelegate(false);
                delegate.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            } else {
                // Download Home shows up inside an existing tab, but only if the last Activity was
                // the ChromeTabbedActivity.
                tab.loadUrl(params);

                // Bring Chrome to the foreground, if possible.
                Intent intent = ChromeIntentUtil.createBringTabToFrontIntent(tab.getId());
                if (intent != null) {
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    IntentUtils.safeStartActivity(appContext, intent);
                }
            }
        } else {
            // Download Home shows up as a new Activity on phones.
            Intent intent = new Intent();
            intent.setClass(appContext, DownloadActivity.class);
            intent.putExtra(EXTRA_SHOW_PREFETCHED_CONTENT, showPrefetchedContent);
            if (tab != null) intent.putExtra(EXTRA_IS_OFF_THE_RECORD, tab.isIncognito());
            if (activity == null) {
                // Stands alone in its own task.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                appContext.startActivity(intent);
            } else {
                // Sits on top of another Activity.
                intent.addFlags(
                        Intent.FLAG_ACTIVITY_MULTIPLE_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
                activity.startActivity(intent);
            }
        }

        if (BrowserStartupController.getInstance().isFullBrowserStarted()) {
            // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
            // incognito profile) instead of always using regular profile. It works correctly now,
            // but it is not safe.
            Profile profile = (tab == null ? Profile.getLastUsedRegularProfile()
                                           : Profile.fromWebContents(tab.getWebContents()));
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            tracker.notifyEvent(EventConstants.DOWNLOAD_HOME_OPENED);
        }
        DownloadMetrics.recordDownloadPageOpen(source);
        return true;
    }

    /**
     * @return Whether or not the Intent corresponds to a DownloadActivity that should show off the
     *         record downloads.
     */
    public static boolean shouldShowOffTheRecordDownloads(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false);
    }

    /**
     * @return Whether or not the prefetched content section should be expanded on launch of the
     * DownloadActivity.
     */
    public static boolean shouldShowPrefetchContent(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_SHOW_PREFETCHED_CONTENT, false);
    }

    /**
     * @return Whether or not pagination headers should be shown on download home.
     */
    public static boolean shouldShowPaginationHeaders() {
        return AccessibilityUtil.isAccessibilityEnabled()
                || AccessibilityUtil.isHardwareKeyboardAttached(
                        ContextUtils.getApplicationContext().getResources().getConfiguration());
    }

    /**
     * Records metrics related to downloading a page. Should be called after a tap on the download
     * page button.
     * @param tab The Tab containing the page being downloaded.
     */
    public static void recordDownloadPageMetrics(Tab tab) {
        RecordHistogram.recordPercentageHistogram(
                "OfflinePages.SavePage.PercentLoaded", Math.round(tab.getProgress() * 100));
    }

    /**
     * Shows a "Downloading..." toast. Should be called after a download has been started.
     * @param context The {@link Context} used to make the toast.
     */
    public static void showDownloadStartToast(Context context) {
        Toast.makeText(context, R.string.download_started, Toast.LENGTH_SHORT).show();
    }

    /**
     * Issues a request to the {@link DownloadManagerService} associated to check for externally
     * removed downloads.
     * See {@link DownloadManagerService#checkForExternallyRemovedDownloads}.
     * @param isOffTheRecord  Whether to check downloads for the off the record profile.
     */
    public static void checkForExternallyRemovedDownloads(boolean isOffTheRecord) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)) {
            return;
        }

        if (isOffTheRecord) {
            DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                    true);
        }
        DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                false);
        RecordUserAction.record(
                "Android.DownloadManager.CheckForExternallyRemovedItems");
    }

    /**
     * Trigger the download of an Offline Page.
     * @param context Context to pull resources from.
     */
    public static void downloadOfflinePage(Context context, Tab tab) {
        OfflinePageOrigin origin = new OfflinePageOrigin(context, tab);

        if (tab.isShowingErrorPage()) {
            // The download needs to be scheduled to happen at later time due to current network
            // error.
            final OfflinePageBridge bridge =
                    OfflinePageBridge.getForProfile(Profile.fromWebContents(tab.getWebContents()));
            bridge.scheduleDownload(tab.getWebContents(), OfflinePageBridge.ASYNC_NAMESPACE,
                    tab.getUrlString(), DownloadUiActionFlags.PROMPT_DUPLICATE, origin);
        } else {
            // Otherwise, the download can be started immediately.
            OfflinePageDownloadBridge.startDownload(tab, origin);
            DownloadUtils.recordDownloadPageMetrics(tab);
        }

        Tracker tracker =
                TrackerFactory.getTrackerForProfile(Profile.fromWebContents(tab.getWebContents()));
        tracker.notifyEvent(EventConstants.DOWNLOAD_PAGE_STARTED);
    }

    /**
     * Whether the user should be allowed to download the current page.
     * @param tab Tab displaying the page that will be downloaded.
     * @return    Whether the "Download Page" button should be enabled.
     */
    public static boolean isAllowedToDownloadPage(Tab tab) {
        if (tab == null) return false;

        // Offline pages isn't supported in Incognito. This should be checked before calling
        // OfflinePageBridge.getForProfile because OfflinePageBridge instance will not be found
        // for incognito profile.
        if (tab.isIncognito()) return false;

        // Check if the page url is supported for saving. Only HTTP and HTTPS pages are allowed.
        if (!OfflinePageBridge.canSavePage(tab.getUrlString())) return false;

        // Download will only be allowed for the error page if download button is shown in the page.
        if (tab.isShowingErrorPage()) {
            final OfflinePageBridge bridge =
                    OfflinePageBridge.getForProfile(Profile.fromWebContents(tab.getWebContents()));
            return bridge.isShowingDownloadButtonInErrorPage(tab.getWebContents());
        }

        if (((TabImpl) tab).isShowingInterstitialPage()) return false;

        // Don't allow re-downloading the currently displayed offline page.
        if (OfflinePageUtils.isOfflinePage(tab)) return false;

        return true;
    }

    /**
     * Returns a URI that points at the file.
     * @param filePath File path to get a URI for.
     * @return URI that points at that file, either as a content:// URI or a file:// URI.
     */
    @MainThread
    public static Uri getUriForItem(String filePath) {
        if (ContentUriUtils.isContentUri(filePath)) return Uri.parse(filePath);

        // It's ok to use blocking calls on main thread here, since the user is waiting to open or
        // share the file to other apps.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        Uri uri = null;

        try {
            boolean isOnSDCard = DownloadDirectoryProvider.isDownloadOnSDCard(filePath);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_FILE_PROVIDER)
                    && isOnSDCard) {
                // Use custom file provider to generate content URI for download on SD card.
                uri = DownloadFileProvider.createContentUri(filePath);
            } else {
                // Use FileProvider to generate content URI or file URI.
                uri = FileUtils.getUriForFile(new File(filePath));
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        return uri;
    }

    /**
     * Get the URI when shared or opened by other apps.
     *
     * @param filePath Downloaded file path.
     * @return URI for other apps to use the file via {@link android.content.ContentResolver}.
     */
    public static Uri getUriForOtherApps(String filePath) {
        // Some old Samsung devices with Android M- must use file URI. See https://crbug.com/705748.
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.M ? getUriForItem(filePath)
                                                             : Uri.fromFile(new File(filePath));
    }

    @CalledByNative
    private static String getUriStringForPath(String filePath) {
        if (ContentUriUtils.isContentUri(filePath)) return filePath;
        Uri uri = getUriForItem(filePath);
        return uri != null ? uri.toString() : new String();
    }

    /**
     * Utility method to open an {@link OfflineItem}, which can be a chrome download, offline page.
     * Falls back to open download home.
     * @param contentId The {@link ContentId} of the associated offline item.
     * @param isOffTheRecord Whether the download should be opened in incognito mode.
     * @param source The location from which the download was opened.
     */
    public static void openItem(
            ContentId contentId, boolean isOffTheRecord, @DownloadOpenSource int source) {
        if (LegacyHelpers.isLegacyAndroidDownload(contentId)) {
            ContextUtils.getApplicationContext().startActivity(
                    new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS)
                            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } else if (LegacyHelpers.isLegacyOfflinePage(contentId)) {
            OpenParams openParams = new OpenParams(LaunchLocation.PROGRESS_BAR);
            openParams.openInIncognito = isOffTheRecord;
            OfflineContentAggregatorFactory.get().openItem(openParams, contentId);
        } else {
            DownloadManagerService.getDownloadManagerService().openDownload(
                    contentId, isOffTheRecord, source);
        }
    }

    /**
     * Opens a file in Chrome or in another app if appropriate.
     * @param filePath Path to the file to open, can be a content Uri.
     * @param mimeType mime type of the file.
     * @param downloadGuid The associated download GUID.
     * @param isOffTheRecord whether we are in an off the record context.
     * @param originalUrl The original url of the downloaded file.
     * @param referrer Referrer of the downloaded file.
     * @param source The source that tries to open the download file.
     * @return whether the file could successfully be opened.
     */
    public static boolean openFile(String filePath, String mimeType, String downloadGuid,
            boolean isOffTheRecord, String originalUrl, String referrer,
            @DownloadOpenSource int source) {
        DownloadMetrics.recordDownloadOpen(source, mimeType);
        Context context = ContextUtils.getApplicationContext();
        DownloadManagerService service = DownloadManagerService.getDownloadManagerService();

        // Check if Chrome should open the file itself.
        if (service.isDownloadOpenableInBrowser(isOffTheRecord, mimeType)) {
            // Share URIs use the content:// scheme when able, which looks bad when displayed
            // in the URL bar.
            Uri contentUri = getUriForItem(filePath);
            Uri fileUri = contentUri;
            if (!ContentUriUtils.isContentUri(filePath)) {
                File file = new File(filePath);
                fileUri = Uri.fromFile(file);
            }
            String normalizedMimeType = Intent.normalizeMimeType(mimeType);

            Intent intent = MediaViewerUtils.getMediaViewerIntent(fileUri /*displayUri*/,
                    contentUri /*contentUri*/, normalizedMimeType,
                    true /* allowExternalAppHandlers */);
            IntentHandler.startActivityForTrustedIntent(intent);
            service.updateLastAccessTime(downloadGuid, isOffTheRecord);
            return true;
        }

        // Check if any apps can open the file.
        try {
            // TODO(qinmin): Move this to an AsyncTask so we don't need to temper with strict mode.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            Uri uri = ContentUriUtils.isContentUri(filePath) ? Uri.parse(filePath)
                                                             : getUriForOtherApps(filePath);
            StrictMode.setThreadPolicy(oldPolicy);
            Intent viewIntent =
                    MediaViewerUtils.createViewIntentForUri(uri, mimeType, originalUrl, referrer);
            context.startActivity(viewIntent);
            service.updateLastAccessTime(downloadGuid, isOffTheRecord);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Cannot start activity to open file", e);
        }

        // If this is a zip file, check if Android Files app exists.
        if (MIME_TYPE_ZIP.equals(mimeType)) {
            try {
                PackageInfo packageInfo = context.getPackageManager().getPackageInfo(
                        DOCUMENTS_UI_PACKAGE_NAME, PackageManager.GET_ACTIVITIES);
                if (packageInfo != null) {
                    Intent viewDownloadsIntent = new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS);
                    viewDownloadsIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    viewDownloadsIntent.setPackage(DOCUMENTS_UI_PACKAGE_NAME);
                    context.startActivity(viewDownloadsIntent);
                    return true;
                }
            } catch (Exception e) {
                Log.e(TAG, "Cannot find files app for openning zip files", e);
            }
        }
        // Can't launch the Intent.
        if (source != DownloadOpenSource.DOWNLOAD_PROGRESS_INFO_BAR) {
            Toast.makeText(context, context.getString(R.string.download_cant_open_file),
                         Toast.LENGTH_SHORT)
                    .show();
        }
        return false;
    }

    @CalledByNative
    private static void openDownload(String filePath, String mimeType, String downloadGuid,
            boolean isOffTheRecord, String originalUrl, String referer,
            @DownloadOpenSource int source) {
        boolean canOpen = DownloadUtils.openFile(
                filePath, mimeType, downloadGuid, isOffTheRecord, originalUrl, referer, source);
        if (!canOpen) {
            DownloadUtils.showDownloadManager(null, null, source);
        }
    }

    /**
     * Fires an Intent to open a downloaded item.
     * @param context Context to use.
     * @param intent  Intent that can be fired.
     * @return Whether an Activity was successfully started for the Intent.
     */
    static boolean fireOpenIntentForDownload(Context context, Intent intent) {
        try {
            if (TextUtils.equals(intent.getPackage(), context.getPackageName())) {
                IntentHandler.startActivityForTrustedIntent(intent);
            } else {
                context.startActivity(intent);
            }
            return true;
        } catch (ActivityNotFoundException ex) {
            Log.d(TAG, "Activity not found for " + intent.getType() + " over "
                    + intent.getData().getScheme(), ex);
        } catch (SecurityException ex) {
            Log.d(TAG, "cannot open intent: " + intent, ex);
        } catch (Exception ex) {
            Log.d(TAG, "cannot open intent: " + intent, ex);
        }

        return false;
    }

    /**
     * Get the resume mode based on the current fail state, to distinguish the case where download
     * cannot be resumed at all or can be resumed in the middle, or should be restarted from the
     * beginning.
     * @param url URL of the download.
     * @param failState Why the download failed.
     * @return The resume mode for the current fail state.
     */
    public static @ResumeMode int getResumeMode(String url, @FailState int failState) {
        return DownloadUtilsJni.get().getResumeMode(url, failState);
    }

    /**
     * Query the Download backends about whether a download is paused.
     *
     * The Java-side contains more information about the status of a download than is persisted
     * by the native backend, so it is queried first.
     *
     * @param item Download to check the status of.
     * @return Whether the download is paused or not.
     */
    public static boolean isDownloadPaused(DownloadItem item) {
        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry =
                helper.getDownloadSharedPreferenceEntry(item.getContentId());

        if (entry != null) {
            // The Java downloads backend knows more about the download than the native backend.
            return !entry.isAutoResumable;
        } else {
            // Only the native downloads backend knows about the download.
            if (item.getDownloadInfo().state() == DownloadState.IN_PROGRESS) {
                return item.getDownloadInfo().isPaused();
            } else {
                return item.getDownloadInfo().state() == DownloadState.INTERRUPTED;
            }
        }
    }

    /**
     * Return whether a download is pending.
     * @param item Download to check the status of.
     * @return Whether the download is pending or not.
     */
    public static boolean isDownloadPending(DownloadItem item) {
        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry =
                helper.getDownloadSharedPreferenceEntry(item.getContentId());
        return entry != null && item.getDownloadInfo().state() == DownloadState.INTERRUPTED
                && entry.isAutoResumable;
    }

    @NativeMethods
    interface Natives {
        int getResumeMode(String url, @FailState int failState);
    }
}
