// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog shown to the user to enroll a credit card into the virtual card feature. */
public class AutofillVirtualCardEnrollmentDialog {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final VirtualCardEnrollmentFields mVirtualCardEnrollmentFields;
    private final String mAcceptButtonText;
    private final String mDeclineButtonText;
    // TODO(@vishwasuppoor): Replace the 3 link click callbacks with a single callback after fixing
    // crbug.com/1318681.
    private final Callback<String> mOnEducationTextLinkClicked;
    private final Callback<String> mOnGoogleLegalMessageLinkClicked;
    private final Callback<String> mOnIssuerLegalMessageLinkClicked;
    private final Callback<Integer> mResultHandler;
    private PropertyModel mDialogModel;

    public AutofillVirtualCardEnrollmentDialog(Context context,
            ModalDialogManager modalDialogManager,
            VirtualCardEnrollmentFields virtualCardEnrollmentFields, String acceptButtonText,
            String declineButtonText, Callback<String> onEducationTextLinkClicked,
            Callback<String> onGoogleLegalMessageLinkClicked,
            Callback<String> onIssuerLegalMessageLinkClicked, Callback<Integer> resultHandler) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mVirtualCardEnrollmentFields = virtualCardEnrollmentFields;
        mAcceptButtonText = acceptButtonText;
        mDeclineButtonText = declineButtonText;
        mOnEducationTextLinkClicked = onEducationTextLinkClicked;
        mOnGoogleLegalMessageLinkClicked = onGoogleLegalMessageLinkClicked;
        mOnIssuerLegalMessageLinkClicked = onIssuerLegalMessageLinkClicked;
        mResultHandler = resultHandler;
    }

    public void show() {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.CUSTOM_VIEW, getCustomViewForModalDialog())
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mAcceptButtonText)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mDeclineButtonText)
                        .with(ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        mModalDialogManager, mResultHandler));
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    public void dismiss(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }

    private View getCustomViewForModalDialog() {
        View customView = LayoutInflater.from(mContext).inflate(
                R.layout.virtual_card_enrollment_dialog, null);

        TextView titleTextView = (TextView) customView.findViewById(R.id.dialog_title);
        AutofillUiUtils.inlineTitleStringWithLogo(mContext, titleTextView,
                mContext.getString(R.string.autofill_virtual_card_enrollment_dialog_title_label),
                R.drawable.google_pay_with_divider);

        TextView virtualCardEducationTextView =
                (TextView) customView.findViewById(R.id.virtual_card_education);
        virtualCardEducationTextView.setText(
                AutofillUiUtils.getSpannableStringWithClickableSpansToOpenLinksInCustomTabs(
                        mContext, R.string.autofill_virtual_card_enrollment_dialog_education_text,
                        ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL,
                        mOnEducationTextLinkClicked));
        virtualCardEducationTextView.setMovementMethod(LinkMovementMethod.getInstance());

        TextView googleLegalMessageTextView =
                (TextView) customView.findViewById(R.id.google_legal_message);
        googleLegalMessageTextView.setText(AutofillUiUtils.getSpannableStringForLegalMessageLines(
                mContext, mVirtualCardEnrollmentFields.getGoogleLegalMessages(),
                /* underlineLinks= */ false, mOnGoogleLegalMessageLinkClicked));
        googleLegalMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());

        TextView issuerLegalMessageTextView =
                (TextView) customView.findViewById(R.id.issuer_legal_message);
        issuerLegalMessageTextView.setText(AutofillUiUtils.getSpannableStringForLegalMessageLines(
                mContext, mVirtualCardEnrollmentFields.getIssuerLegalMessages(),
                /* underlineLinks= */ false, mOnIssuerLegalMessageLinkClicked));
        issuerLegalMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());

        ((TextView) customView.findViewById(R.id.credit_card_identifier))
                .setText(mContext.getString(
                        R.string.autofill_virtual_card_enrollment_dialog_card_label,
                        mVirtualCardEnrollmentFields.getCardIdentifierString()));
        ((ImageView) customView.findViewById(R.id.credit_card_issuer_icon))
                .setImageBitmap(mVirtualCardEnrollmentFields.getIssuerCardArt());
        return customView;
    }
}
