// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

/** Describes the appearance and behavior of each host list entry. */
class HostListAdapter extends ArrayAdapter<HostInfo> {
    public HostListAdapter(Context context, int textViewResourceId, HostInfo[] hosts) {
        super(context, textViewResourceId, hosts);
    }

    /** Generates a View corresponding to this particular host. */
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        TextView target = (TextView) super.getView(position, convertView, parent);

        final HostInfo host = getItem(position);

        target.setText(host.name);
        target.setCompoundDrawablesWithIntrinsicBounds(
                host.isOnline ? R.drawable.ic_host_online : R.drawable.ic_host_offline, 0, 0, 0);

        if (!host.isOnline) {
            target.setTextColor(getContext().getResources().getColor(R.color.host_offline_text));
        }

        return target;
    }
}
