// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.SearchManager;
import android.app.assist.AssistContent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.SystemClock;
import android.support.annotation.CallSuper;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Pair;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;
import org.chromium.base.annotations.UsedByReflection;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ObservableSupplier;
import org.chromium.base.ObservableSupplierImpl;
import org.chromium.base.StrictModeContext;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabPanel;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManagerHandler;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager.ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.directactions.DirectActionInitializer;
import org.chromium.chrome.browser.dom_distiller.DomDistillerUIUtils;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.firstrun.ForcedSigninProcessor;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.gsa.ContextReporter;
import org.chromium.chrome.browser.gsa.GSAAccountChangeListener;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentFactory;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.media.PictureInPictureController;
import org.chromium.chrome.browser.metrics.ActivityTabStartupMetricsTracker;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.modaldialog.AppModalPresenter;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.nfc.BeamController;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorController;
import org.chromium.chrome.browser.omaha.UpdateInfoBarController;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper.MenuButtonState;
import org.chromium.chrome.browser.omaha.UpdateNotificationController;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareMenuActionHandler;
import org.chromium.chrome.browser.snackbar.BottomContainer;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.touchless.TouchlessUiCoordinator;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.vr.ArDelegate;
import org.chromium.chrome.browser.vr.ArDelegateProvider;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.webapps.AddToHomescreenManager;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.policy.CombinedPolicyProvider;
import org.chromium.policy.CombinedPolicyProvider.PolicyChangeListener;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;
import org.chromium.webapk.lib.client.WebApkNavigationClient;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Consumer;

/**
 * A {@link AsyncInitializationActivity} that builds and manages a {@link CompositorViewHolder}
 * and associated classes.
 * @param <C> - type of associated Dagger component.
 */
