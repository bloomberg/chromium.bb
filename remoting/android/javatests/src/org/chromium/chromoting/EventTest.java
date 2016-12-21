// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.os.Handler;
import android.os.Looper;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.Feature;
import org.chromium.chromoting.test.util.MutableReference;

import java.util.ArrayList;
import java.util.List;

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

    @SmallTest
    @Feature({"Chromoting"})
    public static void testPromisedEvent() {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                assertNull(Looper.myLooper());
                Event.Raisable<Object> event = new Event.PromisedRaisable<>();
                final List<Object> called1 = new ArrayList<>();
                final List<Object> called2 = new ArrayList<>();
                final List<Object> called3 = new ArrayList<>();
                final List<Object> called4 = new ArrayList<>();
                final List<Object> parameters = new ArrayList<>();
                event.add(new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called1.add(obj);
                    }
                });
                Object parameter = new Object();
                event.raise(parameter);
                parameters.add(parameter);
                event.add(new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called2.add(obj);
                    }
                });
                parameter = new Object();
                event.raise(parameter);
                parameters.add(parameter);
                event.add(new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called3.add(obj);
                    }
                });
                parameter = new Object();
                event.raise(parameter);
                parameters.add(parameter);
                event.add(new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called4.add(obj);
                    }
                });

                assertEquals(called1.size(), 3);
                assertEquals(called2.size(), 3);
                assertEquals(called3.size(), 2);
                assertEquals(called4.size(), 1);

                for (int i = 0; i < 3; i++) {
                    assertTrue(called1.get(i) == parameters.get(i));
                    assertTrue(called2.get(i) == parameters.get(i));
                }
                for (int i = 0; i < 2; i++) {
                    assertTrue(called3.get(i) == parameters.get(i + 1));
                }
                assertTrue(called4.get(0) == parameters.get(2));
            }
        });
        thread.setUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                // Forward exceptions from test thread.
                assertFalse(true);
            }
        });
        thread.start();
        try {
            thread.join();
        } catch (InterruptedException ex) { }
    }

    @SmallTest
    @Feature({"Chromoting"})
    public static void testPromisedEventWithLooper() {
        assertNotNull(Looper.myLooper());
        Event.Raisable<Object> event = new Event.PromisedRaisable<>();
        final List<Object> called1 = new ArrayList<>();
        final List<Object> called2 = new ArrayList<>();
        final List<Object> called3 = new ArrayList<>();
        final List<Object> called4 = new ArrayList<>();
        final List<Object> parameters = new ArrayList<>();
        event.add(new Event.ParameterRunnable<Object>() {
            @Override
            public void run(Object obj) {
                called1.add(obj);
            }
        });
        Object parameter = new Object();
        event.raise(parameter);
        parameters.add(parameter);
        event.add(new Event.ParameterRunnable<Object>() {
            @Override
            public void run(Object obj) {
                called2.add(obj);
            }
        });
        parameter = new Object();
        event.raise(parameter);
        parameters.add(parameter);
        event.add(new Event.ParameterRunnable<Object>() {
            @Override
            public void run(Object obj) {
                called3.add(obj);
            }
        });
        parameter = new Object();
        event.raise(parameter);
        parameters.add(parameter);
        event.add(new Event.ParameterRunnable<Object>() {
            @Override
            public void run(Object obj) {
                called4.add(obj);
            }
        });

        Handler h = new Handler(Looper.myLooper());
        h.post(new Runnable() {
            @Override
            public void run() {
                Looper.myLooper().quit();
            }
        });
        Looper.loop();

        assertEquals(called1.size(), 3);
        assertEquals(called2.size(), 3);
        assertEquals(called3.size(), 2);
        assertEquals(called4.size(), 1);

        for (int i = 0; i < 3; i++) {
            assertTrue(called1.get(i) == parameters.get(i));
        }
        assertTrue(called2.get(0) == parameters.get(1));
        for (int i = 0; i < 2; i++) {
            assertTrue(called2.get(i + 1) == parameters.get(2));
            assertTrue(called3.get(i) == parameters.get(2));
        }
        assertTrue(called4.get(0) == parameters.get(2));
    }
}
