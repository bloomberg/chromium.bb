// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import com.google.common.collect.Multimap;

import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.HashSet;
import java.util.Set;

class ModelTypeResolverImpl implements ModelTypeResolver {
    @Override
    public Set<ModelType> resolveModelTypes(Set<ModelType> modelTypes) {
        // Create a new set that we will return as a result, and add all original ModelTypes.
        Set<ModelType> typesWithGroups = new HashSet<ModelType>();
        Set<ModelType> modelTypesNonNull =
                modelTypes == null ? new HashSet<ModelType>() : modelTypes;
        typesWithGroups.addAll(modelTypesNonNull);

        Multimap<ModelType, ModelType> modelTypeGroups = ModelType.modelTypeGroups();
        // Remove ModelTypes that are specified, that does not have their group ModelType specified.
        for (ModelType modelType : modelTypeGroups.keySet()) {
            if (modelTypesNonNull.contains(modelType)) {
                typesWithGroups.addAll(modelTypeGroups.get(modelType));
            } else {
                typesWithGroups.removeAll(modelTypeGroups.get(modelType));
            }
        }

        // Add all control types.
        typesWithGroups.addAll(ModelType.controlTypes());
        return typesWithGroups;
    }
}
