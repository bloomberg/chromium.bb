// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import android.app.ApplicationErrorReport;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Consumer;
import org.chromium.base.Function;
import org.chromium.base.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * StrictMode whitelist installer.
 *
 * <p><b>How this works:</b><br>
 * When StrictMode is enabled without the death penalty, it queues up all ThreadPolicy violations
 * into a ThreadLocal ArrayList, and then posts a Runnable to the start of the Looper queue to
 * process them. This is done in order to set a cap to the number of logged/handled violations per
 * event loop, and avoid overflowing the log buffer or other penalty handlers with violations. <br>
 * Because the violations are queued into a ThreadLocal ArrayList, they must be queued on the
 * offending thread, and thus the offending stack frame will exist in the stack trace. The
 * whitelisting mechanism works by using reflection to set a custom ArrayList into the ThreadLocal.
 * When StrictMode is adding a new item to the ArrayList, our custom ArrayList checks the stack
 * trace for any whitelisted frames, and if one is found, no-ops the addition. Then, when the
 * processing runnable executes, it sees there are no items, and no-ops. <br>
 * However, if the death penalty is enabled, the concern about handling too many violations no
 * longer exists (since death will occur after the first one), so the queue is bypassed, and death
 * occurs instantly without allowing the whitelisting system to intercept it. In order to retain the
 * death penalty, the whitelisting mechanism itself can be configured to execute the death penalty
 * after the first non-whitelisted violation.
 */
final class ReflectiveThreadStrictModeInterceptor implements ThreadStrictModeInterceptor {
    private static final String TAG = "ThreadStrictMode";

    @NonNull
    private final List<Function<Violation, Integer>> mWhitelistEntries;
    @Nullable
    private final Consumer mCustomPenalty;

    ReflectiveThreadStrictModeInterceptor(
            @NonNull List<Function<Violation, Integer>> whitelistEntries,
            @Nullable Consumer customPenalty) {
        mWhitelistEntries = whitelistEntries;
        mCustomPenalty = customPenalty;
    }

    @Override
    public void install(ThreadPolicy detectors) {
        interceptWithReflection();
        StrictMode.setThreadPolicy(new ThreadPolicy.Builder(detectors).penaltyLog().build());
    }

    private void interceptWithReflection() {
        ThreadLocal<ArrayList<Object>> violationsBeingTimed;
        try {
            violationsBeingTimed = getViolationsBeingTimed();
        } catch (Exception e) {
            throw new RuntimeException(null, e);
        }
        violationsBeingTimed.get().clear();
        violationsBeingTimed.set(new ArrayList<Object>() {
            @Override
            public boolean add(Object o) {
                int violationType = getViolationType(o);
                StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();
                Violation violation =
                        new Violation(violationType, Arrays.copyOf(stackTrace, stackTrace.length));
                if (violationType != Violation.DETECT_UNKNOWN
                        && violation.isInWhitelist(mWhitelistEntries)) {
                    return true;
                }
                if (mCustomPenalty != null) {
                    mCustomPenalty.accept(violation);
                }
                return super.add(o);
            }
        });
    }

    @SuppressWarnings({"unchecked"})
    private static ThreadLocal<ArrayList<Object>> getViolationsBeingTimed()
            throws IllegalAccessException, NoSuchFieldException {
        Field violationTimingField = StrictMode.class.getDeclaredField("violationsBeingTimed");
        violationTimingField.setAccessible(true);
        return (ThreadLocal<ArrayList<Object>>) violationTimingField.get(null);
    }

    /** @param o {@code android.os.StrictMode.ViolationInfo} */
    @SuppressWarnings({"unchecked", "PrivateApi"})
    private static int getViolationType(Object o) {
        try {
            Class<?> violationInfo = Class.forName("android.os.StrictMode$ViolationInfo");
            Field crashInfoField = violationInfo.getDeclaredField("crashInfo");
            crashInfoField.setAccessible(true);
            ApplicationErrorReport.CrashInfo crashInfo =
                    (ApplicationErrorReport.CrashInfo) crashInfoField.get(o);
            Method parseViolationFromMessage =
                    StrictMode.class.getDeclaredMethod("parseViolationFromMessage", String.class);
            parseViolationFromMessage.setAccessible(true);
            int mask = (int) parseViolationFromMessage.invoke(
                    null /* static */, crashInfo.exceptionMessage);
            return mask & Violation.DETECT_ALL_KNOWN;
        } catch (Exception e) {
            Log.e(TAG, "Unable to get violation.", e);
            return Violation.DETECT_UNKNOWN;
        }
    }
}
