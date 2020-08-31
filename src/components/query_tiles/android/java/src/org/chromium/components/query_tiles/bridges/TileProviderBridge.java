// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles.bridges;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TileProvider;

import java.util.List;

/**
 * Bridge to the native query tile service for the given {@link Profile}.
 */
@JNINamespace("query_tiles")
public class TileProviderBridge implements TileProvider {
    private long mNativeTileProviderBridge;

    @CalledByNative
    private static TileProviderBridge create(long nativePtr) {
        return new TileProviderBridge(nativePtr);
    }

    private TileProviderBridge(long nativePtr) {
        mNativeTileProviderBridge = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeTileProviderBridge = 0;
    }

    @Override
    public void getQueryTiles(Callback<List<QueryTile>> callback) {
        if (mNativeTileProviderBridge == 0) return;
        TileProviderBridgeJni.get().getQueryTiles(mNativeTileProviderBridge, this, callback);
    }

    @NativeMethods
    interface Natives {
        void getQueryTiles(long nativeTileProviderBridge, TileProviderBridge caller,
                Callback<List<QueryTile>> callback);
    }
}
