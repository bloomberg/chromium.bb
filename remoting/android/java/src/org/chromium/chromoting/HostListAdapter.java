// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.text.Html;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

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
        TextView target = (TextView)super.getView(position, convertView, parent);

        final HostInfo host = getItem(position);

        // TODO(lambroslambrou): Don't use hardcoded ONLINE/OFFLINE strings here.
        // See http://crbug.com/331103
        target.setText(Html.fromHtml(host.name + " (<font color=\"" +
                (host.isOnline ? HOST_COLOR_ONLINE : HOST_COLOR_OFFLINE) + "\">" +
                (host.isOnline ? "ONLINE" : "OFFLINE") + "</font>)"));

        if (host.isOnline) {
            target.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        mChromoting.connectToHost(host);
                    }
            });
        } else {
            // Disallow interaction with this entry.
            target.setEnabled(false);
        }

        return target;
    }
}
