// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import org.chromium.base.ThreadUtils;

import android.content.Context;

public class PrintingControllerFactory {
    /** The singleton instance for this class. */
    private static PrintingController sInstance;

    private PrintingControllerFactory() {} // Static factory

    /**
     * Creates a controller for handling printing with the framework.
     *
     * The controller is a singleton, since there can be only one printing action at any time.
     *
     * @param context The application context.
     * @return The resulting PrintingController.
     */
    public static PrintingController getPrintingController(
            Context context) {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new PrintingControllerImpl(context);
        }
        return sInstance;
    }
}
