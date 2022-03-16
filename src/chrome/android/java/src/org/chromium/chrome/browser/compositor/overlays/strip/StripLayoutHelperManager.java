// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.CompositorOnClickHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaGestureEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.GestureHandler;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.url.GURL;

import java.util.List;

/**
 * This class handles managing which {@link StripLayoutHelper} is currently active and dispatches
 * all input and model events to the proper destination.
 */
public class StripLayoutHelperManager implements SceneOverlay {
    private static final long FADE_SCRIM_DURATION_MS = 200;
    // Caching Variables
    private final RectF mStripFilterArea = new RectF();

    // Model selector buttons constants.
    private static final float MODEL_SELECTOR_BUTTON_Y_OFFSET_DP = 10.f;
    private static final float MODEL_SELECTOR_BUTTON_END_PADDING_DP = 6.f;
    private static final float MODEL_SELECTOR_BUTTON_START_PADDING_DP = 3.f;
    private static final float MODEL_SELECTOR_BUTTON_WIDTH_DP = 24.f;
    private static final float MODEL_SELECTOR_BUTTON_HEIGHT_DP = 24.f;

    // External influences
    private TabModelSelector mTabModelSelector;
    private final LayoutUpdateHost mUpdateHost;

    // Event Filters
    private final AreaGestureEventFilter mEventFilter;

    // Internal state
    private boolean mIsIncognito;
    private final StripLayoutHelper mNormalHelper;
    private final StripLayoutHelper mIncognitoHelper;

    // UI State
    private float mWidth;  // in dp units
    private final float mHeight;  // in dp units
    private int mOrientation;
    private final CompositorButton mModelSelectorButton;

    private final StripScrim mStripScrim;
    private ValueAnimator mScrimFadeAnimation;

    private TabStripSceneLayer mTabStripTreeProvider;

