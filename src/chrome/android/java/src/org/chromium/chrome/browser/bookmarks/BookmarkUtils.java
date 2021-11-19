// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.IncognitoCCTCallerId;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.bottomsheet.BookmarkBottomSheetCoordinator;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** A class holding static util functions for bookmark. */
public class BookmarkUtils {
    private static final String TAG = "BookmarkUtils";

    /**
     * If the tab has already been bookmarked, start {@link BookmarkEditActivity} for the
     * normal bookmark or show the reading list page for reading list bookmark.
     * If not, add the bookmark to {@link BookmarkModel}, and show a snackbar notifying the user.
     *
     * @param existingBookmarkItem The {@link BookmarkItem} if the tab has already been bookmarked.
     * @param bookmarkModel The bookmark model.
     * @param tab The tab to add or edit a bookmark.
     * @param snackbarManager The {@link SnackbarManager} used to show the snackbar.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param activity Current activity.
     * @param fromCustomTab boolean indicates whether it is called by Custom Tab.
     * @param bookmarkType Type of the added bookmark.
     * @param callback Invoked with the resulting bookmark ID, which could be null if unsuccessful.
     */
    public static void addOrEditBookmark(@Nullable BookmarkItem existingBookmarkItem,
            BookmarkModel bookmarkModel, Tab tab, SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController, Activity activity, boolean fromCustomTab,
            @BookmarkType int bookmarkType, Callback<BookmarkId> callback) {
        assert bookmarkModel.isBookmarkModelLoaded();
        if (existingBookmarkItem != null) {
            startEditActivity(activity, existingBookmarkItem.getId());
            callback.onResult(existingBookmarkItem.getId());
            return;
        }

        // TODO(crbug.com/1252228): Reading list support needs some tests.
        if (BookmarkFeatures.isImprovedSaveFlowEnabled()) {
            BookmarkId newBookmarkId = addBookmarkAndShowSaveFlow(
                    activity, bookmarkModel, tab, bottomSheetController, bookmarkType);
            callback.onResult(newBookmarkId);
            return;
        }

        // TODO(crbug.com/1249283): Add separate entrypoint to save reading list items that uses
        // the improved save flow.
        if (bookmarkType != BookmarkType.READING_LIST
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.BOOKMARK_BOTTOM_SHEET)) {
            // Show a bottom sheet to let the user select target bookmark folder.
            showBookmarkBottomSheet(bookmarkModel, tab, snackbarManager, bottomSheetController,
                    activity, fromCustomTab, callback);
            return;
        }

