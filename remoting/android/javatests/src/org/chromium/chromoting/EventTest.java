// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chromoting.test.util.MutableReference;

/**
 * Tests for {@link Event}.
 */
public class EventTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Chromoting"})
    public static void testBasicScenario() {
        Event.Raisable<Void> event = new Event.Raisable<>();
        final MutableReference<Integer> callTimes = new MutableReference<Integer>(0);
        Object eventId1 = event.add(new Event.ParameterRunnable<Void>() {
                @Override
                public void run(Void nil) {
                    callTimes.set(callTimes.get() + 1);
                }
        });
        Object eventId2 = event.add(new Event.ParameterRunnable<Void>() {
                @Override
                public void run(Void nil) {
                    callTimes.set(callTimes.get() + 1);
                }
        });
        Object eventId3 = event.add(new Event.ParameterRunnable<Void>() {
                @Override
                public void run(Void nil) {
                    // Should not reach.
                    fail();
                    callTimes.set(callTimes.get() + 1);
                }
        });
        assertNotNull(eventId1);
        assertNotNull(eventId2);
        assertNotNull(eventId3);
        assertTrue(event.remove(eventId3));
        for (int i = 0; i < 100; i++) {
            assertEquals(event.raise(null), 2);
            assertEquals(callTimes.get().intValue(), (i + 1) << 1);
        }
        assertTrue(event.remove(eventId1));
        assertTrue(event.remove(eventId2));
        assertFalse(event.remove(eventId3));
    }

    private static class MultithreadingTestRunner extends Thread {
        private final Event.Raisable<Void> mEvent;
        private final MutableReference<Boolean> mError;

        public MultithreadingTestRunner(Event.Raisable<Void> event,
                                        MutableReference<Boolean> error) {
            Preconditions.notNull(event);
            Preconditions.notNull(error);
            mEvent = event;
            mError = error;
        }

        @Override
        public void run() {
            for (int i = 0; i < 100; i++) {
                final MutableReference<Boolean> called = new MutableReference<>();
                Object id = mEvent.add(new Event.ParameterRunnable<Void>() {
                        @Override
                        public void run(Void nil) {
                            called.set(true);
                        }
                });
                if (id == null) {
                    mError.set(true);
                }
                for (int j = 0; j < 100; j++) {
                    called.set(false);
                    if (mEvent.raise(null) <= 0) {
                        mError.set(true);
                    }
                    if (!called.get()) {
                        mError.set(true);
                    }
                }
                if (!mEvent.remove(id)) {
                    mError.set(true);
                }
            }
        }
    }

    @MediumTest
    @Feature({"Chromoting"})
    public static void testMultithreading() {
        Event.Raisable<Void> event = new Event.Raisable<>();
        final int threadCount = 10;
        final MutableReference<Boolean> error = new MutableReference<>();
        Thread[] threads = new Thread[threadCount];
        for (int i = 0; i < threadCount; i++) {
            threads[i] = new MultithreadingTestRunner(event, error);
            threads[i].start();
        }
        for (int i = 0; i < threadCount; i++) {
            try {
                threads[i].join();
            } catch (InterruptedException exception) {
                fail();
            }
        }
        assertNull(error.get());
    }

    @SmallTest
    @Feature({"Chromoting"})
    public static void testSelfRemovable() {
        Event.Raisable<Void> event = new Event.Raisable<>();
        final MutableReference<Boolean> called = new MutableReference<>();
        final MutableReference<Boolean> nextReturn = new MutableReference<>();
        nextReturn.set(true);
        event.addSelfRemovable(new Event.ParameterCallback<Boolean, Void>() {
            @Override
            public Boolean run(Void nil) {
                called.set(true);
                return nextReturn.get();
            }
        });
        assertEquals(event.raise(null), 1);
        assertTrue(called.get());
        assertFalse(event.isEmpty());
        called.set(false);
        nextReturn.set(false);
        assertEquals(event.raise(null), 1);
        assertTrue(called.get());
        assertTrue(event.isEmpty());
        called.set(false);
        nextReturn.set(true);
        assertEquals(event.raise(null), 0);
        assertFalse(called.get());
        assertTrue(event.isEmpty());
    }
}
