// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.app.Activity;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.SimpleAnimationLayout;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchObserver;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ScrimView.ScrimObserver;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;

import java.util.HashSet;
import java.util.PriorityQueue;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * This class is responsible for managing the content shown by the {@link BottomSheet}. Features
 * wishing to show content in the {@link BottomSheet} UI must implement {@link BottomSheetContent}
 * and call {@link #requestShowContent(BottomSheetContent, boolean)} which will return true if the
 * content was actually shown (see full doc on method).
 */
public class BottomSheetController implements ApplicationStatus.ActivityStateListener {
    /** The initial capacity for the priority queue handling pending content show requests. */
    private static final int INITIAL_QUEUE_CAPACITY = 1;

    /** The parameters that controll how the scrim behaves while the sheet is open. */
    private final ScrimParams mScrimParams;

    /** A handle to the {@link LayoutManager} to determine what state the browser is in. */
    private final LayoutManager mLayoutManager;

    /** A handle to the {@link BottomSheet} that this class controls. */
    private final BottomSheet mBottomSheet;

    /** A handle to the {@link SnackbarManager} that manages snackbars inside the bottom sheet. */
    private final SnackbarManager mSnackbarManager;

    /** A queue for content that is waiting to be shown in the {@link BottomSheet}. */
    private PriorityQueue<BottomSheetContent> mContentQueue;

    /** A set of contents that have requested to be shown, rather than just preloading. */
    private Set<BottomSheetContent> mFullShowRequestedSet;

    /** Whether the controller is already processing a hide request for the tab. */
    private boolean mIsProcessingHideRequest;

    /** Track whether the sheet was shown for the current tab. */
    private boolean mWasShownForCurrentTab;

    /** Whether composited UI is currently showing (such as Contextual Search). */
    private boolean mIsCompositedUIShowing;

    /** Whether the bottom sheet is temporarily suppressed. */
    private boolean mIsSuppressed;

    /** The manager for Contextual Search to attach listeners to. */
    private ContextualSearchManager mContextualSearchManager;

    /** Whether the bottom sheet should be suppressed when Contextual Search is showing. */
    private boolean mSuppressSheetForContextualSearch;

    /**
     * Build a new controller of the bottom sheet.
     * @param tabModelSelector A tab model selector to track events on tabs open in the browser.
     * @param layoutManager A layout manager for detecting changes in the active layout.
     * @param scrim The scrim that shows when the bottom sheet is opened.
     * @param bottomSheet The bottom sheet that this class will be controlling.
     * @param contextualSearchManager The manager for Contextual Search to attach listeners to.
     * @param suppressSheetForContextualSearch Whether the bottom sheet should be suppressed when
     *                                         Contextual Search is showing.
     */
    public BottomSheetController(final Activity activity, final TabModelSelector tabModelSelector,
            final LayoutManager layoutManager, final ScrimView scrim, BottomSheet bottomSheet,
            ContextualSearchManager contextualSearchManager,
            boolean suppressSheetForContextualSearch) {
        mBottomSheet = bottomSheet;
        mLayoutManager = layoutManager;
        mContextualSearchManager = contextualSearchManager;
        mSuppressSheetForContextualSearch = suppressSheetForContextualSearch;
        mSnackbarManager = new SnackbarManager(
                activity, mBottomSheet.findViewById(R.id.bottom_sheet_snackbar_container));
        mSnackbarManager.onStart();
        ApplicationStatus.registerStateListenerForActivity(this, activity);
        mFullShowRequestedSet = new HashSet<>();

        // Watch for navigation and tab switching that close the sheet.
        new TabModelSelectorTabObserver(tabModelSelector) {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                if (tab != tabModelSelector.getCurrentTab()) return;
                clearRequestsAndHide();
            }

            @Override
            public void onCrash(Tab tab, boolean sadTabShown) {
                if (tab != tabModelSelector.getCurrentTab()) return;
                clearRequestsAndHide();
            }
        };

        final TabModelObserver tabSelectionObserver = new EmptyTabModelObserver() {
            /** The currently active tab. */
            private Tab mCurrentTab = tabModelSelector.getCurrentTab();

            @Override
            public void didSelectTab(Tab tab, @TabModel.TabSelectionType int type, int lastId) {
                if (tab == mCurrentTab) return;
                mCurrentTab = tab;
                clearRequestsAndHide();
            }
        };

        tabModelSelector.getCurrentModel().addObserver(tabSelectionObserver);
        tabModelSelector.addObserver(new TabModelSelectorObserver() {
            @Override
            public void onChange() {}

            @Override
            public void onNewTabCreated(Tab tab) {}

            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                if (oldModel != null) oldModel.removeObserver(tabSelectionObserver);
                newModel.addObserver(tabSelectionObserver);
                clearRequestsAndHide();
            }

            @Override
            public void onTabStateInitialized() {}
        });

        // If the layout changes (to tab switcher, toolbar swipe, etc.) hide the sheet.
        mLayoutManager.addSceneChangeObserver(new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {}

            @Override
            public void onSceneChange(Layout layout) {
                // If the tab did not change, reshow the existing content. Once the tab actually
                // changes, existing content and requests will be cleared.
                if (canShowInLayout(layout)) {
                    unsuppressSheet();
                } else if (!canShowInLayout(layout)) {
                    suppressSheet(StateChangeReason.COMPOSITED_UI);
                }
            }
        });

        ScrimObserver scrimObserver = new ScrimObserver() {
            @Override
            public void onScrimClick() {
                if (!mBottomSheet.isSheetOpen()) return;
                mBottomSheet.setSheetState(
                        mBottomSheet.getCurrentSheetContent().isPeekStateEnabled()
                                ? BottomSheet.SheetState.PEEK
                                : BottomSheet.SheetState.HIDDEN,
                        true, StateChangeReason.TAP_SCRIM);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mScrimParams = new ScrimParams(mBottomSheet, false, true, 0, scrimObserver);

        mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                scrim.showScrim(mScrimParams);
                scrim.setViewAlpha(0);
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                scrim.hideScrim(false);
            }

            @Override
            public void onTransitionPeekToHalf(float transitionFraction) {
                // TODO(mdjones): This event should not occur after the bottom sheet is closed.
                if (scrim.getVisibility() == View.VISIBLE) {
                    scrim.setViewAlpha(transitionFraction);
                }
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                mSnackbarManager.dismissAllSnackbars();
            }
        });

        // TODO(mdjones): This should be changed to a generic OverlayPanel observer.
        if (mContextualSearchManager != null && mSuppressSheetForContextualSearch) {
            mContextualSearchManager.addObserver(new ContextualSearchObserver() {
                @Override
                public void onShowContextualSearch(
                        @Nullable GSAContextDisplaySelection selectionContext) {
                    // Contextual Search can call this method more than once per show event.
                    if (mIsCompositedUIShowing) return;
                    mIsCompositedUIShowing = true;
                    suppressSheet(StateChangeReason.COMPOSITED_UI);
                }

                @Override
                public void onHideContextualSearch() {
                    mIsCompositedUIShowing = false;
                    unsuppressSheet();
                }
            });
        }

        // Initialize the queue with a comparator that checks content priority.
        mContentQueue = new PriorityQueue<>(INITIAL_QUEUE_CAPACITY,
                (content1, content2) -> content2.getPriority() - content1.getPriority());
    }

    /**
     * Temporarily suppress the bottom sheet while other UI is showing. This will not itself change
     * the content displayed by the sheet.
     * @param reason The reason the sheet was suppressed.
     */
    private void suppressSheet(@StateChangeReason int reason) {
        mIsSuppressed = true;
        mBottomSheet.setSheetState(BottomSheet.SheetState.HIDDEN, false, reason);
    }

    /**
     * Unsuppress the bottom sheet. This may or may not affect the sheet depending on the state of
     * the browser (i.e. the tab switcher may be showing).
     */
    private void unsuppressSheet() {
        if (!mIsSuppressed || !canShowInLayout(mLayoutManager.getActiveLayout())
                || !mWasShownForCurrentTab || isOtherUIObscuring()) {
            return;
        }
        mIsSuppressed = false;

        if (mBottomSheet.getCurrentSheetContent() != null) {
            mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, true);
        } else {
            // In the event the previous content was hidden, try to show the next one.
            showNextContent();
        }
    }

    /**
     * @return The {@link BottomSheet} controlled by this class.
     */
    public BottomSheet getBottomSheet() {
        return mBottomSheet;
    }

    /**
     * @return The {@link SnackbarManager} that manages snackbars inside the bottom sheet.
     */
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    /**
     * Request that some content be shown in the bottom sheet.
     * @param content The content to be shown in the bottom sheet.
     * @param animate Whether the appearance of the bottom sheet should be animated.
     * @return True if the content was shown, false if it was suppressed. Content is suppressed if
     *         higher priority content is in the sheet, the sheet is expanded beyond the peeking
     *         state, or the browser is in a mode that does not support showing the sheet.
     */
    public boolean requestShowContent(BottomSheetContent content, boolean animate) {
        // If pre-load failed, do nothing. The content will automatically be queued.
        mFullShowRequestedSet.add(content);
        if (!loadInternal(content)) return false;
        if (!mBottomSheet.isSheetOpen() && !isOtherUIObscuring()) {
            mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, animate);
        }
        mWasShownForCurrentTab = true;
        return true;
    }

    /**
     * Handles loading or suppressing of content based on priority.
     * @param content The content to load.
     * @return True if the content started loading.
     */
    private boolean loadInternal(BottomSheetContent content) {
        if (content == mBottomSheet.getCurrentSheetContent()) return true;
        if (!canShowInLayout(mLayoutManager.getActiveLayout())) return false;

        BottomSheetContent shownContent = mBottomSheet.getCurrentSheetContent();
        boolean shouldSuppressExistingContent = shownContent != null
                && content.getPriority() < shownContent.getPriority()
                && canBottomSheetSwitchContent();

        if (shouldSuppressExistingContent) {
            mContentQueue.add(mBottomSheet.getCurrentSheetContent());
            shownContent = content;
        } else if (mBottomSheet.getCurrentSheetContent() == null) {
            shownContent = content;
        } else {
            mContentQueue.add(content);
        }

        assert shownContent != null;
        mBottomSheet.showContent(shownContent);

        return shownContent == content;
    }

    /**
     * Hide content shown in the bottom sheet. If the content is not showing, this call retracts the
     * request to show it.
     * @param content The content to be hidden.
     * @param animate Whether the sheet should animate when hiding.
     */
    public void hideContent(BottomSheetContent content, boolean animate) {
        mFullShowRequestedSet.remove(content);

        if (content != mBottomSheet.getCurrentSheetContent()) {
            mContentQueue.remove(content);
            return;
        }

        // If the sheet is already processing a request to hide visible content, do nothing.
        if (mIsProcessingHideRequest) return;

        // Handle showing the next content if it exists.
        if (mBottomSheet.getSheetState() == BottomSheet.SheetState.HIDDEN) {
            // If the sheet is already hidden, simply show the next content.
            showNextContent();
        } else {
            // If the sheet wasn't hidden, wait for it to be before showing the next content.
            BottomSheetObserver hiddenSheetObserver = new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int currentState) {
                    // Don't do anything until the sheet is completely hidden.
                    if (currentState != BottomSheet.SheetState.HIDDEN) return;

                    showNextContent();
                    mBottomSheet.removeObserver(this);
                    mIsProcessingHideRequest = false;
                }
            };

            mIsProcessingHideRequest = true;
            mBottomSheet.addObserver(hiddenSheetObserver);
            mBottomSheet.setSheetState(BottomSheet.SheetState.HIDDEN, animate);
        }
    }

    /**
     * Expand the {@link BottomSheet}. If there is no content in the sheet, this is a noop.
     */
    public void expandSheet() {
        if (mBottomSheet.getCurrentSheetContent() == null) return;
        mBottomSheet.setSheetState(BottomSheet.SheetState.HALF, true);
        if (mContextualSearchManager != null) {
            mContextualSearchManager.hideContextualSearch(OverlayPanel.StateChangeReason.UNKNOWN);
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STARTED) {
            mSnackbarManager.onStart();
        } else if (newState == ActivityState.STOPPED) {
            mSnackbarManager.onStop();
        } else if (newState == ActivityState.DESTROYED) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
    }

    /**
     * Show the next {@link BottomSheetContent} if it is available and peek the sheet. If no content
     * is available the sheet's content is set to null.
     */
    private void showNextContent() {
        if (mContentQueue.isEmpty()) {
            mBottomSheet.showContent(null);
            return;
        }

        BottomSheetContent nextContent = mContentQueue.poll();
        mBottomSheet.showContent(nextContent);
        if (mFullShowRequestedSet.contains(nextContent)) {
            mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, true);
        }
    }

    /**
     * Clear all the content show requests and hide the current content.
     */
    private void clearRequestsAndHide() {
        mContentQueue.clear();
        mFullShowRequestedSet.clear();
        // TODO(mdjones): Replace usages of bottom sheet with a model in line with MVC.
        // TODO(mdjones): It would probably be useful to expose an observer method that notifies
        //                objects when all content requests are cleared.
        hideContent(mBottomSheet.getCurrentSheetContent(), true);
        mWasShownForCurrentTab = false;
        mIsSuppressed = false;
    }

    /**
     * @param layout A {@link Layout} to check if the sheet can be shown in.
     * @return Whether the bottom sheet can show in the specified layout.
     */
    protected boolean canShowInLayout(Layout layout) {
        return layout instanceof StaticLayout || layout instanceof SimpleAnimationLayout;
    }

    /**
     * @return Whether some other UI is preventing the sheet from showing.
     */
    protected boolean isOtherUIObscuring() {
        return mIsCompositedUIShowing;
    }

    /**
     * @return Whether the sheet currently supports switching its content.
     */
    protected boolean canBottomSheetSwitchContent() {
        return !mBottomSheet.isSheetOpen();
    }
}
