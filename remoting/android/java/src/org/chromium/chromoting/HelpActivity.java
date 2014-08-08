// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.ui.UiUtils;

/**
 * The Activity for showing the Help screen.
 */
public class HelpActivity extends Activity {
    private static final String PLAY_STORE_URL = "market://details?id=";

    private static final String FEEDBACK_PACKAGE = "com.google.android.gms";

    private static final String FEEDBACK_CLASS =
            "com.google.android.gms.feedback.LegacyBugReportService";

    /**
     * Maximum dimension for the screenshot to be sent to the Send Feedback handler.  This size
     * ensures the size of bitmap < 1MB, which is a requirement of the handler.
     */
    private static final int MAX_FEEDBACK_SCREENSHOT_DIMENSION = 600;

    /**
     * This global variable is used for passing the screenshot from the originating Activity to the
     * HelpActivity. There seems to be no better way of doing this.
     */
    private static Bitmap sScreenshot;

    /** Constant used to send the feedback parcel to the system feedback service. */
    private static final int SEND_FEEDBACK_INFO = Binder.FIRST_CALL_TRANSACTION;

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

    private void sendFeedback() {
        Intent intent = new Intent(Intent.ACTION_BUG_REPORT);
        intent.setComponent(new ComponentName(FEEDBACK_PACKAGE, FEEDBACK_CLASS));
        if (getPackageManager().resolveService(intent, 0) == null) {
            Log.e("help", "Unable to resolve Feedback service.");
            return;
        }

        ServiceConnection conn = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                try {
                    Parcel parcel = Parcel.obtain();
                    if (sScreenshot != null) {
                        sScreenshot.writeToParcel(parcel, 0);
                    }
                    service.transact(SEND_FEEDBACK_INFO, parcel, null, 0);
                    parcel.recycle();
                } catch (RemoteException ex) {
                    Log.e("help", "Unexpected error sending feedback: ", ex);
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };

        bindService(intent, conn, BIND_AUTO_CREATE);
    }

    /** Launches the Help activity. */
    public static void launch(Activity activity, String helpUrl) {
        View rootView = activity.getWindow().getDecorView().getRootView();
        sScreenshot = UiUtils.generateScaledScreenshot(rootView, MAX_FEEDBACK_SCREENSHOT_DIMENSION,
                Bitmap.Config.ARGB_8888);

        Intent intent = new Intent(activity, HelpActivity.class);
        intent.setData(Uri.parse(helpUrl));
        activity.startActivity(intent);
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
        int id = item.getItemId();
        if (id == R.id.actionbar_feedback) {
            sendFeedback();
            return true;
        }
        if (id == R.id.actionbar_play_store) {
            openUrl(PLAY_STORE_URL + getPackageName());
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
