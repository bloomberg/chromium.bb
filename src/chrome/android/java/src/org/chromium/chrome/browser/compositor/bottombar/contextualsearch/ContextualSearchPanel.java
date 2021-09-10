// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.scene_layer.ContextualSearchSceneLayer;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

/**
 * Controls the Contextual Search Panel, primarily the Bar - the
 * {@link ContextualSearchBarControl} - and the content area that shows the Search Result.
 */
public class ContextualSearchPanel extends OverlayPanel implements ContextualSearchPanelInterface {
    /** Allows controls that appear in this panel to call back with requests or notifications. */
    interface ContextualSearchPanelSectionHost {
        /** Returns the current Y position of the panel section. */
        float getYPositionPx();

        /** Notifies the panel that the caller's section is changing its size. */
        void onPanelSectionSizeChange(boolean hasStarted);
    }

    /** The interface that the Opt-in promo uses to communicate with this Panel. */
    interface ContextualSearchPromoHost extends ContextualSearchPanelSectionHost {
        /**
         * Notifies that the user has opted in.
         * @param wasMandatory Whether the Promo was mandatory.
         */
        void onPromoOptIn(boolean wasMandatory);

        /**
         * Notifies that the user has opted out.
         */
        void onPromoOptOut();
    }

    /** The interface that the help section uses to communicate with this Panel. */
    interface ContextualSearchHelpSectionHost extends ContextualSearchPanelSectionHost {
        /** Returns whether the help section of the panel is enabled for the current user. */
        boolean isPanelHelpEnabled();

        /** Notifies that the user has clicked the OK button in the help section of the panel. */
        void onPanelHelpOkClicked();
    }

    /** The interface that the Related Searches section uses to communicate with this Panel. */
    interface RelatedSearchesSectionHost extends ContextualSearchPanelSectionHost {
        /**
         * Notifies that the user has clicked on a suggestions in this section of the panel.
         * @param suggestionIndex The 0-based index into the list of suggestions provided by the
         *        panel and presented in the UI. E.g. if the user clicked the second chit this value
         *        would be 1.
         */
        void onSuggestionClicked(int suggestionIndex);
    }

    /** Restricts the maximized panel height to the given fraction of a tab. */
    private static final float MAXIMIZED_HEIGHT_FRACTION = 0.95f;

    /** Used for logging state changes. */
    private final ContextualSearchPanelMetrics mPanelMetrics;

    /**
     * The {@link CompositorViewHolder}, used as an anchor view. Also injected into other classes.
     */
    private final CompositorViewHolder mCompositorViewHolder;

    /** The {@link WindowAndroid} for the current activity.  */
    private final WindowAndroid mWindowAndroid;

    /** Used to query toolbar state. */
    private final ToolbarManager mToolbarManager;

    /** The {@link ActivityType} for the current activity. */
    private final @ActivityType int mActivityType;

    /** Supplies the current {@link Tab} for the activity. */
    private final Supplier<Tab> mCurrentTabSupplier;

    /** The distance of the divider from the end of the bar, in dp. */
    private final float mEndButtonWidthDp;

    /** Whether the Panel should be promoted to a new tab after being maximized. */
    private boolean mShouldPromoteToTabAfterMaximizing;

    /** The object for handling global Contextual Search management duties */
    private ContextualSearchManagementDelegate mManagementDelegate;

    /** Whether the content view has been touched. */
    private boolean mHasContentBeenTouched;

    /** The compositor layer used for drawing the panel. */
    private ContextualSearchSceneLayer mSceneLayer;

    /**
     * A ScrimCoordinator for adjusting the Status Bar's brightness when a scrim is present (when
     * the panel is open).
     */
    private ScrimCoordinator mScrimCoordinator;

