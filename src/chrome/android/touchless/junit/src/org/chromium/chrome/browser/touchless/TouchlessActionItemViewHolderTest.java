// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.cards.SuggestionsSection;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils;

/**
 * Unit tests for the TouchlessActionItemViewHolder class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TouchlessActionItemViewHolderTest {
    @Mock
    private ContextMenuManager mContextMenuManager;

    @Mock
    private SuggestionsUiDelegate mUiDelegate;

    @Mock
    private UiConfig mUiConfig;

    @Mock
    private SuggestionsSection mSection;

    @Mock
    private SuggestionsRanker mRanker;

    @Mock
    private SuggestionsEventReporter mSuggestionsEventReporter;

    @Captor
    private ArgumentCaptor<Runnable> mCallbackCaptor;

    private TouchlessActionItemViewHolder mViewHolder;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);

        SuggestionsRecyclerView recyclerView =
                new SuggestionsRecyclerView(RuntimeEnvironment.application);
        mViewHolder = new TouchlessActionItemViewHolder(
                recyclerView, mContextMenuManager, mUiDelegate, mUiConfig);
        SuggestionsCategoryInfo info =
                new ContentSuggestionsTestUtils.CategoryInfoBuilder(KnownCategories.ARTICLES)
                        .withAction(ContentSuggestionsAdditionalAction.FETCH)
                        .showIfEmpty()
                        .build();
        Mockito.when(mSection.getCategoryInfo()).thenReturn(info);
        Mockito.when(mUiDelegate.getEventReporter()).thenReturn(mSuggestionsEventReporter);
    }

    @Test
    @SmallTest
    public void testOnFailureToast() {
        mViewHolder.onBindViewHolder(new ActionItem(mSection, mRanker));
        mViewHolder.getButtonForTesting().performClick();
        Assert.assertEquals(0, ShadowToast.shownToastCount());
        Mockito.verify(mSection).fetchSuggestions(/*onFailure*/
                mCallbackCaptor.capture(), /*onNoNewSuggestions*/ Mockito.any(Runnable.class));
        mCallbackCaptor.getValue().run();

        Mockito.verify(mUiDelegate, Mockito.never()).getSnackbarManager();
        Assert.assertEquals(1, ShadowToast.shownToastCount());
    }

    @Test
    @SmallTest
    public void testOnNoNewSuggestionsToast() {
        mViewHolder.onBindViewHolder(new ActionItem(mSection, mRanker));
        mViewHolder.getButtonForTesting().performClick();
        Assert.assertEquals(0, ShadowToast.shownToastCount());
        Mockito.verify(mSection).fetchSuggestions(/*onFailure*/
                Mockito.any(Runnable.class), /*onNoNewSuggestions*/ mCallbackCaptor.capture());
        mCallbackCaptor.getValue().run();

        Mockito.verify(mUiDelegate, Mockito.never()).getSnackbarManager();
        Assert.assertEquals(1, ShadowToast.shownToastCount());
    }
}
