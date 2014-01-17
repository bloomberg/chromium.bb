// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.tools.binary_size;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;

/**
 * Converts records to a format that simulates output from 'nm'.
 */
class NmDumper {
    private final String mOutPath;
    private final String mSkipPath;
    private final String mFailPath;
    private final Output mOutput;

    /**
     * Create a new dumper that will output to the specified paths.
     * @param outPath where to write all records and lines, including lines
     * that are skipped or records that failed to resolve
     * @param failPath if not null, a path to which <em>only</em> records that
     * failed to resolve will be written
     * @param skipPath if not null, a path to which <em>only</em> lines that
     * were skipped will be written
     */
    NmDumper(final String outPath, final String failPath, final String skipPath) {
        mOutPath = outPath;
        mFailPath = failPath;
        mSkipPath = skipPath;
        mOutput = new Output();
    }

    /**
     * Close all output files.
     */
    void close() {
        mOutput.closeAll();
    }

    /**
     * Output a line that was skipped.
     * @param line the line
     */
    void skipped(String line) {
        mOutput.printSkip(line);
    }

    /**
     * Output a record that failed to resolve.
     * @param record the record
     */
    void failed(Record record) {
        mOutput.printFail(simulateNmOutput(record));
    }

    /**
     * Output a record that successfully resolved.
     * @param record the record
     */
    void succeeded(Record record) {
        mOutput.print(simulateNmOutput(record));
    }

    /**
     * Generate a string that looks like output from nm for a given record.
     * @param record the record
     * @return nm-like output
     */
    private static final String simulateNmOutput(final Record record) {
        StringBuilder builder = new StringBuilder(record.address);
        builder.append(' ');
        builder.append(record.size);
        builder.append(' ');
        builder.append(record.symbolType);
        builder.append(' ');
        builder.append(record.symbolName != null ? record.symbolName : "unknown");
        if (record.location != null) {
            builder.append('\t');
            builder.append(record.location);
        }
        return builder.toString();
    }

    private class Output {
        private final PrintWriter skipWriter;
        private final PrintWriter failWriter;
        private final PrintWriter outWriter;

        private Output() {
            try {
                new File(mOutPath).getParentFile().mkdirs();
                outWriter = new PrintWriter(mOutPath);
            } catch (FileNotFoundException e) {
                throw new RuntimeException("Can't open output file: " + mOutPath, e);
            }

            if (mFailPath != null) {
                try {
                    new File(mFailPath).getParentFile().mkdirs();
                    failWriter = new PrintWriter(mFailPath);
                } catch (FileNotFoundException e) {
                    throw new RuntimeException("Can't open fail file: " + mFailPath, e);
                }
            } else {
                failWriter = null;
            }

            if (mSkipPath != null) {
                try {
                    new File(mSkipPath).getParentFile().mkdirs();
                    skipWriter = new PrintWriter(mSkipPath);
                } catch (IOException e) {
                    throw new RuntimeException("Can't open skip file: " + mSkipPath, e);
                }
            } else  {
                skipWriter = null;
            }
        }

        private synchronized void println(PrintWriter writer, String string) {
            if (writer != null) {
                writer.println(string);
                writer.flush();
            }
        }

        synchronized void print(String string) {
            println(outWriter, string);
        }

        synchronized void printSkip(String string) {
            println(skipWriter, string);
            println(outWriter, string);
        }

        synchronized void printFail(String string) {
            println(failWriter, string);
            println(outWriter, string);
        }

        private void closeAll() {
            for (PrintWriter writer : new PrintWriter[]{outWriter, failWriter, skipWriter}) {
                if (writer != null) {
                    try {
                        writer.flush(); }
                    catch (Exception ignored) {
                        // Nothing to be done.
                    }
                    try {
                        writer.close(); }
                    catch (Exception ignored) {
                        // Nothing to be done.
                    }
                }
            }
        }
    }
}