    /**
     * Params that configure our use of the ScrimCoordinator for adjusting the Status Bar's
     * brightness when a scrim is present (when the panel is open).
     */
    private PropertyModel mScrimProperties;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param layoutManager A layout manager for observing scene changes.
     * @param panelManager The object managing the how different panels are shown.
     * @param browserControlsStateProvider Used to measure the browser controls.
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param compositorViewHolder The {@link CompositorViewHolder} for the current activity.
     * @param toolbarHeightDp The height of the toolbar in dp.
     * @param toolbarManager The {@link ToolbarManager}, used to query for colors.
     * @param activityType The {@link ActivityType} for the current activity.
     * @param currentTabSupplier Supplies the current activity tab.
     */
    public ContextualSearchPanel(@NonNull Context context, @NonNull LayoutManagerImpl layoutManager,
            @NonNull OverlayPanelManager panelManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull WindowAndroid windowAndroid,
            @NonNull CompositorViewHolder compositorViewHolder, float toolbarHeightDp,
            @NonNull ToolbarManager toolbarManager, @ActivityType int activityType,
            @NonNull Supplier<Tab> currentTabSupplier) {
        super(context, layoutManager, panelManager, browserControlsStateProvider, windowAndroid,
                compositorViewHolder, toolbarHeightDp, currentTabSupplier);
        mSceneLayer = createNewContextualSearchSceneLayer();
        mPanelMetrics = new ContextualSearchPanelMetrics();
        mCompositorViewHolder = compositorViewHolder;
        mWindowAndroid = windowAndroid;
        mToolbarManager = toolbarManager;
        mActivityType = activityType;
        mCurrentTabSupplier = currentTabSupplier;

        mEndButtonWidthDp = mContext.getResources().getDimensionPixelSize(
                                    R.dimen.contextual_search_padded_button_width)
                * mPxToDp;
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContent(mManagementDelegate.getOverlayContentDelegate(),
                new PanelProgressObserver(), mActivity, /* isIncognito= */ false, getBarHeight(),
                mCompositorViewHolder, mWindowAndroid, mCurrentTabSupplier);
    }

    // ============================================================================================
    // Scene Overlay
    // ============================================================================================

