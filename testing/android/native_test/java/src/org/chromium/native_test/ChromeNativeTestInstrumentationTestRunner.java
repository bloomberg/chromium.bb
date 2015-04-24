// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
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
public class ChromeNativeTestInstrumentationTestRunner extends Instrumentation {
    // TODO(jbudorick): Remove this extra when b/18981674 is fixed.
    public static final String EXTRA_ONLY_OUTPUT_FAILURES =
            "org.chromium.native_test.ChromeNativeTestInstrumentationTestRunner."
                    + "OnlyOutputFailures";

    private static final String TAG = "ChromeNativeTestInstrumentationTestRunner";

    private static final int ACCEPT_TIMEOUT_MS = 5000;
    private static final Pattern RE_TEST_OUTPUT = Pattern.compile("\\[ *([^ ]*) *\\] ?([^ ]+) .*");

    private static interface ResultsBundleGenerator {
        public Bundle generate(Map<String, TestResult> rawResults);
    }

    private String mCommandLineFile;
    private String mCommandLineFlags;
    private File mStdoutFile;
    private Bundle mLogBundle;
    private ResultsBundleGenerator mBundleGenerator;
    private boolean mOnlyOutputFailures;

    @Override
    public void onCreate(Bundle arguments) {
        mCommandLineFile = arguments.getString(ChromeNativeTestActivity.EXTRA_COMMAND_LINE_FILE);
        mCommandLineFlags = arguments.getString(ChromeNativeTestActivity.EXTRA_COMMAND_LINE_FLAGS);
        try {
            mStdoutFile = File.createTempFile(
                    ".temp_stdout_", ".txt", Environment.getExternalStorageDirectory());
            Log.i(TAG, "stdout file created: " + mStdoutFile.getAbsolutePath());
        } catch (IOException e) {
            Log.e(TAG, "Unable to create temporary stdout file." + e.toString());
            finish(Activity.RESULT_CANCELED, new Bundle());
            return;
        }
        mLogBundle = new Bundle();
        mBundleGenerator = new RobotiumBundleGenerator();
        mOnlyOutputFailures = arguments.containsKey(EXTRA_ONLY_OUTPUT_FAILURES);
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

        Log.i(TAG, "Waiting for tests to finish.");
        try {
            while (!activityUnderTest.isFinishing()) {
                Thread.sleep(100);
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted while waiting for activity to be destroyed: " + e.toString());
        }

        Log.i(TAG, "Getting results.");
        Map<String, TestResult> results = parseResults(activityUnderTest);

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
        i.putExtra(ChromeNativeTestActivity.EXTRA_STDOUT_FILE, mStdoutFile.getAbsolutePath());
        return startActivitySync(i);
    }

    private static enum TestResult {
        PASSED, FAILED, ERROR, UNKNOWN
    }

    /**
     *  Generates a map between test names and test results from the instrumented Activity's
     *  output.
     */
    private Map<String, TestResult> parseResults(Activity activityUnderTest) {
        Map<String, TestResult> results = new HashMap<String, TestResult>();

        BufferedReader r = null;

        try {
            if (mStdoutFile == null || !mStdoutFile.exists()) {
                Log.e(TAG, "Unable to find stdout file.");
                return results;
            }

            r = new BufferedReader(new InputStreamReader(
                    new BufferedInputStream(new FileInputStream(mStdoutFile))));

            for (String l = r.readLine(); l != null && !l.equals("<<ScopedMainEntryLogger");
                    l = r.readLine()) {
                Matcher m = RE_TEST_OUTPUT.matcher(l);
                boolean isFailure = false;
                if (m.matches()) {
                    if (m.group(1).equals("RUN")) {
                        results.put(m.group(2), TestResult.UNKNOWN);
                    } else if (m.group(1).equals("FAILED")) {
                        results.put(m.group(2), TestResult.FAILED);
                        isFailure = true;
                        mLogBundle.putString(Instrumentation.REPORT_KEY_STREAMRESULT, l + "\n");
                        sendStatus(0, mLogBundle);
                    } else if (m.group(1).equals("OK")) {
                        results.put(m.group(2), TestResult.PASSED);
                    }
                }

                // TODO(jbudorick): mOnlyOutputFailures is a workaround for b/18981674. Remove it
                // once that issue is fixed.
                if (!mOnlyOutputFailures || isFailure) {
                    mLogBundle.putString(Instrumentation.REPORT_KEY_STREAMRESULT, l + "\n");
                    sendStatus(0, mLogBundle);
                }
                Log.i(TAG, l);
            }
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Couldn't find stdout file file: " + e.toString());
        } catch (IOException e) {
            Log.e(TAG, "Error handling stdout file: " + e.toString());
        } finally {
            if (r != null) {
                try {
                    r.close();
                } catch (IOException e) {
                    Log.e(TAG, "Error while closing stdout reader.");
                }
            }
            if (mStdoutFile != null) {
                if (!mStdoutFile.delete()) {
                    Log.e(TAG, "Unable to delete " + mStdoutFile.getAbsolutePath());
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
                        // TODO(jbudorick): Remove this log message once AMP execution and
                        // results handling has been stabilized.
                        Log.d(TAG, "FAILED: " + entry.getKey());
                        ++testsFailed;
                        break;
                    default:
                        Log.w(TAG, "Unhandled: " + entry.getKey() + ", "
                                + entry.getValue().toString());
                        break;
                }
            }

            StringBuilder resultBuilder = new StringBuilder();
            if (testsFailed > 0) {
                resultBuilder.append(
                        "\nFAILURES!!! Tests run: " + Integer.toString(rawResults.size())
                        + ", Failures: " + Integer.toString(testsFailed) + ", Errors: 0");
            } else {
                resultBuilder.append("\nOK (" + Integer.toString(testsPassed) + " tests)");
            }
            resultsBundle.putString(Instrumentation.REPORT_KEY_STREAMRESULT,
                    resultBuilder.toString());
            return resultsBundle;
        }
    }

}
