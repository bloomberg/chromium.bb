// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.android_webview.devui.util.WebViewPackageHelper;
import org.chromium.ui.widget.Toast;

import java.util.Locale;

/**
 * Dev UI main fragment.
 * It shows a summary about WebView package and the device.
 */
public class HomeFragment extends DevUiBaseFragment {
    private Context mContext;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_home, null);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView DevTools");

        PackageInfo webViewPackage = WebViewPackageHelper.getContextPackageInfo(mContext);
        InfoItem[] infoItems = new InfoItem[] {
                new InfoItem("WebView package", webViewPackage.packageName),
                new InfoItem("WebView version",
                        String.format(Locale.US, "%s (%s)", webViewPackage.versionName,
                                webViewPackage.versionCode)),
                new InfoItem("Device info",
                        String.format(Locale.US, "%s - %s", Build.MODEL, Build.FINGERPRINT)),
        };

        ListView infoListView = view.findViewById(R.id.main_info_list);
        ArrayAdapter<InfoItem> itemsArrayAdapter = new InfoListAdapter(infoItems);
        infoListView.setAdapter(itemsArrayAdapter);

        // Copy item's text to clipboard on long tapping a list item.
        infoListView.setOnItemLongClickListener((parent, clickedView, pos, id) -> {
            InfoItem item = (InfoItem) parent.getItemAtPosition(pos);
            ClipboardManager clipboard =
                    (ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData clip = ClipData.newPlainText(item.title, item.subtitle);
            clipboard.setPrimaryClip(clip);
            // Show a toast that the text has been copied.
            Toast.makeText(mContext, "Copied " + item.title, Toast.LENGTH_SHORT).show();

            return true;
        });
    }

    /**
     * A model class for a key-value piece of information to be displayed as a title (key) and
     * subtitle (value).
     */
    private static class InfoItem {
        public static final String UNKNOWN = "Unknown";
        public final String title;
        public final String subtitle;

        public InfoItem(String title, String subtitle) {
            this.title = title;
            this.subtitle = subtitle == null ? UNKNOWN : subtitle;
        }
    }

    /**
     * An ArrayAdapter to show a list of {@code InfoItem} objects.
     *
     * It uses android stock {@code android.R.layout.simple_list_item_2} which has two {@code
     * TextView}; {@code text1} acts as the item title and {@code text2} as the item subtitle.
     */
    private class InfoListAdapter extends ArrayAdapter<InfoItem> {
        private final InfoItem[] mItems;

        public InfoListAdapter(InfoItem[] items) {
            super(mContext, R.layout.two_line_list_item, items);
            mItems = items;
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.two_line_list_item, null, true);
            }

            InfoItem item = mItems[position];
            TextView title = view.findViewById(android.R.id.text1);
            TextView subtitle = view.findViewById(android.R.id.text2);

            title.setText(item.title);
            subtitle.setText(item.subtitle);

            return view;
        }
    }
}
