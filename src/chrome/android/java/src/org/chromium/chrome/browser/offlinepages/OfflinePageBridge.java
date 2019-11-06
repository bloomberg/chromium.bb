// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Access gate to C++ side offline pages functionalities.
 */
@JNINamespace("offline_pages::android")
public class OfflinePageBridge {
    // These constants must be kept in sync with the constants defined in
    // //components/offline_pages/core/client_namespace_constants.cc
    public static final String ASYNC_NAMESPACE = "async_loading";
    public static final String BOOKMARK_NAMESPACE = "bookmark";
    public static final String LAST_N_NAMESPACE = "last_n";
    public static final String SHARE_NAMESPACE = "share";
    public static final String CCT_NAMESPACE = "custom_tabs";
    public static final String DOWNLOAD_NAMESPACE = "download";
    public static final String NTP_SUGGESTIONS_NAMESPACE = "ntp_suggestions";
    public static final String SUGGESTED_ARTICLES_NAMESPACE = "suggested_articles";
    public static final String BROWSER_ACTIONS_NAMESPACE = "browser_actions";
    public static final String LIVE_PAGE_SHARING_NAMESPACE = "live_page_sharing";

    private long mNativeOfflinePageBridge;
    private boolean mIsNativeOfflinePageModelLoaded;
    private final ObserverList<OfflinePageModelObserver> mObservers = new ObserverList<>();

    /**
     * Retrieves the OfflinePageBridge for the given profile, creating it the first time
     * getForProfile or getForProfileKey is called for the profile.  Must be called on the UI
     * thread.
     *
     * @param profile The profile associated with the OfflinePageBridge to get.
     */
    public static OfflinePageBridge getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();

        if (profile == null)
            return null;

