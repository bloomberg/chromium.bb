// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;

/** A LinearLayout that can notify when its size changes. */
public class SizeListenableLinearLayout extends LinearLayout {
    @Nullable
    private BottomSheet.ContentSizeListener mListener;

    public SizeListenableLinearLayout(Context context) {
        super(context);
    }

    public SizeListenableLinearLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SizeListenableLinearLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (mListener != null) {
            mListener.onSizeChanged(w, h, oldw, oldh);
        }
    }

    void setContentSizeListener(@Nullable BottomSheet.ContentSizeListener listener) {
        mListener = listener;
    }
}
