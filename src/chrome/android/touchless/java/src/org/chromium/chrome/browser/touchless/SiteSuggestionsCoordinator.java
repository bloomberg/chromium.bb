// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewStub;

import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * Controller for the carousel version of most likely, to be shown on touchless devices.
 */
class SiteSuggestionsCoordinator {
    static final PropertyModel.WritableIntPropertyKey CURRENT_INDEX_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel
            .ReadableObjectPropertyKey<PropertyListModel<PropertyModel, PropertyKey>>
                    SUGGESTIONS_KEY = new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.WritableIntPropertyKey ITEM_COUNT_KEY =
            new PropertyModel.WritableIntPropertyKey();

    private SiteSuggestionsMediator mMediator;

    SiteSuggestionsCoordinator(View parentView, Profile profile,
            SuggestionsNavigationDelegate navigationDelegate, ContextMenuManager contextMenuManager,
            ImageFetcher imageFetcher) {
        PropertyModel model =
                new PropertyModel.Builder(CURRENT_INDEX_KEY, SUGGESTIONS_KEY, ITEM_COUNT_KEY)
                        .with(SUGGESTIONS_KEY, new PropertyListModel<>())
                        .with(ITEM_COUNT_KEY, 1)
                        .build();
        View suggestionsView =
                ((ViewStub) parentView.findViewById(R.id.most_likely_stub)).inflate();
        Context context = parentView.getContext();

        RoundedIconGenerator iconGenerator = ViewUtils.createDefaultRoundedIconGenerator(
                context.getResources(), /* circularIcon = */ true);
        int iconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_min_size);

        LinearLayoutManager layoutManager = new SiteSuggestionsLayoutManager(context);
        RecyclerView recyclerView =
                suggestionsView.findViewById(R.id.most_likely_launcher_recycler);
        SiteSuggestionsAdapter adapterDelegate = new SiteSuggestionsAdapter(model, iconGenerator,
                navigationDelegate, contextMenuManager, layoutManager,
                suggestionsView.findViewById(R.id.most_likely_web_title_text));

        RecyclerViewAdapter<SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder, PropertyKey>
                adapter = new RecyclerViewAdapter<>(
                        adapterDelegate, new SiteSuggestionsViewHolderFactory());

        // Add spacing because tile margins get swallowed/overridden somehow.
        // TODO(chili): use layout margin.
        recyclerView.addItemDecoration(new RecyclerView.ItemDecoration() {
            @Override
            public void getItemOffsets(
                    Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
                outRect.bottom = context.getResources().getDimensionPixelSize(
                        R.dimen.most_likely_carousel_edge_spacer);
                outRect.left = context.getResources().getDimensionPixelSize(
                        R.dimen.most_likely_tile_horizontal_spacer);
                outRect.right = context.getResources().getDimensionPixelSize(
                        R.dimen.most_likely_tile_horizontal_spacer);
                outRect.top = context.getResources().getDimensionPixelSize(
                        R.dimen.most_likely_carousel_edge_spacer);
            }
        });
        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(adapter);

        mMediator = new SiteSuggestionsMediator(model, profile, imageFetcher, iconSize);
    }

    public void destroy() {
        mMediator.destroy();
    }
}
