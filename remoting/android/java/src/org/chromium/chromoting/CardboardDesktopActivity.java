// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.os.Bundle;

import com.google.vrtoolkit.cardboard.CardboardActivity;
import com.google.vrtoolkit.cardboard.CardboardView;

/**
 * Virtual desktop activity for Cardboard.
 */
public class CardboardDesktopActivity extends CardboardActivity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.cardboard_desktop);

        CardboardView cardboardView = (CardboardView) findViewById(R.id.cardboard_view);

        // Associate a CardboardView.StereoRenderer with cardboardView.
        cardboardView.setRenderer(new CardboardDesktopRenderer(this));

        // Associate the cardboardView with this activity.
        setCardboardView(cardboardView);
    }
}
