// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import org.chromium.chromoting.help.CreditsActivity;
import org.chromium.chromoting.help.HelpContext;
import org.chromium.chromoting.help.HelpSingleton;

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

    public static ListView createNavigationMenu(final Activity activity) {
        ListView navigationMenu = (ListView) activity.getLayoutInflater()
                .inflate(R.layout.navigation_list, null);

        NavigationMenuItem creditsItem = new NavigationMenuItem(R.menu.credits_list_item,
                new Runnable() {
                    @Override
                    public void run() {
                        activity.startActivity(new Intent(activity, CreditsActivity.class));
                    }
                });

        NavigationMenuItem feedbackItem = new NavigationMenuItem(R.menu.feedback_list_item,
                new Runnable() {
                    @Override
                    public void run() {
                        HelpSingleton.getInstance().launchFeedback(activity);
                    }
                });

        NavigationMenuItem helpItem = new NavigationMenuItem(R.menu.help_list_item,
                new Runnable() {
                    @Override
                    public void run() {
                        HelpSingleton.getInstance().launchHelp(activity,
                                HelpContext.HOST_LIST);
                    }
                });

        NavigationMenuItem[] navigationMenuItems = { creditsItem, feedbackItem, helpItem };
        NavigationMenuAdapter adapter = new NavigationMenuAdapter(activity, navigationMenuItems);
        navigationMenu.setAdapter(adapter);
        navigationMenu.setOnItemClickListener(adapter);
        return navigationMenu;
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
