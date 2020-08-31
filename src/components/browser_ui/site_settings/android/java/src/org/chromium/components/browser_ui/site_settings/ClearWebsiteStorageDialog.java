// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.res.Configuration;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceDialogFragmentCompat;

import org.chromium.base.Callback;

/**
 * The fragment used to display the clear website storage confirmation dialog.
 */
public class ClearWebsiteStorageDialog extends PreferenceDialogFragmentCompat {
    public static final String TAG = "ClearWebsiteStorageDialog";

    private static Callback<Boolean> sCallback;

    // The view containing the dialog ui elements.
    private View mDialogView;

    public static ClearWebsiteStorageDialog newInstance(
            Preference preference, Callback<Boolean> callback) {
        ClearWebsiteStorageDialog fragment = new ClearWebsiteStorageDialog();
        sCallback = callback;
        Bundle bundle = new Bundle(1);
        bundle.putString(PreferenceDialogFragmentCompat.ARG_KEY, preference.getKey());
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    protected void onBindDialogView(View view) {
        mDialogView = view;

        TextView signedOutView = view.findViewById(R.id.signed_out_text);
        TextView offlineTextView = view.findViewById(R.id.offline_text);
        signedOutView.setText(ClearWebsiteStorage.getSignedOutText());
        offlineTextView.setText(ClearWebsiteStorage.getOfflineText());

        super.onBindDialogView(view);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mDialogView != null) {
            // When the device switches to multi-window in landscape mode, the height of the
            // offlineTextView is not calculated correctly (its height gets truncated) and a layout
            // pass is needed to fix it. See https://crbug.com/1072922.
            mDialogView.getHandler().post(mDialogView::requestLayout);
        }
    }

    @Override
    public void onDialogClosed(boolean positiveResult) {
        sCallback.onResult(positiveResult);
    }
}
