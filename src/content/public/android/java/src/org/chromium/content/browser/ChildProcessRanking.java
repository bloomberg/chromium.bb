// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.content_public.browser.ChildProcessImportance;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

/**
 * Ranking of ChildProcessConnections for a particular ChildConnectionAllocator.
 */
public class ChildProcessRanking implements Iterable<ChildProcessConnection> {
    private static class ConnectionWithRank {
        public final ChildProcessConnection connection;

        // Info for ranking a connection.
        public boolean visible;
        public long frameDepth;
        public boolean intersectsViewport;
        @ChildProcessImportance
        public int importance;

        public ConnectionWithRank(ChildProcessConnection connection, boolean visible,
                long frameDepth, boolean intersectsViewport,
                @ChildProcessImportance int importance) {
            this.connection = connection;
            this.visible = visible;
            this.frameDepth = frameDepth;
            this.intersectsViewport = intersectsViewport;
            this.importance = importance;
        }
    }

    private static class RankComparator implements Comparator<ConnectionWithRank> {
        private static int compareByIntersectsViewportAndDepth(
                ConnectionWithRank o1, ConnectionWithRank o2) {
            if (o1.intersectsViewport && !o2.intersectsViewport) {
                return -1;
            } else if (!o1.intersectsViewport && o2.intersectsViewport) {
                return 1;
            }

            return (int) (o1.frameDepth - o2.frameDepth);
        }

        @Override
        public int compare(ConnectionWithRank o1, ConnectionWithRank o2) {
            // Sort null to the end.
            if (o1 == null && o2 == null) {
                return 0;
            } else if (o1 == null && o2 != null) {
                return 1;
            } else if (o1 != null && o2 == null) {
                return -1;
            }

            assert o1 != null;
            assert o2 != null;

            // Ranking order:
            // * (visible and main frame) or ChildProcessImportance.IMPORTANT
            // * (visible and subframe and intersect viewport) or ChildProcessImportance.MODERATE
            // * invisible main frame
            // * visible subframe and not intersect viewport
            // * invisible subframes
            // Within each group, ties are broken by intersect viewport and then frame depth where
            // applicable. Note boostForPendingViews is not used for ranking.

            boolean o1IsVisibleMainOrImportant = (o1.visible && o1.frameDepth == 0)
                    || o1.importance == ChildProcessImportance.IMPORTANT;
            boolean o2IsVisibleMainOrImportant = (o2.visible && o2.frameDepth == 0)
                    || o2.importance == ChildProcessImportance.IMPORTANT;
            if (o1IsVisibleMainOrImportant && o2IsVisibleMainOrImportant) {
                return compareByIntersectsViewportAndDepth(o1, o2);
            } else if (o1IsVisibleMainOrImportant && !o2IsVisibleMainOrImportant) {
                return -1;
            } else if (!o1IsVisibleMainOrImportant && o2IsVisibleMainOrImportant) {
                return 1;
            }

            boolean o1VisibleIntersectSubframeOrModerate =
                    (o1.visible && o1.frameDepth > 0 && o1.intersectsViewport)
                    || o1.importance == ChildProcessImportance.MODERATE;
            boolean o2VisibleIntersectSubframeOrModerate =
                    (o2.visible && o2.frameDepth > 0 && o2.intersectsViewport)
                    || o2.importance == ChildProcessImportance.MODERATE;
            if (o1VisibleIntersectSubframeOrModerate && o2VisibleIntersectSubframeOrModerate) {
                return compareByIntersectsViewportAndDepth(o1, o2);
            } else if (o1VisibleIntersectSubframeOrModerate
                    && !o2VisibleIntersectSubframeOrModerate) {
                return -1;
            } else if (!o1VisibleIntersectSubframeOrModerate
                    && o2VisibleIntersectSubframeOrModerate) {
                return 1;
            }

            boolean o1InvisibleMainFrame = !o1.visible && o1.frameDepth == 0;
            boolean o2InvisibleMainFrame = !o2.visible && o2.frameDepth == 0;
            if (o1InvisibleMainFrame && o2InvisibleMainFrame) {
                return 0;
            } else if (o1InvisibleMainFrame && !o2InvisibleMainFrame) {
                return -1;
            } else if (!o1InvisibleMainFrame && o2InvisibleMainFrame) {
                return 1;
            }

            // The rest of the groups can just be ranked by visibility, intersects viewport, and
            // frame depth.
            if (o1.visible && !o2.visible) {
                return -1;
            } else if (!o1.visible && o2.visible) {
                return 1;
            }
            return compareByIntersectsViewportAndDepth(o1, o2);
        }
    }

