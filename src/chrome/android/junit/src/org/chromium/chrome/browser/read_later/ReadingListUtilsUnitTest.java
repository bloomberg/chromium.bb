// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Unit test for {@link ReadingListUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadingListUtilsUnitTest {
    @Mock
    BookmarkBridge mBookmarkModel;

    @Mock
    BookmarkId mBookmarkId;
    @Mock
    BookmarkItem mBookmarkItem;
    @Mock
    BookmarkId mReadingListId;
    @Mock
    BookmarkItem mReadingListItem;
    @Mock
    BookmarkId mReadingListFolder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mReadingListId).when(mReadingListItem).getId();
        doReturn(mReadingListItem).when(mBookmarkModel).getReadingListItem(any());
        doAnswer((invocation) -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(mBookmarkModel)
                .finishLoadingBookmarkModel(any());
        doReturn(BookmarkType.NORMAL).when(mBookmarkId).getType();
        doReturn(BookmarkType.READING_LIST).when(mReadingListFolder).getType();
        doReturn(mReadingListFolder).when(mBookmarkModel).getReadingListFolder();
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);
        doReturn(mReadingListId).when(mBookmarkModel).addToReadingList(any(), any());
    }

    @Test
    @SmallTest
    public void isReadingListSupported() {
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(null));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(GURL.emptyGURL()));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.HTTP_URL)));

        // empty url
        GURL testUrl = GURL.emptyGURL();
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(testUrl));

        // invalid url
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.INVALID_URL)));
    }

    @Test
    @SmallTest
    public void deleteFromReadingList() {
        BookmarkModel bookmarkModel = Mockito.mock(BookmarkModel.class);
        BookmarkId readingListId = Mockito.mock(BookmarkId.class);
        BookmarkItem readingListItem = Mockito.mock(BookmarkItem.class);
        doReturn(readingListId).when(readingListItem).getId();
        doReturn(readingListItem).when(bookmarkModel).getReadingListItem(any());
        doAnswer((invocation) -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(bookmarkModel)
                .finishLoadingBookmarkModel(any());

        ReadingListUtils.deleteFromReadingList(bookmarkModel, Mockito.mock(SnackbarManager.class),
                Mockito.mock(Activity.class), Mockito.mock(Tab.class));
        verify(bookmarkModel).getReadingListItem(any());
        verify(bookmarkModel).deleteBookmarks(readingListId);
    }

    @Test
    @SmallTest
    public void isSwappableReadingListItem() {
        BookmarkId readingListId = new BookmarkId(1, BookmarkType.READING_LIST);
        BookmarkId regularId = new BookmarkId(1, BookmarkType.NORMAL);

        // feature disabled
        setBookmarkTypeSwappingEnabled(false);
        Assert.assertFalse(ReadingListUtils.isSwappableReadingListItem(readingListId));

        // wrong type
        setBookmarkTypeSwappingEnabled(true);
        Assert.assertFalse(ReadingListUtils.isSwappableReadingListItem(regularId));

        setBookmarkTypeSwappingEnabled(true);
        Assert.assertTrue(ReadingListUtils.isSwappableReadingListItem(readingListId));
    }

    @Test
    @SmallTest
    public void maybeTypeSwapAndShowSaveFlow() {
        setBookmarkTypeSwappingEnabled(true);
        ReadingListUtils.setSkipShowSaveFlowForTesting(true);

        Assert.assertTrue(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                mBookmarkModel, mBookmarkId, BookmarkType.READING_LIST));
        verify(mBookmarkModel).addToReadingList(any(), any());
    }

    @Test
    @SmallTest
    public void maybeTypeSwapAndShowSaveFlow_EdgeCases() {
        BookmarkId bookmarkId = Mockito.mock(BookmarkId.class);
        doReturn(BookmarkType.NORMAL).when(bookmarkId).getType();

        // feature disabled
        setBookmarkTypeSwappingEnabled(false);
        Assert.assertFalse(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                Mockito.mock(BookmarkModel.class), bookmarkId, BookmarkType.READING_LIST));

        // bookmarkId null
        setBookmarkTypeSwappingEnabled(true);
        Assert.assertFalse(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                Mockito.mock(BookmarkModel.class), /*bookmarkId=*/null, BookmarkType.READING_LIST));

        // bad original type
        setBookmarkTypeSwappingEnabled(true);
        doReturn(BookmarkType.READING_LIST).when(bookmarkId).getType();
        Assert.assertFalse(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                Mockito.mock(BookmarkModel.class), bookmarkId, BookmarkType.READING_LIST));

        // bad desired type
        setBookmarkTypeSwappingEnabled(true);
        doReturn(BookmarkType.NORMAL).when(bookmarkId).getType();
        Assert.assertFalse(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                Mockito.mock(BookmarkModel.class), bookmarkId, BookmarkType.NORMAL));
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToReadingList() {
        setBookmarkTypeSwappingEnabled(true);

        BookmarkId parentId = new BookmarkId(0, BookmarkType.READING_LIST);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);
        doReturn(existingBookmark).when(bookmarkBridge).getBookmarkById(existingBookmarkId);
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.READING_LIST);
        doReturn(newBookmarkId)
                .when(bookmarkBridge)
                .addToReadingList("Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkBridge, bookmarks, typeSwappedBookmarks, parentId);
        verify(bookmarkBridge)
                .addToReadingList("Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId);
        Assert.assertEquals(0, bookmarks.size());
        Assert.assertEquals(1, typeSwappedBookmarks.size());
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToBookmark() {
        setBookmarkTypeSwappingEnabled(true);

        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);
        doReturn(existingBookmark).when(bookmarkBridge).getBookmarkById(existingBookmarkId);
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        doReturn(newBookmarkId)
                .when(bookmarkBridge)
                .addBookmark(parentId, 0, "Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkBridge, bookmarks, typeSwappedBookmarks, parentId);
        verify(bookmarkBridge)
                .addBookmark(parentId, 0, "Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId);
        Assert.assertEquals(0, bookmarks.size());
        Assert.assertEquals(1, typeSwappedBookmarks.size());
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToBookmark_Multiple() {
        setBookmarkTypeSwappingEnabled(true);

        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId1 = new BookmarkId(1, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark1 = new BookmarkItem(existingBookmarkId1, "Test1",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkId existingBookmarkId2 = new BookmarkId(2, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark2 = new BookmarkItem(existingBookmarkId2, "Test2",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);
        doReturn(existingBookmark1).when(bookmarkBridge).getBookmarkById(existingBookmarkId1);
        doReturn(existingBookmark2).when(bookmarkBridge).getBookmarkById(existingBookmarkId2);

        BookmarkId newBookmarkId1 = new BookmarkId(3, BookmarkType.NORMAL);
        doReturn(newBookmarkId1)
                .when(bookmarkBridge)
                .addBookmark(parentId, 0, "Test1", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        BookmarkId newBookmarkId2 = new BookmarkId(4, BookmarkType.NORMAL);
        doReturn(newBookmarkId2)
                .when(bookmarkBridge)
                .addBookmark(parentId, 0, "Test2", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId1);
        bookmarks.add(existingBookmarkId2);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkBridge, bookmarks, typeSwappedBookmarks, parentId);
        Assert.assertEquals(0, bookmarks.size());
        Assert.assertEquals(2, typeSwappedBookmarks.size());

        verify(bookmarkBridge)
                .addBookmark(parentId, 0, "Test1", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId1);
        verify(bookmarkBridge)
                .addBookmark(parentId, 0, "Test2", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId2);
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_TypeMatches() {
        setBookmarkTypeSwappingEnabled(true);

        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkBridge, bookmarks, typeSwappedBookmarks, parentId);
        Assert.assertEquals(1, bookmarks.size());
        Assert.assertEquals(0, typeSwappedBookmarks.size());
        Assert.assertEquals(existingBookmarkId, bookmarks.get(0));
    }

    private void setBookmarkTypeSwappingEnabled(boolean enabled) {
        TestValues readLaterTypeSwapping = new TestValues();
        readLaterTypeSwapping.addFeatureFlagOverride(ChromeFeatureList.READ_LATER, true);
        readLaterTypeSwapping.addFieldTrialParamOverride(ChromeFeatureList.READ_LATER,
                "allow_bookmark_type_swapping", enabled ? "true" : "false");
        FeatureList.setTestValues(readLaterTypeSwapping);
    }
}
