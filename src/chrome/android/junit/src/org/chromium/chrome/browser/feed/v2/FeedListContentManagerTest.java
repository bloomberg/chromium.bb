// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.support.test.filters.SmallTest;
import android.view.View;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.ListContentManagerObserver;

import java.util.List;

/** Unit tests for {@link FeedListContentManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedListContentManagerTest implements ListContentManagerObserver {
    private FeedListContentManager mManager;
    private Context mContext;

    private boolean mItemRangeInserted;
    private int mItemRangeInsertedStartIndex;
    private int mItemRangeInsertedCount;
    private boolean mItemRangeRemoved;
    private int mItemRangeRemovedStartIndex;
    private int mItemRangeRemovedCount;
    private boolean mItemRangeChanged;
    private int mItemRangeChangedStartIndex;
    private int mItemRangeChangedCount;
    private boolean mItemMoved;
    private int mItemMovedCurIndex;
    private int mItemMovedNewIndex;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mManager = new FeedListContentManager(null, null);
        mManager.addObserver(this);
    }

    @Test
    @SmallTest
    public void testFindContentPositionByKey() {
        FeedListContentManager.FeedContent c1 = createExternalViewContent("a");
        FeedListContentManager.FeedContent c2 = createExternalViewContent("b");
        FeedListContentManager.FeedContent c3 = createExternalViewContent("c");

        addContents(0, ImmutableList.of(c1, c2, c3));
        assertEquals(3, mManager.getItemCount());

        assertEquals(0, mManager.findContentPositionByKey("a"));
        assertEquals(1, mManager.findContentPositionByKey("b"));
        assertEquals(2, mManager.findContentPositionByKey("c"));
    }

    @Test
    @SmallTest
    public void testAddContents() {
        FeedListContentManager.FeedContent c1 = createExternalViewContent("a");
        FeedListContentManager.FeedContent c2 = createExternalViewContent("b");
        FeedListContentManager.FeedContent c3 = createExternalViewContent("c");

        addContents(0, ImmutableList.of(c1));
        assertEquals(1, mManager.getItemCount());
        assertEquals(c1, mManager.getContent(0));

        addContents(0, ImmutableList.of(c2, c3));
        assertEquals(3, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c1, mManager.getContent(2));

        addContents(3, ImmutableList.of(c2, c3));
        assertEquals(5, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c1, mManager.getContent(2));
        assertEquals(c2, mManager.getContent(3));
        assertEquals(c3, mManager.getContent(4));
    }

    @Test
    @SmallTest
    public void testRemoveContents() {
        FeedListContentManager.FeedContent c1 = createExternalViewContent("a");
        FeedListContentManager.FeedContent c2 = createExternalViewContent("b");
        FeedListContentManager.FeedContent c3 = createExternalViewContent("c");
        FeedListContentManager.FeedContent c4 = createExternalViewContent("d");
        FeedListContentManager.FeedContent c5 = createExternalViewContent("e");

        addContents(0, ImmutableList.of(c1, c2, c3, c4, c5));
        assertEquals(5, mManager.getItemCount());

        removeContents(0, 2);
        assertEquals(3, mManager.getItemCount());
        assertEquals(c3, mManager.getContent(0));
        assertEquals(c4, mManager.getContent(1));
        assertEquals(c5, mManager.getContent(2));

        removeContents(1, 2);
        assertEquals(1, mManager.getItemCount());
        assertEquals(c3, mManager.getContent(0));

        removeContents(0, 1);
        assertEquals(0, mManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testUpdateContents() {
        FeedListContentManager.FeedContent c1 = createExternalViewContent("a");
        FeedListContentManager.FeedContent c2 = createExternalViewContent("b");
        FeedListContentManager.FeedContent c3 = createExternalViewContent("c");
        FeedListContentManager.FeedContent c4 = createExternalViewContent("d");
        FeedListContentManager.FeedContent c5 = createExternalViewContent("e");

        addContents(0, ImmutableList.of(c1, c2, c3));
        assertEquals(3, mManager.getItemCount());

        updateContents(1, ImmutableList.of(c4, c5));
        assertEquals(3, mManager.getItemCount());
        assertEquals(c1, mManager.getContent(0));
        assertEquals(c4, mManager.getContent(1));
        assertEquals(c5, mManager.getContent(2));
    }

    @Test
    @SmallTest
    public void testMoveContent() {
        FeedListContentManager.FeedContent c1 = createExternalViewContent("a");
        FeedListContentManager.FeedContent c2 = createExternalViewContent("b");
        FeedListContentManager.FeedContent c3 = createExternalViewContent("c");
        FeedListContentManager.FeedContent c4 = createExternalViewContent("d");
        FeedListContentManager.FeedContent c5 = createExternalViewContent("e");

        addContents(0, ImmutableList.of(c1, c2, c3, c4, c5));
        assertEquals(5, mManager.getItemCount());

        moveContent(0, 3);
        assertEquals(5, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c4, mManager.getContent(2));
        assertEquals(c1, mManager.getContent(3));
        assertEquals(c5, mManager.getContent(4));

        moveContent(4, 2);
        assertEquals(5, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c5, mManager.getContent(2));
        assertEquals(c4, mManager.getContent(3));
        assertEquals(c1, mManager.getContent(4));
    }

    @Test
    @SmallTest
    public void testGetViewData() {
        FeedListContentManager.FeedContent c1 = createExternalViewContent("foo");
        View v2 = new View(mContext);
        FeedListContentManager.FeedContent c2 = createNativeViewContent(v2);
        View v3 = new View(mContext);
        FeedListContentManager.FeedContent c3 = createNativeViewContent(v3);
        FeedListContentManager.FeedContent c4 = createExternalViewContent("hello");
        View v5 = new View(mContext);
        FeedListContentManager.FeedContent c5 = createNativeViewContent(v5);

        addContents(0, ImmutableList.of(c1, c2, c3, c4, c5));
        assertEquals(5, mManager.getItemCount());

        assertFalse(mManager.isNativeView(0));
        assertTrue(mManager.isNativeView(1));
        assertTrue(mManager.isNativeView(2));
        assertFalse(mManager.isNativeView(3));
        assertTrue(mManager.isNativeView(4));

        assertArrayEquals("foo".getBytes(), mManager.getExternalViewBytes(0));
        assertEquals(v2, mManager.getNativeView(1, null));
        assertEquals(v3, mManager.getNativeView(2, null));
        assertArrayEquals("hello".getBytes(), mManager.getExternalViewBytes(3));
        assertEquals(v5, mManager.getNativeView(4, null));
    }

    @Override
    public void onItemRangeInserted(int startIndex, int count) {
        mItemRangeInserted = true;
        mItemRangeInsertedStartIndex = startIndex;
        mItemRangeInsertedCount = count;
    }

    @Override
    public void onItemRangeRemoved(int startIndex, int count) {
        mItemRangeRemoved = true;
        mItemRangeRemovedStartIndex = startIndex;
        mItemRangeRemovedCount = count;
    }

    @Override
    public void onItemRangeChanged(int startIndex, int count) {
        mItemRangeChanged = true;
        mItemRangeChangedStartIndex = startIndex;
        mItemRangeChangedCount = count;
    }

    @Override
    public void onItemMoved(int curIndex, int newIndex) {
        mItemMoved = true;
        mItemMovedCurIndex = curIndex;
        mItemMovedNewIndex = newIndex;
    }

    private void addContents(int index, List<FeedListContentManager.FeedContent> contents) {
        mItemRangeInserted = false;
        mItemRangeInsertedStartIndex = -1;
        mItemRangeInsertedCount = -1;
        mManager.addContents(index, contents);
        assertTrue(mItemRangeInserted);
        assertEquals(index, mItemRangeInsertedStartIndex);
        assertEquals(contents.size(), mItemRangeInsertedCount);
    }

    private void removeContents(int index, int count) {
        mItemRangeRemoved = false;
        mItemRangeRemovedStartIndex = -1;
        mItemRangeRemovedCount = -1;
        mManager.removeContents(index, count);
        assertTrue(mItemRangeRemoved);
        assertEquals(index, mItemRangeRemovedStartIndex);
        assertEquals(count, mItemRangeRemovedCount);
    }

    private void updateContents(int index, List<FeedListContentManager.FeedContent> contents) {
        mItemRangeChanged = false;
        mItemRangeChangedStartIndex = -1;
        mItemRangeChangedCount = -1;
        mManager.updateContents(index, contents);
        assertTrue(mItemRangeChanged);
        assertEquals(index, mItemRangeChangedStartIndex);
        assertEquals(contents.size(), mItemRangeChangedCount);
    }

    private void moveContent(int curIndex, int newIndex) {
        mItemMoved = false;
        mItemMovedCurIndex = -1;
        mItemMovedNewIndex = -1;
        mManager.moveContent(curIndex, newIndex);
        assertTrue(mItemMoved);
        assertEquals(curIndex, mItemMovedCurIndex);
        assertEquals(newIndex, mItemMovedNewIndex);
    }

    private FeedListContentManager.FeedContent createExternalViewContent(String s) {
        return new FeedListContentManager.ExternalViewContent(s, s.getBytes());
    }

    private FeedListContentManager.FeedContent createNativeViewContent(View v) {
        return new FeedListContentManager.NativeViewContent(v.toString(), v);
    }
}
