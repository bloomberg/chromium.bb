// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.robolectric.annotation.Config;
import org.robolectric.internal.ManifestFactory;
import org.robolectric.internal.ManifestIdentifier;
import org.robolectric.res.Fs;
import org.robolectric.res.FsFile;

import java.io.File;
import java.net.MalformedURLException;

/**
 * Class that manages passing Android manifest information to Robolectric.
 */
public class GNManifestFactory implements ManifestFactory {
    private static final String CHROMIUM_RES_APK = "chromium.robolectric.resource.ap_";

    @Override
    public ManifestIdentifier identify(Config config) {
        String resourceApk = System.getProperty(CHROMIUM_RES_APK);

        FsFile apkFile = null;
        try {
            apkFile = Fs.fromURL(new File(resourceApk).toURI().toURL());
        } catch (MalformedURLException e) {
        }
        return new ManifestIdentifier(config.packageName(), null, null, null, null, apkFile);
    }
}
