// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.tools.binary_size;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.charset.Charset;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Queue;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A pool of workers that run 'addr2line', looking up source locations for
 * addresses that are fed into the pool via a queue. As address lookups
 * complete, records are placed onto an output queue.
 */
class Addr2LineWorkerPool {
    private static final Charset sAscii = Charset.forName("US-ASCII");
    private final Addr2LineWorker[] mWorkers;
    private final ArrayBlockingQueue<Record> mRecordsIn = new ArrayBlockingQueue<Record>(1000);
    private final Queue<Record> mRecordsOut = new ConcurrentLinkedQueue<Record>();
    private final CountDownLatch mCompletionLatch;
    private final String mAddr2linePath;
    private final String mLibraryPath;
    private final boolean mDisambiguate;
    private final boolean mDedupe;
    private final String stripLocation;
    private final ConcurrentMap<Long, Record> mAddressesSeen =
            new ConcurrentHashMap<Long, Record>(100000, 0.75f, 32);
    private volatile Map<String,String> mFileLookupTable = null;
    private final AtomicInteger mDisambiguationSuccessCount = new AtomicInteger(0);
    private final AtomicInteger mDisambiguationFailureCount = new AtomicInteger(0);
    private final AtomicInteger mDedupeCount = new AtomicInteger(0);

    /**
     * These are the suffixes of files that we are potentially interested in
     * searching during symbol lookups when disambiguation is enabled. Anything
     * that could theoretically end up being linked into the library file
     * should be listed here.
     * <p>
     * IMPORTANT: All of these should be lowercase. When doing comparisons,
     * always lowercase the file name before attempting the match.
     */
    private static final String[] INTERESTING_FILE_ENDINGS = new String[]{
            ".c", ".cc", ".h", ".cp", ".cpp", ".cxx", ".c++", ".asm", ".inc", ".s", ".hxx"
    };


    Addr2LineWorkerPool(final int size,
            final String addr2linePath, final String libraryPath,
            final boolean disambiguate, final boolean dedupe)
                    throws IOException {
        this.mAddr2linePath = addr2linePath;
        this.mLibraryPath = libraryPath;
        this.mDisambiguate = disambiguate;
        this.mDedupe = dedupe;

        // Prepare disambiguation table if necessary. This must be done
        // before launching the threads in the processing pool for visibility.
        if (disambiguate) {
            try {
                createDisambiguationTable();
            } catch (IOException e) {
                throw new RuntimeException("Can't create lookup table", e);
            }
        }

        // The library is in, e.g.: src/out/Release
        // Convert all output paths to be relative to src.
        // Strip everything up to and including "src/".
        String canonical = new File(libraryPath).getCanonicalPath();
        int end = canonical.lastIndexOf("/src/");
        if (end < 0) {
            // Shouldn't happen if the library exists.
            throw new RuntimeException("Bad library path: " + libraryPath +
                    ". Library is expected to be within a build directory.");
        }
        stripLocation = canonical.substring(0, end + "/src/".length());

        mWorkers = new Addr2LineWorker[size];
        mCompletionLatch = new CountDownLatch(size);
        for (int x = 0; x < mWorkers.length; x++) {
            mWorkers[x] = new Addr2LineWorker();
        }
    }

    void submit(Record record) throws InterruptedException {
        mRecordsIn.put(record);
    }

    void allRecordsSubmitted() {
        for (Addr2LineWorker worker : mWorkers) {
            worker.stopIfQueueIsEmpty = true;
        }
    }

    boolean await(int amount, TimeUnit unit) throws InterruptedException {
        return mCompletionLatch.await(amount, unit);
    }

    /**
     * @param value the base value
     * @param percent absolute percentage to jitter by (in the range [0,100])
     * @return a value that is on average uniformly distributed within
     * plus or minus <em>percent</em> of the base value.
     */
    private static int jitter(final int value, final int percent) {
        Random r = new Random();
        int delta = (r.nextBoolean() ? 1 : -1) * r.nextInt((percent * value) / 100);
        return value + delta;
    }


    /**
     * A class that encapsulates an addr2line process and the work that it
     * needs to do.
     */
    private class Addr2LineWorker {
        // Our work queues
        private final AtomicReference<Process> processRef = new AtomicReference<Process>();
        private final Thread workerThread;
        private volatile boolean stopIfQueueIsEmpty = false;

        /**
         * After this many addresses have been processed, the addr2line process
         * itself will be recycled. This prevents the addr2line process from
         * getting too huge, which in turn allows more parallel addr2line
         * processes to run. There is a balance to be achieved here, as
         * creating a new addr2line process is very costly. A value of
         * approximately 2000 appears, empirically, to be a good tradeoff
         * on a modern machine; memory usage stays tolerable, and good
         * throughput can be achieved. The value is jittered by +/- 10% so that
         * the processes don't all restart at once.
         */
        private final int processRecycleThreshold = jitter(2000, 10);

