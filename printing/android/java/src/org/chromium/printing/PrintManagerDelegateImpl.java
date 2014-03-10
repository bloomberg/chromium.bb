// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.content.Context;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintManager;

/**
 * An implementation of {@link PrintManagerDelegate} using the Android framework print manager.
 */
public class PrintManagerDelegateImpl implements PrintManagerDelegate {
    private final PrintManager mPrintManager;

    public PrintManagerDelegateImpl(Context context) {
        mPrintManager =  (PrintManager) context.getSystemService(Context.PRINT_SERVICE);
    }

    @Override
    public void print(String printJobName, PrintDocumentAdapter documentAdapter,
            PrintAttributes attributes) {
        mPrintManager.print(printJobName, documentAdapter, attributes);
    }

}
