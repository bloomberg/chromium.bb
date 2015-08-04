// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.PointF;
import android.os.Bundle;

import com.google.vrtoolkit.cardboard.CardboardActivity;
import com.google.vrtoolkit.cardboard.CardboardView;

import org.chromium.chromoting.jni.JniInterface;

/**
 * Virtual desktop activity for Cardboard.
 */
public class CardboardDesktopActivity extends CardboardActivity {
    // Flag to indicate whether the current activity is going to switch to normal
    // desktop activity.
    private boolean mSwitchToDesktopActivity;

    private CardboardDesktopRenderer mRenderer;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.cardboard_desktop);
        mSwitchToDesktopActivity = false;
        CardboardView cardboardView = (CardboardView) findViewById(R.id.cardboard_view);
        mRenderer = new CardboardDesktopRenderer(this);

        // Associate a CardboardView.StereoRenderer with cardboardView.
        cardboardView.setRenderer(mRenderer);

        // Associate the cardboardView with this activity.
        setCardboardView(cardboardView);
    }

    @Override
    public void onCardboardTrigger() {
        if (mRenderer.isLookingAtDesktop()) {
            PointF coordinates = mRenderer.getMouseCoordinates();
            JniInterface.sendMouseEvent((int) coordinates.x, (int) coordinates.y,
                    TouchInputHandler.BUTTON_LEFT, true);
            JniInterface.sendMouseEvent((int) coordinates.x, (int) coordinates.y,
                    TouchInputHandler.BUTTON_LEFT, false);
        } else {
            mSwitchToDesktopActivity = true;
            finish();
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        JniInterface.enableVideoChannel(true);
        mRenderer.attachRedrawCallback();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (!mSwitchToDesktopActivity) {
            JniInterface.enableVideoChannel(false);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        JniInterface.enableVideoChannel(true);
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (mSwitchToDesktopActivity) {
            mSwitchToDesktopActivity = false;
        } else {
            JniInterface.enableVideoChannel(false);
        }
    }
}
