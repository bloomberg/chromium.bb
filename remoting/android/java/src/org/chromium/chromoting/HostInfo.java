// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

/** Class to represent a Host returned by {@link HostListLoader}. */
public class HostInfo {
    public final String name;
    public final String id;
    public final String jabberId;
    public final String publicKey;
    public final boolean isOnline;
    private final ArrayList<String> mTokenUrlPatterns;

    public HostInfo(String name,
                    String id,
                    String jabberId,
                    String publicKey,
                    ArrayList<String> tokenUrlPatterns,
                    boolean isOnline) {
        this.name = name;
        this.id = id;
        this.jabberId = jabberId;
        this.publicKey = publicKey;
        this.mTokenUrlPatterns = tokenUrlPatterns;
        this.isOnline = isOnline;
    }

    public ArrayList<String> getTokenUrlPatterns() {
        return new ArrayList<String>(mTokenUrlPatterns);
    }

    public static HostInfo create(JSONObject json) throws JSONException {
        assert json != null;

        ArrayList<String> tokenUrlPatterns = new ArrayList<String>();
        JSONArray jsonPatterns = json.optJSONArray("tokenUrlPatterns");

        if (jsonPatterns != null) {
            for (int i = 0; i < jsonPatterns.length(); i++) {
                String pattern = jsonPatterns.getString(i);
                if (pattern != null && !pattern.isEmpty()) {
                    tokenUrlPatterns.add(pattern);
                }
            }
        }
        return new HostInfo(
                json.getString("hostName"),
                json.getString("hostId"),
                json.optString("jabberId"),
                json.optString("publicKey"),
                tokenUrlPatterns,
                json.optString("status").equals("ONLINE"));
    }
}
