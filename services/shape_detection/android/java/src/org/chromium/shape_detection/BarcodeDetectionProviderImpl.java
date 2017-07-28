// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.vision.barcode.Barcode;

import org.chromium.base.ContextUtils;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.BarcodeDetectionProvider;
import org.chromium.shape_detection.mojom.BarcodeDetectorOptions;

/**
 * Service provider to create BarcodeDetection services
 */
public class BarcodeDetectionProviderImpl implements BarcodeDetectionProvider {
    private static final boolean sIsGmsCoreSupported;

    static {
        sIsGmsCoreSupported = GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                                      ContextUtils.getApplicationContext())
                == ConnectionResult.SUCCESS;
    }

    public BarcodeDetectionProviderImpl() {}

    @Override
    public void createBarcodeDetection(
            InterfaceRequest<BarcodeDetection> request, BarcodeDetectorOptions options) {
        if (!sIsGmsCoreSupported) return;

        BarcodeDetection.MANAGER.bind(new BarcodeDetectionImpl(options), request);
    }

    @Override
    public void enumerateSupportedFormats(EnumerateSupportedFormatsResponse callback) {
        if (!sIsGmsCoreSupported) {
            int[] supportedFormats = {};
            callback.call(supportedFormats);
            return;
        }
        int[] supportedFormats = {Barcode.AZTEC, Barcode.CODE_128, Barcode.CODE_39, Barcode.CODE_93,
                Barcode.CODABAR, Barcode.DATA_MATRIX, Barcode.EAN_13, Barcode.EAN_8, Barcode.ITF,
                Barcode.PDF417, Barcode.QR_CODE, Barcode.UPC_A, Barcode.UPC_E};
        callback.call(supportedFormats);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    /**
     * A factory class to register BarcodeDetectionProvider interface.
     */
    public static class Factory implements InterfaceFactory<BarcodeDetectionProvider> {
        public Factory() {}

        @Override
        public BarcodeDetectionProvider createImpl() {
            return new BarcodeDetectionProviderImpl();
        }
    }
}
