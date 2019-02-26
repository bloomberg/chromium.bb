// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.media_session.mojom.MediaSessionAction;

import java.util.ArrayList;

/**
 * Robolectric tests for compact view button computation in {@link MediaNotificationManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaNotificationButtonComputationTest {
    @Test
    @Feature({"MediaNotification"})
    public void testLessThanThreeActionsWillBeAllShownInCompactView() {
        ArrayList<Integer> actions = new ArrayList<>();
        actions.add(MediaSessionAction.NEXT_TRACK);
        actions.add(MediaSessionAction.SEEK_FORWARD);
        actions.add(MediaSessionAction.PLAY);

        int[] compactViewActions =
                MediaNotificationManager.computeCompactViewActionIndices(actions);

        assertEquals(3, compactViewActions.length);
        assertEquals(0, compactViewActions[0]);
        assertEquals(1, compactViewActions[1]);
        assertEquals(2, compactViewActions[2]);
    }

    @Test
    @Feature({"MediaNotification"})
    public void testCompactViewPrefersActionPairs_SwitchTrack() {
        ArrayList<Integer> actions = new ArrayList<>();
        actions.add(MediaSessionAction.PREVIOUS_TRACK);
        actions.add(MediaSessionAction.NEXT_TRACK);
        actions.add(MediaSessionAction.SEEK_FORWARD);
        actions.add(MediaSessionAction.PLAY);

        int[] compactViewActions =
                MediaNotificationManager.computeCompactViewActionIndices(actions);

        assertEquals(3, compactViewActions.length);
        assertEquals(0, compactViewActions[0]);
        assertEquals(3, compactViewActions[1]);
        assertEquals(1, compactViewActions[2]);
    }

    @Test
    @Feature({"MediaNotification"})
    public void testCompactViewPrefersActionPairs_Seek() {
        ArrayList<Integer> actions = new ArrayList<>();
        actions.add(MediaSessionAction.NEXT_TRACK);
        actions.add(MediaSessionAction.SEEK_BACKWARD);
        actions.add(MediaSessionAction.SEEK_FORWARD);
        actions.add(MediaSessionAction.PLAY);

        int[] compactViewActions =
                MediaNotificationManager.computeCompactViewActionIndices(actions);

        assertEquals(3, compactViewActions.length);
        assertEquals(1, compactViewActions[0]);
        assertEquals(3, compactViewActions[1]);
        assertEquals(2, compactViewActions[2]);
    }

    @Test
    @Feature({"MediaNotification"})
    public void testCompactViewPreferSwitchTrackWhenBothPairsExist() {
        ArrayList<Integer> actions = new ArrayList<>();
        actions.add(MediaSessionAction.PREVIOUS_TRACK);
        actions.add(MediaSessionAction.NEXT_TRACK);
        actions.add(MediaSessionAction.SEEK_BACKWARD);
        actions.add(MediaSessionAction.SEEK_FORWARD);
        actions.add(MediaSessionAction.PLAY);

        int[] compactViewActions =
                MediaNotificationManager.computeCompactViewActionIndices(actions);

        assertEquals(3, compactViewActions.length);
        assertEquals(0, compactViewActions[0]);
        assertEquals(4, compactViewActions[1]);
        assertEquals(1, compactViewActions[2]);
    }
}
