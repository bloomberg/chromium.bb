// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;

import java.util.List;

/**
 * Holds on to a {@link PickerBitmapView} that displays information about a picker bitmap.
 */
public class PickerBitmapViewHolder
        extends ViewHolder implements DecoderServiceHost.ImageDecodedCallback {
    // Our parent category.
    private PickerCategoryView mCategoryView;

    // The bitmap view we are holding on to.
    private final PickerBitmapView mItemView;

    // The request we are showing the bitmap for.
    private PickerBitmap mBitmapDetails;

    /**
     * The PickerBitmapViewHolder.
     * @param itemView The {@link PickerBitmapView} view for showing the image.
     */
    public PickerBitmapViewHolder(PickerBitmapView itemView) {
        super(itemView);
        mItemView = itemView;
    }

    // DecoderServiceHost.ImageDecodedCallback

    @Override
    public void imageDecodedCallback(String filePath, Bitmap bitmap, String videoDuration) {
        if (bitmap == null || bitmap.getWidth() == 0 || bitmap.getHeight() == 0) {
            return;
        }

        if (mCategoryView.getHighResThumbnails().get(filePath) == null) {
            mCategoryView.getHighResThumbnails().put(
                    filePath, new PickerCategoryView.Thumbnail(bitmap, videoDuration));
        }

        if (mCategoryView.getLowResThumbnails().get(filePath) == null) {
            Resources resources = mItemView.getContext().getResources();
            new BitmapScalerTask(mCategoryView.getLowResThumbnails(), bitmap, filePath,
                    videoDuration,
                    resources.getDimensionPixelSize(R.dimen.photo_picker_grainy_thumbnail_size))
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        if (!TextUtils.equals(mBitmapDetails.getUri().getPath(), filePath)) {
            return;
        }

        if (mItemView.setThumbnailBitmap(bitmap, videoDuration)) {
            mItemView.fadeInThumbnail();
        }
    }

    /**
     * Display a single item from |position| in the PickerCategoryView.
     * @param categoryView The PickerCategoryView to use to fetch the image.
     * @param position The position of the item to fetch.
     * @return The decoding action required to display the item.
     */
    public @PickerAdapter.DecodeActions int displayItem(
            PickerCategoryView categoryView, int position) {
        mCategoryView = categoryView;

        List<PickerBitmap> pickerBitmaps = mCategoryView.getPickerBitmaps();
        mBitmapDetails = pickerBitmaps.get(position);

        if (mBitmapDetails.type() == PickerBitmap.TileTypes.CAMERA
                || mBitmapDetails.type() == PickerBitmap.TileTypes.GALLERY) {
            mItemView.initialize(mBitmapDetails, null, null, false);
            return PickerAdapter.DecodeActions.NO_ACTION;
        }

        String filePath = mBitmapDetails.getUri().getPath();
        PickerCategoryView.Thumbnail original = mCategoryView.getHighResThumbnails().get(filePath);
        if (original != null) {
            mItemView.initialize(mBitmapDetails, original.bitmap, original.videoDuration, false);
            return PickerAdapter.DecodeActions.FROM_CACHE;
        }

        int size = mCategoryView.getImageSize();
        PickerCategoryView.Thumbnail payload = mCategoryView.getLowResThumbnails().get(filePath);
        if (payload != null) {
            Bitmap placeholder = payload.bitmap;
            // For performance stats see http://crbug.com/719919.
            long begin = SystemClock.elapsedRealtime();
            placeholder = BitmapUtils.scale(placeholder, size, false);
            long scaleTime = SystemClock.elapsedRealtime() - begin;
            RecordHistogram.recordTimesHistogram(
                    "Android.PhotoPicker.UpscaleLowResBitmap", scaleTime);

            mItemView.initialize(mBitmapDetails, placeholder, payload.videoDuration, true);
        } else {
            mItemView.initialize(mBitmapDetails, null, null, true);
        }

        mCategoryView.getDecoderServiceHost().decodeImage(
                mBitmapDetails.getUri(), mBitmapDetails.type(), size, this);
        return PickerAdapter.DecodeActions.DECODE;
    }

    /**
     * Returns the file path of the current request, or null if no request is in progress for this
     * holder.
     */
    public String getFilePath() {
        if (mBitmapDetails == null || mBitmapDetails.type() != PickerBitmap.TileTypes.PICTURE)
            return null;
        return mBitmapDetails.getUri().getPath();
    }
}
