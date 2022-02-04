// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.jank_tracker.JankScenario;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabListDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.ui.resources.ResourceManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;

/**
 * A {@link Layout} that shows all tabs in one grid or carousel view.
 */
public class StartSurfaceLayout extends Layout {
    private static final String TAG = "SSLayout";

    // Duration of the transition animation
    public static final long ZOOMING_DURATION = 300;
    private static final int BACKGROUND_FADING_DURATION_MS = 150;

    private static final String TRACE_SHOW_TAB_SWITCHER = "StartSurfaceLayout.Show.TabSwitcher";
    private static final String TRACE_HIDE_TAB_SWITCHER = "StartSurfaceLayout.Hide.TabSwitcher";
    private static final String TRACE_SHOW_START_SURFACE = "StartSurfaceLayout.Show.StartSurface";
    private static final String TRACE_HIDE_START_SURFACE = "StartSurfaceLayout.Hide.StartSurface";

    // The transition animation from a tab to the tab switcher.
    private AnimatorSet mTabToSwitcherAnimation;
    private boolean mIsAnimating;

    private TabListSceneLayer mSceneLayer;
    private final StartSurface mStartSurface;
    private final JankTracker mJankTracker;
    private final StartSurface.Controller mController;
    private final StartSurface.OverviewModeObserver mStartSurfaceObserver;
    // Always use getGridTabListDelegate() instead to make sure it's not null.
    @Nullable
    private TabSwitcher.TabListDelegate mGridTabListDelegate;
    // Always use getCarouselOrSingleTabListDelegate() instead to make sure it's not null.
    @Nullable
    private TabSwitcher.TabListDelegate mCarouselOrSingleTabListDelegate;

    // To force Toolbar finishes its animation when this Layout finished hiding.
    private final LayoutTab mDummyLayoutTab;
    private boolean mIsInitialized;

    private float mBackgroundAlpha;

    private int mFrameCount;
    private long mStartTime;
    private long mLastFrameTime;
    private long mMaxFrameInterval;
    private int mStartFrame;
    private float mThumbnailAspectRatio;

    private boolean mAndroidViewFinishedShowing;

    private Animator mBackgroundTabAnimation;

    interface PerfListener {
        void onAnimationDone(
                int frameRendered, long elapsedMs, long maxFrameInterval, int dirtySpan);
    }

    private PerfListener mPerfListenerForTesting;

