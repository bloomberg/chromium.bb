// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link FeatureChangeImpl} class. */
@RunWith(RobolectricTestRunner.class)
public class FeatureChangeImplTest {
    @Mock
    private ModelFeature modelFeature;

    private String modelContentId;
    private ContentIdGenerators idGenerators = new ContentIdGenerators();

    @Before
    public void setup() {
        initMocks(this);
        modelContentId = idGenerators.createFeatureContentId(1);
        StreamFeature streamFeature =
                StreamFeature.newBuilder().setContentId(modelContentId).build();
        when(modelFeature.getStreamFeature()).thenReturn(streamFeature);
    }

    @Test
    public void testStreamFeatureChange() {
        FeatureChangeImpl featureChange = new FeatureChangeImpl(modelFeature);

        assertThat(featureChange.getModelFeature()).isEqualTo(modelFeature);
        assertThat(featureChange.getContentId()).isEqualTo(modelContentId);
        assertThat(featureChange.getChildChanges().getAppendedChildren()).isEmpty();
        assertThat(featureChange.getChildChanges().getRemovedChildren()).isEmpty();
        assertThat(featureChange.isFeatureChanged()).isFalse();

        featureChange.setFeatureChanged(true);
        assertThat(featureChange.isFeatureChanged()).isTrue();
    }

    @Test
    public void testAppendChild() {
        ModelChild modelChild = mock(ModelChild.class);
        FeatureChangeImpl featureChange = new FeatureChangeImpl(modelFeature);
        featureChange.getChildChangesImpl().addAppendChild(modelChild);
        assertThat(featureChange.getChildChanges().getAppendedChildren()).hasSize(1);
        assertThat(featureChange.getChildChanges().getAppendedChildren()).contains(modelChild);
    }

    @Test
    public void testRemoveChild() {
        ModelChild modelChild = mock(ModelChild.class);
        FeatureChangeImpl featureChange = new FeatureChangeImpl(modelFeature);
        featureChange.getChildChangesImpl().removeChild(modelChild);
        assertThat(featureChange.getChildChanges().getRemovedChildren()).hasSize(1);
        assertThat(featureChange.getChildChanges().getRemovedChildren()).contains(modelChild);
    }
}
