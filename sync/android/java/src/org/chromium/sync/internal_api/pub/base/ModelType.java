// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.internal_api.pub.base;

import android.util.Log;

import com.google.common.collect.HashMultimap;
import com.google.common.collect.Multimap;
import com.google.common.collect.Sets;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protos.ipc.invalidation.Types;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/**
 * The model types that are synced in Chrome for Android.
 */
public enum ModelType {
    /**
     * An autofill object.
     */
    AUTOFILL("AUTOFILL"),
    /**
     * An autofill profile object.
     */
    AUTOFILL_PROFILE("AUTOFILL_PROFILE"),
    /**
     * A bookmark folder or a bookmark URL object.
     */
    BOOKMARK("BOOKMARK"),
    /**
     * Flags to enable experimental features.
     */
    EXPERIMENTS("EXPERIMENTS"),
    /**
     * An object representing a set of Nigori keys.
     */
    NIGORI("NIGORI"),
    /**
     * A password entry.
     */
    PASSWORD("PASSWORD"),
    /**
     * An object representing a browser session or tab.
     */
    SESSION("SESSION"),
    /**
     * A typed_url folder or a typed_url object.
     */
    TYPED_URL("TYPED_URL"),
    /**
     * A history delete directive object.
     */
    HISTORY_DELETE_DIRECTIVE("HISTORY_DELETE_DIRECTIVE"),
    /**
     * A device info object.
     */
    DEVICE_INFO("DEVICE_INFO"),
    /**
     * A proxy tabs object (placeholder for sessions).
     */
    PROXY_TABS("NULL"),
    /**
     * A favicon image object.
     */
    FAVICON_IMAGE("FAVICON_IMAGE"),
    /**
     * A favicon tracking object.
     */
    FAVICON_TRACKING("FAVICON_TRACKING");

    /** Special type representing all possible types. */
    public static final String ALL_TYPES_TYPE = "ALL_TYPES";

    private static final String TAG = ModelType.class.getSimpleName();

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

    /**
     * Converts string representations of types to sync to {@link ModelType}s.
     * <p>
     * If {@code syncTypes} contains {@link #ALL_TYPES_TYPE}, then the returned
     * set contains all values of the {@code ModelType} enum.
     * <p>
     * Otherwise, the returned set contains the {@code ModelType} values for all elements of
     * {@code syncTypes} for which {@link ModelType#valueOf(String)} successfully returns; other
     * elements are dropped.
     */
    public static Set<ModelType> syncTypesToModelTypes(Collection<String> syncTypes) {
        if (syncTypes.contains(ALL_TYPES_TYPE)) {
            return Sets.newHashSet(ModelType.values());
        } else {
            Set<ModelType> modelTypes = Sets.newHashSetWithExpectedSize(syncTypes.size());
            for (String syncType : syncTypes) {
                try {
                    modelTypes.add(valueOf(syncType));
                } catch (IllegalArgumentException exception) {
                    // Drop invalid sync types.
                    Log.w(TAG, "Could not translate sync type to model type: " + syncType);
                }
            }
            return modelTypes;
        }
    }

    /** Converts a set of {@link ModelType} to a set of {@link ObjectId}. */
    public static Set<ObjectId> modelTypesToObjectIds(Set<ModelType> modelTypes) {
        Set<ObjectId> objectIds = Sets.newHashSetWithExpectedSize(modelTypes.size());
        for (ModelType modelType : modelTypes) {
            objectIds.add(modelType.toObjectId());
        }
        return objectIds;
    }

    /** Converts a set of {@link ModelType} to a set of string names. */
    public static Set<String> modelTypesToSyncTypes(Set<ModelType> modelTypes) {
        Set<String> objectIds = Sets.newHashSetWithExpectedSize(modelTypes.size());
        for (ModelType modelType : modelTypes) {
            objectIds.add(modelType.toString());
        }
        return objectIds;
    }
}