        private Addr2LineWorker() throws IOException {
            this.processRef.set(createAddr2LineProcess());
            workerThread = new Thread(new Addr2LineTask(), "Addr2Line Worker");
            workerThread.setDaemon(true);
            workerThread.start();
        }

        /**
         * Builds a new addr2line process for use in this worker.
         * @return the process
         * @throws IOException if unable to create the process for any reason
         */
        private Process createAddr2LineProcess()
                throws IOException {
            ProcessBuilder builder = new ProcessBuilder(mAddr2linePath, "-e", mLibraryPath, "-f");
            Process process = builder.start();
            return process;
        }

        /**
         * Reads records from the input queue and pipes addresses into
         * addr2line, using the output to complete the record and pushing
         * the record into the output queue.
         */
        private class Addr2LineTask implements Runnable {
            @Override
            public void run() {
                int processTaskCounter = 0;
                InputStream inStream = processRef.get().getInputStream();
                Reader isr = new InputStreamReader(inStream);
                BufferedReader reader = new BufferedReader(isr);
                try {
                    while (true) {
                        // Check for a task.
                        final Record record = mRecordsIn.poll(1, TimeUnit.SECONDS);
                        if (record == null) {
                            if (stopIfQueueIsEmpty) {
                                // All tasks have been submitted, so if the
                                // queue is empty then there is nothing left
                                // to do and it's ready to shut down
                                return;
                            }
                            continue; // else, queue starvation. Try again.
                        }

                        // Avoid reprocessing previously-seen symbols if
                        // deduping is enabled
                        if (tryDedupe(record)) continue;

                        // Create a new reader if the addr2line process is new
                        // or has been recycled. A single reader will be used
                        // for the entirety of the addr2line process' lifetime.
                        final Process process = processRef.get();
                        if (inStream == null) {
                            inStream = process.getInputStream();
                            isr = new InputStreamReader(inStream);
                            reader = new BufferedReader(isr);
                        }

                        // Write the address to stdin of addr2line
                        process.getOutputStream().write(record.address.getBytes(sAscii));
                        process.getOutputStream().write('\n');
                        process.getOutputStream().flush();

                        // Read the answer from addr2line. Example:
                        // ABGRToYRow_C
                        // /src/out/Release/../../third_party/libyuv/source/row_common.cc:293
                        final String name = reader.readLine();
                        if (name == null || name.isEmpty()) {
                            stopIfQueueIsEmpty = true;
                            continue;
                        }

                        String location = reader.readLine();
                        if (location == null || location.isEmpty()) {
                            stopIfQueueIsEmpty = true;
                            continue;
                        }

                        record.resolvedSuccessfully = !(
                                name.equals("??") && location.equals("??:0"));

                        if (record.resolvedSuccessfully) {
                            // Keep the name from the initial NM dump.
                            // Some addr2line impls, such as the one for Android
                            // on ARM, seem to lose data here.
                            // Note that the location may also include a line
                            // discriminator, which maps to a part of a line.
                            // Not currently used.
                            record.location = processLocation(location);;
                        }

                        // Check if there is more input on the stream.
                        // If there is then it is a serious processing
                        // error, and reading anything else would de-sync
                        // the input queue from the results being read.
                        if (inStream.available() > 0) {
                            throw new IllegalStateException(
                                    "Alignment mismatch in output from address " + record.address);
                        }

                        // Track stats and move record to output queue
                        processTaskCounter++;
                        mRecordsOut.add(record);

                        // If the addr2line process has done too much work,
                        // kill it and start a new one to reduce memory
                        // pressure created by the pool.
                        if (processTaskCounter >= processRecycleThreshold) {
                            // Out with the old...
                            try {
                                processRef.get().destroy();
                            } catch (Exception e) {
                                System.err.println("WARNING: zombie process");
                                e.printStackTrace();
                            }
                            // ... and in with the new!
                            try {
                                processRef.set(createAddr2LineProcess());
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                            processTaskCounter = 0;
                            inStream = null; // New readers need to be created next iteration
                        }
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                } finally {
                    try {
                        // Make an attempt to clean up. If we fail, there is
                        // nothing we can do beyond this.
                        processRef.get().destroy();
                    } catch (Exception e) {
                        // There's nothing more we can do here.
                    }
                    mCompletionLatch.countDown();
                }
            }
        }

        /**
         * Examines the location from a record and attempts to canonicalize
         * it and strip off the common source root.
         * @param location the location to process
         * @return the canonicalized, stripped location or the original input
         * string if the location cannot be canonicalized
         */
        private String processLocation(String location) {
            if (location.startsWith("/")) {
                try {
                    location = new File(location).getCanonicalPath();
                } catch (IOException e) {
                    System.err.println("Unable to canonicalize path: " + location);
                }
            } else if (mDisambiguate) {
                // Ambiguous path (only has the file name)
                // Try dictionary lookup if that's enabled
                final int indexOfColon = location.lastIndexOf(':');
                final String key;
                final String line;
                if (indexOfColon != -1) {
                    key = location.substring(0, indexOfColon);
                    line = location.substring(indexOfColon + 1);
                } else {
                    key = location;
                    line = null;
                }
                final String found = mFileLookupTable.get(key);
                if (found != null) {
                    mDisambiguationSuccessCount.incrementAndGet();
                    location = found;
                    if (line != null) location = location + ":" + line;
                } else {
                    mDisambiguationFailureCount.incrementAndGet();
                }
            }
            if (location.startsWith(stripLocation)) {
                location = location.substring(stripLocation.length());
            }
            return location;
        }

        /**
         * Attempts to dedupe a record using a cache of previously-seen
         * addresses if and only if deduping is enabled.
         * @param record the record to attempt deduping
         * @return true if deduplication is enabled and the record references
         * an address that has already been seen; otherwise false
         */
        private boolean tryDedupe(Record record) {
            if (mDedupe) {
                long addressLong = Long.parseLong(record.address, 16);
                Record existing = mAddressesSeen.get(addressLong);
                if (existing != null) {
                    if (!existing.size.equals(record.size)) {
                        System.err.println("WARNING: Deduped address " +
                                record.address + " has a size mismatch, " +
                                existing.size + " != " + record.size);
                    }
                    mDedupeCount.incrementAndGet();
                    return true;
                }
                if (mAddressesSeen.putIfAbsent(addressLong, record) != null) {
                    // putIfAbsent is used to ensure that we have
                    // an accurate dedupeCount; otherwise, two
                    // workers could insert the same record in
                    // parallel without realizing that one of them
                    // was actually a duplicate.
                    mDedupeCount.incrementAndGet();
                    return true;
                }
            }
            return false;
        }
    }

