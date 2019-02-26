// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.widget.BoundedLinearLayout;
import org.chromium.chrome.browser.widget.FadingEdgeScrollView;
import org.chromium.ui.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Generic dialog view for app modal or tab modal alert dialogs.
 */
public class ModalDialogView extends BoundedLinearLayout implements View.OnClickListener {
    /**
     * Interface that controls the actions on the modal dialog.
     */
    public interface Controller {
        /**
         * Handle click event of the buttons on the dialog.
         * @param model The dialog model that is associated with this click event.
         * @param buttonType The type of the button.
         */
        void onClick(PropertyModel model, @ButtonType int buttonType);

        /**
         * Handle dismiss event when the dialog is dismissed by actions on the dialog. Note that it
         * can be dangerous to the {@code dismissalCause} for business logic other than metrics
         * recording, unless the dismissal cause is fully controlled by the client (e.g. button
         * clicked), because the dismissal cause can be different values depending on modal dialog
         * type and mode of presentation (e.g. it could be unknown on VR but a specific value on
         * non-VR).
         * @param model The dialog model that is associated with this dismiss event.
         * @param dismissalCause The reason of the dialog being dismissed.
         * @see DialogDismissalCause
         */
        void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause);
    }

    @IntDef({ButtonType.POSITIVE, ButtonType.NEGATIVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        int POSITIVE = 0;
        int NEGATIVE = 1;
    }

    private Controller mController;

    private FadingEdgeScrollView mScrollView;
    private ViewGroup mTitleContainer;
    private TextView mTitleView;
    private ImageView mTitleIcon;
    private TextView mMessageView;
    private ViewGroup mCustomViewContainer;
    private View mButtonBar;
    private Button mPositiveButton;
    private Button mNegativeButton;
    private Callback<Integer> mOnButtonClickedCallback;
    private boolean mTitleScrollable;

    /**
     * Constructor for inflating from XML.
     */
    public ModalDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = findViewById(R.id.modal_dialog_scroll_view);
        mTitleContainer = findViewById(R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        mMessageView = findViewById(R.id.message);
        mCustomViewContainer = findViewById(R.id.custom);
        mButtonBar = findViewById(R.id.button_bar);
        mPositiveButton = findViewById(R.id.positive_button);
        mNegativeButton = findViewById(R.id.negative_button);

        mPositiveButton.setOnClickListener(this);
        mNegativeButton.setOnClickListener(this);
        updateContentVisibility();
        updateButtonVisibility();

        // If the scroll view can not be scrolled, make the scroll view not focusable so that the
        // focusing behavior for hardware keyboard is less confusing.
        // See https://codereview.chromium.org/2939883002.
        mScrollView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    boolean isScrollable = v.canScrollVertically(-1) || v.canScrollVertically(1);
                    v.setFocusable(isScrollable);
                });
    }

    // View.OnClickListener implementation.

    @Override
    public void onClick(View view) {
        if (view == mPositiveButton) {
            mOnButtonClickedCallback.onResult(ButtonType.POSITIVE);
        } else if (view == mNegativeButton) {
            mOnButtonClickedCallback.onResult(ButtonType.NEGATIVE);
        }
    }

    /**
     * @return The controller that controls the actions on the dialogs.
     */
    public Controller getController() {
        return mController;
    }

    /**
     * @param controller The {@link Controller} that handles events on user actions.
     */
    void setController(Controller controller) {
        mController = controller;
    }

    /**
     * @param callback The {@link Callback<Integer>} when a button on the dialog button bar is
     *                 clicked. The {@link Integer} indicates the button type.
     */
    void setOnButtonClickedCallback(Callback<Integer> callback) {
        mOnButtonClickedCallback = callback;
    }

    /** @param title The title of the dialog. */
    public void setTitle(CharSequence title) {
        mTitleView.setText(title);
        updateContentVisibility();
    }

    /**
     * @param drawable The icon drawable on the title.
     */
    public void setTitleIcon(Drawable drawable) {
        mTitleIcon.setImageDrawable(drawable);
        updateContentVisibility();
    }

    /** @param titleScrollable Whether the title is scrollable with the message. */
    void setTitleScrollable(boolean titleScrollable) {
        if (mTitleScrollable == titleScrollable) return;

        mTitleScrollable = titleScrollable;
        CharSequence title = mTitleView.getText();
        Drawable icon = mTitleIcon.getDrawable();

        // Hide the previous title container since the scrollable and non-scrollable title container
        // should not be shown at the same time.
        mTitleContainer.setVisibility(View.GONE);

        mTitleContainer = findViewById(
                titleScrollable ? R.id.scrollable_title_container : R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        setTitle(title);
        setTitleIcon(icon);

        LayoutParams layoutParams = (LayoutParams) mCustomViewContainer.getLayoutParams();
        if (titleScrollable) {
            layoutParams.height = LayoutParams.WRAP_CONTENT;
            layoutParams.weight = 0;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.FADING, FadingEdgeScrollView.EdgeType.FADING);
        } else {
            layoutParams.height = 0;
            layoutParams.weight = 1;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.NONE);
        }
        mCustomViewContainer.setLayoutParams(layoutParams);
    }

    /** @param message The message in the dialog content. */
    void setMessage(String message) {
        mMessageView.setText(message);
        updateContentVisibility();
    }

    /** @param view The customized view in the dialog content. */
    void setCustomView(View view) {
        if (mCustomViewContainer.getChildCount() > 0) mCustomViewContainer.removeAllViews();

        if (view != null) {
            UiUtils.removeViewFromParent(view);
            mCustomViewContainer.addView(view);
            mCustomViewContainer.setVisibility(View.VISIBLE);
        } else {
            mCustomViewContainer.setVisibility(View.GONE);
        }
    }

    /**
     * @param buttonType Indicates which button should be returned.
     */
    private Button getButton(@ButtonType int buttonType) {
        switch (buttonType) {
            case ButtonType.POSITIVE:
                return mPositiveButton;
            case ButtonType.NEGATIVE:
                return mNegativeButton;
            default:
                assert false;
                return null;
        }
    }

    /**
     * Sets button text for the specified button. If {@code buttonText} is empty or null, the
     * specified button will not be visible.
     * @param buttonType The {@link ButtonType} of the button.
     * @param buttonText The text to be set on the specified button.
     */
    void setButtonText(@ButtonType int buttonType, String buttonText) {
        getButton(buttonType).setText(buttonText);
        updateButtonVisibility();
    }

    /**
     * @param buttonType The {@link ButtonType} of the button.
     * @param enabled Whether the specified button should be enabled.
     */
    void setButtonEnabled(@ButtonType int buttonType, boolean enabled) {
        getButton(buttonType).setEnabled(enabled);
    }

    private void updateContentVisibility() {
        boolean titleVisible = !TextUtils.isEmpty(mTitleView.getText());
        boolean titleIconVisible = mTitleIcon.getDrawable() != null;
        boolean titleContainerVisible = titleVisible || titleIconVisible;
        boolean messageVisible = !TextUtils.isEmpty(mMessageView.getText());
        boolean scrollViewVisible = (mTitleScrollable && titleContainerVisible) || messageVisible;

        mTitleView.setVisibility(titleVisible ? View.VISIBLE : View.GONE);
        mTitleIcon.setVisibility(titleIconVisible ? View.VISIBLE : View.GONE);
        mTitleContainer.setVisibility(titleContainerVisible ? View.VISIBLE : View.GONE);
        mMessageView.setVisibility(messageVisible ? View.VISIBLE : View.GONE);
        mScrollView.setVisibility(scrollViewVisible ? View.VISIBLE : View.GONE);
    }

    private void updateButtonVisibility() {
        boolean positiveButtonVisible = !TextUtils.isEmpty(mPositiveButton.getText());
        boolean negativeButtonVisible = !TextUtils.isEmpty(mNegativeButton.getText());
        boolean buttonBarVisible = positiveButtonVisible || negativeButtonVisible;

        mPositiveButton.setVisibility(positiveButtonVisible ? View.VISIBLE : View.GONE);
        mNegativeButton.setVisibility(negativeButtonVisible ? View.VISIBLE : View.GONE);
        mButtonBar.setVisibility(buttonBarVisible ? View.VISIBLE : View.GONE);
    }
}
