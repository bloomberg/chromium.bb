// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Point;
import android.graphics.Rect;
import android.util.SparseArray;

import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.gfx.mojom.PointF;
import org.chromium.gfx.mojom.RectF;
import org.chromium.mojo.system.MojoException;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.BarcodeDetectionResult;
import org.chromium.shape_detection.mojom.BarcodeDetectorOptions;
import org.chromium.shape_detection.mojom.BarcodeFormat;

/**
 * Implementation of mojo BarcodeDetection, using Google Play Services vision package.
 */
public class BarcodeDetectionImpl implements BarcodeDetection {
    private static final String TAG = "BarcodeDetectionImpl";

    private BarcodeDetector mBarcodeDetector;

    public BarcodeDetectionImpl(BarcodeDetectorOptions options) {
        // TODO(mcasas): extract the barcode formats to hunt for out of
        // |options| and use them for building |mBarcodeDetector|.
        // https://crbug.com/582266.
        mBarcodeDetector =
                new BarcodeDetector.Builder(ContextUtils.getApplicationContext()).build();
    }

    @Override
    public void detect(org.chromium.skia.mojom.Bitmap bitmapData, DetectResponse callback) {
        // The vision library will be downloaded the first time the API is used
        // on the device; this happens "fast", but it might have not completed,
        // bail in this case. Also, the API was disabled between and v.9.0 and
        // v.9.2, see https://developers.google.com/android/guides/releases.
        if (!mBarcodeDetector.isOperational()) {
            Log.e(TAG, "BarcodeDetector is not operational");
            callback.call(new BarcodeDetectionResult[0]);
            return;
        }

        Frame frame = BitmapUtils.convertToFrame(bitmapData);
        if (frame == null) {
            Log.e(TAG, "Error converting Mojom Bitmap to Frame");
            callback.call(new BarcodeDetectionResult[0]);
            return;
        }

        final SparseArray<Barcode> barcodes = mBarcodeDetector.detect(frame);

        BarcodeDetectionResult[] barcodeArray = new BarcodeDetectionResult[barcodes.size()];
        for (int i = 0; i < barcodes.size(); i++) {
            barcodeArray[i] = new BarcodeDetectionResult();
            final Barcode barcode = barcodes.valueAt(i);
            barcodeArray[i].rawValue = barcode.rawValue;
            final Rect rect = barcode.getBoundingBox();
            barcodeArray[i].boundingBox = new RectF();
            barcodeArray[i].boundingBox.x = rect.left;
            barcodeArray[i].boundingBox.y = rect.top;
            barcodeArray[i].boundingBox.width = rect.width();
            barcodeArray[i].boundingBox.height = rect.height();
            final Point[] corners = barcode.cornerPoints;
            barcodeArray[i].cornerPoints = new PointF[corners.length];
            for (int j = 0; j < corners.length; j++) {
                barcodeArray[i].cornerPoints[j] = new PointF();
                barcodeArray[i].cornerPoints[j].x = corners[j].x;
                barcodeArray[i].cornerPoints[j].y = corners[j].y;
            }
            barcodeArray[i].format = toBarcodeFormat(barcode.format);
        }
        callback.call(barcodeArray);
    }

    @Override
    public void close() {
        mBarcodeDetector.release();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    private int toBarcodeFormat(int format) {
        switch (format) {
            case Barcode.CODE_128:
                return BarcodeFormat.CODE_128;
            case Barcode.CODE_39:
                return BarcodeFormat.CODE_39;
            case Barcode.CODE_93:
                return BarcodeFormat.CODE_93;
            case Barcode.CODABAR:
                return BarcodeFormat.CODABAR;
            case Barcode.DATA_MATRIX:
                return BarcodeFormat.DATA_MATRIX;
            case Barcode.EAN_13:
                return BarcodeFormat.EAN_13;
            case Barcode.EAN_8:
                return BarcodeFormat.CODE_128;
            case Barcode.ITF:
                return BarcodeFormat.EAN_8;
            case Barcode.QR_CODE:
                return BarcodeFormat.QR_CODE;
            case Barcode.UPC_A:
                return BarcodeFormat.UPC_A;
            case Barcode.UPC_E:
                return BarcodeFormat.UPC_E;
            case Barcode.PDF417:
                return BarcodeFormat.PDF417;
            case Barcode.AZTEC:
                return BarcodeFormat.AZTEC;
        }
        return BarcodeFormat.UNKNOWN;
    }
}
