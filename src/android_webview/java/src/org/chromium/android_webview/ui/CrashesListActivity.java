// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.ui;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.SpannableStringBuilder;
import android.text.style.StyleSpan;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.android_webview.R;
import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.ui.util.WebViewCrashInfoCollector;

import java.util.Date;
import java.util.List;

/**
 * An activity to show a list of recent WebView crashes.
 */
public class CrashesListActivity extends Activity {
    // Max number of crashes to show in the crashes list.
    private static final int MAX_CRASHES_NUMBER = 20;

    private ListView mCrashListView;
    private TextView mCrashesSummaryView;
    private WebViewCrashInfoCollector mCrashCollector;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setTitle(R.string.crashes_list_activity_title);
        setContentView(R.layout.activity_crashes_list);

        mCrashListView = findViewById(R.id.crashes_list);
        mCrashesSummaryView = findViewById(R.id.crashes_summary_textview);
        mCrashCollector = new WebViewCrashInfoCollector();

        updateCrashesList();
    }

    /**
     * Adapter to create crashes list rows from a list of CrashInfo.
     */
    private class CrashListAdapter extends ArrayAdapter<CrashInfo> {
        private final List<CrashInfo> mCrashInfoList;

        public CrashListAdapter(List<CrashInfo> crashInfoList) {
            super(CrashesListActivity.this, R.layout.crashes_list_row, crashInfoList);
            mCrashInfoList = crashInfoList;
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.crashes_list_row, null, true);
            }

            TextView label = view.findViewById(R.id.crash_info_label);
            TextView infoTextView = view.findViewById(R.id.crash_info_textview);
            Button button = view.findViewById(R.id.crash_report_button);

            CrashInfo crashInfo = mCrashInfoList.get(position);
            label.setText(getCrashInfoLabelText(crashInfo));
            infoTextView.setText(crashInfoToString(crashInfo));

            // Report button is only clickable if the crash report is uploaded.
            if (crashInfo.uploadState == UploadState.UPLOADED) {
                button.setEnabled(true);
                button.setOnClickListener(v -> {
                    startActivity(new Intent(Intent.ACTION_VIEW, getReportUri(crashInfo)));
                });
            } else {
                button.setEnabled(false);
            }

            return view;
        }

        private String getCrashInfoLabelText(CrashInfo crashInfo) {
            String status =
                    crashInfo.uploadState == null ? "UNKOWN" : crashInfo.uploadState.toString();
            return getString(R.string.crash_label, crashInfo.localId, status);
        }

        /**
         * Convert CrashInfo object to a string to show it in a TexView.
         * Field name is in BOLD while the value is not.
         */
        private SpannableStringBuilder crashInfoToString(CrashInfo crashInfo) {
            SpannableStringBuilder builder = new SpannableStringBuilder();
            if (crashInfo.uploadId != null) {
                builder.append("upload ID: ", new StyleSpan(android.graphics.Typeface.BOLD), 0)
                        .append(crashInfo.uploadId)
                        .append("\n");
            }
            if (crashInfo.uploadTime >= 0) {
                builder.append("upload time: ", new StyleSpan(android.graphics.Typeface.BOLD), 0)
                        .append(new Date(crashInfo.uploadTime * 1000).toString())
                        .append("\n");
            }
            if (crashInfo.captureTime >= 0) {
                builder.append("capture time: ", new StyleSpan(android.graphics.Typeface.BOLD), 0)
                        .append(new Date(crashInfo.captureTime).toString())
                        .append("\n");
            }
            if (crashInfo.packageName != null) {
                builder.append("app package name: ", new StyleSpan(android.graphics.Typeface.BOLD),
                               0)
                        .append(crashInfo.packageName)
                        .append("\n");
            }
            if (crashInfo.variations != null) {
                builder.append("variations: ", new StyleSpan(android.graphics.Typeface.BOLD), 0)
                        .append(crashInfo.variations.toString())
                        .append("\n");
            }
            return builder;
        }

        // Build a report uri to open an issue on https://bugs.chromium.org/p/chromium/issues/entry.
        // It uses WebView Bugs Template and adds "User-Submitted" Label.
        // It adds the upload id at the end of the template and populates the Application package
        // name field.
        // TODO(https://crbug.com/991594) populate more fields in the template.
        private Uri getReportUri(CrashInfo crashInfo) {
            return new Uri.Builder()
                    .scheme("https")
                    .authority("bugs.chromium.org")
                    .path("/p/chromium/issues/entry")
                    .appendQueryParameter("template", "Webview+Bugs")
                    .appendQueryParameter("comment",
                            getString(R.string.crash_report_template, crashInfo.uploadId))
                    .appendQueryParameter("labels", "User-Submitted")
                    .build();
        }
    }

    private void updateCrashesList() {
        List<CrashInfo> crashesList = mCrashCollector.loadCrashesInfo(MAX_CRASHES_NUMBER);
        mCrashListView.setAdapter(new CrashListAdapter(crashesList));

        mCrashesSummaryView.setText(getString(R.string.crashes_num, crashesList.size()));
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.crashes_options_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.options_menu_refresh) {
            updateCrashesList();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
