// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ItemDecoration;
import android.support.v7.widget.RecyclerView.Recycler;
import android.support.v7.widget.RecyclerView.State;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator.DateOrderedListObserver;
import org.chromium.chrome.browser.download.home.list.holder.ListItemViewHolder;
import org.chromium.chrome.browser.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * The View component of a DateOrderedList.  This takes the DateOrderedListModel and creates the
 * glue to display it on the screen.
 */
class DateOrderedListView {
    private final DownloadManagerUiConfig mConfig;
    private final DecoratedListItemModel mModel;

    private final int mIdealImageWidthPx;
    private final int mImagePaddingPx;
    private final int mPrefetchVerticalPaddingPx;
    private final int mPrefetchHorizontalPaddingPx;
    private final int mMaxWidthImageItemPx;

    private final RecyclerView mView;
    private final UiConfig mUiConfig;

    /** Creates an instance of a {@link DateOrderedListView} representing {@code model}. */
    public DateOrderedListView(Context context, DownloadManagerUiConfig config,
            DecoratedListItemModel model, DateOrderedListObserver dateOrderedListObserver) {
        mConfig = config;
        mModel = model;

        mIdealImageWidthPx = context.getResources().getDimensionPixelSize(
                R.dimen.download_manager_ideal_image_width);
        mImagePaddingPx = context.getResources().getDimensionPixelOffset(
                R.dimen.download_manager_image_padding);
        mPrefetchHorizontalPaddingPx = context.getResources().getDimensionPixelSize(
                R.dimen.download_manager_prefetch_horizontal_margin);
        mPrefetchVerticalPaddingPx = context.getResources().getDimensionPixelSize(
                R.dimen.download_manager_prefetch_vertical_margin);
        mMaxWidthImageItemPx = context.getResources().getDimensionPixelSize(
                R.dimen.download_manager_max_image_item_width_wide_screen);

        mView = new RecyclerView(context) {
            private int mScreenOrientation = Configuration.ORIENTATION_UNDEFINED;

            @Override
            protected void onConfigurationChanged(Configuration newConfig) {
                super.onConfigurationChanged(newConfig);
                mUiConfig.updateDisplayStyle();
                if (newConfig.orientation == mScreenOrientation) return;

                mScreenOrientation = newConfig.orientation;
                mView.invalidateItemDecorations();
            }
        };
        mView.setHasFixedSize(true);
        ((DefaultItemAnimator) mView.getItemAnimator()).setSupportsChangeAnimations(false);
        mView.getItemAnimator().setMoveDuration(0);
        mView.setLayoutManager(new GridLayoutManagerImpl(context));
        mView.addItemDecoration(new ItemDecorationImpl());

        PropertyModelChangeProcessor.create(
                mModel.getProperties(), mView, new ListPropertyViewBinder());

        // Do the final hook up to the underlying data adapter.
        DateOrderedListViewAdapter adapter = new DateOrderedListViewAdapter(
                mModel, new ModelChangeProcessor(mModel), ListItemViewHolder::create);
        mView.setAdapter(adapter);
        mView.post(adapter::notifyDataSetChanged);
        mView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView view, int dx, int dy) {
                dateOrderedListObserver.onListScroll(mView.canScrollVertically(-1));
            }
        });

        mUiConfig = new UiConfig(mView);
        mUiConfig.addObserver((newDisplayStyle) -> {
            int padding = getPaddingForDisplayStyle(newDisplayStyle, context.getResources());
            ViewCompat.setPaddingRelative(
                    mView, padding, mView.getPaddingTop(), padding, mView.getPaddingBottom());
        });
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }

    /**
     * @return The start and end padding of the recycler view for the given display style.
     */
    private static int getPaddingForDisplayStyle(
            UiConfig.DisplayStyle displayStyle, Resources resources) {
        int padding = 0;
        if (displayStyle.horizontal == HorizontalDisplayStyle.WIDE) {
            int screenWidthDp = resources.getConfiguration().screenWidthDp;
            padding = (int) (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f)
                    * resources.getDisplayMetrics().density);
            padding = (int) Math.max(
                    resources.getDimensionPixelSize(
                            R.dimen.download_manager_recycler_view_min_padding_wide_screen),
                    padding);
        }
        return padding;
    }

    /** @return The view width available after start and end padding. */
    private int getAvailableViewWidth() {
        return mView.getWidth() - ViewCompat.getPaddingStart(mView)
                - ViewCompat.getPaddingEnd(mView);
    }

    private class GridLayoutManagerImpl extends GridLayoutManager {
        /** Creates an instance of a {@link GridLayoutManagerImpl}. */
        public GridLayoutManagerImpl(Context context) {
            super(context, 1 /* spanCount */, VERTICAL, false /* reverseLayout */);
            setSpanSizeLookup(new SpanSizeLookupImpl());
        }

        // GridLayoutManager implementation.
        @Override
        public void onLayoutChildren(Recycler recycler, State state) {
            assert getOrientation() == VERTICAL;

            int availableWidth = getAvailableViewWidth() - mImagePaddingPx;
            int columnWidth = mIdealImageWidthPx - mImagePaddingPx;

            int easyFitSpan = availableWidth / columnWidth;
            double remaining =
                    ((double) (availableWidth - easyFitSpan * columnWidth)) / columnWidth;
            if (remaining > 0.5) easyFitSpan++;
            setSpanCount(Math.max(1, easyFitSpan));

            super.onLayoutChildren(recycler, state);
        }

        @Override
        public boolean supportsPredictiveItemAnimations() {
            return false;
        }

        private class SpanSizeLookupImpl extends SpanSizeLookup {
            // SpanSizeLookup implementation.
            @Override
            public int getSpanSize(int position) {
                return ListUtils.getSpanSize(mModel.get(position), mConfig, getSpanCount());
            }
        }
    }

    private class ItemDecorationImpl extends ItemDecoration {
        // ItemDecoration implementation.
        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
            int position = parent.getChildAdapterPosition(view);
            if (position < 0 || position >= mModel.size()) return;

            ListItem item = mModel.get(position);
            boolean isFullWidthMedia = false;
            switch (ListUtils.getViewTypeForItem(mModel.get(position), mConfig)) {
                case ListUtils.ViewType.IMAGE:
                case ListUtils.ViewType.IMAGE_FULL_WIDTH:
                case ListUtils.ViewType.IN_PROGRESS_IMAGE:
                    outRect.left = mImagePaddingPx;
                    outRect.right = mImagePaddingPx;
                    outRect.top = mImagePaddingPx;
                    outRect.bottom = mImagePaddingPx;
                    isFullWidthMedia = ((ListItem.OfflineItemListItem) item).spanFullWidth;
                    break;
                case ListUtils.ViewType.VIDEO: // Intentional fallthrough.
                case ListUtils.ViewType.IN_PROGRESS_VIDEO:
                    outRect.left = mPrefetchHorizontalPaddingPx;
                    outRect.right = mPrefetchHorizontalPaddingPx;
                    outRect.top = mPrefetchVerticalPaddingPx / 2;
                    outRect.bottom = mPrefetchVerticalPaddingPx / 2;
                    isFullWidthMedia = true;
                    break;
                case ListUtils.ViewType.PREFETCH:
                    outRect.left = mPrefetchHorizontalPaddingPx;
                    outRect.right = mPrefetchHorizontalPaddingPx;
                    outRect.top = mPrefetchVerticalPaddingPx / 2;
                    outRect.bottom = mPrefetchVerticalPaddingPx / 2;
                    break;
            }

            if (isFullWidthMedia
                    && mUiConfig.getCurrentDisplayStyle().horizontal
                            == HorizontalDisplayStyle.WIDE) {
                outRect.right += Math.max(getAvailableViewWidth() - mMaxWidthImageItemPx, 0);
            }
        }
    }

    private class ModelChangeProcessor extends ForwardingListObservable<Void>
            implements RecyclerViewAdapter.Delegate<ListItemViewHolder, Void> {
        private final DecoratedListItemModel mModel;

        public ModelChangeProcessor(DecoratedListItemModel model) {
            mModel = model;
            model.addObserver(this);
        }

        @Override
        public int getItemCount() {
            return mModel.size();
        }

        @Override
        public int getItemViewType(int position) {
            return ListUtils.getViewTypeForItem(mModel.get(position), mConfig);
        }

        @Override
        public void onBindViewHolder(
                ListItemViewHolder viewHolder, int position, @Nullable Void payload) {
            viewHolder.bind(mModel.getProperties(), mModel.get(position));
        }

        @Override
        public void onViewRecycled(ListItemViewHolder viewHolder) {
            viewHolder.recycle();
        }
    }
}