    private TabStripEventHandler mTabStripEventHandler;
    private TabSwitcherLayoutObserver mTabSwitcherLayoutObserver;

    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver =
            new TabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    tabModelSwitched(newModel.isIncognito());
                }
            };

    private TabModelObserver mTabModelObserver;

    private final String mDefaultTitle;
    private final Supplier<LayerTitleCache> mLayerTitleCacheSupplier;

    private class TabStripEventHandler implements GestureHandler {
        @Override
        public void onDown(float x, float y, boolean fromMouse, int buttons) {
            if (mModelSelectorButton.onDown(x, y)) return;
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().onDown(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void onUpOrCancel() {
            if (mModelSelectorButton.onUpOrCancel() && mTabModelSelector != null) {
                getActiveStripLayoutHelper().finishAnimation();
                if (!mModelSelectorButton.isVisible()) return;
                mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
                return;
            }
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().onUpOrCancel(time());
        }

        @Override
        public void drag(float x, float y, float dx, float dy, float tx, float ty) {
            mModelSelectorButton.drag(x, y);
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().drag(time(), x, y, dx, dy, tx, ty);
        }

        @Override
        public void click(float x, float y, boolean fromMouse, int buttons) {
            long time = time();
            if (mModelSelectorButton.click(x, y)) {
                mModelSelectorButton.handleClick(time);
                return;
            }
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().click(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void fling(float x, float y, float velocityX, float velocityY) {
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().fling(time(), x, y, velocityX, velocityY);
        }

        @Override
        public void onLongPress(float x, float y) {
            if (mStripScrim.isVisible()) return;
            getActiveStripLayoutHelper().onLongPress(time(), x, y);
        }

        @Override
        public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {
            // Not implemented.
        }

        private long time() {
            return LayoutManagerImpl.time();
        }
    }

    /**
     * Observer for Tab Switcher layout events.
     */
    class TabSwitcherLayoutObserver implements LayoutStateObserver {
        @Override
        public void onStartedShowing(@LayoutType int layoutType, boolean showToolbar) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            updateScrimVisibility(true);
        }

        @Override
        public void onStartedHiding(
                @LayoutType int layoutType, boolean showToolbar, boolean delayAnimation) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            updateScrimVisibility(false);
        }

        private void updateScrimVisibility(boolean visibility) {
            if (!isGridTabSwitcherEnabled()) return;

            if (mScrimFadeAnimation != null) {
                mScrimFadeAnimation.cancel();
            }

            float startAlpha = visibility ? 0.f : 1.f;
            float endAlpha = visibility ? 1.f : 0.f;
            mScrimFadeAnimation = ValueAnimator.ofFloat(startAlpha, endAlpha);
            mScrimFadeAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            mScrimFadeAnimation.setDuration(FADE_SCRIM_DURATION_MS);
            mScrimFadeAnimation.addUpdateListener((anim -> {
                final float currentAlpha = (float) anim.getAnimatedValue();
                mStripScrim.setAlpha(currentAlpha);
                mTabStripTreeProvider.updateStripScrim(mStripScrim);
            }));
            mScrimFadeAnimation.start();
            mStripScrim.setVisible(visibility);
        }
    }

    /**
     * @return Returns layout observer for tab switcher.
     */
    public TabSwitcherLayoutObserver getTabSwitcherObserver() {
        return mTabSwitcherLayoutObserver;
    }

    /**
     * Creates an instance of the {@link StripLayoutHelperManager}.
     * @param context The current Android {@link Context}.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The {@link LayoutRenderHost}.
     * @param titleCacheSupplier A supplier of the title cache.
     * @param layerTitleCacheSupplier A supplier of the cache that holds the title textures.
     */
    public StripLayoutHelperManager(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, Supplier<LayerTitleCache> layerTitleCacheSupplier) {
        mUpdateHost = updateHost;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mTabStripTreeProvider = new TabStripSceneLayer(context);
        mTabStripEventHandler = new TabStripEventHandler();
        mTabSwitcherLayoutObserver = new TabSwitcherLayoutObserver();
        mDefaultTitle = context.getString(R.string.tab_loading_default_title);
        mEventFilter =
                new AreaGestureEventFilter(context, mTabStripEventHandler, null, false, false);

        mNormalHelper = new StripLayoutHelper(context, updateHost, renderHost, false);
        mIncognitoHelper = new StripLayoutHelper(context, updateHost, renderHost, true);

        CompositorOnClickHandler selectorClickHandler = new CompositorOnClickHandler() {
            @Override
            public void onClick(long time) {
                handleModelSelectorButtonClick();
            }
        };
        mModelSelectorButton = new CompositorButton(context, MODEL_SELECTOR_BUTTON_WIDTH_DP,
                MODEL_SELECTOR_BUTTON_HEIGHT_DP, selectorClickHandler);
        mModelSelectorButton.setIncognito(false);
        mModelSelectorButton.setVisible(false);
        // Pressed resources are the same as the unpressed resources.
        mModelSelectorButton.setResources(R.drawable.btn_tabstrip_switch_normal,
                R.drawable.btn_tabstrip_switch_normal, R.drawable.location_bar_incognito_badge,
                R.drawable.location_bar_incognito_badge);
        mModelSelectorButton.setY(MODEL_SELECTOR_BUTTON_Y_OFFSET_DP);

        Resources res = context.getResources();
        mHeight = res.getDimension(R.dimen.tab_strip_height) / res.getDisplayMetrics().density;
        mModelSelectorButton.setAccessibilityDescription(
                res.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_standard),
                res.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_incognito));

        mStripScrim = new StripScrim(res, mWidth, mHeight);
        mStripScrim.setVisible(false);

        onContextChanged(context);
    }

    /**
     * @return Return scrim to be applied on tab strip.
     */
    public StripScrim getStripScrim() {
        return mStripScrim;
    }

    /**
     * Cleans up internal state.
     */
    public void destroy() {
        mTabStripTreeProvider.destroy();
        mTabStripTreeProvider = null;
        mIncognitoHelper.destroy();
        mNormalHelper.destroy();
        if (mTabModelSelector != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);

            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabObserver.destroy();
        }
    }

    private void handleModelSelectorButtonClick() {
        if (mTabModelSelector == null) return;
        getActiveStripLayoutHelper().finishAnimation();
        if (!mModelSelectorButton.isVisible()) return;
        mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
    }

    @VisibleForTesting
    public void simulateClick(float x, float y, boolean fromMouse, int buttons) {
        mTabStripEventHandler.click(x, y, fromMouse, buttons);
    }

    @VisibleForTesting
    public void simulateLongPress(float x, float y) {
        mTabStripEventHandler.onLongPress(x, y);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        assert mTabStripTreeProvider != null;

        Tab selectedTab = mTabModelSelector.getCurrentModel().getTabAt(
                mTabModelSelector.getCurrentModel().index());
        int selectedTabId = selectedTab == null ? TabModel.INVALID_TAB_INDEX : selectedTab.getId();
        mTabStripTreeProvider.pushAndUpdateStrip(this, mLayerTitleCacheSupplier.get(),
                resourceManager, getActiveStripLayoutHelper().getStripLayoutTabsToRender(), yOffset,
                selectedTabId);
        return mTabStripTreeProvider;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        // TODO(mdjones): This matches existing behavior but can be improved to return false if
        // the browser controls offset is equal to the browser controls height.
        return true;
    }

    @Override
    public EventFilter getEventFilter() {
        return mEventFilter;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        mWidth = width;
        mOrientation = orientation;
        if (!LocalizationUtils.isLayoutRtl()) {
            mModelSelectorButton.setX(
                    mWidth - MODEL_SELECTOR_BUTTON_WIDTH_DP - MODEL_SELECTOR_BUTTON_END_PADDING_DP);
        } else {
            mModelSelectorButton.setX(MODEL_SELECTOR_BUTTON_END_PADDING_DP);
        }

        updateStripScrim();

        mNormalHelper.onSizeChanged(mWidth, mHeight);
        mIncognitoHelper.onSizeChanged(mWidth, mHeight);

        mStripFilterArea.set(0, 0, mWidth, Math.min(getHeight(), visibleViewportOffsetY));
        mEventFilter.setEventArea(mStripFilterArea);
    }

    private void updateStripScrim() {
        if (!isGridTabSwitcherEnabled()) return;
        // Update width
        float scrimWidth = mModelSelectorButton.isVisible()
                ? mWidth - getModelSelectorButtonWidthWithPadding()
                : mWidth;
        mStripScrim.setWidth(scrimWidth);

        // Update drawX
        float drawX = 0;
        if (LocalizationUtils.isLayoutRtl() && mModelSelectorButton.isVisible()) {
            drawX = getModelSelectorButtonWidthWithPadding();
        }
        mStripScrim.setX(drawX);
    }

    private float getModelSelectorButtonWidthWithPadding() {
        return MODEL_SELECTOR_BUTTON_WIDTH_DP + MODEL_SELECTOR_BUTTON_END_PADDING_DP
                + MODEL_SELECTOR_BUTTON_START_PADDING_DP;
    }

    public TintedCompositorButton getNewTabButton() {
        return getActiveStripLayoutHelper().getNewTabButton();
    }

    public CompositorButton getModelSelectorButton() {
        return mModelSelectorButton;
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {
        if (mModelSelectorButton.isVisible()) views.add(mModelSelectorButton);
        if (!getStripScrim().isVisible()) getActiveStripLayoutHelper().getVirtualViews(views);
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    /**
     * @return The opacity to use for the fade on the left side of the tab strip.
     */
    public float getLeftFadeOpacity() {
        return getActiveStripLayoutHelper().getLeftFadeOpacity();
    }

    /**
     * @return The opacity to use for the fade on the right side of the tab strip.
     */
    public float getRightFadeOpacity() {
        return getActiveStripLayoutHelper().getRightFadeOpacity();
    }

    /**
     * @return The brightness of background tabs in the tabstrip.
     */
    public float getBackgroundTabBrightness() {
        return getActiveStripLayoutHelper().getBackgroundTabBrightness();
    }

    /**
     * @return The brightness of the entire tabstrip.
     */
    public float getBrightness() {
        return getActiveStripLayoutHelper().getBrightness();
    }

    /** Update the title cache for the available tabs in the model. */
    private void updateTitleCacheForInit() {
        LayerTitleCache titleCache = mLayerTitleCacheSupplier.get();
        if (mTabModelSelector == null || titleCache == null) return;

        // Make sure any tabs already restored get loaded into the title cache.
        List<TabModel> models = mTabModelSelector.getModels();
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (int j = 0; j < model.getCount(); j++) {
                Tab tab = model.getTabAt(j);
                if (tab != null) {
                    titleCache.getUpdatedTitle(
                            tab, tab.getContext().getString(R.string.tab_loading_default_title));
                }
            }
        }
    }

    /**
     * Sets the {@link TabModelSelector} that this {@link StripLayoutHelperManager} will visually
     * represent, and various objects associated with it.
     * @param modelSelector The {@link TabModelSelector} to visually represent.
     * @param tabCreatorManager The {@link TabCreatorManager}, used to create new tabs.
     */
    public void setTabModelSelector(TabModelSelector modelSelector,
            TabCreatorManager tabCreatorManager) {
        if (mTabModelSelector == modelSelector) return;

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int launchType, @TabCreationState int creationState) {
                updateTitleForTab(tab);
            }
        };
        modelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        mTabModelSelector = modelSelector;

        updateTitleCacheForInit();

        if (mTabModelSelector.isTabStateInitialized()) {
            updateModelSwitcherButton();
        } else {
            mTabModelSelector.addObserver(new TabModelSelectorObserver() {
                @Override
                public void onTabStateInitialized() {
                    updateModelSwitcherButton();
                    new Handler().post(() -> mTabModelSelector.removeObserver(this));
                }
            });
        }

        mNormalHelper.setTabModel(mTabModelSelector.getModel(false),
                tabCreatorManager.getTabCreator(false));
        mIncognitoHelper.setTabModel(mTabModelSelector.getModel(true),
                tabCreatorManager.getTabCreator(true));
        tabModelSwitched(mTabModelSelector.isIncognitoSelected());

        mTabModelSelectorTabModelObserver = new TabModelSelectorTabModelObserver(modelSelector) {
            /**
             * @return The actual current time of the app in ms.
             */
            public long time() {
                return SystemClock.uptimeMillis();
            }

            @Override
            public void tabRemoved(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                // For right-direction move, layout helper re-ordering logic
                // expects destination index = position + 1
                getStripLayoutHelper(tab.isIncognito())
                        .tabMoved(time(), tab.getId(), curIndex,
                                newIndex > curIndex ? newIndex + 1 : newIndex);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosureCancelled(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                if (mLayerTitleCacheSupplier.hasValue()) {
                    mLayerTitleCacheSupplier.get().remove(tab.getId());
                }
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                getStripLayoutHelper(incognito).tabClosed(time(), tabId);
                updateModelSwitcherButton();
            }

            @Override
            public void willCloseAllTabs(boolean incognito) {
                getStripLayoutHelper(incognito).allTabsClosed();
                updateModelSwitcherButton();
            }

            @Override
            public void allTabsClosureCommitted() {
                if (mLayerTitleCacheSupplier.hasValue()) {
                    mLayerTitleCacheSupplier.get().clearExcept(Tab.INVALID_TAB_ID);
                }
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (tab.getId() == lastId) return;
                getStripLayoutHelper(tab.isIncognito()).tabSelected(time(), tab.getId(), lastId);
            }

            @Override
            public void didAddTab(Tab tab, int type, int creationState) {
                boolean selected = type != TabLaunchType.FROM_LONGPRESS_BACKGROUND
                        || (mTabModelSelector.isIncognitoSelected() && tab.isIncognito());
                getStripLayoutHelper(tab.isIncognito())
                        .tabCreated(
                                time(), tab.getId(), mTabModelSelector.getCurrentTabId(), selected);
            }
        };

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(modelSelector) {
            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                getStripLayoutHelper(tab.isIncognito()).tabLoadStarted(tab.getId());
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                getStripLayoutHelper(tab.isIncognito()).tabLoadFinished(tab.getId());
            }

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadStarted(tab.getId());
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onCrash(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTitleForTab(tab);
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon) {
                updateTitleForTab(tab);
            }
        };

        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    private void updateTitleForTab(Tab tab) {
        if (mLayerTitleCacheSupplier.get() == null) return;

        String title = mLayerTitleCacheSupplier.get().getUpdatedTitle(tab, mDefaultTitle);
        getStripLayoutHelper(tab.isIncognito()).tabTitleChanged(tab.getId(), title);
        mUpdateHost.requestUpdate();
    }

    public float getHeight() {
        return mHeight;
    }

    public float getWidth() {
        return mWidth;
    }

    public int getOrientation() {
        return mOrientation;
    }

    /**
     * Updates all internal resources and dimensions.
     * @param context The current Android {@link Context}.
     */
    public void onContextChanged(Context context) {
        mNormalHelper.onContextChanged(context);
        mIncognitoHelper.onContextChanged(context);
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        getInactiveStripLayoutHelper().finishAnimation();
        return getActiveStripLayoutHelper().updateLayout(time, dt);
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    private void tabModelSwitched(boolean incognito) {
        if (incognito == mIsIncognito) return;
        mIsIncognito = incognito;

        mIncognitoHelper.tabModelSelected(mIsIncognito);
        mNormalHelper.tabModelSelected(!mIsIncognito);

        updateModelSwitcherButton();

        mUpdateHost.requestUpdate();
    }

    private void updateModelSwitcherButton() {
        mModelSelectorButton.setIncognito(mIsIncognito);
        if (mTabModelSelector != null) {
            boolean isVisible = mTabModelSelector.getModel(true).getCount() != 0;
            mModelSelectorButton.setVisible(isVisible);

            float endMargin = isVisible ? getModelSelectorButtonWidthWithPadding() : 0.0f;

            mNormalHelper.setEndMargin(endMargin);
            mIncognitoHelper.setEndMargin(endMargin);
            updateStripScrim();
        }
    }

    private boolean isGridTabSwitcherEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS);
    }

    /**
     * @param incognito Whether or not you want the incognito StripLayoutHelper
     * @return The requested StripLayoutHelper.
     */
    @VisibleForTesting
    public StripLayoutHelper getStripLayoutHelper(boolean incognito) {
        return incognito ? mIncognitoHelper : mNormalHelper;
    }

    /**
     * @return The currently visible strip layout helper.
     */
    @VisibleForTesting
    public StripLayoutHelper getActiveStripLayoutHelper() {
        return getStripLayoutHelper(mIsIncognito);
    }

    private StripLayoutHelper getInactiveStripLayoutHelper() {
        return mIsIncognito ? mNormalHelper : mIncognitoHelper;
    }
}
