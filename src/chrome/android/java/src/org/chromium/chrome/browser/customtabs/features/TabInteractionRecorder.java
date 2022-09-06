// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Locale;

/**
 * Class used to monitor interactions for the current custom tab. This class is created in native
 * and owned by C++ object. This class has the ability to record whether the current web content has
 * seen interaction when the tab is closing, as well as the timestamp when this happens.
 *
 * Note that this object's lifecycle is bounded to a {@link WebContents} but not a {@link Tab}. To
 * observe the first frame of tab load, this recorder has to attach to the web content before the
 * first navigation for the visible frame finishes, or a pre-rendered frame become active.
 * */
@JNINamespace("customtabs")
public class TabInteractionRecorder {
    private static final String TAG = "CctInteraction";

    private final long mNativeTabInteractionRecorder;

    // Do not instantiate in Java.
    private TabInteractionRecorder(long nativePtr) {
        mNativeTabInteractionRecorder = nativePtr;
    }

    @CalledByNative
    private static @Nullable TabInteractionRecorder create(long nativePtr) {
        if (nativePtr == 0) return null;
        return new TabInteractionRecorder(nativePtr);
    }

    /**
     * Get the TabInteractionRecorder that lives in the main web contents of the given tab.
     * Note that the object might be come stale if the web contents of the given tab is swapped
     * after this function is called.
     * */
    public static @Nullable TabInteractionRecorder getFromTab(Tab tab) {
        return TabInteractionRecorderJni.get().getFromTab(tab);
    }

    /**
     * Create a TabInteractionRecorder and start observing the web contents in the given tab. If an
     * observer already exists for the tab, do nothing.
     */
    public static void createForTab(Tab tab) {
        TabInteractionRecorderJni.get().createForTab(tab);
    }

    /**
     * Notify this recorder tab is being closed. Record whether this instance has seen any
     * interaction, and the timestamp when the tab is closed, into SharedPreferences.
     *
     * This class works correctly assuming there will be only one tab opened throughout the lifetime
     * of a given CCT session. If CCT ever changed into serving multiple tabs, this recorder will
     * only works for the last tab being closed.
     */
    public void onTabClosing() {
        long timestamp = SystemClock.uptimeMillis();
        boolean hadInteraction =
                TabInteractionRecorderJni.get().hadInteraction(mNativeTabInteractionRecorder);

        Log.d(TAG,
                String.format(Locale.US,
                        "timestamp=%d, TabInteractionRecorder.recordInteractions=%b", timestamp,
                        hadInteraction));

        SharedPreferencesManager pref = SharedPreferencesManager.getInstance();
        pref.writeLong(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP, timestamp);
        pref.writeBoolean(
                ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, hadInteraction);
        RecordHistogram.recordBooleanHistogram("CustomTabs.HadInteractionOnClose", hadInteraction);
    }

    /**
     *  Remove all the shared preferences related to tab interactions.
     */
    public static void resetTabInteractionRecords() {
        SharedPreferencesManager pref = SharedPreferencesManager.getInstance();
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP);
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION);
    }

    @NativeMethods
    interface Natives {
        TabInteractionRecorder getFromTab(Tab tab);
        TabInteractionRecorder createForTab(Tab tab);
        boolean hadInteraction(long nativeTabInteractionRecorderAndroid);
    }
}