        BookmarkId newBookmarkId = addBookmarkAndShowSnackbar(
                bookmarkModel, tab, snackbarManager, activity, fromCustomTab, bookmarkType);
        callback.onResult(newBookmarkId);
    }

    private static void showBookmarkBottomSheet(BookmarkModel bookmarkModel, Tab tab,
            SnackbarManager snackbarManager, BottomSheetController bottomSheetController,
            Activity activity, boolean fromCustomTab, Callback<BookmarkId> callback) {
        BookmarkBottomSheetCoordinator bookmarkBottomSheet =
                new BookmarkBottomSheetCoordinator(activity, bottomSheetController, bookmarkModel);
        RecordUserAction.record("Android.Bookmarks.BottomSheet.Open");
        bookmarkBottomSheet.show((selectedBookmarkItem) -> {
            if (selectedBookmarkItem == null) {
                callback.onResult(null);
                return;
            }

            // Add to the selected bookmark folder.
            BookmarkId newBookmarkId =
                    addBookmarkAndShowSnackbar(bookmarkModel, tab, snackbarManager, activity,
                            fromCustomTab, selectedBookmarkItem.getId().getType());

            RecordHistogram.recordEnumeratedHistogram("Bookmarks.BottomSheet.DestinationFolder",
                    selectedBookmarkItem.getId().getType(), BookmarkType.LAST + 1);
            callback.onResult(newBookmarkId);
        });
    }

    private static BookmarkId addBookmarkAndShowSaveFlow(Activity activity,
            BookmarkModel bookmarkModel, Tab tab, BottomSheetController bottomSheetController,
            @BookmarkType int bookmarkType) {
        BookmarkId bookmarkId;
        // TODO(crbug.com/1252228): Reading list support needs some tests.
        bookmarkId = addBookmarkInternal(activity, bookmarkModel, tab.getTitle(),
                tab.getOriginalUrl(), tab.getWebContents(), /*parent=*/null, bookmarkType);
        BookmarkSaveFlowCoordinator bookmarkSaveFlowCoordinator =
                new BookmarkSaveFlowCoordinator(activity, bottomSheetController,
                        new CommerceSubscriptionsServiceFactory()
                                .getForLastUsedProfile()
                                .getSubscriptionsManager());
        bookmarkSaveFlowCoordinator.show(bookmarkId, /*fromExplicitTrackUi=*/false);

        return bookmarkId;
    }

    // The legacy code path to add or edit bookmark without triggering the bookmark bottom sheet.
    private static BookmarkId addBookmarkAndShowSnackbar(BookmarkModel bookmarkModel, Tab tab,
            SnackbarManager snackbarManager, Activity activity, boolean fromCustomTab,
            @BookmarkType int bookmarkType) {
        if (ReadingListFeatures.isReadingListEnabled()
                && bookmarkType == BookmarkType.READING_LIST) {
            return addToReadingList(
                    tab.getOriginalUrl(), tab.getTitle(), snackbarManager, bookmarkModel, activity);
        }
        BookmarkId bookmarkId = addBookmarkInternal(activity, bookmarkModel, tab.getTitle(),
                tab.getOriginalUrl(), tab.getWebContents(), /*parent=*/null, BookmarkType.NORMAL);

        if (bookmarkId != null && bookmarkId.getType() == BookmarkType.NORMAL) {
            @BrowserProfileType
            int type = Profile.getBrowserProfileTypeFromProfile(
                    Profile.fromWebContents(tab.getWebContents()));
            RecordHistogram.recordEnumeratedHistogram(
                    "Bookmarks.AddedPerProfileType", type, BrowserProfileType.MAX_VALUE + 1);
        }

        Snackbar snackbar = null;
        if (bookmarkId == null) {
            snackbar = Snackbar.make(activity.getString(R.string.bookmark_page_failed),
                    new SnackbarController() {
                        @Override
                        public void onDismissNoAction(Object actionData) { }

                        @Override
                        public void onAction(Object actionData) { }
                    }, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_BOOKMARK_ADDED)
                    .setSingleLine(false);
            RecordUserAction.record("EnhancedBookmarks.AddingFailed");
        } else {
            String folderName = bookmarkModel.getBookmarkTitle(
                    bookmarkModel.getBookmarkById(bookmarkId).getParentId());
            SnackbarController snackbarController =
                    createSnackbarControllerForEditButton(activity, bookmarkId);
            if (getLastUsedParent(activity, bookmarkModel) == null) {
                if (fromCustomTab) {
                    String packageLabel = BuildInfo.getInstance().hostPackageLabel;
                    snackbar = Snackbar.make(
                            activity.getString(R.string.bookmark_page_saved, packageLabel),
                            snackbarController, Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_ADDED);
                } else {
                    snackbar = Snackbar.make(
                            activity.getString(R.string.bookmark_page_saved_default),
                            snackbarController, Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_ADDED);
                }
            } else {
                snackbar = Snackbar.make(folderName, snackbarController, Snackbar.TYPE_ACTION,
                        Snackbar.UMA_BOOKMARK_ADDED)
                        .setTemplateText(activity.getString(R.string.bookmark_page_saved_folder));
            }
            snackbar.setSingleLine(false).setAction(activity.getString(R.string.bookmark_item_edit),
                    null);
        }
        snackbarManager.showSnackbar(snackbar);
        return bookmarkId;
    }

    /**
     * Add an article to the reading list. If the article was already loaded, the entry will be
     * overwritten. After successful addition, a snackbar will be shown notifying the user about the
     * result of the operation.
     * @param url The associated URL.
     * @param title The title of the reading list item being added.
     * @param snackbarManager The snackbar manager that will be used to show a snackbar.
     * @param bookmarkBridge The bookmark bridge that talks to the bookmark backend.
     * @param context The associated context.
     * @return The bookmark ID created after saving the article to the reading list.
     */
    public static BookmarkId addToReadingList(GURL url, String title,
            SnackbarManager snackbarManager, BookmarkBridge bookmarkBridge, Context context) {
        assert bookmarkBridge.isBookmarkModelLoaded();
        BookmarkId bookmarkId = bookmarkBridge.addToReadingList(title, url);

        if (bookmarkId != null) {
            Snackbar snackbar = Snackbar.make(context.getString(R.string.reading_list_saved),
                    new SnackbarController() {}, Snackbar.TYPE_ACTION,
                    Snackbar.UMA_READING_LIST_BOOKMARK_ADDED);
            snackbarManager.showSnackbar(snackbar);

            TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                    .notifyEvent(EventConstants.READ_LATER_ARTICLE_SAVED);
        }
        return bookmarkId;
    }

    /**
     * Adds a bookmark with the given {@link Tab}. This will reset last used parent if it fails to
     * add a bookmark.
     *
     * @param context The current Android {@link Context}.
     * @param bookmarkModel The current {@link BookmarkModel} which talks to native.
     * @param tab The current {@link Tab} which bookmark properties are pulled.
     * @param bookmarkType The {@link BookmarkType} of the bookmark.
     */
    static BookmarkId addBookmarkInternal(Context context, BookmarkModel bookmarkModel,
            String title, GURL url, WebContents webContents, @Nullable BookmarkId parent,
            @BookmarkType int bookmarkType) {
        parent = parent == null ? getLastUsedParent(context, bookmarkModel) : parent;
        BookmarkItem parentItem = null;
        if (parent != null) {
            parentItem = bookmarkModel.getBookmarkById(parent);
        }
        if (parent == null || parentItem == null || parentItem.isManaged()
                || !parentItem.isFolder()) {
            parent = bookmarkModel.getDefaultFolder();
        }

        // Reading list items will be added when either one of the 2 conditions is met:
        // 1. The bookmark type explicitly specifies READING_LIST.
        // 2. The last used parent implicitly specifies READING_LIST.
        if (ReadingListFeatures.isReadingListEnabled()
                && (bookmarkType == BookmarkType.READING_LIST
                        || parent.getType() == BookmarkType.READING_LIST)) {
            return bookmarkModel.addToReadingList(title, url);
        }

        BookmarkId bookmarkId = null;
        // Use "New tab" as title for both incognito and regular NTP.
        if (url.getSpec().equals(UrlConstants.NTP_URL)) {
            title = context.getResources().getString(R.string.new_tab_title);
        }
        // The shopping list experiment saves extra metadata along with the bookmark.
        if (ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.SHOPPING_LIST)) {
            bookmarkId = bookmarkModel.addPowerBookmark(
                    webContents, parent, bookmarkModel.getChildCount(parent), title, url);
        } else {
            bookmarkId = bookmarkModel.addBookmark(
                    parent, bookmarkModel.getChildCount(parent), title, url);
        }
        // TODO(lazzzis): remove log after bookmark sync is fixed, crbug.com/986978
        if (bookmarkId == null) {
            Log.e(TAG,
                    "Failed to add bookmarks: parentTypeAndId %s, defaultFolderTypeAndId %s, "
                            + "mobileFolderTypeAndId %s, parentEditable Managed isFolder %s,",
                    parent, bookmarkModel.getDefaultFolder(), bookmarkModel.getMobileFolderId(),
                    parentItem == null ? "null"
                                       : (parentItem.isEditable() + " " + parentItem.isManaged()
                                               + " " + parentItem.isFolder()));
            setLastUsedParent(context, bookmarkModel.getDefaultFolder());
        }
        return bookmarkId;
    }

    /**
     * Creates a snackbar controller for a case where "Edit" button is shown to edit the newly
     * created bookmark.
     */
    private static SnackbarController createSnackbarControllerForEditButton(
            final Activity activity, final BookmarkId bookmarkId) {
        return new SnackbarController() {
            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonNotClicked");
            }

            @Override
            public void onAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonClicked");
                startEditActivity(activity, bookmarkId);
            }
        };
    }

    /**
     * Shows bookmark main UI.
     * @param activity An activity to start the manager with.
     * @param isIncognito Whether the bookmark manager is opened in incognito mode.
     */
    public static void showBookmarkManager(Activity activity, boolean isIncognito) {
        showBookmarkManager(activity, null, isIncognito);
    }

    /**
     * Shows bookmark main UI.
     * @param activity An activity to start the manager with. If null, the bookmark manager will be
     *         started as a new task.
     * @param folderId The bookmark folder to open. If null, the bookmark manager will open the most
     *         recent folder.
     * @param isIncognito Whether the bookmark UI is opened in incognito mode.
     */
    public static void showBookmarkManager(
            @Nullable Activity activity, @Nullable BookmarkId folderId, boolean isIncognito) {
        ThreadUtils.assertOnUiThread();
        Context context = activity == null ? ContextUtils.getApplicationContext() : activity;
        String url = getFirstUrlToLoad(context, folderId);

        if (ReadingListFeatures.shouldUseRootFolderAsDefaultForReadLater()
                && SharedPreferencesManager.getInstance().contains(
                        ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL)) {
            RecordUserAction.record("MobileBookmarkManagerReopenBookmarksInSameSession");
        }

        // Tablet.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            openUrl(context, url, activity == null ? null : activity.getComponentName());
            return;
        }

        // Phone.
        Intent intent = new Intent(context, BookmarkActivity.class);
        intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, isIncognito);
        intent.setData(Uri.parse(url));
        if (activity != null) {
            // Start from an existing activity.
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            activity.startActivity(intent);
        } else {
            // Start a new task.
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(intent);
        }
    }

    /**
     * @return the bookmark folder URL to open.
     */
    private static String getFirstUrlToLoad(Context context, @Nullable BookmarkId folderId) {
        String url;
        if (folderId == null) {
            // Load most recently visited bookmark folder.
            url = getLastUsedUrl(context);
        } else {
            // Load a specific folder.
            url = BookmarkUIState.createFolderUrl(folderId).toString();
        }

        return TextUtils.isEmpty(url) ? UrlConstants.BOOKMARKS_URL : url;
    }

    /**
     * Saves the last used url to preference. The saved url will be later queried by
     * {@link #getLastUsedUrl(Context)}
     */
    static void setLastUsedUrl(Context context, String url) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, url);
    }

    /**
     * Fetches url representing the user's state last time they close the bookmark manager.
     */
    @VisibleForTesting
    static String getLastUsedUrl(Context context) {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, UrlConstants.BOOKMARKS_URL);
    }

    /**
     * Save the last used {@link BookmarkId} as a folder to put new bookmarks to.
     */
    static void setLastUsedParent(Context context, BookmarkId bookmarkId) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, bookmarkId.toString());
    }

    /**
     * @param context The current android {@link Context}.
     * @param bookmarkModel The bookmark model used to reset the last used parent for type swapping
     *         edge cases.
     * @return The parent {@link BookmarkId} that the user used the last time or null if the user
     *         has never selected a parent folder to use.
     */
    static BookmarkId getLastUsedParent(Context context, BookmarkModel bookmarkModel) {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        if (!preferences.contains(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT)) return null;

        BookmarkId parent = BookmarkId.getBookmarkIdFromString(
                preferences.readString(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, null));

        // We need to reset the last used parent to support toggling reading list type-swapping.
        if (parent.getType() == BookmarkType.READING_LIST
                && !ReadingListFeatures.shouldAllowBookmarkTypeSwapping()) {
            setLastUsedParent(context, bookmarkModel.getDefaultFolder());
            return null;
        }

        return parent;
    }

    /** Starts an {@link BookmarkEditActivity} for the given {@link BookmarkId}. */
    public static void startEditActivity(Context context, BookmarkId bookmarkId) {
        RecordUserAction.record("MobileBookmarkManagerEditBookmark");
        Intent intent = new Intent(context, BookmarkEditActivity.class);
        intent.putExtra(BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
        if (context instanceof BookmarkActivity) {
            ((BookmarkActivity) context).startActivityForResult(
                    intent, BookmarkActivity.EDIT_BOOKMARK_REQUEST_CODE);
        } else {
            context.startActivity(intent);
        }
    }

    /** Starts an {@link BookmarkFolderSelectActivity} for the given {@link BookmarkId}. */
    public static void startFolderSelectActivity(Context context, BookmarkId bookmarkId) {
        BookmarkFolderSelectActivity.startFolderSelectActivity(context, bookmarkId);
    }

    /**
     * Opens a bookmark and reports UMA.
     * @param context The current context used to launch the intent.
     * @param openBookmarkComponentName The component to use when opening a bookmark.
     * @param model Bookmarks model to manage the bookmark.
     * @param bookmarkId ID of the bookmark to be opened.
     * @param isIncognito Whether the bookmark manager is opened in incognito mode.
     * @return Whether the bookmark was successfully opened.
     */
    public static boolean openBookmark(Context context, ComponentName openBookmarkComponentName,
            BookmarkModel model, BookmarkId bookmarkId, boolean isIncognito) {
        if (model.getBookmarkById(bookmarkId) == null) return false;

        RecordUserAction.record("MobileBookmarkManagerEntryOpened");
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.OpenBookmarkType", bookmarkId.getType(), BookmarkType.LAST + 1);

        BookmarkItem bookmarkItem = model.getBookmarkById(bookmarkId);
        assert bookmarkItem != null;
        RecordHistogram.recordCustomTimesHistogram("Bookmarks.OpenBookmarkTimeInterval2."
                        + bookmarkTypeToHistogramSuffix(bookmarkId.getType()),
                System.currentTimeMillis() - bookmarkItem.getDateAdded(), 1,
                DateUtils.DAY_IN_MILLIS * 30, 50);

        if (bookmarkItem.getId().getType() == BookmarkType.READING_LIST
                && !bookmarkItem.isFolder()) {
            openReadingListInCustomTab(context, bookmarkItem.getUrl().getSpec(), isIncognito);
            model.setReadStatusForReadingList(bookmarkItem.getUrl(), true);
        } else {
            openUrl(context, bookmarkItem.getUrl().getSpec(), openBookmarkComponentName);
        }
        return true;
    }

    private static String bookmarkTypeToHistogramSuffix(@BookmarkType int type) {
        switch (type) {
            case BookmarkType.NORMAL:
                return "Normal";
            case BookmarkType.PARTNER:
                return "Partner";
            case BookmarkType.READING_LIST:
                return "ReadingList";
        }
        assert false : "Unknown BookmarkType";
        return "";
    }

    /**
     * @param context {@link Context} used to retrieve the drawable.
     * @param type The bookmark type of the folder.
     * @return A {@link Drawable} to use for displaying bookmark folders.
     */
    public static Drawable getFolderIcon(Context context, @BookmarkType int type) {
        if (type == BookmarkType.READING_LIST) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_reading_list_folder_24dp, getFolderIconTint(type));
        }
        return UiUtils.getTintedDrawable(
                context, R.drawable.ic_folder_blue_24dp, getFolderIconTint(type));
    }

    /**
     * @param type The bookmark type.
     * @return The tint used on the bookmark folder icon.
     */
    public static @ColorRes int getFolderIconTint(@BookmarkType int type) {
        return (type == BookmarkType.READING_LIST) ? R.color.default_icon_color_accent1_tint_list
                                                   : R.color.default_icon_color_tint_list;
    }

    /**
     * Retrieve the save flow title for the given bookmark.
     *
     * @param context The current Android {@link Context}.
     * @param bookmarkId The {@link BookmarkId} to get the title for.
     * @return The title associated with the given bookmarkId.
     */
    public static String getSaveFlowTitleForBookmark(
            Context context, BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta) {
        if (meta != null && meta.getType() == PowerBookmarkType.SHOPPING) {
            return context.getResources().getString(R.string.price_tracking_save_flow_title);
        }
        return context.getResources().getString(R.string.bookmark_page_saved_default);
    }

    /**
     * Retrieve the save flow subtitle for the given bookmark.
     *
     * @param context The current Android {@link Context}.
     * @param bookmarkId The {@link BookmarkId} to get the subtitle for.
     * @return The subtitle associated with the given bookmarkId.
     */
    public static String getSaveFlowSubtitleForBookmark(
            Context context, BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta) {
        if (meta != null && meta.getType() == PowerBookmarkType.SHOPPING) {
            return context.getResources().getString(R.string.price_tracking_save_flow_subtitle);
        }
        return null;
    }

    /**
     * Retrieve the save flow start icon for the given bookmark.
     *
     * @param bookmarkId The {@link BookmarkId} to get the start icon for.
     * @return The start icon associated with the given bookmarkId.
     */
    public static Drawable getSaveFlowStartIconForBookmark(BookmarkId bookmarkId) {
        // TODO(crbug.com/1243383): Add start icon for price tracking.
        return null;
    }

    private static void openUrl(Context context, String url, ComponentName componentName) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, context.getApplicationContext().getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.AUTO_BOOKMARK);

        if (componentName != null) {
            ChromeTabbedActivity.setNonAliasedComponent(intent, componentName);
        } else {
            // If the bookmark manager is shown in a tab on a phone (rather than in a separate
            // activity) the component name may be null. Send the intent through
            // ChromeLauncherActivity instead to avoid crashing. See crbug.com/615012.
            intent.setClass(context.getApplicationContext(), ChromeLauncherActivity.class);
        }

        IntentHandler.startActivityForTrustedIntent(intent);
    }

    private static void openReadingListInCustomTab(
            Context context, String url, boolean isOffTheRecord) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON);
        CustomTabsIntent customTabIntent = builder.build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.READ_LATER);

        // Extras for incognito CCT.
        if (isOffTheRecord) {
            IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                    intent, IncognitoCCTCallerId.READ_LATER);
        }

        IntentUtils.addTrustedIntentExtras(intent);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.startActivityForTrustedIntent(intent);
    }

    /**
     * Closes the {@link BookmarkActivity} on Phone. Does nothing on tablet.
     */
    public static void finishActivityOnPhone(Context context) {
        if (context instanceof BookmarkActivity) {
            ((Activity) context).finish();
        }
    }

    /**
     * Populates the top level bookmark folder ids.
     * @param bookmarkModel The bookmark model that talks to bookmark native backend.
     * @return The list of top level bookmark folder ids.
     */
    public static List<BookmarkId> populateTopLevelFolders(BookmarkModel bookmarkModel) {
        List<BookmarkId> topLevelFolders = new ArrayList<>();
        BookmarkId desktopNodeId = bookmarkModel.getDesktopFolderId();
        BookmarkId mobileNodeId = bookmarkModel.getMobileFolderId();
        BookmarkId othersNodeId = bookmarkModel.getOtherFolderId();

        List<BookmarkId> specialFoldersIds =
                bookmarkModel.getTopLevelFolderIDs(/*getSpecial=*/true, /*getNormal=*/false);
        BookmarkId rootFolder = bookmarkModel.getRootFolderId();

        // managed and partner bookmark folders will be put to the bottom.
        List<BookmarkId> managedAndPartnerFolderIds = new ArrayList<>();

        for (BookmarkId bookmarkId : specialFoldersIds) {
            // Adds reading list as the first top level folder.
            if (bookmarkId.getType() == BookmarkType.READING_LIST) {
                topLevelFolders.add(bookmarkId);
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                        .notifyEvent(EventConstants.READ_LATER_BOTTOM_SHEET_FOLDER_SEEN);
                continue;
            }
            BookmarkId parent = bookmarkModel.getBookmarkById(bookmarkId).getParentId();
            if (parent.equals(rootFolder)) managedAndPartnerFolderIds.add(bookmarkId);
        }

        // Adds normal bookmark top level folders.
        if (bookmarkModel.isFolderVisible(mobileNodeId)) {
            topLevelFolders.add(mobileNodeId);
        }
        if (bookmarkModel.isFolderVisible(desktopNodeId)) {
            topLevelFolders.add(desktopNodeId);
        }
        if (bookmarkModel.isFolderVisible(othersNodeId)) {
            topLevelFolders.add(othersNodeId);
        }

        // Add any top-level managed and partner bookmark folders that are children of the root
        // folder.
        topLevelFolders.addAll(managedAndPartnerFolderIds);
        return topLevelFolders;
    }

    /**
     * Expires the stored last used url if Chrome has been in the background long enough to mark it
     * as a new session. We're using the "Start Surface" concept of session here which is if the
     * app has been in the background for X amount of time. Called from #onStartWithNative, after
     * which the time stored in {@link ChromeInactivityTracker} is expired.
     *
     * @param timeSinceLastBackgroundedMs The time since Chrome has sent into the background.
     */
    public static void maybeExpireLastBookmarkLocationForReadLater(
            long timeSinceLastBackgroundedMs) {
        if (!ReadingListFeatures.shouldUseRootFolderAsDefaultForReadLater()) return;

        int readLaterSessionLengthMs = ReadingListFeatures.getSessionLengthMs();
        if (timeSinceLastBackgroundedMs > readLaterSessionLengthMs) {
            SharedPreferencesManager.getInstance().removeKey(
                    ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL);
        }
    }
}
