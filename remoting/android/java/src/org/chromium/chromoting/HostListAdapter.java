// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.text.Html;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONException;
import org.json.JSONObject;

/** Describes the appearance and behavior of each host list entry. */
class HostListAdapter extends ArrayAdapter<JSONObject> {
    /** Color to use for hosts that are online. */
    private static final String HOST_COLOR_ONLINE = "green";

    /** Color to use for hosts that are offline. */
    private static final String HOST_COLOR_OFFLINE = "red";

    private Chromoting mChromoting;

    /** Constructor. */
    public HostListAdapter(Chromoting chromoting, int textViewResourceId) {
        super(chromoting, textViewResourceId);
        mChromoting = chromoting;
    }

    /** Generates a View corresponding to this particular host. */
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        TextView target = (TextView)super.getView(position, convertView, parent);

        try {
            final JSONObject host = getItem(position);
            String status = host.getString("status");
            boolean online = status.equals("ONLINE");
            target.setText(Html.fromHtml(host.getString("hostName") + " (<font color=\"" +
                    (online ? HOST_COLOR_ONLINE : HOST_COLOR_OFFLINE) + "\">" + status +
                    "</font>)"));

            if (online) {
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
        } catch (JSONException ex) {
            Log.w("hostlist", ex);
            Toast.makeText(mChromoting, mChromoting.getString(R.string.error_displaying_host),
                    Toast.LENGTH_LONG).show();

            // Close the application.
            mChromoting.finish();
        }

        return target;
    }
}
