// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.annotation.PluralsRes;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.infobar.DownloadProgressInfoBar;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.components.download.DownloadState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This class is responsible for tracking various updates for download items, offline items, android
 * downloads and computing the the state of the {@link DownloadProgressInfoBar} .
 */
public class DownloadInfoBarController implements OfflineContentProvider.Observer {
    private static final String SPEEDING_UP_MESSAGE_ENABLED = "speeding_up_message_enabled";
    private static final long DURATION_ACCELERATED_INFOBAR_IN_MS = 3000;
    private static final long DURATION_SHOW_RESULT_IN_MS = 6000;

    // Values for the histogram Android.Download.InfoBar.Shown. Keep this in sync with the
    // DownloadInfoBar.ShownState enum in enums.xml.
    @IntDef({UmaInfobarShown.ANY_STATE, UmaInfobarShown.ACCELERATED, UmaInfobarShown.DOWNLOADING,
            UmaInfobarShown.COMPLETE, UmaInfobarShown.FAILED, UmaInfobarShown.PENDING,
            UmaInfobarShown.MULTIPLE_DOWNLOADING, UmaInfobarShown.MULTIPLE_COMPLETE,
            UmaInfobarShown.MULTIPLE_FAILED, UmaInfobarShown.MULTIPLE_PENDING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface UmaInfobarShown {
        int ANY_STATE = 0;
        int ACCELERATED = 1;
        int DOWNLOADING = 2;
        int COMPLETE = 3;
        int FAILED = 4;
        int PENDING = 5;
        int MULTIPLE_DOWNLOADING = 6;
        int MULTIPLE_COMPLETE = 7;
        int MULTIPLE_FAILED = 8;
        int MULTIPLE_PENDING = 9;
        int NUM_ENTRIES = 10;
    }

    /**
     * Represents various UI states that the InfoBar cycles through.
     * Note: This enum is append-only and the values must match the DownloadInfoBarState enum in
     * enums.xml. Values should be number from 0 and can't have gaps.
     */
    @VisibleForTesting
    @IntDef({DownloadInfoBarState.INITIAL, DownloadInfoBarState.DOWNLOADING,
            DownloadInfoBarState.SHOW_RESULT, DownloadInfoBarState.CANCELLED})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface DownloadInfoBarState {
        // Default initial state. It is also the final state after all the downloads are paused or
        // removed. No InfoBar is shown in this state.
        int INITIAL = 0;
        // InfoBar is showing a message indicating the downloads in progress. In case of a single
        // accelerated download, the InfoBar would show the speeding-up download message for {@code
        // DURATION_ACCELERATED_INFOBAR_IN_MS} before transitioning to downloading file(s) message.
        // If download completes,fails or goes to pending state, the transition happens immediately
        // to SHOW_RESULT state.
        int DOWNLOADING = 1;
        // InfoBar is showing download complete, failed or pending message. The InfoBar stays in
        // this state for {@code DURATION_SHOW_RESULT_IN_MS} before transitioning to the next state,
        // which can be another SHOW_RESULT or DOWNLOADING state. This can also happen to be the
        // terminal state if there are no more updates to be shown.
        // In case of a new download, completed download or cancellation signal, the transition
        // happens immediately.
        int SHOW_RESULT = 2;
        // The state of the InfoBar after it was explicitly cancelled by the user. The InfoBar is
        // resurfaced only when there is a new download or an existing download moves to completion,
        // failed or pending state.
        int CANCELLED = 3;
        // Number of entries
        int NUM_ENTRIES = 4;
    }

    /**
     * Represents the data required to show UI elements of the {@link DownloadProgressInfoBar}..
     */
    public static class DownloadProgressInfoBarData {
        @Nullable
        public ContentId id;

        public String message;
        public String accessibilityMessage;
        public String link;
        public int icon;

        // Whether the icon corresponds to a vector drawable.
        public boolean hasVectorDrawable;

        // Whether the icon should have animation.
        public boolean hasAnimation;

        // Whether the animation should be shown only once.
        public boolean dontRepeat;

        // Whether the the InfoBar must be shown in the current tab, even if the user has navigated
        // to a different tab. In that case, the InfoBar associated with the previous tab is closed
        // and a new InfoBar is created with the currently focused tab.
        public boolean forceReparent;

        // Keeps track of the current number of downloads in various states. Only used to compute
        // |forceReparent|.
        public DownloadCount downloadCount = new DownloadCount();

        // Used for differentiating various states (e.g. completed, failed, pending etc) in the
        // SHOW_RESULT state. Keeps track of the state of the currently displayed item(s) and should
        // be reset to null when moving out DOWNLOADING/SHOW_RESULT state.
        @OfflineItemState
        public Integer resultState;

        @Override
        public int hashCode() {
            int result = (id == null ? 0 : id.hashCode());
            result = 31 * result + (message == null ? 0 : message.hashCode());
            result = 31 * result + (link == null ? 0 : link.hashCode());
            result = 31 * result + icon;
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof DownloadProgressInfoBarData)) return false;

            DownloadProgressInfoBarData other = (DownloadProgressInfoBarData) obj;
            boolean idEquality = (id == null ? other.id == null : id.equals(other.id));
            return idEquality && TextUtils.equals(message, other.message)
                    && TextUtils.equals(link, other.link) && icon == other.icon;
        }
    }

