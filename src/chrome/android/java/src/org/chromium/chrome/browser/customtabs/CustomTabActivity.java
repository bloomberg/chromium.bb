// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static android.support.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static android.support.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.USER_NAVIGATION;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsSessionToken;
import android.text.TextUtils;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.RemoteViews;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabTaskDescriptionHelper;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.browserservices.BrowserSessionContentHandler;
import org.chromium.chrome.browser.browserservices.BrowserSessionContentUtils;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleCoordinator;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.incognito.IncognitoTabHost;
import org.chromium.chrome.browser.incognito.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends ChromeActivity<CustomTabActivityComponent> {
    private static final String TAG = "CustomTabActivity";

    // For CustomTabs.ConnectionStatusOnReturn, see histograms.xml. Append only.
    @IntDef({ConnectionStatus.DISCONNECTED, ConnectionStatus.DISCONNECTED_KEEP_ALIVE,
            ConnectionStatus.CONNECTED, ConnectionStatus.CONNECTED_KEEP_ALIVE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ConnectionStatus {
        int DISCONNECTED = 0;
        int DISCONNECTED_KEEP_ALIVE = 1;
        int CONNECTED = 2;
        int CONNECTED_KEEP_ALIVE = 3;
        int NUM_ENTRIES = 4;
    }

    private CustomTabIntentDataProvider mIntentDataProvider;
    private CustomTabsSessionToken mSession;
    private BrowserSessionContentHandler mBrowserSessionContentHandler;
    private CustomTabBottomBarDelegate mBottomBarDelegate;
    private CustomTabTopBarDelegate mTopBarDelegate;
    private CustomTabActivityTabController mTabController;
    private CustomTabActivityTabProvider mTabProvider;
    private CustomTabActivityTabFactory mTabFactory;
    private CustomTabActivityNavigationController mNavigationController;
    private CustomTabStatusBarColorProvider mCustomTabStatusBarColorProvider;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    private boolean mShouldOverridePackage;

    /** Adds and removes observers from tabs when needed. */
    private TabObserverRegistrar mTabObserverRegistrar;

    private boolean mIsKeepAlive;

    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();

    @Nullable
    private DynamicModuleCoordinator mDynamicModuleCoordinator;

    private ActivityTabTaskDescriptionHelper mTaskDescriptionHelper;

    private CustomTabNightModeStateController mNightModeStateController;

    /**
     * Return true when the activity has been launched in a separate task. The default behavior is
     * to reuse the same task and put the activity on top of the previous one (i.e hiding it). A
     * separate task creates a new entry in the Android recent screen.
     **/
    private boolean useSeparateTask() {
        final int separateTaskFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        return (getIntent().getFlags() & separateTaskFlags) != 0;
    }

    private CustomTabActivityTabProvider.Observer mTabChangeObserver =
            new CustomTabActivityTabProvider.Observer() {
        @Override
        public void onInitialTabCreated(@NonNull Tab tab, int mode) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onTabSwapped(@NonNull Tab tab) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onAllTabsClosed() {
            resetPostMessageHandlersForCurrentSession();
        }
    };

    @Nullable
    private IncognitoTabHost mIncognitoTabHost;

    @Override
    protected Drawable getBackgroundDrawable() {
        int initialBackgroundColor = mIntentDataProvider.getInitialBackgroundColor();
        if (mIntentDataProvider.isTrustedIntent() && initialBackgroundColor != Color.TRANSPARENT) {
            return new ColorDrawable(initialBackgroundColor);
        } else {
            return super.getBackgroundDrawable();
        }
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.CUSTOM_TAB;
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.CustomTabs", timeMs);
    }

    @Override
    public void onStart() {
        super.onStart();
        mIsKeepAlive = mConnection.keepAliveForSession(
                mIntentDataProvider.getSession(), mIntentDataProvider.getKeepAliveServiceIntent());
    }

    @Override
    public void onStop() {
        super.onStop();
        mConnection.dontKeepAliveForSession(mIntentDataProvider.getSession());
        mIsKeepAlive = false;
    }

    @Override
    public void performPreInflationStartup() {
        // Parse the data from the Intent before calling super to allow the Intent to customize
        // the Activity parameters, including the background of the page.
        // Note that color scheme is fixed for the lifetime of Activity: if the system setting
        // changes, we recreate the activity.
        mIntentDataProvider = new CustomTabIntentDataProvider(getIntent(), this, getColorScheme());

        super.performPreInflationStartup();
        mTabProvider.addObserver(mTabChangeObserver);
        // We might have missed an onInitialTabCreated event.
        resetPostMessageHandlersForCurrentSession();

        mSession = mIntentDataProvider.getSession();

        if (mIntentDataProvider.isIncognito()) {
            initializeIncognito();
        }

        initalizePreviewsObserver();
        CustomTabNavigationBarController.updateNavigationBarColor(this, mIntentDataProvider);
    }

    private int getColorScheme() {
        if (mNightModeStateController != null) {
            return mNightModeStateController.isInNightMode() ? COLOR_SCHEME_DARK :
                    COLOR_SCHEME_LIGHT;
        }
        assert false : "NightModeStateController should have been already created";
        return COLOR_SCHEME_LIGHT;
    }

    private void initializeIncognito() {
        mIncognitoTabHost = new IncognitoCustomTabHost();
        IncognitoTabHostRegistry.getInstance().register(mIncognitoTabHost);

        if (!CommandLine.getInstance().hasSwitch(
                ChromeSwitches.ENABLE_INCOGNITO_SNAPSHOTS_IN_ANDROID_RECENTS)) {
            // Disable taking screenshots and seeing snapshots in recents
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }
    }

    @Override
    public boolean shouldAllocateChildConnection() {
        return mTabController.shouldAllocateChildConnection();
    }

    @Override
    public void performPostInflationStartup() {
        super.performPostInflationStartup();

        getToolbarManager().setCloseButtonDrawable(FeatureUtilities.isNoTouchModeEnabled()
                        ? null
                        : mIntentDataProvider.getCloseButtonDrawable());
        getToolbarManager().setShowTitle(
                mIntentDataProvider.getTitleVisibilityState() == CustomTabsIntent.SHOW_PAGE_TITLE);
        if (mConnection.shouldHideDomainForSession(mSession)) {
            getToolbarManager().setUrlBarHidden(true);
        }
        int toolbarColor = mIntentDataProvider.getToolbarColor();
        getToolbarManager().onThemeColorChanged(toolbarColor, false);
        if (!mIntentDataProvider.isOpenedByChrome()) {
            getToolbarManager().setShouldUpdateToolbarPrimaryColor(false);
        }

        getStatusBarColorController().updateStatusBarColor(ColorUtils.isUsingDefaultToolbarColor(
                getResources(), false, getBaseStatusBarColor()));

        // Properly attach tab's InfoBarContainer to the view hierarchy if the tab is already
        // attached to a ChromeActivity, as the main tab might have been initialized prior to
        // inflation.
        if (mTabProvider.getTab() != null) {
            ViewGroup bottomContainer = (ViewGroup) findViewById(R.id.bottom_container);
            InfoBarContainer.get(mTabProvider.getTab()).setParentView(bottomContainer);
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        ApiCompatibilityUtils.setTaskDescription(this, null, null, toolbarColor);
        showCustomButtonsOnToolbar();
        mBottomBarDelegate = getComponent().resolveBottomBarDelegate();
        mBottomBarDelegate.showBottomBarIfNecessary();
        mTopBarDelegate = getComponent().resolveTobBarDelegate();
    }

    @Override
    protected TabModelSelector createTabModelSelector() {
        return mTabFactory.createTabModelSelector();
    }

    @Override
    protected Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return mTabFactory.createTabCreators();
    }

    @Override
    protected NightModeStateProvider createNightModeStateProvider() {
        mNightModeStateController = new CustomTabNightModeStateController(getLifecycleDispatcher());
        return mNightModeStateController;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        mNightModeStateController.initialize(getDelegate(), getIntent());
    }

    @Override
    public void finishNativeInitialization() {
        if (!mIntentDataProvider.isInfoPage()) FirstRunSignInProcessor.start(this);

        // Try to initialize dynamic module early to enqueue navigation events
        // @see DynamicModuleNavigationEventObserver
        if (mIntentDataProvider.isDynamicModuleEnabled()) {
            mDynamicModuleCoordinator = getComponent().resolveDynamicModuleCoordinator();
        }

        LayoutManager layoutDriver = new LayoutManager(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content),
                (ToolbarControlContainer) findViewById(R.id.control_container));
        getToolbarManager().initializeWithNative(getTabModelSelector(),
                getFullscreenManager().getBrowserVisibilityDelegate(), getFindToolbarManager(),
                null, layoutDriver, null, null, null, new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        RecordUserAction.record("CustomTabs.CloseButtonClicked");
                        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()) {
                            RecordUserAction.record("CustomTabs.CloseButtonClicked.DownloadsUI");
                        }
                        mNavigationController.navigateOnClose();
                    }
                });

        mBrowserSessionContentHandler = new BrowserSessionContentHandler() {
            @Override
            public void loadUrlAndTrackFromTimestamp(LoadUrlParams params, long timestamp) {
                if (!TextUtils.isEmpty(params.getUrl())) {
                    params.setUrl(DataReductionProxySettings.getInstance()
                            .maybeRewriteWebliteUrl(params.getUrl()));
                }
                mNavigationController.navigate(params, timestamp);
            }

            @Override
            public CustomTabsSessionToken getSession() {
                return mSession;
            }

            @Override
            public boolean shouldIgnoreIntent(Intent intent) {
                return mIntentHandler.shouldIgnoreIntent(intent);
            }

            @Override
            public boolean updateCustomButton(int id, Bitmap bitmap, String description) {
                CustomButtonParams params = mIntentDataProvider.getButtonParamsForId(id);
                if (params == null) {
                    Log.w(TAG, "Custom toolbar button with ID %d not found", id);
                    return false;
                }

                params.update(bitmap, description);
                if (params.showOnToolbar()) {
                    if (!CustomButtonParams.doesIconFitToolbar(CustomTabActivity.this, bitmap)) {
                        return false;
                    }
                    int index = mIntentDataProvider.getCustomToolbarButtonIndexForId(id);
                    assert index != -1;
                    getToolbarManager().updateCustomActionButton(
                            index, params.getIcon(CustomTabActivity.this), description);
                } else {
                    if (mBottomBarDelegate != null) {
                        mBottomBarDelegate.updateBottomBarButtons(params);
                    }
                }
                return true;
            }

            @Override
            public boolean updateRemoteViews(RemoteViews remoteViews, int[] clickableIDs,
                    PendingIntent pendingIntent) {
                if (mBottomBarDelegate == null) return false;
                return mBottomBarDelegate.updateRemoteViews(
                        remoteViews, clickableIDs, pendingIntent);
            }

            @Override
            @Nullable
            public String getCurrentUrl() {
                return getActivityTab() == null ? null : getActivityTab().getUrl();
            }

            @Override
            @Nullable
            public String getPendingUrl() {
                if (getActivityTab() == null) return null;
                if (getActivityTab().getWebContents() == null) return null;

                NavigationEntry entry = getActivityTab().getWebContents().getNavigationController()
                        .getPendingEntry();
                return entry != null ? entry.getUrl() : null;
            }

            @Override
            public int getTaskId() {
                return CustomTabActivity.this.getTaskId();
            }

            @Override
            public Class<? extends Activity> getActivityClass() {
                return CustomTabActivity.this.getClass();
            }
        };

        mConnection.showSignInToastIfNecessary(mSession, getIntent());

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && useSeparateTask()) {
            mTaskDescriptionHelper = new ActivityTabTaskDescriptionHelper(this,
                    ApiCompatibilityUtils.getColor(getResources(), R.color.default_primary_color));
        }

        if (isTaskRoot() && BuildInfo.isAtLeastQ()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.USAGE_STATS)) {
            UsageStatsService.getInstance().createPageViewObserver(getTabModelSelector(), this);
        }

        super.finishNativeInitialization();

        // We start the Autofill Assistant after the call to super.finishNativeInitialization() as
        // this will initialize the BottomSheet that is used to embed the Autofill Assistant bottom
        // bar.
        if (isAutofillAssistantEnabled()) {
            AutofillAssistantFacade.start(this);
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        Intent originalIntent = getIntent();
        super.onNewIntent(intent);
        // Currently we can't handle arbitrary updates of intent parameters, so make sure
        // getIntent() returns the same intent as before.
        setIntent(originalIntent);
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        super.onNewIntentWithNative(intent);
        BrowserSessionContentUtils.setActiveContentHandler(mBrowserSessionContentHandler);
        if (!BrowserSessionContentUtils.handleBrowserServicesIntent(intent)) {
            int flagsToRemove = Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP;
            intent.setFlags(intent.getFlags() & ~flagsToRemove);
            startActivity(intent);
        }
    }

    private void resetPostMessageHandlersForCurrentSession() {
        Tab tab = mTabProvider.getTab();
        WebContents webContents = tab == null ? null : tab.getWebContents();
        mConnection.resetPostMessageHandlerForSession(
                mIntentDataProvider.getSession(), webContents);

        if (mDynamicModuleCoordinator != null) {
            mDynamicModuleCoordinator.resetPostMessageHandlersForCurrentSession(null);
        }
    }

    private void initalizePreviewsObserver() {
        mTabObserverRegistrar.registerTabObserver(new EmptyTabObserver() {
            /** Keeps track of the original color before the preview was shown. */
            private int mOriginalColor;

            /** True if a change to the toolbar color was made because of a preview. */
            private boolean mTriggeredPreviewChange;

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                // Update the color when the page load finishes.
                updateColor(tab);
            }

            @Override
            public void didReloadLoFiImages(Tab tab) {
                // Update the color when the LoFi preview is reloaded.
                updateColor(tab);
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the color on every new URL.
                updateColor(tab);
            }

            /**
             * Updates the color of the Activity's status bar and the CCT Toolbar. When a preview is
             * shown, it should be reset to the default color. If the user later navigates away from
             * that preview to a non-preview page, reset the color back to the original. This does
             * not interfere with site-specific theme colors which are disabled when a preview is
             * being shown.
             */
            private void updateColor(Tab tab) {
                final ToolbarManager manager = getToolbarManager();
                if (manager == null) return;

                // Record the original toolbar color in case we need to revert back to it later
                // after a preview has been shown then the user navigates to another non-preview
                // page.
                if (mOriginalColor == 0) mOriginalColor = manager.getPrimaryColor();

                final boolean shouldUpdateOriginal = manager.getShouldUpdateToolbarPrimaryColor();
                manager.setShouldUpdateToolbarPrimaryColor(true);

                if (tab.isPreview()) {
                    final int defaultColor = ColorUtils.getDefaultThemeColor(getResources(), false);
                    manager.onThemeColorChanged(defaultColor, false);
                    mTriggeredPreviewChange = true;
                } else if (mOriginalColor != manager.getPrimaryColor() && mTriggeredPreviewChange) {
                    manager.onThemeColorChanged(mOriginalColor, false);

                    mTriggeredPreviewChange = false;
                    mOriginalColor = 0;
                }

                getStatusBarColorController().updateStatusBarColor(false);
                manager.setShouldUpdateToolbarPrimaryColor(shouldUpdateOriginal);
            }
        });
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        getTabModelSelector().onNativeLibraryReady(getTabContentManager());
        mBottomBarDelegate.addOverlayPanelManagerObserver();
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        BrowserSessionContentUtils.setActiveContentHandler(mBrowserSessionContentHandler);
        @TabCreationMode int mode = mTabProvider.getInitialTabCreationMode();
        boolean earlyCreatedTabIsReady =
                (mode == TabCreationMode.HIDDEN || mode == TabCreationMode.EARLY)
                && !mTabProvider.getTab().isLoading();
        if (earlyCreatedTabIsReady) postDeferredStartupIfNeeded();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        BrowserSessionContentUtils.removeActiveContentHandler(mBrowserSessionContentHandler);
    }

    @Override
    protected void onDestroyInternal() {
        super.onDestroyInternal();

        if (mIncognitoTabHost != null) {
            IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHost);
        }

        if (mTaskDescriptionHelper != null) mTaskDescriptionHelper.destroy();
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        if (getActivityTab() == null) return;
        getActivityTab().loadUrl(new LoadUrlParams(searchUrl));
    }

    @Override
    public TabModelSelectorImpl getTabModelSelector() {
        return (TabModelSelectorImpl) super.getTabModelSelector();
    }

    @Override
    @Nullable
    public Tab getActivityTab() {
        return mTabProvider.getTab();
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), mIntentDataProvider.getUiType(),
                mIntentDataProvider.getMenuTitles(), mIntentDataProvider.isOpenedByChrome(),
                mIntentDataProvider.shouldShowShareMenuItem(),
                mIntentDataProvider.shouldShowStarButton(),
                mIntentDataProvider.shouldShowDownloadButton(), mIntentDataProvider.isIncognito());
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getToolbarLayoutId() {
        return R.layout.custom_tabs_toolbar;
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage) return mIntentDataProvider.getClientPackageName();
        return super.getPackageName();
    }

    @Override
    public void finish() {
        super.finish();
        if (mIntentDataProvider != null && mIntentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(mIntentDataProvider.getAnimationEnterRes(),
                    mIntentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        } else if (mIntentDataProvider != null && mIntentDataProvider.isOpenedByChrome()) {
            overridePendingTransition(R.anim.no_anim, R.anim.activity_close_exit);
        }
    }

    /**
     * Internal implementation that finishes the activity and removes the references from Android
     * recents.
     */
    protected void handleFinishAndClose() {
        if (useSeparateTask()) {
            ApiCompatibilityUtils.finishAndRemoveTask(this);
        } else {
            finish();
        }
    }

    @Override
    protected boolean handleBackPressed() {
        return mNavigationController.navigateOnBack();
    }

    private void recordClientConnectionStatus() {
        String packageName =
                (getActivityTab() == null) ? null : TabAssociatedApp.getAppId(getActivityTab());
        if (packageName == null) return; // No associated package

        boolean isConnected =
                packageName.equals(mConnection.getClientPackageNameForSession(mSession));
        int status = -1;
        if (isConnected) {
            if (mIsKeepAlive) {
                status = ConnectionStatus.CONNECTED_KEEP_ALIVE;
            } else {
                status = ConnectionStatus.CONNECTED;
            }
        } else {
            if (mIsKeepAlive) {
                status = ConnectionStatus.DISCONNECTED_KEEP_ALIVE;
            } else {
                status = ConnectionStatus.DISCONNECTED;
            }
        }
        assert status >= 0;

        if (GSAState.isGsaPackageName(packageName)) {
            RecordHistogram.recordEnumeratedHistogram("CustomTabs.ConnectionStatusOnReturn.GSA",
                    status, ConnectionStatus.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram("CustomTabs.ConnectionStatusOnReturn.NonGSA",
                    status, ConnectionStatus.NUM_ENTRIES);
        }
    }

    /**
     * Configures the custom button on toolbar. Does nothing if invalid data is provided by clients.
     */
    private void showCustomButtonsOnToolbar() {
        final List<CustomButtonParams> paramList = mIntentDataProvider.getCustomButtonsOnToolbar();
        for (CustomButtonParams params : paramList) {
            getToolbarManager().addCustomActionButton(
                    params.getIcon(this), params.getDescription(), v -> {
                        if (getActivityTab() == null) return;
                        mIntentDataProvider.sendButtonPendingIntentWithUrlAndTitle(
                                ContextUtils.getApplicationContext(), params,
                                getActivityTab().getUrl(), getActivityTab().getTitle());
                        RecordUserAction.record("CustomTabsCustomActionButtonClick");
                        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()
                                && TextUtils.equals(
                                           params.getDescription(), getString(R.string.share))) {
                            RecordUserAction.record(
                                    "CustomTabsCustomActionButtonClick.DownloadsUI.Share");
                        }
                    });
        }
    }

    @Override
    public boolean canShowAppMenu() {
        if (getActivityTab() == null || !getToolbarManager().isInitialized()) return false;

        return super.canShowAppMenu();
    }

    @Override
    public boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        int menuIndex =
                CustomTabAppMenuPropertiesDelegate.getIndexOfMenuItemFromBundle(menuItemData);
        if (menuIndex >= 0) {
            mIntentDataProvider.clickMenuItemWithUrlAndTitle(
                    this, menuIndex, getActivityTab().getUrl(), getActivityTab().getTitle());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }

        return super.onOptionsItemSelected(itemId, menuItemData);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event, this,
                getToolbarManager().isInitialized());
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!getToolbarManager().isInitialized()) {
            return super.onKeyDown(keyCode, event);
        }
        return KeyboardShortcuts.onKeyDown(event, this, true, false)
                || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating new tabs, bookmark, history, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar || id == R.id.all_bookmarks_menu_id
                || id == R.id.help_id || id == R.id.recent_tabs_menu_id
                || id == R.id.new_incognito_tab_menu_id || id == R.id.new_tab_menu_id
                || id == R.id.open_history_menu_id) {
            return true;
        } else if (id == R.id.bookmark_this_page_id) {
            addOrEditBookmark(getActivityTab());
            RecordUserAction.record("MobileMenuAddToBookmarks");
            return true;
        } else if (id == R.id.open_in_browser_id) {
            if (mNavigationController.openCurrentUrlInBrowser(false)) {
                RecordUserAction.record("CustomTabsMenuOpenInChrome");
                mConnection.notifyOpenInBrowser(mSession);
            }
            return true;
        } else if (id == R.id.info_menu_id) {
            if (getTabModelSelector().getCurrentTab() == null) return false;
            PageInfoController.show(this, getTabModelSelector().getCurrentTab(),
                    getToolbarManager().getContentPublisher(),
                    PageInfoController.OpenedFromSource.MENU);
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    public int getBaseStatusBarColor() {
        return mCustomTabStatusBarColorProvider
                .getBaseStatusBarColor(super.getBaseStatusBarColor());
    }

    @Override
    public boolean isStatusBarDefaultThemeColor() {
        return mCustomTabStatusBarColorProvider
                .isStatusBarDefaultThemeColor(super.isStatusBarDefaultThemeColor());
    }

    @Override
    public void onUpdateStateChanged() {}

    /**
     * @return The {@link CustomTabIntentDataProvider} for this {@link CustomTabActivity}.
     */
    @VisibleForTesting
    public CustomTabIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();
        if (mIntentDataProvider.isMediaViewer()) {
            getToolbarManager().setToolbarShadowVisibility(View.GONE);
        }
    }

    @Override
    public boolean supportsAppMenu() {
        // The media viewer has no default menu items, so if there are also no custom items, we
        // should disable the menu altogether.
        if (mIntentDataProvider.isMediaViewer() && mIntentDataProvider.getMenuTitles().isEmpty()) {
            return false;
        }
        return super.supportsAppMenu();
    }

    /**
     * Show the web page with CustomTabActivity, without any navigation control.
     * Used in showing the terms of services page or help pages for Chrome.
     * @param context The current activity context.
     * @param url The url of the web page.
     */
    public static void showInfoPage(Context context, String url) {
        // TODO(xingliu): The title text will be the html document title, figure out if we want to
        // use Chrome strings here as EmbedContentViewActivity does.
        CustomTabsIntent customTabIntent = new CustomTabsIntent.Builder()
                .setShowTitle(true)
                .setToolbarColor(ApiCompatibilityUtils.getColor(
                        context.getResources(),
                        R.color.dark_action_bar_color))
                .build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                CustomTabIntentDataProvider.CustomTabsUiType.INFO_PAGE);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.addTrustedIntentExtras(intent);

        context.startActivity(intent);
    }

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // Custom Tabs can be used to open Chrome help pages before the ToS has been accepted.
        if (IntentHandler.notSecureIsIntentChromeOrFirstParty(intent)
                && IntentUtils.safeGetIntExtra(intent, CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                           CustomTabIntentDataProvider.CustomTabsUiType.DEFAULT)
                        == CustomTabIntentDataProvider.CustomTabsUiType.INFO_PAGE) {
            return false;
        }

        return super.requiresFirstRunToBeCompleted(intent);
    }

    @Override
    public boolean canShowTrustedCdnPublisherUrl() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)) {
            return false;
        }

        Tab tab = mTabProvider.getTab();
        if (tab != null && tab.isPreview()) {
            return false;
        }

        String publisherUrlPackage = mConnection.getTrustedCdnPublisherUrlPackage();
        return publisherUrlPackage != null
                && publisherUrlPackage.equals(mConnection.getClientPackageNameForSession(mSession));
    }

    private class IncognitoCustomTabHost implements IncognitoTabHost {

        public IncognitoCustomTabHost() {
            assert mIntentDataProvider.isIncognito();
        }

        @Override
        public boolean hasIncognitoTabs() {
            return !isFinishing();
        }

        @Override
        public void closeAllIncognitoTabs() {
            mNavigationController.finish(CustomTabActivityNavigationController.FinishReason.OTHER);
        }
    }

    @Override
    protected CustomTabActivityComponent createComponent(
            ChromeActivityCommonsModule commonsModule) {
        CustomTabActivityModule customTabsModule =
                new CustomTabActivityModule(mIntentDataProvider, mNightModeStateController);
        CustomTabActivityComponent component =
                ChromeApplication.getComponent().createCustomTabActivityComponent(
                        commonsModule, customTabsModule);

        mCustomTabStatusBarColorProvider = component.resolveCustomTabStatusBarColorProvider();
        mTabObserverRegistrar = component.resolveTabObserverRegistrar();
        mTabController = component.resolveTabController();
        mTabProvider = component.resolveTabProvider();
        mTabFactory = component.resolveTabFactory();
        component.resolveUmaTracker();
        mNavigationController = component.resolveNavigationController();
        mNavigationController.setFinishHandler((reason) -> {
            if (reason == USER_NAVIGATION) recordClientConnectionStatus();
            handleFinishAndClose();
        });
        component.resolveInitialPageLoader();

        if (mIntentDataProvider.isTrustedWebActivity()) {
            component.resolveTrustedWebActivityCoordinator();
        }
        if (mConnection.shouldHideTopBarOnModuleManagedUrlsForSession(
                    mIntentDataProvider.getSession())) {
            component.resolveDynamicModuleToolbarController();
        }

        return component;
    }

    @Override
    protected boolean shouldInitializeBottomSheet() {
        return isAutofillAssistantEnabled();
    }

    private boolean isAutofillAssistantEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT)
                && AutofillAssistantFacade.isConfigured(getInitialIntent().getExtras());
    }
}
