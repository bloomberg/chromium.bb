// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.tools.binary_size;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A tool for parallelizing "addr2line" against a given binary.
 * The tool runs "nm" to dump the symbols from a library, then spawns a pool
 * of addr2name workers that resolve addresses to name in parallel.
 * <p>
 * This tool is intentionally written to be standalone so that it can be
 * compiled without reliance upon any other libraries. All that is required
 * is a vanilla installation of the Java Runtime Environment, 1.5 or later.
 */
// TODO(andrewhayden): Rewrite entire tool in Python
public class ParallelAddress2Line {
    private final AtomicBoolean mStillEnqueuing = new AtomicBoolean(true);
    private final AtomicInteger mEnqueuedCount =
        new AtomicInteger(Integer.MAX_VALUE);
    private final AtomicInteger mDoneCount = new AtomicInteger(0);
    private final AtomicInteger mSuccessCount = new AtomicInteger(0);
    private final AtomicInteger mAddressSkipCount = new AtomicInteger(0);
    private final String mLibraryPath;
    private final String mNmPath;
    private final String mNmInPath;
    private final String mAddr2linePath;
    private final boolean mVerbose;
    private final boolean mNoProgress;
    private Addr2LineWorkerPool mPool;
    private final boolean mNoDedupe;
    private final boolean mDisambiguate;
    private final NmDumper mNmDumper;

    private static final String USAGE =
        "--addr2line    [ARG]    instead of the 'addr2line' in $PATH, use this (e.g.,\n" +
        "                        arch-specific binary) (optional)\n" +
        "--disambiguate          create a listing of all source files which can be used\n" +
        "                        to disambiguate some percentage of ambiguous source\n" +
        "                        references; only useful on some architectures and adds\n" +
        "                        significant startup cost (optional)\n" +
        "--failfile     [ARG]    output symbols from failed lookups to the specified\n" +
        "                        file (optional)\n" +
        "--library      [ARG]    path to the library to process, e.g.\n" +
        "                        out/Release/lib/libchromeview.so (required)\n" +
        "--nm           [ARG]    instead of the 'nm' in $PATH, use this (e.g.,\n" +
        "                        arch-specific binary) (optional)\n" +
        "--nm-infile    [ARG]    instead of running nm on the specified library,\n" +
        "                        ingest the specified nm file. (optional)\n" +
        "--no-dedupe             don't de-dupe symbols that live at the same address;\n" +
        "                        deduping more accurately describes the use of space\n" +
        "                        within the binary; if spatial analysis is your goal,\n" +
        "                        leave deduplication on. (optional)\n" +
        "--no-progress           don't output periodic progress reports (optional)\n" +
        "--outfile      [ARG]    output results into the specified file (required)\n" +
        "--skipfile     [ARG]    output skipped symbols to the specified file (optional)\n" +
        "--threads      [ARG]    number of parallel worker threads to create. Start low\n" +
        "                        and watch your memory, defaults to 1 (optional)\n" +
        "--verbose               be verbose (optional)\n";

    // Regex for parsing "nm" output. A sample line looks like this:
    // 0167b39c 00000018 t ACCESS_DESCRIPTION_free /path/file.c:95
    //
    // The fields are: address, size, type, name, source location
    // Regular expression explained ( see also: https://xkcd.com/208 ):
    // ([0-9a-f]{8}+)    The address
    // [\\s]+            Whitespace separator
    // ([0-9a-f]{8}+)    The size. From here on out it's all optional.
    // [\\s]+            Whitespace separator
    // (\\S?)            The symbol type, which is any non-whitespace char
    // [\\s*]            Whitespace separator
    // ([^\\t]*)         Symbol name, any non-tab character (spaces ok!)
    // [\\t]?            Tab separator
    // (.*)              The location (filename[:linennum|?][ (discriminator n)]
    private static final Pattern sNmPattern = Pattern.compile(
            "([0-9a-f]{8}+)[\\s]+([0-9a-f]{8}+)[\\s]*(\\S?)[\\s*]([^\\t]*)[\\t]?(.*)");

    private ParallelAddress2Line(
            final String libraryPath,
            final String nmPath,
            final String nmInPath,
            final String addr2linePath,
            final String outPath,
            final String skipPath,
            final String failPath,
            final boolean verbose,
            final boolean noProgress,
            final boolean noDedupe,
            final boolean disambiguate) {
        this.mLibraryPath = libraryPath;
        this.mNmPath = nmPath;
        this.mNmInPath = nmInPath;
        this.mAddr2linePath = addr2linePath;
        this.mVerbose = verbose;
        this.mNoProgress = noProgress;
        this.mNoDedupe = noDedupe;
        this.mDisambiguate = disambiguate;
        this.mNmDumper = new NmDumper(outPath, failPath, skipPath);

        final File libraryFile = new File(libraryPath);
        if (!(libraryFile.exists() && libraryFile.canRead())) {
            throw new IllegalStateException("Can't read library file: " + libraryPath);
        }
    }

