// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/**
 * The Activity for showing the Help screen.
 */
public class HelpActivity extends Activity {
    private static final String PLAY_STORE_URL = "market://details?id=";

    /** Launches an external web browser or application. */
    private void openUrl(String url) {
        Uri uri = Uri.parse(url);
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);

        // Verify that the device can launch an application for this intent, otherwise
        // startActivity() may crash the application.
        if (intent.resolveActivity(getPackageManager()) != null) {
            startActivity(intent);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        WebView webView = new WebView(this);
        setContentView(webView);

        getActionBar().setTitle(getString(R.string.actionbar_help_title));

        CharSequence appName = getTitle();
        CharSequence versionName = null;
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            versionName = info.versionName;
        } catch (PackageManager.NameNotFoundException ex) {
            throw new RuntimeException("Unable to get version: " + ex);
        }

        CharSequence subtitle = TextUtils.concat(appName, " ", versionName);
        getActionBar().setSubtitle(subtitle);

        // This line ensures the WebView remains embedded in this activity and doesn't launch an
        // external Chrome browser.
        webView.setWebViewClient(new WebViewClient());
        webView.getSettings().setJavaScriptEnabled(true);
        String url = getIntent().getDataString();
        webView.loadUrl(url);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.help_actionbar, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.actionbar_play_store:
                openUrl(PLAY_STORE_URL + getPackageName());
                return true;

            default:
                return super.onOptionsItemSelected(item);
        }
    }
}