        return getForProfileKey(profile.getProfileKey());
    }

    /**
     * Retrieves the OfflinePageBridge for the profile with the given key, creating it the first
     * time getForProfile or getForProfileKey is called for the profile.  Must be called on the UI
     * thread.
     *
     * @param profileKey Key of the profile associated with the OfflinePageBridge to get.
     */
    public static OfflinePageBridge getForProfileKey(ProfileKey profileKey) {
        ThreadUtils.assertOnUiThread();

        return nativeGetOfflinePageBridgeForProfileKey(profileKey);
    }

    /**
     * Callback used when saving an offline page.
     */
    public interface SavePageCallback {
        /**
         * Delivers result of saving a page.
         *
         * @param savePageResult Result of the saving. Uses {@see
         *         org.chromium.components.offlinepages.SavePageResult} enum.
         * @param url URL of the saved page.
         * @see OfflinePageBridge#savePage(WebContents, ClientId, OfflinePageOrigin,
         *         SavePageCallback)
         */
        @CalledByNative("SavePageCallback")
        void onSavePageDone(int savePageResult, String url, long offlineId);
    }

    /**
     * Base observer class listeners to be notified of changes to the offline page model.
     */
    public abstract static class OfflinePageModelObserver {
        /**
         * Called when the native side of offline pages is loaded and now in usable state.
         */
        public void offlinePageModelLoaded() {}

        /**
         * Called when the native side of offline pages is changed due to adding, removing or
         * update an offline page.
         */
        public void offlinePageAdded(OfflinePageItem addedPage) {}

        /**
         * Called when an offline page is deleted. This can be called as a result of
         * #checkOfflinePageMetadata().
         * @param deletedPage Info about the deleted offline page.
         */
        public void offlinePageDeleted(DeletedPageInfo deletedPage) {}
    }

    /**
     * Creates an offline page bridge for a given profile.
     */
    @VisibleForTesting
    protected OfflinePageBridge(long nativeOfflinePageBridge) {
        mNativeOfflinePageBridge = nativeOfflinePageBridge;
    }

    /**
     * Called by the native OfflinePageBridge so that it can cache the new Java OfflinePageBridge.
     */
    @CalledByNative
    private static OfflinePageBridge create(long nativeOfflinePageBridge) {
        return new OfflinePageBridge(nativeOfflinePageBridge);
    }

    /**
     * @return True if an offline copy of the given URL can be saved.
     */
    public static boolean canSavePage(String url) {
        return nativeCanSavePage(url);
    }

    /**
     * @return the string representing the origin of the tab.
     */
    @CalledByNative
    private static String getEncodedOriginApp(Tab tab) {
        return new OfflinePageOrigin(ContextUtils.getApplicationContext(), tab)
                .encodeAsJsonString();
    }

    /**
     * Adds an observer to offline page model changes.
     * @param observer The observer to be added.
     */
    public void addObserver(OfflinePageModelObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to offline page model changes.
     * @param observer The observer to be removed.
     */
    public void removeObserver(OfflinePageModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Gets all available offline pages, returning results via the provided callback.
     *
     * @param callback The callback to run when the operation completes.
     */
    @VisibleForTesting
    public void getAllPages(final Callback<List<OfflinePageItem>> callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        nativeGetAllPages(mNativeOfflinePageBridge, result, callback);
    }

    /**
     * Gets the offline pages associated with the provided client IDs.
     *
     * @param clientIds Client's IDs associated with offline pages.
     * @param callback The callback to run when the response is ready. It will be passed a list of
     *         {@link OfflinePageItem} matching the provided IDs, or an empty list if none exist.
     */
    @VisibleForTesting
    public void getPagesByClientIds(
            final List<ClientId> clientIds, final Callback<List<OfflinePageItem>> callback) {
        String[] namespaces = new String[clientIds.size()];
        String[] ids = new String[clientIds.size()];

        for (int i = 0; i < clientIds.size(); i++) {
            namespaces[i] = clientIds.get(i).getNamespace();
            ids[i] = clientIds.get(i).getId();
        }

        List<OfflinePageItem> result = new ArrayList<>();
        nativeGetPagesByClientId(mNativeOfflinePageBridge, result, namespaces, ids, callback);
    }

    /**
     * Gets the offline pages associated with the provided origin.
     * @param origin The JSON-like string of the app's package name and encrypted signature hash.
     * @param callback The callback to run when the response is ready. It will be passed a list of
     *         {@link OfflinePageItem} matching the provided origin, or an empty list if none exist.
     */
    public void getPagesByRequestOrigin(String origin, Callback<List<OfflinePageItem>> callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        nativeGetPagesByRequestOrigin(mNativeOfflinePageBridge, result, origin, callback);
    }

    /**
     * Gets the offline pages associated with the provided namespace.
     *
     * @param namespace The string form of the namespace to query.
     * @param callback The callback to run when the response is ready. It will be passed a list of
     *         {@link OfflinePageItem} matching the provided namespace, or an empty list if none
     * exist.
     */
    public void getPagesByNamespace(
            final String namespace, final Callback<List<OfflinePageItem>> callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        nativeGetPagesByNamespace(mNativeOfflinePageBridge, result, namespace, callback);
    }

    /**
     * Get the offline page associated with the provided offline URL.
     *
     * @param onlineUrl URL of the page.
     * @param tabId Android tab ID.
     * @param callback callback to pass back the matching {@link OfflinePageItem} if found. Will
     *         pass back null if not.
     */
    public void selectPageForOnlineUrl(String onlineUrl, int tabId,
            Callback<OfflinePageItem> callback) {
        nativeSelectPageForOnlineUrl(mNativeOfflinePageBridge, onlineUrl, tabId, callback);
    }

    /**
     * Get the offline page associated with the provided offline ID.
     *
     * @param offlineId ID of the offline page.
     * @param callback callback to pass back the matching {@link OfflinePageItem} if found. Will
     *         pass back <code>null</code> if not.
     */
    public void getPageByOfflineId(final long offlineId, final Callback<OfflinePageItem> callback) {
        nativeGetPageByOfflineId(mNativeOfflinePageBridge, offlineId, callback);
    }

    /**
     * Saves the web page loaded into web contents offline.
     * Retrieves the origin of the page from the WebContents.
     *
     * @param webContents Contents of the page to save.
     * @param clientId Client ID of the bookmark related to the offline page.
     * @param callback Interface that contains a callback. This may be called synchronously, e.g. if
     *         the web contents is already destroyed.
     * @see SavePageCallback
     */
    public void savePage(final WebContents webContents, final ClientId clientId,
            final SavePageCallback callback) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        OfflinePageOrigin origin;
        if (activity != null && activity.getActivityTab() != null) {
            origin = new OfflinePageOrigin(
                    ContextUtils.getApplicationContext(), activity.getActivityTab());
        } else {
            origin = new OfflinePageOrigin();
        }
        savePage(webContents, clientId, origin, callback);
    }

    /**
     * Saves the web page loaded into web contents offline.
     *
     * @param webContents Contents of the page to save.
     * @param clientId Client ID of the bookmark related to the offline page.
     * @param origin The app that initiated the download.
     * @param callback Interface that contains a callback. This may be called synchronously, e.g. if
     *         the web contents is already destroyed.
     * @see SavePageCallback
     */
    public void savePage(final WebContents webContents, final ClientId clientId,
            final OfflinePageOrigin origin, final SavePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        assert webContents != null;
        assert origin != null;

        nativeSavePage(mNativeOfflinePageBridge, callback, webContents, clientId.getNamespace(),
                clientId.getId(), origin.encodeAsJsonString());
    }

    /**
     * Deletes an offline page related to a specified bookmark.
     *
     * @param clientId Client ID for which the offline copy will be deleted.
     * @param callback Interface that contains a callback.
     */
    @VisibleForTesting
    public void deletePage(final ClientId clientId, Callback<Integer> callback) {
        assert mIsNativeOfflinePageModelLoaded;
        ArrayList<ClientId> ids = new ArrayList<ClientId>();
        ids.add(clientId);

        deletePagesByClientId(ids, callback);
    }

    /**
     * Deletes offline pages based on the list of provided client IDs. Calls the callback
     * when operation is complete. Requires that the model is already loaded.
     *
     * @param clientIds A list of Client IDs for which the offline pages will be deleted.
     * @param callback A callback that will be called once operation is completed.
     */
    public void deletePagesByClientId(List<ClientId> clientIds, Callback<Integer> callback) {
        String[] namespaces = new String[clientIds.size()];
        String[] ids = new String[clientIds.size()];

        for (int i = 0; i < clientIds.size(); i++) {
            namespaces[i] = clientIds.get(i).getNamespace();
            ids[i] = clientIds.get(i).getId();
        }

        nativeDeletePagesByClientId(mNativeOfflinePageBridge, namespaces, ids, callback);
    }

    /**
     * Deletes offline pages based on the list of provided client IDs only if they originate
     * from the same origin. Calls the callback when operation is complete. Requires that the
     * model is already loaded.
     *
     * @param clientIds A list of Client IDs for which the offline pages will be deleted.
     * @param callback A callback that will be called once operation is completed.
     */
    public void deletePagesByClientIdAndOrigin(
            List<ClientId> clientIds, OfflinePageOrigin origin, Callback<Integer> callback) {
        String[] namespaces = new String[clientIds.size()];
        String[] ids = new String[clientIds.size()];

        for (int i = 0; i < clientIds.size(); i++) {
            namespaces[i] = clientIds.get(i).getNamespace();
            ids[i] = clientIds.get(i).getId();
        }

        nativeDeletePagesByClientIdAndOrigin(
                mNativeOfflinePageBridge, namespaces, ids, origin.encodeAsJsonString(), callback);
    }

    /**
     * Deletes offline pages based on the list of offline IDs. Calls the callback
     * when operation is complete. Note that offline IDs are not intended to be saved across
     * restarts of Chrome; they should be obtained by querying the model for the appropriate client
     * ID.
     *
     * @param offlineIdList A list of offline IDs of pages that will be deleted.
     * @param callback A callback that will be called once operation is completed, called with the
     *         DeletePageResult of the operation..
     */
    public void deletePagesByOfflineId(List<Long> offlineIdList, Callback<Integer> callback) {
        if (offlineIdList == null) {
            callback.onResult(Integer.valueOf(DeletePageResult.SUCCESS));
            return;
        }

        long[] offlineIds = new long[offlineIdList.size()];
        for (int i = 0; i < offlineIdList.size(); i++) {
            offlineIds[i] = offlineIdList.get(i).longValue();
        }
        nativeDeletePagesByOfflineId(mNativeOfflinePageBridge, offlineIds, callback);
    }

    /**
     * Ask the native code to publish the internal page asychronously.
     * @param offlineId ID of the offline page to publish.
     * @param publishedCallback Function to call when publishing is done.  This will be called with
     *         the new path of the file.
     */
    public void publishInternalPageByOfflineId(long offlineId, Callback<String> publishedCallback) {
        nativePublishInternalPageByOfflineId(
                mNativeOfflinePageBridge, offlineId, publishedCallback);
    }

    /**
     * Ask the native code to publish the internal page asychronously.
     * @param guid Client ID of the offline page to publish.
     * @param publishedCallback Function to call when publishing is done.
     */
    public void publishInternalPageByGuid(String guid, Callback<String> publishedCallback) {
        nativePublishInternalPageByGuid(mNativeOfflinePageBridge, guid, publishedCallback);
    }

    /**
     * Whether or not the underlying offline page model is loaded.
     */
    public boolean isOfflinePageModelLoaded() {
        return mIsNativeOfflinePageModelLoaded;
    }

    /**
     * Retrieves the extra request header to reload the offline page.
     * @param webContents Contents of the page to reload.
     * @return The extra request header string.
     */
    public String getOfflinePageHeaderForReload(WebContents webContents) {
        return nativeGetOfflinePageHeaderForReload(mNativeOfflinePageBridge, webContents);
    }

    /**
     * @param webContents Contents of the page to check.
     * @return True if an offline preview is being shown.
     */
    public boolean isShowingOfflinePreview(WebContents webContents) {
        return nativeIsShowingOfflinePreview(mNativeOfflinePageBridge, webContents);
    }

    /**
     * @param webContents Contents of the page to check.
     * @return True if download button is being shown in the error page.
     */
    public boolean isShowingDownloadButtonInErrorPage(WebContents webContents) {
        return nativeIsShowingDownloadButtonInErrorPage(mNativeOfflinePageBridge, webContents);
    }

    /** Tells the native side that the tab of |webContents| will be closed. */
    void willCloseTab(WebContents webContents) {
        nativeWillCloseTab(mNativeOfflinePageBridge, webContents);
    }

    /**
     * Schedules to download a page from |url| and categorize under |nameSpace|.
     * The duplicate pages or requests will be checked.
     * Origin is presumed to be Chrome.
     *
     * @param webContents Web contents upon which the infobar is shown.
     * @param nameSpace Namespace of the page to save.
     * @param url URL of the page to save.
     * @param uiAction UI action, like showing infobar or toast on certain case.
     */
    public void scheduleDownload(
            WebContents webContents, String nameSpace, String url, int uiAction) {
        scheduleDownload(webContents, nameSpace, url, uiAction, new OfflinePageOrigin());
    }

    /**
     * Schedules to download a page from |url| and categorize under |namespace| from |origin|.
     * The duplicate pages or requests will be checked.
     *
     * @param webContents Web contents upon which the infobar is shown.
     * @param nameSpace Namespace of the page to save.
     * @param url URL of the page to save.
     * @param uiAction UI action, like showing infobar or toast on certain case.
     * @param origin Origin of the page.
     */
    public void scheduleDownload(WebContents webContents, String nameSpace, String url,
            int uiAction, OfflinePageOrigin origin) {
        nativeScheduleDownload(mNativeOfflinePageBridge, webContents, nameSpace, url, uiAction,
                origin.encodeAsJsonString());
    }

    /**
     * Checks if an offline page is shown for the webContents.
     * @param webContents Web contents used to find the offline page.
     * @return True if the offline page is opened.
     */
    public boolean isOfflinePage(WebContents webContents) {
        return nativeIsOfflinePage(mNativeOfflinePageBridge, webContents);
    }

    /**
     * Returns whether |nameSpace| is a temporary namespace.
     * @param nameSpace Namespace of the page in question.
     * @return true if the page is in a temporary namespace.
     */
    public boolean isTemporaryNamespace(String nameSpace) {
        return nativeIsTemporaryNamespace(mNativeOfflinePageBridge, nameSpace);
    }

    /**
     * Checks if the supplied file path is in a private dir internal to chrome.
     * @param filePath Path of the file to check.
     * @return True if the file is in a private directory.
     */
    public boolean isInPrivateDirectory(String filePath) {
        return nativeIsInPrivateDirectory(mNativeOfflinePageBridge, filePath);
    }

    /**
     * Retrieves the offline page that is shown for the tab.
     * @param webContents Web contents used to find the offline page.
     * @return The offline page if tab currently displays it, null otherwise.
     */
    @Nullable
    public OfflinePageItem getOfflinePage(WebContents webContents) {
        return nativeGetOfflinePage(mNativeOfflinePageBridge, webContents);
    }

    /**
     * Queries the model for offline content that's been added since the given timestamp.
     * @param freshnessTimeMillis Returned content must be newer than |timestamp|, a date
     *         represented as the number of millis since the Java epoch.
     * @param callback Fired when the model check has been finished, with a String parameter that
     *         represents the source of the offline content.  The parameter will be the empty string
     *         if no fresh enough content is found.
     */
    public void checkForNewOfflineContent(long freshnessTimeMillis, Callback<String> callback) {
        nativeCheckForNewOfflineContent(mNativeOfflinePageBridge, freshnessTimeMillis, callback);
    }

    /**
     * Get the the url params to open the offline page associated with the provided offline ID.
     * Depending on whether it is trusted or not, either http/https or file URL will be returned in
     * the callback.
     *
     * @param offlineId ID of the offline page.
     * @param location Where the offline page is launched.
     * @param callback callback to pass back the url string if found. Will pass back
     *         <code>null</code> if not.
     */
    public void getLoadUrlParamsByOfflineId(
            long offlineId, @LaunchLocation int location, Callback<LoadUrlParams> callback) {
        nativeGetLoadUrlParamsByOfflineId(mNativeOfflinePageBridge, offlineId, location, callback);
    }

    /**
     * Get the url params to open the intent carrying MHTML file or content.
     *
     * @param url The file:// or content:// URL.
     * @param callback Callback to pass back the url params.
     */
    public void getLoadUrlParamsForOpeningMhtmlFileOrContent(
            String url, Callback<LoadUrlParams> callback) {
        nativeGetLoadUrlParamsForOpeningMhtmlFileOrContent(mNativeOfflinePageBridge, url, callback);
    }

    /**
     * Checks if the web contents is showing a trusted offline page.
     * @param webContents Web contents shown.
     * @return True if a trusted offline page is shown.
     */
    public boolean isShowingTrustedOfflinePage(WebContents webContents) {
        return nativeIsShowingTrustedOfflinePage(mNativeOfflinePageBridge, webContents);
    }

    /**
     * Tries to acquire the storage access permssion if not yet.
     *
     * @param webContents Contents of the page to check.
     * @param callback Callback to notify the result.
     */
    public void acquireFileAccessPermission(WebContents webContents, Callback<Boolean> callback) {
        nativeAcquireFileAccessPermission(mNativeOfflinePageBridge, webContents, callback);
    }

    @CalledByNative
    protected void offlinePageModelLoaded() {
        mIsNativeOfflinePageModelLoaded = true;
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageModelLoaded();
        }
    }

    @CalledByNative
    protected void offlinePageAdded(OfflinePageItem addedPage) {
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageAdded(addedPage);
        }
    }

    /**
     * Removes references to the native OfflinePageBridge when it is being destroyed.
     */
    @CalledByNative
    protected void offlinePageBridgeDestroyed() {
        ThreadUtils.assertOnUiThread();
        assert mNativeOfflinePageBridge != 0;

        mIsNativeOfflinePageModelLoaded = false;
        mNativeOfflinePageBridge = 0;

        // TODO(dewittj): Add a model destroyed method to the observer interface.
        mObservers.clear();
    }

    @CalledByNative
    void offlinePageDeleted(DeletedPageInfo deletedPage) {
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageDeleted(deletedPage);
        }
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(List<OfflinePageItem> offlinePagesList,
            String url, long offlineId, String clientNamespace, String clientId, String title,
            String filePath, long fileSize, long creationTime, int accessCount,
            long lastAccessTimeMs, String requestOrigin) {
        offlinePagesList.add(createOfflinePageItem(url, offlineId, clientNamespace, clientId, title,
                filePath, fileSize, creationTime, accessCount, lastAccessTimeMs, requestOrigin));
    }

    @CalledByNative
    private static OfflinePageItem createOfflinePageItem(String url, long offlineId,
            String clientNamespace, String clientId, String title, String filePath, long fileSize,
            long creationTime, int accessCount, long lastAccessTimeMs, String requestOrigin) {
        return new OfflinePageItem(url, offlineId, clientNamespace, clientId, title, filePath,
                fileSize, creationTime, accessCount, lastAccessTimeMs, requestOrigin);
    }

    @CalledByNative
    private static ClientId createClientId(String clientNamespace, String id) {
        return new ClientId(clientNamespace, id);
    }

    @CalledByNative
    private static DeletedPageInfo createDeletedPageInfo(
            long offlineId, String clientNamespace, String clientId, String requestOrigin) {
        return new DeletedPageInfo(offlineId, clientNamespace, clientId, requestOrigin);
    }

    @CalledByNative
    private static LoadUrlParams createLoadUrlParams(
            String url, String extraHeaderKey, String extraHeaderValue) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        if (!TextUtils.isEmpty(extraHeaderKey) && !TextUtils.isEmpty(extraHeaderValue)) {
            // Set both map-based and collapsed headers to support all use scenarios.
            Map<String, String> headers = new HashMap<String, String>();
            headers.put(extraHeaderKey, extraHeaderValue);
            loadUrlParams.setExtraHeaders(headers);
            loadUrlParams.setVerbatimHeaders(extraHeaderKey + ": " + extraHeaderValue);
        }
        return loadUrlParams;
    }

    private static native boolean nativeCanSavePage(String url);
    private static native OfflinePageBridge nativeGetOfflinePageBridgeForProfileKey(
            ProfileKey profileKey);
    @VisibleForTesting
    native void nativeGetAllPages(long nativeOfflinePageBridge, List<OfflinePageItem> offlinePages,
            final Callback<List<OfflinePageItem>> callback);
    private native void nativeWillCloseTab(long nativeOfflinePageBridge, WebContents webContents);

    @VisibleForTesting
    native void nativeGetPageByOfflineId(
            long nativeOfflinePageBridge, long offlineId, Callback<OfflinePageItem> callback);
    @VisibleForTesting
    native void nativeGetPagesByClientId(long nativeOfflinePageBridge, List<OfflinePageItem> result,
            String[] namespaces, String[] ids, Callback<List<OfflinePageItem>> callback);
    native void nativeGetPagesByRequestOrigin(long nativeOfflinePageBridge,
            List<OfflinePageItem> result, String requestOrigin,
            Callback<List<OfflinePageItem>> callback);
    native void nativeGetPagesByNamespace(long nativeOfflinePageBridge,
            List<OfflinePageItem> result, String nameSpace,
            Callback<List<OfflinePageItem>> callback);
    @VisibleForTesting
    native void nativeDeletePagesByClientId(long nativeOfflinePageBridge, String[] namespaces,
            String[] ids, Callback<Integer> callback);
    native void nativeDeletePagesByClientIdAndOrigin(long nativeOfflinePageBridge,
            String[] namespaces, String[] ids, String origin, Callback<Integer> callback);
    @VisibleForTesting
    native void nativeDeletePagesByOfflineId(
            long nativeOfflinePageBridge, long[] offlineIds, Callback<Integer> callback);
    @VisibleForTesting
    private native void nativePublishInternalPageByOfflineId(
            long nativeOfflinePageBridge, long offlineId, Callback<String> publishedCallback);
    @VisibleForTesting
    private native void nativePublishInternalPageByGuid(
            long nativeOfflinePageBridge, String guid, Callback<String> publishedCallback);
    private native void nativeSelectPageForOnlineUrl(
            long nativeOfflinePageBridge, String onlineUrl, int tabId,
            Callback<OfflinePageItem> callback);
    private native void nativeSavePage(long nativeOfflinePageBridge, SavePageCallback callback,
            WebContents webContents, String clientNamespace, String clientId, String origin);
    private native String nativeGetOfflinePageHeaderForReload(
            long nativeOfflinePageBridge, WebContents webContents);
    private native boolean nativeIsShowingOfflinePreview(
            long nativeOfflinePageBridge, WebContents webContents);
    private native boolean nativeIsShowingDownloadButtonInErrorPage(
            long nativeOfflinePageBridge, WebContents webContents);
    private native void nativeScheduleDownload(long nativeOfflinePageBridge,
            WebContents webContents, String nameSpace, String url, int uiAction, String origin);
    private native boolean nativeIsOfflinePage(
            long nativeOfflinePageBridge, WebContents webContents);
    private native boolean nativeIsInPrivateDirectory(
            long nativeOfflinePageBridge, String filePath);
    private native boolean nativeIsTemporaryNamespace(
            long nativeOfflinePageBridge, String nameSpace);
    private native OfflinePageItem nativeGetOfflinePage(
            long nativeOfflinePageBridge, WebContents webContents);
    private native void nativeCheckForNewOfflineContent(
            long nativeOfflinePageBridge, long freshnessTimeMillis, Callback<String> callback);
    private native void nativeGetLoadUrlParamsByOfflineId(long nativeOfflinePageBridge,
            long offlineId, int location, Callback<LoadUrlParams> callback);
    private native boolean nativeIsShowingTrustedOfflinePage(
            long nativeOfflinePageBridge, WebContents webContents);
    private native void nativeGetLoadUrlParamsForOpeningMhtmlFileOrContent(
            long nativeOfflinePageBridge, String url, Callback<LoadUrlParams> callback);
    private native void nativeAcquireFileAccessPermission(
            long nativeOfflinePageBridge, WebContents webContents, Callback<Boolean> callback);
}