    private class ReverseRankIterator implements Iterator<ChildProcessConnection> {
        private final int mSizeOnConstruction;
        private int mNextIndex;

        public ReverseRankIterator() {
            mSizeOnConstruction = ChildProcessRanking.this.mRankings.size();
            mNextIndex = mSizeOnConstruction - 1;
        }

        @Override
        public boolean hasNext() {
            modificationCheck();
            return mNextIndex >= 0;
        }

        @Override
        public ChildProcessConnection next() {
            modificationCheck();
            return ChildProcessRanking.this.mRankings.get(mNextIndex--).connection;
        }

        private void modificationCheck() {
            assert mSizeOnConstruction == ChildProcessRanking.this.mRankings.size();
        }
    }

    private static final RankComparator COMPARATOR = new RankComparator();

    // |mMaxSize| can be -1 to indicate there can be arbitrary number of connections.
    private final int mMaxSize;
    // ArrayList is not the most theoretically efficient data structure, but is good enough
    // for sizes in production and more memory efficient than linked data structures.
    private final List<ConnectionWithRank> mRankings = new ArrayList<>();

    public ChildProcessRanking() {
        mMaxSize = -1;
    }

    /**
     * Create with a maxSize. Trying to insert more will throw exceptions.
     */
    public ChildProcessRanking(int maxSize) {
        assert maxSize > 0;
        mMaxSize = maxSize;
    }

    /**
     * Iterate from lowest to highest rank. Ranking should not be modified during iteration,
     * including using Iterator.delete.
     */
    @Override
    public Iterator<ChildProcessConnection> iterator() {
        return new ReverseRankIterator();
    }

    public void addConnection(ChildProcessConnection connection, boolean visible, long frameDepth,
            boolean intersectsViewport, @ChildProcessImportance int importance) {
        assert connection != null;
        assert indexOf(connection) == -1;
        if (mMaxSize != -1 && mRankings.size() >= mMaxSize) {
            throw new RuntimeException(
                    "mRankings.size:" + mRankings.size() + " mMaxSize:" + mMaxSize);
        }
        mRankings.add(new ConnectionWithRank(
                connection, visible, frameDepth, intersectsViewport, importance));
        sort();
    }

    public void removeConnection(ChildProcessConnection connection) {
        assert connection != null;
        assert mRankings.size() > 0;
        int i = indexOf(connection);
        assert i != -1;

        // Null is sorted to the end.
        mRankings.remove(i);
    }

    public void updateConnection(ChildProcessConnection connection, boolean visible,
            long frameDepth, boolean intersectsViewport, @ChildProcessImportance int importance) {
        assert connection != null;
        assert mRankings.size() > 0;
        int i = indexOf(connection);
        assert i != -1;

        ConnectionWithRank rank = mRankings.get(i);
        rank.visible = visible;
        rank.frameDepth = frameDepth;
        rank.intersectsViewport = intersectsViewport;
        rank.importance = importance;
        sort();
    }

    public ChildProcessConnection getLowestRankedConnection() {
        if (mRankings.isEmpty()) return null;
        return mRankings.get(mRankings.size() - 1).connection;
    }

    private int indexOf(ChildProcessConnection connection) {
        for (int i = 0; i < mRankings.size(); ++i) {
            if (mRankings.get(i).connection == connection) return i;
        }
        return -1;
    }

    private void sort() {
        // Sort is stable.
        Collections.sort(mRankings, COMPARATOR);
    }
}
