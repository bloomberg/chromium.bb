// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.ListPopupWindow;

import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * The dropdown list popup window.
 */
public class DropdownPopupWindow extends ListPopupWindow {

    private final Context mContext;
    private final ViewAndroidDelegate mViewAndroidDelegate;
    private final View mAnchorView;
    private float mAnchorWidth;
    private float mAnchorHeight;
    private float mAnchorX;
    private float mAnchorY;
    private OnLayoutChangeListener mLayoutChangeListener;

    /**
     * Creates an DropdownPopupWindow with specified parameters.
     * @param context Application context.
     * @param viewAndroidDelegate View delegate used to add and remove views.
     */
    public DropdownPopupWindow(Context context, ViewAndroidDelegate viewAndroidDelegate) {
        super(context, null, 0, R.style.DropdownPopupWindow);
        mContext = context;
        mViewAndroidDelegate = viewAndroidDelegate;

        mAnchorView = mViewAndroidDelegate.acquireAnchorView();
        mAnchorView.setId(R.id.dropdown_popup_window);
        mAnchorView.setTag(this);

        mLayoutChangeListener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (v == mAnchorView) DropdownPopupWindow.this.show();
            }
        };

        mAnchorView.addOnLayoutChangeListener(mLayoutChangeListener);
        setAnchorView(mAnchorView);
    }

    /**
     * Sets the location and the size of the anchor view that the DropdownPopupWindow will use to
     * attach itself.
     * @param x X coordinate of the top left corner of the anchor view.
     * @param y Y coordinate of the top left corner of the anchor view.
     * @param width The width of the anchor view.
     * @param height The height of the anchor view.
     */
    public void setAnchorRect(float x, float y, float width, float height) {
        mAnchorWidth = width;
        mAnchorHeight = height;
        mAnchorX = x;
        mAnchorY = y;
        if (mAnchorView != null) {
            mViewAndroidDelegate.setAnchorViewPosition(mAnchorView, mAnchorX, mAnchorY,
                    mAnchorWidth, mAnchorHeight);
        }
    }

    @Override
    public void show() {
        // An ugly hack to keep the popup from expanding on top of the keyboard.
        setInputMethodMode(INPUT_METHOD_NEEDED);
        super.show();
        getListView().setDividerHeight(0);
        int contentWidth = measureContentWidth();
        float contentWidthInDip = contentWidth /
                mContext.getResources().getDisplayMetrics().density;
        if (contentWidthInDip > mAnchorWidth) {
            setContentWidth(contentWidth);
        } else {
            setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        }
        mViewAndroidDelegate.setAnchorViewPosition(mAnchorView, mAnchorX, mAnchorY, mAnchorWidth,
                mAnchorHeight);
    }

    @Override
    public void dismiss() {
        super.dismiss();
        mAnchorView.removeOnLayoutChangeListener(mLayoutChangeListener);
        mAnchorView.setTag(null);
        mViewAndroidDelegate.releaseAnchorView(mAnchorView);
    }

    /**
     * Measures the width of the list content.
     * @return The popup window width in pixels.
     */
    private int measureContentWidth() {
        int maxWidth = 0;
        View itemView = null;
        final ListAdapter adapter = getListView().getAdapter();
        if (adapter == null)
          return 0;
        final int widthMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        final int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        for (int i = 0; i < adapter.getCount(); i++) {
            itemView = adapter.getView(i, itemView, null);
            LinearLayout.LayoutParams params =
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT);
            itemView.setLayoutParams(params);
            itemView.measure(widthMeasureSpec, heightMeasureSpec);
            maxWidth = Math.max(maxWidth, itemView.getMeasuredWidth());
        }
        return maxWidth;
    }
}
