// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(sky): this is a forked copy of that from src/chrome, refactor and share.

package org.chromium.weblayer_private;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.List;

/**
 * Exposes information about the current accessibility state.
 */
public class AccessibilityUtil {
    /**
     * An observer to be notified of accessibility status changes.
     */
    public interface Observer {
        /**
         * @param enabled Whether a touch exploration or an accessibility service that performs can
         *        perform gestures is enabled. Indicates that the UI must be fully navigable using
         *        the accessibility view tree.
         */
        void onAccessibilityModeChanged(boolean enabled);
    }

    private Boolean mIsAccessibilityEnabled;
    private ObserverList<Observer> mObservers;
    private final class ModeChangeHandler
            implements AccessibilityStateChangeListener, TouchExplorationStateChangeListener {
        // AccessibilityStateChangeListener

        @Override
        public final void onAccessibilityStateChanged(boolean enabled) {
            updateIsAccessibilityEnabledAndNotify();
        }

        // TouchExplorationStateChangeListener

        @Override
        public void onTouchExplorationStateChanged(boolean enabled) {
            updateIsAccessibilityEnabledAndNotify();
        }
    }

    private ModeChangeHandler mModeChangeHandler;

    protected AccessibilityUtil() {}

    /**
     * Checks to see that this device has accessibility and touch exploration enabled.
     * @return        Whether or not accessibility and touch exploration are enabled.
     */
    public boolean isAccessibilityEnabled() {
        if (mModeChangeHandler == null) registerModeChangeListeners();
        if (mIsAccessibilityEnabled != null) return mIsAccessibilityEnabled;

        TraceEvent.begin("AccessibilityManager::isAccessibilityEnabled");

        AccessibilityManager manager = getAccessibilityManager();
        boolean accessibilityEnabled =
                manager != null && manager.isEnabled() && manager.isTouchExplorationEnabled();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && manager != null
                && manager.isEnabled() && !accessibilityEnabled) {
            List<AccessibilityServiceInfo> services = manager.getEnabledAccessibilityServiceList(
                    AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
            for (AccessibilityServiceInfo service : services) {
                if (canPerformGestures(service)) {
                    accessibilityEnabled = true;
                    break;
                }
            }
        }

        mIsAccessibilityEnabled = accessibilityEnabled;

        TraceEvent.end("AccessibilityManager::isAccessibilityEnabled");
        return mIsAccessibilityEnabled;
    }

    /**
     * Add {@link Observer} object. The observer will be notified of the current accessibility
     * mode immediately.
     * @param observer Observer object monitoring a11y mode change.
     */
    public void addObserver(Observer observer) {
        getObservers().addObserver(observer);

        // Notify mode change to a new observer so things are initialized correctly when Chrome
        // has been re-started after closing due to the last tab being closed when homepage is
        // enabled. See crbug.com/541546.
        observer.onAccessibilityModeChanged(isAccessibilityEnabled());
    }

    /**
     * Remove {@link Observer} object.
     * @param observer Observer object monitoring a11y mode change.
     */
    public void removeObserver(Observer observer) {
        getObservers().removeObserver(observer);
    }

    /**
     * @return True if a hardware keyboard is detected.
     */
    public static boolean isHardwareKeyboardAttached(Configuration c) {
        return c.keyboard != Configuration.KEYBOARD_NOKEYS;
    }

    private AccessibilityManager getAccessibilityManager() {
        return (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                Context.ACCESSIBILITY_SERVICE);
    }

    private void registerModeChangeListeners() {
        assert mModeChangeHandler == null;
        mModeChangeHandler = new ModeChangeHandler();
        AccessibilityManager manager = getAccessibilityManager();
        manager.addAccessibilityStateChangeListener(mModeChangeHandler);
        manager.addTouchExplorationStateChangeListener(mModeChangeHandler);
    }

    /**
     * Removes all global state tracking observers/listeners as well as any observers added to this.
     * As this removes all observers, be very careful in calling. In general, only call when the
     * application is going to be destroyed.
     */
    protected void stopTrackingStateAndRemoveObservers() {
        if (mObservers != null) mObservers.clear();
        if (mModeChangeHandler == null) return;
        AccessibilityManager manager = getAccessibilityManager();
        manager.removeAccessibilityStateChangeListener(mModeChangeHandler);
        manager.removeTouchExplorationStateChangeListener(mModeChangeHandler);
    }

    /**
     * Forces recalculating the value of isAccessibilityEnabled(). If the value has changed observer
     * are notified.
     */
    protected void updateIsAccessibilityEnabledAndNotify() {
        boolean oldIsAccessibilityEnabled = isAccessibilityEnabled();
        // Setting to null forces the next call to isAccessibilityEnabled() to update the value.
        mIsAccessibilityEnabled = null;
        if (oldIsAccessibilityEnabled != isAccessibilityEnabled()) notifyModeChange();
    }

    private ObserverList<Observer> getObservers() {
        if (mObservers == null) mObservers = new ObserverList<>();
        return mObservers;
    }

    /**
     * Notify all the observers of the mode change.
     */
    private void notifyModeChange() {
        boolean enabled = isAccessibilityEnabled();
        for (Observer observer : getObservers()) {
            observer.onAccessibilityModeChanged(enabled);
        }
    }

    /**
     * Checks whether the given {@link AccessibilityServiceInfo} can perform gestures.
     * @param service The service to check.
     * @return Whether the {@code service} can perform gestures. On N+, this relies on the
     *         capabilities the service can perform. On L & M, this looks specifically for
     *         Switch Access.
     */
    private boolean canPerformGestures(AccessibilityServiceInfo service) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return (service.getCapabilities()
                           & AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES)
                    != 0;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return service.getResolveInfo() != null
                    && service.getResolveInfo().toString().contains("switchaccess");
        }
        return false;
    }

    /**
     * Set whether the device has accessibility enabled. Should be reset back to null after the test
     * has finished.
     * @param isEnabled whether the device has accessibility enabled.
     */
    @VisibleForTesting
    public void setAccessibilityEnabledForTesting(@Nullable Boolean isEnabled) {
        mIsAccessibilityEnabled = isEnabled;
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, this::notifyModeChange);
    }
}