    /**
     * Create a new scene layer for this panel. This should be overridden by tests as necessary.
     */
    protected ContextualSearchSceneLayer createNewContextualSearchSceneLayer() {
        return new ContextualSearchSceneLayer(mContext.getResources().getDisplayMetrics().density);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        super.getUpdatedSceneOverlayTree(viewport, visibleViewport, resourceManager, yOffset);
        mSceneLayer.update(resourceManager, this, getSearchBarControl(), getBarBannerControl(),
                getPromoControl(), getPanelHelp(), getRelatedSearchesControl(), getImageControl());

        return mSceneLayer;
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    /**
     * Sets the {@code ContextualSearchManagementDelegate} associated with this panel.
     * @param delegate The {@code ContextualSearchManagementDelegate}.
     */
    @Override
    public void setManagementDelegate(ContextualSearchManagementDelegate delegate) {
        if (mManagementDelegate != delegate) {
            mManagementDelegate = delegate;
            if (delegate != null) {
                setActivity(mManagementDelegate.getActivity());
            }
        }
    }

    /**
     * Notifies that the preference state has changed.
     * @param isEnabled Whether the feature is enabled.
     */
    @Override
    public void onContextualSearchPrefChanged(boolean isEnabled) {
        if (!isShowing()) return;

        getPromoControl().onContextualSearchPrefChanged(isEnabled);
    }

    // ============================================================================================
    // Panel State
    // ============================================================================================

    @Override
    public void setPanelState(@PanelState int toState, @StateChangeReason int reason) {
        @PanelState
        int fromState = getPanelState();

        mPanelMetrics.onPanelStateChanged(
                fromState, toState, reason, Profile.getLastUsedRegularProfile());

        if (toState == PanelState.PEEKED
                && (fromState == PanelState.CLOSED || fromState == PanelState.UNDEFINED)) {
            // If the Bar Banner is visible, it should animate when the SearchBar peeks.
            if (getBarBannerControl().isVisible()) {
                getBarBannerControl().animateAppearance();
            }
        }

        if ((fromState == PanelState.UNDEFINED || fromState == PanelState.CLOSED)
                && toState == PanelState.PEEKED) {
            mManagementDelegate.onPanelFinishedShowing();
        }

        super.setPanelState(toState, reason);
    }

    @Override
    protected boolean isSupportedState(@PanelState int state) {
        return canDisplayContentInPanel() || state != PanelState.MAXIMIZED;
    }

    @Override
    protected @PanelState int getProjectedState(float velocity) {
        @PanelState
        int projectedState = super.getProjectedState(velocity);

        // Prevent the fling gesture from moving the Panel from PEEKED to MAXIMIZED. This is to
        // make sure the Promo will be visible, considering that the EXPANDED state is the only
        // one that will show the Promo.
        if (getPromoControl().isVisible() && projectedState == PanelState.MAXIMIZED
                && getPanelState() == PanelState.PEEKED) {
            projectedState = PanelState.EXPANDED;
        }

        // If we're swiping the panel down from MAXIMIZED skip the EXPANDED state and go all the
        // way to PEEKED.
        if (getPanelState() == PanelState.MAXIMIZED && projectedState == PanelState.EXPANDED) {
            projectedState = PanelState.PEEKED;
        }

        return projectedState;
    }

    @Override
    public boolean onBackPressed() {
        if (!isShowing()) return false;
        mManagementDelegate.hideContextualSearch(StateChangeReason.BACK_PRESS);
        return true;
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    @Override
    protected void onClosed(@StateChangeReason int reason) {
        // Must be called before destroying Content because unseen visits should be removed from
        // history, and if the Content gets destroyed there won't be a Webcontents to do that.
        mManagementDelegate.onCloseContextualSearch(reason);

        setProgressBarCompletion(0);
        setProgressBarVisible(false);
        getImageControl().hideCustomImage(false);

        super.onClosed(reason);

        if (mSceneLayer != null) mSceneLayer.hideTree();
        if (mScrimCoordinator != null) mScrimCoordinator.hideScrim(false);
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    private boolean isCoordinateInsideActionTarget(float x) {
        if (LocalizationUtils.isLayoutRtl()) {
            return x >= getContentX() + mEndButtonWidthDp;
        } else {
            return x <= getContentX() + getWidth() - mEndButtonWidthDp;
        }
    }

    /**
     * Handles a bar click. The position is given in dp.
     */
    @Override
    public void handleBarClick(float x, float y) {
        getSearchBarControl().onSearchBarClick(x);

        if (isPeeking()) {
            if (getSearchBarControl().getQuickActionControl().hasQuickAction()
                    && isCoordinateInsideActionTarget(x)) {
                getSearchBarControl().getQuickActionControl().sendIntent(mCurrentTabSupplier.get());
            } else {
                // super takes care of expanding the Panel when peeking.
                super.handleBarClick(x, y);
            }
        } else if (isExpanded() || isMaximized()) {
            if (canPromoteToNewTab() && isCoordinateInsideOpenTabButton(x)) {
                mManagementDelegate.promoteToTab();
            } else {
                peekPanel(StateChangeReason.UNKNOWN);
            }
        }
    }

    @Override
    public boolean onInterceptBarClick() {
        return onInterceptOpeningPanel();
    }

    @Override
    public boolean onInterceptBarSwipe() {
        return onInterceptOpeningPanel();
    }

    /**
     * @return True if the event on the bar was intercepted.
     */
    private boolean onInterceptOpeningPanel() {
        if (mManagementDelegate.isRunningInCompatibilityMode()) {
            mManagementDelegate.openResolvedSearchUrlInNewTab();
            return true;
        }
        return false;
    }

    @Override
    public void onShowPress(float x, float y) {
        if (isCoordinateInsideBar(x, y)) getSearchBarControl().onShowPress(x);
        super.onShowPress(x, y);
    }

    // ============================================================================================
    // Panel base methods
    // ============================================================================================

    @Override
    protected void destroyComponents() {
        super.destroyComponents();
        destroyPromoControl();
        destroyPanelHelp();
        destroyRelatedSearchesControl();
        destroyBarBannerControl();
        destroySearchBarControl();
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        super.onActivityStateChange(activity, newState);
        if (newState == ActivityState.PAUSED) {
            mManagementDelegate.logCurrentState();
        }
    }

    @Override
    public @PanelPriority int getPriority() {
        return PanelPriority.HIGH;
    }

    @Override
    public boolean canBeSuppressed() {
        // The selected text on the page is lost when the panel is closed, thus, this panel cannot
        // be restored if it is suppressed.
        return false;
    }

    @Override
    public void notifyBarTouched(float x) {
        if (canDisplayContentInPanel()) {
            getOverlayPanelContent().showContent();
        }
    }

    @Override
    public float getOpenTabIconX() {
        if (LocalizationUtils.isLayoutRtl()) {
            return getOffsetX() + getBarMarginSide();
        } else {
            return getOffsetX() + getWidth() - getBarMarginSide() - getCloseIconDimension();
        }
    }

    @Override
    protected boolean isCoordinateInsideCloseButton(float x) {
        return false;
    }

    @Override
    protected boolean isCoordinateInsideOpenTabButton(float x) {
        return getOpenTabIconX() - getButtonPaddingDps() <= x
                && x <= getOpenTabIconX() + getOpenTabIconDimension() + getButtonPaddingDps();
    }

    @Override
    public float getContentY() {
        return getOffsetY() + getBarContainerHeight() + getPanelHelpHeight()
                + getRelatedSearchesHeight() + getPromoHeightPx() * mPxToDp;
    }

    @Override
    public float getBarContainerHeight() {
        return getBarHeight() + getBarBannerControl().getHeightPx();
    }

    @Override
    protected float getPeekedHeight() {
        return getBarHeight() + getBarBannerControl().getHeightPeekingPx() * mPxToDp;
    }

    @Override
    protected float getMaximizedHeight() {
        // Max height does not cover the entire content screen.
        return getTabHeight() * MAXIMIZED_HEIGHT_FRACTION;
    }

    // ============================================================================================
    // Animation Handling
    // ============================================================================================

    @Override
    protected void onHeightAnimationFinished() {
        super.onHeightAnimationFinished();

        if (mShouldPromoteToTabAfterMaximizing && getPanelState() == PanelState.MAXIMIZED) {
            mShouldPromoteToTabAfterMaximizing = false;
            mManagementDelegate.promoteToTab();
        }
    }

    // ============================================================================================
    // Contextual Search Panel API
    // ============================================================================================

    /**
     * Notify the panel that the content was seen.
     */
    @Override
    public void setWasSearchContentViewSeen() {
        mPanelMetrics.setWasSearchContentViewSeen();
    }

    /**
     * @param isActive Whether the promo is active.
     */
    @Override
    public void setIsPromoActive(boolean isActive, boolean isMandatory) {
        if (isActive) {
            getPromoControl().show(isMandatory);
        } else {
            getPromoControl().hide();
        }

        mPanelMetrics.setIsPromoActive(isActive);
    }

    @Override
    public void setIsPanelHelpActive(boolean isActive) {
        if (isActive) {
            getPanelHelp().show();
        } else {
            getPanelHelp().hide();
        }
    }

    @Override
    public void clearRelatedSearches() {
        getRelatedSearchesControl().hide();
    }

    /**
     * Shows the Bar Banner.
     */
    public void showBarBanner() {
        getBarBannerControl().show();
    }

    /**
     * Hides the Bar Banner.
     */
    public void hideBarBanner() {
        getBarBannerControl().hide();
    }

    /**
     * @return Whether the Bar Banner is visible.
     */
    @VisibleForTesting
    public boolean isBarBannerVisible() {
        return getBarBannerControl().isVisible();
    }

    /**
     * @return The visible Related Search suggestions.
     */
    @VisibleForTesting
    @Nullable
    public String[] getRelatedSearches() {
        return getRelatedSearchesControl().getRelatedSearchesSuggestions();
    }

    /**
     * Called after the panel has navigated to prefetched Search Results.
     * If the user has the panel open then they will see the prefetched result starting to load.
     * Currently this just logs the time between the start of the search until the results start to
     * render in the Panel.
     * @param didResolve Whether the search required the Search Term to be resolved.
     */
    @Override
    public void onPanelNavigatedToPrefetchedSearch(boolean didResolve) {
        mPanelMetrics.onPanelNavigatedToPrefetchedSearch(didResolve);
    }

    /**
     * Maximizes the Contextual Search Panel.
     * @param reason The {@code StateChangeReason} behind the maximization.
     */
    @Override
    public void maximizePanel(@StateChangeReason int reason) {
        mShouldPromoteToTabAfterMaximizing = false;
        super.maximizePanel(reason);
    }

    /**
     * Maximizes the Contextual Search Panel, then promotes it to a regular Tab.
     * @param reason The {@code StateChangeReason} behind the maximization and promotion to tab.
     */
    @Override
    public void maximizePanelThenPromoteToTab(@StateChangeReason int reason) {
        mShouldPromoteToTabAfterMaximizing = true;
        super.maximizePanel(reason);
    }

    @Override
    public void peekPanel(@StateChangeReason int reason) {
        super.peekPanel(reason);

        if (getPanelState() == PanelState.CLOSED || getPanelState() == PanelState.PEEKED) {
            mHasContentBeenTouched = false;
        }

        if ((getPanelState() == PanelState.UNDEFINED || getPanelState() == PanelState.CLOSED)
                && reason == StateChangeReason.TEXT_SELECT_TAP) {
            mPanelMetrics.onPanelTriggeredFromTap();
        }
    }

    @Override
    public void closePanel(@StateChangeReason int reason, boolean animate) {
        super.closePanel(reason, animate);
        mHasContentBeenTouched = false;
    }

    @Override
    public void expandPanel(@StateChangeReason int reason) {
        super.expandPanel(reason);
    }

    @Override
    public void requestPanelShow(@StateChangeReason int reason) {
        // If a re-tap is causing the panel to show when already shown, the superclass may ignore
        // that, but we want to be sure to capture search metrics for each tap.
        if (isShowing() && getPanelState() == PanelState.PEEKED) {
            peekPanel(reason);
        }
        super.requestPanelShow(reason);
    }

    /**
     * Gets whether a touch on the content view has been done yet or not.
     */
    @Override
    public boolean didTouchContent() {
        return mHasContentBeenTouched;
    }

    /**
     * Sets the search term to display in the SearchBar.
     * This should be called when the search term is set without search term resolution, or
     * after search term resolution completed.
     * @param searchTerm The string that represents the search term.
     */
    @Override
    public void setSearchTerm(String searchTerm) {
        getImageControl().hideCustomImage(true);
        getSearchBarControl().setSearchTerm(searchTerm);
        mPanelMetrics.onSearchRequestStarted();
        // Make sure the new Search Term draws.
        requestUpdate();
    }

    /**
     * Sets the search context details to display in the SearchBar.
     * @param selection The portion of the context that represents the user's selection.
     * @param end The portion of the context from the selection to its end.
     */
    @Override
    public void setContextDetails(String selection, String end) {
        getImageControl().hideCustomImage(true);
        getSearchBarControl().setContextDetails(selection, end);
        mPanelMetrics.onSearchRequestStarted();
        // Make sure the new Context draws.
        requestUpdate();
    }

    /**
     * Sets the caption to display in the SearchBar.
     * When the caption is displayed, the Search Term is pushed up and the caption shows below.
     * @param caption The string to show in as the caption.
     */
    @Override
    public void setCaption(String caption) {
        getSearchBarControl().setCaption(caption);
    }

    /** Ensures that we have a Caption to display in the SearchBar. */
    @Override
    public void ensureCaption() {
        if (getSearchBarControl().getCaptionVisible()) return;
        getSearchBarControl().setCaption(
                mContext.getResources().getString(R.string.contextual_search_default_caption));
    }

    /**
     * Handles showing the resolved search term in the SearchBar.
     * @param searchTerm The string that represents the search term.
     * @param thumbnailUrl The URL of the thumbnail to display.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@code QuickActionCategory} for the quick action.
     * @param cardTagEnum The {@link CardTag} that the server returned if there was a card,
     *        or {@code 0}.
     * @param relatedSearches Suggested searches to be displayed in the Panel.
     */
    @Override
    public void onSearchTermResolved(String searchTerm, String thumbnailUrl, String quickActionUri,
            int quickActionCategory, @CardTag int cardTagEnum, @Nullable String[] relatedSearches) {
        getRelatedSearchesControl().setRelatedSearchesSuggestions(relatedSearches);
        mPanelMetrics.onSearchTermResolved();
        if (cardTagEnum == CardTag.CT_DEFINITION
                || cardTagEnum == CardTag.CT_CONTEXTUAL_DEFINITION) {
            getSearchBarControl().updateForDictionaryDefinition(searchTerm, cardTagEnum);
            return;
        }

        getSearchBarControl().setSearchTerm(searchTerm);
        getSearchBarControl().animateSearchTermResolution();
        if (mActivity == null || mToolbarManager == null) return;

        getSearchBarControl().setQuickAction(
                quickActionUri, quickActionCategory, mToolbarManager.getPrimaryColor());
        getImageControl().setThumbnailUrl(thumbnailUrl);
    }

    /**
     * Calculates the position of the Contextual Search panel on the screen.
     * @return A {@link Rect} object that represents the Contextual Search panel's position in
     *         the screen, in pixels.
     */
    @Override
    public Rect getPanelRect() {
        int[] contentLocationInWindow = new int[2];
        mActivity.findViewById(android.R.id.content).getLocationInWindow(contentLocationInWindow);
        int leftPadding = contentLocationInWindow[0];
        int topPadding = contentLocationInWindow[1];

        // getOffsetX() and getOffsetY() return the position of the panel relative to the activity,
        // therefore leftPadding and topPadding are added to get the position in the screen.
        int left = (int) (getOffsetX() / mPxToDp) + leftPadding;
        int top = (int) (getOffsetY() / mPxToDp) + topPadding;
        int bottom = top + (int) (getBarHeight() / mPxToDp);
        int right = left + (int) (getWidth() / mPxToDp);

        return new Rect(left, top, right, bottom);
    }

    /** @return The padding used for each side of the button in the Bar. */
    public float getButtonPaddingDps() {
        return mButtonPaddingDps;
    }

    // ============================================================================================
    // Panel Metrics
    // ============================================================================================

    // TODO(pedrosimonetti): replace proxy methods with direct PanelMetrics usage

    /**
     * @return The {@link ContextualSearchPanelMetrics}.
     */
    @Override
    public ContextualSearchPanelMetrics getPanelMetrics() {
        return mPanelMetrics;
    }

    /**
     * Sets that the contextual search involved the promo.
     */
    @Override
    public void setDidSearchInvolvePromo() {
        mPanelMetrics.setDidSearchInvolvePromo();
    }

    // ============================================================================================
    // Panel Rendering
    // ============================================================================================

    // TODO(pedrosimonetti): generalize the dispatching of panel updates.

    @Override
    protected void updatePanelForCloseOrPeek(float percentage) {
        super.updatePanelForCloseOrPeek(percentage);

        getPromoControl().onUpdateFromCloseToPeek(percentage);
        getPanelHelp().onUpdateFromCloseToPeek(percentage);
        getRelatedSearchesControl().onUpdateFromCloseToPeek(percentage);
        getBarBannerControl().onUpdateFromCloseToPeek(percentage);
        getSearchBarControl().onUpdateFromCloseToPeek(percentage);
    }

    @Override
    protected void updatePanelForExpansion(float percentage) {
        super.updatePanelForExpansion(percentage);

        getPromoControl().onUpdateFromPeekToExpand(percentage);
        getPanelHelp().onUpdateFromPeekToExpand(percentage);
        getRelatedSearchesControl().onUpdateFromPeekToExpand(percentage);
        getBarBannerControl().onUpdateFromPeekToExpand(percentage);
        getSearchBarControl().onUpdateFromPeekToExpand(percentage);
    }

    @Override
    protected void updatePanelForMaximization(float percentage) {
        super.updatePanelForMaximization(percentage);

        getPromoControl().onUpdateFromExpandToMaximize(percentage);
        getPanelHelp().onUpdateFromExpandToMaximize(percentage);
        getRelatedSearchesControl().onUpdateFromExpandToMaximize(percentage);
        getBarBannerControl().onUpdateFromExpandToMaximize(percentage);
    }

    @Override
    protected void updatePanelForSizeChange() {
        if (getPromoControl().isVisible()) {
            getPromoControl().invalidate(true);
        }
        if (getPanelHelp().isVisible()) {
            getPanelHelp().invalidate(true);
        }
        if (getRelatedSearchesControl().isVisible()) {
            getRelatedSearchesControl().invalidate(true);
        }
        if (getBarBannerControl().isVisible()) {
            getBarBannerControl().onResized(this);
        }

        // NOTE(pedrosimonetti): We cannot tell where the selection will be after the
        // orientation change, so we are setting the selection position to zero, which
        // means the base page will be positioned in its original state and we won't
        // try to keep the selection in view.
        updateBasePageSelectionYPx(0.f);
        updateBasePageTargetY();

        super.updatePanelForSizeChange();

        mManagementDelegate.onPanelResized();
    }

    @Override
    protected void updateStatusBar() {
        float maxBrightness = getMaxBasePageBrightness();
        float minBrightness = getMinBasePageBrightness();
        float basePageBrightness = getBasePageBrightness();
        // Compute Status Bar alpha based on the base-page brightness range applied by the Overlay.
        // TODO(donnd): Create a full-screen sized view and apply the black_alpha_65 color to get
        // an exact match between the scrim and the status bar colors instead of adjusting the
        // status bar alpha to approximate the native overlay brightness filter.
        // Details in https://crbug.com/848922.
        float statusBarAlpha =
                (maxBrightness - basePageBrightness) / (maxBrightness - minBrightness);
        if (statusBarAlpha == 0.0) {
            if (mScrimCoordinator != null) mScrimCoordinator.hideScrim(false);
            mScrimProperties = null;
            mScrimCoordinator = null;
            return;

        } else {
            mScrimCoordinator = mManagementDelegate.getScrimCoordinator();
            if (mScrimProperties == null) {
                mScrimProperties =
                        new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                                .with(ScrimProperties.TOP_MARGIN, 0)
                                .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                                .with(ScrimProperties.ANCHOR_VIEW, mCompositorViewHolder)
                                .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                                .with(ScrimProperties.VISIBILITY_CALLBACK, null)
                                .with(ScrimProperties.CLICK_DELEGATE, null)
                                .build();
                mScrimCoordinator.showScrim(mScrimProperties);
            }
            mScrimCoordinator.setAlpha(statusBarAlpha);
        }
    }

    // ============================================================================================
    // Selection position
    // ============================================================================================

    /** The approximate Y coordinate of the selection in pixels. */
    private float mBasePageSelectionYPx = -1.f;

    /**
     * Updates the coordinate of the existing selection.
     * @param y The y coordinate of the selection in pixels.
     */
    @Override
    public void updateBasePageSelectionYPx(float y) {
        mBasePageSelectionYPx = y;
    }

    @Override
    protected float calculateBasePageDesiredOffset() {
        float offset = 0.f;
        if (mBasePageSelectionYPx > 0.f) {
            // Convert from px to dp.
            final float selectionY = mBasePageSelectionYPx * mPxToDp;

            // Calculate the offset to center the selection on the available area.
            final float availableHeight = getTabHeight() - getExpandedHeight();
            offset = -selectionY + availableHeight / 2;
            offset += getLayoutOffsetYDps();
        }
        return offset;
    }

    // ============================================================================================
    // ContextualSearchBarControl
    // ============================================================================================

    private ContextualSearchBarControl mSearchBarControl;

    /**
     * Creates the ContextualSearchBarControl, if needed. The Views are set to INVISIBLE, because
     * they won't actually be displayed on the screen (their snapshots will be displayed instead).
     */
    @Override
    public ContextualSearchBarControl getSearchBarControl() {
        if (mSearchBarControl == null) {
            mSearchBarControl =
                    new ContextualSearchBarControl(this, mContext, mContainerView, mResourceLoader);
        }
        return mSearchBarControl;
    }

    /**
     * Destroys the ContextualSearchBarControl.
     */
    protected void destroySearchBarControl() {
        if (mSearchBarControl != null) {
            mSearchBarControl.destroy();
            mSearchBarControl = null;
        }
    }

    // ============================================================================================
    // Image Control
    // ============================================================================================
    /**
     * @return The {@link ContextualSearchImageControl} for the panel.
     */
    public ContextualSearchImageControl getImageControl() {
        return getSearchBarControl().getImageControl();
    }

    // ============================================================================================
    // Bar Banner
    // ============================================================================================

    private ContextualSearchBarBannerControl mBarBannerControl;

    /**
     * Creates the ContextualSearchBarBannerControl, if needed.
     */
    private ContextualSearchBarBannerControl getBarBannerControl() {
        if (mBarBannerControl == null) {
            mBarBannerControl = new ContextualSearchBarBannerControl(
                    this, mContext, mContainerView, mResourceLoader);
        }
        return mBarBannerControl;
    }

    /**
     * Destroys the ContextualSearchBarBannerControl.
     */
    private void destroyBarBannerControl() {
        if (mBarBannerControl != null) {
            mBarBannerControl.destroy();
            mBarBannerControl = null;
        }
    }

    // ============================================================================================
    // Promo
    // ============================================================================================

    private ContextualSearchPromoControl mPromoControl;
    private ContextualSearchPromoHost mPromoHost;

    /**
     * @return Whether the Promo reached a state in which it could be interacted.
     */
    @Override
    public boolean wasPromoInteractive() {
        return getPromoControl().wasInteractive();
    }

    /**
     * @return Height of the promo in pixels.
     */
    private float getPromoHeightPx() {
        return getPromoControl().getHeightPx();
    }

    /**
     * Creates the ContextualSearchPromoControl, if needed.
     */
    private ContextualSearchPromoControl getPromoControl() {
        if (mPromoControl == null) {
            mPromoControl =
                    new ContextualSearchPromoControl(this, getContextualSearchPromoHost(),
                            mContext, mContainerView, mResourceLoader);
        }
        return mPromoControl;
    }

    /**
     * Destroys the ContextualSearchPromoControl.
     */
    private void destroyPromoControl() {
        if (mPromoControl != null) {
            mPromoControl.destroy();
            mPromoControl = null;
        }
    }

    /**
     * @return An implementation of {@link ContextualSearchPromoHost}.
     */
    private ContextualSearchPromoHost getContextualSearchPromoHost() {
        if (mPromoHost == null) {
            // Create a handler for callbacks from the Opt-in promo.
            mPromoHost = new ContextualSearchPromoHost() {
                @Override
                public float getYPositionPx() {
                    // Needs to enumerate anything that can appear above it in the panel.
                    return Math.round((getOffsetY() + getBarContainerHeight()
                                              + getRelatedSearchesHeight() + getPanelHelpHeight())
                            / mPxToDp);
                }

                @Override
                public void onPanelSectionSizeChange(boolean hasStarted) {
                    // The promo section is causing movement, but since there's nothing
                    // below it we don't need to do anything.
                }

                @Override
                public void onPromoOptIn(boolean wasMandatory) {
                    if (wasMandatory) {
                        getOverlayPanelContent().showContent();
                        expandPanel(StateChangeReason.OPTIN);
                    }
                    mManagementDelegate.onPromoOptIn();
                }

                @Override
                public void onPromoOptOut() {
                    closePanel(OverlayPanel.StateChangeReason.OPTOUT, true);
                }
            };
        }

        return mPromoHost;
    }

    // ============================================================================================
    // In-Panel Help
    // ============================================================================================

    /** The Help section of the Panel. */
    private ContextualSearchPanelHelp mPanelHelp;
    private ContextualSearchHelpSectionHost mHelpSectionHost;

    /**
     * Returns the {@link ContextualSearchPanelHelp} that controls the help section of this
     * panel.
     */
    private ContextualSearchPanelHelp getPanelHelp() {
        if (mPanelHelp == null) {
            mPanelHelp = new ContextualSearchPanelHelp(this, getContextualSearchHelpSectionHost(),
                    mContext, mContainerView, mResourceLoader);
        }
        return mPanelHelp;
    }

    /**
     * @return Height of the help section of the panel in DPs.
     */
    private float getPanelHelpHeight() {
        return getPanelHelp().getHeightPx() * mPxToDp;
    }

    /** Destroys the Help section of this panel. */
    private void destroyPanelHelp() {
        mPanelHelp.destroy();
        mPanelHelp = null;
    }

    /**
     * @return An implementation of {@link ContextualSearchHelpSectionHost}.
     */
    private ContextualSearchHelpSectionHost getContextualSearchHelpSectionHost() {
        if (mHelpSectionHost == null) {
            mHelpSectionHost = new ContextualSearchHelpSectionHost() {
                @Override
                public float getYPositionPx() {
                    // Needs to enumerate anything that can appear above it in the panel.
                    return Math.round(
                            (getOffsetY() + getBarContainerHeight() + getRelatedSearchesHeight())
                            / mPxToDp);
                }

                @Override
                public void onPanelSectionSizeChange(boolean hasStarted) {
                    // The help section is causing movement which means the promo below
                    // it will move.
                    getPromoControl().onUpdateForMovement(hasStarted);
                }

                @Override
                public boolean isPanelHelpEnabled() {
                    return mManagementDelegate.isPanelHelpEnabled();
                }

                @Override
                public void onPanelHelpOkClicked() {
                    // Tell the manager the user said OK.
                    mManagementDelegate.onPanelHelpOkClicked();
                }
            };
        }
        return mHelpSectionHost;
    }

    // ============================================================================================
    // Related Searches Control Panel-Section
    // ============================================================================================

    private RelatedSearchesControl mRelatedSearchesControl;
    private RelatedSearchesSectionHost mRelatedSearchesSectionHost;

    /**
     * Creates the RelatedSearchesControl, if needed.
     */
    private RelatedSearchesControl getRelatedSearchesControl() {
        if (mRelatedSearchesControl == null) {
            mRelatedSearchesControl = new RelatedSearchesControl(this,
                    getRelatedSearchesSectionHost(), mContext, mContainerView, mResourceLoader);
        }
        return mRelatedSearchesControl;
    }

    /**
     * @return Height of this section of the panel in DPs.
     */
    private float getRelatedSearchesHeight() {
        return getRelatedSearchesControl().getHeightPx() * mPxToDp;
    }

    /**
     * Destroys the RelatedSearchesControl.
     */
    private void destroyRelatedSearchesControl() {
        if (mRelatedSearchesControl != null) {
            mRelatedSearchesControl.destroy();
            mRelatedSearchesControl = null;
        }
    }

    /**
     * @return An implementation of {@link RelatedSearchesSectionHost}.
     */
    private RelatedSearchesSectionHost getRelatedSearchesSectionHost() {
        if (mRelatedSearchesSectionHost == null) {
            mRelatedSearchesSectionHost = new RelatedSearchesSectionHost() {
                @Override
                public float getYPositionPx() {
                    // Nothing can appear above it in the panel.
                    return Math.round((getOffsetY() + getBarContainerHeight()) / mPxToDp);
                }

                @Override
                public void onPanelSectionSizeChange(boolean hasStarted) {
                    // This section currently doesn't change size, so we can ignore this.
                }

                @Override
                public void onSuggestionClicked(int selectionIndex) {
                    mManagementDelegate.onRelatedSearchesSuggestionClicked(selectionIndex);
                }
            };
        }
        return mRelatedSearchesSectionHost;
    }

    // ============================================================================================
    // Panel Content
    // ============================================================================================

    /**
     * @return Whether the content can be displayed in the panel.
     */
    public boolean canDisplayContentInPanel() {
        return !getPromoControl().isMandatory();
    }

    @Override
    public void onTouchSearchContentViewAck() {
        mHasContentBeenTouched = true;
    }

    /**
     * Destroy the current content in the panel.
     * NOTE(mdjones): This should not be exposed. The only use is in ContextualSearchManager for a
     * bug related to loading new panel content.
     */
    @Override
    public void destroyContent() {
        super.destroyOverlayPanelContent();
    }

    /**
     * @return Whether the panel content can be displayed in a new tab.
     */
    public boolean canPromoteToNewTab() {
        return mActivityType == ActivityType.TABBED && canDisplayContentInPanel();
    }

    // ============================================================================================
    // Testing Support
    // ============================================================================================

    /**
     * Simulates a tap on the panel's end button.
     */
    @VisibleForTesting
    public void simulateTapOnEndButton() {
        endHeightAnimation();

        // Determine the x-position for the simulated tap.
        float xPosition;
        if (LocalizationUtils.isLayoutRtl()) {
            xPosition = getContentX() + (mEndButtonWidthDp / 2);
        } else {
            xPosition = getContentX() + getWidth() - (mEndButtonWidthDp / 2);
        }

        // Determine the y-position for the simulated tap.
        float yPosition = getOffsetY() + (getHeight() / 2);

        // Simulate the tap.
        handleClick(xPosition, yPosition);
    }
}
