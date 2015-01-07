// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Handler;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.ui.R;

/**
 * A prompt that bugs users to enter their CVC when unmasking a Wallet instrument (credit card).
 */
public class CardUnmaskPrompt implements DialogInterface.OnDismissListener, TextWatcher {
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
         * Returns whether |userResponse| represents a valid value.
         */
        boolean checkUserInputValidity(String userResponse);

        /**
         * Called when the user has entered a value and pressed "verify".
         * @param userResponse The value the user entered (a CVC), or an empty string if the
         *        user canceled.
         */
        void onUserInput(String userResponse);
    }

    public CardUnmaskPrompt(
            Context context, CardUnmaskPromptDelegate delegate, String title, String instructions) {
        mDelegate = delegate;

        LayoutInflater inflater = LayoutInflater.from(context);
        View v = inflater.inflate(R.layout.autofill_card_unmask_prompt, null);
        ((TextView) v.findViewById(R.id.card_unmask_instructions)).setText(instructions);

        mDialog = new AlertDialog.Builder(context)
                          .setTitle(title)
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
        verifyButton.setEnabled(false);
        verifyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mDelegate.onUserInput(cardUnmaskInput().getText().toString());
            }
        });

        final EditText input = cardUnmaskInput();
        input.addTextChangedListener(this);
        input.post(new Runnable() {
            @Override
            public void run() {
                showKeyboardForUnmaskInput();
            }
        });
    }

    public void dismiss() {
        mDialog.dismiss();
    }

    public void disableAndWaitForVerification() {
        cardUnmaskInput().setEnabled(false);
        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);
        TextView message = verificationView();
        message.setText("Verifying...");
        message.setVisibility(View.VISIBLE);
    }

    public void verificationFinished(boolean success) {
        if (!success) {
            verificationView().setText("Verification failed. Please try again.");
            EditText input = cardUnmaskInput();
            input.setEnabled(true);
            showKeyboardForUnmaskInput();
            // TODO(estade): UI decision - should we clear the input?
        } else {
            verificationView().setText("Success!");
            Handler h = new Handler();
            h.postDelayed(new Runnable() {
                public void run() {
                    dismiss();
                }
            }, 1000);
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        mDelegate.dismissed();
    }

    @Override
    public void afterTextChanged(Editable s) {
        boolean valid = mDelegate.checkUserInputValidity(cardUnmaskInput().getText().toString());
        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(valid);
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}

    private void showKeyboardForUnmaskInput() {
        InputMethodManager imm = (InputMethodManager) mDialog.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(cardUnmaskInput(), InputMethodManager.SHOW_IMPLICIT);
    }

    private EditText cardUnmaskInput() {
        return (EditText) mDialog.findViewById(R.id.card_unmask_input);
    }

    private TextView verificationView() {
        return (TextView) mDialog.findViewById(R.id.card_unmask_verification_message);
    }
}
