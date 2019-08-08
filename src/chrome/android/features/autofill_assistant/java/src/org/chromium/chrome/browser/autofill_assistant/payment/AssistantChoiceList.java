// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;
import android.content.res.TypedArray;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayout;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewDebug;
import android.view.ViewGroup;
import android.widget.RadioButton;
import android.widget.Space;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.widget.ChromeImageView;

import java.util.ArrayList;
import java.util.List;

/**
 * A widget that displays a list of choices in a regular grid. It is similar to a RadioGroup, but
 * much more customizable, because it allows arbitrary content views rather than only TextViews.
 *
 * The layout is as follows:
 * [radio-button] |    content   | [edit-button]
 * [radio-button] |    content   | [edit-button]
 * ...
 * [add-icon]     | [add-button] |
 *
 * A number of custom view attributes control the layout and look of this widget.
 *  - Edit-buttons are optional on an item-by-item basis.
 *  - The add-button at the end of the list is optional.
 *  - Spacing between rows and columns can be customized.
 *  - The text for the `add' and `edit' buttons can be customized.
 */
public class AssistantChoiceList extends GridLayout {
    /**
     * Represents a single choice with a radio button, customizable content and an edit button.
     */
    private class Item {
        final RadioButton mRadioButton;
        final View mContent;
        final View mEditButton;

        Item(@Nullable RadioButton radioButton, View content, @Nullable View editButton) {
            this.mRadioButton = radioButton;
            this.mContent = content;
            this.mEditButton = editButton;
        }
    }

    /**
     * |mCanAddItems| is true if the custom view parameter |add_button_text| was specified.
     * If true, the list will have an additional 'add' button at the end.
     */
    private final boolean mCanAddItems;
    /**
     * |mAddButton| and |mAddButtonLabel| are guaranteed to be non-null if |mCanAddItems| is true.
     */
    private final ChromeImageView mAddButton;
    private final TextView mAddButtonLabel;
    private final int mRowSpacing;
    private final int mColumnSpacing;
    private final List<Item> mItems = new ArrayList<>();

    private Runnable mAddButtonListener;
    private Callback<View> mItemSelectedListener;
    private Callback<View> mEditButtonListener;

    public AssistantChoiceList(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.getTheme().obtainStyledAttributes(
                attrs, R.styleable.AssistantChoiceList, 0, 0);
        mCanAddItems = a.hasValue(R.styleable.AssistantChoiceList_add_button_text);
        String addButtonText =
                mCanAddItems ? a.getString(R.styleable.AssistantChoiceList_add_button_text) : null;
        mRowSpacing = a.getDimensionPixelSize(R.styleable.AssistantChoiceList_row_spacing, 0);
        mColumnSpacing = a.getDimensionPixelSize(R.styleable.AssistantChoiceList_column_spacing, 0);
        a.recycle();

        // One column for the radio buttons, one for the content, one for the edit buttons.
        setColumnCount(3);

        if (mCanAddItems) {
            mAddButton = createAddButtonIcon();
            mAddButtonLabel = createAddButtonLabel(addButtonText);

            addViewInternal(mAddButton, -1, createRadioButtonLayoutParams());
            GridLayout.LayoutParams lp = createContentLayoutParams();
            lp.columnSpec = GridLayout.spec(UNDEFINED, 2);
            addViewInternal(mAddButtonLabel, -1, lp);

            // Set margin to 0 because list is currently empty.
            updateAddButtonMargins(0);
        } else {
            mAddButton = null;
            mAddButtonLabel = null;
        }
    }

    /**
     * Children of this container are automatically added as selectable items to the list.
     *
     * This method is automatically called by layout inflaters and xml files. In code, you usually
     * want to call |addItem| directly.
     */
    @Override
    public void addView(View view, int index, ViewGroup.LayoutParams lp) {
        assert index == -1;
        String editText = null;
        if (lp instanceof AssistantChoiceList.LayoutParams) {
            editText = ((LayoutParams) lp).mEditText;
        }

        addItem(view, editText);
    }

