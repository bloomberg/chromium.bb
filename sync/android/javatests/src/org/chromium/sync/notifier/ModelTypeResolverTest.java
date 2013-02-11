// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.HashSet;
import java.util.Set;

public class ModelTypeResolverTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Sync"})
    public void testControlTypesShouldAlwaysBeAddedEvenForNullModelTypes() throws Exception {
        ModelTypeResolverImpl resolver = new ModelTypeResolverImpl();
        Set<ModelType> result = resolver.resolveModelTypes(null);
        assertNotNull(result);
        assertEquals("Size should be the same as number of control types",
                ModelType.controlTypes().size(), result.size());
        assertTrue("Should contain all control ModelTypes",
                result.containsAll(ModelType.controlTypes()));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testControlTypesShouldAlwaysBeAdded() throws Exception {
        ModelTypeResolverImpl resolver = new ModelTypeResolverImpl();
        Set<ModelType> result = resolver.resolveModelTypes(new HashSet<ModelType>());
        assertNotNull(result);
        assertEquals("Size should be the same as number of control types",
                ModelType.controlTypes().size(), result.size());
        assertTrue("Should contain all control ModelTypes",
                result.containsAll(ModelType.controlTypes()));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testAddingAutofillShouldAddAutofillProfile() throws Exception {
        Set<ModelType> modelTypes = new HashSet<ModelType>();
        modelTypes.add(ModelType.AUTOFILL);
        ModelTypeResolverImpl resolver = new ModelTypeResolverImpl();
        Set<ModelType> result = resolver.resolveModelTypes(modelTypes);
        assertNotNull(result);
        assertEquals("Size should be 2 plus the number of control types",
                2 + ModelType.controlTypes().size(), result.size());
        assertTrue("Should have AUTOFILL ModelType", result.contains(ModelType.AUTOFILL));
        assertTrue("Should have AUTOFILL_PROFILE ModelType",
                result.contains(ModelType.AUTOFILL_PROFILE));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testModelTypesThatArePartOfGroupsShouldStillWork() throws Exception {
        Set<ModelType> modelTypes = new HashSet<ModelType>();
        modelTypes.add(ModelType.BOOKMARK);
        modelTypes.add(ModelType.SESSION);
        modelTypes.add(ModelType.TYPED_URL);
        ModelTypeResolverImpl resolver = new ModelTypeResolverImpl();
        Set<ModelType> result = resolver.resolveModelTypes(modelTypes);
        assertNotNull(result);
        assertEquals("Size should be " + modelTypes.size() + " plus the number of control types",
                modelTypes.size() + ModelType.controlTypes().size(), result.size());
        assertTrue("Should have BOOKMARK ModelType", result.contains(ModelType.BOOKMARK));
        assertTrue("Should have SESSION ModelType", result.contains(ModelType.SESSION));
        assertTrue("Should have TYPED_URL ModelType", result.contains(ModelType.TYPED_URL));
    }
}
