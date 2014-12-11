// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import org.chromium.ui.R;

/**
 * A prompt that bugs users to enter their CVC when unmasking a Wallet instrument (credit card).
 */
public class CardUnmaskPrompt implements DialogInterface.OnClickListener,
        DialogInterface.OnDismissListener {
    private CardUnmaskPromptDelegate mDelegate;
    private int mClickedButton;
    private AlertDialog mDialog;

    /**
     * An interface to handle the interaction with an CardUnmaskPrompt object.
     */
    public interface CardUnmaskPromptDelegate {
        /**
         * Called when the dialog has been dismissed.
         * @param userResponse The value the user entered (a CVC), or an empty string if the
         *        user canceled.
         */
        void dismissed(String userResponse);
    }

    public CardUnmaskPrompt(Context context, CardUnmaskPromptDelegate delegate) {
        mDelegate = delegate;
        mClickedButton = DialogInterface.BUTTON_NEGATIVE;

        LayoutInflater inflater = LayoutInflater.from(context);
        View v = inflater.inflate(R.layout.autofill_card_unmask_prompt, null);

        mDialog = new AlertDialog.Builder(context)
                .setTitle("Unlocking Visa - 1111")
                .setView(v)
                .setNegativeButton("Back", this)
                .setPositiveButton("Rock on", this)
                .setOnDismissListener(this).create();
    }

    public void show() {
        mDialog.show();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        mClickedButton = which;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        if (mClickedButton == DialogInterface.BUTTON_POSITIVE) {
            String response = ((EditText) mDialog.findViewById(R.id.card_unmask_input))
                    .getText().toString();
            mDelegate.dismissed(response);
        } else {
            mDelegate.dismissed("");
        }
    }
}
