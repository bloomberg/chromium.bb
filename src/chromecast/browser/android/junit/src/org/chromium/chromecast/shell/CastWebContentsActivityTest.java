// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.anyObject;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.media.AudioManager;
import android.view.KeyEvent;
import android.view.WindowManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for CastWebContentsActivity.
 *
 * TODO(sanfin): Add more tests.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsActivityTest {
    private ActivityController<CastWebContentsActivity> mActivityLifecycle;
    private CastWebContentsActivity mActivity;
    private ShadowActivity mShadowActivity;
    private @Mock WebContents mWebContents;

    private static Intent defaultIntentForCastWebContentsActivity(WebContents webContents) {
        return CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, webContents, true, false, true, "0");
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                defaultIntentForCastWebContentsActivity(mWebContents));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mShadowActivity = Shadows.shadowOf(mActivity);
    }

    @Test
    public void testNewIntentAfterFinishLaunchesNewActivity() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        Intent intent = new Intent(Intent.ACTION_VIEW, null, RuntimeEnvironment.application,
                CastWebContentsActivity.class);
        mActivityLifecycle.newIntent(intent);
        Intent next = mShadowActivity.getNextStartedActivity();
        assertEquals(next.getComponent().getClassName(), CastWebContentsActivity.class.getName());
    }

    @Test
    public void testFinishDoesNotLaunchNewActivity() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        Intent intent = mShadowActivity.getNextStartedActivity();
        assertNull(intent);
    }

    @Test
    public void testReleasesStreamMuteIfNecessaryOnPause() {
        CastAudioManager mockAudioManager = mock(CastAudioManager.class);
        mActivity.setAudioManagerForTesting(mockAudioManager);
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause();
        verify(mockAudioManager).releaseStreamMuteIfNecessary(AudioManager.STREAM_MUSIC);
    }

    @Test
    public void testDropsIntentWithoutUri() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        WebContents newWebContents = mock(WebContents.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, newWebContents, true, false, true, null);
        intent.removeExtra(CastWebContentsIntentUtils.INTENT_EXTRA_URI);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(anyObject());
    }

    @Test
    public void testDropsIntentWithoutWebContents() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, null, true, false, true, "1");
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(anyObject());
    }

    @Test
    public void testNotifiesSurfaceHelperWithValidIntent() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        WebContents newWebContents = mock(WebContents.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, newWebContents, true, false, true, "2");
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper)
                .onNewStartParams(new CastWebContentsSurfaceHelper.StartParams(
                        CastWebContentsIntentUtils.getInstanceUri("2"), newWebContents, false,
                        true));
    }

    @Test
    public void testDropsIntentWithDuplicateUri() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        // Send duplicate Intent.
        Intent intent = defaultIntentForCastWebContentsActivity(mWebContents);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(anyObject());
    }

    @Test
    public void testTurnsScreenOnIfTurnOnScreen() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(
                        RuntimeEnvironment.application, mWebContents, true, false, true, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mShadowActivity = Shadows.shadowOf(mActivity);
        mActivityLifecycle.create();

        Assert.assertTrue(Shadows.shadowOf(mShadowActivity.getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON));
    }

    @Test
    public void testDoesNotTurnScreenOnIfNotTurnOnScreen() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(
                        RuntimeEnvironment.application, mWebContents, true, false, false, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mShadowActivity = Shadows.shadowOf(mActivity);
        mActivityLifecycle.create();

        Assert.assertFalse(Shadows.shadowOf(mShadowActivity.getWindow())
                                   .getFlag(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON));
    }

    @Test
    public void testSetsKeepScreenOnFlag() {
        mActivityLifecycle.create();
        Assert.assertTrue(Shadows.shadowOf(mShadowActivity.getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
    }

    @Test
    public void testStopDoesNotCauseFinish() {
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().stop();
        Assert.assertFalse(mShadowActivity.isFinishing());
    }

    @Test
    public void testUserLeaveAndStopCausesFinish() {
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().userLeaving().stop();
        Assert.assertTrue(mShadowActivity.isFinishing());
    }

    @Test
    public void testBackButtonFinishes() {
        mActivityLifecycle.create().start().resume();
        mActivity.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
        Assert.assertTrue(mShadowActivity.isFinishing());
    }
}
