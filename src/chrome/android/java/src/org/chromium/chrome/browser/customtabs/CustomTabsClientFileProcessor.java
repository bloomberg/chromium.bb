// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.support.annotation.WorkerThread;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen.SplashImageHolder;

import java.io.FileDescriptor;
import java.io.IOException;

import javax.inject.Inject;
import javax.inject.Named;
import javax.inject.Singleton;

import dagger.Lazy;

/**
 * Processes the files received via Custom Tab connection from client apps.
 */
@Singleton
public class CustomTabsClientFileProcessor {

    private static final String TAG = "CustomTabFiles";

    private final Context mContext;
    private final Lazy<SplashImageHolder> mTwaSplashImageHolder;
    private boolean mTwaSplashImageHolderCreated;

    @Inject
    public CustomTabsClientFileProcessor(@Named(APP_CONTEXT) Context context,
            Lazy<SplashImageHolder> twaSplashImageHolder) {
        mTwaSplashImageHolder = twaSplashImageHolder;
        mContext = context;
    }

    /**
     * Processes the file located at given URI.
     * @return {@code true} if successful.
     */
    @WorkerThread
    public boolean processFile(CustomTabsSessionToken session, Uri uri,
            int purpose, Bundle extras) {
        if (uri == null) {
            Log.w(TAG, "Received a null uri");
            return false;
        }
        switch (purpose) {
            case CustomTabsService.FILE_PURPOSE_TWA_SPLASH_IMAGE:
                return receiveTwaSplashImage(session, uri);
        }
        Log.w(TAG, "Unknown FilePurpose " + purpose);
        return false;
    }

    private boolean receiveTwaSplashImage(CustomTabsSessionToken sessionToken, Uri uri) {
        try(ParcelFileDescriptor parcelFileDescriptor =
                mContext.getContentResolver().openFileDescriptor(uri, "r")) {
            if (parcelFileDescriptor == null) {
                Log.w(TAG, "Null ParcelFileDescriptor from uri " + uri);
                return false;
            }
            FileDescriptor fileDescriptor = parcelFileDescriptor.getFileDescriptor();
            if (fileDescriptor == null) {
                Log.w(TAG, "Null FileDescriptor from uri " + uri);
                return false;
            }
            Bitmap bitmap = BitmapFactory.decodeFileDescriptor(fileDescriptor);
            if (bitmap == null) {
                Log.w(TAG, "Failed to decode image from uri " + uri);
                return false;
            }
            mTwaSplashImageHolder.get().putImage(sessionToken, bitmap);
            mTwaSplashImageHolderCreated = true;
            return true;
        } catch (IOException e) {
            Log.w(TAG, "IO exception when reading uri " + uri);
            return false;
        }
    }

    /**
     * Cleans up files associated with the session that has been disconnected.
     */
    public void onSessionDisconnected(CustomTabsSessionToken session) {
        if (mTwaSplashImageHolderCreated) {
            // If the image still hasn't been claimed, delete it.
            mTwaSplashImageHolder.get().takeImage(session);
        }
    }
}
