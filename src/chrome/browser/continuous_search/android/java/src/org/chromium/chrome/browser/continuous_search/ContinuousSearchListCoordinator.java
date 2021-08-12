// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.LinearSmoothScroller;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.VisibilitySettings;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Coordinator for Continuous Search Navigation UI: A RecyclerView that displays items provided
 * by the CSN infrastructure.
 */
public class ContinuousSearchListCoordinator {
    private final ContinuousSearchListMediator mListMediator;
    private final SimpleRecyclerViewAdapter mRecyclerViewAdapter;
    private final ObservableSupplier<Tab> mTabSupplier;
    private final PropertyModel mRootViewModel;
    private final Resources mResources;

    public ContinuousSearchListCoordinator(
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<Tab> tabSupplier, Callback<VisibilitySettings> setLayoutVisibility,
            ThemeColorProvider themeColorProvider, Resources resources) {
        ContinuousSearchConfiguration.initialize();
        mRootViewModel = new PropertyModel(ContinuousSearchListProperties.ALL_KEYS);
        ModelList listItems = new ModelList();
        mRecyclerViewAdapter = new SimpleRecyclerViewAdapter(listItems);
        mResources = resources;
        mListMediator = new ContinuousSearchListMediator(browserControlsStateProvider, listItems,
                mRootViewModel, setLayoutVisibility, themeColorProvider, resources);

        boolean twoLineChip = mListMediator.shouldShowResultTitle();
        mRecyclerViewAdapter.registerType(ListItemType.SEARCH_RESULT,
                (parent)
                        -> inflateListItemView(parent, ListItemType.SEARCH_RESULT, twoLineChip),
                ContinuousSearchListViewBinder::bindListItem);
        mRecyclerViewAdapter.registerType(ListItemType.AD,
                (parent)
                        -> inflateListItemView(parent, ListItemType.AD, twoLineChip),
                ContinuousSearchListViewBinder::bindListItem);

        mTabSupplier = tabSupplier;
        mTabSupplier.addObserver(mListMediator);
    }

    private View inflateListItemView(
            ViewGroup parentView, @ListItemType int listItemType, boolean twoLineChip) {
        int layoutId = 0;
        switch (listItemType) {
            case ListItemType.SEARCH_RESULT:
                layoutId = R.layout.continuous_search_list_item;
                break;
            case ListItemType.AD:
                layoutId = R.layout.continuous_search_list_ad;
                break;
            case ListItemType.DEPRECATED_PROVIDER:
                assert false : "CSN provider should not be a ListItem";
        }

        View view =
                LayoutInflater.from(parentView.getContext()).inflate(layoutId, parentView, false);
        if (twoLineChip) ((ContinuousSearchChipView) view).initTwoLineChipView();
        return view;
    }

    void initializeLayout(ViewGroup container) {
        View rootView = LayoutInflater.from(container.getContext())
                                .inflate(R.layout.continuous_search_layout, container, false);
        PropertyModelChangeProcessor.create(
                mRootViewModel, rootView, ContinuousSearchListViewBinder::bindRootView);

        ViewGroup.LayoutParams lp = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        container.addView(rootView, /*index=*/0, lp);

        RecyclerView recyclerView = rootView.findViewById(R.id.recycler_view);

        OnScrollListener userInputScrollListener = new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                // onScrolled is also called after a layout calculation. dx will be 0 in that case.
                if (dx != 0) mListMediator.onScrolled();
            }
        };
        // TODO(crbug.com/1231562): Add tests.
        LinearLayoutManager layoutManager = new LinearLayoutManager(
                container.getContext(), LinearLayoutManager.HORIZONTAL, false) {
            @Override
            public void smoothScrollToPosition(
                    RecyclerView recyclerView, State state, int position) {
                LinearSmoothScroller scroller =
                        new LinearSmoothScroller(recyclerView.getContext()) {
                            @Override
                            public int calculateDtToFit(int viewStart, int viewEnd, int boxStart,
                                    int boxEnd, int snapPreference) {
                                // Return distance between visible view's center and selected item's
                                // center
                                return (boxStart + (boxEnd - boxStart) / 2)
                                        - (viewStart + (viewEnd - viewStart) / 2);
                            }
                        };
                // Remove the user input OnScrollListener to avoid calling onScrolled() when the
                // scroll is done programmatically.
                recyclerView.removeOnScrollListener(userInputScrollListener);
                recyclerView.addOnScrollListener(new OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(
                            @NonNull RecyclerView recyclerView, int newState) {
                        if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                            recyclerView.removeOnScrollListener(this);
                            recyclerView.addOnScrollListener(userInputScrollListener);
                        }
                    }
                });

                scroller.setTargetPosition(position);
                startSmoothScroll(scroller);
            }
        };
        recyclerView.setLayoutManager(layoutManager);
        recyclerView.addItemDecoration(new SpaceItemDecoration(mResources));
        recyclerView.setAdapter(mRecyclerViewAdapter);
        recyclerView.addOnScrollListener(userInputScrollListener);
    }

    void destroy() {
        mTabSupplier.removeObserver(mListMediator);
        mListMediator.destroy();
    }

    @VisibleForTesting
    ContinuousSearchListMediator getMediatorForTesting() {
        return mListMediator;
    }

    private static class SpaceItemDecoration extends ItemDecoration {
        private final int mChipSpacingPx;
        private final int mSidePaddingPx;

        public SpaceItemDecoration(Resources resources) {
            mChipSpacingPx =
                    (int) resources.getDimensionPixelSize(R.dimen.csn_chip_list_chip_spacing);
            mSidePaddingPx =
                    (int) resources.getDimensionPixelSize(R.dimen.csn_chip_list_side_padding);
        }

        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
            int position = parent.getChildAdapterPosition(view);
            boolean isFirst = position == 0;
            boolean isLast = position == parent.getAdapter().getItemCount() - 1;

            // Provider icon/text already has the required padding so the first item doesn't need to
            // have any.
            outRect.left = isFirst ? 0 : mChipSpacingPx;
            outRect.right = isLast ? mSidePaddingPx : mChipSpacingPx;
        }
    }
}