    /**
     * Adds an item to the list. Additional widgets to select and edit the item are created as
     * necessary.
     * @param view The view to add to the list.
     * @param editButtonText The text of the edit button to display next to |view|. Can be null to
     * indicate that no edit button should be provided.
     */
    public void addItem(View view, @Nullable String editButtonText) {
        RadioButton radioButton = new RadioButton(getContext());
        // Insert at end, before the `add' button (if any).
        int index = mCanAddItems ? indexOfChild(mAddButton) : getChildCount();
        addViewInternal(radioButton, index++, createRadioButtonLayoutParams());
        addViewInternal(view, index++, createContentLayoutParams());

        TextView editButton = null;
        if (editButtonText != null) {
            editButton = (TextView) LayoutInflater.from(getContext())
                                 .inflate(R.layout.autofill_assistant_button_hairline,
                                         /*parent = */ null);
            editButton.setText(editButtonText);
            editButton.setOnClickListener(unusedView -> {
                if (mEditButtonListener != null) {
                    mEditButtonListener.onResult(view);
                }
            });
            addViewInternal(editButton, index++, createEditButtonLayoutParams());
        } else {
            View spacer = new Space(getContext());
            addViewInternal(spacer, index++, createEditButtonLayoutParams());
        }

        Item item = new Item(radioButton, view, editButton);
        radioButton.setOnClickListener(unusedView -> setCheckedItem(item));
        // TODO(crbug.com/806868): Forward event to radiobutton to re-trigger animation.
        view.setOnClickListener(unusedView -> setCheckedItem(item));
        mItems.add(item);

        // Need to adjust button margins after first item was inserted.
        if (mItems.size() == 1) {
            updateAddButtonMargins(mRowSpacing);
        }
    }

    public void setOnAddButtonClickedListener(Runnable listener) {
        mAddButtonListener = listener;
    }

    public void setOnEditButtonClickedListener(Callback<View> listener) {
        mEditButtonListener = listener;
    }

    public void setOnItemSelectedListener(Callback<View> listener) {
        mItemSelectedListener = listener;
    }

    /**
     * Selects the specified item and de-selects all other items in the UI.
     *
     * @param content The content view to select, as specified in |addItem|. Can be null to indicate
     * that all items should be de-selected.
     */
    public void setCheckedItem(@Nullable View content) {
        for (int i = 0; i < mItems.size(); i++) {
            Item item = mItems.get(i);
            if (item.mContent == content) {
                setCheckedItem(item);
                break;
            }
        }
    }

    @Override
    public LayoutParams generateLayoutParams(AttributeSet attrs) {
        return new AssistantChoiceList.LayoutParams(getContext(), attrs);
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new AssistantChoiceList.LayoutParams();
    }

