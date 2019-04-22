// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.Manifest;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.net.MimeTypeFilter;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * A worker task to enumerate image files on disk.
 */
class FileEnumWorkerTask extends AsyncTask<List<PickerBitmap>> {
    /**
     * An interface to use to communicate back the results to the client.
     */
    public interface FilesEnumeratedCallback {
        /**
         * A callback to define to receive the list of all images on disk.
         * @param files The list of images.
         */
        void filesEnumeratedCallback(List<PickerBitmap> files);
    }

    private final WindowAndroid mWindowAndroid;

    // The callback to use to communicate the results.
    private FilesEnumeratedCallback mCallback;

    // The filter to apply to the list.
    private MimeTypeFilter mFilter;

    // The ContentResolver to use to retrieve image metadata from disk.
    private ContentResolver mContentResolver;

    // The camera directory undir DCIM.
    private static final String SAMPLE_DCIM_SOURCE_SUB_DIRECTORY = "Camera";

    /**
     * A FileEnumWorkerTask constructor.
     * @param windowAndroid The window wrapper associated with the current activity.
     * @param callback The callback to use to communicate back the results.
     * @param filter The file filter to apply to the list.
     * @param contentResolver The ContentResolver to use to retrieve image metadata from disk.
     */
    public FileEnumWorkerTask(WindowAndroid windowAndroid, FilesEnumeratedCallback callback,
            MimeTypeFilter filter, ContentResolver contentResolver) {
        mWindowAndroid = windowAndroid;
        mCallback = callback;
        mFilter = filter;
        mContentResolver = contentResolver;
    }

    /**
     * Retrieves the DCIM/camera directory.
     */
    private File getCameraDirectory() {
        return new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM),
                SAMPLE_DCIM_SOURCE_SUB_DIRECTORY);
    }

    /**
     * Enumerates (in the background) the image files on disk. Called on a non-UI thread
     * @param params Ignored, do not use.
     * @return A sorted list of images (by last-modified first).
     */
    @Override
    protected List<PickerBitmap> doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        List<PickerBitmap> pickerBitmaps = new ArrayList<>();

        final String[] selectColumns = {MediaStore.Images.Media._ID,
                MediaStore.Images.Media.DATE_TAKEN, MediaStore.Images.Media.DATA};

        String whereClause = null;
        String[] whereArgs = null;
        // Looks like we loose access to the filter, starting with the Q SDK.
        if (!BuildInfo.isAtLeastQ()) {
            whereClause = "(" + MediaStore.Images.Media.DATA + " LIKE ? OR "
                    + MediaStore.Images.Media.DATA + " LIKE ? OR " + MediaStore.Images.Media.DATA
                    + " LIKE ?) AND " + MediaStore.Images.Media.DATA + " NOT LIKE ?";

            whereArgs = new String[] {
                    // Include:
                    getCameraDirectory().toString() + "%",
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES)
                            + "%",
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                            + "%",
                    // Exclude low-quality sources, such as the screenshots directory:
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES)
                            + "/Screenshots/"
                            + "%"};
        }

        final String orderBy = MediaStore.Images.Media.DATE_TAKEN + " DESC";

        Cursor imageCursor = mContentResolver.query(MediaStore.Images.Media.EXTERNAL_CONTENT_URI,
                selectColumns, whereClause, whereArgs, orderBy);

        while (imageCursor.moveToNext()) {
            int dataIndex = imageCursor.getColumnIndex(MediaStore.Images.Media.DATA);
            int dateTakenIndex = imageCursor.getColumnIndex(MediaStore.Images.Media.DATE_TAKEN);
            int idIndex = imageCursor.getColumnIndex(MediaStore.Images.ImageColumns._ID);
            Uri uri = ContentUris.withAppendedId(
                    MediaStore.Images.Media.EXTERNAL_CONTENT_URI, imageCursor.getInt(idIndex));
            long dateTaken = imageCursor.getLong(dateTakenIndex);
            pickerBitmaps.add(new PickerBitmap(uri, dateTaken, PickerBitmap.TileTypes.PICTURE));
        }
        imageCursor.close();

        pickerBitmaps.add(0, new PickerBitmap(null, 0, PickerBitmap.TileTypes.GALLERY));
        boolean hasCameraAppAvailable =
                mWindowAndroid.canResolveActivity(new Intent(MediaStore.ACTION_IMAGE_CAPTURE));
        boolean hasOrCanRequestCameraPermission =
                mWindowAndroid.hasPermission(Manifest.permission.CAMERA)
                || mWindowAndroid.canRequestPermission(Manifest.permission.CAMERA);
        if (hasCameraAppAvailable && hasOrCanRequestCameraPermission) {
            pickerBitmaps.add(0, new PickerBitmap(null, 0, PickerBitmap.TileTypes.CAMERA));
        }

        return pickerBitmaps;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     * @param files The resulting list of files on disk.
     */
    @Override
    protected void onPostExecute(List<PickerBitmap> files) {
        if (isCancelled()) {
            return;
        }

        mCallback.filesEnumeratedCallback(files);
    }
}
