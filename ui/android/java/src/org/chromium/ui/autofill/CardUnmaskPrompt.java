// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.ui.R;

/**
 * A prompt that bugs users to enter their CVC when unmasking a Wallet instrument (credit card).
 */
public class CardUnmaskPrompt implements DialogInterface.OnDismissListener {
    private CardUnmaskPromptDelegate mDelegate;
    private AlertDialog mDialog;

    /**
     * An interface to handle the interaction with an CardUnmaskPrompt object.
     */
    public interface CardUnmaskPromptDelegate {
        /**
         * Called when the dialog has been dismissed.
         */
        void dismissed();

        /**
         * Called when the user has entered a value and pressed "verify".
         * @param userResponse The value the user entered (a CVC), or an empty string if the
         *        user canceled.
         */
        void onUserInput(String userResponse);
    }

    public CardUnmaskPrompt(Context context, CardUnmaskPromptDelegate delegate) {
        mDelegate = delegate;

        LayoutInflater inflater = LayoutInflater.from(context);
        View v = inflater.inflate(R.layout.autofill_card_unmask_prompt, null);

        mDialog = new AlertDialog.Builder(context)
                          .setTitle("Unlocking Visa - 1111")
                          .setView(v)
                          .setNegativeButton("Back", null)
                          .setPositiveButton("Rock on", null)
                          .setOnDismissListener(this)
                          .create();
    }

    public void show() {
        mDialog.show();

        // Override the View.OnClickListener so that pressing the positive button doesn't dismiss
        // the dialog.
        Button verifyButton = mDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        verifyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mDelegate.onUserInput(cardUnmaskInput().getText().toString());
            }
        });
    }

    public void dismiss() {
        mDialog.dismiss();
    }

    public void disableAndWaitForVerification() {
        cardUnmaskInput().setEnabled(false);
        TextView message = verificationView();
        message.setText("Verifying...");
        message.setVisibility(View.VISIBLE);
    }

    public void verificationFailed() {
        verificationView().setText("Verification failed. Please try again.");
        cardUnmaskInput().setEnabled(true);
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        mDelegate.dismissed();
    }

    private EditText cardUnmaskInput() {
        return (EditText) mDialog.findViewById(R.id.card_unmask_input);
    }

    private TextView verificationView() {
        return (TextView) mDialog.findViewById(R.id.card_unmask_verification_message);
    }
}
