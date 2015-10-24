// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import org.chromium.chromoting.cardboard.DesktopActivity;
import org.chromium.chromoting.jni.JniInterface;

import java.util.Set;
import java.util.TreeSet;

/**
 * A simple screen that does nothing except display a DesktopView and notify it of rotations.
 */
public class Desktop extends AppCompatActivity implements View.OnSystemUiVisibilityChangeListener {
    /** Web page to be displayed in the Help screen when launched from this activity. */
    private static final String HELP_URL =
            "https://support.google.com/chrome/?p=mobile_crd_connecthost";

    /**
     * Preference used for displaying an interestitial dialog only when the user first accesses the
     * Cardboard function.
     */
    private static final String PREFERENCE_CARDBOARD_DIALOG_SEEN =
            "cardboard_dialog_seen";

    /** The surface that displays the remote host's desktop feed. */
    private DesktopView mRemoteHostDesktop;

    /** Set of pressed keys for which we've sent TextEvent. */
    private Set<Integer> mPressedTextKeys = new TreeSet<Integer>();

    private ActivityLifecycleListener mActivityLifecycleListener;

    // Flag to indicate whether the current activity is going to switch to Cardboard
    // desktop activity.
    private boolean mSwitchToCardboardDesktopActivity;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.desktop);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        mRemoteHostDesktop = (DesktopView) findViewById(R.id.desktop_view);
        mRemoteHostDesktop.setDesktop(this);
        mSwitchToCardboardDesktopActivity = false;

        getSupportActionBar().setDisplayShowTitleEnabled(false);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        // For this Activity, the home button in the action bar acts as a Disconnect button, so
        // set the description for accessibility/screen readers.
        getSupportActionBar().setHomeActionContentDescription(R.string.disconnect_myself_button);

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
        JniInterface.enableVideoChannel(true);
        mRemoteHostDesktop.attachRedrawCallback();
    }

    @Override
    protected void onPause() {
        if (isFinishing()) mActivityLifecycleListener.onActivityPaused(this);
        super.onPause();
        if (!mSwitchToCardboardDesktopActivity) {
            JniInterface.enableVideoChannel(false);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mActivityLifecycleListener.onActivityResumed(this);
        JniInterface.enableVideoChannel(true);
    }

    @Override
    protected void onStop() {
        mActivityLifecycleListener.onActivityStopped(this);
        super.onStop();
        if (mSwitchToCardboardDesktopActivity) {
            mSwitchToCardboardDesktopActivity = false;
        } else {
            JniInterface.enableVideoChannel(false);
        }
    }

    /** Called to initialize the action bar. */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.desktop_actionbar, menu);

        mActivityLifecycleListener.onActivityCreatedOptionsMenu(this, menu);

        boolean enableCardboard = false;
        try {
            ApplicationInfo ai = getPackageManager()
                    .getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
            Bundle bundle = ai.metaData;
            enableCardboard = bundle.getInt("enable_cardboard") == 1;
        } catch (NameNotFoundException e) {
            // Does nothing since by default Cardboard activity is turned off.
        }

        MenuItem item = menu.findItem(R.id.actionbar_cardboard);
        item.setVisible(enableCardboard);

        ChromotingUtil.tintMenuIcons(this, menu);

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
            hideActionBarWithoutSystemUi();
        } else {
            showActionBarWithoutSystemUi();
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
        // Request exit from any fullscreen mode. The action-bar controls will be shown in response
        // to the SystemUiVisibility notification. The visibility of the action-bar should be tied
        // to the fullscreen state of the system, so there's no need to explicitly show it here.
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
    }

    /** Shows the action bar without changing SystemUiVisibility. */
    private void showActionBarWithoutSystemUi() {
        getSupportActionBar().show();
    }

    @SuppressLint("InlinedApi")
    public void hideActionBar() {
        // Request the device to enter fullscreen mode. Don't hide the controls yet, because the
        // system might not honor the fullscreen request immediately (for example, if the
        // keyboard is visible, the system might delay fullscreen until the keyboard is hidden).
        // The controls will be hidden in response to the SystemUiVisibility notification.
        // This helps ensure that the visibility of the controls is synchronized with the
        // fullscreen state.
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
            flags |= (View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE);
        }

        decorView.setSystemUiVisibility(flags);
    }

    /** Hides the action bar without changing SystemUiVisibility. */
    private void hideActionBarWithoutSystemUi() {
        getSupportActionBar().hide();
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();

        mActivityLifecycleListener.onActivityOptionsItemSelected(this, item);

        if (id == R.id.actionbar_cardboard) {
            onCardboardItemSelected();
            return true;
        }
        if (id == R.id.actionbar_keyboard) {
            ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE)).toggleSoftInput(0, 0);
            return true;
        }
        if (id == R.id.actionbar_hide) {
            hideActionBar();
            return true;
        }
        if (id == R.id.actionbar_disconnect || id == android.R.id.home) {
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
                JniInterface.sendKeyEvent(0, key, true);
            }
            for (int key : keys) {
                JniInterface.sendKeyEvent(0, key, false);
            }
            return true;
        }
        if (id == R.id.actionbar_help) {
            HelpActivity.launch(this, HELP_URL);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void onCardboardItemSelected() {
        if (getPreferences(MODE_PRIVATE).getBoolean(PREFERENCE_CARDBOARD_DIALOG_SEEN, false)) {
            switchToCardboardMode();
            return;
        }

        new AlertDialog.Builder(this)
                .setTitle(getTitle())
                .setMessage(R.string.cardboard_warning_message)
                .setIcon(R.drawable.ic_cardboard)
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        getPreferences(MODE_PRIVATE)
                                .edit()
                                .putBoolean(PREFERENCE_CARDBOARD_DIALOG_SEEN, true)
                                .apply();
                        switchToCardboardMode();
                    }
                })
                .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                    }
                })
                .create()
                .show();
    }

    private void switchToCardboardMode() {
        mSwitchToCardboardDesktopActivity = true;
        Intent intent = new Intent(this, DesktopActivity.class);
        startActivityForResult(intent, Chromoting.CARDBOARD_DESKTOP_ACTIVITY);
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
            JniInterface.disconnectFromHost();
            return super.dispatchKeyEvent(event);
        }

        boolean pressed = event.getAction() == KeyEvent.ACTION_DOWN;

        // Physical keyboard must work as if it is connected to the remote host
        // and so events coming from physical keyboard never generate text
        // events. Also scan codes must be used instead of key code, so that
        // the keyboard layout selected on the client doesn't affect the key
        // codes sent to the host.
        if (event.getDeviceId() != KeyCharacterMap.VIRTUAL_KEYBOARD) {
            return JniInterface.sendKeyEvent(event.getScanCode(), 0, pressed);
        }

        // Events received from software keyboards generate TextEvent in two
        // cases:
        //   1. This is an ACTION_MULTIPLE event.
        //   2. Ctrl, Alt and Meta are not pressed.
        // This ensures that on-screen keyboard always injects input that
        // correspond to what user sees on the screen, while physical keyboard
        // acts as if it is connected to the remote host.
        if (event.getAction() == KeyEvent.ACTION_MULTIPLE) {
            JniInterface.sendTextEvent(event.getCharacters());
            return true;
        }

        // For Enter getUnicodeChar() returns 10 (line feed), but we still
        // want to send it as KeyEvent.
        int unicode = keyCode != KeyEvent.KEYCODE_ENTER ? event.getUnicodeChar() : 0;

        boolean no_modifiers = !event.isAltPressed()
                && !event.isCtrlPressed() && !event.isMetaPressed();

        if (pressed && unicode != 0 && no_modifiers) {
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
            // KEYCODE_AT, KEYCODE_POUND, KEYCODE_STAR and KEYCODE_PLUS are
            // deprecated, but they still need to be here for older devices and
            // third-party keyboards that may still generate these events. See
            // https://source.android.com/devices/input/keyboard-devices.html#legacy-unsupported-keys
            case KeyEvent.KEYCODE_AT:
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_2, pressed);
                return true;

            case KeyEvent.KEYCODE_POUND:
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_3, pressed);
                return true;

            case KeyEvent.KEYCODE_STAR:
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_8, pressed);
                return true;

            case KeyEvent.KEYCODE_PLUS:
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                JniInterface.sendKeyEvent(0, KeyEvent.KEYCODE_EQUALS, pressed);
                return true;

            default:
                // We try to send all other key codes to the host directly.
                return JniInterface.sendKeyEvent(0, keyCode, pressed);
        }
    }
}