public abstract class ChromeActivity<C extends ChromeActivityComponent>
        extends AsyncInitializationActivity
        implements TabCreatorManager, AccessibilityStateChangeListener, PolicyChangeListener,
                   ContextualSearchTabPromotionDelegate, SnackbarManageable, SceneChangeObserver,
                   StatusBarColorController.StatusBarColorProvider, AppMenuDelegate, AppMenuBlocker,
                   MenuOrKeyboardActionController {
    /**
     * No control container to inflate during initialization.
     */
    public static final int NO_CONTROL_CONTAINER = -1;

    /**
     * The different types of activities extending ChromeActivity.
     */
    @IntDef({ActivityType.BASE, ActivityType.TABBED, ActivityType.CUSTOM_TAB, ActivityType.WEBAPP,
            ActivityType.NO_TOUCH, ActivityType.DINO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActivityType {
        int BASE = 0;
        int TABBED = 1;
        int CUSTOM_TAB = 2;
        int WEBAPP = 3;
        int NO_TOUCH = 4;
        int DINO = 5;
    }

    /**
     * No toolbar layout to inflate during initialization.
     */
    static final int NO_TOOLBAR_LAYOUT = -1;

    private static final int RECORD_MULTI_WINDOW_SCREEN_WIDTH_DELAY_MS = 5000;

    /**
     * Timeout in ms for reading PartnerBrowserCustomizations provider.
     */
    private static final int PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS = 10000;

    private C mComponent;

    protected ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabCreatorManager.TabCreator mRegularTabCreator;
    private TabCreatorManager.TabCreator mIncognitoTabCreator;
    private TabContentManager mTabContentManager;
    private UmaSessionStats mUmaSessionStats;
    private ContextReporter mContextReporter;

    private boolean mPartnerBrowserRefreshNeeded;

    protected IntentHandler mIntentHandler;

    /** Set if {@link #postDeferredStartupIfNeeded()} is called before native has loaded. */
    private boolean mDeferredStartupQueued;

    /** Whether or not {@link #postDeferredStartupIfNeeded()} has already successfully run. */
    private boolean mDeferredStartupPosted;

    private boolean mTabModelsInitialized;
    private boolean mNativeInitialized;
    private boolean mRemoveWindowBackgroundDone;
    private boolean mToolbarInitialized;

    // The class cannot implement TouchExplorationStateChangeListener,
    // because it is only available for Build.VERSION_CODES.KITKAT and later.
    // We have to instantiate the TouchExplorationStateChangeListner object in the code.
    @SuppressLint("NewApi")
    private TouchExplorationStateChangeListener mTouchExplorationStateChangeListener;

    // Observes when sync becomes ready to create the mContextReporter.
    private ProfileSyncService.SyncStateChangedListener mSyncStateChangedListener;

    @Nullable
    private ChromeFullscreenManager mFullscreenManager;

    // The PictureInPictureController is initialized lazily https://crbug.com/729738.
    private PictureInPictureController mPictureInPictureController;

    private CompositorViewHolder mCompositorViewHolder;
    private ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier =
            new ObservableSupplierImpl<>();
    private InsetObserverView mInsetObserverView;
    private ContextualSearchManager mContextualSearchManager;
    protected ReaderModeManager mReaderModeManager;
    private SnackbarManager mSnackbarManager;
    @Nullable
    private ToolbarManager mToolbarManager;
    private BottomSheetController mBottomSheetController;
    private UpdateNotificationController mUpdateNotificationController;
    private BottomSheet mBottomSheet;
    private ScrimView mScrimView;
    private StatusBarColorController mStatusBarColorController;

    // Timestamp in ms when initial layout inflation begins
    private long mInflateInitialLayoutBeginMs;
    // Timestamp in ms when initial layout inflation ends
    private long mInflateInitialLayoutEndMs;

    private int mUiMode;
    private int mDensityDpi;

    private final DiscardableReferencePool mReferencePool = new DiscardableReferencePool();
    private final ManualFillingComponent mManualFillingComponent =
            ManualFillingComponentFactory.createComponent();

    private AssistStatusHandler mAssistStatusHandler;

    @Nullable
    protected DirectActionInitializer mDirectActionInitializer;

    // A set of views obscuring all tabs. When this set is nonempty,
    // all tab content will be hidden from the accessibility tree.
    private Set<View> mViewsObscuringAllTabs = new HashSet<>();

    // See enableHardwareAcceleration()
    private boolean mSetWindowHWA;

    /** Whether or not a PolicyChangeListener was added. */
    private boolean mDidAddPolicyChangeListener;

    private ActivityTabStartupMetricsTracker mActivityTabStartupMetricsTracker;

    /** A means of providing the foreground tab of the activity to different features. */
    private ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();

    /** A means of providing the theme color to different features. */
    private TabThemeColorProvider mTabThemeColorProvider;

    /** Whether or not the activity is in started state. */
    private boolean mStarted;

    private final Runnable mUpdateStateChangedListener = this::onUpdateStateChanged;

    /**
     * The RootUiCoordinator associated with the activity. This variable is held to facilitate
     * testing.
     */
    private RootUiCoordinator mRootUiCoordinator;

    /**
     * Coordinates Touchless UI across ChromeActivity-derived classes.
     */
    private TouchlessUiCoordinator mTouchlessUiCoordinator;

    // TODO(972867): Pull MenuOrKeyboardActionController out of ChromeActivity.
    private List<MenuOrKeyboardActionController.MenuOrKeyboardActionHandler> mMenuActionHandlers =
            new ArrayList<>();

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ChromeWindow(this);
    }

    @Override
    public void performPreInflationStartup() {
        // Create component before calling super to give its members a chance to catch
        // onPreInflationStartup event.
        mComponent = createComponent();

        super.performPreInflationStartup();

        mRootUiCoordinator = createRootUiCoordinator();

        // See comments on #getTouchlessUiCoordinator for why we're doing this here.
        getTouchlessUiCoordinator();

        VrModuleProvider.getDelegate().doPreInflationStartup(this, getSavedInstanceState());

        // Force a partner customizations refresh if it has yet to be initialized.  This can happen
        // if Chrome is killed and you refocus a previous activity from Android recents, which does
        // not go through ChromeLauncherActivity that would have normally triggered this.
        mPartnerBrowserRefreshNeeded = !PartnerBrowserCustomizations.isInitialized();

        CommandLine commandLine = CommandLine.getInstance();
        if (!commandLine.hasSwitch(ChromeSwitches.DISABLE_FULLSCREEN)) {
            TypedValue threshold = new TypedValue();
            getResources().getValue(R.dimen.top_controls_show_threshold, threshold, true);
            commandLine.appendSwitchWithValue(ContentSwitches.TOP_CONTROLS_SHOW_THRESHOLD,
                    threshold.coerceToString().toString());
            getResources().getValue(R.dimen.top_controls_hide_threshold, threshold, true);
            commandLine.appendSwitchWithValue(ContentSwitches.TOP_CONTROLS_HIDE_THRESHOLD,
                    threshold.coerceToString().toString());
        }

        getWindow().setBackgroundDrawable(getBackgroundDrawable());
    }

    protected RootUiCoordinator createRootUiCoordinator() {
        // TODO(https://crbug.com/931496): Remove dependency on ChromeActivity in favor of passing
        // in direct dependencies on needed classes. While migrating code from Chrome*Activity
        // to the RootUiCoordinator, passing the activity is an easy way to get access to a
        // number of objects that will ultimately be owned by the RootUiCoordinator. This is not
        // a recommended pattern.
        return new RootUiCoordinator(this);
    }

    private C createComponent() {
        ChromeActivityCommonsModule.Factory overridenCommonsFactory =
                ModuleFactoryOverrides.getOverrideFor(ChromeActivityCommonsModule.Factory.class);

        ChromeActivityCommonsModule commonsModule = overridenCommonsFactory == null
                ? new ChromeActivityCommonsModule(this, getLifecycleDispatcher())
                : overridenCommonsFactory.create(this);

        return createComponent(commonsModule);
    }

    /**
     * Override this to create a component that represents a richer dependency graph for a
     * particular subclass of ChromeActivity. The specialized component should be activity-scoped
     * and include all modules for ChromeActivityComponent, such as
     * {@link ChromeActivityCommonsModule}, along with any additional modules.
     *
     * You may immediately resolve some of the classes belonging to the component in this method.
     */
    @SuppressWarnings("unchecked")
    protected C createComponent(ChromeActivityCommonsModule commonsModule) {
        return (C) ChromeApplication.getComponent().createChromeActivityComponent(commonsModule);
    }

    /**
     * @return the activity-scoped component associated with this instance of activity.
     */
    public final C getComponent() {
        return mComponent;
    }

    @SuppressLint("NewApi")
    @Override
    public void performPostInflationStartup() {
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.performPostInflationStartup")) {
            super.performPostInflationStartup();

            ViewGroup coordinator = findViewById(R.id.coordinator);
            mScrimView = new ScrimView(
                    this, getStatusBarColorController().getStatusBarScrimDelegate(), coordinator);

            Intent intent = getIntent();
            if (intent != null && getSavedInstanceState() == null) {
                VrModuleProvider.getDelegate().maybeHandleVrIntentPreNative(this, intent);
            }

            mSnackbarManager = new SnackbarManager(this, null);

            mAssistStatusHandler = createAssistStatusHandler();
            if (mAssistStatusHandler != null) {
                if (mTabModelSelector != null) {
                    mAssistStatusHandler.setTabModelSelector(mTabModelSelector);
                }
                mAssistStatusHandler.updateAssistState();
            }

            // This check is only applicable for JB since in KK svelte was supported from the start.
            // See https://crbug.com/826460 for context.
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                // If a user had ALLOW_LOW_END_DEVICE_UI explicitly set to false then we manually
                // override SysUtils.isLowEndDevice() with a switch so that they continue to see the
                // normal UI. This is only the case for grandfathered-in svelte users. We no longer
                // do

                // so for newer users.
                if (!ChromePreferenceManager.getInstance().readBoolean(
                            ChromePreferenceManager.ALLOW_LOW_END_DEVICE_UI, true)) {
                    CommandLine.getInstance().appendSwitch(
                            BaseSwitches.DISABLE_LOW_END_DEVICE_MODE);
                }
            }

            AccessibilityManager manager = (AccessibilityManager) getBaseContext().getSystemService(
                    Context.ACCESSIBILITY_SERVICE);
            manager.addAccessibilityStateChangeListener(this);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                mTouchExplorationStateChangeListener = enabled -> {
                    AccessibilityUtil.resetAccessibilityEnabled();
                    checkAccessibility();
                };
                manager.addTouchExplorationStateChangeListener(
                        mTouchExplorationStateChangeListener);
            }

            // Make the activity listen to policy change events
            CombinedPolicyProvider.get().addPolicyChangeListener(this);
            mDidAddPolicyChangeListener = true;

            // Set up the animation placeholder to be the SurfaceView. This disables the
            // SurfaceView's 'hole' clipping during animations that are notified to the window.
            getWindowAndroid().setAnimationPlaceholderView(
                    mCompositorViewHolder.getCompositorView());

            initializeTabModels();
            initializeToolbarIfNecessary();
            if (!isFinishing() && getFullscreenManager() != null) {
                getFullscreenManager().initialize(
                        (ControlContainer) findViewById(R.id.control_container),
                        getTabModelSelector(), getControlContainerHeightResource());
            }

            ((BottomContainer) findViewById(R.id.bottom_container))
                    .initialize(mFullscreenManager,
                            mManualFillingComponent.getKeyboardExtensionViewResizer());

            // If onStart was called before postLayoutInflation (because inflation was done in a
            // background thread) then make sure to call the relevant methods belatedly.
            if (mStarted) {
                mCompositorViewHolder.onStart();
            }
        }
    }

    @Override
    protected void initializeStartupMetrics() {
        mActivityTabStartupMetricsTracker = new ActivityTabStartupMetricsTracker(this);
    }

    protected ActivityTabStartupMetricsTracker getActivityTabStartupMetricsTracker() {
        return mActivityTabStartupMetricsTracker;
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        View controlContainer = findViewById(R.id.control_container);
        return controlContainer != null ? controlContainer
                : super.getViewToBeDrawnBeforeInitializingNative();
    }

    /**
     * This function triggers the layout inflation. If subclasses override {@link
     * #doLayoutInflation}, no calls to {@link #getCompositorViewHolder()} can be done until
     * inflation is complete and {@link #onInitialLayoutInflationComplete()} is called. If the
     * subclass does not override {@link #doLayoutInflation}, then {@link
     * #getCompositorViewHolder()} is safe to be called after calling super.
     */
    @Override
    protected final void triggerLayoutInflation() {
        mInflateInitialLayoutBeginMs = SystemClock.elapsedRealtime();
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.triggerLayoutInflation")) {
            SelectionPopupController.setShouldGetReadbackViewFromWindowAndroid();

            enableHardwareAcceleration();
            setLowEndTheme();

            WarmupManager warmupManager = WarmupManager.getInstance();
            if (warmupManager.hasViewHierarchyWithToolbar(getControlContainerLayoutId())) {
                View placeHolderView = new View(this);
                setContentView(placeHolderView);
                ViewGroup contentParent = (ViewGroup) placeHolderView.getParent();
                warmupManager.transferViewHierarchyTo(contentParent);
                contentParent.removeView(placeHolderView);
                onInitialLayoutInflationComplete();
            } else {
                warmupManager.clearViewHierarchy();
                doLayoutInflation();
            }
        }
    }

    /**
     * This function implements the actual layout inflation, Subclassing Activities that override
     * this method without calling super need to call {@link #onInitialLayoutInflationComplete()}.
     */
    protected void doLayoutInflation() {
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.doLayoutInflation")) {
            // Allow disk access for the content view and toolbar container setup.
            // On certain android devices this setup sequence results in disk writes outside
            // of our control, so we have to disable StrictMode to work. See
            // https://crbug.com/639352.
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                TraceEvent.begin("setContentView(R.layout.main)");
                setContentView(R.layout.main);
                TraceEvent.end("setContentView(R.layout.main)");
                if (getControlContainerLayoutId() != NO_CONTROL_CONTAINER) {
                    ViewStub toolbarContainerStub =
                            ((ViewStub) findViewById(R.id.control_container_stub));

                    toolbarContainerStub.setLayoutResource(getControlContainerLayoutId());
                    TraceEvent.begin("toolbarContainerStub.inflate");
                    toolbarContainerStub.inflate();
                    TraceEvent.end("toolbarContainerStub.inflate");
                }

                // It cannot be assumed that the result of toolbarContainerStub.inflate() will
                // be the control container since it may be wrapped in another view.
                ControlContainer controlContainer =
                        (ControlContainer) findViewById(R.id.control_container);

                if (controlContainer == null) {
                    // omnibox_results_container_stub anchors off of control_container, and will
                    // crash during layout if control_container doesn't exist.
                    UiUtils.removeViewFromParent(findViewById(R.id.omnibox_results_container_stub));
                }

                // Inflate the correct toolbar layout for the device.
                int toolbarLayoutId = getToolbarLayoutId();
                if (toolbarLayoutId != NO_TOOLBAR_LAYOUT && controlContainer != null) {
                    controlContainer.initWithToolbar(toolbarLayoutId);
                }
            }
            onInitialLayoutInflationComplete();
        }
    }

    @Override
    protected void onInitialLayoutInflationComplete() {
        mInflateInitialLayoutEndMs = SystemClock.elapsedRealtime();

        getStatusBarColorController().updateStatusBarColor(true);

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
        mCompositorViewHolder = (CompositorViewHolder) findViewById(R.id.compositor_view_holder);
        // If the UI was inflated on a background thread, then the CompositorView may not have been
        // fully initialized yet as that may require the creation of a handler which is not allowed
        // outside the UI thread. This call should fully initialize the CompositorView if it hasn't
        // been yet.
        mCompositorViewHolder.setRootView(rootView);

        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);

        // Add a custom view right after the root view that stores the insets to access later.
        // WebContents needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        mInsetObserverView = InsetObserverView.create(this);
        rootView.addView(mInsetObserverView, 0);

        super.onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    private void initializeToolbarIfNecessary() {
        // TODO(https://crbug.com/931496): Move toolbar ownership to RootUiCoordinator.
        // TODO(pshmakov): make ToolbarManager lazy, don't create them unless getToolbarManager()
        // is called.
        if (!mToolbarInitialized) {
            mToolbarInitialized = true;
            initializeToolbar();
        }
    }

    /**
     * Constructs {@link ToolbarManager} and the handler necessary for controlling the menu on the
     * {@link Toolbar}. Extending classes can override this call to avoid creating the toolbar.
     */
    protected void initializeToolbar() {
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.initializeToolbar")) {
            final View controlContainer = findViewById(R.id.control_container);
            assert controlContainer != null;
            ToolbarControlContainer toolbarContainer = (ToolbarControlContainer) controlContainer;
            Callback<Boolean> urlFocusChangedCallback = hasFocus -> onOmniboxFocusChanged(hasFocus);
            mToolbarManager = new ToolbarManager(this, toolbarContainer,
                    getCompositorViewHolder().getInvalidator(), urlFocusChangedCallback,
                    mTabThemeColorProvider);
        }
    }

    /**
     * Initialize the {@link TabModelSelector}, {@link TabModel}s, and
     * {@link org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator} needed by
     * this activity.
     */
    public final void initializeTabModels() {
        if (mTabModelsInitialized) return;

        mTabModelSelector = createTabModelSelector();
        mTabModelSelectorSupplier.set(mTabModelSelector);
        mActivityTabProvider.setTabModelSelector(mTabModelSelector);
        mTabThemeColorProvider = new TabThemeColorProvider(this);
        mTabThemeColorProvider.setActivityTabProvider(mActivityTabProvider);
        getStatusBarColorController().setTabModelSelector(mTabModelSelector);

        if (mTabModelSelector == null) {
            assert isFinishing();
            mTabModelsInitialized = true;
            return;
        }

        Pair<? extends TabCreator, ? extends TabCreator> tabCreators = createTabCreators();
        mRegularTabCreator = tabCreators.first;
        mIncognitoTabCreator = tabCreators.second;

        OfflinePageUtils.observeTabModelSelector(this, mTabModelSelector);
        NewTabPageUma.monitorNTPCreation(mTabModelSelector);

        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                postDeferredStartupIfNeeded();
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                postDeferredStartupIfNeeded();
                OfflinePageUtils.showOfflineSnackbarIfNecessary(tab);
            }

            @Override
            public void onCrash(Tab tab) {
                postDeferredStartupIfNeeded();
            }
        };

        if (mAssistStatusHandler != null) {
            mAssistStatusHandler.setTabModelSelector(mTabModelSelector);
        }

        mTabModelsInitialized = true;
    }

    /**
     * @return The {@link TabModelSelector} owned by this {@link ChromeActivity}.
     */
    protected abstract TabModelSelector createTabModelSelector();

    /**
     * @return The {@link org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator}s owned
     *         by this {@link ChromeActivity}.  The first item in the Pair is the normal model tab
     *         creator, and the second is the tab creator for incognito tabs.
     */
    protected abstract Pair<? extends TabCreator, ? extends TabCreator> createTabCreators();

    /**
     * @return {@link ToolbarManager} that belongs to this activity or null if the current activity
     *         does not support a toolbar.
     */
    @Nullable
    public ToolbarManager getToolbarManager() {
        if (isInitialLayoutInflationComplete()) {
            initializeToolbarIfNecessary();
        }
        return mToolbarManager;
    }

    /**
     * @return The {@link ManualFillingComponent} that belongs to this activity.
     */
    public ManualFillingComponent getManualFillingComponent() {
        return mManualFillingComponent;
    }

    /**
     * Get the Chrome Home bottom sheet if it exists.
     * @return The bottom sheet or null.
     */
    @Nullable
    public BottomSheet getBottomSheet() {
        return mBottomSheet;
    }

    /**
     * @return The View used to obscure content and bring focus to a foreground view.
     */
    public ScrimView getScrim() {
        return mScrimView;
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new AppMenuPropertiesDelegateImpl(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), null);
    }

    /**
     * @return The assist handler for this activity.
     */
    protected AssistStatusHandler getAssistStatusHandler() {
        return mAssistStatusHandler;
    }

    /**
     * @return A newly constructed assist handler for this given activity type.
     */
    protected AssistStatusHandler createAssistStatusHandler() {
        return new AssistStatusHandler(this);
    }

    /**
     * @return The resource id for the layout to use for {@link ControlContainer}. 0 by default.
     */
    protected int getControlContainerLayoutId() {
        return NO_CONTROL_CONTAINER;
    }

    /**
     * @return The resource id that contains how large the browser controls are.
     */
    public int getControlContainerHeightResource() {
        return NO_CONTROL_CONTAINER;
    }

    /**
     * @return The layout ID for the toolbar to use.
     */
    protected int getToolbarLayoutId() {
        return NO_TOOLBAR_LAYOUT;
    }

    /**
     * @return Whether contextual search is allowed for this activity or not.
     */
    protected boolean isContextualSearchAllowed() {
        return true;
    }

    @Override
    public void initializeState() {
        super.initializeState();

        IntentHandler.setTestIntentsEnabled(
                CommandLine.getInstance().hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS));
        mIntentHandler = new IntentHandler(createIntentHandlerDelegate(), getPackageName());
    }

    @Override
    public void initializeCompositor() {
        TraceEvent.begin("ChromeActivity:CompositorInitialization");
        super.initializeCompositor();

        setTabContentManager(new TabContentManager(
                this, getContentOffsetProvider(), DeviceClassManager.enableSnapshots()));
        mCompositorViewHolder.onNativeLibraryReady(getWindowAndroid(), getTabContentManager());

        if (isContextualSearchAllowed() && ContextualSearchFieldTrial.isEnabled()) {
            mContextualSearchManager = new ContextualSearchManager(this, this);
        }

        if (ReaderModeManager.isEnabled(this)) {
            mReaderModeManager = new ReaderModeManager(getTabModelSelector(), this);
        }

        TraceEvent.end("ChromeActivity:CompositorInitialization");
    }

    @Override
    public void onStartWithNative() {
        assert mNativeInitialized : "onStartWithNative was called before native was initialized.";

        super.onStartWithNative();
        UpdateMenuItemHelper.getInstance().onStart();
        ChromeActivitySessionTracker.getInstance().onStartWithNative();
        OfflineIndicatorController.initialize();

        // postDeferredStartupIfNeeded() is called in TabModelSelectorTabObsever#onLoadStopped(),
        // #onPageLoadFinished() and #onCrash(). If we are not actively loading a tab (e.g.
        // in Android N multi-instance, which is created by re-parenting an existing tab),
        // ensure onDeferredStartup() gets called by calling postDeferredStartupIfNeeded() here.
        if (mDeferredStartupQueued || getActivityTab() == null || !getActivityTab().isLoading()) {
            postDeferredStartupIfNeeded();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        maybeRemoveWindowBackground();

        Tab tab = getActivityTab();
        if (hasFocus) {
            if (tab != null) {
                tab.onActivityShown();
                // When resuming the activity, force an update to the fullscreen state to ensure a
                // subactivity did not change the fullscreen configuration of this ChromeTab's
                // renderer in the case where it was shared.
                TabBrowserControlsState.updateEnabledState(tab);
            }
            VrModuleProvider.getDelegate().onActivityShown(this);
        } else {
            boolean stopped = ApplicationStatus.getStateForActivity(this) == ActivityState.STOPPED;
            if (stopped) {
                VrModuleProvider.getDelegate().onActivityHidden(this);
                if (tab != null) tab.onActivityHidden();
            }
        }

        Clipboard.getInstance().onWindowFocusChanged(hasFocus);
    }

    /**
     * @return The {@link StatusBarColorController} that adjusts the status bar color.
     */
    public final StatusBarColorController getStatusBarColorController() {
        // TODO(https://crbug.com/943371): Initialize in SystemUiCoordinator. This requires
        // SystemUiCoordinator to be created before WebappActivty#onResume().
        if (mStatusBarColorController == null) {
            mStatusBarColorController = new StatusBarColorController(this);
        }

        return mStatusBarColorController;
    }

    @Override
    public int getBaseStatusBarColor() {
        return StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;
    }

    @Override
    public boolean isStatusBarDefaultThemeColor() {
        return false;
    }

    private void createContextReporterIfNeeded() {
        if (!mStarted) return; // Sync state reporting should work only in started state.
        if (mContextReporter != null || getActivityTab() == null) return;

        final SyncController syncController = SyncController.get(this);
        final ProfileSyncService syncService = ProfileSyncService.get();

        if (syncController != null && syncController.isSyncingUrlsWithKeystorePassphrase()) {
            assert syncService != null;
            mContextReporter = AppHooks.get().createGsaHelper().getContextReporter(this);

            if (mSyncStateChangedListener != null) {
                syncService.removeSyncStateChangedListener(mSyncStateChangedListener);
                mSyncStateChangedListener = null;
            }

            return;
        } else {
            ContextReporter.reportSyncStatus(syncService);
        }

        if (mSyncStateChangedListener == null && syncService != null) {
            mSyncStateChangedListener = () -> createContextReporterIfNeeded();
            syncService.addSyncStateChangedListener(mSyncStateChangedListener);
        }
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        markSessionResume();
        RecordUserAction.record("MobileComeToForeground");

        Tab tab = getActivityTab();
        if (tab != null) {
            WebContents webContents = tab.getWebContents();
            LaunchMetrics.commitLaunchMetrics(webContents);

            // For picture-in-picture mode
            if (webContents != null) webContents.notifyRendererPreferenceUpdate();
        }

        FeatureUtilities.setCustomTabVisible(isCustomTab());
        FeatureUtilities.setIsInMultiWindowMode(
                MultiWindowUtils.getInstance().isInMultiWindowMode(this));

        if (mPictureInPictureController != null) {
            mPictureInPictureController.cleanup(this);
        }
        VrModuleProvider.getDelegate().maybeRegisterVrEntryHook(this);
        ArDelegate arDelegate = ArDelegateProvider.getDelegate();
        if (arDelegate != null) {
            arDelegate.registerOnResumeActivity(this);
        }

        getManualFillingComponent().onResume();
    }

    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();

        if (mPictureInPictureController == null) {
            mPictureInPictureController = new PictureInPictureController();
        }
        mPictureInPictureController.attemptPictureInPicture(this);
    }

    @Override
    public void onPauseWithNative() {
        RecordUserAction.record("MobileGoToBackground");
        Tab tab = getActivityTab();
        if (tab != null) getTabContentManager().cacheTabThumbnail(tab);
        getManualFillingComponent().onPause();

        VrModuleProvider.getDelegate().maybeUnregisterVrEntryHook();
        markSessionEnd();

        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        Tab tab = getActivityTab();
        if (!hasWindowFocus()) {
            VrModuleProvider.getDelegate().onActivityHidden(this);
            if (tab != null) tab.onActivityHidden();
        }

        if (GSAState.getInstance(this).isGsaAvailable() && !SysUtils.isLowEndDevice()) {
            GSAAccountChangeListener.getInstance().disconnect();
        }
        if (mSyncStateChangedListener != null) {
            ProfileSyncService syncService = ProfileSyncService.get();
            if (syncService != null) {
                syncService.removeSyncStateChangedListener(mSyncStateChangedListener);
            }
            mSyncStateChangedListener = null;
        }
        if (mContextReporter != null) mContextReporter.disable();

        super.onStopWithNative();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        // This should be called before the call to super so that the needed VR flags are set as
        // soon as the VR intent is received.
        VrModuleProvider.getDelegate().maybeHandleVrIntentPreNative(this, intent);
        super.onNewIntent(intent);
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        if (mPictureInPictureController != null) {
            mPictureInPictureController.cleanup(this);
        }

        super.onNewIntentWithNative(intent);
        if (mIntentHandler.shouldIgnoreIntent(intent)) return;

        // We send this intent so that we can enter WebVr presentation mode if needed. This
        // call doesn't consume the intent because it also has the url that we need to load.
        VrModuleProvider.getDelegate().onNewIntentWithNative(this, intent);
        mIntentHandler.onNewIntent(intent);
        if (mUpdateNotificationController == null) {
            mUpdateNotificationController = new UpdateNotificationController(this);
        }
        mUpdateNotificationController.onNewIntent(intent);
    }

    /**
     * @return The type for this activity.
     */
    public @ActivityType int getActivityType() {
        return ActivityType.BASE;
    }

    /**
     * @return Whether the given activity contains a CustomTab.
     */
    public boolean isCustomTab() {
        return getActivityType() == ActivityType.CUSTOM_TAB;
    }

    /**
     * @return Whether the given activity can show the publisher URL from a trusted CDN.
     */
    public boolean canShowTrustedCdnPublisherUrl() {
        return false;
    }

    /**
     * Actions that may be run at some point after startup. Place tasks that are not critical to the
     * startup path here.  This method will be called automatically.
     */
    private void onDeferredStartup() {
        initDeferredStartupForActivity();
        ProcessInitializationHandler.getInstance().initializeDeferredStartupTasks();
        DeferredStartupHandler.getInstance().queueDeferredTasksOnIdleHandler();
    }

    /**
     * All deferred startup tasks that require the activity rather than the app should go here.
     *
     * Overriding methods should queue tasks on the DeferredStartupHandler before or after calling
     * super depending on whether the tasks should run before or after these ones.
     */
    @CallSuper
    protected void initDeferredStartupForActivity() {
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityFinishingOrDestroyed()) return;
            UpdateInfoBarController.createInstance(ChromeActivity.this);
            if (mUpdateNotificationController == null) {
                mUpdateNotificationController =
                        new UpdateNotificationController(ChromeActivity.this);
            }
            mUpdateNotificationController.onNewIntent(getIntent());
            UpdateMenuItemHelper.getInstance().registerObserver(mUpdateStateChangedListener);
        });

        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityFinishingOrDestroyed()) return;
            BeamController.registerForBeam(ChromeActivity.this, () -> {
                Tab currentTab = getActivityTab();
                if (currentTab == null) return null;
                if (!currentTab.isUserInteractable()) return null;
                return currentTab.getUrl();
            });
        });

        final String simpleName = getClass().getSimpleName();
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityFinishingOrDestroyed()) return;
            if (mToolbarManager != null) {
                RecordHistogram.recordTimesHistogram(
                        "MobileStartup.ToolbarInflationTime." + simpleName,
                        mInflateInitialLayoutEndMs - mInflateInitialLayoutBeginMs);
                mToolbarManager.onDeferredStartup(getOnCreateTimestampMs(), simpleName);
            }

            if (MultiWindowUtils.getInstance().isInMultiWindowMode(ChromeActivity.this)) {
                onDeferredStartupForMultiWindowMode();
            }

            long intentTimestamp = IntentHandler.getTimestampFromIntent(getIntent());
            if (intentTimestamp != -1) {
                recordIntentToCreationTime(getOnCreateTimestampMs() - intentTimestamp);
            }

            recordDisplayDimensions();
        });

        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityFinishingOrDestroyed()) return;
            ForcedSigninProcessor.checkCanSignIn(ChromeActivity.this);
        });

        // GSA connection is not needed on low-end devices because Icing is disabled.
        if (!SysUtils.isLowEndDevice()) {
            if (isActivityFinishingOrDestroyed()) return;
            DeferredStartupHandler.getInstance().addDeferredTask(() -> {
                if (!GSAState.getInstance(this).isGsaAvailable()) {
                    ContextReporter.reportStatus(ContextReporter.STATUS_GSA_NOT_AVAILABLE);
                    return;
                }

                GSAAccountChangeListener.getInstance().connect();
                createContextReporterIfNeeded();
            });
        }
    }

    /**
     * Actions that may be run at some point after startup for Android N multi-window mode. Should
     * be called from #onDeferredStartup() if the activity is in multi-window mode.
     */
    protected void onDeferredStartupForMultiWindowMode() {
        // If the Activity was launched in multi-window mode, record a user action.
        recordMultiWindowModeChangedUserAction(true);
    }

    /**
     * Records the time it takes from creating an intent for {@link ChromeActivity} to activity
     * creation, including time spent in the framework.
     * @param timeMs The time from creating an intent to activity creation.
     */
    @CallSuper
    protected void recordIntentToCreationTime(long timeMs) {
        RecordHistogram.recordTimesHistogram("MobileStartup.IntentToCreationTime", timeMs);
    }

    @Override
    public void onStart() {
        if (AsyncTabParamsManager.hasParamsWithTabToReparent()) {
            mCompositorViewHolder.prepareForTabReparenting();
        }
        super.onStart();

        if (mPartnerBrowserRefreshNeeded) {
            mPartnerBrowserRefreshNeeded = false;
            PartnerBrowserCustomizations.initializeAsync(getApplicationContext(),
                    PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS);
            PartnerBrowserCustomizations.setOnInitializeAsyncFinished(() -> {
                if (PartnerBrowserCustomizations.isIncognitoDisabled()) {
                    terminateIncognitoSession();
                }
            });
        }
        if (mCompositorViewHolder != null) mCompositorViewHolder.onStart();

        // Explicitly call checkAccessibility() so things are initialized correctly when Chrome has
        // been re-started after closing due to the last tab being closed when homepage is enabled.
        // See crbug.com/541546.
        checkAccessibility();

        Configuration config = getResources().getConfiguration();
        mUiMode = config.uiMode;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            mDensityDpi = config.densityDpi;
        } else {
            mDensityDpi = getResources().getDisplayMetrics().densityDpi;
        }
        mStarted = true;
    }

    @Override
    public void onStop() {
        super.onStop();

        // We want to refresh partner browser provider every onStart().
        mPartnerBrowserRefreshNeeded = true;
        if (mCompositorViewHolder != null) mCompositorViewHolder.onStop();

        // If postInflationStartup hasn't been called yet (because inflation was done asynchronously
        // and has not yet completed), it no longer needs to do the belated onStart code since we
        // were stopped in the mean time.
        mStarted = false;
    }

    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public void onProvideAssistContent(AssistContent outContent) {
        if (getAssistStatusHandler() == null || !getAssistStatusHandler().isAssistSupported()) {
            // No information is provided in incognito mode.
            return;
        }
        Tab tab = getActivityTab();
        if (tab != null && !isInOverviewMode()) {
            outContent.setWebUri(Uri.parse(tab.getUrl()));
        }
    }

    // TODO(crbug.com/973781): Once Chromium is built against Android Q SDK, replace
    // @SuppressWarnings with @Override
    @SuppressWarnings("MissingOverride")
    @TargetApi(29)
    @UsedByReflection("Called from Android Q")
    public void onPerformDirectAction(String actionId, Bundle arguments,
            CancellationSignal cancellationSignal, Consumer<Bundle> callback) {
        if (mDirectActionInitializer == null) return;

        mDirectActionInitializer.getCoordinator().onPerformDirectAction(
                actionId, arguments, callback);
    }

    // TODO(crbug.com/973781): Once Chromium is built against Android Q SDK:
    //  - replace @SuppressWarnings with @Override
    //  - replace Consumer with Consumer<List<DirectAction>>
    @SuppressWarnings("MissingOverride")
    @TargetApi(29)
    @UsedByReflection("Called from Android Q")
    public void onGetDirectActions(CancellationSignal cancellationSignal, Consumer callback) {
        if (mDirectActionInitializer == null) return;

        mDirectActionInitializer.getCoordinator().onGetDirectActions(callback);
    }

    @Override
    public long getOnCreateTimestampMs() {
        return super.getOnCreateTimestampMs();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        VrModuleProvider.getDelegate().onSaveInstanceState(outState);
    }

    /**
     * This cannot be overridden in order to preserve destruction order.  Override
     * {@link #onDestroyInternal()} instead to perform clean up tasks.
     */
    @SuppressLint("NewApi")
    @Override
    protected final void onDestroy() {
        if (mReaderModeManager != null) {
            mReaderModeManager.destroy();
            mReaderModeManager = null;
        }

        if (mContextualSearchManager != null) {
            mContextualSearchManager.destroy();
            mContextualSearchManager = null;
        }

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mCompositorViewHolder != null) {
            if (mCompositorViewHolder.getLayoutManager() != null) {
                mCompositorViewHolder.getLayoutManager().removeSceneChangeObserver(this);
            }
            mCompositorViewHolder.shutDown();
            mCompositorViewHolder = null;
        }

        onDestroyInternal();

        if (mToolbarManager != null) {
            mToolbarManager.destroy();
            mToolbarManager = null;
        }

        if (mBottomSheet != null) {
            mBottomSheet.destroy();
            mBottomSheet = null;
        }

        if (mDidAddPolicyChangeListener) {
            CombinedPolicyProvider.get().removePolicyChangeListener(this);
            mDidAddPolicyChangeListener = false;
        }

        if (mTabContentManager != null) {
            mTabContentManager.destroy();
            mTabContentManager = null;
        }

        mManualFillingComponent.destroy();

        if (mActivityTabStartupMetricsTracker != null) {
            mActivityTabStartupMetricsTracker.destroy();
            mActivityTabStartupMetricsTracker = null;
        }

        if (mFullscreenManager != null) {
            mFullscreenManager.destroy();
            mFullscreenManager = null;
        }

        if (mTabModelsInitialized) {
            TabModelSelector selector = getTabModelSelector();
            if (selector != null) selector.destroy();
        }

        UpdateMenuItemHelper.getInstance().unregisterObserver(mUpdateStateChangedListener);

        AccessibilityManager manager = (AccessibilityManager)
                getBaseContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        manager.removeAccessibilityStateChangeListener(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            manager.removeTouchExplorationStateChangeListener(mTouchExplorationStateChangeListener);
        }

        if (mTabThemeColorProvider != null) {
            mTabThemeColorProvider.destroy();
            mTabThemeColorProvider = null;
        }

        mActivityTabProvider.destroy();

        mComponent = null;

        super.onDestroy();
    }

    /**
     * Override this to perform destruction tasks.  Note that by the time this is called, the
     * {@link CompositorViewHolder} will be destroyed, but the {@link WindowAndroid} and
     * {@link TabModelSelector} will not.
     * <p>
     * After returning from this, the {@link TabModelSelector} will be destroyed followed
     * by the {@link WindowAndroid}.
     */
    protected void onDestroyInternal() {
    }

    /**
     * @return The unified manager for all snackbar related operations.
     */
    @Override
    public SnackbarManager getSnackbarManager() {
        if (getTouchlessUiCoordinator() != null) {
            return getTouchlessUiCoordinator().getSnackbarManager();
        }
        boolean useBottomSheetContainer = mBottomSheetController != null
                && mBottomSheetController.getBottomSheet().isSheetOpen()
                && !mBottomSheetController.getBottomSheet().isClosing();
        return useBottomSheetContainer ? mBottomSheetController.getSnackbarManager()
                                       : mSnackbarManager;
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        if (getTouchlessUiCoordinator() != null) {
            return getTouchlessUiCoordinator().createModalDialogManager();
        }
        return new ModalDialogManager(
                new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);
    }

    protected Drawable getBackgroundDrawable() {
        return new ColorDrawable(
                ApiCompatibilityUtils.getColor(getResources(), R.color.light_background_color));
    }

    private void maybeRemoveWindowBackground() {
        // Only need to do this logic once.
        if (mRemoveWindowBackgroundDone) return;

        // Remove the window background only after native init and window getting focus. It's done
        // after native init because before native init, a fake background gets shown. The window
        // focus dependency is because doing it earlier can cause drawing bugs, e.g. crbug/673831.
        if (!mNativeInitialized || !hasWindowFocus()) return;

        // The window background color is used as the resizing background color in Android N+
        // multi-window mode. See crbug.com/602366.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            getWindow().setBackgroundDrawable(new ColorDrawable(
                    ApiCompatibilityUtils.getColor(getResources(),
                            R.color.resizing_background_color)));
        } else {
            // Post the removeWindowBackground() call as a separate task, as doing it synchronously
            // here can cause redrawing glitches. See crbug.com/686662 for an example problem.
            Handler handler = new Handler();
            handler.post(() -> removeWindowBackground());
        }

        mRemoveWindowBackgroundDone = true;
    }

    @Override
    public void finishNativeInitialization() {
        mNativeInitialized = true;
        OfflineContentAggregatorNotificationBridgeUiFactory.instance();
        maybeRemoveWindowBackground();
        DownloadManagerService.getDownloadManagerService().onActivityLaunched();

        VrModuleProvider.maybeInit();
        VrModuleProvider.getDelegate().onNativeLibraryAvailable();
        ArDelegate arDelegate = ArDelegateProvider.getDelegate();
        if (arDelegate != null) {
            arDelegate.init();
        }
        if (getSavedInstanceState() == null && getIntent() != null) {
            VrModuleProvider.getDelegate().onNewIntentWithNative(this, getIntent());
        }
        super.finishNativeInitialization();

        mManualFillingComponent.initialize(getWindowAndroid(),
                findViewById(R.id.keyboard_accessory_stub),
                findViewById(R.id.keyboard_accessory_sheet_stub));
        getCompositorViewHolder().addCompositorViewResizer(
                mManualFillingComponent.getKeyboardExtensionViewResizer());

        if (mBottomSheet == null && shouldInitializeBottomSheet()) {
            // TODO(yusufo): Unify initialization.
            initializeBottomSheet(true);
        }

        // TODO(crbug.com/959841): Once crrev.com/c/1669360 is submitted, make the code below
        // conditional on ChromeFeatureList.isEnabled(ChromeFeatureList.DIRECT_ACTIONS)
        mDirectActionInitializer = DirectActionInitializer.create(mTabModelSelector);
        if (mDirectActionInitializer != null) {
            registerDirectActions();
        }

        AppHooks.get().startSystemSettingsObserver();
    }

    /**
     * Register direct actions to {@link #mDirectActionInitializer}, guaranteed to be non-null.
     *
     * <p>Subclasses should override this method and either extend the set of direct actions or
     * suppress registration of default direct actions.
     */
    protected void registerDirectActions() {
        mDirectActionInitializer.registerCommonChromeActions(this, getActivityType(), this,
                this::onBackPressed, mTabModelSelector, mBottomSheetController, mScrimView);
    }

    /**
     * @return OverviewModeBehavior if this activity supports an overview mode and the
     *         OverviewModeBehavior has been initialized, null otherwise.
     */
    public @Nullable OverviewModeBehavior getOverviewModeBehavior() {
        return null;
    }

    /**
     * @return {@link ObservableSupplier} for the {@link OverviewModeBehavior} for this activity
     *         if it supports an overview mode, null otherwise.
     */
    public @Nullable ObservableSupplier<OverviewModeBehavior> getOverviewModeBehaviorSupplier() {
        return null;
    }

    /**
     * @return Whether this Activity should initialize the BottomSheet and BottomSheetController.
     */
    protected boolean shouldInitializeBottomSheet() {
        return AutofillAssistantFacade.areDirectActionsAvailable(getActivityType());
    }

    /**
     * Initializes the {@link BottomSheet} and {@link BottomSheetController} for use.
     * @param suppressSheetForContextualSearch Whether the sheet should be suppressed when
     *                                         Contextual search is showing.
     */
    protected void initializeBottomSheet(boolean suppressSheetForContextualSearch) {
        ViewGroup coordinator = findViewById(R.id.coordinator);
        getLayoutInflater().inflate(R.layout.bottom_sheet, coordinator);
        mBottomSheet = coordinator.findViewById(R.id.bottom_sheet);
        mBottomSheet.init(coordinator, this);

        ((BottomContainer) findViewById(R.id.bottom_container)).setBottomSheet(mBottomSheet);

        mBottomSheetController = new BottomSheetController(this, getLifecycleDispatcher(),
                mActivityTabProvider, mScrimView, mBottomSheet,
                getCompositorViewHolder().getLayoutManager().getOverlayPanelManager(),
                suppressSheetForContextualSearch);
    }

    /**
     * @return Whether native initialization has been completed for this activity.
     */
    public boolean didFinishNativeInitialization() {
        return mNativeInitialized;
    }

    /**
     * Called when the accessibility status of this device changes.  This might be triggered by
     * touch exploration or general accessibility status updates.  It is an aggregate of two other
     * accessibility update methods.
     *
     * @see #onAccessibilityStateChanged
     * @see #mTouchExplorationStateChangeListener
     * @param enabled Whether or not accessibility and touch exploration are currently enabled.
     */
    protected void onAccessibilityModeChanged(boolean enabled) {
        InfoBarContainer.setIsAllowedToAutoHide(!enabled);
        if (mToolbarManager != null) mToolbarManager.onAccessibilityStatusChanged(enabled);
        if (mContextualSearchManager != null) {
            mContextualSearchManager.onAccessibilityModeChanged(enabled);
        }
    }

    @Override
    public boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        if (mManualFillingComponent != null) mManualFillingComponent.dismiss();
        return onMenuOrKeyboardAction(itemId, true);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item != null) {
            if (onOptionsItemSelected(item.getItemId(), null)) return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    @VisibleForTesting
    public void onShareMenuItemSelected(final boolean shareDirectly, final boolean isIncognito) {
        ShareMenuActionHandler.getInstance().onShareMenuItemSelected(
                this, getActivityTab(), shareDirectly, isIncognito);
    }

    /**
     * @return Whether the activity is in overview mode.
     */
    public boolean isInOverviewMode() {
        return false;
    }

    @CallSuper
    @Override
    public boolean canShowAppMenu() {
        if (isActivityFinishingOrDestroyed()) return false;

        @ActivityState
        int state = ApplicationStatus.getStateForActivity(this);
        boolean inMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(this);
        if (state != ActivityState.RESUMED && (!inMultiWindow || state != ActivityState.PAUSED)) {
            return false;
        }

        return true;
    }

    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new IntentHandlerDelegate() {
            @Override
            public void processWebSearchIntent(String query) {
                final Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
                searchIntent.putExtra(SearchManager.QUERY, query);
                Callback<Boolean> callback = result -> {
                    if (result != null && result) startActivity(searchIntent);
                };
                LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                        ChromeActivity.this, callback);
            }

            @Override
            public void processUrlViewIntent(String url, String referer, String headers,
                    @TabOpenType int tabOpenType, String externalAppId, int tabIdToBringToFront,
                    boolean hasUserGesture, Intent intent) {}
        };
    }

    @Override
    public final void onAccessibilityStateChanged(boolean enabled) {
        AccessibilityUtil.resetAccessibilityEnabled();
        checkAccessibility();
    }

    private void checkAccessibility() {
        onAccessibilityModeChanged(AccessibilityUtil.isAccessibilityEnabled());
    }

    /**
     * @return A casted version of {@link #getApplication()}.
     */
    public ChromeApplication getChromeApplication() {
        return (ChromeApplication) getApplication();
    }

    /**
     * Add the specified tab to bookmarks or allows to edit the bookmark if the specified tab is
     * already bookmarked. If a new bookmark is added, a snackbar will be shown.
     * @param tabToBookmark The tab that needs to be bookmarked.
     */
    public void addOrEditBookmark(final Tab tabToBookmark) {
        if (tabToBookmark == null || tabToBookmark.isFrozen()) {
            return;
        }

        // Defense in depth against the UI being erroneously enabled.
        if (!mToolbarManager.getBookmarkBridge().isEditBookmarksEnabled()) {
            assert false;
            return;
        }

        // Note the use of getUserBookmarkId() over getBookmarkId() here: Managed bookmarks can't be
        // edited. If the current URL is only bookmarked by managed bookmarks, this will return
        // INVALID_BOOKMARK_ID, so the code below will fall back on adding a new bookmark instead.
        // TODO(bauerb): This does not take partner bookmarks into account.
        final long bookmarkId = tabToBookmark.getUserBookmarkId();

        final BookmarkModel bookmarkModel = new BookmarkModel();

        bookmarkModel.finishLoadingBookmarkModel(() -> {
            // Gives up the bookmarking if the tab is being destroyed.
            if (!tabToBookmark.isClosing() && tabToBookmark.isInitialized()) {
                // The BookmarkModel will be destroyed by BookmarkUtils#addOrEditBookmark() when
                // done.
                BookmarkId newBookmarkId = BookmarkUtils.addOrEditBookmark(bookmarkId,
                        bookmarkModel, tabToBookmark, getSnackbarManager(), ChromeActivity.this,
                        isCustomTab());
                // If a new bookmark was created, try to save an offline page for it.
                if (newBookmarkId != null && newBookmarkId.getId() != bookmarkId) {
                    OfflinePageUtils.saveBookmarkOffline(newBookmarkId, tabToBookmark);
                }
            } else {
                bookmarkModel.destroy();
            }
        });
    }

    /**
     * @return Whether the tab models have been fully initialized.
     */
    public boolean areTabModelsInitialized() {
        return mTabModelsInitialized;
    }

    /**
     * {@link TabModelSelector} no longer implements TabModel.  Use getTabModelSelector() or
     * getCurrentTabModel() depending on your needs.
     * @return The {@link TabModelSelector}, possibly null.
     */
    public TabModelSelector getTabModelSelector() {
        if (!mTabModelsInitialized) {
            throw new IllegalStateException(
                    "Attempting to access TabModelSelector before initialization");
        }
        return mTabModelSelector;
    }

    /**
     * @return The provider of the visible tab in the current activity.
     */
    public ActivityTabProvider getActivityTabProvider() {
        return mActivityTabProvider;
    }

    /**
     * Returns the {@link InsetObserverView} that has the current system window
     * insets information.
     * @return The {@link InsetObserverView}, possibly null.
     */
    public InsetObserverView getInsetObserverView() {
        return mInsetObserverView;
    }

    @Override
    public TabCreatorManager.TabCreator getTabCreator(boolean incognito) {
        if (!mTabModelsInitialized) {
            throw new IllegalStateException(
                    "Attempting to access TabCreator before initialization");
        }
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    /**
     * Convenience method that returns a tab creator for the currently selected {@link TabModel}.
     * @return A tab creator for the currently selected {@link TabModel}.
     */
    public TabCreatorManager.TabCreator getCurrentTabCreator() {
        return getTabCreator(getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Gets the {@link TabContentManager} instance which holds snapshots of the tabs in this model.
     * @return The thumbnail cache, possibly null.
     */
    public TabContentManager getTabContentManager() {
        return mTabContentManager;
    }

    /**
     * Sets the {@link TabContentManager} owned by this {@link ChromeActivity}.
     * @param tabContentManager A {@link TabContentManager} instance.
     */
    private void setTabContentManager(TabContentManager tabContentManager) {
        mTabContentManager = tabContentManager;
        TabContentManagerHandler.create(tabContentManager, getTabModelSelector());
    }

    /**
     * Gets the current (inner) TabModel.  This is a convenience function for
     * getModelSelector().getCurrentModel().  It is *not* equivalent to the former getModel()
     * @return Never null, if modelSelector or its field is uninstantiated returns a
     *         {@link EmptyTabModel} singleton
     */
    public TabModel getCurrentTabModel() {
        TabModelSelector modelSelector = getTabModelSelector();
        if (modelSelector == null) return EmptyTabModel.getInstance();
        return modelSelector.getCurrentModel();
    }

    /**
     * DEPRECATED: Instead, use/hold a reference to {@link #mActivityTabProvider}. See
     *             https://crbug.com/871279 for more details.
     *
     * Returns the tab being displayed by this ChromeActivity instance. This allows differentiation
     * between ChromeActivity subclasses that swap between multiple tabs (e.g. ChromeTabbedActivity)
     * and subclasses that only display one Tab (e.g. DocumentActivity).
     *
     * The default implementation grabs the tab currently selected by the TabModel, which may be
     * null if the Tab does not exist or the system is not initialized.
     */
    public Tab getActivityTab() {
        if (!mTabModelsInitialized) {
            return null;
        }
        return TabModelUtils.getCurrentTab(getCurrentTabModel());
    }

    /**
     * @return The current WebContents, or null if the tab does not exist or is not showing a
     *         WebContents.
     */
    public WebContents getCurrentWebContents() {
        if (!mTabModelsInitialized) {
            return null;
        }
        return TabModelUtils.getCurrentWebContents(getCurrentTabModel());
    }

    /**
     * @return A {@link CompositorViewHolder} instance.
     */
    public CompositorViewHolder getCompositorViewHolder() {
        return mCompositorViewHolder;
    }

    /**
     * Gets the full screen manager, creates it unless already created.
     */
    @NonNull
    public ChromeFullscreenManager getFullscreenManager() {
        if (mFullscreenManager == null) {
            // When finish()ing, getFullscreenManager() is required to perform cleanup logic.
            // It should never be called when it results in creating a new manager though.
            if (isActivityFinishingOrDestroyed()) {
                throw new IllegalStateException();
            }
            mFullscreenManager = createFullscreenManager();
            assert mFullscreenManager != null;
        }
        return mFullscreenManager;
    }

    /**
     * Sets the overlay mode.
     * Overlay mode means that we are currently using AndroidOverlays to display video, and
     * that the compositor's surface should support alpha and not be marked as opaque.
     */
    public void setOverlayMode(boolean useOverlayMode) {
        if (mCompositorViewHolder != null) mCompositorViewHolder.setOverlayMode(useOverlayMode);
    }

    /**
     * @return The content offset provider, may be null.
     */
    public ContentOffsetProvider getContentOffsetProvider() {
        return mCompositorViewHolder;
    }

    /**
     * @return The {@code ContextualSearchManager} or {@code null} if none;
     */
    public ContextualSearchManager getContextualSearchManager() {
        return mContextualSearchManager;
    }

    /**
     * @return The {@code ReaderModeManager} or {@code null} if none;
     */
    @VisibleForTesting
    public ReaderModeManager getReaderModeManager() {
        return mReaderModeManager;
    }

    /**
     * @return The {@code EphemeralTabPanel} or {@code null} if none.
     */
    public EphemeralTabPanel getEphemeralTabPanel() {
        LayoutManager layoutManager = getCompositorViewHolder().getLayoutManager();
        return layoutManager != null ? layoutManager.getEphemeralTabPanel() : null;
    }

    /**
     * Create a full-screen manager to be used by this activity.
     * Note: This may be called before native code is initialized.
     * @return A {@link ChromeFullscreenManager} instance that's been created.
     */
    @NonNull
    protected ChromeFullscreenManager createFullscreenManager() {
        return new ChromeFullscreenManager(this, ChromeFullscreenManager.ControlsPosition.TOP);
    }

    /**
     * Exits the fullscreen mode, if any. Does nothing if no fullscreen is present.
     * @return Whether the fullscreen mode is currently showing.
     */
    protected boolean exitFullscreenIfShowing() {
        ChromeFullscreenManager fullscreenManager = getFullscreenManager();
        if (fullscreenManager.getPersistentFullscreenMode()) {
            fullscreenManager.exitPersistentFullscreenMode();
            return true;
        }
        return false;
    }

    /**
     * Initializes the {@link CompositorViewHolder} with the relevant content it needs to properly
     * show content on the screen.
     * @param layoutManager             A {@link LayoutManager} instance.  This class is
     *                                  responsible for driving all high level screen content and
     *                                  determines which {@link Layout} is shown when.
     * @param urlBar                    The {@link View} representing the URL bar (must be
     *                                  focusable) or {@code null} if none exists.
     * @param contentContainer          A {@link ViewGroup} that can have content attached by
     *                                  {@link Layout}s.
     * @param controlContainer          A {@link ControlContainer} instance to draw.
     */
    public void initializeCompositorContent(LayoutManager layoutManager, View urlBar,
            ViewGroup contentContainer, ControlContainer controlContainer) {
        if (mContextualSearchManager != null) {
            mContextualSearchManager.initialize(contentContainer);
            mContextualSearchManager.setSearchContentViewDelegate(layoutManager);
        }

        layoutManager.addSceneChangeObserver(this);
        mCompositorViewHolder.setLayoutManager(layoutManager);
        mCompositorViewHolder.setFocusable(false);
        mCompositorViewHolder.setControlContainer(controlContainer);
        mCompositorViewHolder.setFullscreenHandler(getFullscreenManager());
        mCompositorViewHolder.setUrlBar(urlBar);
        mCompositorViewHolder.setInsetObserverView(getInsetObserverView());
        mCompositorViewHolder.onFinishNativeInitialization(getTabModelSelector(), this,
                getTabContentManager(), contentContainer, mContextualSearchManager);

        if (controlContainer != null && DeviceClassManager.enableToolbarSwipe()
                && getCompositorViewHolder().getLayoutManager().getToolbarSwipeHandler() != null) {
            controlContainer.setSwipeHandler(
                    getCompositorViewHolder().getLayoutManager().getToolbarSwipeHandler());
        }

        mActivityTabProvider.setLayoutManager(layoutManager);
        EphemeralTabPanel panel = layoutManager.getEphemeralTabPanel();
        if (panel != null) panel.setChromeActivity(this);

        mLayoutManagerSupplier.set(layoutManager);
    }

    /**
     * @return An {@link ObservableSupplier} that will supply the {@link LayoutManager} when it is
     *         ready.
     */
    public ObservableSupplier<LayoutManager> getLayoutManagerSupplier() {
        return mLayoutManagerSupplier;
    }

    /**
     * Called when the back button is pressed.
     * @return Whether or not the back button was handled.
     */
    protected abstract boolean handleBackPressed();

    /**
     * @return If no higher priority back actions occur, whether pressing the back button
     *         would result in closing the tab. A true return value does not guarantee that
     *         a subsequent call to {@link #handleBackPressed()} will close the tab.
     */
    public boolean backShouldCloseTab(Tab tab) {
        return false;
    }

    @Override
    public void onOrientationChange(int orientation) {
        if (mToolbarManager != null) mToolbarManager.onOrientationChange();
    }

    /**
     * Notified when the focus of the omnibox has changed.
     *
     * @param hasFocus Whether the omnibox currently has focus.
     */
    protected void onOmniboxFocusChanged(boolean hasFocus) {}

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // We only handle VR UI mode and UI mode night changes. Any other changes should follow the
        // default behavior of recreating the activity. Note that if UI mode night changes, with or
        // without other changes, we will still recreate() until we get a callback from the
        // ChromeBaseAppCompatActivity#onNightModeStateChanged or the overridden method in
        // sub-classes if necessary.
        if (didChangeNonVrUiMode(mUiMode, newConfig.uiMode)
                && !didChangeUiModeNight(mUiMode, newConfig.uiMode)) {
            recreate();
            return;
        }
        mUiMode = newConfig.uiMode;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            if (newConfig.densityDpi != mDensityDpi) {
                if (!VrModuleProvider.getDelegate().onDensityChanged(
                            mDensityDpi, newConfig.densityDpi)) {
                    recreate();
                    return;
                }
                mDensityDpi = newConfig.densityDpi;
            }
        }
    }

    private static boolean didChangeNonVrUiMode(int oldMode, int newMode) {
        if (oldMode == newMode) return false;
        return isInVrUiMode(oldMode) == isInVrUiMode(newMode);
    }

    private static boolean isInVrUiMode(int uiMode) {
        return (uiMode & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_VR_HEADSET;
    }

    private static boolean didChangeUiModeNight(int oldMode, int newMode) {
        return (oldMode & Configuration.UI_MODE_NIGHT_MASK)
                != (newMode & Configuration.UI_MODE_NIGHT_MASK);
    }

    /**
     * Called by the system when the activity changes from fullscreen mode to multi-window mode
     * and visa-versa.
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     */
    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        // If native is not initialized, the multi-window user action will be recorded in
        // #onDeferredStartupForMultiWindowMode() and FeatureUtilities#setIsInMultiWindowMode() will
        // be called in #onResumeWithNative(). Both of these methods require native to be
        // initialized, so do not call here to avoid crashing. See https://crbug.com/797921.
        if (mNativeInitialized) {
            recordMultiWindowModeChangedUserAction(isInMultiWindowMode);

            if (!isInMultiWindowMode
                    && ApplicationStatus.getStateForActivity(this) == ActivityState.RESUMED) {
                // Start a new UMA session when exiting multi-window mode if the activity is
                // currently resumed. When entering multi-window Android recents gains focus, so
                // ChromeActivity will get a call to onPauseWithNative(), ending the current UMA
                // session. When exiting multi-window, however, if ChromeActivity is resumed it
                // stays in that state.
                markSessionEnd();
                markSessionResume();
                FeatureUtilities.setIsInMultiWindowMode(
                        MultiWindowUtils.getInstance().isInMultiWindowMode(this));
            }
        }

        VrModuleProvider.getDelegate().onMultiWindowModeChanged(isInMultiWindowMode);

        super.onMultiWindowModeChanged(isInMultiWindowMode);
    }

    /**
     * Records user actions associated with entering and exiting Android N multi-window mode
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     */
    protected void recordMultiWindowModeChangedUserAction(boolean isInMultiWindowMode) {
        if (isInMultiWindowMode) {
            RecordUserAction.record("Android.MultiWindowMode.Enter");
        } else {
            RecordUserAction.record("Android.MultiWindowMode.Exit");
        }
    }

    @Override
    public final void onBackPressed() {
        if (mNativeInitialized) RecordUserAction.record("SystemBack");

        TextBubble.dismissBubbles();
        if (VrModuleProvider.getDelegate().onBackPressed()) return;
        if (mCompositorViewHolder != null) {
            LayoutManager layoutManager = mCompositorViewHolder.getLayoutManager();
            if (layoutManager != null && layoutManager.onBackPressed()) return;
        }

        SelectionPopupController controller = getSelectionPopupController();
        if (controller != null && controller.isSelectActionBarShowing()) {
            controller.clearSelection();
            return;
        }

        if (handleBackPressed()) return;

        super.onBackPressed();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (ChromeApplication.isSevereMemorySignal(level)) {
            mReferencePool.drain();
            clearToolbarResourceCache();
        }
    }

    private SelectionPopupController getSelectionPopupController() {
        WebContents webContents = getCurrentWebContents();
        return webContents != null ? SelectionPopupController.fromWebContents(webContents) : null;
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        Tab currentTab = getActivityTab();
        if (currentTab == null) return;

        TabCreator tabCreator = getTabCreator(currentTab.isIncognito());
        if (tabCreator == null) return;

        tabCreator.createNewTab(new LoadUrlParams(searchUrl, PageTransition.LINK),
                TabLaunchType.FROM_LINK, getActivityTab());
    }

    /**
     * Callback for when UpdateMenuItemHelper has a state change.
     */
    public void onUpdateStateChanged() {
        if (isActivityFinishingOrDestroyed()) return;

        MenuButtonState buttonState = UpdateMenuItemHelper.getInstance().getUiState().buttonState;

        if (buttonState != null) {
            mToolbarManager.showAppMenuUpdateBadge();
            mCompositorViewHolder.requestRender();
        } else {
            mToolbarManager.removeAppMenuUpdateBadge(false);
        }
    }

    /**
     * @return The {@link MenuOrKeyboardActionController} for registering menu or keyboard action
     *         handler for this activity.
     */
    public MenuOrKeyboardActionController getMenuOrKeyboardActionController() {
        return this;
    }

    @Override
    public void registerMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler) {
        mMenuActionHandlers.add(handler);
    }

    @Override
    public void unregisterMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler) {
        mMenuActionHandlers.remove(handler);
    }

    /**
     * Handles menu item selection and keyboard shortcuts.
     *
     * @param id The ID of the selected menu item (defined in main_menu.xml) or
     *           keyboard shortcut (defined in values.xml).
     * @param fromMenu Whether this was triggered from the menu.
     * @return Whether the action was handled.
     */
    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        for (MenuOrKeyboardActionController.MenuOrKeyboardActionHandler handler :
                mMenuActionHandlers) {
            if (handler.handleMenuOrKeyboardAction(id, fromMenu)) return true;
        }

        if (id == R.id.preferences_id) {
            PreferencesLauncher.launchSettingsPageCompat(this, null);
            RecordUserAction.record("MobileMenuSettings");
        }

        if (id == R.id.update_menu_id) {
            UpdateMenuItemHelper.getInstance().onMenuItemClicked(this);
            return true;
        }

        final Tab currentTab = getActivityTab();

        if (id == R.id.help_id) {
            String url = currentTab != null ? currentTab.getUrl() : "";
            Profile profile = mTabModelSelector.isIncognitoSelected()
                    ? Profile.getLastUsedProfile().getOffTheRecordProfile()
                    : Profile.getLastUsedProfile().getOriginalProfile();
            startHelpAndFeedback(url, "MobileMenuFeedback", profile);
            return true;
        }

        // All the code below assumes currentTab is not null, so return early if it is null.
        if (currentTab == null) {
            return false;
        } else if (id == R.id.forward_menu_id) {
            if (currentTab.canGoForward()) {
                currentTab.goForward();
                RecordUserAction.record("MobileMenuForward");
            }
        } else if (id == R.id.bookmark_this_page_id) {
            addOrEditBookmark(currentTab);
            RecordUserAction.record("MobileMenuAddToBookmarks");
        } else if (id == R.id.offline_page_id) {
            DownloadUtils.downloadOfflinePage(this, currentTab);
            RecordUserAction.record("MobileMenuDownloadPage");
        } else if (id == R.id.reload_menu_id) {
            if (currentTab.isLoading()) {
                currentTab.stopLoading();
                RecordUserAction.record("MobileMenuStop");
            } else {
                currentTab.reload();
                RecordUserAction.record("MobileMenuReload");
            }
        } else if (id == R.id.info_menu_id) {
            PageInfoController.show(
                    this, currentTab, null, PageInfoController.OpenedFromSource.MENU);
        } else if (id == R.id.open_history_menu_id) {
            if (NewTabPage.isNTPUrl(currentTab.getUrl())) {
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_HISTORY_MANAGER);
            }
            RecordUserAction.record("MobileMenuHistory");
            HistoryManagerUtils.showHistoryManager(this, currentTab);
        } else if (id == R.id.translate_id) {
            RecordUserAction.record("MobileMenuTranslate");
            Tracker tracker = TrackerFactory.getTrackerForProfile(getActivityTab().getProfile());
            tracker.notifyEvent(EventConstants.TRANSLATE_MENU_BUTTON_CLICKED);
            TranslateBridge.translateTabWhenReady(getActivityTab());
        } else if (id == R.id.share_menu_id || id == R.id.direct_share_menu_id) {
            onShareMenuItemSelected(id == R.id.direct_share_menu_id,
                    getCurrentTabModel().isIncognito());
        } else if (id == R.id.print_id) {
            PrintingController printingController = PrintingControllerImpl.getInstance();
            if (printingController != null && !printingController.isBusy()
                    && PrefServiceBridge.getInstance().isPrintingEnabled()) {
                printingController.startPrint(new TabPrinter(currentTab),
                        new PrintManagerDelegateImpl(this));
                RecordUserAction.record("MobileMenuPrint");
            }
        } else if (id == R.id.add_to_homescreen_id) {
            AddToHomescreenManager addToHomescreenManager =
                    new AddToHomescreenManager(this, currentTab);
            addToHomescreenManager.start();
            RecordUserAction.record("MobileMenuAddToHomescreen");
        } else if (id == R.id.open_webapk_id) {
            Context context = ContextUtils.getApplicationContext();
            String packageName =
                    WebApkValidator.queryFirstWebApkPackage(context, currentTab.getUrl());
            Intent launchIntent = WebApkNavigationClient.createLaunchWebApkIntent(
                    packageName, currentTab.getUrl(), false);
            try {
                context.startActivity(launchIntent);
                RecordUserAction.record("MobileMenuOpenWebApk");
            } catch (ActivityNotFoundException e) {
                Toast.makeText(context, R.string.open_webapk_failed, Toast.LENGTH_SHORT).show();
            }
        } else if (id == R.id.request_desktop_site_id || id == R.id.request_desktop_site_check_id) {
            final boolean reloadOnChange = !currentTab.isNativePage();
            final boolean usingDesktopUserAgent =
                    currentTab.getWebContents().getNavigationController().getUseDesktopUserAgent();
            currentTab.getWebContents().getNavigationController().setUseDesktopUserAgent(
                    !usingDesktopUserAgent, reloadOnChange);
            RecordUserAction.record("MobileMenuRequestDesktopSite");
        } else if (id == R.id.reader_mode_prefs_id) {
            DomDistillerUIUtils.openSettings(currentTab.getWebContents());
        } else {
            return false;
        }
        return true;
    }

    /**
     * Shows HelpAndFeedback and records the user action as well.
     * @param url The URL of the tab the user is currently on.
     * @param recordAction The user action to record.
     * @param profile The current {@link Profile}.
     */
    public void startHelpAndFeedback(String url, String recordAction, Profile profile) {
        // Since reading back the compositor is asynchronous, we need to do the readback
        // before starting the GoogleHelp.
        String helpContextId = HelpAndFeedback.getHelpContextIdFromUrl(
                this, url, getCurrentTabModel().isIncognito());
        HelpAndFeedback.getInstance(this).show(this, helpContextId, profile, url);
        RecordUserAction.record(recordAction);
    }

    /**
     * Add a view to the set of views that obscure the content of all tabs for
     * accessibility. As long as this set is nonempty, all tabs should be
     * hidden from the accessibility tree.
     *
     * @param view The view that obscures the contents of all tabs.
     */
    public void addViewObscuringAllTabs(View view) {
        mViewsObscuringAllTabs.add(view);

        Tab tab = getActivityTab();
        if (tab != null) tab.updateAccessibilityVisibility();
    }

    /**
     * Remove a view that previously obscured the content of all tabs.
     *
     * @param view The view that no longer obscures the contents of all tabs.
     */
    public void removeViewObscuringAllTabs(View view) {
        mViewsObscuringAllTabs.remove(view);

        Tab tab = getActivityTab();
        if (tab != null) tab.updateAccessibilityVisibility();
    }

    /**
     * Returns whether or not any views obscure all tabs.
     */
    public boolean isViewObscuringAllTabs() {
        return !mViewsObscuringAllTabs.isEmpty();
    }

    private void markSessionResume() {
        // Start new session for UMA.
        if (mUmaSessionStats == null) {
            mUmaSessionStats = new UmaSessionStats(this);
        }

        UmaSessionStats.updateMetricsServiceState();
        mUmaSessionStats.startNewSession(getTabModelSelector());
    }

    /**
     * Mark that the UMA session has ended.
     */
    private void markSessionEnd() {
        if (mUmaSessionStats == null) {
            // If you hit this assert, please update crbug.com/172653 on how you got there.
            assert false;
            return;
        }
        // Record session metrics.
        mUmaSessionStats.logAndEndSession();
    }

    public final void postDeferredStartupIfNeeded() {
        if (!mNativeInitialized) {
            // Native hasn't loaded yet.  Queue it up for later.
            mDeferredStartupQueued = true;
            return;
        }
        mDeferredStartupQueued = false;

        if (!mDeferredStartupPosted) {
            mDeferredStartupPosted = true;
            onDeferredStartup();
        }
    }

    @Override
    public void terminateIncognitoSession() {}

    @Override
    public void onTabSelectionHinted(int tabId) { }

    @Override
    public void onSceneChange(Layout layout) { }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        // See enableHardwareAcceleration()
        if (mSetWindowHWA) {
            mSetWindowHWA = false;
            getWindow().setWindowManager(
                    getWindow().getWindowManager(),
                    getWindow().getAttributes().token,
                    getComponentName().flattenToString(),
                    true /* hardwareAccelerated */);
        }
    }

    private boolean shouldDisableHardwareAcceleration() {
        // Low end devices should disable hardware acceleration for memory gains.
        if (SysUtils.isLowEndDevice()) return true;

        // Turning off hardware acceleration reduces crash rates. See http://crbug.com/651918
        // GT-S7580 on JDQ39 accounts for 42% of crashes in libPowerStretch.so on dev and beta.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.JELLY_BEAN_MR1
                && Build.MODEL.equals("GT-S7580")) {
            return true;
        }
        // SM-N9005 on JSS15J accounts for 44% of crashes in libPowerStretch.so on stable channel.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.JELLY_BEAN_MR2
                && Build.MODEL.equals("SM-N9005")) {
            return true;
        }
        return false;
    }

    private void enableHardwareAcceleration() {
        // HW acceleration is disabled in the manifest and may be re-enabled here.
        if (!shouldDisableHardwareAcceleration()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);

            // When HW acceleration is enabled manually for an activity, child windows (e.g.
            // dialogs) don't inherit HW acceleration state. However, when HW acceleration is
            // enabled in the manifest, child windows do inherit HW acceleration state. That
            // looks like a bug, so I filed b/23036374
            //
            // In the meanwhile the workaround is to call
            //   window.setWindowManager(..., hardwareAccelerated=true)
            // to let the window know that it's HW accelerated. However, since there is no way
            // to know 'appToken' argument until window's view is attached to the window (!!),
            // we have to do the workaround in onAttachedToWindow()
            mSetWindowHWA = true;
        }
    }

    /** @return the theme ID to use. */
    public static int getThemeId() {
        boolean useLowEndTheme =
                SysUtils.isLowEndDevice() && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
        return (useLowEndTheme ? R.style.Theme_Chromium_WithWindowAnimation_LowEnd
                               : R.style.Theme_Chromium_WithWindowAnimation);
    }

    /**
     * Looks up the Chrome activity of the given web contents. This can be null. Should never be
     * cached, because web contents can change activities, e.g., when user selects "Open in Chrome"
     * menu item.
     *
     * @param webContents The web contents for which to lookup the Chrome activity.
     * @return Possibly null Chrome activity that should never be cached.
     */
    @Nullable public static ChromeActivity fromWebContents(@Nullable WebContents webContents) {
        if (webContents == null) return null;

        if (webContents.isDestroyed()) return null;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        Activity activity = window.getActivity().get();
        if (activity == null) return null;
        if (!(activity instanceof ChromeActivity)) return null;

        return (ChromeActivity) activity;
    }

    private void setLowEndTheme() {
        if (getThemeId() == R.style.Theme_Chromium_WithWindowAnimation_LowEnd)
            setTheme(R.style.Theme_Chromium_WithWindowAnimation_LowEnd);
    }

    /**
     * Records histograms related to display dimensions.
     */
    private void recordDisplayDimensions() {
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(this);
        int displayWidth = DisplayUtil.pxToDp(display, display.getDisplayWidth());
        int displayHeight = DisplayUtil.pxToDp(display, display.getDisplayHeight());
        int largestDisplaySize = displayWidth > displayHeight ? displayWidth : displayHeight;
        int smallestDisplaySize = displayWidth < displayHeight ? displayWidth : displayHeight;

        RecordHistogram.recordSparseHistogram("Android.DeviceSize.SmallestDisplaySize",
                MathUtils.clamp(smallestDisplaySize, 0, 1000));
        RecordHistogram.recordSparseHistogram("Android.DeviceSize.LargestDisplaySize",
                MathUtils.clamp(largestDisplaySize, 200, 1200));
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (super.onActivityResultWithNative(requestCode, resultCode, intent)) return true;
        if (VrModuleProvider.getDelegate().onActivityResultWithNative(requestCode, resultCode))
            return true;
        return false;
    }

    /**
     * Called when VR mode is entered using this activity. 2D UI components that steal focus or
     * draw over VR contents should be hidden in this call.
     */
    public void onEnterVr() {}

    /**
     * Called when VR mode using this activity is exited. Any state set for VR should be restored
     * in this call, including showing 2D UI that was hidden.
     */
    public void onExitVr() {}

    /**
     * @return the reference pool for this activity.
     * @deprecated Use {@link GlobalDiscardableReferencePool#getReferencePool} instead.
     */
    // TODO(bauerb): Migrate clients to GlobalDiscardableReferencePool#getReferencePool.
    @Deprecated
    public DiscardableReferencePool getReferencePool() {
        return mReferencePool;
    }

    private void clearToolbarResourceCache() {
        ControlContainer controlContainer = (ControlContainer) findViewById(R.id.control_container);
        if (controlContainer != null) {
            controlContainer.getToolbarResourceAdapter().dropCachedBitmap();
        }
    }

    @Override
    public void startActivity(Intent intent) {
        startActivity(intent, null);
    }

    @Override
    public void startActivity(Intent intent, Bundle options) {
        if (VrModuleProvider.getDelegate().canLaunch2DIntents()
                || VrModuleProvider.getIntentDelegate().isVrIntent(intent)) {
            if (VrModuleProvider.getDelegate().isInVr()) {
                VrModuleProvider.getIntentDelegate().setupVrIntent(intent);
            }
            super.startActivity(intent, options);
            return;
        }
        VrModuleProvider.getDelegate().requestToExitVrAndRunOnSuccess(() -> {
            if (!VrModuleProvider.getDelegate().canLaunch2DIntents()) {
                throw new IllegalStateException("Still in VR after having exited VR.");
            }
            super.startActivity(intent, options);
        });
    }

    @Override
    public void startActivityForResult(Intent intent, int requestCode) {
        startActivityForResult(intent, requestCode, null);
    }

    @Override
    public void startActivityForResult(Intent intent, int requestCode, Bundle options) {
        if (VrModuleProvider.getDelegate().canLaunch2DIntents()
                || VrModuleProvider.getIntentDelegate().isVrIntent(intent)) {
            super.startActivityForResult(intent, requestCode, options);
            return;
        }
        VrModuleProvider.getDelegate().requestToExitVrAndRunOnSuccess(() -> {
            if (!VrModuleProvider.getDelegate().canLaunch2DIntents()) {
                throw new IllegalStateException("Still in VR after having exited VR.");
            }
            super.startActivityForResult(intent, requestCode, options);
        });
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent, int requestCode) {
        return startActivityIfNeeded(intent, requestCode, null);
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent, int requestCode, Bundle options) {
        // Avoid starting Activities when possible while in VR.
        if (VrModuleProvider.getDelegate().isInVr()
                && !VrModuleProvider.getIntentDelegate().isVrIntent(intent))
            return false;
        return super.startActivityIfNeeded(intent, requestCode, options);
    }

    /**
     * If the density of the device changes while Chrome is in the background (not resumed), we
     * won't have received an onConfigurationChanged yet for this new density. In this case, the
     * density this Activity thinks it's in, and the actual display density will differ.
     * @return The density this Activity thinks it's in (the density it was in last time it was in
     *         the resumed state).
     */
    public float getLastActiveDensity() {
        return mDensityDpi;
    }

    /**
     * TODO(https://crbug.com/931496): Revisit this as part of the broader discussion around
     * activity-specific UI customizations.
     * @return Whether this Activity supports the App Menu.
     */
    public boolean supportsAppMenu() {
        if (FeatureUtilities.isNoTouchModeEnabled()) return false;

        // Derived classes that disable the toolbar should also have the Menu disabled without
        // having to explicitly disable the Menu as well.
        return getToolbarLayoutId() != NO_TOOLBAR_LAYOUT;
    }

    /**
     * @return  Whether this activity supports the find in page feature.
     */
    public boolean supportsFindInPage() {
        return true;
    }

    /**
     * TODO(mthiesse): Figure out a way to clean this up. The problem is that the
     * TouchlessUiCoordinator has an implementation of the ModalDialogManager, which is created in
     * AsyncInitializationActivity#onCreateInternal, before any ChromeActivity init functions are
     * called, and making AsyncInitializationActivity aware of the TouchlessUiCoordinator would be
     * wrong. Hence, we create the UiCoordinator as soon as somebody tries to use it, but we also
     * need to make sure it gets initialized early on regardless of whether somebody tries to use it
     * as it monitors Lifecycles, etc.
     */
    public TouchlessUiCoordinator getTouchlessUiCoordinator() {
        if (mTouchlessUiCoordinator == null && FeatureUtilities.isNoTouchModeEnabled()) {
            mTouchlessUiCoordinator = AppHooks.get().createTouchlessUiCoordinator(this);
        }
        return mTouchlessUiCoordinator;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        KeyEvent toPropagate = getTouchlessUiCoordinator() != null
                ? getTouchlessUiCoordinator().processKeyEvent(event)
                : event;

        return toPropagate == null || super.dispatchKeyEvent(toPropagate);
    }

    /** Returns {@link BottomSheetController}, if present. */
    @Nullable
    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    @VisibleForTesting
    public RootUiCoordinator getRootUiCoordinatorForTesting() {
        return mRootUiCoordinator;
    }
}