    private static final File findFile(File directory, String target) {
        for (File file : directory.listFiles()) {
            if (file.isDirectory() && file.canRead()) {
                File result = findFile(file, target);
                if (result != null) return result;
            } else {
                if (target.equals(file.getName())) return file;
            }
        }
        return null;
    }

    private void run(final int addr2linePoolSize) throws InterruptedException {
        try {
            runInternal(addr2linePoolSize);
        } finally {
            mNmDumper.close();
        }
    }

    private void runInternal(final int addr2linePoolSize) throws InterruptedException {
        // Step 1: Dump symbols with nm
        final String nmOutputPath;
        if (mNmInPath == null) {
            // Generate nm output with nm binary
            logVerbose("Running nm to dump symbols from " + mLibraryPath);
            try {
                nmOutputPath = dumpSymbols();
            } catch (Exception e) {
                throw new RuntimeException("nm failed", e);
            }
        } else {
            // Use user-supplied nm output
            logVerbose("Using user-supplied nm file: " + mNmInPath);
            nmOutputPath = mNmInPath;
        }

        // Step 2: Prepare addr2line worker pool to process nm output
        try {
            logVerbose("Creating " + addr2linePoolSize + " workers for " + mAddr2linePath);
            mPool = new Addr2LineWorkerPool(addr2linePoolSize,
                    mAddr2linePath, mLibraryPath, mDisambiguate, !mNoDedupe);
        } catch (IOException e) {
            throw new RuntimeException("Couldn't initialize name2address pool!", e);
        }

        // Step 3: Spool symbol-processing tasks to workers
        final long startTime = System.currentTimeMillis();
        Timer timer = null;
        if (!mNoProgress) {
            timer = startTaskMonitor(startTime);
        }
        final int queued = spoolTasks(nmOutputPath);

        // All tasks have been enqueued.
        mEnqueuedCount.set(queued);
        mStillEnqueuing.set(false);
        mPool.allRecordsSubmitted();
        float percentAddressesSkipped = 100f * (mAddressSkipCount.floatValue()
            / (queued + mAddressSkipCount.get()));
        float percentAddressesQueued = 100f - percentAddressesSkipped;
        int totalAddresses = mAddressSkipCount.get() + queued;
        logVerbose("All addresses have been enqueued (total " + queued + ").");
        // Remember that the queue to which the addresses was enqueued is of a
        // small fixed size; by the time this code executes, there is very
        // little work left to do. Await the termination of the pool with a
        // reasonable timeout for safety purposes.
        boolean timedOut = !mPool.await(5, TimeUnit.MINUTES);
        if (timedOut) {
            throw new RuntimeException("Worker pool did not terminate!");
        }
        if (!mNoProgress) timer.cancel();
        log(totalAddresses + " addresses discovered; " +
                queued + " queued for processing (" +
                String.format("%.2f", percentAddressesQueued) + "%), " +
                mAddressSkipCount.get() + " skipped (" +
                String.format("%.2f", percentAddressesSkipped) + "%)");
        dumpStats(startTime);
        log("Done.");
    }

    /**
     * Monitors the pool periodically printing status updates to stdout.
     * @param addressProcessingStartTime the time address processing began
     * @return the daemon timer that is generating the status updates
     */
    private final Timer startTaskMonitor(
        final long addressProcessingStartTime) {
        Runnable monitorTask = new OutputSpooler();
        Thread monitor = new Thread(monitorTask, "progress monitor");
        monitor.setDaemon(true);
        monitor.start();

        TimerTask task = new TimerTask() {
            @Override
            public void run() {
                dumpStats(addressProcessingStartTime);
            }
        };
        Timer timer = new Timer(true);
        timer.schedule(task, 1000L, 1000L);
        return timer;
    }

