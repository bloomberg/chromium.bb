// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.annotation.IntDef;
import android.support.v7.widget.RecyclerView;

import org.json.JSONException;
import org.json.JSONObject;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Holds onto fields that represent the focused view on a page, and the ability to be round tripped
 * through a String.
 */
public class TouchlessNewTabPageFocusInfo {
    @IntDef({FocusType.UNKNOWN, FocusType.LAST_TAB, FocusType.MOST_LIKELY, FocusType.ARTICLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FocusType {
        int UNKNOWN = 0;
        int LAST_TAB = 1;
        int MOST_LIKELY = 2;
        int ARTICLE = 3;
    }

    private static final String TYPE_KEY = "type";
    private static final String INDEX_KEY = "index";

    public final @FocusType int type;

    /** Depending on the type, the index may be meaningful or instead hold FocusType.UNKNOWN. */
    public final int index;

    public TouchlessNewTabPageFocusInfo(@FocusType int type, int index) {
        this.type = type;
        this.index = index;
    }

    public TouchlessNewTabPageFocusInfo(@FocusType int type) {
        this(type, RecyclerView.NO_POSITION);
    }

    /**
     * Turns the current TouchlessNewTabPageFocus object into a String that can be more easily
     * stored and restored. If anything goes wrong then an empty string is returned instead.
     * @return The values encoded into a String.
     */
    public String serialize() {
        try {
            JSONObject json = new JSONObject();
            json.put(TYPE_KEY, type);
            json.put(INDEX_KEY, index);
            return json.toString();
        } catch (JSONException e) {
            return "";
        }
    }

    /**
     * Turns the given String into a TouchlessNewTabPageFocus object. If anything goes wrong a
     * default value TouchlessNewTabPageFocus is returned that essentially means nothing is focused.
     * @param str The serialized form of a TouchlessNewTabPageFocus.
     * @return An instantiated TouchlessNewTabPageFocus object.
     */
    public static TouchlessNewTabPageFocusInfo deserialize(String str) {
        if (str == null || str.isEmpty()) return newDefault();
        try {
            JSONObject json = new JSONObject(str);
            return new TouchlessNewTabPageFocusInfo(json.getInt(TYPE_KEY), json.getInt(INDEX_KEY));
        } catch (JSONException e) {
            return newDefault();
        }
    }

    /**
     * @return A new focus object that means nothing is currently focused.
     */
    private static TouchlessNewTabPageFocusInfo newDefault() {
        return new TouchlessNewTabPageFocusInfo(FocusType.UNKNOWN, RecyclerView.NO_POSITION);
    }
}