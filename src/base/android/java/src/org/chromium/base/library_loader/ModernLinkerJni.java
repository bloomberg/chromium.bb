// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.chromium.base.annotations.JniIgnoreNatives;

/**
 * Mockable stub for all native methods in ModernLinker.
 *
 * See LinkerJni.java for an explanation of why @JniIgnoreNatives is needed.
 */
@JniIgnoreNatives
class ModernLinkerJni implements ModernLinker.Natives {
    @Override
    public boolean loadLibrary(
            String libFilePath, Linker.LibInfo libInfo, boolean spawnRelroRegion) {
        return nativeLoadLibrary(libFilePath, libInfo, spawnRelroRegion);
    }

    @Override
    public boolean useRelros(Linker.LibInfo libInfo) {
        return nativeUseRelros(libInfo);
    }

    @Override
    public int getRelroSharingResult() {
        return nativeGetRelroSharingResult();
    }

    private static native boolean nativeLoadLibrary(
            String libFilePath, Linker.LibInfo libInfo, boolean spawnRelroRegion);
    private static native boolean nativeUseRelros(Linker.LibInfo libInfo);
    private static native int nativeGetRelroSharingResult();
}
