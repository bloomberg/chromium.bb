// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Locale;

/** Describes the appearance and behavior of each host list entry. */
class HostListAdapter extends ArrayAdapter<HostInfo> {
    private Chromoting mChromoting;

    /** Constructor. */
    public HostListAdapter(Chromoting chromoting, int textViewResourceId, HostInfo[] hosts) {
        super(chromoting, textViewResourceId, hosts);
        mChromoting = chromoting;
    }

    private String getHostOfflineTooltip(String hostOfflineReason) {
        if (TextUtils.isEmpty(hostOfflineReason)) {
            return mChromoting.getString(R.string.host_offline_tooltip);
        }
        try {
            String resourceName = "offline_reason_" + hostOfflineReason.toLowerCase(Locale.ENGLISH);
            int resourceId = mChromoting.getResources().getIdentifier(
                    resourceName, "string", mChromoting.getPackageName());
            return mChromoting.getString(resourceId);
        } catch (Resources.NotFoundException ignored) {
            return mChromoting.getString(R.string.offline_reason_unknown, hostOfflineReason);
        }
    }

    /** Generates a View corresponding to this particular host. */
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        TextView target = (TextView) super.getView(position, convertView, parent);

        final HostInfo host = getItem(position);

        target.setText(host.name);
        target.setCompoundDrawablesWithIntrinsicBounds(
                host.isOnline ? R.drawable.ic_host_online : R.drawable.ic_host_offline, 0, 0, 0);

        if (host.isOnline) {
            target.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        mChromoting.connectToHost(host);
                    }
            });
        } else {
            target.setTextColor(mChromoting.getResources().getColor(R.color.host_offline_text));
            target.setBackgroundResource(R.drawable.list_item_disabled_selector);

            final String tooltip = getHostOfflineTooltip(host.hostOfflineReason);
            target.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Toast.makeText(mChromoting, tooltip, Toast.LENGTH_SHORT).show();
                    }
            });
        }

        return target;
    }
}
