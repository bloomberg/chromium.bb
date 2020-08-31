// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.chrome.browser.share;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;

/**
 * Bottom sheet content to display a 2-row custom share sheet.
 */
public class ShareSheetBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    private Context mContext;
    private ViewGroup mToolbarView;
    private ViewGroup mContentView;
    private static final int SHARE_SHEET_ITEM = 0;

    /**
     * Creates a ShareSheetBottomSheetContent (custom share sheet) opened from the given activity.
     *
     * @param context The context the share sheet was launched from.
     */
    public ShareSheetBottomSheetContent(Context context) {
        mContext = context;

        createContentView();
    }

    private void createContentView() {
        mContentView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.share_sheet_content, null);
    }

    /*
     * Creates a new share sheet view with two rows based on the provided PropertyModels.
     *
     * @param activity The activity the share sheet belongs to.
     * @param topRowModels The PropertyModels used to build the top row.
     * @param bottomRowModels The PropertyModels used to build the bottom row.
     */
    public void createRecyclerViews(
            ArrayList<PropertyModel> topRowModels, ArrayList<PropertyModel> bottomRowModels) {
        RecyclerView topRow = this.getContentView().findViewById(R.id.share_sheet_chrome_apps);
        populateView(topRowModels, topRow);
        topRow.addOnScrollListener(new ScrollEventReporter("SharingHubAndroid.TopRowScrolled"));
        RecyclerView bottomRow = this.getContentView().findViewById(R.id.share_sheet_other_apps);
        populateView(
                bottomRowModels, this.getContentView().findViewById(R.id.share_sheet_other_apps));
        bottomRow.addOnScrollListener(
                new ScrollEventReporter("SharingHubAndroid.BottomRowScrolled"));
    }

    private void populateView(ArrayList<PropertyModel> models, RecyclerView view) {
        ModelList modelList = new ModelList();
        for (PropertyModel model : models) {
            modelList.add(new ListItem(SHARE_SHEET_ITEM, model));
        }
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        adapter.registerType(SHARE_SHEET_ITEM, new LayoutViewBuilder(R.layout.share_sheet_item),
                ShareSheetBottomSheetContent::bindShareItem);
        view.setAdapter(adapter);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(mContext, LinearLayoutManager.HORIZONTAL, false);
        view.setLayoutManager(layoutManager);
    }

    private static void bindShareItem(
            PropertyModel model, ViewGroup parent, PropertyKey propertyKey) {
        if (ShareSheetItemViewProperties.ICON.equals(propertyKey)) {
            ImageView view = (ImageView) parent.findViewById(R.id.icon);
            view.setImageDrawable(model.get(ShareSheetItemViewProperties.ICON));
        } else if (ShareSheetItemViewProperties.LABEL.equals(propertyKey)) {
            TextView view = (TextView) parent.findViewById(R.id.text);
            view.setText(model.get(ShareSheetItemViewProperties.LABEL));
        } else if (ShareSheetItemViewProperties.CLICK_LISTENER.equals(propertyKey)) {
            parent.setOnClickListener(model.get(ShareSheetItemViewProperties.CLICK_LISTENER));
        }
    }

    /**
     * One-shot reporter that records the first time the user scrolls a {@link RecyclerView}.
     */
    private static class ScrollEventReporter extends RecyclerView.OnScrollListener {
        private boolean mFired;
        private String mActionName;

        public ScrollEventReporter(String actionName) {
            mActionName = actionName;
        }

        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            if (mFired) return;
            if (newState != RecyclerView.SCROLL_STATE_DRAGGING) return;

            RecordUserAction.record(mActionName);
            mFired = true;
        }
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // This ensures that the bottom sheet reappears after the first time. Otherwise, the
        // second time that a user initiates a share, the bottom sheet does not re-appear.
        return true;
    }

    @Override
    public int getPeekHeight() {
        // Return false to ensure that the entire bottom sheet is shown.
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // Return WRAP_CONTENT to have the bottom sheet only open as far as it needs to display the
        // list of devices and nothing beyond that.
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.send_tab_to_self_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {}
}
