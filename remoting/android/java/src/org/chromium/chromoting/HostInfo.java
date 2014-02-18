// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/** Class to represent a Host returned by {@link HostListLoader}. */
public class HostInfo {
    public final String name;
    public final String id;
    public final String jabberId;
    public final String publicKey;
    public final boolean isOnline;

    public HostInfo(String name, String id, String jabberId, String publicKey, boolean isOnline) {
        this.name = name;
        this.id = id;
        this.jabberId = jabberId;
        this.publicKey = publicKey;
        this.isOnline = isOnline;
    }
}
