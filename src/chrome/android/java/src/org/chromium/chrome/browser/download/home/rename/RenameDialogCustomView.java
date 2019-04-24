// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.rename;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.browser.widget.AlertDialogEditText;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.RenameResult;

/**
 * Content View of dialog in Download Home that allows users to rename a downloaded file
 */
public class RenameDialogCustomView extends ScrollView {
    private TextView mSubtitleView;
    private AlertDialogEditText mFileName;

    public RenameDialogCustomView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    // ScrollView Implementation
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSubtitleView = findViewById(R.id.subtitle);
        mFileName = findViewById(R.id.file_name);
    }

    /**
     * @param suggestedName The suggested file name to fill the initialized edit text.
     */
    public void initializeView(String suggestedName) {
        if (TextUtils.isEmpty(suggestedName)) return;
        mFileName.setText(suggestedName);
        mSubtitleView.setVisibility(View.GONE);
    }

    /**
     * @param renameResult RenameResult to distinguish dialog type, used for updating the subtitle
     *         view.
     */
    public void updateSubtitleView(@RenameResult int renameResult) {
        if (renameResult == RenameResult.SUCCESS) return;

        mSubtitleView.setVisibility(View.VISIBLE);
        switch (renameResult) {
            case RenameResult.FAILURE_NAME_CONFLICT:
                mSubtitleView.setText(R.string.rename_failure_name_conflict);
                break;
            case RenameResult.FAILURE_NAME_TOO_LONG:
                mSubtitleView.setText(R.string.rename_failure_name_too_long);
                break;
            case RenameResult.FAILURE_UNKNOWN:
                mSubtitleView.setText(R.string.rename_failure_name_unavailable);
                break;
            default:
                break;
        }
    }

    /**
     * @return a string from user input for the target name.
     */
    public String getTargetName() {
        return mFileName.getText().toString();
    }
}
