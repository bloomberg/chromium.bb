// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import org.chromium.base.Log;
import org.chromium.chromecast.base.CastSwitches;

/**
 * View for displaying a WebContents in CastShell.
 *
 * <p>Intended to be used with {@link android.app.Presentation}.
 *
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid through
 * CastWebContentsSurfaceHelper. If the CastContentWindowAndroid is destroyed,
 * CastWebContentsView should be removed from the activity holding it.
 * Similarily, if the view is removed from a activity or the activity holding
 * it is destroyed, CastContentWindowAndroid should be notified by intent.
 */
public class CastWebContentsView extends FrameLayout {
    private static final String TAG = "cr_CastWebContentV";

    private CastWebContentsSurfaceHelper mSurfaceHelper;

    public CastWebContentsView(Context context) {
        super(context);
        initView();
    }

    private void initView() {
        addView(LayoutInflater.from(getContext())
                        .inflate(R.layout.cast_web_contents_activity, null));
    }

    public void onStart(Bundle startArgumentsBundle) {
        Log.d(TAG, "onStart");

        if (mSurfaceHelper != null) {
            return;
        }

        mSurfaceHelper = new CastWebContentsSurfaceHelper(
                CastWebContentsScopes.onLayoutView(getContext(),
                        findViewById(R.id.web_contents_container),
                        CastSwitches.getSwitchValueColor(
                                CastSwitches.CAST_APP_BACKGROUND_COLOR, Color.BLACK),
                        this ::getHostWindowToken),
                (Uri uri) -> sendIntentSync(CastWebContentsIntentUtils.onWebContentStopped(uri)));

        CastWebContentsSurfaceHelper.StartParams params =
                CastWebContentsSurfaceHelper.StartParams.fromBundle(startArgumentsBundle);
        if (params == null) return;

        mSurfaceHelper.onNewStartParams(params);
    }

    public void onResume() {
        Log.d(TAG, "onResume");
    }

    public void onPause() {
        Log.d(TAG, "onPause");
    }

    public void onStop() {
        Log.d(TAG, "onStop");
        if (mSurfaceHelper != null) {
            mSurfaceHelper.onDestroy();
        }
    }

    @Nullable
    protected IBinder getHostWindowToken() {
        return getWindowToken();
    }

    private void sendIntentSync(Intent in) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(in);
    }
}