    // TODO(andrewhayden): Make this less Android-specific
    private void createDisambiguationTable() throws IOException {
        // Convert /src/out/Release/lib/*.so -> /src/out/Release
        final File libraryOutputDirectory = new File(mLibraryPath)
            .getParentFile().getParentFile().getCanonicalFile();

        // Convert /src/out/Release -> /src
        final File root = libraryOutputDirectory
                .getParentFile().getParentFile().getCanonicalFile();

        // There is no code at the root of Chromium.
        // Ignore all the 'out' directories.
        mFileLookupTable = new HashMap<String, String>();
        Set<String> dupes = new HashSet<String>();
        for (File file : root.listFiles()) {
            if (file.isDirectory()) {
                String name = file.getName();
                if (name.startsWith("out")) {
                    if (new File(file, "Release").exists() || new File(file, "Debug").exists()) {
                        // It's an output directory, skip it - except for the
                        // 'obj' and 'gen' subdirectories that are siblings
                        // to the library file's parent dir, which is needed.
                        // Include those at the very end, since they are known.
                        continue;
                    }
                } else if (name.startsWith(".")) {
                    // Skip dot directories: .git, .svn, etcetera.
                    continue;
                }
                findInterestingFiles(file, dupes);
            }
        }

        // Include directories that contain generated resources we are likely
        // to encounter in the symbol table.
        findInterestingFiles(new File(libraryOutputDirectory, "gen"), dupes);
        findInterestingFiles(new File(libraryOutputDirectory, "obj"), dupes);

        // Any duplicates in the filesystem can't be used for disambiguation
        // because it is not obvious which of the duplicates is the true source.
        // Therefore, discard all files that have duplicate names.
        for (String dupe : dupes) {
            mFileLookupTable.remove(dupe);
        }
    }

    // TODO(andrewhayden): Could integrate with build system to know EXACTLY
    // what is out there. This would avoid the need for the dupes set, which
    // would make it possible to do much better deduping.
    private void findInterestingFiles(File directory, Set<String> dupes) {
        for (File file : directory.listFiles()) {
            if (file.isDirectory() && file.canRead()) {
                if (!file.getName().startsWith(".")) {
                    findInterestingFiles(file, dupes);
                }
            } else {
                String name = file.getName();
                String normalized = name.toLowerCase();
                for (String ending : INTERESTING_FILE_ENDINGS) {
                    if (normalized.endsWith(ending)) {
                        String other = mFileLookupTable.put(
                                name, file.getAbsolutePath());
                        if (other != null) dupes.add(name);
                    }
                }
            }
        }
    }

    /**
     * Polls the output queue for the next record.
     * @return the next record
     */
    Record poll() {
        return mRecordsOut.poll();
    }

    /**
     * @return the number of ambiguous paths successfully disambiguated
     */
    int getDisambiguationSuccessCount() {
        return mDisambiguationSuccessCount.get();
    }

    /**
     * @return the number of ambiguous paths that couldn't be disambiguated
     */
    int getDisambiguationFailureCount() {
        return mDisambiguationFailureCount.get();
    }

    /**
     * @return the number of symbols deduped
     */
    int getDedupeCount() {
        return mDedupeCount.get();
    }
}