    /**
     * Spools address-lookup tasks to the addr2line workers.
     * This method will block until most of (or possibly all of) the tasks
     * have been spooled.
     * If a skip path is set, any line in the input file that doesn't have
     * an address will be copied into the skip file.
     *
     * @param inputPath the path to the dump produced by nm
     * @return the number of tasks spooled
     */
    private final int spoolTasks(final String inputPath) {
        FileReader inputReader = null;
        try {
            inputReader = new FileReader(inputPath);
        } catch (IOException e) {
            throw new RuntimeException("Can't open input file: " + inputPath, e);
        }
        final BufferedReader bufferedReader = new BufferedReader(inputReader);

        String currentLine = null;
        int numSpooled = 0;
        try {
            while ((currentLine = bufferedReader.readLine()) != null) {
                try {
                    final Matcher matcher = sNmPattern.matcher(currentLine);
                    if (!matcher.matches()) {
                        // HACK: Special case for ICU data.
                        // This thing is HUGE (5+ megabytes) and is currently
                        // missed because there is no size information.
                        // torne@ has volunteered to patch the generation code
                        // so that the generated ASM includes a size attribute
                        // so that this hard-coding can go away in the future.
                        if (currentLine.endsWith("icudt46_dat")) {
                            Record record = getIcuRecord(currentLine);
                            if (record != null) {
                                numSpooled++;
                                mPool.submit(record);
                                continue;
                            }
                        }
                        mNmDumper.skipped(currentLine);
                        mAddressSkipCount.incrementAndGet();
                        continue;
                    }
                    final Record record = new Record();
                    record.address = matcher.group(1);
                    record.size = matcher.group(2);
                    if (matcher.groupCount() >= 3) {
                        record.symbolType = matcher.group(3).charAt(0);
                    }
                    if (matcher.groupCount() >= 4) {
                        // May or may not be present
                        record.symbolName = matcher.group(4);
                    }
                    numSpooled++;
                    mPool.submit(record);
                } catch (Exception e) {
                    throw new RuntimeException("Error processing line: '" + currentLine + "'", e);
                }
            }
        } catch (Exception e) {
            throw new RuntimeException("Input processing failed", e);
        } finally {
            try {
                bufferedReader.close();
            } catch (Exception ignored) {
                // Nothing to be done
            }
            try {
                inputReader.close();
            } catch (Exception ignored) {
                // Nothing to be done
            }
        }
        return numSpooled;
    }

    private Record getIcuRecord(String line) throws IOException {
        // Line looks like this:
        // 01c9ee00 r icudt46_dat
        String[] parts = line.split("\\s");
        if (parts.length != 3) return null;

        // Convert /src/out/Release/lib/[libraryfile] -> /src/out/Release
        final File libraryOutputDirectory = new File(mLibraryPath)
            .getParentFile().getParentFile().getCanonicalFile();
        final File icuDir = new File(
                libraryOutputDirectory.getAbsolutePath() +
                "/obj/third_party/icu");
        final File icuFile = findFile(icuDir, "icudata.icudt46l_dat.o");
        if (!icuFile.exists()) return null;
        final Record record = new Record();
        record.address = parts[0];
        record.symbolType = parts[1].charAt(0);
        record.symbolName = parts[2];
        record.size = Integer.toHexString((int) icuFile.length());
        record.location = icuFile.getCanonicalPath() + ":0";
        record.resolvedSuccessfully = true;
        while (record.size.length() < 8) {
            record.size = "0" + record.size;
        }
        return record;
    }

    /**
     * @return the path to the file that nm wrote
     * @throws Exception
     * @throws FileNotFoundException
     * @throws InterruptedException
     */
    private String dumpSymbols() throws Exception, FileNotFoundException, InterruptedException {
        final Process process = createNmProcess();
        final File tempFile = File.createTempFile("ParallelAddress2Line", "nm");
        tempFile.deleteOnExit();
        final CountDownLatch completionLatch = sink(
                process.getInputStream(), new FileOutputStream(tempFile), true);
        sink(process.getErrorStream(), System.err, false);
        logVerbose("Dumping symbols to: " + tempFile.getAbsolutePath());
        final int nmRc = process.waitFor();
        if (nmRc != 0) {
            throw new RuntimeException("nm process returned " + nmRc);
        }
        completionLatch.await(); // wait for output to be done
        return tempFile.getAbsolutePath();
    }

    private void dumpStats(final long startTime) {
        long successful = mSuccessCount.get();
        long doneNow = mDoneCount.get();
        long unsuccessful = doneNow - successful;
        float successPercent = doneNow == 0 ? 100f : 100f * ((float)successful / (float)doneNow);
        long elapsedMillis = System.currentTimeMillis() - startTime;
        float elapsedSeconds = elapsedMillis / 1000f;
        long throughput = doneNow / (elapsedMillis / 1000);
        final int mapLookupSuccess = mPool.getDisambiguationSuccessCount();
        final int mapLookupFailure = mPool.getDisambiguationFailureCount();
        final int mapLookupTotal = mapLookupSuccess + mapLookupFailure;
        float mapLookupSuccessPercent = 0f;
        if (mapLookupTotal != 0 && mapLookupSuccess != 0) {
            mapLookupSuccessPercent = 100f *
                    ((float) mapLookupSuccess / (float) mapLookupTotal);
        }

        log(doneNow + " addresses processed (" +
                mSuccessCount.get() + " ok, " + unsuccessful + " failed)" +
                ", avg " + throughput + " addresses/sec, " +
                String.format("%.2f", successPercent) + "% success" +
                ", " + mapLookupTotal + " ambiguous path" +
                (!mDisambiguate ? "" :
                    ", (" + String.format("%.2f", mapLookupSuccessPercent) + "% disambiguated)") +
                (mNoDedupe ? "" : ", " + mPool.getDedupeCount() + " deduped") +
                ", elapsed time " + String.format("%.3f", elapsedSeconds) + " seconds");
    }

