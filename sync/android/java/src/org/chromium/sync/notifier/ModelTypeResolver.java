// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.Set;

/**
 * A utility class that supports groups of {@link ModelType}s and also supports adding the default
 * set of {@link ModelType}s.
 */
interface ModelTypeResolver {
    Set<ModelType> resolveModelTypes(Set<ModelType> modelTypes);
}
