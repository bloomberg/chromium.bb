// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.support.annotation.Nullable;
import android.support.v4.content.res.ResourcesCompat;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab grid.
 * This class supports both full and partial updates to the {@link TabGridViewHolder}.
 */
class TabGridViewBinder {
    /**
     * Partially or fully update the given ViewHolder based on the given model over propertyKey.
     * @param holder The {@link ViewHolder} to use.
     * @param item The model to use.
     * @param propertyKey If present, to be used as the key to partially update. If null, a full
     *                    bind is done.
     */
    public static void onBindViewHolder(
            TabGridViewHolder holder, PropertyModel item, @Nullable PropertyKey propertyKey) {
        if (propertyKey == null) {
            onBindViewHolder(holder, item);
            return;
        }
        if (TabProperties.TITLE == propertyKey) {
            String title = item.get(TabProperties.TITLE);
            holder.title.setText(title);
            holder.closeButton.setContentDescription(holder.itemView.getResources().getString(
                    R.string.accessibility_tabstrip_btn_close_tab, title));
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            Resources res = holder.itemView.getResources();
            Resources.Theme theme = holder.itemView.getContext().getTheme();
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                Drawable selectedDrawable = new InsetDrawable(
                        ResourcesCompat.getDrawable(res, R.drawable.selected_tab_background, theme),
                        (int) res.getDimension(R.dimen.tab_list_selected_inset_kitkat));
                Drawable elevationDrawable =
                        ResourcesCompat.getDrawable(res, R.drawable.popup_bg, theme);
                holder.backgroundView.setVisibility(View.VISIBLE);
                holder.backgroundView.setBackground(
                        item.get(TabProperties.IS_SELECTED) ? selectedDrawable : elevationDrawable);
            } else {
                Drawable drawable = new InsetDrawable(
                        ResourcesCompat.getDrawable(res, R.drawable.selected_tab_background, theme),
                        (int) res.getDimension(R.dimen.tab_list_selected_inset));
                holder.itemView.setForeground(
                        item.get(TabProperties.IS_SELECTED) ? drawable : null);
            }
        } else if (TabProperties.TAB_SELECTED_LISTENER == propertyKey) {
            holder.itemView.setOnClickListener(view -> {
                item.get(TabProperties.TAB_SELECTED_LISTENER).run(holder.getTabId());
            });
        } else if (TabProperties.TAB_CLOSED_LISTENER == propertyKey) {
            holder.closeButton.setOnClickListener(view -> {
                item.get(TabProperties.TAB_CLOSED_LISTENER).run(holder.getTabId());
            });
        } else if (TabProperties.FAVICON == propertyKey) {
            holder.favicon.setImageDrawable(item.get(TabProperties.FAVICON));
        } else if (TabProperties.THUMBNAIL_FETCHER == propertyKey) {
            TabListMediator.ThumbnailFetcher fetcher = item.get(TabProperties.THUMBNAIL_FETCHER);
            if (fetcher == null) return;
            Callback<Bitmap> callback = result -> {
                if (result == null) {
                    holder.thumbnail.setImageResource(0);
                    holder.thumbnail.setMinimumHeight(holder.thumbnail.getWidth());
                } else {
                    holder.thumbnail.setImageBitmap(result);
                }
            };
            fetcher.fetch(callback);
        } else if (TabProperties.IPH_PROVIDER == propertyKey) {
            TabListMediator.IphProvider provider = item.get(TabProperties.IPH_PROVIDER);
            if (provider != null) provider.showIPH(holder.thumbnail);
        } else if (TabProperties.TAB_ID == propertyKey) {
            holder.setTabId(item.get(TabProperties.TAB_ID));
        } else if (TabProperties.CREATE_GROUP_LISTENER == propertyKey) {
            TabListMediator.TabActionListener listener =
                    item.get(TabProperties.CREATE_GROUP_LISTENER);
            if (listener == null) {
                holder.createGroupButton.setVisibility(View.GONE);
                return;
            }
            holder.createGroupButton.setVisibility(View.VISIBLE);
            holder.createGroupButton.setOnClickListener(view -> listener.run(holder.getTabId()));
        } else if (TabProperties.ALPHA == propertyKey) {
            holder.itemView.setAlpha(item.get(TabProperties.ALPHA));
        }
    }

    private static void onBindViewHolder(TabGridViewHolder holder, PropertyModel item) {
        for (PropertyKey propertyKey : TabProperties.ALL_KEYS_TAB_GRID) {
            onBindViewHolder(holder, item, propertyKey);
        }
    }
}
