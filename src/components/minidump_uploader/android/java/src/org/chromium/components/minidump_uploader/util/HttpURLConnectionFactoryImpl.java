// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader.util;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Default implementation of HttpURLConnectionFactory.
 */
public class HttpURLConnectionFactoryImpl implements HttpURLConnectionFactory {
    @Override
    public HttpURLConnection createHttpURLConnection(String url) {
        try {
            return (HttpURLConnection) new URL(url).openConnection();
        } catch (IOException e) {
            return null;
        }
    }
}