    /**
     * An utility class to count the number of downloads at different states at any given time.
     */
    private static class DownloadCount {
        public int inProgress;
        public int pending;
        public int failed;
        public int completed;

        /** @return The total number of downloads being tracked. */
        public int totalCount() {
            return inProgress + pending + failed + completed;
        }

        public int getCount(@OfflineItemState int state) {
            switch (state) {
                case OfflineItemState.IN_PROGRESS:
                    return inProgress;
                case OfflineItemState.COMPLETE:
                    return completed;
                case OfflineItemState.FAILED:
                    return failed;
                case OfflineItemState.PENDING:
                    return pending;
                default:
                    assert false;
            }
            return 0;
        }

        @Override
        public int hashCode() {
            int result = 31 * inProgress;
            result = 31 * result + pending;
            result = 31 * result + failed;
            result = 31 * result + completed;
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof DownloadCount)) return false;

            DownloadCount other = (DownloadCount) obj;
            return inProgress == other.inProgress && pending == other.pending
                    && failed == other.failed && completed == other.completed;
        }
    }

    private final boolean mUseNewDownloadPath;
    private final boolean mIsIncognito;
    private final Handler mHandler = new Handler();
    private final DownloadProgressInfoBar.Client mClient = new DownloadProgressInfoBarClient();

    // Keeps track of a running list of items, which gets updated regularly with every update from
    // the backend. The entries are removed only when the item has reached a certain completion
    // state (i.e. complete, failed or pending) or is cancelled/removed from the backend.
    private final LinkedHashMap<ContentId, OfflineItem> mTrackedItems = new LinkedHashMap<>();

    // Keeps track of all the items that have been seen in the current chrome session.
    private final Set<ContentId> mSeenItems = new HashSet<>();

    // Keeps track of the items which are being ignored by the controller, e.g. user initiated
    // paused items.
    private final Set<ContentId> mIgnoredItems = new HashSet<>();

    // The notification IDs associated with the currently tracked completed items. The notification
    // should be removed when the InfoBar link is clicked to open the item.
    private final Map<ContentId, Integer> mNotificationIds = new HashMap<>();

    // The current state of the InfoBar.
    private @DownloadInfoBarState int mState = DownloadInfoBarState.INITIAL;

    // This is used when the InfoBar is currently in a state awaiting timer completion, e.g. showing
    // the speeding-up message or showing the result of a download. This is used to schedule a task
    // to determine the next state. If the infobar moves out of the current state, the scheduled
    // task should be cancelled.
    private Runnable mEndTimerRunnable;

    // Represents the InfoBar, currently being handled by this controller.
    private DownloadProgressInfoBar mCurrentInfoBar;

    // Represents the currently displayed InfoBar data.
    private DownloadProgressInfoBarData mCurrentInfo;

    /** Constructor. */
    public DownloadInfoBarController(boolean isIncognito) {
        mUseNewDownloadPath =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER);
        mIsIncognito = isIncognito;
        mHandler.post(() -> getOfflineContentProvider().addObserver(this));
    }

    /**
     * Shows the message that download has started. Unlike other methods in this class, this
     * method doesn't require an {@link OfflineItem} and is invoked by the backend to provide a
     * responsive feedback to the users even before the download has actually started.
     */
    public void onDownloadStarted() {
        computeNextStepForUpdate(null, true, false, false);
    }

    /** Updates the InfoBar when new information about a download comes in. */
    public void onDownloadItemUpdated(DownloadItem downloadItem) {
        if (mUseNewDownloadPath) return;

        OfflineItem offlineItem = DownloadInfo.createOfflineItem(downloadItem.getDownloadInfo());
        if (!isVisibleToUser(offlineItem)) return;

        if (downloadItem.getDownloadInfo().state() == DownloadState.COMPLETE) {
            handleDownloadCompletion(downloadItem);
            return;
        }

        if (offlineItem.state == OfflineItemState.CANCELLED) {
            onItemRemoved(offlineItem.id);
            return;
        }

        computeNextStepForUpdate(offlineItem);
    }

    /** Updates the InfoBar after a download has been removed. */
    public void onDownloadItemRemoved(ContentId contentId) {
        if (mUseNewDownloadPath) return;
        onItemRemoved(contentId);
    }

    /** Associates a notification ID with the tracked download for future usage. */
    // TODO(shaktisahu): Find an alternative way after moving to offline content provider.
    public void onNotificationShown(ContentId id, int notificationId) {
        mNotificationIds.put(id, notificationId);
    }

    private void handleDownloadCompletion(DownloadItem downloadItem) {
        // Multiple OnDownloadUpdated() notifications may be issued while the
        // download is in the COMPLETE state. Don't handle it if it was previously not in-progress.
        if (!mTrackedItems.containsKey(downloadItem.getContentId())) return;

        // If the download should be auto-opened, we shouldn't show the infobar.
        DownloadManagerService.getDownloadManagerService().checkIfDownloadWillAutoOpen(
                downloadItem, (result) -> {
                    if (result) {
                        onItemRemoved(downloadItem.getContentId());
                    } else {
                        computeNextStepForUpdate(
                                DownloadInfo.createOfflineItem(downloadItem.getDownloadInfo()));
                    }
                });
    }

    // OfflineContentProvider.Observer implementation.
    @Override
    public void onItemsAdded(ArrayList<OfflineItem> items) {
        for (OfflineItem item : items) {
            if (!isVisibleToUser(item)) continue;
            computeNextStepForUpdate(item);
        }
    }

    @Override
    public void onItemRemoved(ContentId id) {
        if (!mSeenItems.contains(id)) return;

        mTrackedItems.remove(id);
        mNotificationIds.remove(id);
        computeNextStepForUpdate(null, false, false, true);
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
        if (!isVisibleToUser(item)) return;

        if (updateDelta != null && !updateDelta.stateChanged
                && item.state == OfflineItemState.COMPLETE) {
            return;
        }

        if (item.state == OfflineItemState.CANCELLED) {
            onItemRemoved(item.id);
            return;
        }

        computeNextStepForUpdate(item);
    }

    /**
     * Helper method to get the parameters for showing accelerated download infobar IPH.
     * @return The UI parameters to show IPH, if an IPH should be shown, null otherwise.
     */
    public IPHInfoBarSupport.TrackerParameters getTrackerParameters() {
        if (getDownloadCount().inProgress == 0) return null;

        BottomSheet bottomSheet =
                getCurrentTab() == null ? null : getCurrentTab().getActivity().getBottomSheet();
        if (bottomSheet != null && bottomSheet.isSheetOpen()) return null;

        return new IPHInfoBarSupport.TrackerParameters(
                FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE,
                R.string.iph_download_infobar_downloads_are_faster_text,
                R.string.iph_download_infobar_downloads_are_faster_text);
    }

    private boolean isVisibleToUser(OfflineItem offlineItem) {
        if (offlineItem.isTransient || offlineItem.isOffTheRecord != mIsIncognito
                || offlineItem.isSuggested || offlineItem.isDangerous) {
            return false;
        }
        if (LegacyHelpers.isLegacyDownload(offlineItem.id)
                && TextUtils.isEmpty(offlineItem.filePath)) {
            return false;
        }

        if (DownloadUtils.shouldAutoOpenDownload(offlineItem.mimeType, true)) {
            return false;
        }

        return true;
    }

    private void computeNextStepForUpdate(OfflineItem updatedItem) {
        computeNextStepForUpdate(updatedItem, false, false, false);
    }

    /**
     * Updates the state of the infobar based on the update received and current state of the
     * tracked downloads.
     * @param updatedItem The item that was updated just now.
     * @param forceShowDownloadStarted Whether the infobar should show download started even if
     * there is no updated item.
     * @param userCancel Whether the infobar was cancelled just now.
     * ended.
     */
    private void computeNextStepForUpdate(OfflineItem updatedItem, boolean forceShowDownloadStarted,
            boolean userCancel, boolean itemWasRemoved) {
        if (!FeatureUtilities.isDownloadProgressInfoBarEnabled()) return;

        if (updatedItem != null && mIgnoredItems.contains(updatedItem.id)) return;

        preProcessUpdatedItem(updatedItem);
        boolean isNewDownload = forceShowDownloadStarted
                || (updatedItem != null && updatedItem.state == OfflineItemState.IN_PROGRESS
                           && !mSeenItems.contains(updatedItem.id));
        boolean itemResumedFromPending = itemResumedFromPending(updatedItem);

        if (updatedItem != null) {
            mTrackedItems.put(updatedItem.id, updatedItem);
            mSeenItems.add(updatedItem.id);
        }

        boolean itemWasPaused = updatedItem != null && updatedItem.state == OfflineItemState.PAUSED;
        if (itemWasPaused) {
            mIgnoredItems.add(updatedItem.id);
            mTrackedItems.remove(updatedItem.id);
        }

        DownloadCount downloadCount = getDownloadCount();
        boolean shouldShowResult =
                downloadCount.completed + downloadCount.failed + downloadCount.pending > 0;

        boolean shouldShowAccelerating =
                mEndTimerRunnable != null && mState == DownloadInfoBarState.DOWNLOADING;

        @DownloadInfoBarState
        int nextState = mState;
        switch (mState) {
            case DownloadInfoBarState.INITIAL: // Intentional fallthrough.
            case DownloadInfoBarState.CANCELLED:
                if (isNewDownload) {
                    nextState = DownloadInfoBarState.DOWNLOADING;
                    shouldShowAccelerating =
                            isAccelerated(updatedItem) && downloadCount.inProgress == 1;
                } else if (shouldShowResult) {
                    nextState = DownloadInfoBarState.SHOW_RESULT;
                }
                break;
            case DownloadInfoBarState.DOWNLOADING:
                if (isNewDownload) shouldShowAccelerating = false;

                if (shouldShowResult) {
                    nextState = DownloadInfoBarState.SHOW_RESULT;
                } else if (itemWasPaused || itemWasRemoved) {
                    nextState = downloadCount.inProgress == 0 ? DownloadInfoBarState.INITIAL
                                                              : DownloadInfoBarState.DOWNLOADING;
                }
                break;
            case DownloadInfoBarState.SHOW_RESULT:
                if (isNewDownload) {
                    nextState = DownloadInfoBarState.DOWNLOADING;
                    shouldShowAccelerating =
                            isAccelerated(updatedItem) && downloadCount.inProgress == 1;
                } else if (!shouldShowResult) {
                    if (mEndTimerRunnable == null && downloadCount.inProgress > 0) {
                        nextState = DownloadInfoBarState.DOWNLOADING;
                    }

                    boolean currentlyShowingPending = mCurrentInfo != null
                            && mCurrentInfo.resultState != null
                            && mCurrentInfo.resultState == OfflineItemState.PENDING;
                    if (currentlyShowingPending && itemResumedFromPending) {
                        nextState = DownloadInfoBarState.DOWNLOADING;
                    }
                    if ((itemWasPaused || itemWasRemoved) && mTrackedItems.size() == 0) {
                        nextState = DownloadInfoBarState.INITIAL;
                    }
                }
                break;
        }

        if (userCancel) nextState = DownloadInfoBarState.CANCELLED;

        moveToState(nextState, shouldShowAccelerating);
    }

    private void moveToState(@DownloadInfoBarState int nextState, boolean showAccelerating) {
        boolean closeInfoBar = nextState == DownloadInfoBarState.INITIAL
                || nextState == DownloadInfoBarState.CANCELLED;
        if (closeInfoBar) {
            mCurrentInfo = null;
            closePreviousInfoBar();
            if (nextState == DownloadInfoBarState.INITIAL) {
                mTrackedItems.clear();
            } else {
                clearFinishedItems(OfflineItemState.COMPLETE, OfflineItemState.FAILED,
                        OfflineItemState.PENDING);
            }
            clearEndTimerRunnable();
        }

        if (nextState == DownloadInfoBarState.DOWNLOADING
                || nextState == DownloadInfoBarState.SHOW_RESULT) {
            Integer offlineItemState = findOfflineItemStateForInfoBarState(nextState);
            if (offlineItemState == null) {
                // This is expected in the terminal SHOW_RESULT state when we have cleared the
                // tracked items but still want to show the infobar indefinitely.
                return;
            }
            createInfoBarForState(nextState, offlineItemState, showAccelerating);
        }

        mState = nextState;
    }

    /**
     * Determines the {@link OfflineItemState} for the message to be shown on the infobar. For
     * DOWNLOADING state, it will return {@link OfflineItemState.IN_PROGRESS}. Otherwise it should
     * show the result state which can be complete, failed or pending. There is usually a delay of
     * DURATION_SHOW_RESULT_IN_MS between transition between these states, except for the complete
     * state which must be shown as soon as received. While the InfoBar is in one of these states,
     * if we get another download update for the same state, we incorporate that in the existing
     * message and reset the timer to another full duration. Updates for pending and failed would be
     * shown in the order received.
     */
    private Integer findOfflineItemStateForInfoBarState(@DownloadInfoBarState int infoBarState) {
        if (infoBarState == DownloadInfoBarState.DOWNLOADING) return OfflineItemState.IN_PROGRESS;

        assert infoBarState == DownloadInfoBarState.SHOW_RESULT;

        DownloadCount downloadCount = getDownloadCount();

        // If there are completed downloads, show immediately.
        if (downloadCount.completed > 0) return OfflineItemState.COMPLETE;

        // If the infobar is already showing this state, just add this item to the same state.
        Integer previousResultState = mCurrentInfo != null ? mCurrentInfo.resultState : null;
        if (previousResultState != null && downloadCount.getCount(previousResultState) > 0) {
            return previousResultState;
        }

        // Show any failed or pending states in the order they were received.
        for (OfflineItem item : mTrackedItems.values()) {
            if (item.state == OfflineItemState.FAILED || item.state == OfflineItemState.PENDING) {
                return item.state;
            }
        }

        return null;
    }

    /**
     * Prepares the infobar to show the next state. This can be one of the messages i.e. speeding-up
     * download, downloading files or showing result e.g. complete, failed, pending message.
     * @param infoBarState The infobar state to be shown.
     * @param offlineItemState The state of the corresponding offline items to be shown.
     */
    private void createInfoBarForState(@DownloadInfoBarState int infoBarState,
            @OfflineItemState Integer offlineItemState, boolean showAccelerating) {
        DownloadProgressInfoBarData info = new DownloadProgressInfoBarData();

        @PluralsRes
        int stringRes = -1;
        if (infoBarState == DownloadInfoBarState.DOWNLOADING) {
            info.icon = showAccelerating ? R.drawable.infobar_downloading_sweep_animation
                                         : R.drawable.infobar_downloading_fill_animation;
            info.hasVectorDrawable = true;
            if (areAnimationsDisabled()) {
                info.icon = R.drawable.infobar_downloading;
                info.hasVectorDrawable = false;
            }
        } else if (offlineItemState == OfflineItemState.COMPLETE) {
            stringRes = R.plurals.multiple_download_complete;
            info.icon = R.drawable.infobar_download_complete;
            info.hasVectorDrawable = true;
        } else if (offlineItemState == OfflineItemState.FAILED) {
            stringRes = R.plurals.multiple_download_failed;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else if (offlineItemState == OfflineItemState.PENDING) {
            stringRes = R.plurals.multiple_download_pending;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else {
            assert false : "Unexpected offlineItemState " + offlineItemState + " and infoBarState "
                           + infoBarState;
        }

        OfflineItem itemToShow = null;
        long totalDownloadingSizeBytes = 0L;
        for (OfflineItem item : mTrackedItems.values()) {
            if (item.state != offlineItemState) continue;
            itemToShow = item;
            totalDownloadingSizeBytes += item.totalSizeBytes;
        }

        DownloadCount downloadCount = getDownloadCount();
        if (infoBarState == DownloadInfoBarState.DOWNLOADING) {
            if (showAccelerating) {
                info.message =
                        getContext().getString(R.string.download_infobar_speeding_up_download);
            } else {
                int inProgressDownloadCount =
                        downloadCount.inProgress == 0 ? 1 : downloadCount.inProgress;
                if (totalDownloadingSizeBytes <= 0L) {
                    info.message = getContext().getResources().getQuantityString(
                            R.plurals.download_infobar_downloading_files, inProgressDownloadCount,
                            inProgressDownloadCount);
                } else {
                    String bytesString = DownloadUtils.getStringForBytes(
                            getContext(), totalDownloadingSizeBytes);
                    info.message = inProgressDownloadCount == 1
                            ? getContext().getString(
                                      R.string.downloading_file_with_bytes, bytesString)
                            : getContext().getString(R.string.downloading_multiple_files_with_bytes,
                                      inProgressDownloadCount, bytesString);
                }
            }

            info.hasAnimation = !areAnimationsDisabled();
            info.link = showAccelerating ? null : getContext().getString(R.string.details_link);
        } else if (infoBarState == DownloadInfoBarState.SHOW_RESULT) {
            int itemCount = getDownloadCount().getCount(offlineItemState);
            boolean singleDownloadCompleted =
                    itemCount == 1 && offlineItemState == OfflineItemState.COMPLETE;
            if (singleDownloadCompleted) {
                info.message = getContext().getString(
                        R.string.download_infobar_filename, itemToShow.title);
                info.id = itemToShow.id;
                info.link = getContext().getString(R.string.open_downloaded_label);
                info.icon = R.drawable.infobar_download_complete_animation;
                info.hasAnimation = true;
                info.dontRepeat = true;
            } else {
                // TODO(shaktisahu): Incorporate various types of failure messages.
                info.message = getContext().getResources().getQuantityString(
                        stringRes, itemCount, itemCount);
                info.link = getContext().getString(R.string.details_link);
            }
        }

        info.resultState = isResultState(offlineItemState) ? offlineItemState : null;

        if (info.equals(mCurrentInfo)) return;

        boolean startTimer = showAccelerating || infoBarState == DownloadInfoBarState.SHOW_RESULT;

        clearEndTimerRunnable();

        if (startTimer) {
            long delay =
                    showAccelerating ? getDurationAcceleratedInfoBar() : getDurationShowResult();
            mEndTimerRunnable = () -> {
                mEndTimerRunnable = null;
                if (mCurrentInfo != null) mCurrentInfo.resultState = null;
                if (infoBarState == DownloadInfoBarState.SHOW_RESULT) {
                    clearFinishedItems(offlineItemState);
                }
                computeNextStepForUpdate(null, false, false, false);
            };
            mHandler.postDelayed(mEndTimerRunnable, delay);
        }

        setForceReparent(info);
        setAccessibilityMessage(
                info, showAccelerating && infoBarState == DownloadInfoBarState.DOWNLOADING);
        showInfoBar(infoBarState, info);
    }

    private void setForceReparent(DownloadProgressInfoBarData info) {
        info.downloadCount = getDownloadCount();
        info.forceReparent = !info.downloadCount.equals(
                mCurrentInfo == null ? null : mCurrentInfo.downloadCount);
    }

    private void setAccessibilityMessage(
            DownloadProgressInfoBarData info, boolean showAccelerating) {
        info.accessibilityMessage = TextUtils.isEmpty(info.link)
                ? info.message
                : getContext().getString(R.string.download_infobar_accessibility_message_with_link,
                          info.message, info.link);
    }

    private void clearEndTimerRunnable() {
        mHandler.removeCallbacks(mEndTimerRunnable);
        mEndTimerRunnable = null;
    }

    private boolean isResultState(@OfflineItemState int offlineItemState) {
        return offlineItemState == OfflineItemState.COMPLETE
                || offlineItemState == OfflineItemState.FAILED
                || offlineItemState == OfflineItemState.PENDING;
    }

    private void preProcessUpdatedItem(OfflineItem updatedItem) {
        if (updatedItem == null) return;

        // INTERRUPTED downloads should be treated as PENDING in the infobar. From here onwards,
        // there should be no INTERRUPTED state in the core logic.
        if (updatedItem.state == OfflineItemState.INTERRUPTED) {
            updatedItem.state = OfflineItemState.PENDING;
        }
    }

    private boolean itemResumedFromPending(OfflineItem updatedItem) {
        if (updatedItem == null || !mTrackedItems.containsKey(updatedItem.id)) return false;

        return mTrackedItems.get(updatedItem.id).state == OfflineItemState.PENDING
                && updatedItem.state == OfflineItemState.IN_PROGRESS;
    }

    @VisibleForTesting
    protected long getDurationAcceleratedInfoBar() {
        return DURATION_ACCELERATED_INFOBAR_IN_MS;
    }

    @VisibleForTesting
    protected long getDurationShowResult() {
        return DURATION_SHOW_RESULT_IN_MS;
    }

    @VisibleForTesting
    protected boolean isSpeedingUpMessageEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR, SPEEDING_UP_MESSAGE_ENABLED, false);
    }

    private boolean isAccelerated(OfflineItem offlineItem) {
        return isSpeedingUpMessageEnabled() && offlineItem != null && offlineItem.isAccelerated;
    }

    private boolean areAnimationsDisabled() {
        return DeviceConditions.isCurrentlyInPowerSaveMode(getContext());
    }

    /**
     * Central function called to show an InfoBar. If the previous InfoBar was on a different
     * tab which is currently not active, based on the value of |info.forceReparent|, it is
     * determines whether to close the InfoBar and recreate or simply update the existing InfoBar.
     * @param state The state of the infobar to be shown.
     * @param info Contains the information to be displayed in the UI.
     */
    @VisibleForTesting
    protected void showInfoBar(@DownloadInfoBarState int state, DownloadProgressInfoBarData info) {
        if (!FeatureUtilities.isDownloadProgressInfoBarEnabled()) return;

        mCurrentInfo = info;

        Tab currentTab = getCurrentTab();
        if (currentTab != null && (info.forceReparent || mCurrentInfoBar == null)) {
            showInfoBarForCurrentTab(info);
        } else {
            updateExistingInfoBar(info);
        }

        recordInfoBarState(state, info);
    }

    private void showInfoBarForCurrentTab(DownloadProgressInfoBarData info) {
        Tab currentTab = getCurrentTab();
        Tab prevTab = mCurrentInfoBar != null ? mCurrentInfoBar.getTab() : null;

        if (currentTab != prevTab) {
            if (mCurrentInfoBar != null) recordTabReparented();
            closePreviousInfoBar();
        }
        if (mCurrentInfoBar == null) {
            createInfoBar(info);
        } else {
            updateExistingInfoBar(info);
        }
    }

    private void createInfoBar(DownloadProgressInfoBarData info) {
        Tab currentTab = getCurrentTab();
        if (currentTab == null) return;

        InfoBarContainer.get(currentTab).addObserver(mInfoBarContainerObserver);
        DownloadProgressInfoBar.createInfoBar(mClient, currentTab, info);
        recordInfoBarCreated();
    }

    private void updateExistingInfoBar(DownloadProgressInfoBarData info) {
        if (mCurrentInfoBar == null) return;

        mCurrentInfoBar.updateInfoBar(info);
    }

    @VisibleForTesting
    protected void closePreviousInfoBar() {
        if (mCurrentInfoBar == null) return;

        Tab prevTab = mCurrentInfoBar.getTab();
        if (prevTab != null) {
            InfoBarContainer.get(prevTab).removeObserver(mInfoBarContainerObserver);
        }

        mCurrentInfoBar.closeInfoBar();
        mCurrentInfoBar = null;
    }

    private InfoBarContainer.InfoBarContainerObserver mInfoBarContainerObserver =
            new InfoBarContainer.InfoBarContainerObserver() {
                @Override
                public void onAddInfoBar(
                        InfoBarContainer container, InfoBar infoBar, boolean isFirst) {
                    if (infoBar.getInfoBarIdentifier()
                            != InfoBarIdentifier.DOWNLOAD_PROGRESS_INFOBAR_ANDROID) {
                        return;
                    }

                    mCurrentInfoBar = (DownloadProgressInfoBar) infoBar;
                }

                @Override
                public void onRemoveInfoBar(
                        InfoBarContainer container, InfoBar infoBar, boolean isLast) {
                    if (infoBar.getInfoBarIdentifier()
                            != InfoBarIdentifier.DOWNLOAD_PROGRESS_INFOBAR_ANDROID) {
                        return;
                    }

                    mCurrentInfoBar = null;
                    container.removeObserver(this);
                }

                @Override
                public void onInfoBarContainerAttachedToWindow(boolean hasInfobars) {}

                @Override
                public void onInfoBarContainerShownRatioChanged(
                        InfoBarContainer container, float shownRatio) {}
            };

    private void maybeShowDownloadsStillInProgressIPH() {
        if (getDownloadCount().inProgress == 0) return;
        if (getCurrentTab() == null
                || !(getCurrentTab().getActivity() instanceof ChromeTabbedActivity)) {
            return;
        }

        ChromeTabbedActivity activity = (ChromeTabbedActivity) getCurrentTab().getActivity();
        Profile profile = mIsIncognito ? Profile.getLastUsedProfile().getOffTheRecordProfile()
                                       : Profile.getLastUsedProfile().getOriginalProfile();
        ToolbarButtonInProductHelpController.maybeShowDownloadContinuingIPH(activity, profile);
    }

    @Nullable
    private Tab getCurrentTab() {
        if (!ApplicationStatus.hasVisibleActivities()) return null;
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeTabbedActivity)) return null;
        Tab tab = ((ChromeTabbedActivity) activity).getActivityTab();
        if (tab == null || tab.isIncognito() != mIsIncognito) return null;
        return tab;
    }

    private Context getContext() {
        return ContextUtils.getApplicationContext();
    }

    private DownloadCount getDownloadCount() {
        DownloadCount downloadCount = new DownloadCount();
        for (OfflineItem item : mTrackedItems.values()) {
            switch (item.state) {
                case OfflineItemState.IN_PROGRESS:
                    downloadCount.inProgress++;
                    break;
                case OfflineItemState.COMPLETE:
                    downloadCount.completed++;
                    break;
                case OfflineItemState.FAILED:
                    downloadCount.failed++;
                    break;
                case OfflineItemState.CANCELLED:
                    break;
                case OfflineItemState.PENDING:
                    downloadCount.pending++;
                    break;
                case OfflineItemState.INTERRUPTED: // intentional fall through
                case OfflineItemState.PAUSED: // intentional fall through
                default:
                    assert false;
            }
        }

        return downloadCount;
    }

    /**
     * Clears the items in finished state, i.e. completed, failed or pending.
     * @param states States to be removed.
     */
    private void clearFinishedItems(Integer... states) {
        Set<Integer> statesToRemove = new HashSet<>(Arrays.asList(states));
        List<ContentId> idsToRemove = new ArrayList<>();
        for (ContentId id : mTrackedItems.keySet()) {
            OfflineItem item = mTrackedItems.get(id);
            if (item == null) continue;
            if (statesToRemove.contains(item.state)) idsToRemove.add(id);
        }

        for (ContentId id : idsToRemove) {
            mTrackedItems.remove(id);
            mNotificationIds.remove(id);
        }
    }

    private OfflineContentProvider getOfflineContentProvider() {
        return OfflineContentAggregatorFactory.get();
    }

    private void removeNotification(ContentId contentId) {
        if (!mNotificationIds.containsKey(contentId)) return;

        DownloadInfo downloadInfo = new DownloadInfo.Builder().setContentId(contentId).build();
        DownloadManagerService.getDownloadManagerService()
                .getDownloadNotifier()
                .removeDownloadNotification(mNotificationIds.get(contentId), downloadInfo);
        mNotificationIds.remove(contentId);
    }

    private class DownloadProgressInfoBarClient implements DownloadProgressInfoBar.Client {
        @Override
        public void onLinkClicked(ContentId itemId) {
            mTrackedItems.remove(itemId);
            removeNotification(itemId);
            if (itemId != null) {
                DownloadUtils.openItem(
                        itemId, mIsIncognito, DownloadOpenSource.DOWNLOAD_PROGRESS_INFO_BAR);
            } else {
                DownloadManagerService.getDownloadManagerService().openDownloadsPage(
                        getContext(), DownloadOpenSource.DOWNLOAD_PROGRESS_INFO_BAR);
            }
            recordLinkClicked(itemId != null);
            closePreviousInfoBar();
        }

        @Override
        public void onInfoBarClosed(boolean explicitly) {
            // TODO(shaktisahu) : Remove param |explicitly| if it ends up unused.
            if (explicitly) {
                recordCloseButtonClicked();
                maybeShowDownloadsStillInProgressIPH();
                computeNextStepForUpdate(null, false, true, false);
            }
        }
    }

    private void recordInfoBarState(
            @DownloadInfoBarState int state, DownloadProgressInfoBarData info) {
        int shownState = -1;
        int multipleDownloadState = -1;
        if (state == DownloadInfoBarState.DOWNLOADING) {
            shownState = mEndTimerRunnable != null
                    ? UmaInfobarShown.ACCELERATED
                    : (info.downloadCount.inProgress == 1 ? UmaInfobarShown.DOWNLOADING
                                                          : UmaInfobarShown.MULTIPLE_DOWNLOADING);
        } else if (state == DownloadInfoBarState.SHOW_RESULT) {
            switch (info.resultState) {
                case OfflineItemState.COMPLETE:
                    shownState = info.downloadCount.completed == 1
                            ? UmaInfobarShown.COMPLETE
                            : UmaInfobarShown.MULTIPLE_COMPLETE;
                    break;
                case OfflineItemState.FAILED:
                    shownState = info.downloadCount.failed == 1 ? UmaInfobarShown.FAILED
                                                                : UmaInfobarShown.MULTIPLE_FAILED;
                    break;
                case OfflineItemState.PENDING:
                    shownState = info.downloadCount.pending == 1 ? UmaInfobarShown.PENDING
                                                                 : UmaInfobarShown.MULTIPLE_PENDING;
                    break;
                default:
                    assert false : "Unexpected state " + info.resultState;
                    break;
            }
        }

        assert shownState != -1 : "Invalid state " + state;

        RecordHistogram.recordEnumeratedHistogram(
                "Android.Download.InfoBar.Shown", shownState, UmaInfobarShown.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram("Android.Download.InfoBar.Shown",
                UmaInfobarShown.ANY_STATE, UmaInfobarShown.NUM_ENTRIES);
        if (multipleDownloadState != -1) {
            RecordHistogram.recordEnumeratedHistogram("Android.Download.InfoBar.Shown",
                    multipleDownloadState, UmaInfobarShown.NUM_ENTRIES);
        }
    }

    private void recordInfoBarCreated() {
        RecordUserAction.record("Android.Download.InfoBar.Shown");
    }

    private void recordCloseButtonClicked() {
        RecordUserAction.record("Android.Download.InfoBar.CloseButtonClicked");
        RecordHistogram.recordEnumeratedHistogram("Android.Download.InfoBar.CloseButtonClicked",
                mState, DownloadInfoBarState.NUM_ENTRIES);
    }

    private void recordLinkClicked(boolean openItem) {
        if (openItem) {
            RecordUserAction.record("Android.Download.InfoBar.LinkClicked.OpenDownload");
        } else {
            RecordUserAction.record("Android.Download.InfoBar.LinkClicked.OpenDownloadHome");
        }
    }

    private void recordTabReparented() {
        RecordUserAction.record("Android.Download.InfoBar.ReparentedToCurrentTab");
    }
}
