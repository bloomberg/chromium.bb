// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.RectF;
import android.os.Handler;

import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.ui.resources.ResourceManager;

import java.util.Arrays;
import java.util.LinkedList;

/**
 * A {@link Layout} that shows a single tab at full screen. This tab is chosen based on the
 * {@link #tabSelecting(long, int)} call, and is used to show a thumbnail of a {@link Tab}
 * until that {@link Tab} is ready to be shown.
 */
public class StaticLayout extends Layout {
    public static final String TAG = "StaticLayout";

    private static final int HIDE_TIMEOUT_MS = 2000;
    private static final int HIDE_DURATION_MS = 500;

    private boolean mHandlesTabLifecycles;

    private class UnstallRunnable implements Runnable {
        @Override
        public void run() {
            mUnstalling = false;
            if (mLayoutTabs == null || mLayoutTabs.length == 0) return;
            CompositorAnimator
                    .ofFloatProperty(getAnimationHandler(), mLayoutTabs[0], LayoutTab.SATURATION,
                            mLayoutTabs[0].getSaturation(), 1.0f, HIDE_DURATION_MS)
                    .start();
            CompositorAnimator
                    .ofFloatProperty(getAnimationHandler(), mLayoutTabs[0],
                            LayoutTab.STATIC_TO_VIEW_BLEND, mLayoutTabs[0].getStaticToViewBlend(),
                            0.0f, HIDE_DURATION_MS)
                    .start();
            mLayoutTabs[0].setShouldStall(false);
        }
    }

    private final UnstallRunnable mUnstallRunnable;
    private final Handler mHandler;
    private boolean mUnstalling;
    private StaticTabSceneLayer mSceneLayer;

    /**
     * Creates an instance of the {@link StaticLayout}.
     * @param context             The current Android's context.
     * @param updateHost          The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost          The {@link LayoutRenderHost} view for this layout.
     * @param panelManager        The {@link OverlayPanelManager} responsible for showing panels.
     */
    public StaticLayout(Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost,
            EventFilter eventFilter, OverlayPanelManager panelManager) {
        super(context, updateHost, renderHost);

        mHandler = new Handler();
        mUnstallRunnable = new UnstallRunnable();
        mUnstalling = false;
        mSceneLayer = new StaticTabSceneLayer();
    }

    /**
     * @param handlesTabLifecycles Whether or not this {@link Layout} should handle tab closing and
     *                             creating events.
     */
    public void setLayoutHandlesTabLifecycles(boolean handlesTabLifecycles) {
        mHandlesTabLifecycles = handlesTabLifecycles;
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.DYNAMIC_BROWSER_CONTROLS;
    }

    /**
     * Initialize the layout to be shown.
     * @param time   The current time of the app in ms.
     * @param animate Whether to play an entry animation.
     */
    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        mLayoutTabs = null;
        setStaticTab(mTabModelSelector.getCurrentTabId());
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);
        if (mLayoutTabs != null && mLayoutTabs.length > 0) mLayoutTabs[0].updateSnap(dt);
    }

    @Override
    public void onTabSelected(long time, int id, int prevId, boolean incognito) {
        setStaticTab(id);
        super.onTabSelected(time, id, prevId, incognito);
    }

    @Override
    public void onTabSelecting(long time, int id) {
        setStaticTab(id);
        super.onTabSelecting(time, id);
    }

    @Override
    public void onTabCreated(long time, int tabId, int tabIndex, int sourceTabId,
            boolean newIsIncognito, boolean background, float originX, float originY) {
        super.onTabCreated(
                time, tabId, tabIndex, sourceTabId, newIsIncognito, background, originX, originY);
        if (!background) setStaticTab(tabId);
    }

    @Override
    public void onTabModelSwitched(boolean incognito) {
        super.onTabModelSwitched(incognito);
        setStaticTab(mTabModelSelector.getCurrentTabId());
    }

    @Override
    public void setTabModelSelector(TabModelSelector modelSelector, TabContentManager manager) {
        super.setTabModelSelector(modelSelector, manager);
        new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                if (isActive()) unstallImmediately(tab.getId());
            }
        };
    }

    private void setPreHideState() {
        mHandler.removeCallbacks(mUnstallRunnable);
        mLayoutTabs[0].setStaticToViewBlend(1.0f);
        mLayoutTabs[0].setSaturation(0.0f);
        mUnstalling = true;
    }

    private void setPostHideState() {
        mHandler.removeCallbacks(mUnstallRunnable);
        mLayoutTabs[0].setStaticToViewBlend(0.0f);
        mLayoutTabs[0].setSaturation(1.0f);
        mUnstalling = false;
    }

    private void setStaticTab(final int id) {
        if (mLayoutTabs != null && mLayoutTabs.length > 0 && mLayoutTabs[0].getId() == id) {
            if (!mLayoutTabs[0].shouldStall()) setPostHideState();
            return;
        }
        TabModel model = mTabModelSelector.getModelForTabId(id);
        if (model == null) return;

        updateCacheVisibleIdsAndPrimary(new LinkedList<Integer>(Arrays.asList(id)), id);

        if (mLayoutTabs == null || mLayoutTabs.length != 1) mLayoutTabs = new LayoutTab[1];
        mLayoutTabs[0] = createLayoutTab(id, model.isIncognito(), NO_CLOSE_BUTTON, NO_TITLE);
        mLayoutTabs[0].setDrawDecoration(false);
        if (mLayoutTabs[0].shouldStall()) {
            setPreHideState();
            mHandler.postDelayed(mUnstallRunnable, HIDE_TIMEOUT_MS);
        } else {
            setPostHideState();
        }
        requestRender();
    }

    @Override
    public void unstallImmediately(int tabId) {
        if (mLayoutTabs != null && mLayoutTabs.length > 0 && mLayoutTabs[0].getId() == tabId) {
            unstallImmediately();
        }
    }

    @Override
    public void unstallImmediately() {
        if (mLayoutTabs != null && mLayoutTabs.length > 0 && mLayoutTabs[0].shouldStall()
                && mUnstalling) {
            mHandler.removeCallbacks(mUnstallRunnable);
            mUnstallRunnable.run();
        }
    }

    @Override
    public boolean handlesTabCreating() {
        return super.handlesTabCreating() || mHandlesTabLifecycles;
    }

    @Override
    public boolean handlesTabClosing() {
        return mHandlesTabLifecycles;
    }

    @Override
    public boolean handlesCloseAll() {
        return mHandlesTabLifecycles;
    }

    @Override
    public boolean shouldDisplayContentOverlay() {
        return true;
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, fullscreenManager);
        assert mSceneLayer != null;

        final LayoutTab[] tabs = getLayoutTabsToRender();
        if (tabs == null || tabs.length != 1 || tabs[0].getId() == Tab.INVALID_TAB_ID) {
            return;
        }
        LayoutTab layoutTab = tabs[0];
        final float dpToPx = getContext().getResources().getDisplayMetrics().density;

        mSceneLayer.update(
                dpToPx, layerTitleCache, tabContentManager, fullscreenManager, layoutTab);

        // TODO(dtrainor): Find the best way to properly track this metric for cold starts.
        // We should probably erase the thumbnail when we select a tab that we need to restore.
        if (tabContentManager != null
                && tabContentManager.hasFullCachedThumbnail(layoutTab.getId())) {
            TabModelImpl.logPerceivedTabSwitchLatencyMetric();
        }
    }

    @Override
    public void destroy() {
        if (mSceneLayer != null) {
            mSceneLayer.destroy();
            mSceneLayer = null;
        }
    }
}
