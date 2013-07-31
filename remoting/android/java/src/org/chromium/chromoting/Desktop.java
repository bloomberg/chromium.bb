// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

import org.chromium.chromoting.jni.JniInterface;

/**
 * A simple screen that does nothing except display a DesktopView and notify it of rotations.
 */
public class Desktop extends Activity {
    /** The surface that displays the remote host's desktop feed. */
    private DesktopView remoteHostDesktop;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        remoteHostDesktop = new DesktopView(this);
        setContentView(remoteHostDesktop);
    }

    /** Called when the activity is finally finished. */
    @Override
    public void onDestroy() {
        super.onDestroy();
        JniInterface.disconnectFromHost();
    }

    /** Called when the display is rotated (as registered in the manifest). */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        remoteHostDesktop.requestRecheckConstrainingDimension();
    }

    /** Called to initialize the action bar. */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.actionbar, menu);
        return super.onCreateOptionsMenu(menu);
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.actionbar_keyboard:
                ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).toggleSoftInput(0, 0);
                return true;
            case R.id.actionbar_hide:
                getActionBar().hide();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    /** Called when a hardware key is pressed, and usually when a software key is pressed. */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        JniInterface.keyboardAction(event.getKeyCode(), event.getAction() == KeyEvent.ACTION_DOWN);

        if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
            // We stop this event from propagating further to prevent the keyboard from closing.
            return true;
        }

        return super.dispatchKeyEvent(event);
    }
}
