// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.util.concurrent.Executor;

/**
 * Boundary interface for ProxyController.
 */
public interface ProxyControllerBoundaryInterface {
    void setProxyOverride(
            String[][] proxyRules, String[] bypassRules, Runnable listener, Executor executor);
    void clearProxyOverride(Runnable listener, Executor executor);
}
