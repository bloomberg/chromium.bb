// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.os.IBinder;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;

/**
 * Unit tests for {@link CompositorViewHolder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CompositorViewHolderUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    // Since these tests don't depend on the heights being pixels, we can use these as dpi directly.
    private static final int TOOLBAR_HEIGHT = 56;

    @Mock
    private Activity mActivity;
    @Mock
    private ControlContainer mControlContainer;
    @Mock
    private View mContainerView;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private android.content.res.Resources mResources;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private ContentView mContentView;
    @Mock
    private CompositorView mCompositorView;
    @Mock
    private KeyboardVisibilityDelegate mMockKeyboard;

    private Context mContext;
    private CompositorViewHolder mCompositorViewHolder;
    private BrowserControlsManager mBrowserControlsManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        // Setup the mock keyboard.
        KeyboardVisibilityDelegate.setInstance(mMockKeyboard);

        // Setup for BrowserControlsManager which initiates content/control offset changes
        // for CompositorViewHolder.
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(R.dimen.control_container_height))
                .thenReturn(TOOLBAR_HEIGHT);
        when(mControlContainer.getView()).thenReturn(mContainerView);
        when(mTab.isUserInteractable()).thenReturn(true);

        BrowserControlsManager browserControlsManager =
                new BrowserControlsManager(mActivity, BrowserControlsManager.ControlsPosition.TOP);
        mBrowserControlsManager = spy(browserControlsManager);
        mBrowserControlsManager.initialize(mControlContainer, mActivityTabProvider,
                mTabModelSelector, R.dimen.control_container_height);
        when(mBrowserControlsManager.getTab()).thenReturn(mTab);

        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.ColorOverlay);
        mCompositorViewHolder = spy(new CompositorViewHolder(mContext));
        mCompositorViewHolder.setCompositorViewForTesting(mCompositorView);
        mCompositorViewHolder.setBrowserControlsManager(mBrowserControlsManager);
        when(mCompositorViewHolder.getContentView()).thenReturn(mContentView);
        when(mCompositorViewHolder.getWebContents()).thenReturn(mWebContents);
        when(mTab.getWebContents()).thenReturn(mWebContents);

        IBinder windowToken = mock(IBinder.class);
        when(mContainerView.getWindowToken()).thenReturn(windowToken);
    }

    // controlsResizeView tests ---
    // For these tests, we will simulate the scrolls assuming we either completely show or hide (or
    // scroll until the min-height) the controls and don't leave at in-between positions. The reason
    // is that CompositorViewHolder only flips the mControlsResizeView bit if the controls are
    // idle, meaning they're at the min-height or fully shown. Making sure the controls snap to
    // these two positions is not CVH's responsibility as it's handled in native code by compositor
    // or blink.

    @Test
    public void testControlsResizeViewChanges() {
        // Let's use simpler numbers for this test.
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        // Initially, the controls should be fully visible.
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll to fully hidden.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -100,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 0,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("Browser controls aren't at min-height.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        // ControlsResizeView is true, but it should be false when the controls are hidden.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(false));
        reset(mCompositorView);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("Browser controls are hidden when they should be fully visible.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // #controlsResizeView should be flipped back to true.
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);
    }

    @Test
    public void testControlsResizeViewChangesWithMinHeight() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);
        mBrowserControlsManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        // Initially, the controls should be fully visible.
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -75,
                /*bottomControlsOffsetY*/ 60, /*contentOffsetY*/ 25,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("Browser controls aren't at min-height.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        // ControlsResizeView is true but it should be false when the controls are at min-height.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(false));
        reset(mCompositorView);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("Browser controls are at min-height when they should be fully visible.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // #controlsResizeView should be flipped back to true.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);
    }

    @Test
    public void testControlsResizeViewWhenControlsAreNotIdle() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);
        mBrowserControlsManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        // ControlsResizeView is false but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll a little hide the controls partially.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -25,
                /*bottomControlsOffsetY*/ 20, /*contentOffsetY*/ 75,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        // ControlsResizeView is false, but it should still be true. No-op updates won't trigger a
        // changed event.
        verify(mCompositorView, times(0)).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll controls all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -75,
                /*bottomControlsOffsetY*/ 60, /*contentOffsetY*/ 25,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        // ControlsResizeView is true but it should've flipped to false since the controls are idle
        // now.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(false));
        reset(mCompositorView);

        // Scroll controls to show a little more.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -50,
                /*bottomControlsOffsetY*/ 40, /*contentOffsetY*/ 50,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        // ControlsResizeView is true, but it should still be false. No-op updates won't trigger a
        // changed event.
        verify(mCompositorView, times(0)).onControlsResizeViewChanged(any(), eq(false));
    }

    // --- controlsResizeView tests

    // Keyboard resize tests for geometrychange event fired to JS.
    @Test
    public void testWebContentResizeTriggeredDueToKeyboardShow() {
        // Set the overlaycontent flag.
        when(mCompositorViewHolder.shouldVirtualKeyboardOverlayContent(mWebContents))
                .thenReturn(true);
        // show the keyboard and set height of the webcontent.
        // totalAdjustedHeight = keyboardHeight (741) + height passed to #setSize (200)
        int totalAdjustedHeight = 941;
        int totalAdjustedWidth = 1080;
        // Set keyboard height and visibility.
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboard.calculateKeyboardHeight(any())).thenReturn(741);
        mCompositorViewHolder.setSize(mWebContents, mContainerView, 1080, 200);
        verify(mWebContents, times(1)).setSize(totalAdjustedWidth, totalAdjustedHeight);
        verify(mCompositorViewHolder, times(1))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, totalAdjustedWidth, 741);

        // Hide the keyboard.
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(false);
        when(mMockKeyboard.calculateKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setSize(mWebContents, mContainerView, 1080, 700);
        verify(mWebContents, times(1)).setSize(1080, 700);
        verify(mCompositorViewHolder, times(1))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testOverlayGeometryNotTriggeredDueToNoKeyboard() {
        // Set the overlaycontent flag.
        when(mCompositorViewHolder.shouldVirtualKeyboardOverlayContent(mWebContents))
                .thenReturn(true);
        // show the keyboard and set height of the webcontent.
        // totalAdjustedHeight = height passed to #setSize (700)
        int totalAdjustedHeight = 700;
        int totalAdjustedWidth = 1080;
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(false);
        when(mMockKeyboard.calculateKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setSize(
                mWebContents, mContainerView, totalAdjustedWidth, totalAdjustedHeight);
        verify(mWebContents, times(1)).setSize(totalAdjustedWidth, totalAdjustedHeight);
        verify(mCompositorViewHolder, times(0))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeWhenNotOverlayGeometry() {
        // Set the overlaycontent flag.
        when(mCompositorViewHolder.shouldVirtualKeyboardOverlayContent(mWebContents))
                .thenReturn(false);
        // show the keyboard and set height of the webcontent.
        // totalAdjustedHeight = height passed to #setSize (200).
        // The reduced height is because of the keyboard taking up the bottom space.
        int totalAdjustedHeight = 200;
        int totalAdjustedWidth = 1080;
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboard.calculateKeyboardHeight(any())).thenReturn(741);
        mCompositorViewHolder.setSize(
                mWebContents, mContainerView, totalAdjustedWidth, totalAdjustedHeight);
        verify(mWebContents, times(1)).setSize(totalAdjustedWidth, totalAdjustedHeight);
        verify(mCompositorViewHolder, times(0))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testOverlayGeometryWhenViewNotAttachedToWindow() {
        // Set the overlaycontent flag.
        when(mCompositorViewHolder.shouldVirtualKeyboardOverlayContent(mWebContents))
                .thenReturn(true);
        when(mContainerView.getWindowToken()).thenReturn(null);
        // show the keyboard and set height of the webcontent.
        // totalAdjustedHeight = height passed to #setSize (200)
        // The reduced height is because of the keyboard taking up the bottom space.
        int totalAdjustedHeight = 200;
        int totalAdjustedWidth = 1080;
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboard.calculateKeyboardHeight(any())).thenReturn(741);
        mCompositorViewHolder.setSize(
                mWebContents, mContainerView, totalAdjustedWidth, totalAdjustedHeight);
        verify(mCompositorViewHolder, times(0))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }
}
