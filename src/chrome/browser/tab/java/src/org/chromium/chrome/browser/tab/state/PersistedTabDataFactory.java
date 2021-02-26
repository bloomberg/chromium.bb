// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

/**
 * Creates a {@link PersistedTabData}
 * @param <T> {@link PersistedTabData} being created
 */
public interface PersistedTabDataFactory<T extends PersistedTabData> {
    /**
     * @param data serialized {@link PersistedTabData}
     * @param storage storage method {@link PersistedTabDataStorage} for {@link PersistedTabData}
     * @param id identifier for {@link PersistedTabData} in storage
     */
    T create(byte[] data, PersistedTabDataStorage storage, String id);
}
