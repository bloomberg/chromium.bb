// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_LENS_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_TITLE_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.QUERY_TILES_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.RESET_TASK_SURFACE_HEADER_SCROLL_POSITION;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.RESET_FEED_SURFACE_SCROLL_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.jank_tracker.JankScenario;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.start_surface.R;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator implements StartSurface.Controller, TabSwitcher.OverviewModeObserver,
                                      View.OnClickListener, StartSurface.OnTabSelectingListener {
    /** Interface to initialize a secondary tasks surface for more tabs. */
    interface SecondaryTasksSurfaceInitializer {
        /**
         * Initialize the secondary tasks surface and return the surface controller, which is
         * TabSwitcher.Controller.
         * @return The {@link TabSwitcher.Controller} of the secondary tasks surface.
         */
        TabSwitcher.Controller initialize();
    }

    /**
     * Interface to check the associated activity state.
     */
    interface ActivityStateChecker {
        /**
         * @return Whether the associated activity is finishing or destroyed.
         */
        boolean isFinishingOrDestroyed();
    }

    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final TabSwitcher.Controller mController;
    private final TabModelSelector mTabModelSelector;
    @Nullable
    private final PropertyModel mPropertyModel;
    @Nullable
    private final SecondaryTasksSurfaceInitializer mSecondaryTasksSurfaceInitializer;
    private final boolean mIsStartSurfaceEnabled;
    private final ObserverList<StartSurface.StateObserver> mStateObservers = new ObserverList<>();
    private final boolean mHadWarmStart;
    private final boolean mExcludeQueryTiles;

    // Boolean histogram used to record whether cached
    // ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE is consistent with
    // Pref.ARTICLES_LIST_VISIBLE.
    @VisibleForTesting
    static final String FEED_VISIBILITY_CONSISTENCY =
            "Startup.Android.CachedFeedVisibilityConsistency";
    @Nullable
    private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;
    @Nullable
    private TabSwitcher.Controller mSecondaryTasksSurfaceController;
    @Nullable
    private PropertyModel mSecondaryTasksSurfacePropertyModel;
    private boolean mIsIncognito;
    @Nullable
    private OmniboxStub mOmniboxStub;
    private Context mContext;
    @Nullable
    UrlFocusChangeListener mUrlFocusChangeListener;
    @StartSurfaceState
    private int mStartSurfaceState;
    @StartSurfaceState
    private int mPreviousStartSurfaceState;
    @NewTabPageLaunchOrigin
    private int mLaunchOrigin;
    @Nullable
    private TabModel mNormalTabModel;
    @Nullable
    private TabModelObserver mNormalTabModelObserver;
    @Nullable
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    private BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private ActivityStateChecker mActivityStateChecker;
    private boolean mExcludeMVTiles;
    private OneshotSupplier<StartSurface> mStartSurfaceSupplier;
    /**
     * Whether a pending observer needed be added to the normal TabModel after the TabModel is
     * initialized.
     */
    private boolean mPendingObserver;
    /**
     * The value of {@link Pref#ARTICLES_LIST_VISIBLE} on Startup. Getting this value for recording
     * the consistency of {@link ChromePreferenceKeys#FEED_ARTICLES_LIST_VISIBLE} with {@link
     * Pref#ARTICLES_LIST_VISIBLE}.
     */
    private Boolean mFeedVisibilityPrefOnStartUp;
    /**
     * The value of {@link ChromePreferenceKeys#FEED_ARTICLES_LIST_VISIBLE} on Startup. Getting this
     * value for recording the consistency with {@link Pref#ARTICLES_LIST_VISIBLE}.
     */
    @Nullable
    private Boolean mFeedVisibilityInSharedPreferenceOnStartUp;
    private boolean mHasFeedPlaceholderShown;
    private final JankTracker mJankTracker;
    private boolean mHideMVForNewSurface;
    private boolean mHideTabCarouselForNewSurface;
    private boolean mHideOverviewOnTabSelecting = true;
    private StartSurface.OnTabSelectingListener mOnTabSelectingListener;

    StartSurfaceMediator(TabSwitcher.Controller controller, TabModelSelector tabModelSelector,
            @Nullable PropertyModel propertyModel,
            @Nullable SecondaryTasksSurfaceInitializer secondaryTasksSurfaceInitializer,
            boolean isStartSurfaceEnabled, Context context,
            BrowserControlsStateProvider browserControlsStateProvider,
            ActivityStateChecker activityStateChecker, boolean excludeMVTiles,
            boolean excludeQueryTiles, OneshotSupplier<StartSurface> startSurfaceSupplier,
            boolean hadWarmStart, JankTracker jankTracker) {
        mController = controller;
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mSecondaryTasksSurfaceInitializer = secondaryTasksSurfaceInitializer;
        mIsStartSurfaceEnabled = isStartSurfaceEnabled;
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mActivityStateChecker = activityStateChecker;
        mExcludeMVTiles = excludeMVTiles;
        mExcludeQueryTiles = excludeQueryTiles;
        mStartSurfaceSupplier = startSurfaceSupplier;
        mHadWarmStart = hadWarmStart;
        mJankTracker = jankTracker;
        mLaunchOrigin = NewTabPageLaunchOrigin.UNKNOWN;

        if (mPropertyModel != null) {
            assert mIsStartSurfaceEnabled;

            mIsIncognito = mTabModelSelector.isIncognitoSelected();

            mTabModelSelectorObserver = new TabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    // TODO(crbug.com/982018): Optimize to not listen for selected Tab model change
                    // when overview is not shown.
                    updateIncognitoMode(newModel.isIncognito());
                }
            };
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            mPropertyModel.set(MORE_TABS_CLICK_LISTENER, this);

            // Hide tab carousel, which does not exist in incognito mode, when closing all
            // normal tabs.
            mNormalTabModelObserver = new TabModelObserver() {
                @Override
                public void willCloseTab(Tab tab, boolean animate) {
                    if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                            && mTabModelSelector.getModel(false).getCount() <= 1) {
                        setTabCarouselVisibility(false);
                    }
                }
                @Override
                public void tabClosureUndone(Tab tab) {
                    if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
                        setTabCarouselVisibility(true);
                    }
                }

                @Override
                public void restoreCompleted() {
                    if (!(mPropertyModel.get(IS_SHOWING_OVERVIEW)
                                && mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE)) {
                        return;
                    }
                    setTabCarouselVisibility(
                            mTabModelSelector.getModel(false).getCount() > 0 && !mIsIncognito);
                }

                @Override
                public void willAddTab(Tab tab, @TabLaunchType int type) {
                    // When the tab model is empty and a new background tab is added, it is
                    // immediately selected, which normally causes the overview to hide. We
                    // don't want to hide the overview when creating a tab in the background, so
                    // when a background tab is added to an empty tab model, we should skip the next
                    // onTabSelecting().
                    int tabCount = mTabModelSelector.getModel(false).getCount();
                    mHideOverviewOnTabSelecting =
                            tabCount != 0 || type != TabLaunchType.FROM_LONGPRESS_BACKGROUND;

                    // Updates the visibility of the tab switcher module if it is invisible and a
                    // new Tab is created in the background without hiding the Start surface
                    // homepage.
                    if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                            && !mHideOverviewOnTabSelecting
                            && !mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE)) {
                        setTabCarouselVisibility(!mIsIncognito && !mHideTabCarouselForNewSurface);
                    }
                }
            };
            if (mTabModelSelector.getModels().isEmpty()) {
                TabModelSelectorObserver selectorObserver = new TabModelSelectorObserver() {
                    @Override
                    public void onChange() {
                        assert !mTabModelSelector.getModels().isEmpty();
                        assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                                false)
                                != null;
                        assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true)
                                != null;
                        mTabModelSelector.removeObserver(this);
                        mNormalTabModel = mTabModelSelector.getModel(false);
                        if (mPendingObserver) {
                            mPendingObserver = false;
                            mNormalTabModel.addObserver(mNormalTabModelObserver);
                        }
                    }
                };
                mTabModelSelector.addObserver(selectorObserver);
            } else {
                mNormalTabModel = mTabModelSelector.getModel(false);
            }

            mBrowserControlsObserver = new BrowserControlsStateProvider.Observer() {
                @Override
                public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                        int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                    mPropertyModel.set(
                            TOP_MARGIN, mBrowserControlsStateProvider.getContentOffset());
                }

                @Override
                public void onBottomControlsHeightChanged(
                        int bottomControlsHeight, int bottomControlsMinHeight) {
                    // Only pad single pane home page since tabs grid has already been
                    // padded for the bottom bar.
                    if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
                        mPropertyModel.set(BOTTOM_BAR_HEIGHT, bottomControlsHeight);
                    }
                }
            };

            mUrlFocusChangeListener = new UrlFocusChangeListener() {
                @Override
                public void onUrlFocusChange(boolean hasFocus) {
                    assert !mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE);
                    if (hasFakeSearchBox()) setFakeBoxVisibility(!hasFocus);
                    notifyStateChange();
                }
            };

            // Tweak the margins between sections.
            Resources resources = mContext.getResources();
            mPropertyModel.set(TASKS_SURFACE_BODY_TOP_MARGIN,
                    resources.getDimensionPixelSize(R.dimen.tasks_surface_body_top_margin));
            mPropertyModel.set(MV_TILES_CONTAINER_TOP_MARGIN,
                    resources.getDimensionPixelSize(R.dimen.mv_tiles_container_top_margin));
            mPropertyModel.set(TAB_SWITCHER_TITLE_TOP_MARGIN,
                    resources.getDimensionPixelSize(R.dimen.tab_switcher_title_top_margin));
            mPropertyModel.set(FAKE_SEARCH_BOX_TOP_MARGIN,
                    resources.getDimensionPixelSize(R.dimen.fake_search_box_top_margin));
        }

        mController.addOverviewModeObserver(this);
        mPreviousStartSurfaceState = StartSurfaceState.NOT_SHOWN;
        mStartSurfaceState = StartSurfaceState.NOT_SHOWN;
    }

    void initWithNative(@Nullable OmniboxStub omniboxStub,
            @Nullable ExploreSurfaceCoordinatorFactory exploreSurfaceCoordinatorFactory,
            PrefService prefService) {
        mOmniboxStub = omniboxStub;
        mExploreSurfaceCoordinatorFactory = exploreSurfaceCoordinatorFactory;
        if (mPropertyModel != null) {
            assert mOmniboxStub != null;

            // Initialize
            // Note that isVoiceSearchEnabled will return false in incognito mode.
            mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mOmniboxStub.getVoiceRecognitionHandler().isVoiceSearchEnabled());
            boolean shouldShowLensButton = mOmniboxStub.isLensEnabled(LensEntryPoint.TASKS_SURFACE);
            LensMetrics.recordShown(LensEntryPoint.TASKS_SURFACE, shouldShowLensButton);
            mPropertyModel.set(IS_LENS_BUTTON_VISIBLE, shouldShowLensButton);

            if (mController.overviewVisible()) {
                mOmniboxStub.addUrlFocusChangeListener(mUrlFocusChangeListener);
                if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                        && mExploreSurfaceCoordinatorFactory != null) {
                    setExploreSurfaceVisibility(!mIsIncognito);
                }
            }
        }

        mFeedVisibilityPrefOnStartUp = prefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE);
    }

    void setSecondaryTasksSurfacePropertyModel(PropertyModel propertyModel) {
        mSecondaryTasksSurfacePropertyModel = propertyModel;
        mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);

        // Secondary tasks surface is used for more Tabs or incognito mode single pane, where MV
        // tiles and voice recognition button should be invisible.
        mSecondaryTasksSurfacePropertyModel.set(MV_TILES_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(QUERY_TILES_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(IS_LENS_BUTTON_VISIBLE, false);
    }

    void addStateChangeObserver(StartSurface.StateObserver observer) {
        mStateObservers.addObserver(observer);
    }

    void removeStateChangeObserver(StartSurface.StateObserver observer) {
        mStateObservers.removeObserver(observer);
    }

    // Implements StartSurface.Controller
    @Override
    public boolean overviewVisible() {
        return mController.overviewVisible();
    }

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, audit the wording usage and see if we can rename this method to
    // setStartSurfaceState.
    @Override
    public void setOverviewState(
            @StartSurfaceState int state, @NewTabPageLaunchOrigin int launchOrigin) {
        // TODO(crbug.com/1039691): Refactor into state and trigger to separate SHOWING and SHOWN
        // states.

        if (mPropertyModel == null || state == mStartSurfaceState) return;

        // Cache previous state.
        if (mStartSurfaceState != StartSurfaceState.NOT_SHOWN) {
            mPreviousStartSurfaceState = mStartSurfaceState;
        }

        mStartSurfaceState = state;
        setOverviewStateInternal();

        // Immediately transition from SHOWING to SHOWN state if overview is visible but state not
        // SHOWN. This is only necessary when the new state is a SHOWING state.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mStartSurfaceState != StartSurfaceState.NOT_SHOWN
                && !isShownState(mStartSurfaceState)) {
            // Compute SHOWN state before updating previous state, because the previous state is
            // still needed to compute the shown state.
            @StartSurfaceState
            int shownState = computeOverviewStateShown();

            mStartSurfaceState = shownState;
            setOverviewStateInternal();
        }
        notifyStateChange();

        // Start/Stop tracking jank for Homepage and Tab switcher. It is okay if finish or start are
        // called multiple times consecutively.
        switch (mStartSurfaceState) {
            case StartSurfaceState.NOT_SHOWN:
                mJankTracker.finishTrackingScenario(JankScenario.START_SURFACE_HOMEPAGE);
                mJankTracker.finishTrackingScenario(JankScenario.START_SURFACE_TAB_SWITCHER);
                break;
            case StartSurfaceState.SHOWN_HOMEPAGE:
                mJankTracker.startTrackingScenario(JankScenario.START_SURFACE_HOMEPAGE);
                mJankTracker.finishTrackingScenario(JankScenario.START_SURFACE_TAB_SWITCHER);
                break;
            case StartSurfaceState.SHOWN_TABSWITCHER:
                mJankTracker.finishTrackingScenario(JankScenario.START_SURFACE_HOMEPAGE);
                mJankTracker.startTrackingScenario(JankScenario.START_SURFACE_TAB_SWITCHER);
                break;
        }

        setLaunchOrigin(launchOrigin);
        // Metrics collection
        if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
            RecordUserAction.record("StartSurface.SinglePane.Home");
        } else if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
            RecordUserAction.record("StartSurface.SinglePane.Tabswitcher");
        }
    }

    @Override
    public void setOverviewState(@StartSurfaceState int state) {
        setOverviewState(state, mLaunchOrigin);
    }

    private void setLaunchOrigin(@NewTabPageLaunchOrigin int launchOrigin) {
        if (launchOrigin == NewTabPageLaunchOrigin.WEB_FEED) {
            StartSurfaceUserData.getInstance().saveFeedInstanceState(null);
        }
        mLaunchOrigin = launchOrigin;
        // If the ExploreSurfaceCoordinator is already initialized, set the TabId.
        if (mPropertyModel == null) return;
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        if (exploreSurfaceCoordinator != null) {
            exploreSurfaceCoordinator.setTabIdFromLaunchOrigin(mLaunchOrigin);
        }
    }

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, audit the wording usage and see if we can rename this method to
    // setStartSurfaceStateInternal.
    private void setOverviewStateInternal() {
        if (mStartSurfaceState == StartSurfaceState.SHOWING_HOMEPAGE) {
            // When entering the Start surface by tapping home button or new tab page, we need to
            // reset the scrolling position.
            mPropertyModel.set(RESET_TASK_SURFACE_HEADER_SCROLL_POSITION, true);
            mPropertyModel.set(RESET_FEED_SURFACE_SCROLL_POSITION, true);
            StartSurfaceUserData.getInstance().saveFeedInstanceState(null);

            String newHomeSurface =
                    StartSurfaceConfiguration.NEW_SURFACE_FROM_HOME_BUTTON.getValue();
            switch (newHomeSurface) {
                case "hide_tab_switcher_only":
                    mHideMVForNewSurface = false;
                    mHideTabCarouselForNewSurface = true;
                    break;
                case "hide_mv_tiles_and_tab_switcher":
                    mHideMVForNewSurface = true;
                    mHideTabCarouselForNewSurface = true;
                    break;
                default:
                    mHideMVForNewSurface = false;
                    mHideTabCarouselForNewSurface = false;
            }
        } else if (mStartSurfaceState == StartSurfaceState.SHOWING_START) {
            if (!mTabModelSelector.isIncognitoSelected()) {
                // Every time Chrome is started, no matter warm start or cold start, show MV tiles &
                // tab carousel.
                mHideMVForNewSurface = false;
                mHideTabCarouselForNewSurface = false;
            }
        } else if (mStartSurfaceState == StartSurfaceState.SHOWING_TABSWITCHER) {
            // Set secondary surface visible to make sure tab list recyclerview is updated in time
            // (before GTS animations start). We need to skip
            // mSecondaryTasksSurfaceController#showOverview here since it will hide GTS animations.
            setSecondaryTasksSurfaceVisibility(
                    /* isVisible= */ true, /* skipUpdateController = */ true);
        } else if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
            setExploreSurfaceVisibility(!mIsIncognito && mExploreSurfaceCoordinatorFactory != null);
            boolean hasNormalTab = getNormalTabCount() > 0;

            // If new home surface for home button is enabled, MV tiles and carousel tab switcher
            // will not show.
            setTabCarouselVisibility(
                    hasNormalTab && !mIsIncognito && !mHideTabCarouselForNewSurface);
            setMVTilesVisibility(!mIsIncognito && !mHideMVForNewSurface);
            // TODO(qinmin): show query tiles when flag is enabled.
            setQueryTilesVisibility(false);
            setFakeBoxVisibility(!mIsIncognito);
            setSecondaryTasksSurfaceVisibility(mIsIncognito, /* skipUpdateController = */ false);

            // Only pad single pane home page since tabs grid has already been padding for the
            // bottom bar.
            mPropertyModel.set(
                    BOTTOM_BAR_HEIGHT, mBrowserControlsStateProvider.getBottomControlsHeight());
            if (mNormalTabModel != null) {
                mNormalTabModel.addObserver(mNormalTabModelObserver);
            } else {
                mPendingObserver = true;
            }
        } else if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
            setTabCarouselVisibility(false);
            setMVTilesVisibility(false);
            setQueryTilesVisibility(false);
            setFakeBoxVisibility(false);
            setSecondaryTasksSurfaceVisibility(
                    /* isVisible= */ true, /* skipUpdateController = */ false);
            setExploreSurfaceVisibility(false);
        } else if (mStartSurfaceState == StartSurfaceState.NOT_SHOWN) {
            if (mSecondaryTasksSurfacePropertyModel != null) {
                setSecondaryTasksSurfaceVisibility(
                        /* isVisible= */ false, /* skipUpdateController = */ false);
            }
        }

        if (isShownState(mStartSurfaceState)) {
            setIncognitoModeDescriptionVisibility(
                    mIsIncognito && (mTabModelSelector.getModel(true).getCount() <= 0));
        }
    }

    @Override
    @StartSurfaceState
    public int getStartSurfaceState() {
        return mStartSurfaceState;
    }

    @Override
    public int getPreviousStartSurfaceState() {
        return mPreviousStartSurfaceState;
    }

    @Override
    public void addOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        mController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) {
            RecordUserAction.record("StartSurface.Shown");

            // update incognito
            mIsIncognito = mTabModelSelector.isIncognitoSelected();
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            // if OverviewModeState is NOT_SHOWN, default to SHOWING_TABSWITCHER. This should only
            // happen when entering Start through SwipeDown gesture on URL bar.
            if (mStartSurfaceState == StartSurfaceState.NOT_SHOWN) {
                mStartSurfaceState = StartSurfaceState.SHOWING_TABSWITCHER;
            }

            // set OverviewModeState
            @StartSurfaceState
            int shownState = computeOverviewStateShown();
            assert (isShownState(shownState));
            setOverviewState(shownState);

            // Make sure ExploreSurfaceCoordinator is built before the explore surface is showing
            // by default.
            if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    && mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) == null
                    && !mActivityStateChecker.isFinishingOrDestroyed()
                    && mExploreSurfaceCoordinatorFactory != null) {
                mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR,
                        mExploreSurfaceCoordinatorFactory.create(ColorUtils.inNightMode(mContext),
                                mHasFeedPlaceholderShown, mLaunchOrigin));
            }
            mTabModelSelector.addObserver(mTabModelSelectorObserver);

            if (mBrowserControlsObserver != null) {
                mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
            }

            mPropertyModel.set(TOP_MARGIN, mBrowserControlsStateProvider.getTopControlsHeight());

            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            if (mOmniboxStub != null) {
                mOmniboxStub.addUrlFocusChangeListener(mUrlFocusChangeListener);
            }
        }

        mController.showOverview(animate);
    }

    @Override
    public boolean onBackPressed() {
        boolean isOnHomepage = mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE;

        // When the SecondaryTasksSurface is shown, the TabGridDialog is controlled by
        // mSecondaryTasksSurfaceController, while the TabSelectionEditor dialog is controlled
        // by mController. Therefore, we need to check both controllers whether any dialog is
        // visible. If so, the corresponding controller will handle the back button.
        // When the Start surface is shown, tapping "Group Tabs" from menu will also show the
        // the TabSelectionEditor dialog. Therefore, we need to check both controllers as well.
        if (mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.isDialogVisible()) {
            return mSecondaryTasksSurfaceController.onBackPressed(isOnHomepage);
        } else if (mController.isDialogVisible()) {
            return mController.onBackPressed(isOnHomepage);
        }

        if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER) {
            if (mPreviousStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE && !mIsIncognito) {
                // Secondary tasks surface is used as the main surface in incognito mode.
                // If we reached Tab switcher from HomePage, and there isn't any dialog shown,
                // updates the state, and ChromeTabbedActivity will handle the back button.
                setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
                return true;
            } else {
                return mSecondaryTasksSurfaceController.onBackPressed(isOnHomepage);
            }
        }

        return mController.onBackPressed(isOnHomepage);
    }

    @Override
    public void enableRecordingFirstMeaningfulPaint(long activityCreateTimeMs) {
        mController.enableRecordingFirstMeaningfulPaint(activityCreateTimeMs);
    }

    void onOverviewShownAtLaunch(long activityCreationTimeMs) {
        mController.onOverviewShownAtLaunch(activityCreationTimeMs);
        if (mPropertyModel != null) {
            ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                    mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
            if (exploreSurfaceCoordinator != null) {
                exploreSurfaceCoordinator.onOverviewShownAtLaunch(activityCreationTimeMs);
            }
        }

        assert mFeedVisibilityInSharedPreferenceOnStartUp != null;
        if (mFeedVisibilityPrefOnStartUp != null) {
            RecordHistogram.recordBooleanHistogram(FEED_VISIBILITY_CONSISTENCY,
                    mFeedVisibilityPrefOnStartUp.equals(
                            mFeedVisibilityInSharedPreferenceOnStartUp));
        }
    }

    @Override
    public boolean inShowState() {
        return mStartSurfaceState != StartSurfaceState.NOT_SHOWN
                && mStartSurfaceState != StartSurfaceState.DISABLED;
    }

    // Implements TabSwitcher.OverviewModeObserver.
    @Override
    public void startedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedShowing();
        }

        if (BrowserStartupController.getInstance().isFullBrowserStarted()
                && StartSurfaceConfiguration.WARM_UP_RENDERER.getValue()) {
            StartSurfaceConfigurationJni.get().warmupRenderer(Profile.getLastUsedRegularProfile());
        }
    }

    @Override
    public void startedHiding() {
        if (mPropertyModel != null) {
            if (mOmniboxStub != null) {
                mOmniboxStub.removeUrlFocusChangeListener(mUrlFocusChangeListener);
            }
            mPropertyModel.set(IS_SHOWING_OVERVIEW, false);

            destroyExploreSurfaceCoordinator();
            if (mNormalTabModelObserver != null) {
                if (mNormalTabModel != null) {
                    mNormalTabModel.removeObserver(mNormalTabModelObserver);
                } else if (mPendingObserver) {
                    mPendingObserver = false;
                }
            }
            if (mTabModelSelectorObserver != null) {
                mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            }
            if (mBrowserControlsObserver != null) {
                mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
            }
            setOverviewState(StartSurfaceState.NOT_SHOWN);
            RecordUserAction.record("StartSurface.Hidden");
        }
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }

    private void destroyExploreSurfaceCoordinator() {
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        if (exploreSurfaceCoordinator != null) exploreSurfaceCoordinator.destroy();
        mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, null);
    }

    // TODO(crbug.com/982018): turn into onClickMoreTabs() and hide the OnClickListener signature
    // inside. Implements View.OnClickListener, which listens for the more tabs button.
    @Override
    public void onClick(View v) {
        assert mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE;

        if (mSecondaryTasksSurfacePropertyModel == null) {
            mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
            assert mSecondaryTasksSurfacePropertyModel != null;
        }

        RecordUserAction.record("StartSurface.SinglePane.MoreTabs");
        setOverviewState(StartSurfaceState.SHOWN_TABSWITCHER);
    }

    // StartSurface.OnTabSelectingListener
    @Override
    public void onTabSelecting(long time, int tabId) {
        if (!mHideOverviewOnTabSelecting) {
            mHideOverviewOnTabSelecting = true;
            return;
        }
        assert mOnTabSelectingListener != null;
        mOnTabSelectingListener.onTabSelecting(time, tabId);
    }

    public boolean shouldShowFeedPlaceholder() {
        if (mFeedVisibilityInSharedPreferenceOnStartUp == null) {
            mFeedVisibilityInSharedPreferenceOnStartUp =
                    StartSurfaceConfiguration.getFeedArticlesVisibility();
        }

        return mIsStartSurfaceEnabled
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)
                && StartSurfaceConfiguration.getFeedArticlesVisibility() && !mHadWarmStart
                && !mHasFeedPlaceholderShown;
    }

    public void setSecondaryTasksSurfaceController(
            TabSwitcher.Controller secondaryTasksSurfaceController) {
        mSecondaryTasksSurfaceController = secondaryTasksSurfaceController;
    }

    void setFeedPlaceholderHasShown() {
        mHasFeedPlaceholderShown = true;
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible && mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR) == null
                && !mActivityStateChecker.isFinishingOrDestroyed()) {
            mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR,
                    mExploreSurfaceCoordinatorFactory.create(ColorUtils.inNightMode(mContext),
                            mHasFeedPlaceholderShown, mLaunchOrigin));
        }

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, isVisible);

        // Pull-to-refresh is not supported when explore surface is not visible, i.e. in tab
        // switcher mode.
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR);
        if (exploreSurfaceCoordinator != null) {
            exploreSurfaceCoordinator.enableSwipeRefresh(isVisible);
        }
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mIsIncognito) return;
        mIsIncognito = isIncognito;

        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        setOverviewStateInternal();

        // TODO(crbug.com/1021399): This looks not needed since there is no way to change incognito
        // mode when focusing on the omnibox and incognito mode change won't affect the visibility
        // of the tab switcher toolbar.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)) notifyStateChange();
    }

    /**
     * Set the visibility of secondary tasks surface. Secondary tasks surface is used for showing
     * normal grid tab switcher and incognito gird tab switcher.
     * @param isVisible Whether secondary tasks surface is visible.
     * @param skipUpdateController Whether to skip mSecondaryTasksSurfaceController#showOverview and
     *         mSecondaryTasksSurfaceController#hideOverview.
     */
    private void setSecondaryTasksSurfaceVisibility(
            boolean isVisible, boolean skipUpdateController) {
        assert mIsStartSurfaceEnabled;

        if (isVisible) {
            if (mSecondaryTasksSurfacePropertyModel == null) {
                mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
            }
            if (mSecondaryTasksSurfacePropertyModel != null) {
                mSecondaryTasksSurfacePropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, false);
                mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);
            }
            if (mSecondaryTasksSurfaceController != null && !skipUpdateController) {
                mSecondaryTasksSurfaceController.showOverview(/* animate = */ true);
            }
        } else {
            if (mSecondaryTasksSurfaceController != null && !skipUpdateController) {
                mSecondaryTasksSurfaceController.hideOverview(/* animate = */ false);
            }
        }
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, isVisible);
    }

    private void notifyStateChange() {
        // StartSurface is being supplied with OneShotSupplier, notification sends after
        // StartSurface is available to avoid missing events. More detail see:
        // https://crrev.com/c/2427428.
        mStartSurfaceSupplier.onAvailable((unused) -> {
            for (StartSurface.StateObserver observer : mStateObservers) {
                observer.onStateChanged(mStartSurfaceState, shouldShowTabSwitcherToolbar());
            }
        });
    }

    private boolean hasFakeSearchBox() {
        // No fake search box on the explore pane in two panes mode.
        return mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE;
    }

    @VisibleForTesting
    public boolean shouldShowTabSwitcherToolbar() {
        // Always show in TABSWITCHER
        if (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                || mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE)) {
            return true;
        }

        // Hide when focusing the Omnibox on the primary surface.
        return hasFakeSearchBox() && mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE);
    }

    private void setTabCarouselVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE)) return;

        mPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, isVisible);
        mPropertyModel.set(
                IS_TAB_CAROUSEL_TITLE_VISIBLE, isVisible && mController.showTabSwitcherTitle());
    }

    private void setMVTilesVisibility(boolean isVisible) {
        if (mExcludeMVTiles || isVisible == mPropertyModel.get(MV_TILES_VISIBLE)) return;
        mPropertyModel.set(MV_TILES_VISIBLE, isVisible);
    }

    private void setQueryTilesVisibility(boolean isVisible) {
        if (mExcludeQueryTiles || isVisible == mPropertyModel.get(QUERY_TILES_VISIBLE)) return;
        mPropertyModel.set(QUERY_TILES_VISIBLE, isVisible);
    }

    private void setFakeBoxVisibility(boolean isVisible) {
        if (mPropertyModel == null) return;
        mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, isVisible);

        // This is because VoiceRecognitionHandler monitors incognito mode and returns
        // false in incognito mode. However, when switching incognito mode, this class is notified
        // earlier than the VoiceRecognitionHandler, so isVoiceSearchEnabled returns
        // incorrect state if check synchronously.
        ThreadUtils.postOnUiThread(() -> {
            if (mOmniboxStub != null) {
                if (mOmniboxStub.getVoiceRecognitionHandler() != null) {
                    mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                            mOmniboxStub.getVoiceRecognitionHandler().isVoiceSearchEnabled());
                }
                mPropertyModel.set(IS_LENS_BUTTON_VISIBLE,
                        mOmniboxStub.isLensEnabled(LensEntryPoint.TASKS_SURFACE));
            }
        });
    }

    private void setIncognitoModeDescriptionVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE)) return;

        if (!mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED)) {
            mPropertyModel.set(IS_INCOGNITO_DESCRIPTION_INITIALIZED, true);
        }
        mPropertyModel.set(IS_INCOGNITO_DESCRIPTION_VISIBLE, isVisible);
        mPropertyModel.set(IS_SURFACE_BODY_VISIBLE, !isVisible);
        if (mSecondaryTasksSurfacePropertyModel != null) {
            if (!mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED)) {
                mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO_DESCRIPTION_INITIALIZED, true);
            }
            mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO_DESCRIPTION_VISIBLE, isVisible);
            mSecondaryTasksSurfacePropertyModel.set(IS_SURFACE_BODY_VISIBLE, !isVisible);
        }
    }

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, audit the wording usage and see if we can rename this method to
    // computeStartSurfaceState.
    @StartSurfaceState
    private int computeOverviewStateShown() {
        if (mIsStartSurfaceEnabled) {
            if (mStartSurfaceState == StartSurfaceState.SHOWING_PREVIOUS) {
                assert mPreviousStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                        || mPreviousStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                        || mPreviousStartSurfaceState == StartSurfaceState.NOT_SHOWN;

                // This class would be re-instantiated after changing theme, then
                // mPreviousOverviewModeState will be reset to OverviewModeState.NOT_SHOWN. We
                // default to OverviewModeState.SHOWN_HOMEPAGE in this case when SHOWING_PREVIOUS.
                return mPreviousStartSurfaceState == StartSurfaceState.NOT_SHOWN
                        ? StartSurfaceState.SHOWN_HOMEPAGE
                        : mPreviousStartSurfaceState;
            } else if (mStartSurfaceState == StartSurfaceState.SHOWING_START) {
                if (mTabModelSelector.isIncognitoSelected()) {
                    return StartSurfaceState.SHOWN_TABSWITCHER;
                }
                return StartSurfaceState.SHOWN_HOMEPAGE;
            } else if (mStartSurfaceState == StartSurfaceState.SHOWING_TABSWITCHER) {
                return StartSurfaceState.SHOWN_TABSWITCHER;
            } else if (mStartSurfaceState == StartSurfaceState.SHOWING_HOMEPAGE) {
                return StartSurfaceState.SHOWN_HOMEPAGE;
            } else {
                assert (isShownState(mStartSurfaceState)
                        || mStartSurfaceState == StartSurfaceState.NOT_SHOWN);
                return mStartSurfaceState;
            }
        }
        return StartSurfaceState.DISABLED;
    }

    private boolean isShownState(@StartSurfaceState int state) {
        return state == StartSurfaceState.SHOWN_HOMEPAGE
                || state == StartSurfaceState.SHOWN_TABSWITCHER;
    }

    private int getNormalTabCount() {
        if (!mTabModelSelector.isTabStateInitialized()) {
            return SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.REGULAR_TAB_COUNT);
        } else {
            return mTabModelSelector.getModel(false).getCount();
        }
    }

    TabSwitcher.Controller getSecondaryTasksSurfaceController() {
        return mSecondaryTasksSurfaceController;
    }

    void setOnTabSelectingListener(StartSurface.OnTabSelectingListener onTabSelectingListener) {
        mOnTabSelectingListener = onTabSelectingListener;
    }
}
