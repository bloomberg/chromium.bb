// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.widget.FrameLayout;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkMetaDataUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;
import org.chromium.webapk.shell_apk.R;
import org.chromium.webapk.shell_apk.WebApkUtils;

/**
 * Splash screen activity which is launched on top of host browser and stays
 * up till the host browser draws a non empty frame.
 */
public class OverlaySplashActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        showSplashScreen();
    }

    // TODO(pkotwicz): Make SplashActivity and OverlaySplashActivity share code.
    private void showSplashScreen() {
        Bundle metadata = WebApkUtils.readMetaData(this);
        Resources resources = getResources();

        Bitmap icon = WebApkUtils.decodeBitmapFromDrawable(resources, R.drawable.splash_icon);
        @SplashLayout.IconClassification
        int iconClassification = SplashLayout.classifyIcon(resources, icon, false);

        FrameLayout layout = new FrameLayout(this);
        setContentView(layout);

        int backgroundColor = WebApkUtils.getColor(resources, R.color.background_color);
        SplashLayout.createLayout(this, layout, icon, false /* isIconAdaptive */,
                iconClassification, resources.getString(R.string.name),
                WebApkUtils.shouldUseLightForegroundOnBackground(backgroundColor));

        int themeColor = (int) WebApkMetaDataUtils.getLongFromMetaData(
                metadata, WebApkMetaDataKeys.THEME_COLOR, Color.BLACK);
        WebApkUtils.setStatusBarColor(
                getWindow(), WebApkUtils.getDarkenedColorForStatusBar(themeColor));
    }
}
