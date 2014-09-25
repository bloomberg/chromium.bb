// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import android.widget.Toast;

/** Describes the appearance and behavior of each host list entry. */
class HostListAdapter extends ArrayAdapter<HostInfo> {
    /** Color to use for hosts that are online. */
    private static final String HOST_COLOR_ONLINE = "green";

    /** Color to use for hosts that are offline. */
    private static final String HOST_COLOR_OFFLINE = "red";

    private Chromoting mChromoting;

    /** Constructor. */
    public HostListAdapter(Chromoting chromoting, int textViewResourceId, HostInfo[] hosts) {
        super(chromoting, textViewResourceId, hosts);
        mChromoting = chromoting;
    }

    /** Generates a View corresponding to this particular host. */
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        TextView target = (TextView) super.getView(position, convertView, parent);

        final HostInfo host = getItem(position);

        target.setText(host.name);
        target.setCompoundDrawablesWithIntrinsicBounds(
                host.isOnline ? R.drawable.icon_host : R.drawable.icon_host_offline, 0, 0, 0);

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
            target.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Toast.makeText(mChromoting,
                                mChromoting.getString(R.string.host_offline_tooltip),
                                Toast.LENGTH_SHORT).show();
                    }
            });
        }

        return target;
    }
}
