// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.Collections;
import java.util.List;

/**
 * Helper class to manage all the logic and UI behind adding the reading list section headers in the
 * bookmark content UI.
 */
class ReadingListSectionHeader {
    /**
     * Sorts the reading list and adds section headers if the list is a reading list.
     * Noop, if the list isn't a reading list. The layout rows are shown in the following order :
     * 1 - Any promo header
     * 2 - Section header with title "Unread"
     * 3 - Unread reading list articles.
     * 4 - Section header with title "Read"
     * 5 - Read reading list articles.
     * @param listItems The list of bookmark items to be shown in the UI.
     * @param context The associated activity context.
     */
    public static void maybeSortAndInsertSectionHeaders(
            List<BookmarkListEntry> listItems, Context context) {
        if (listItems.isEmpty()) return;

        // The topmost item(s) could be promo headers.
        int readingListStartIndex = 0;
        for (BookmarkListEntry listItem : listItems) {
            boolean isReadingListItem = listItem.getBookmarkItem() != null
                    && listItem.getBookmarkItem().getId().getType() == BookmarkType.READING_LIST;
            if (isReadingListItem) break;
            readingListStartIndex++;
        }

        // If we have no read/unread elements, exit.
        if (readingListStartIndex == listItems.size()) return;
        sort(listItems, readingListStartIndex);
        recordMetrics(listItems);

        // Add a section header at the top. If it is for read, exit right away.
        assert listItems.get(readingListStartIndex).getBookmarkItem().getId().getType()
                == BookmarkType.READING_LIST;
        boolean isRead = listItems.get(readingListStartIndex).getBookmarkItem().isRead();
        listItems.add(readingListStartIndex, createReadingListSectionHeader(isRead, context));
        if (isRead) return;

        // Search for the first read element, and insert the read section header.
        for (int i = readingListStartIndex + 2; i < listItems.size(); i++) {
            BookmarkListEntry listItem = listItems.get(i);
            assert listItem.getBookmarkItem().getId().getType() == BookmarkType.READING_LIST;
            if (listItem.getBookmarkItem().isRead()) {
                listItems.add(i, createReadingListSectionHeader(true /*read*/, context));
                return;
            }
        }
    }

    /**
     * Sorts the given {@code listItems} to show unread items ahead of read items.
     */
    private static void sort(List<BookmarkListEntry> listItems, int readingListStartIndex) {
        // TODO(crbug.com/1147259): Sort items by creation time possibly.
        Collections.sort(listItems.subList(readingListStartIndex, listItems.size()), (lhs, rhs) -> {
            // Unread items are shown first.
            boolean lhsRead = lhs.getBookmarkItem().isRead();
            boolean rhsRead = rhs.getBookmarkItem().isRead();
            if (lhsRead == rhsRead) return 0;
            return lhsRead ? 1 : -1;
        });
    }

    private static BookmarkListEntry createReadingListSectionHeader(boolean read, Context context) {
        return BookmarkListEntry.createSectionHeader(
                read ? R.string.reading_list_read : R.string.reading_list_unread,
                read ? null : R.string.reading_list_ready_for_offline, context);
    }

    private static void recordMetrics(List<BookmarkListEntry> listItems) {
        int numUnreadItems = 0;
        int numReadItems = 0;
        for (int i = 0; i < listItems.size(); i++) {
            // Skip the headers.
            if (listItems.get(i).getBookmarkItem() == null) continue;
            if (listItems.get(i).getBookmarkItem().isRead()) {
                numReadItems++;
            } else {
                numUnreadItems++;
            }
        }
        RecordUserAction.record("Android.BookmarkPage.ReadingList.OpenReadingList");
        RecordHistogram.recordCountHistogram(
                "Bookmarks.ReadingList.NumberOfReadItems", numReadItems);
        RecordHistogram.recordCountHistogram(
                "Bookmarks.ReadingList.NumberOfUnreadItems", numUnreadItems);
        RecordHistogram.recordCountHistogram(
                "Bookmarks.ReadingList.NumberOfItems", numReadItems + numUnreadItems);
    }
}