    @Override
    protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams lp) {
        if (lp instanceof LayoutParams) {
            return new LayoutParams((LayoutParams) lp);
        } else if (lp instanceof GridLayout.LayoutParams) {
            return new LayoutParams((GridLayout.LayoutParams) lp);
        }
        return new LayoutParams(lp);
    }

    // Override to allow type-checking of LayoutParams.
    @Override
    protected boolean checkLayoutParams(ViewGroup.LayoutParams p) {
        return p instanceof AssistantChoiceList.LayoutParams;
    }

    /**
     * Adds a view to the underlying gridlayout.
     *
     * This method is used internally to add a view to the actual layout. A single call to |addView|
     * will result in multiple calls to |addViewInternal|, because additional widgets are
     * automatically generated (e.g., radio-buttons and edit-buttons).
     * @param view The view to add to the layout.
     * @param index The index at which to insert the view into the layout. Note that this - along
     * with the column width specified in |lp| - will determine the column in which the view will
     * end up in.
     * @param lp Additional layout parameters, see |GridLayout.LayoutParams|.
     */
    private void addViewInternal(View view, int index, ViewGroup.LayoutParams lp) {
        super.addView(view, index, lp);
    }

    private ChromeImageView createAddButtonIcon() {
        ChromeImageView addButtonIcon = new ChromeImageView(getContext());
        addButtonIcon.setImageResource(R.drawable.ic_autofill_assistant_add_circle_24dp);
        addButtonIcon.setOnClickListener(unusedView -> {
            if (mAddButtonListener != null) {
                mAddButtonListener.run();
            }
        });
        return addButtonIcon;
    }

    private TextView createAddButtonLabel(String addButtonText) {
        TextView addButtonLabel = new TextView(getContext());
        ApiCompatibilityUtils.setTextAppearance(
                addButtonLabel, org.chromium.chrome.R.style.TextAppearance_BlueButtonText2);
        addButtonLabel.setText(addButtonText);
        addButtonLabel.setOnClickListener(unusedView -> {
            if (mAddButtonListener != null) {
                mAddButtonListener.run();
            }
        });
        return addButtonLabel;
    }

    private GridLayout.LayoutParams createContentLayoutParams() {
        // Set layout params to let content grow to maximum size.
        GridLayout.LayoutParams lp =
                new GridLayout.LayoutParams(GridLayout.spec(UNDEFINED), GridLayout.spec(1, 1));
        lp.setGravity(Gravity.FILL_HORIZONTAL | Gravity.CENTER_VERTICAL);
        lp.width = 0;
        lp.topMargin = mItems.isEmpty() ? 0 : mRowSpacing;
        return lp;
    }

    private GridLayout.LayoutParams createRadioButtonLayoutParams() {
        GridLayout.LayoutParams lp =
                new GridLayout.LayoutParams(GridLayout.spec(UNDEFINED), GridLayout.spec(0, 1));
        lp.setGravity(Gravity.CENTER);
        lp.setMarginEnd(mColumnSpacing);
        lp.topMargin = mItems.isEmpty() ? 0 : mRowSpacing;
        return lp;
    }

    private GridLayout.LayoutParams createEditButtonLayoutParams() {
        GridLayout.LayoutParams lp =
                new GridLayout.LayoutParams(GridLayout.spec(UNDEFINED), GridLayout.spec(2, 1));
        lp.setGravity(Gravity.CENTER_VERTICAL);
        lp.setMarginStart(mColumnSpacing);
        lp.topMargin = mItems.isEmpty() ? 0 : mRowSpacing;
        return lp;
    }

    /**
     * Adjusts the margins of the 'add' button.
     *
     * For empty lists, the margins should be 0. For non-empty lists, the margins should be equal
     * to |mRowSpacing|.
     */
    private void updateAddButtonMargins(int marginTop) {
        if (!mCanAddItems) {
            return;
        }

        LayoutParams lp = (LayoutParams) mAddButton.getLayoutParams();
        lp.setMargins(lp.leftMargin, marginTop, lp.rightMargin, lp.bottomMargin);
        mAddButton.setLayoutParams(lp);

        lp = (LayoutParams) mAddButtonLabel.getLayoutParams();
        lp.setMargins(lp.leftMargin, marginTop, lp.rightMargin, lp.bottomMargin);
        mAddButtonLabel.setLayoutParams(lp);
    }

    private void setCheckedItem(Item item) {
        for (int i = 0; i < mItems.size(); i++) {
            RadioButton radioButton = mItems.get(i).mRadioButton;
            radioButton.setChecked(mItems.get(i) == item);
        }

        if (mItemSelectedListener != null) {
            mItemSelectedListener.onResult(item.mContent);
        }
    }

    /**
     * Per-child layout information associated with AssistantChoiceList.
     */
    public static class LayoutParams extends GridLayout.LayoutParams {
        /**
         * Indicates whether an 'edit' button should be added for this item.
         */
        @ViewDebug.ExportedProperty(category = "layout")
        public String mEditText;

        public LayoutParams(Context c, AttributeSet attrs) {
            super(c, attrs);
            TypedArray a = c.obtainStyledAttributes(attrs, R.styleable.AssistantChoiceList);
            mEditText = a.getString(R.styleable.AssistantChoiceList_layout_edit_button_text);
            a.recycle();
        }

        public LayoutParams(ViewGroup.LayoutParams p) {
            super(p);
        }

        public LayoutParams(GridLayout.LayoutParams p) {
            super(p);
        }

        private LayoutParams() {
            super();
        }
    }
}
