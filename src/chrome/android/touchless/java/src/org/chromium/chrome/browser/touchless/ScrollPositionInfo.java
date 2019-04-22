// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.v7.widget.RecyclerView;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Holds onto fields that represent the scroll position on a page, and the ability to be round
 * tripped through a String.
 */
public class ScrollPositionInfo {
    private static final String INDEX_KEY = "index";
    private static final String OFFSET_KEY = "offset";

    public final int index;
    public final int offset;

    public ScrollPositionInfo(int index, int offset) {
        this.index = index;
        this.offset = offset;
    }

    /**
     * Turns the current ScrollPosition object into a String that can be more easily stored and
     * restored. If anything goes wrong then an empty string is returned instead.
     *
     * @return The values encoded into a String.
     */
    public String serialize() {
        try {
            JSONObject json = new JSONObject();
            json.put(INDEX_KEY, index);
            json.put(OFFSET_KEY, offset);
            return json.toString();
        } catch (JSONException e) {
            return "";
        }
    }

    /**
     * Turns the given String into a ScrollPosition object. If anything goes wrong a default value
     * ScrollPosition is returned that essentially means there was no scrolling yet.
     *
     * @param str The serialized form of a ScrollPosition.
     * @return An instantiated ScrollPosition object.
     */
    public static ScrollPositionInfo deserialize(String str) {
        if (str == null || str.isEmpty()) return newDefault();
        try {
            JSONObject json = new JSONObject(str);
            return new ScrollPositionInfo(json.getInt(INDEX_KEY), json.getInt(OFFSET_KEY));
        } catch (JSONException e) {
            return newDefault();
        }
    }

    private static ScrollPositionInfo newDefault() {
        return new ScrollPositionInfo(RecyclerView.NO_POSITION, 0);
    }
}