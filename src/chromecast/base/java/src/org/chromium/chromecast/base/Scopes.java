// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Helper methods to construct and compose Scope objects.
 */
public class Scopes {
    private Scopes() {} // Uninstantiable.

    // Use this when a Scope that does nothing is needed to avoid unnecessary heap allocations.
    public static final Scope NO_OP = () -> {};
}
