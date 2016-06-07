// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * The parameter for an OnSoftInputMethodVisibilityChanged event.
 *
 * {@link android.graphics.Rect} is mutable, so this class owns four integers to represent the
 * rectangle of new layout.
 */
public final class SoftInputMethodVisibilityChangedEventParameter {
    public final boolean visible;
    public final int left;
    public final int top;
    public final int right;
    public final int bottom;

    public SoftInputMethodVisibilityChangedEventParameter(boolean visible,
                                                          int left,
                                                          int top,
                                                          int right,
                                                          int bottom) {
        this.visible = visible;
        this.left = left;
        this.top = top;
        this.right = right;
        this.bottom = bottom;
    }
}
