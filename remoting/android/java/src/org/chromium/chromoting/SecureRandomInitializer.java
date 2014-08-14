// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import java.io.FileInputStream;
import java.io.IOException;
import java.security.SecureRandom;

/**
 * This class contains code to initialize a SecureRandom generator securely on Android platforms
 * <= 4.3. See
 * {@link http://android-developers.blogspot.com/2013/08/some-securerandom-thoughts.html}.
 */
public class SecureRandomInitializer {
    private static final int NUM_RANDOM_BYTES = 16;

    /**
     * Safely initializes the random number generator, by seeding it with data from /dev/urandom.
     */
    public static void initialize(SecureRandom generator) throws IOException {
        FileInputStream fis = null;
        try {
            fis = new FileInputStream("/dev/urandom");
            byte[] bytes = new byte[NUM_RANDOM_BYTES];
            if (bytes.length != fis.read(bytes)) {
                throw new IOException("Failed to get enough random data.");
            }
            generator.setSeed(bytes);
        } finally {
            try {
                if (fis != null) {
                    fis.close();
                }
            } catch (IOException e) {
                // Ignore exception closing the device.
            }
        }
    }
}
