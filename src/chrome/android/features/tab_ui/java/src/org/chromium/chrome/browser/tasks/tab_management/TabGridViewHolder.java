// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Build;
import android.support.annotation.IntDef;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// TODO(mattsimmons): Rename this to be less `grid` specific since it's used by both GRID and
//  CAROUSEL modes. Do the same for the grid layouts inflated in `create`.
/**
 * {@link RecyclerView.ViewHolder} for tab grid. Owns the tab info card
 * and the associated view hierarchy.
 */
class TabGridViewHolder extends RecyclerView.ViewHolder {
    @IntDef({TabGridViewItemType.CLOSABLE_TAB, TabGridViewItemType.SELECTABLE_TAB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabGridViewItemType {
        int CLOSABLE_TAB = 0;
        int SELECTABLE_TAB = 1;
        int NUM_ENTRIES = 2;
    }

    public final ImageView favicon;
    public final TextView title;
    public final ImageView thumbnail;
    public final ButtonCompat createGroupButton;
    public final ChromeImageView backgroundView;
    public final View selectedViewBelowLollipop;
    public final View dividerView;
    public final View cardView;
    public final ImageView actionButton;

    private int mTabId;

    protected TabGridViewHolder(View itemView) {
        super(itemView);
        this.cardView = itemView.findViewById(R.id.card_view);
        this.thumbnail = itemView.findViewById(R.id.tab_thumbnail);
        this.title = itemView.findViewById(R.id.tab_title);
        this.favicon = itemView.findViewById(R.id.tab_favicon);
        this.actionButton = itemView.findViewById(R.id.action_button);
        this.createGroupButton = itemView.findViewById(R.id.create_group_button);
        this.backgroundView = itemView.findViewById(R.id.background_view);
        this.selectedViewBelowLollipop = itemView.findViewById(R.id.selected_view_below_lollipop);
        this.dividerView = itemView.findViewById(R.id.divider_view);
    }

    public static TabGridViewHolder create(ViewGroup parent, int itemViewType) {
        if (itemViewType == TabGridViewItemType.CLOSABLE_TAB) {
            View view = LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.closable_tab_grid_card_item, parent, false);
            return new ClosableTabGridViewHolder(view);
        } else {
            SelectableTabGridView view =
                    (SelectableTabGridView) LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.selectable_tab_grid_card_item, parent, false);
            return new SelectableTabGridViewHolder(view);
        }
    }

    public void setTabId(int tabId) {
        mTabId = tabId;
    }

    public int getTabId() {
        return mTabId;
    }

    public void resetThumbnail() {
        thumbnail.setImageDrawable(null);
        thumbnail.setMinimumHeight(thumbnail.getWidth());
    }

    public void updateColor(boolean isIncognito) {
        // ViewCompat.SetBackgroundTintList does not work here for L devices, because cardView is a
        // RelativeLayout, and in order for ViewCompat.SetBackgroundTintList to work on any L-
        // devices, the view has to implement the TintableBackgroundView interface. RelativeLayout
        // is not a TintableBackgroundView. The work around here is to set different drawable as the
        // background depends on the incognito mode.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            cardView.setBackground(TabUiColorProvider.getCardViewBackgroundDrawable(
                    cardView.getContext(), isIncognito));
        } else {
            ViewCompat.setBackgroundTintList(cardView,
                    TabUiColorProvider.getCardViewTintList(cardView.getContext(), isIncognito));
        }

        dividerView.setBackgroundColor(
                TabUiColorProvider.getDividerColor(dividerView.getContext(), isIncognito));

        ApiCompatibilityUtils.setImageTintList(actionButton,
                TabUiColorProvider.getActionButtonTintList(actionButton.getContext(), isIncognito));

        ApiCompatibilityUtils.setTextAppearance(
                title, TabUiColorProvider.getTitleTextAppearance(isIncognito));

        if (thumbnail.getDrawable() == null) {
            thumbnail.setImageResource(
                    TabUiColorProvider.getThumbnailPlaceHolderColorResource(isIncognito));
        }

        if (FeatureUtilities.isTabGroupsAndroidEnabled()) {
            ViewCompat.setBackgroundTintList(backgroundView,
                    TabUiColorProvider.getHoveredCardBackgroundTintList(
                            backgroundView.getContext(), isIncognito));
        }
    }
}
