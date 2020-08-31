// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.Mockito.doReturn;

import android.app.Activity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.ui.base.WindowAndroid;

/**
 * Unit tests for {@link TabModelSelectorImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelSelectorImplTest {
    @Mock
    TabPersistencePolicy mMockTabPersistencePolicy;
    @Mock
    TabContentManager mMockTabContentManager;
    @Mock
    TabDelegateFactory mTabDelegateFactory;

    TabModelSelectorImpl mTabModelSelector;
    MockTabCreatorManager mTabCreatorManager;
    Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        MockitoAnnotations.initMocks(this);

        doReturn(TabPersistentStore.SAVED_STATE_FILE_PREFIX)
                .when(mMockTabPersistencePolicy)
                .getStateFileName();

        mTabCreatorManager = new MockTabCreatorManager();
        mTabModelSelector = new TabModelSelectorImpl(mActivity, mTabCreatorManager,
                mMockTabPersistencePolicy,
                /*supportUndo=*/false, /*isTabbedActivity=*/false, /*startIncognito=*/false);
        mTabCreatorManager.initialize(mTabModelSelector);
        mTabModelSelector.onNativeLibraryReadyInternal(mMockTabContentManager,
                new MockTabModel(false, null), new MockTabModel(true, null));
    }

    @Test
    public void testTabActivityAttachmentChanged_detaching() {
        MockTab tab = new MockTab(1, false);
        mTabModelSelector.getModel(false).addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tab.updateAttachment(null, null);

        Assert.assertEquals("detaching a tab should result in it being removed from the model", 0,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_movingWindows() {
        MockTab tab = new MockTab(1, false);
        mTabModelSelector.getModel(false).addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tab.updateAttachment(Mockito.mock(WindowAndroid.class), mTabDelegateFactory);

        Assert.assertEquals("moving a tab between windows shouldn't remove it from the model", 1,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_detachingWhileReparentingInProgress() {
        MockTab tab = new MockTab(1, false);
        mTabModelSelector.getModel(false).addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mTabModelSelector.enterReparentingMode();
        tab.updateAttachment(null, null);

        Assert.assertEquals("tab shouldn't be removed while reparenting is in progress", 1,
                mTabModelSelector.getModel(false).getCount());
    }
}