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
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.support.v7.app.ActionBar.OnMenuVisibilityListener;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.View.OnTouchListener;
import android.view.inputmethod.InputMethodManager;

import org.chromium.chromoting.cardboard.DesktopActivity;
import org.chromium.chromoting.help.HelpContext;
import org.chromium.chromoting.help.HelpSingleton;
import org.chromium.chromoting.jni.JniInterface;

import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * A simple screen that does nothing except display a DesktopView and notify it of rotations.
 */
public class Desktop
        extends AppCompatActivity implements View.OnSystemUiVisibilityChangeListener,
                                             CapabilityManager.CapabilitiesChangedListener {
    /** Used to set/store the selected input mode. */
    public enum InputMode {
        UNKNOWN,
        TRACKPAD,
        TOUCH;

        public boolean isSet() {
            return this != UNKNOWN;
        }
    }

    /**
     * Preference used for displaying an interestitial dialog only when the user first accesses the
     * Cardboard function.
     */
    private static final String PREFERENCE_CARDBOARD_DIALOG_SEEN = "cardboard_dialog_seen";

    /** Preference used to track the last input mode selected by the user. */
    private static final String PREFERENCE_INPUT_MODE = "input_mode";

    /** The amount of time to wait to hide the Actionbar after user input is seen. */
    private static final int ACTIONBAR_AUTO_HIDE_DELAY_MS = 3000;

    /** The surface that displays the remote host's desktop feed. */
    private DesktopView mRemoteHostDesktop;

    /** Set of pressed keys for which we've sent TextEvent. */
    private Set<Integer> mPressedTextKeys = new TreeSet<Integer>();

    private ActivityLifecycleListener mActivityLifecycleListener;

    /** Flag to indicate whether the current activity is switching to Cardboard desktop activity. */
    private boolean mSwitchToCardboardDesktopActivity;

    /** Flag to indicate whether to manually hide the system UI when the OSK is dismissed. */
    private boolean mHideSystemUIOnSoftKeyboardDismiss = false;

    /** Indicates whether a Soft Input UI (such as a keyboard) is visible. */
    private boolean mSoftInputVisible = false;

    /** Holds the scheduled task object which will be called to hide the ActionBar. */
    private Runnable mActionBarAutoHideTask;

    /** The Toolbar instance backing our SupportActionBar. */
    private Toolbar mToolbar;

    /** Tracks the current input mode (e.g. trackpad/touch). */
    private InputMode mInputMode = InputMode.UNKNOWN;

    /** Indicates whether the remote host supports touch injection. */
    private CapabilityManager.HostCapability mHostTouchCapability =
            CapabilityManager.HostCapability.UNKNOWN;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.desktop);

        mToolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(mToolbar);

        mRemoteHostDesktop = (DesktopView) findViewById(R.id.desktop_view);
        mRemoteHostDesktop.setDesktop(this);
        mSwitchToCardboardDesktopActivity = false;

        getSupportActionBar().setDisplayShowTitleEnabled(false);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        // For this Activity, the home button in the action bar acts as a Disconnect button, so
        // set the description for accessibility/screen readers.
        getSupportActionBar().setHomeActionContentDescription(R.string.disconnect_myself_button);

        // The action bar is already shown when the activity is started however calling the
        // function below will set our preferred system UI flags which will adjust the layout
        // size of the canvas and we can avoid an initial resize event.
        showActionBar();

        View decorView = getWindow().getDecorView();
        decorView.setOnSystemUiVisibilityChangeListener(this);

        mActivityLifecycleListener = CapabilityManager.getInstance().onActivityAcceptingListener(
                this, Capabilities.CAST_CAPABILITY);
        mActivityLifecycleListener.onActivityCreated(this, savedInstanceState);

        mInputMode = getInitialInputModeValue();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            attachKeyboardVisibilityListener();

            // Only create an Autohide task if the system supports immersive fullscreen mode.  Older
            // versions of the OS benefit less from this functionality and we don't want to change
            // the experience for them.
            mActionBarAutoHideTask = new Runnable() {
                public void run() {
                    if (!mToolbar.isOverflowMenuShowing()) {
                        hideActionBar();
                    }
                }
            };

            // Suspend the ActionBar timer when the user interacts with the options menu.
            getSupportActionBar().addOnMenuVisibilityListener(new OnMenuVisibilityListener() {
                public void onMenuVisibilityChanged(boolean isVisible) {
                    if (isVisible) {
                        stopActionBarAutoHideTimer();
                    } else {
                        startActionBarAutoHideTimer();
                    }
                }
            });
        } else {
            mRemoteHostDesktop.setFitsSystemWindows(true);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        mActivityLifecycleListener.onActivityStarted(this);
        JniInterface.enableVideoChannel(true);
        mRemoteHostDesktop.attachRedrawCallback();
        CapabilityManager.getInstance().addListener(this);
    }

    @Override
    protected void onPause() {
        if (isFinishing()) mActivityLifecycleListener.onActivityPaused(this);
        super.onPause();
        if (!mSwitchToCardboardDesktopActivity) {
            JniInterface.enableVideoChannel(false);
        }
        stopActionBarAutoHideTimer();
    }

    @Override
    public void onResume() {
        super.onResume();
        mActivityLifecycleListener.onActivityResumed(this);
        JniInterface.enableVideoChannel(true);
        startActionBarAutoHideTimer();
    }

    @Override
    protected void onStop() {
        CapabilityManager.getInstance().removeListener(this);
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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            // We don't need to show a hide ActionBar button if immersive fullscreen is supported.
            menu.findItem(R.id.actionbar_hide).setVisible(false);

            // Although the MenuItems are being created here, they do not have any backing Views yet
            // as those are created just after this method exits.  We post an async task to the UI
            // thread here so that we can attach our interaction listeners shortly after the views
            // have been created.
            final Menu menuFinal = menu;
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    // Attach a listener to the toolbar itself then attach one to each menu item
                    // which has a backing view object.
                    attachToolbarInteractionListenerToView(mToolbar);
                    int items = menuFinal.size();
                    for (int i = 0; i < items; i++) {
                        int itemId = menuFinal.getItem(i).getItemId();
                        View menuItemView = findViewById(itemId);
                        if (menuItemView != null) {
                            attachToolbarInteractionListenerToView(menuItemView);
                        }
                    }
                }
            });
        }

        ChromotingUtil.tintMenuIcons(this, menu);

        // Wait to set the input mode until after the default tinting has been applied.
        setInputMode(mInputMode);

        return super.onCreateOptionsMenu(menu);
    }

    private InputMode getInitialInputModeValue() {
        // Load the previously-selected input mode from Preferences.
        // TODO(joedow): Evaluate and determine if we should use a different input mode based on
        //               a device characteristic such as screen size.
        InputMode inputMode = InputMode.TRACKPAD;
        String previousInputMode =
                getPreferences(MODE_PRIVATE)
                        .getString(PREFERENCE_INPUT_MODE, inputMode.name());

        try {
            inputMode = InputMode.valueOf(previousInputMode);
        } catch (IllegalArgumentException ex) {
            // Invalid or unexpected value was found, just use the default mode.
        }

        return inputMode;
    }

    private void setInputMode(InputMode inputMode) {
        Menu menu = mToolbar.getMenu();
        MenuItem trackpadModeMenuItem = menu.findItem(R.id.actionbar_trackpad_mode);
        MenuItem touchModeMenuItem = menu.findItem(R.id.actionbar_touch_mode);
        if (inputMode == InputMode.TRACKPAD) {
            trackpadModeMenuItem.setVisible(true);
            touchModeMenuItem.setVisible(false);
        } else if (inputMode == InputMode.TOUCH) {
            touchModeMenuItem.setVisible(true);
            trackpadModeMenuItem.setVisible(false);
        } else {
            assert false : "Unreached";
            return;
        }

        mInputMode = inputMode;
        getPreferences(MODE_PRIVATE)
                .edit()
                .putString(PREFERENCE_INPUT_MODE, mInputMode.name())
                .apply();

        mRemoteHostDesktop.changeInputMode(mInputMode, mHostTouchCapability);
    }

    @Override
    public void onCapabilitiesChanged(List<String> newCapabilities) {
        if (newCapabilities.contains(Capabilities.TOUCH_CAPABILITY)) {
            mHostTouchCapability = CapabilityManager.HostCapability.SUPPORTED;
        } else {
            mHostTouchCapability = CapabilityManager.HostCapability.UNSUPPORTED;
        }

        mRemoteHostDesktop.changeInputMode(mInputMode, mHostTouchCapability);
    }

    // Any time an onTouchListener is attached, a lint warning about filtering touch events is
    // generated.  Since the function below is only used to listen to, not intercept, the events,
    // the lint warning can be safely suppressed.
    @SuppressLint("ClickableViewAccessibility")
    private void attachToolbarInteractionListenerToView(View view) {
        view.setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        stopActionBarAutoHideTimer();
                        break;

                    case MotionEvent.ACTION_UP:
                        startActionBarAutoHideTimer();
                        break;

                    default:
                        // Ignore.
                        break;
                }

                return false;
            }
        });
    }

    // Posts a deplayed task to hide the ActionBar.  If an existing task has already been scheduled,
    // then the previous task is removed and the new one scheduled, effectively resetting the timer.
    private void startActionBarAutoHideTimer() {
        if (mActionBarAutoHideTask != null) {
            stopActionBarAutoHideTimer();
            getWindow().getDecorView().postDelayed(
                    mActionBarAutoHideTask, ACTIONBAR_AUTO_HIDE_DELAY_MS);
        }
    }

    // Clear all existing delayed tasks to prevent the ActionBar from being hidden.
    private void stopActionBarAutoHideTimer() {
        if (mActionBarAutoHideTask != null) {
            getWindow().getDecorView().removeCallbacks(mActionBarAutoHideTask);
        }
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
        int fullscreenFlags = getFullscreenFlags();
        if ((visibility & fullscreenFlags) != 0) {
            hideActionBarWithoutSystemUi();
        } else {
            showActionBarWithoutSystemUi();
        }
    }

    @SuppressLint("InlinedApi")
    private static int getFullscreenFlags() {
        int flags = View.SYSTEM_UI_FLAG_LOW_PROFILE;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            flags |= View.SYSTEM_UI_FLAG_FULLSCREEN;
        }
        return flags;
    }

    @SuppressLint("InlinedApi")
    private static int getImmersiveLayoutFlags() {
        int flags = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
        }
        return flags;
    }

    public void showActionBar() {
        mHideSystemUIOnSoftKeyboardDismiss = false;

        // Request exit from any fullscreen mode. The action-bar controls will be shown in response
        // to the SystemUiVisibility notification. The visibility of the action-bar should be tied
        // to the fullscreen state of the system, so there's no need to explicitly show it here.
        int flags = View.SYSTEM_UI_FLAG_VISIBLE | getImmersiveLayoutFlags();
        getWindow().getDecorView().setSystemUiVisibility(flags);

        // The OS will not call onSystemUiVisibilityChange() if the keyboard is visible which means
        // our ActionBar will not be visible until then.  This check allows us to work around this
        // issue and still allow the system to show the ActionBar normally with the soft keyboard.
        if (mSoftInputVisible) {
            showActionBarWithoutSystemUi();
        }
    }

    /** Shows the action bar without changing SystemUiVisibility. */
    private void showActionBarWithoutSystemUi() {
        getSupportActionBar().show();
        startActionBarAutoHideTimer();
    }

    @SuppressLint("InlinedApi")
    public void hideActionBar() {
        // Request the device to enter fullscreen mode. Don't hide the controls yet, because the
        // system might not honor the fullscreen request immediately (for example, if the
        // keyboard is visible, the system might delay fullscreen until the keyboard is hidden).
        // The controls will be hidden in response to the SystemUiVisibility notification.
        // This helps ensure that the visibility of the controls is synchronized with the
        // fullscreen state.

        // LOW_PROFILE gives the status and navigation bars a "lights-out" appearance.
        // FULLSCREEN hides the status bar on supported devices (4.1 and above).
        int flags = getFullscreenFlags();

        // HIDE_NAVIGATION hides the navigation bar. However, if the user touches the screen, the
        // event is not seen by the application and instead the navigation bar is re-shown.
        // IMMERSIVE(_STICKY) fixes this problem and allows the user to interact with the app while
        // keeping the navigation controls hidden. This flag was introduced in 4.4, later than
        // HIDE_NAVIGATION, and so a runtime check is needed before setting either of these flags.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            flags |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            flags |= View.SYSTEM_UI_FLAG_IMMERSIVE;
            flags |= getImmersiveLayoutFlags();
        }

        getWindow().getDecorView().setSystemUiVisibility(flags);

        // The OS will not call onSystemUiVisibilityChange() until the keyboard has been dismissed
        // which means our ActionBar will still be visible.  This check allows us to work around
        // this issue when the keyboard is visible and the user wants additional space on the screen
        // and still allow the system to hide the ActionBar normally when no keyboard is present.
        if (mSoftInputVisible) {
            hideActionBarWithoutSystemUi();

            // Android OSes prior to Marshmallow do not call onSystemUiVisibilityChange after the
            // OSK is dismissed if the user has interacted with the status bar.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                mHideSystemUIOnSoftKeyboardDismiss = true;
            }
        }
    }

    /** Hides the action bar without changing SystemUiVisibility. */
    private void hideActionBarWithoutSystemUi() {
        getSupportActionBar().hide();
        stopActionBarAutoHideTimer();
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
        if (id == R.id.actionbar_trackpad_mode) {
            // When the trackpad icon is tapped, we want to switch the input mode to touch.
            setInputMode(InputMode.TOUCH);
            return true;
        }
        if (id == R.id.actionbar_touch_mode) {
            // When the touch icon is tapped, we want to switch the input mode to trackpad.
            setInputMode(InputMode.TRACKPAD);
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
            HelpSingleton.getInstance().launchHelp(this, HelpContext.DESKTOP);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void attachKeyboardVisibilityListener() {
        View keyboardVisibilityDetector = findViewById(R.id.resize_detector);
        keyboardVisibilityDetector.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            // Tracks the maximum 'bottom' value seen during layout changes.  This value represents
            // the top of the SystemUI displayed at the bottom of the screen.
            // Note: This value is a screen coordinate so a larger value means lower on the screen.
            private int mMaxBottomValue;

            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // As the activity is started, a number of layout changes will flow through.  If
                // this is a fresh start, then we will see one layout change which will represent
                // the steady state of the UI and will include an accurate 'bottom' value.  If we
                // are transitioning from another activity/orientation, then there may be several
                // layout change events as the view is updated (i.e. the OSK might have been
                // displayed previously but is being dismissed).  Therefore we want to track the
                // largest value we have seen and use it to determine if a new system UI (such as
                // the OSK) is being displayed.
                if (mMaxBottomValue < bottom) {
                    mMaxBottomValue = bottom;
                    return;
                }

                // If the delta between lowest bound we have seen (should be a systemUI such as
                // the navigation bar) and the current bound does not match, then we have a form
                // of soft input displayed.  Note that the size of a soft input device can change
                // when the input method is changed so we want to send updates to the image canvas
                // whenever they occur.
                mSoftInputVisible = (bottom < mMaxBottomValue);
                mRemoteHostDesktop.onSoftInputMethodVisibilityChanged(
                        mSoftInputVisible, new Rect(left, top, right, bottom));

                if (!mSoftInputVisible && mHideSystemUIOnSoftKeyboardDismiss) {
                    // Queue a task which will run after the current action (OSK dismiss) has
                    // completed, otherwise the hide request will not take effect.
                    new Handler().post(new Runnable() {
                        @Override
                        public void run() {
                            if (mHideSystemUIOnSoftKeyboardDismiss) {
                                mHideSystemUIOnSoftKeyboardDismiss = false;
                                hideActionBar();
                            }
                        }
                    });
                }
            }
        });
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
