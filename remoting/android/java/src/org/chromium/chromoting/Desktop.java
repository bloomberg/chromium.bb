// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageButton;

import org.chromium.chromoting.jni.JniInterface;

/**
 * A simple screen that does nothing except display a DesktopView and notify it of rotations.
 */
public class Desktop extends Activity {
    /** The surface that displays the remote host's desktop feed. */
    private DesktopView mRemoteHostDesktop;

    /** The button used to show the action bar. */
    private ImageButton mOverlayButton;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.desktop);
        mRemoteHostDesktop = (DesktopView)findViewById(R.id.desktop_view);
        mOverlayButton = (ImageButton)findViewById(R.id.desktop_overlay_button);
        mRemoteHostDesktop.setDesktop(this);

        // Ensure the button is initially hidden.
        showActionBar();
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
        mRemoteHostDesktop.onScreenConfigurationChanged();
    }

    /** Called to initialize the action bar. */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.desktop_actionbar, menu);
        return super.onCreateOptionsMenu(menu);
    }

    public void showActionBar() {
        mOverlayButton.setVisibility(View.INVISIBLE);
        getActionBar().show();
    }

    public void hideActionBar() {
        mOverlayButton.setVisibility(View.VISIBLE);
        getActionBar().hide();
    }

    /** The overlay button's onClick handler. */
    public void onOverlayButtonPressed(View view) {
        showActionBar();
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.actionbar_keyboard:
                ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).toggleSoftInput(0, 0);
                return true;

            case R.id.actionbar_hide:
                hideActionBar();
                return true;

            case R.id.actionbar_disconnect:
                JniInterface.disconnectFromHost();
                return true;

            case R.id.actionbar_send_ctrl_alt_del:
                {
                    int[] keys = {
                        KeyEvent.KEYCODE_CTRL_LEFT,
                        KeyEvent.KEYCODE_ALT_LEFT,
                        KeyEvent.KEYCODE_FORWARD_DEL,
                    };
                    for (int key : keys) {
                        JniInterface.keyboardAction(key, true);
                    }
                    for (int key : keys) {
                        JniInterface.keyboardAction(key, false);
                    }
                }
                return true;

            default:
                return super.onOptionsItemSelected(item);
        }
    }

    /**
     * Called once when a keyboard key is pressed, then again when that same key is released. This
     * is not guaranteed to be notified of all soft keyboard events: certian keyboards might not
     * call it at all, while others might skip it in certain situations (e.g. swipe input).
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        boolean depressed = event.getAction() == KeyEvent.ACTION_DOWN;

        switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_AT:
                JniInterface.keyboardAction(KeyEvent.KEYCODE_SHIFT_LEFT, depressed);
                JniInterface.keyboardAction(KeyEvent.KEYCODE_2, depressed);
                break;

            case KeyEvent.KEYCODE_POUND:
                JniInterface.keyboardAction(KeyEvent.KEYCODE_SHIFT_LEFT, depressed);
                JniInterface.keyboardAction(KeyEvent.KEYCODE_3, depressed);
                break;

            case KeyEvent.KEYCODE_STAR:
                JniInterface.keyboardAction(KeyEvent.KEYCODE_SHIFT_LEFT, depressed);
                JniInterface.keyboardAction(KeyEvent.KEYCODE_8, depressed);
                break;

            case KeyEvent.KEYCODE_PLUS:
                JniInterface.keyboardAction(KeyEvent.KEYCODE_SHIFT_LEFT, depressed);
                JniInterface.keyboardAction(KeyEvent.KEYCODE_EQUALS, depressed);
                break;

            default:
                // We try to send all other key codes to the host directly.
                JniInterface.keyboardAction(event.getKeyCode(), depressed);
        }

        return super.dispatchKeyEvent(event);
    }
}
