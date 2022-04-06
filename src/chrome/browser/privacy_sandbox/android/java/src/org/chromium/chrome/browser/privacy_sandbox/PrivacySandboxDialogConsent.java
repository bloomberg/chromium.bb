// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Dialog;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.ui.drawable.StateListDrawableBuilder;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;
import org.chromium.ui.widget.ChromeBulletSpan;

/**
 * Dialog in the form of a consent shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogConsent extends Dialog implements View.OnClickListener {
    private final View mContentView;
    private final LayoutInflater mLayoutInflater;
    private final CheckableImageView mExpandArrowView;
    private boolean mDropdownExpanded;

    public PrivacySandboxDialogConsent(Context context) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mLayoutInflater = LayoutInflater.from(context);
        mDropdownExpanded = false;
        mContentView = mLayoutInflater.inflate(R.layout.privacy_sandbox_consent, null);
        setContentView(mContentView);

        // Overall consent buttons.
        ButtonCompat yesButton = (ButtonCompat) mContentView.findViewById(R.id.yes_button);
        yesButton.setOnClickListener(this);
        ButtonCompat noButton = (ButtonCompat) mContentView.findViewById(R.id.no_button);
        noButton.setOnClickListener(this);

        // Controls for the expanding section.
        LinearLayout dropdownElement =
                (LinearLayout) mContentView.findViewById(R.id.dropdown_element);
        dropdownElement.setOnClickListener(this);
        updateDropdownControlContentDescription(dropdownElement);
        mExpandArrowView = (CheckableImageView) mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(createExpandDrawable(context));
        mExpandArrowView.setChecked(mDropdownExpanded);
    }

    @Override
    public void show() {
        PrivacySandboxBridge.dialogActionOccurred(DialogAction.CONSENT_SHOWN);
        super.show();
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.yes_button) {
            PrivacySandboxBridge.dialogActionOccurred(DialogAction.CONSENT_ACCEPTED);
            dismiss();
        } else if (id == R.id.no_button) {
            PrivacySandboxBridge.dialogActionOccurred(DialogAction.CONSENT_DECLINED);
            dismiss();
        } else if (id == R.id.dropdown_element) {
            LinearLayout dropdownContainer = mContentView.findViewById(R.id.dropdown_container);
            if (mDropdownExpanded) {
                PrivacySandboxBridge.dialogActionOccurred(DialogAction.CONSENT_MORE_INFO_CLOSED);
                dropdownContainer.removeAllViews();
                dropdownContainer.setVisibility(View.GONE);
            } else {
                PrivacySandboxBridge.dialogActionOccurred(DialogAction.CONSENT_MORE_INFO_OPENED);
                dropdownContainer.setVisibility(View.VISIBLE);
                mLayoutInflater.inflate(
                        R.layout.privacy_sandbox_consent_dropdown, dropdownContainer);
                setDropdownDescription(dropdownContainer, R.id.privacy_sandbox_consent_dropdown_one,
                        R.string.privacy_sandbox_learn_more_description_1);
                setDropdownDescription(dropdownContainer, R.id.privacy_sandbox_consent_dropdown_two,
                        R.string.privacy_sandbox_learn_more_description_2);
                setDropdownDescription(dropdownContainer,
                        R.id.privacy_sandbox_consent_dropdown_three,
                        R.string.privacy_sandbox_learn_more_description_3);
            }
            mDropdownExpanded = !mDropdownExpanded;
            mExpandArrowView.setChecked(mDropdownExpanded);
            updateDropdownControlContentDescription(view);
            view.announceForAccessibility(getContext().getResources().getString(mDropdownExpanded
                            ? R.string.accessibility_expanded_group
                            : R.string.accessibility_collapsed_group));
        }
    }

    private void setDropdownDescription(
            ViewGroup container, @IdRes int viewId, @StringRes int stringRes) {
        TextView view = container.findViewById(viewId);
        SpannableString spannableString =
                SpanApplier.applySpans(getContext().getResources().getString(stringRes),
                        new SpanApplier.SpanInfo(
                                "<b>", "</b>", new StyleSpan(android.graphics.Typeface.BOLD)));
        spannableString.setSpan(new ChromeBulletSpan(getContext()), 0, spannableString.length(), 0);
        view.setText(spannableString);
    }

    private void updateDropdownControlContentDescription(View dropdownElement) {
        // TODO(crbug.com/1286276): add CONCAT_TWO_STRINGS_WITH_PERIODS to Android strings and use
        // that instead to avoid l10n issues.
        String description = getContext().getResources().getString(
                                     R.string.privacy_sandbox_consent_dropdown_button)
                + ". "
                + getContext().getResources().getString(mDropdownExpanded
                                ? R.string.accessibility_expanded_group
                                : R.string.accessibility_collapsed_group);
        dropdownElement.setContentDescription(description);
    }

    private static Drawable createExpandDrawable(Context context) {
        StateListDrawableBuilder builder = new StateListDrawableBuilder(context);
        StateListDrawableBuilder.State checked = builder.addState(
                R.drawable.ic_expand_less_black_24dp, android.R.attr.state_checked);
        StateListDrawableBuilder.State unchecked =
                builder.addState(R.drawable.ic_expand_more_black_24dp);
        builder.addTransition(
                checked, unchecked, R.drawable.transition_expand_less_expand_more_black_24dp);
        builder.addTransition(
                unchecked, checked, R.drawable.transition_expand_more_expand_less_black_24dp);

        Drawable tintableDrawable = DrawableCompat.wrap(builder.build());
        DrawableCompat.setTintList(tintableDrawable,
                AppCompatResources.getColorStateList(
                        context, R.color.default_icon_color_tint_list));
        return tintableDrawable;
    }
}