    public StartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface, JankTracker jankTracker) {
        super(context, updateHost, renderHost);
        mDummyLayoutTab = createLayoutTab(Tab.INVALID_TAB_ID, false);
        mDummyLayoutTab.setShowToolbar(true);
        mStartSurface = startSurface;
        mStartSurface.setOnTabSelectingListener(this::onTabSelecting);
        mController = mStartSurface.getController();
        mJankTracker = jankTracker;

        mStartSurfaceObserver = new StartSurface.OverviewModeObserver() {
            @Override
            public void startedShowing() {
                mAndroidViewFinishedShowing = false;
            }

            @Override
            public void finishedShowing() {
                mAndroidViewFinishedShowing = true;
                doneShowing();
                // The Tab-to-GTS animation is done, and it's time to renew the thumbnail without
                // causing janky frames. When animation is off, the thumbnail is already updated
                // when showing the GTS.
                if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
                    // Delay thumbnail taking a bit more to make it less likely to happen before the
                    // thumbnail taking triggered by ThumbnailFetcher. See crbug.com/996385 for
                    // details.
                    new Handler().postDelayed(() -> {
                        Tab currentTab = mTabModelSelector.getCurrentTab();
                        if (currentTab != null) mTabContentManager.cacheTabThumbnail(currentTab);
                        mLayoutTabs = null;
                    }, ZOOMING_DURATION);
                } else {
                    // crbug.com/1176548, mLayoutTabs is used to capture thumbnail, null it in a
                    // post delay handler to avoid creating a new pending surface in native, which
                    // will hold the thumbnail capturing task.
                    new Handler().postDelayed(() -> { mLayoutTabs = null; }, ZOOMING_DURATION);
                }
            }

            @Override
            public void startedHiding() {}

            @Override
            public void finishedHiding() {
                // The Android View version of GTS overview is hidden.
                // If not doing GTS-to-Tab transition animation or start surface homepage is hiding
                // (instead of grid tab switcher), we show the fade-out instead, which was already
                // done.
                if (!TabUiFeatureUtilities.isTabToGtsAnimationEnabled()
                        || isHidingStartSurfaceHomepage()) {
                    postHiding();
                    return;
                }
                // If we are doing GTS-to-Tab transition animation, we start showing the Bitmap
                // version of the GTS overview in the background while expanding the thumbnail to
                // the viewport.
                expandTab(getThumbnailLocationOfCurrentTab());
            }
        };

        mController.addOverviewModeObserver(mStartSurfaceObserver);
        if (TabUiFeatureUtilities.isTabThumbnailAspectRatioNotOne()) {
            mThumbnailAspectRatio = (float) TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO.getValue();
            mThumbnailAspectRatio = MathUtils.clamp(mThumbnailAspectRatio, 0.5f, 2.0f);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        if (mIsInitialized) return;

        mIsInitialized = true;
        mStartSurface.initWithNative();
        ensureSceneLayerCreated();
        mSceneLayer.setTabModelSelector(mTabModelSelector);
    }

    // Layout implementation.
    @Override
    public void setTabModelSelector(TabModelSelector modelSelector, TabContentManager manager) {
        super.setTabModelSelector(modelSelector, manager);
        if (mSceneLayer != null) {
            mSceneLayer.setTabModelSelector(modelSelector);
        }
    }

    @Override
    public LayoutTab getLayoutTab(int id) {
        return mDummyLayoutTab;
    }

    @Override
    public void destroy() {
        if (mController != null) {
            mController.removeOverviewModeObserver(mStartSurfaceObserver);
        }
    }

    @Override
    public void show(long time, boolean animate) {
        boolean isShowingStartSurfaceHomepage = isShowingStartSurfaceHomepage();
        if (isShowingStartSurfaceHomepage) {
            showStartSurface(time, animate);
        } else {
            showTabSwitcher(time, animate);
        }
    }

    private void showStartSurface(long time, boolean animate) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_SHOW_START_SURFACE)) {
            show(time, animate, true /*isShowingStartSurfaceHomepage*/);
        }
    }
    private void showTabSwitcher(long time, boolean animate) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_SHOW_TAB_SWITCHER)) {
            show(time, animate, false /*isShowingStartSurfaceHomepage*/);
        }
    }

    private void show(long time, boolean animate, boolean isShowingStartSurfaceHomepage) {
        super.show(time, animate);

        // When shown on StartSurface jank is tracked under
        // JankScenario.START_SURFACE_TAB_SWITCHER and it's started/stopped on
        // StartSurfaceMediator.
        if (!StartSurfaceConfiguration.isStartSurfaceFlagEnabled()) {
            mJankTracker.startTrackingScenario(JankScenario.TAB_SWITCHER);
        }

        // Lazy initialization if needed.
        mStartSurface.initialize();

        // Keep the current tab in mLayoutTabs even if we are not going to show the shrinking
        // animation so that thumbnail taking is not blocked.
        LayoutTab sourceLayoutTab = createLayoutTab(
                mTabModelSelector.getCurrentTabId(), mTabModelSelector.isIncognitoSelected());
        sourceLayoutTab.setDecorationAlpha(0);

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};

        boolean quick;
        // If start surface homepage is showing, carousel or single tab switcher is used.
        // Otherwise grid tab switcher is used.
        if (isShowingStartSurfaceHomepage) {
            quick = getCarouselOrSingleTabListDelegate().prepareOverview();
        } else {
            quick = getGridTabListDelegate().prepareOverview();
        }

        // Skip shrinking animation when there is no tab in current tab model. If it's showing
        // start surface, we don't show the shrink tab animation.
        boolean isCurrentTabModelEmpty = mTabModelSelector.getCurrentModel().getCount() == 0;
        boolean showShrinkingAnimation = animate
                && TabUiFeatureUtilities.isTabToGtsAnimationEnabled() && !isCurrentTabModelEmpty
                && !isShowingStartSurfaceHomepage;

        boolean skipSlowZooming = TabUiFeatureUtilities.SKIP_SLOW_ZOOMING.getValue();
        Log.d(TAG, "SkipSlowZooming = " + skipSlowZooming);
        if (skipSlowZooming) {
            showShrinkingAnimation &= quick;
        }
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            // Intentionally disable the shrinking animation when accessibility is enabled.
            // During the shrinking animation, since the ComponsitorViewHolder is not focusable,
            // I think we are in a temporary no "valid" focus target state, so the focus shifts
            // to the omnibox and triggers an accessibility announcement of the URL and a
            // keyboard hiding event. Disable the animation to avoid this temporary state.
            showShrinkingAnimation &= !ChromeAccessibilityUtil.get().isAccessibilityEnabled();
        }

        if (!showShrinkingAnimation) {
            mController.showOverview(animate);
            return;
        }

        shrinkTab(animate, () -> getGridTabListDelegate().getThumbnailLocationOfCurrentTab(false));
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayerCreated();
        super.updateLayout(time, dt);
        if (mLayoutTabs == null) return;

        assert mLayoutTabs.length >= 1;
        boolean needUpdate = updateSnap(dt, mLayoutTabs[0]);
        if (needUpdate) requestUpdate();
    }

    @Override
    public void startHiding(int nextId, boolean hintAtTabSelection) {
        int startSurfaceState = mController.getStartSurfaceState();
        StartSurfaceUserData.getInstance().setUnusedTabRestoredAtStartup(false);
        if (startSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
            startHidingStartSurface(nextId, hintAtTabSelection);
        } else {
            startHidingTabSwitcher(nextId, hintAtTabSelection);
        }
    }

    private void startHidingStartSurface(int nextId, boolean hintAtTabSelection) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_HIDE_START_SURFACE)) {
            startHidingImpl(nextId, hintAtTabSelection);
        }
    }
    private void startHidingTabSwitcher(int nextId, boolean hintAtTabSelection) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_HIDE_TAB_SWITCHER)) {
            startHidingImpl(nextId, hintAtTabSelection);
        }
    }

    private void startHidingImpl(int nextId, boolean hintAtTabSelection) {
        super.startHiding(nextId, hintAtTabSelection);

        int sourceTabId = nextId;
        if (sourceTabId == Tab.INVALID_TAB_ID) {
            sourceTabId = mTabModelSelector.getCurrentTabId();
        }

        LayoutTab sourceLayoutTab =
                createLayoutTab(sourceTabId, mTabModelSelector.isIncognitoSelected());
        sourceLayoutTab.setDecorationAlpha(0);

        List<LayoutTab> layoutTabs = new ArrayList<>();
        layoutTabs.add(sourceLayoutTab);

        if (sourceTabId != mTabModelSelector.getCurrentTabId()) {
            // Keep the original tab in mLayoutTabs to unblock thumbnail taking at the end of
            // the animation.
            LayoutTab originalTab = createLayoutTab(
                    mTabModelSelector.getCurrentTabId(), mTabModelSelector.isIncognitoSelected());
            originalTab.setScale(0);
            originalTab.setDecorationAlpha(0);
            layoutTabs.add(originalTab);
        }
        mLayoutTabs = layoutTabs.toArray(new LayoutTab[0]);

        updateCacheVisibleIds(new LinkedList<>(Arrays.asList(sourceTabId)));

        mIsAnimating = true;
        mController.hideOverview(!TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
    }

    @Override
    public void doneHiding() {
        try (TraceEvent e = TraceEvent.scoped("StartSurfaceLayout.DoneHiding")) {
            super.doneHiding();
            RecordUserAction.record("MobileExitStackView");
            // When shown on StartSurface jank is tracked under
            // JankScenario.START_SURFACE_TAB_SWITCHER and it's started/stopped on
            // StartSurfaceMediator.
            if (!StartSurfaceConfiguration.isStartSurfaceFlagEnabled()) {
                mJankTracker.finishTrackingScenario(JankScenario.TAB_SWITCHER);
            }
        }
    }

    @Override
    public void doneShowing() {
        try (TraceEvent e = TraceEvent.scoped("StartSurfaceLayout.DoneShowing")) {
            if (!mAndroidViewFinishedShowing) return;
            super.doneShowing();
        }
    }

    @Override
    public boolean onBackPressed() {
        return mController.onBackPressed();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    private void ensureSceneLayerCreated() {
        if (mSceneLayer != null) return;
        mSceneLayer = new TabListSceneLayer();
    }

    @Override
    public boolean handlesTabClosing() {
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesCloseAll() {
        return false;
    }

    @Override
    protected void forceAnimationToFinish() {
        super.forceAnimationToFinish();
        if (mTabToSwitcherAnimation != null) {
            if (mTabToSwitcherAnimation.isRunning()) mTabToSwitcherAnimation.end();
        }
    }

    @Override
    public void onTabCreated(long time, int id, int index, int sourceId, boolean newIsIncognito,
            boolean background, float originX, float originY) {
        super.onTabCreated(time, id, index, sourceId, newIsIncognito, background, originX, originY);
        if (!background || newIsIncognito
                || mController.getStartSurfaceState() != StartSurfaceState.SHOWN_HOMEPAGE) {
            return;
        }
        TasksSurface primaryTasksSurface = mStartSurface.getPrimaryTasksSurface();
        assert primaryTasksSurface != null;

        if (mBackgroundTabAnimation != null && mBackgroundTabAnimation.isStarted()) {
            mBackgroundTabAnimation.end();
        }
        mBackgroundTabAnimation =
                BackgroundTabAnimation.create(this, (ViewGroup) primaryTasksSurface.getView(),
                        originX, originY, getOrientation() == Orientation.PORTRAIT);
        mBackgroundTabAnimation.start();
    }

    /**
     * Animate shrinking a tab to a target {@link Rect} area.
     * @param animate Whether to play an entry animation.
     * @param target The target {@link Rect} area.
     */
    private void shrinkTab(boolean animate, Supplier<Rect> target) {
        if (target.get() == null) {
            mController.showOverview(animate);
            return;
        }

        forceAnimationToFinish();
        LayoutTab sourceLayoutTab = mLayoutTabs[0];
        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // Step 1: zoom out the source tab
        Supplier<Float> scaleStartValueSupplier = () -> 1.0f;
        Supplier<Float> scaleEndValueSupplier = () -> target.get().width() / (getWidth() * mDpToPx);

        Supplier<Float> xStartValueSupplier = () -> 0f;
        Supplier<Float> xEndValueSupplier = () -> target.get().left / mDpToPx;

        Supplier<Float> yStartValueSupplier = () -> 0f;
        Supplier<Float> yEndValueSupplier = () -> target.get().top / mDpToPx;

        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.SCALE, scaleStartValueSupplier, scaleEndValueSupplier, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.X, xStartValueSupplier, xEndValueSupplier, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.Y, yStartValueSupplier, yEndValueSupplier, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT, sourceLayoutTab.getUnclampedOriginalContentHeight(),
                TabUiFeatureUtilities.isTabThumbnailAspectRatioNotOne()
                        ? Math.min(getWidth() / mThumbnailAspectRatio,
                                sourceLayoutTab.getUnclampedOriginalContentHeight())
                        : getWidth(),
                ZOOMING_DURATION, Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 0f, 1f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                // Step 2: fade in the real GTS RecyclerView.
                mController.showOverview(true);

                reportAnimationPerf(true);
            }
        });
        mStartFrame = mFrameCount;
        mStartTime = SystemClock.elapsedRealtime();
        mLastFrameTime = SystemClock.elapsedRealtime();
        mMaxFrameInterval = 0;
        mTabToSwitcherAnimation.start();
    }

    /**
     * Animate expanding a tab from a source {@link Rect} area.
     * @param source The source {@link Rect} area.
     */
    private void expandTab(Rect source) {
        LayoutTab sourceLayoutTab = mLayoutTabs[0];

        forceAnimationToFinish();
        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // Zoom in the source tab
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.SCALE, source.width() / (getWidth() * mDpToPx), 1, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.X, source.left / mDpToPx, 0f, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.Y, source.top / mDpToPx, 0f, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT,
                TabUiFeatureUtilities.isTabThumbnailAspectRatioNotOne()
                        ? Math.min(getWidth() / mThumbnailAspectRatio,
                                sourceLayoutTab.getUnclampedOriginalContentHeight())
                        : getWidth(),
                sourceLayoutTab.getUnclampedOriginalContentHeight(), ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 1f, 0f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                postHiding();

                reportAnimationPerf(false);
            }
        });
        mStartFrame = mFrameCount;
        mStartTime = SystemClock.elapsedRealtime();
        mLastFrameTime = SystemClock.elapsedRealtime();
        mMaxFrameInterval = 0;
        mTabToSwitcherAnimation.start();
    }

    private Rect getThumbnailLocationOfCurrentTab() {
        if (isHidingStartSurfaceHomepage()) {
            return getCarouselOrSingleTabListDelegate().getThumbnailLocationOfCurrentTab(true);
        } else {
            return getGridTabListDelegate().getThumbnailLocationOfCurrentTab(true);
        }
    }

    private TabListDelegate getCarouselOrSingleTabListDelegate() {
        if (mCarouselOrSingleTabListDelegate == null) {
            mCarouselOrSingleTabListDelegate = mStartSurface.getCarouselOrSingleTabListDelegate();
        }
        return mCarouselOrSingleTabListDelegate;
    }

    private TabListDelegate getGridTabListDelegate() {
        if (mGridTabListDelegate == null) {
            mGridTabListDelegate = mStartSurface.getGridTabListDelegate();
        }
        return mGridTabListDelegate;
    }

    private TabListDelegate getLastUsedTabListDelegate() {
        // It is possible that the StartSurfaceState becomes StartSurfaceState.NOT_SHOWN when hiding
        // the overview page, thus, the last used TabListDelegate is returned.
        if (mController.getStartSurfaceState() == StartSurfaceState.NOT_SHOWN) {
            assert mGridTabListDelegate != null || mCarouselOrSingleTabListDelegate != null;
            return mGridTabListDelegate != null ? mGridTabListDelegate
                                                : mCarouselOrSingleTabListDelegate;
        }
        return isShowingStartSurfaceHomepage() ? getCarouselOrSingleTabListDelegate()
                                               : getGridTabListDelegate();
    }

    /**
     * When state is SHOWN_HOMEPAGE or SHOWING_HOMEPAGE or SHOWING_START, state surface homepage is
     * showing. When state is StartSurfaceState.SHOWING_PREVIOUS and the previous state is
     * SHOWN_HOMEPAGE or NOT_SHOWN, homepage is showing.
     * @return Whether start surface homepage is showing.
     */
    private boolean isShowingStartSurfaceHomepage() {
        @StartSurfaceState
        int currentState = mController.getStartSurfaceState();
        @StartSurfaceState
        int previousState = mController.getPreviousStartSurfaceState();

        return currentState == StartSurfaceState.SHOWN_HOMEPAGE
                || currentState == StartSurfaceState.SHOWING_HOMEPAGE
                || currentState == StartSurfaceState.SHOWING_START
                || (currentState == StartSurfaceState.SHOWING_PREVIOUS
                        && (previousState == StartSurfaceState.SHOWN_HOMEPAGE
                                || previousState == StartSurfaceState.NOT_SHOWN));
    }

    private boolean isHidingStartSurfaceHomepage() {
        return mController.getPreviousStartSurfaceState() == StartSurfaceState.SHOWN_HOMEPAGE;
    }

    private void postHiding() {
        if (ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(getContext())) {
            mStartSurface.onHide();
        } else {
            getGridTabListDelegate().postHiding();
        }
        mIsAnimating = false;
        doneHiding();
    }

    @VisibleForTesting
    void setPerfListenerForTesting(PerfListener perfListener) {
        mPerfListenerForTesting = perfListener;
    }

    @VisibleForTesting
    public StartSurface getStartSurfaceForTesting() {
        return mStartSurface;
    }

    private void reportAnimationPerf(boolean isShrinking) {
        int frameRendered = mFrameCount - mStartFrame;
        long elapsedMs = SystemClock.elapsedRealtime() - mStartTime;
        // If it's hiding start surface, TabListDelegate for carousel/single tab switcher should be
        // used.
        long lastDirty = isHidingStartSurfaceHomepage()
                ? getCarouselOrSingleTabListDelegate().getLastDirtyTime()
                : getGridTabListDelegate().getLastDirtyTime();
        int dirtySpan = (int) (lastDirty - mStartTime);
        float fps = 1000.f * frameRendered / elapsedMs;
        String message = String.format(Locale.US,
                "fps = %.2f (%d / %dms), maxFrameInterval = %d, dirtySpan = %d", fps, frameRendered,
                elapsedMs, mMaxFrameInterval, dirtySpan);

        // TODO(crbug.com/964406): stop logging it after this feature stabilizes.
        if (!VersionInfo.isStableBuild()) {
            Log.i(TAG, message);
        }

        String suffix;
        if (isShrinking) {
            suffix = ".Shrink";
        } else {
            suffix = ".Expand";
        }

        // TODO(crbug.com/982018): Separate histograms for carousel tab switcher.
        RecordHistogram.recordCount100Histogram(
                "GridTabSwitcher.FramePerSecond" + suffix, (int) fps);
        RecordHistogram.recordTimesHistogram(
                "GridTabSwitcher.MaxFrameInterval" + suffix, mMaxFrameInterval);
        RecordHistogram.recordTimesHistogram("GridTabSwitcher.DirtySpan" + suffix, dirtySpan);

        if (mPerfListenerForTesting != null) {
            mPerfListenerForTesting.onAnimationDone(
                    frameRendered, elapsedMs, mMaxFrameInterval, dirtySpan);
        }
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            TabContentManager tabContentManager, ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayerCreated();
        super.updateSceneLayer(
                viewport, contentViewport, tabContentManager, resourceManager, browserControls);
        assert mSceneLayer != null;
        TabListDelegate currentTabListDelegate = getLastUsedTabListDelegate();

        // The content viewport is intentionally sent as both params below.
        mSceneLayer.pushLayers(getContext(), contentViewport, contentViewport, this,
                tabContentManager, resourceManager, browserControls,
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled()
                        ? currentTabListDelegate.getResourceId()
                        : 0,
                mBackgroundAlpha, currentTabListDelegate.getTabListTopOffset());
        mFrameCount++;
        if (mLastFrameTime != 0) {
            long elapsed = SystemClock.elapsedRealtime() - mLastFrameTime;
            mMaxFrameInterval = Math.max(mMaxFrameInterval, elapsed);
        }
        mLastFrameTime = SystemClock.elapsedRealtime();
    }

    @Override
    public int getLayoutType() {
        return LayoutType.TAB_SWITCHER;
    }

    @Override
    public boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        return mTabToSwitcherAnimation == null && !mIsAnimating;
    }

    @Override
    public boolean canHostBeFocusable() {
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()
                && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            // We don't allow this layout to gain focus when accessibility is enabled so that the
            // CompositorViewHolder doesn't steal focus when entering tab switcher.
            // (crbug.com/1125185).
            return false;
        }
        return super.canHostBeFocusable();
    }
}
