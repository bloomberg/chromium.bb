// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.common.functional.Predicate;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/**
 * Fake for tests using {@link ModelProviderFactory}. Functionality should be added to this class as
 * needed.
 */
public class FakeModelProviderFactory implements ModelProviderFactory {

  private FakeModelProvider modelProvider;
  private String createSessionId;
  /*@Nullable*/ private ViewDepthProvider viewDepthProvider;

  @Override
  public ModelProvider create(String sessionId, UiContext uiContext) {
    this.modelProvider = new FakeModelProvider();
    this.createSessionId = sessionId;
    return modelProvider;
  }

  @Override
  public ModelProvider createNew(
      /*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext) {
    this.viewDepthProvider = viewDepthProvider;
    this.modelProvider = new FakeModelProvider();
    return modelProvider;
  }

  @Override
  public ModelProvider createNew(
      /*@Nullable*/ ViewDepthProvider viewDepthProvider,
      /*@Nullable*/ Predicate<StreamStructure> filterPredicate,
      UiContext uiContext) {
    this.viewDepthProvider = viewDepthProvider;
    this.modelProvider = new FakeModelProvider();
    return modelProvider;
  }

  public FakeModelProvider getLatestModelProvider() {
    return modelProvider;
  }

  public String getLatestCreateSessionId() {
    return createSessionId;
  }

  /*@Nullable*/
  public ViewDepthProvider getViewDepthProvider() {
    return viewDepthProvider;
  }
}
