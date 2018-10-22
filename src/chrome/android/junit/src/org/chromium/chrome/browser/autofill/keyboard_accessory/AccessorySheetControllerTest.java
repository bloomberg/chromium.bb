// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.v4.view.ViewPager;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.asynctask.CustomShadowAsyncTask;
import org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetModel.PropertyKey;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Tab;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable;

/**
 * Controller tests for the keyboard accessory bottom sheet component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
public class AccessorySheetControllerTest {
    @Mock
    private PropertyObservable.PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock
    private ListObservable.ListObserver<Void> mTabListObserver;
    @Mock
    private ViewStub mMockViewStub;
    @Mock
    private ViewPager mMockView;

    private final Tab[] mTabs =
            new Tab[] {new Tab(null, null, 0, 0, null), new Tab(null, null, 0, 0, null),
                    new Tab(null, null, 0, 0, null), new Tab(null, null, 0, 0, null)};

    private AccessorySheetCoordinator mCoordinator;
    private AccessorySheetMediator mMediator;
    private AccessorySheetModel mModel;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        when(mMockViewStub.inflate()).thenReturn(mMockView);
        mCoordinator = new AccessorySheetCoordinator(mMockViewStub, /*unused*/ () -> null);
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    @Test
    public void testCreatesValidSubComponents() {
        assertThat(mCoordinator, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mModel, is(notNullValue()));
    }

    @Test
    public void testModelNotifiesAboutVisibilityOncePerChange() {
        mModel.addObserver(mMockPropertyObserver);

        // Calling show on the mediator should make model propagate that it's visible.
        mMediator.show();
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(true));

        // Calling show again does nothing.
        mMediator.show();
        verify(mMockPropertyObserver) // Still the same call and no new one added.
                .onPropertyChanged(mModel, PropertyKey.VISIBLE);

        // Calling hide on the mediator should make model propagate that it's invisible.
        mMediator.hide();
        verify(mMockPropertyObserver, times(2)).onPropertyChanged(mModel, PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(false));
    }

    @Test
    public void testModelNotifiesHeightChanges() {
        mModel.addObserver(mMockPropertyObserver);

        // Setting height triggers the observer and changes the model.
        mCoordinator.setHeight(123);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.HEIGHT);
        assertThat(mModel.getHeight(), is(123));

        // Setting the same height doesn't trigger anything.
        mCoordinator.setHeight(123);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.HEIGHT); // No 2nd call.

        // Setting a different height triggers again.
        mCoordinator.setHeight(234);
        verify(mMockPropertyObserver, times(2)).onPropertyChanged(mModel, PropertyKey.HEIGHT);
        assertThat(mModel.getHeight(), is(234));
    }

    @Test
    public void testModelNotifiesChangesForNewSheet() {
        mModel.addObserver(mMockPropertyObserver);
        mModel.getTabList().addObserver(mTabListObserver);

        assertThat(mModel.getTabList().size(), is(0));
        mCoordinator.addTab(mTabs[0]);
        verify(mTabListObserver).onItemRangeInserted(mModel.getTabList(), 0, 1);
        assertThat(mModel.getTabList().size(), is(1));
    }

    @Test
    public void testFirstAddedTabBecomesActiveTab() {
        mModel.addObserver(mMockPropertyObserver);

        // Initially, there is no active Tab.
        assertThat(mModel.getTabList().size(), is(0));
        assertThat(mCoordinator.getTab(), is(nullValue()));

        // The first tab becomes the active Tab.
        mCoordinator.addTab(mTabs[0]);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(1));
        assertThat(mModel.getActiveTabIndex(), is(0));
        assertThat(mCoordinator.getTab(), is(mTabs[0]));

        // A second tab is added but doesn't become automatically active.
        mCoordinator.addTab(mTabs[1]);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(2));
        assertThat(mModel.getActiveTabIndex(), is(0));
    }

    @Test
    public void testDeletingFirstTabActivatesNewFirstTab() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        assertThat(mModel.getTabList().size(), is(4));
        assertThat(mModel.getActiveTabIndex(), is(0));

        mCoordinator.removeTab(mTabs[0]);

        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(0));
    }

    @Test
    public void testDeletingFirstAndOnlyTabInvalidatesActiveTab() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.removeTab(mTabs[0]);

        assertThat(mModel.getTabList().size(), is(0));
        assertThat(mModel.getActiveTabIndex(), is(AccessorySheetModel.NO_ACTIVE_TAB));
    }

    @Test
    public void testDeletedActiveTabDisappearsAndActivatesLeftNeighbor() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        mModel.setActiveTabIndex(2);
        mModel.addObserver(mMockPropertyObserver);

        mCoordinator.removeTab(mTabs[2]);

        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(1));
    }

    @Test
    public void testCorrectsPositionOfActiveTabForDeletedPredecessors() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        mModel.setActiveTabIndex(2);
        mModel.addObserver(mMockPropertyObserver);

        mCoordinator.removeTab(mTabs[1]);

        verify(mMockPropertyObserver).onPropertyChanged(mModel, PropertyKey.ACTIVE_TAB_INDEX);
        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(1));
    }

    @Test
    public void testDoesntChangePositionOfActiveTabForDeletedSuccessors() {
        mCoordinator.addTab(mTabs[0]);
        mCoordinator.addTab(mTabs[1]);
        mCoordinator.addTab(mTabs[2]);
        mCoordinator.addTab(mTabs[3]);
        mModel.setActiveTabIndex(2);

        mCoordinator.removeTab(mTabs[3]);

        assertThat(mModel.getTabList().size(), is(3));
        assertThat(mModel.getActiveTabIndex(), is(2));
    }

    @Test
    public void testRecordsSheetClosure() {
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED),
                is(0));

        // Although sheets must be opened manually as of now, don't assume that every opened sheet
        // in the future will be manually opened. Closing is the only thing to be tested here.
        mCoordinator.show();
        mCoordinator.hide();
        assertThat(getTriggerMetricsCount(AccessorySheetTrigger.ANY_CLOSE), is(1));

        // Log closing every time it happens.
        mCoordinator.show();
        mCoordinator.hide();
        assertThat(getTriggerMetricsCount(AccessorySheetTrigger.ANY_CLOSE), is(2));
    }

    private int getTriggerMetricsCount(@AccessorySheetTrigger int bucket) {
        return RecordHistogram.getHistogramValueCountForTesting(
                KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED, bucket);
    }
}