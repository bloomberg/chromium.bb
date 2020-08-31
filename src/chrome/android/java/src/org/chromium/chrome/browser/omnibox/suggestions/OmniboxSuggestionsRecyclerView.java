// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.KeyNavigationUtil;

/**
 * A widget for showing a list of omnibox suggestions.
 */
@VisibleForTesting
public class OmniboxSuggestionsRecyclerView
        extends RecyclerView implements OmniboxSuggestionsDropdown {
    private final OmniboxSuggestionsDropdownDelegate mDropdownDelegate;
    private final SuggestionScrollListener mScrollListener;
    private OmniboxSuggestionsRecyclerViewAdapter mAdapter;

    private final int[] mTempMeasureSpecs = new int[2];

    /** Scroll listener that propagates scroll event notification to registered observers. */
    private class SuggestionScrollListener extends RecyclerView.OnScrollListener {
        private OmniboxSuggestionsDropdown.Observer mObserver;

        void setObserver(OmniboxSuggestionsDropdown.Observer observer) {
            mObserver = observer;
        }

        @Override
        public void onScrolled(RecyclerView view, int dx, int dy) {}

        @Override
        public void onScrollStateChanged(RecyclerView view, int scrollState) {
            if (scrollState == SCROLL_STATE_DRAGGING && mObserver != null) {
                mObserver.onSuggestionDropdownScroll();
            }
        }
    }

    /**
     * RecyclerView pool that records performance of the view recycling mechanism.
     * @see OmniboxSuggestionsListViewListAdapter#canReuseView(View, int)
     */
    private class HistogramRecordingRecycledViewPool extends RecycledViewPool {
        HistogramRecordingRecycledViewPool() {
            setMaxRecycledViews(OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION, 1);
            setMaxRecycledViews(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION, 1);
            setMaxRecycledViews(OmniboxSuggestionUiType.ANSWER_SUGGESTION, 1);
            setMaxRecycledViews(OmniboxSuggestionUiType.DEFAULT, 15);
            setMaxRecycledViews(OmniboxSuggestionUiType.ENTITY_SUGGESTION, 5);
            setMaxRecycledViews(OmniboxSuggestionUiType.TAIL_SUGGESTION, 10);
        }

        @Override
        public ViewHolder getRecycledView(int viewType) {
            ViewHolder result = super.getRecycledView(viewType);
            SuggestionsMetrics.recordSuggestionViewReused(result != null);
            return result;
        }
    }

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     * @param context Context used for contained views.
     */
    public OmniboxSuggestionsRecyclerView(Context context) {
        super(context, null, android.R.attr.dropDownListViewStyle);
        setLayoutManager(new LinearLayoutManager(context));
        setFocusable(true);
        setFocusableInTouchMode(true);
        setRecycledViewPool(new HistogramRecordingRecycledViewPool());

        // By default RecyclerViews come with item animators.
        setItemAnimator(null);

        final Resources resources = context.getResources();
        int paddingBottom =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_bottom);
        ViewCompat.setPaddingRelative(this, 0, 0, 0, paddingBottom);

        mDropdownDelegate = new OmniboxSuggestionsDropdownDelegate(resources, this);
        mScrollListener = new SuggestionScrollListener();
        setOnScrollListener(mScrollListener);
    }

    @Override
    public ViewGroup getViewGroup() {
        return this;
    }

    @Override
    public void setEmbedder(OmniboxSuggestionsDropdown.Embedder embedder) {
        mDropdownDelegate.setEmbedder(embedder);
    }

    @Override
    public void setObserver(OmniboxSuggestionsDropdown.Observer observer) {
        mScrollListener.setObserver(observer);
        mDropdownDelegate.setObserver(observer);
    }

    @Override
    public void resetSelection() {
        if (mAdapter == null) return;
        mAdapter.resetSelection();
    }

    @Override
    public int getItemCount() {
        if (mAdapter == null) return 0;
        return mAdapter.getItemCount();
    }

    @Override
    public void show() {
        if (getVisibility() == VISIBLE) return;

        setVisibility(VISIBLE);
        if (mAdapter != null && mAdapter.getSelectedViewIndex() != 0) {
            mAdapter.resetSelection();
        }
    }

    @Override
    public void refreshPopupBackground(boolean isIncognito) {
        setBackground(mDropdownDelegate.getPopupBackground(isIncognito));
    }

    @Override
    public void setAdapter(Adapter adapter) {
        mAdapter = (OmniboxSuggestionsRecyclerViewAdapter) adapter;
        super.setAdapter(mAdapter);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Measure");
                SuggestionsMetrics.TimingMetric metric =
                        SuggestionsMetrics.recordSuggestionListMeasureTime()) {
            mDropdownDelegate.calculateOnMeasureAndTriggerUpdates(mTempMeasureSpecs);
            super.onMeasure(mTempMeasureSpecs[0], mTempMeasureSpecs[1]);
        }
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Layout");
                SuggestionsMetrics.TimingMetric metric =
                        SuggestionsMetrics.recordSuggestionListLayoutTime()) {
            super.onLayout(changed, l, t, r, b);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!isShown()) return false;

        int selectedPosition = mAdapter.getSelectedViewIndex();
        if (KeyNavigationUtil.isGoDown(event)) {
            return mAdapter.setSelectedViewIndex(selectedPosition + 1);
        } else if (KeyNavigationUtil.isGoUp(event)) {
            return mAdapter.setSelectedViewIndex(selectedPosition - 1);
        } else if (KeyNavigationUtil.isGoRight(event) || KeyNavigationUtil.isGoLeft(event)) {
            View selectedView = mAdapter.getSelectedView();
            if (selectedView != null) return selectedView.onKeyDown(keyCode, event);
        } else if (KeyNavigationUtil.isEnter(event)) {
            View selectedView = mAdapter.getSelectedView();
            if (selectedView != null) return selectedView.performClick();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        return mDropdownDelegate.shouldIgnoreGenericMotionEvent(event)
                || super.onGenericMotionEvent(event);
    }
}
