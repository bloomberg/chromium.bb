// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.internal_api.pub.base;

import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protos.ipc.invalidation.Types;

/**
 * The model types that are synced in Chrome for Android.
 */
public enum ModelType {

    /**
     * A bookmark folder or a bookmark URL object.
     */
    BOOKMARK("BOOKMARK"),
    /**
     * A typed_url folder or a typed_url object.
     */
    TYPED_URL("TYPED_URL"),
    /**
     * An object representing a browser session or tab.
     */
    SESSION("SESSION");

    private final String mModelType;

    ModelType(String modelType) {
        mModelType = modelType;
    }

    public ObjectId toObjectId() {
        return ObjectId.newInstance(Types.ObjectSource.Type.CHROME_SYNC.getNumber(),
                mModelType.getBytes());
    }

    public static ModelType fromObjectId(ObjectId objectId) {
        try {
            return valueOf(new String(objectId.getName()));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }
}
