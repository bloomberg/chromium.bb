// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * A {@link TabListMediator.ThumbnailProvider} that will create a single Bitmap Thumbnail for all
 * the related tabs for the given tabs.
 */
public class MultiThumbnailCardProvider implements TabListMediator.ThumbnailProvider {
    private final TabContentManager mTabContentManager;
    private final TabModelSelector mTabModelSelector;

    private final float mPadding;
    private final float mRadius;
    private final Paint mEmptyThumbnailPaint;
    private final Paint mThumbnailFramePaint;
    private final Paint mTextPaint;
    private final int mSize;
    private final List<RectF> mRects = new ArrayList<>(4);

    private class MultiThumbnailFetcher {
        private final Tab mInitialTab;
        private final Callback<Bitmap> mFinalCallback;
        private final boolean mForceUpdate;
        private final List<Tab> mTabs = new ArrayList<>(4);
        private final AtomicInteger mThumbnailsToFetch = new AtomicInteger();

        private Canvas mCanvas;
        private Bitmap mMultiThumbnailBitmap;
        private String mText;

        MultiThumbnailFetcher(Tab initialTab, Callback<Bitmap> finalCallback, boolean forceUpdate) {
            mFinalCallback = finalCallback;
            mInitialTab = initialTab;
            mForceUpdate = forceUpdate;
        }

        private void initializeAndStartFetching(Tab tab) {
            // Initialize mMultiThumbnailBitmap.
            int width = mSize;
            int height = mSize;
            mMultiThumbnailBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mMultiThumbnailBitmap);
            mCanvas.drawColor(Color.TRANSPARENT);

            // Initialize Tabs.
            List<Tab> relatedTabList = new ArrayList<>();
            relatedTabList.addAll(mTabModelSelector.getTabModelFilterProvider()
                                          .getCurrentTabModelFilter()
                                          .getRelatedTabList(tab.getId()));
            if (relatedTabList.size() <= 4) {
                mThumbnailsToFetch.set(relatedTabList.size());

                mTabs.add(tab);
                relatedTabList.remove(tab);

                for (int i = 0; i < 3; i++) {
                    mTabs.add(i < relatedTabList.size() ? relatedTabList.get(i) : null);
                }
            } else {
                mText = "+" + (relatedTabList.size() - 3);
                mThumbnailsToFetch.set(3);

                mTabs.add(tab);
                relatedTabList.remove(tab);

                mTabs.add(relatedTabList.get(0));
                mTabs.add(relatedTabList.get(1));
                mTabs.add(null);
            }

            // Fetch and draw all.
            for (int i = 0; i < 4; i++) {
                if (mTabs.get(i) != null) {
                    final int index = i;
                    mTabContentManager.getTabThumbnailWithCallback(mTabs.get(i), result -> {
                        drawBitmapOnCanvasWithFrame(result, index, mEmptyThumbnailPaint);
                        if (mThumbnailsToFetch.decrementAndGet() == 0) {
                            PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE,
                                    () -> mFinalCallback.onResult(mMultiThumbnailBitmap));
                        }
                    }, mForceUpdate && i == 0);
                } else {
                    drawBitmapOnCanvasWithFrame(null, i, mEmptyThumbnailPaint);
                    if (mText != null && i == 3) {
                        // Draw the text exactly centered on the thumbnail rect.
                        mCanvas.drawText(mText, (mRects.get(i).left + mRects.get(i).right) / 2,
                                (mRects.get(i).top + mRects.get(i).bottom) / 2
                                        - ((mTextPaint.descent() + mTextPaint.ascent()) / 2),
                                mTextPaint);
                    }
                }
            }
        }

        private void drawBitmapOnCanvasWithFrame(
                Bitmap result, int index, Paint emptyThumbnailPaint) {
            // Draw the rounded rect. If Bitmap is not null, this is used for XferMode.
            mCanvas.drawRoundRect(mRects.get(index), mRadius, mRadius, emptyThumbnailPaint);

            if (result == null) return;

            result = Bitmap.createScaledBitmap(result, (int) mRects.get(index).width(),
                    (int) mRects.get(index).height(), true);

            emptyThumbnailPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
            mCanvas.drawBitmap(result, new Rect(0, 0, result.getWidth(), result.getHeight()),
                    mRects.get(index), emptyThumbnailPaint);
            result.recycle();

            emptyThumbnailPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_OVER));

            mCanvas.drawRoundRect(mRects.get(index), mRadius, mRadius, mThumbnailFramePaint);
        }

        private void fetch() {
            initializeAndStartFetching(mInitialTab);
        }
    }

    MultiThumbnailCardProvider(Context context, TabContentManager tabContentManager,
            TabModelSelector tabModelSelector) {
        mTabContentManager = tabContentManager;
        mTabModelSelector = tabModelSelector;
        mPadding = context.getResources().getDimension(R.dimen.tab_list_card_padding);
        mRadius = context.getResources().getDimension(R.dimen.tab_list_mini_card_radius);
        mSize = (int) context.getResources().getDimension(
                R.dimen.tab_grid_thumbnail_card_default_size);

        // Initialize Paints to use.
        mEmptyThumbnailPaint = new Paint();
        mEmptyThumbnailPaint.setStyle(Paint.Style.FILL);
        mEmptyThumbnailPaint.setColor(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.modern_grey_100));
        mEmptyThumbnailPaint.setAntiAlias(true);

        mThumbnailFramePaint = new Paint();
        mThumbnailFramePaint.setStyle(Paint.Style.STROKE);
        mThumbnailFramePaint.setStrokeWidth(
                context.getResources().getDimension(R.dimen.tab_list_mini_card_frame_size));
        mThumbnailFramePaint.setColor(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.modern_grey_300));
        mThumbnailFramePaint.setAntiAlias(true);

        mTextPaint = new Paint();
        mTextPaint.setTextSize(
                context.getResources().getDimension(R.dimen.compositor_tab_title_text_size));
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Paint.Align.CENTER);

        // Initialize Rects.
        mRects.add(
                new RectF(mPadding, mPadding, mSize / 2 - mPadding / 2, mSize / 2 - mPadding / 2));
        mRects.add(new RectF(
                mSize / 2 + mPadding / 2, mPadding, mSize - mPadding, mSize / 2 - mPadding / 2));
        mRects.add(new RectF(
                mPadding, mSize / 2 + mPadding / 2, mSize / 2 - mPadding / 2, mSize - mPadding));
        mRects.add(new RectF(mSize / 2 + mPadding / 2, mSize / 2 + mPadding / 2, mSize - mPadding,
                mSize - mPadding));
    }

    @Override
    public void getTabThumbnailWithCallback(
            Tab tab, Callback<Bitmap> finalCallback, boolean forceUpdate) {
        if (mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getRelatedTabList(tab.getId())
                        .size()
                == 1) {
            mTabContentManager.getTabThumbnailWithCallback(tab, finalCallback, forceUpdate);
            return;
        }

        new MultiThumbnailFetcher(tab, finalCallback, forceUpdate).fetch();
    }
}
