// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;

/**
 * Describes the appearance and behavior of the navigation menu. This also implements
 * AdapterView.OnItemClickListener so it can be used as the ListView's onItemClickListener.
 */
public class NavigationMenuAdapter extends ArrayAdapter<NavigationMenuAdapter.NavigationMenuItem>
        implements AdapterView.OnItemClickListener {
    /**
     * Defines a menu item.
     */
    public static class NavigationMenuItem {
        private int mLayoutResourceId;
        private Runnable mCallback;
        public NavigationMenuItem(int layoutResourceId, Runnable callback) {
            mLayoutResourceId = layoutResourceId;
            mCallback = callback;
        }
    }

    public NavigationMenuAdapter(Context context, NavigationMenuItem[] objects) {
        super(context, -1, objects);
    }

    /** Generates a View corresponding to the particular navigation item. */
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            NavigationMenuItem item = getItem(position);
            LayoutInflater inflater =
                    (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            convertView = inflater.inflate(item.mLayoutResourceId, null);
        }
        return convertView;
    }


    /** AdapterView.OnItemClickListener override. */
    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        getItem(position).mCallback.run();
    }
}
