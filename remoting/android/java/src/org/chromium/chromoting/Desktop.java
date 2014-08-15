// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.annotation.SuppressLint;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.ActionBarActivity;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageButton;

import org.chromium.chromoting.jni.JniInterface;

import java.util.Set;
import java.util.TreeSet;

/**
 * A simple screen that does nothing except display a DesktopView and notify it of rotations.
 */
public class Desktop extends ActionBarActivity implements View.OnSystemUiVisibilityChangeListener {
    /** Web page to be displayed in the Help screen when launched from this activity. */
    private static final String HELP_URL =
            "http://support.google.com/chrome/?p=mobile_crd_connecthost";

    /** The surface that displays the remote host's desktop feed. */
    private DesktopView mRemoteHostDesktop;

    /** The button used to show the action bar. */
    private ImageButton mOverlayButton;

    /** Set of pressed keys for which we've sent TextEvent. */
    private Set<Integer> mPressedTextKeys = new TreeSet<Integer>();

    private ActivityLifecycleListener mActivityLifecycleListener;


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

        View decorView = getWindow().getDecorView();
        decorView.setOnSystemUiVisibilityChangeListener(this);

        mActivityLifecycleListener = CapabilityManager.getInstance()
            .onActivityAcceptingListener(this, Capabilities.CAST_CAPABILITY);
        mActivityLifecycleListener.onActivityCreated(this, savedInstanceState);
    }

    @Override
    protected void onStart() {
        super.onStart();
        mActivityLifecycleListener.onActivityStarted(this);
    }

    @Override
    protected void onPause() {
      if (isFinishing()) {
        mActivityLifecycleListener.onActivityPaused(this);
      }
      super.onPause();
    }

    @Override
    public void onResume() {
        super.onResume();
        mActivityLifecycleListener.onActivityResumed(this);
    }

    @Override
    protected void onStop() {
        mActivityLifecycleListener.onActivityStopped(this);
        super.onStop();
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

        mActivityLifecycleListener.onActivityCreatedOptionsMenu(this, menu);

        return super.onCreateOptionsMenu(menu);
    }

    /** Called whenever the visibility of the system status bar or navigation bar changes. */
    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        // Ensure the action-bar's visibility matches that of the system controls. This
        // minimizes the number of states the UI can be in, to keep things simple for the user.

        // Determine if the system is in fullscreen/lights-out mode. LOW_PROFILE is needed since
        // it's the only flag supported in 4.0. But it is not sufficient in itself; when
        // IMMERSIVE_STICKY mode is used, the system clears this flag (leaving the FULLSCREEN flag
        // set) when the user swipes the edge to reveal the bars temporarily. When this happens,
        // the action-bar should remain hidden.
        int fullscreenFlags = getSystemUiFlags();
        if ((visibility & fullscreenFlags) != 0) {
            hideActionBar();
        } else {
            showActionBar();
        }
    }

    @SuppressLint("InlinedApi")
    private int getSystemUiFlags() {
        int flags = View.SYSTEM_UI_FLAG_LOW_PROFILE;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            flags |= View.SYSTEM_UI_FLAG_FULLSCREEN;
        }
        return flags;
    }

    public void showActionBar() {
        mOverlayButton.setVisibility(View.INVISIBLE);
        getActionBar().show();

        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
    }

    @SuppressLint("InlinedApi")
    public void hideActionBar() {
        mOverlayButton.setVisibility(View.VISIBLE);
        getActionBar().hide();

        View decorView = getWindow().getDecorView();

        // LOW_PROFILE gives the status and navigation bars a "lights-out" appearance.
        // FULLSCREEN hides the status bar on supported devices (4.1 and above).
        int flags = getSystemUiFlags();

        // HIDE_NAVIGATION hides the navigation bar. However, if the user touches the screen, the
        // event is not seen by the application and instead the navigation bar is re-shown.
        // IMMERSIVE(_STICKY) fixes this problem and allows the user to interact with the app while
        // keeping the navigation controls hidden. This flag was introduced in 4.4, later than
        // HIDE_NAVIGATION, and so a runtime check is needed before setting either of these flags.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            flags |= (View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }

        decorView.setSystemUiVisibility(flags);
    }

    /** The overlay button's onClick handler. */
    public void onOverlayButtonPressed(View view) {
        showActionBar();
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();

        mActivityLifecycleListener.onActivityOptionsItemSelected(this, item);

        if (id == R.id.actionbar_keyboard) {
            ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).toggleSoftInput(0, 0);
            return true;
        }
        if (id == R.id.actionbar_hide) {
            hideActionBar();
            return true;
        }
        if (id == R.id.actionbar_disconnect) {
            JniInterface.disconnectFromHost();
            return true;
        }
        if (id == R.id.actionbar_send_ctrl_alt_del) {
            int[] keys = {
                KeyEvent.KEYCODE_CTRL_LEFT,
                KeyEvent.KEYCODE_ALT_LEFT,
                KeyEvent.KEYCODE_FORWARD_DEL,
            };
            for (int key : keys) {
                JniInterface.sendKeyEvent(key, true);
            }
            for (int key : keys) {
                JniInterface.sendKeyEvent(key, false);
            }
            return true;
        }
        if (id == R.id.actionbar_help) {
            HelpActivity.launch(this, HELP_URL);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /**
     * Called once when a keyboard key is pressed, then again when that same key is released. This
     * is not guaranteed to be notified of all soft keyboard events: certian keyboards might not
     * call it at all, while others might skip it in certain situations (e.g. swipe input).
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();

        // Dispatch the back button to the system to handle navigation
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            return super.dispatchKeyEvent(event);
        }

        // Send TextEvent in two cases:
        //   1. This is an ACTION_MULTIPLE event.
        //   2. The event was generated by on-screen keyboard and Ctrl, Alt and
        //      Meta are not pressed.
        // This ensures that on-screen keyboard always injects input that
        // correspond to what user sees on the screen, while physical keyboard
        // acts as if it is connected to the remote host.
        if (event.getAction() == KeyEvent.ACTION_MULTIPLE) {
            JniInterface.sendTextEvent(event.getCharacters());
            return true;
        }

        boolean pressed = event.getAction() == KeyEvent.ACTION_DOWN;

        // For Enter getUnicodeChar() returns 10 (line feed), but we still
        // want to send it as KeyEvent.
        int unicode = keyCode != KeyEvent.KEYCODE_ENTER ? event.getUnicodeChar() : 0;

        boolean no_modifiers = !event.isAltPressed() &&
                               !event.isCtrlPressed() && !event.isMetaPressed();

        if (event.getDeviceId() == KeyCharacterMap.VIRTUAL_KEYBOARD &&
            pressed && unicode != 0 && no_modifiers) {
            mPressedTextKeys.add(keyCode);
            int[] codePoints = { unicode };
            JniInterface.sendTextEvent(new String(codePoints, 0, 1));
            return true;
        }

        if (!pressed && mPressedTextKeys.contains(keyCode)) {
            mPressedTextKeys.remove(keyCode);
            return true;
        }

        switch (keyCode) {
            case KeyEvent.KEYCODE_AT:
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_2, pressed);
                return true;

            case KeyEvent.KEYCODE_POUND:
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_3, pressed);
                return true;

            case KeyEvent.KEYCODE_STAR:
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_8, pressed);
                return true;

            case KeyEvent.KEYCODE_PLUS:
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(KeyEvent.KEYCODE_EQUALS, pressed);
                return true;

            default:
                // We try to send all other key codes to the host directly.
                return JniInterface.sendKeyEvent(keyCode, pressed);
        }
    }
}
