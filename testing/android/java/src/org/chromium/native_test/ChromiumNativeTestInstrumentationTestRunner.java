// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 *  An Instrumentation that runs tests based on ChromeNativeTestActivity.
 */
public class ChromiumNativeTestInstrumentationTestRunner extends Instrumentation {

    private static final String TAG = "ChromiumNativeTestInstrumentationTestRunner";
    private static final Pattern RE_TEST_OUTPUT = Pattern.compile("\\[ *([^ ]*) *\\] ?([^ ]*) .*");

    private static interface ResultsBundleGenerator {
        public Bundle generate(Map<String, TestResult> rawResults);
    }

    private String mCommandLineFile;
    private String mCommandLineFlags;
    private Bundle mLogBundle;
    private ResultsBundleGenerator mBundleGenerator;

    @Override
    public void onCreate(Bundle arguments) {
        mCommandLineFile = arguments.getString(ChromeNativeTestActivity.EXTRA_COMMAND_LINE_FILE);
        mCommandLineFlags = arguments.getString(ChromeNativeTestActivity.EXTRA_COMMAND_LINE_FLAGS);
        mLogBundle = new Bundle();
        mBundleGenerator = new RobotiumBundleGenerator();
        start();
    }

    @Override
    public void onStart() {
        super.onStart();
        Bundle results = runTests();
        finish(Activity.RESULT_OK, results);
    }

    /** Runs the tests in the ChromeNativeTestActivity and returns a Bundle containing the results.
     */
    private Bundle runTests() {
        Log.i(TAG, "Creating activity.");
        Activity activityUnderTest = startNativeTestActivity();

        Log.i(TAG, "Getting results from FIFO.");
        Map<String, TestResult> results = parseResultsFromFifo(activityUnderTest);

        Log.i(TAG, "Finishing activity.");
        activityUnderTest.finish();

        Log.i(TAG, "Parsing results and generating output.");
        return mBundleGenerator.generate(results);
    }

    /** Starts the ChromeNativeTestActivty.
     */
    private Activity startNativeTestActivity() {
        Intent i = new Intent(Intent.ACTION_MAIN);
        i.setComponent(new ComponentName(
                "org.chromium.native_test",
                "org.chromium.native_test.ChromeNativeTestActivity"));
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (mCommandLineFile != null) {
            Log.i(TAG, "Passing command line file extra: " + mCommandLineFile);
            i.putExtra(ChromeNativeTestActivity.EXTRA_COMMAND_LINE_FILE, mCommandLineFile);
        }
        if (mCommandLineFlags != null) {
            Log.i(TAG, "Passing command line flag extra: " + mCommandLineFlags);
            i.putExtra(ChromeNativeTestActivity.EXTRA_COMMAND_LINE_FLAGS, mCommandLineFlags);
        }
        return startActivitySync(i);
    }

    private static enum TestResult {
        PASSED, FAILED, ERROR, UNKNOWN
    }

    /**
     *  Generates a map between test names and test results from the instrumented Activity's FIFO.
     */
    private Map<String, TestResult> parseResultsFromFifo(Activity activityUnderTest) {
        Map<String, TestResult> results = new HashMap<String, TestResult>();

        File fifo = null;
        BufferedReader r = null;

        try {
            // Wait for the test to create the FIFO.
            fifo = new File(getTargetContext().getFilesDir().getAbsolutePath(), "test.fifo");
            while (!fifo.exists()) {
                Thread.sleep(1000);
            }

            r = new BufferedReader(
                    new InputStreamReader(new BufferedInputStream(new FileInputStream(fifo))));

            StringBuilder resultsStr = new StringBuilder();
            for (String l = r.readLine(); l != null && !l.equals("<<ScopedMainEntryLogger");
                    l = r.readLine()) {
                Matcher m = RE_TEST_OUTPUT.matcher(l);
                if (m.matches()) {
                    if (m.group(1).equals("RUN")) {
                        results.put(m.group(2), TestResult.UNKNOWN);
                    } else if (m.group(1).equals("FAILED")) {
                        results.put(m.group(2), TestResult.FAILED);
                    } else if (m.group(1).equals("OK")) {
                        results.put(m.group(2), TestResult.PASSED);
                    }
                }
                resultsStr.append(l);
                resultsStr.append("\n");
            }
            mLogBundle.putString(Instrumentation.REPORT_KEY_STREAMRESULT, resultsStr.toString());
            sendStatus(0, mLogBundle);
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted while waiting for FIFO file creation: " + e.toString());
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Couldn't find FIFO file: " + e.toString());
        } catch (IOException e) {
            Log.e(TAG, "Error handling FIFO file: " + e.toString());
        } finally {
            if (r != null) {
                try {
                    r.close();
                } catch (IOException e) {
                    Log.e(TAG, "Error while closing FIFO reader.");
                }
            }
            if (fifo != null) {
                if (!fifo.delete()) {
                    Log.e(TAG, "Unable to delete " + fifo.getAbsolutePath());
                }
            }
        }
        return results;
    }

    /**
     * Creates a results bundle that emulates the one created by Robotium.
     */
    private static class RobotiumBundleGenerator implements ResultsBundleGenerator {
        public Bundle generate(Map<String, TestResult> rawResults) {
            Bundle resultsBundle = new Bundle();

            int testsPassed = 0;
            int testsFailed = 0;

            for (Map.Entry<String, TestResult> entry : rawResults.entrySet()) {
                switch (entry.getValue()) {
                    case PASSED:
                        ++testsPassed;
                        break;
                    case FAILED:
                        ++testsFailed;
                        break;
                    default:
                        Log.w(TAG, "Unhandled: " + entry.getKey() + ", "
                                + entry.getValue().toString());
                        break;
                }
            }

            StringBuilder resultBuilder = new StringBuilder();
            resultBuilder.append("\nOK (" + Integer.toString(testsPassed) + " tests)");
            if (testsFailed > 0) {
                resultBuilder.append(
                        "\nFAILURES!!! Tests run: " + Integer.toString(rawResults.size())
                        + ", Failures: " + Integer.toString(testsFailed) + ", Errors: 0");
            }
            resultsBundle.putString(Instrumentation.REPORT_KEY_STREAMRESULT,
                    resultBuilder.toString());
            return resultsBundle;
        }
    }

}
