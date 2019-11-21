// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;

import java.util.ArrayList;
import java.util.List;

/**
 * Builds a {@link FeatureChange} representing the addition or removal of given {@link ModelChild}
 * instances.
 */
public class FeatureChangeBuilder {
    private final List<ModelChild> removedChildren = new ArrayList<>();
    private final List<ModelChild> appendedChildren = new ArrayList<>();

    public FeatureChangeBuilder addChildForRemoval(ModelChild modelChild) {
        removedChildren.add(modelChild);
        return this;
    }

    public FeatureChangeBuilder addChildForAppending(ModelChild modelChild) {
        appendedChildren.add(modelChild);
        return this;
    }

    public FeatureChange build() {
        return new FeatureChange() {
            @Override
            public String getContentId() {
                return "";
            }

            @Override
            public boolean isFeatureChanged() {
                return false;
            }

            @Override
            public ModelFeature getModelFeature() {
                return FakeModelFeature.newBuilder().build();
            }

            @Override
            public ChildChanges getChildChanges() {
                return new ChildChanges() {
                    @Override
                    public List<ModelChild> getAppendedChildren() {
                        return appendedChildren;
                    }

                    @Override
                    public List<ModelChild> getRemovedChildren() {
                        return removedChildren;
                    }
                };
            }
        };
    }
}
