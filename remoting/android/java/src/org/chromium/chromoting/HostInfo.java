// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;

import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

/** Class to represent a Host returned by {@link HostListManager}. */
public class HostInfo {
    private static final String TAG = "Chromoting";

    public final String name;
    public final String id;
    public final String jabberId;
    public final String publicKey;
    public final boolean isOnline;
    public final String hostOfflineReason;
    public final Date updatedTime;
    public final String hostVersion;
    public final String hostOs;
    public final String hostOsVersion;

    private final ArrayList<String> mTokenUrlPatterns;

    // Format of time values coming from the Chromoting REST API.
    // This is a specific form of RFC3339 (with no timezone info).
    // Example value to be parsed: 2014-11-21T01:02:33.814Z
    private static final String RFC_3339_FORMAT = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'SSS'Z'";

    // Value to use if time string received from the network is malformed.
    // Such malformed string should in theory never happen, but we want
    // to have a safe fallback in case it does happen.
    private static final Date FALLBACK_DATE_IN_THE_PAST = new Date(0);

    public HostInfo(String name, String id, String jabberId, String publicKey,
            ArrayList<String> tokenUrlPatterns, boolean isOnline, String hostOfflineReason,
            String updatedTime, String hostVersion, String hostOs, String hostOsVersion) {
        this.name = name;
        this.id = id;
        this.jabberId = jabberId;
        this.publicKey = publicKey;
        this.mTokenUrlPatterns = tokenUrlPatterns;
        this.isOnline = isOnline;
        this.hostOfflineReason = hostOfflineReason;
        this.hostVersion = hostVersion;
        this.hostOs = hostOs;
        this.hostOsVersion = hostOsVersion;

        ParsePosition parsePosition = new ParsePosition(0);
        SimpleDateFormat format = new SimpleDateFormat(RFC_3339_FORMAT, Locale.US);
        format.setTimeZone(TimeZone.getTimeZone("UTC"));
        Date updatedTimeCandidate = format.parse(updatedTime, parsePosition);
        if (updatedTimeCandidate == null) {
            Log.e(TAG, "Unparseable host.updatedTime JSON: errorIndex = %d, input = %s",
                    parsePosition.getErrorIndex(), updatedTime);
            updatedTimeCandidate = FALLBACK_DATE_IN_THE_PAST;
        }
        this.updatedTime = updatedTimeCandidate;
    }

    public String getHostOfflineReasonText(Context context) {
        if (TextUtils.isEmpty(hostOfflineReason)) {
            return context.getString(R.string.host_offline_tooltip);
        }
        try {
            // TODO(wnwen): Replace this with explicit usage so errors move to compile time.
            String resourceName = "offline_reason_" + hostOfflineReason.toLowerCase(Locale.ENGLISH);
            int resourceId = context.getResources().getIdentifier(
                    resourceName, "string", context.getPackageName());
            return context.getString(resourceId);
        } catch (Resources.NotFoundException ignored) {
            return context.getString(R.string.offline_reason_unknown, hostOfflineReason);
        }
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
        return new HostInfo(json.getString("hostName"), json.getString("hostId"),
                json.optString("jabberId"), json.optString("publicKey"), tokenUrlPatterns,
                json.optString("status").equals("ONLINE"), json.optString("hostOfflineReason"),
                json.optString("updatedTime"), json.optString("hostVersion"),
                json.optString("hostOs"), json.optString("hostOsVersion"));
    }
}
