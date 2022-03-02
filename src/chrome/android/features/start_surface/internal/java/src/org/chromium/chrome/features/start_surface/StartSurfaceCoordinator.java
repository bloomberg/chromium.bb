// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.feed.FeedLaunchReliabilityLoggingState;
import org.chromium.chrome.browser.feed.FeedSwipeRefreshLayout;
import org.chromium.chrome.browser.feed.ScrollListener;
import org.chromium.chrome.browser.feed.ScrollableContainerDelegate;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger.SurfaceType;
import org.chromium.chrome.start_surface.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final Activity mActivity;
    private final ScrimCoordinator mScrimCoordinator;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private final boolean mIsStartSurfaceEnabled;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mParentTabSupplier;
    private final WindowAndroid mWindowAndroid;
    private ViewGroup mContainerView;
    private final Supplier<DynamicResourceLoader> mDynamicResourceLoaderSupplier;
    private final TabModelSelector mTabModelSelector;
    private final BrowserControlsManager mBrowserControlsManager;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final Supplier<OmniboxStub> mOmniboxStubSupplier;
    private final TabContentManager mTabContentManager;
    private final ModalDialogManager mModalDialogManager;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TabCreatorManager mTabCreatorManager;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final Supplier<Toolbar> mToolbarSupplier;

    @VisibleForTesting
    static final String START_SHOWN_AT_STARTUP_UMA = "Startup.Android.StartSurfaceShownAtStartup";
    private static final String TAG = "StartSurface";

    // Non-null in SurfaceMode.SINGLE_PANE mode.
    @Nullable
    private TasksSurface mTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode.
    @Nullable
    private PropertyModelChangeProcessor mTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private TasksSurface mSecondaryTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private PropertyModelChangeProcessor mSecondaryTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.NO_START_SURFACE to show the tabs.
    @Nullable
    private TabSwitcher mTabSwitcher;

    // Non-null in SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;

    // Non-null in SurfaceMode.SINGLE_PANE modes.
    // TODO(crbug.com/982018): Get rid of this reference since the mediator keeps a reference to it.
    @Nullable
    private PropertyModel mPropertyModel;

    // Used to remember TabSwitcher.OnTabSelectingListener in SurfaceMode.SINGLE_PANE mode for more
    // tabs surface if necessary.
    @Nullable
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;

    // Whether the {@link initWithNative()} is called.
    private boolean mIsInitializedWithNative;

    // A flag of whether there is a pending call to {@link initialize()} but waiting for native's
    // initialization.
    private boolean mIsInitPending;

    private boolean mIsSecondaryTaskInitPending;
    private FeedPlaceholderCoordinator mFeedPlaceholderCoordinator;

    // Listeners used by the contained surfaces (e.g., Explore) to listen to the scroll changes on
    // the main scrollable container of the start surface.
    private final ObserverList<ScrollListener> mScrollListeners =
            new ObserverList<ScrollListener>();

    // Stores surface creation time for Feed launch reliability logging.
    private final FeedLaunchReliabilityLoggingState mFeedLaunchReliabilityLoggingState;

    @Nullable
    private AppBarLayout.OnOffsetChangedListener mOffsetChangedListenerToGenerateScrollEvents;

    // For pull-to-refresh.
    @Nullable
    private FeedSwipeRefreshLayout mSwipeRefreshLayout;

    private class ScrollableContainerDelegateImpl implements ScrollableContainerDelegate {
        @Override
        public void addScrollListener(ScrollListener listener) {
            mScrollListeners.addObserver(listener);
        }
        @Override
        public void removeScrollListener(ScrollListener listener) {
            mScrollListeners.removeObserver(listener);
        }

        @Override
        public int getVerticalScrollOffset() {
            // Always return a zero dummy value because the offset is directly provided
            // by the observer.
            return 0;
        }

        @Override
        public int getRootViewHeight() {
            return mContainerView.getHeight();
        }

        @Override
        public int getTopPositionRelativeToContainerView(View childView) {
            int[] pos = new int[2];
            ViewUtils.getRelativeLayoutPosition(mContainerView, childView, pos);
            return pos[1];
        }
    }

    /**
     * @param activity The current Android {@link Activity}.
     * @param scrimCoordinator The coordinator for the scrim widget.
     * @param sheetController Controls the bottom sheet.
     * @param startSurfaceOneshotSupplier Supplies the start surface.
     * @param parentTabSupplier Supplies the current parent {@link Tab}.
     * @param hadWarmStart Whether the application had a warm start.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param containerView The container {@link ViewGroup} for this ui, also the root view for
     *         StartSurface.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param browserControlsManager Manages the browser controls.
     * @param snackbarManager Manages the snackbar.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param omniboxStubSupplier Supplies the {@link OmniboxStub}.
     * @param tabContentManager Manages the tab content.
     * @param modalDialogManager Manages modal dialogs.
     * @param chromeActivityNativeDelegate An activity delegate to handle native initialization.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabCreatorManager Manages {@link Tab} creation.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param jankTracker Measures jank while feed or tab switcher are visible.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     */
    public StartSurfaceCoordinator(@NonNull Activity activity,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull BottomSheetController sheetController,
            @NonNull OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            @NonNull Supplier<Tab> parentTabSupplier, boolean hadWarmStart,
            @NonNull WindowAndroid windowAndroid, @NonNull ViewGroup containerView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull Supplier<OmniboxStub> omniboxStubSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull JankTracker jankTracker, @NonNull Supplier<Toolbar> toolbarSupplier) {
        mFeedLaunchReliabilityLoggingState =
                new FeedLaunchReliabilityLoggingState(SurfaceType.START_SURFACE, System.nanoTime());
        mActivity = activity;
        mScrimCoordinator = scrimCoordinator;
        mIsStartSurfaceEnabled = ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(mActivity);
        mBottomSheetController = sheetController;
        mParentTabSupplier = parentTabSupplier;
        mWindowAndroid = windowAndroid;
        mContainerView = containerView;
        mDynamicResourceLoaderSupplier = dynamicResourceLoaderSupplier;
        mTabModelSelector = tabModelSelector;
        mBrowserControlsManager = browserControlsManager;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mOmniboxStubSupplier = omniboxStubSupplier;
        mTabContentManager = tabContentManager;
        mModalDialogManager = modalDialogManager;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabCreatorManager = tabCreatorManager;
        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mToolbarSupplier = toolbarSupplier;

        boolean excludeMVTiles = StartSurfaceConfiguration.START_SURFACE_EXCLUDE_MV_TILES.getValue()
                || !mIsStartSurfaceEnabled;
        boolean excludeQueryTiles =
                StartSurfaceConfiguration.START_SURFACE_EXCLUDE_QUERY_TILES.getValue()
                || !mIsStartSurfaceEnabled;
        if (!mIsStartSurfaceEnabled) {
            // Create Tab switcher directly to save one layer in the view hierarchy.
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(activity,
                    activityLifecycleDispatcher, tabModelSelector, tabContentManager,
                    browserControlsManager, tabCreatorManager, menuOrKeyboardActionController,
                    containerView, shareDelegateSupplier, multiWindowModeStateDispatcher,
                    scrimCoordinator, /* rootView= */ containerView);
        } else {
            // createSwipeRefreshLayout has to be called before creating any surface.
            createSwipeRefreshLayout();
            createAndSetStartSurface(excludeMVTiles, excludeQueryTiles);
        }

        TabSwitcher.Controller controller =
                mTabSwitcher != null ? mTabSwitcher.getController() : mTasksSurface.getController();
        mStartSurfaceMediator =
                new StartSurfaceMediator(controller, mTabModelSelector, mPropertyModel,
                        mIsStartSurfaceEnabled ? this::initializeSecondaryTasksSurface : null,
                        mIsStartSurfaceEnabled, mActivity, mBrowserControlsManager,
                        this::isActivityFinishingOrDestroyed, excludeMVTiles, excludeQueryTiles,
                        startSurfaceOneshotSupplier, hadWarmStart, jankTracker);

        // Show feed loading image.
        if (mStartSurfaceMediator.shouldShowFeedPlaceholder()) {
            mFeedPlaceholderCoordinator = new FeedPlaceholderCoordinator(
                    mActivity, mTasksSurface.getBodyViewContainer(), false);
            mFeedPlaceholderCoordinator.setUpPlaceholderView();
            mStartSurfaceMediator.setFeedPlaceholderHasShown();
        }
        startSurfaceOneshotSupplier.set(this);
    }

    // Implements StartSurface.
    @Override
    public void initialize() {
        // TODO (crbug.com/1041047): Move more stuff from the constructor to here for lazy
        // initialization.
        if (!mIsInitializedWithNative) {
            mIsInitPending = true;
            return;
        }

        mIsInitPending = false;
        if (mTasksSurface != null) {
            mTasksSurface.initialize();
        }
    }

    @Override
    public void destroy() {
        onHide();
        if (mOffsetChangedListenerToGenerateScrollEvents != null) {
            removeHeaderOffsetChangeListener(mOffsetChangedListenerToGenerateScrollEvents);
            mOffsetChangedListenerToGenerateScrollEvents = null;
        }
    }

    @Override
    public void onHide() {
        if (mIsInitializedWithNative) {
            if (mTasksSurface != null) {
                mTasksSurface.onHide();
            }
            if (mSecondaryTasksSurface != null) {
                mSecondaryTasksSurface.onHide();
            }
        }
        if (mFeedPlaceholderCoordinator != null) {
            mFeedPlaceholderCoordinator.destroy();
            mFeedPlaceholderCoordinator = null;
        }
    }

    @Override
    public void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        // TODO (crbug.com/1113852): Add a header offset change listener for incognito homepage.
        if (mTasksSurface != null) {
            mTasksSurface.addHeaderOffsetChangeListener(onOffsetChangedListener);
        }
    }

    @Override
    public void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mTasksSurface != null) {
            mTasksSurface.removeHeaderOffsetChangeListener(onOffsetChangedListener);
        }
    }

    @Override
    public void addStateChangeObserver(StateObserver observer) {
        mStartSurfaceMediator.addStateChangeObserver(observer);
    }

    @Override
    public void removeStateChangeObserver(StateObserver observer) {
        mStartSurfaceMediator.removeStateChangeObserver(observer);
    }

    @Override
    public void setOnTabSelectingListener(StartSurface.OnTabSelectingListener listener) {
        mStartSurfaceMediator.setOnTabSelectingListener(listener);
        if (mTasksSurface != null) {
            mTasksSurface.setOnTabSelectingListener(mStartSurfaceMediator);
        } else {
            mTabSwitcher.setOnTabSelectingListener(mStartSurfaceMediator);
        }

        // Set OnTabSelectingListener to the more tabs tasks surface as well if it has been
        // instantiated, otherwise remember it for the future instantiation.
        if (mIsStartSurfaceEnabled) {
            if (mSecondaryTasksSurface == null) {
                mOnTabSelectingListener = mStartSurfaceMediator;
            } else {
                mSecondaryTasksSurface.setOnTabSelectingListener(mStartSurfaceMediator);
            }
        }
    }

    @Override
    public void initWithNative() {
        if (mIsInitializedWithNative) return;

        mIsInitializedWithNative = true;
        if (mIsStartSurfaceEnabled) {
            mExploreSurfaceCoordinatorFactory = new ExploreSurfaceCoordinatorFactory(mActivity,
                    mTasksSurface.getBodyViewContainer(), mPropertyModel, mBottomSheetController,
                    mParentTabSupplier, new ScrollableContainerDelegateImpl(), mSnackbarManager,
                    mShareDelegateSupplier, mWindowAndroid, mTabModelSelector, mToolbarSupplier,
                    mFeedLaunchReliabilityLoggingState, mSwipeRefreshLayout);
        }
        mStartSurfaceMediator.initWithNative(
                mIsStartSurfaceEnabled ? mOmniboxStubSupplier.get() : null,
                mExploreSurfaceCoordinatorFactory,
                UserPrefs.get(Profile.getLastUsedRegularProfile()));

        if (mTabSwitcher != null) {
            mTabSwitcher.initWithNative(mActivity, mTabContentManager,
                    mDynamicResourceLoaderSupplier.get(), mSnackbarManager, mModalDialogManager);
        }
        if (mTasksSurface != null) {
            mTasksSurface.onFinishNativeInitialization(mActivity, mOmniboxStubSupplier.get());
        }

        if (mIsInitPending) {
            initialize();
        }

        if (mIsSecondaryTaskInitPending) {
            mIsSecondaryTaskInitPending = false;
            mSecondaryTasksSurface.onFinishNativeInitialization(
                    mActivity, mOmniboxStubSupplier.get());
            mSecondaryTasksSurface.initialize();
        }
    }

    @Override
    public Controller getController() {
        return mStartSurfaceMediator;
    }

    @Override
    public TabSwitcher.TabListDelegate getGridTabListDelegate() {
        if (mIsStartSurfaceEnabled) {
            if (mSecondaryTasksSurface == null) {
                mStartSurfaceMediator.setSecondaryTasksSurfaceController(
                        initializeSecondaryTasksSurface());
            }
            return mSecondaryTasksSurface.getTabListDelegate();
        } else {
            return mTabSwitcher.getTabListDelegate();
        }
    }

    @Override
    public TabSwitcher.TabListDelegate getCarouselOrSingleTabListDelegate() {
        if (mIsStartSurfaceEnabled) {
            assert mTasksSurface != null;
            return mTasksSurface.getTabListDelegate();
        } else {
            return null;
        }
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        // If TabSwitcher has been created directly, use the TabGridDialogVisibilitySupplier from
        // TabSwitcher.
        if (mTabSwitcher != null) {
            return mTabSwitcher.getTabGridDialogVisibilitySupplier();
        }
        return () -> {
            // Return true if either mTasksSurface or mSecondaryTasksSurface has a visible dialog.
            assert mTasksSurface != null;
            if (mTasksSurface.getTabGridDialogVisibilitySupplier() != null) {
                if (mTasksSurface.getTabGridDialogVisibilitySupplier().get()) return true;
            }
            if (mSecondaryTasksSurface != null
                    && mSecondaryTasksSurface.getTabGridDialogVisibilitySupplier() != null) {
                if (mSecondaryTasksSurface.getTabGridDialogVisibilitySupplier().get()) return true;
            }
            return false;
        };
    }

    @Override
    public void onOverviewShownAtLaunch(
            boolean isOverviewShownOnStartup, long activityCreationTimeMs) {
        if (isOverviewShownOnStartup) {
            mStartSurfaceMediator.onOverviewShownAtLaunch(activityCreationTimeMs);
            if (mFeedPlaceholderCoordinator != null) {
                mFeedPlaceholderCoordinator.onOverviewShownAtLaunch(activityCreationTimeMs);
            }
        }
        if (StartSurfaceConfiguration.CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP.getValue()) {
            ReturnToChromeExperimentsUtil.cachePrimaryAccountSyncStatus();
        }
        if (ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(mActivity)) {
            Log.i(TAG, "Recorded %s = %b", START_SHOWN_AT_STARTUP_UMA, isOverviewShownOnStartup);
            RecordHistogram.recordBooleanHistogram(
                    START_SHOWN_AT_STARTUP_UMA, isOverviewShownOnStartup);
        }
        if (!TextUtils.isEmpty(StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue())) {
            ReturnToChromeExperimentsUtil.cacheSegmentationResult();
        }
    }

    @Override
    @Nullable
    public TasksSurface getPrimaryTasksSurface() {
        return mTasksSurface;
    }

    @VisibleForTesting
    public boolean isInitPendingForTesting() {
        return mIsInitPending;
    }

    @VisibleForTesting
    public boolean isInitializedWithNativeForTesting() {
        return mIsInitializedWithNative;
    }

    @VisibleForTesting
    public boolean isSecondaryTaskInitPendingForTesting() {
        return mIsSecondaryTaskInitPending;
    }

    @VisibleForTesting
    public void showTabSelectionEditorForTesting(List<Tab> tabs) {
        mStartSurfaceMediator.getSecondaryTasksSurfaceController().showTabSelectionEditor(tabs);
    }

    @VisibleForTesting
    public boolean isMVTilesCleanedUpForTesting() {
        return mTasksSurface.isMVTilesCleanedUp();
    }

    @VisibleForTesting
    public boolean isMVTilesInitializedForTesting() {
        return mTasksSurface.isMVTilesInitialized();
    }

    private void createAndSetStartSurface(boolean excludeMVTiles, boolean excludeQueryTiles) {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);

        int tabSwitcherType =
                mIsStartSurfaceEnabled ? TabSwitcherType.CAROUSEL : TabSwitcherType.GRID;
        if (StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue()) {
            tabSwitcherType = TabSwitcherType.SINGLE;
        }
        mTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(mActivity,
                mScrimCoordinator, mPropertyModel, tabSwitcherType, mParentTabSupplier,
                !excludeMVTiles, !excludeQueryTiles, mWindowAndroid, mActivityLifecycleDispatcher,
                mTabModelSelector, mSnackbarManager, mDynamicResourceLoaderSupplier,
                mTabContentManager, mModalDialogManager, mBrowserControlsManager,
                mTabCreatorManager, mMenuOrKeyboardActionController, mShareDelegateSupplier,
                mMultiWindowModeStateDispatcher, mContainerView);
        mTasksSurface.getView().setId(R.id.primary_tasks_surface_view);
        initializeOffsetChangedListener();
        addHeaderOffsetChangeListener(mOffsetChangedListenerToGenerateScrollEvents);

        mTasksSurfacePropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel,
                new TasksSurfaceViewBinder.ViewHolder(mContainerView, mTasksSurface.getView()),
                TasksSurfaceViewBinder::bind);
    }

    private TabSwitcher.Controller initializeSecondaryTasksSurface() {
        assert mIsStartSurfaceEnabled;
        assert mSecondaryTasksSurface == null;

        PropertyModel propertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
        mStartSurfaceMediator.setSecondaryTasksSurfacePropertyModel(propertyModel);
        mSecondaryTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(
                mActivity, mScrimCoordinator, propertyModel, TabSwitcherType.GRID,
                mParentTabSupplier,
                /* hasMVTiles= */ false, /* hasQueryTiles= */ false, mWindowAndroid,
                mActivityLifecycleDispatcher, mTabModelSelector, mSnackbarManager,
                mDynamicResourceLoaderSupplier, mTabContentManager, mModalDialogManager,
                mBrowserControlsManager, mTabCreatorManager, mMenuOrKeyboardActionController,
                mShareDelegateSupplier, mMultiWindowModeStateDispatcher, mContainerView);
        if (mIsInitializedWithNative) {
            mSecondaryTasksSurface.onFinishNativeInitialization(
                    mActivity, mOmniboxStubSupplier.get());
            mSecondaryTasksSurface.initialize();
        } else {
            mIsSecondaryTaskInitPending = true;
        }

        mSecondaryTasksSurface.getView().setId(R.id.secondary_tasks_surface_view);
        mSecondaryTasksSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new TasksSurfaceViewBinder.ViewHolder(
                                mContainerView, mSecondaryTasksSurface.getView()),
                        SecondaryTasksSurfaceViewBinder::bind);
        if (mOnTabSelectingListener != null) {
            mSecondaryTasksSurface.setOnTabSelectingListener(mOnTabSelectingListener);
            mOnTabSelectingListener = null;
        }
        return mSecondaryTasksSurface.getController();
    }

    // TODO(crbug.com/1047488): This is a temporary solution of the issue crbug.com/1047488, which
    // has not been reproduced locally. The crash is because we can not find ChromeTabbedActivity's
    // ActivityInfo in the ApplicationStatus. However, from the code, ActivityInfo is created in
    // ApplicationStatus during AsyncInitializationActivity.onCreate, which happens before
    // ChromeTabbedActivity.startNativeInitialization where creates the Start surface. So one
    // possible reason is the ChromeTabbedActivity is finishing or destroyed when showing overview.
    private boolean isActivityFinishingOrDestroyed() {
        boolean finishingOrDestroyed =
                mChromeActivityNativeDelegate.isActivityFinishingOrDestroyed()
                || ApplicationStatus.getStateForActivity(mActivity) == ActivityState.DESTROYED;
        // TODO(crbug.com/1047488): Assert false. Do not do that in this CL to keep it small since
        // Start surface is eanbled in the fieldtrial_testing_config.json, which requires update of
        // the other browser tests.
        return finishingOrDestroyed;
    }

    /**
     * Creates a {@link SwipeRefreshLayout} to do a pull-to-refresh.
     */
    private void createSwipeRefreshLayout() {
        assert mSwipeRefreshLayout == null;
        mSwipeRefreshLayout = FeedSwipeRefreshLayout.create(mActivity, R.id.toolbar_container);

        // If FeedSwipeRefreshLayout is not created because the feature is not enabled, don't create
        // another layer.
        if (mSwipeRefreshLayout == null) return;

        // SwipeRefreshLayout can only support one direct child. So we have to create a FrameLayout
        // as a container of possible more than one task views.
        mContainerView.addView(mSwipeRefreshLayout);
        FrameLayout directChildHolder = new FrameLayout(mActivity);
        mSwipeRefreshLayout.addView(directChildHolder);
        mContainerView = directChildHolder;
    }

    private void initializeOffsetChangedListener() {
        int realVerticalMargin = getPixelSize(R.dimen.location_bar_vertical_margin);
        int fakeSearchBoxToRealSearchBoxTop = getPixelSize(R.dimen.control_container_height)
                + getPixelSize(R.dimen.start_surface_fake_search_box_top_margin)
                - realVerticalMargin;

        // The following |fake*| values mean the values of the fake search box; |real*| values
        // mean the values of the real search box.
        int fakeHeight = getPixelSize(R.dimen.ntp_search_box_height);
        int realHeight = getPixelSize(R.dimen.toolbar_height_no_shadow) - realVerticalMargin * 2;
        int fakeAndRealHeightDiff = fakeHeight - realHeight;

        int fakeEndPadding = getPixelSize(R.dimen.search_box_end_padding);
        // realEndPadding is 0;

        // fakeTranslationX is 0;
        int realTranslationX = getPixelSize(R.dimen.location_bar_status_icon_width)
                + getPixelSize(R.dimen.location_bar_icon_end_padding_focused)
                + (getPixelSize(R.dimen.fake_search_box_lateral_padding)
                        - getPixelSize(R.dimen.search_box_start_padding));

        float fakeTextSize = mActivity.getResources().getDimension(
                R.dimen.tasks_surface_location_bar_url_text_size);
        float realTextSize =
                mActivity.getResources().getDimension(R.dimen.location_bar_url_text_size);

        int fakeButtonSize = getPixelSize(R.dimen.tasks_surface_location_bar_url_button_size);
        int realButtonSize = getPixelSize(R.dimen.location_bar_action_icon_width);

        int fakeLensButtonStartMargin =
                getPixelSize(R.dimen.tasks_surface_location_bar_url_button_start_margin);
        // realLensButtonStartMargin is 0;

        mOffsetChangedListenerToGenerateScrollEvents = (appBarLayout, verticalOffset) -> {
            for (ScrollListener scrollListener : mScrollListeners) {
                scrollListener.onHeaderOffsetChanged(verticalOffset);
            }

            // This function should be called together with
            // StartSurfaceToolbarMediator#updateTranslationY, which scroll up the start surface
            // toolbar together with the header.
            int scrolledHeight = -verticalOffset;
            // When the fake search box top is scrolled to the search box top, start to reduce
            // fake search box's height until it's the same as the real search box.
            int reducedHeight = MathUtils.clamp(
                    scrolledHeight - fakeSearchBoxToRealSearchBoxTop, 0, fakeAndRealHeightDiff);
            float expansionFraction = (float) reducedHeight / fakeAndRealHeightDiff;

            mTasksSurface.updateFakeSearchBox(fakeHeight - reducedHeight, reducedHeight,
                    (int) (fakeEndPadding * (1 - expansionFraction)),
                    fakeTextSize + (realTextSize - fakeTextSize) * expansionFraction,
                    SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(false)
                            ? realTranslationX * expansionFraction
                            : 0,
                    (int) (fakeButtonSize + (realButtonSize - fakeButtonSize) * expansionFraction),
                    (int) (fakeLensButtonStartMargin * (1 - expansionFraction)));
        };
    }

    private int getPixelSize(int id) {
        return mActivity.getResources().getDimensionPixelSize(id);
    }

    @VisibleForTesting
    boolean isSecondaryTasksSurfaceEmptyForTesting() {
        return mSecondaryTasksSurface == null;
    }
}
