// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.base.ViewUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * <p>
 * Manages a group of exclusive RadioButtonWithDescriptions, automatically inserting a margin in
 * between the rows to prevent them from squishing together. Has the option to set an accessory view
 * on any given RadioButtonWithDescription. Only one accessory view per layout is supported.
 * <pre>
 * -------------------------------------------------
 * | O | MESSAGE #1                                |
 *        description_1                            |
 *        [optional] accessory view                |
 * | O | MESSAGE #N                                |
 *        description_n                            |
 * -------------------------------------------------
 * </pre>
 * </p>
 *
 * <p>
 * To declare in XML, define a RadioButtonWithDescriptionLayout that contains
 * RadioButtonWithDescription and/or RadioButtonWithEditText children.
 * For example:
 * <pre>{@code
 *  <org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout
 *       android:layout_width="match_parent"
 *       android:layout_height="match_parent" >
 *       <org.chromium.components.browser_ui.widget.RadioButtonWithDescription
 *           ... />
 *       <org.chromium.components.browser_ui.widget.RadioButtonWithEditText
 *           ... />
 *   </org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout>
 * }</pre>
 * </p>
 */
public final class RadioButtonWithDescriptionLayout
        extends RadioGroup implements RadioButtonWithDescription.ButtonCheckedStateChangedListener {
    private final int mMarginBetweenRows;
    private final List<RadioButtonWithDescription> mRadioButtonsWithDescriptions;
    private OnCheckedChangeListener mOnCheckedChangeListener;

    public RadioButtonWithDescriptionLayout(Context context) {
        this(context, null);
    }

    public RadioButtonWithDescriptionLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMarginBetweenRows = context.getResources().getDimensionPixelSize(
                R.dimen.default_vertical_margin_between_items);

        mRadioButtonsWithDescriptions = new ArrayList<>();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) getChildAt(i);
            setupButton(b);
        }

        updateMargins();
    }

    /**
     * @see RadioGroup.OnCheckedChangeListener
     */
    @Override
    public void setOnCheckedChangeListener(OnCheckedChangeListener onCheckedChangeListener) {
        mOnCheckedChangeListener = onCheckedChangeListener;
    }

    /**
     * @see RadioButtonWithDescription.ButtonCheckedStateChangedListener
     */
    @Override
    public void onButtonCheckedStateChanged(RadioButtonWithDescription checkedRadioButton) {
        mOnCheckedChangeListener.onCheckedChanged(this, checkedRadioButton.getId());
    }

    /**
     * Add group of {@link RadioButtonWithDescription} into current layout. For buttons that already
     * exist in other radio button group, the radio button group will be transferred into the group
     * inside current layout.
     * @param buttons List of {@link RadioButtonWithDescription} to add to this group.
     */
    public void addButtons(List<RadioButtonWithDescription> buttons) {
        for (RadioButtonWithDescription b : buttons) {
            setupButton(b);
            addView(b, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        updateMargins();
    }

    private View removeAttachedAccessoryView(View view) {
        // Remove the view from it's parent if it has one.
        if (view.getParent() != null) {
            ViewGroup previousGroup = (ViewGroup) view.getParent();
            previousGroup.removeView(view);
        }
        return view;
    }

    /**
     * Attach the given accessory view to the given RadioButtonWithDescription. The attachmentPoint
     * must be a direct child of this.
     *
     * @param accessoryView The accessory view to be attached.
     * @param attachmentPoint The RadioButtonWithDescription that the accessory view will be
     *                        attached to.
     */
    public void attachAccessoryView(
            View accessoryView, RadioButtonWithDescription attachmentPoint) {
        removeAttachedAccessoryView(accessoryView);
        int attachmentPointIndex = indexOfChild(attachmentPoint);
        assert attachmentPointIndex >= 0 : "attachmentPoint view must a child of layout.";
        addView(accessoryView, attachmentPointIndex + 1);
    }

    /** Sets margins between each of the radio buttons. */
    private void updateMargins() {
        int childCount = getChildCount();
        for (int i = 0; i < childCount - 1; i++) {
            View child = getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();
            params.bottomMargin = mMarginBetweenRows;
        }
        // LayoutParam changes only take effect after the next layout pass.
        requestLayout();
    }

    private void setupButton(RadioButtonWithDescription radioButton) {
        radioButton.setOnCheckedChangeListener(this);
        // Give the button a unique id to allow for calling onCheckedChanged (see
        // #onCheckedChanged).
        if (radioButton.getId() == NO_ID) radioButton.setId(generateViewId());
        radioButton.setRadioButtonGroup(mRadioButtonsWithDescriptions);
        mRadioButtonsWithDescriptions.add(radioButton);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        for (int i = 0; i < getChildCount(); i++) {
            ViewUtils.setEnabledRecursive(getChildAt(i), enabled);
        }
    }

    /**
     * Marks a RadioButton child as being checked.
     *
     * @param childIndex Index of the child to select.
     */
    @VisibleForTesting
    void selectChildAtIndexForTesting(int childIndex) {
        RadioButtonWithDescription b = (RadioButtonWithDescription) getChildAt(childIndex);
        b.setChecked(true);
    }
}