    private Process createNmProcess() throws Exception {
        ProcessBuilder builder = new ProcessBuilder(
                mNmPath,
                "-C", // demangle (for the humans)
                "-S", // print size
                mLibraryPath);
        logVerbose("Creating process: " + builder.command());
        return builder.start();
    }

    /**
     * Make a pipe to drain the specified input stream into the specified
     * output stream asynchronously.
     * @param in read from here
     * @param out and write to here
     * @param closeWhenDone whether or not to close the target output stream
     * when the input stream terminates
     * @return a latch that can be used to await the final write to the
     * output stream, which occurs when either of the streams closes
     */
    private static final CountDownLatch sink(final InputStream in,
        final OutputStream out, final boolean closeWhenDone) {
        final CountDownLatch latch = new CountDownLatch(1);
        final Runnable task = new Runnable() {
            @Override
            public void run() {
                byte[] buffer = new byte[4096];
                try {
                    int numRead = 0;
                    do {
                        numRead = in.read(buffer);
                        if (numRead > 0) {
                            out.write(buffer, 0, numRead);
                            out.flush();
                        }
                    } while (numRead >= 0);
                } catch (Exception e) {
                    e.printStackTrace();
                } finally {
                    try { out.flush(); } catch (Exception ignored) {
                        // Nothing to be done
                    }
                    if (closeWhenDone) {
                        try { out.close(); } catch (Exception ignored) {
                            // Nothing to be done
                        }
                    }
                    latch.countDown();
                }
            }
        };
        final Thread worker = new Thread(task, "pipe " + in + "->" + out);
        worker.setDaemon(true);
        worker.start();
        return latch;
    }

    private final class OutputSpooler implements Runnable {
        @Override
        public void run() {
            do {
                readRecord();
            } while (mStillEnqueuing.get() || (mDoneCount.get() < mEnqueuedCount.get()));
        }

        /**
         * Read a record and process it.
         */
        private void readRecord() {
            Record record = mPool.poll();
            if (record != null) {
                mDoneCount.incrementAndGet();
                if (record.resolvedSuccessfully) {
                    mSuccessCount.incrementAndGet();
                    mNmDumper.succeeded(record);
                } else {
                    mNmDumper.failed(record);
                }
            } else {
                try {
                    // wait to keep going
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    /**
     * Log a message to the console.
     * @param message the message to log
     */
    private final void log(String message) {
        System.out.println(message);
    }

    /**
     * Log a message to the console iff verbose logging is enabled.
     * @param message the message to log
     */
    private final void logVerbose(String message) {
        if (mVerbose) log(message);
    }

    /**
     * Runs the tool. Run with --help for limited help.
     * @param args
     * @throws Exception if anything explodes
     */
    public static void main(String[] args) throws Exception {
        ParallelAddress2Line tool = new ParallelAddress2Line(
                getArg(args, "--library"),
                getArg(args, "--nm", "nm"),
                getArg(args, "--nm-infile", null),
                getArg(args, "--addr2line", "addr2line"),
                getArg(args, "--outfile"),
                getArg(args, "--skipfile", null),
                getArg(args, "--failfile", null),
                hasFlag(args, "--verbose"),
                hasFlag(args, "--no-progress"),
                hasFlag(args, "--no-dedupe"),
                hasFlag(args, "--disambiguate"));
        tool.run(Integer.parseInt(getArg(args, "--threads", "1")));
    }

    private static boolean hasFlag(String[] args, String name) {
        for (int x = 0; x < args.length; x++) if (name.equals(args[x])) return true;
        return false;
    }

    private static String getArg(String[] args, String name, String defaultValue) {
        for (int x = 0; x < args.length; x++) {
            if (name.equals(args[x])) {
                if (x < args.length - 1) return args[x + 1];
                throw new RuntimeException(name + " is missing a value\n" + USAGE);
            }
        }
        return defaultValue;
    }

    private static String getArg(String[] args, String name) {
        String result = getArg(args, name, null);
        if (result == null) throw new RuntimeException(name + " is required\n" + USAGE);
        return result;
    }
}