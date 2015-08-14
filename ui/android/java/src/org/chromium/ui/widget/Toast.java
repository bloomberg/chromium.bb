// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.SysUtils;

/**
 * Toast wrapper, makes sure toasts don't trigger HW acceleration when created
 * from activities that are not HW accelerated.
 *
 * Can (and should) also be used for Chromium-related additions and extensions.
 */
public class Toast {

    public static final int LENGTH_SHORT = android.widget.Toast.LENGTH_SHORT;
    public static final int LENGTH_LONG = android.widget.Toast.LENGTH_LONG;

    private android.widget.Toast mToast;
    private ViewGroup mSWLayout;

    public Toast(Context context) {
        this(context, new android.widget.Toast(context));
    }

    private Toast(Context context, android.widget.Toast toast) {
        mToast = toast;

        if (SysUtils.isLowEndDevice()
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                && context.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.LOLLIPOP
                && isHWAccelerationDisabled(context)) {
            // Don't HW accelerate Toasts. Unfortunately the only way to do it is to make
            // toast.getView().getContext().getApplicationInfo().targetSdkVersion return
            // something less than LOLLIPOP (see WindowManagerGlobal.addView).
            mSWLayout = new FrameLayout(new ContextWrapper(context) {
                @Override
                public ApplicationInfo getApplicationInfo() {
                    ApplicationInfo info = new ApplicationInfo(super.getApplicationInfo());
                    info.targetSdkVersion = Build.VERSION_CODES.KITKAT;
                    return info;
                }
            });

            setView(toast.getView());
        }
    }

    public android.widget.Toast getAndroidToast() {
        return mToast;
    }

    public void show() {
        mToast.show();
    }

    public void cancel() {
        mToast.cancel();
    }

    public void setView(View view) {
        if (mSWLayout != null) {
            mSWLayout.removeAllViews();
            if (view != null) {
                mSWLayout.addView(view, WRAP_CONTENT, WRAP_CONTENT);
                mToast.setView(mSWLayout);
            } else {
                // When null view is set we propagate it to the toast to trigger appropriate
                // handling (for example show() throws an exception when view is null).
                mToast.setView(null);
            }
        } else {
            mToast.setView(view);
        }
    }

    public View getView() {
        if (mToast.getView() == null) {
            return null;
        }
        if (mSWLayout != null) {
            return mSWLayout.getChildAt(0);
        } else {
            return mToast.getView();
        }
    }

    public void setDuration(int duration) {
        mToast.setDuration(duration);
    }

    public int getDuration() {
        return mToast.getDuration();
    }

    public void setMargin(float horizontalMargin, float verticalMargin) {
        mToast.setMargin(horizontalMargin, verticalMargin);
    }

    public float getHorizontalMargin() {
        return mToast.getHorizontalMargin();
    }

    public float getVerticalMargin() {
        return mToast.getVerticalMargin();
    }

    public void setGravity(int gravity, int xOffset, int yOffset) {
        mToast.setGravity(gravity, xOffset, yOffset);
    }

    public int getGravity() {
        return mToast.getGravity();
    }

    public int getXOffset() {
        return mToast.getXOffset();
    }

    public int getYOffset() {
        return mToast.getYOffset();
    }

    public void setText(int resId) {
        mToast.setText(resId);
    }

    public void setText(CharSequence s) {
        mToast.setText(s);
    }

    @SuppressLint("ShowToast")
    public static Toast makeText(Context context, CharSequence text, int duration) {
        return new Toast(context, android.widget.Toast.makeText(context, text, duration));
    }

    @SuppressLint("ShowToast")
    public static Toast makeText(Context context, int resId, int duration)
            throws Resources.NotFoundException {
        return new Toast(context, android.widget.Toast.makeText(context, resId, duration));
    }

    private static Activity getActivity(Context context) {
        while (context != null) {
            if (context instanceof Activity) {
                return (Activity) context;
            }
            if (!(context instanceof ContextWrapper)) {
                break;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
    }

    private static boolean isHWAccelerationDisabled(Context context) {
        Activity activity = getActivity(context);
        if (activity == null) {
            return false;
        }
        try {
            ActivityInfo info = activity.getPackageManager().getActivityInfo(
                    activity.getComponentName(), 0);
            return (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) == 0;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }
}
