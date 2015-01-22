// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.PowerMonitor;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.library_loader.NativeLibraries;

import java.io.File;

/**
 *  Android's NativeActivity is mostly useful for pure-native code.
 *  Our tests need to go up to our own java classes, which is not possible using
 *  the native activity class loader.
 */
public class ChromeNativeTestActivity extends Activity {
    public static final String EXTRA_COMMAND_LINE_FILE =
            "org.chromium.native_test.ChromeNativeTestActivity.CommandLineFile";
    public static final String EXTRA_COMMAND_LINE_FLAGS =
            "org.chromium.native_test.ChromeNativeTestActivity.CommandLineFlags";
    public static final String EXTRA_STDOUT_FILE =
            "org.chromium.native_test.ChromeNativeTestActivity.StdoutFile";

    private static final String TAG = "ChromeNativeTestActivity";
    private static final String EXTRA_RUN_IN_SUB_THREAD = "RunInSubThread";
    // We post a delayed task to run tests so that we do not block onCreate().
    private static final long RUN_TESTS_DELAY_IN_MS = 300;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        CommandLine.init(new String[]{});

        // Needed by path_utils_unittest.cc
        PathUtils.setPrivateDataDirectorySuffix("chrome");

        ResourceExtractor resourceExtractor = ResourceExtractor.get(getApplicationContext());
        resourceExtractor.setExtractAllPaksAndV8SnapshotForTesting();
        resourceExtractor.startExtractingResources();
        resourceExtractor.waitForCompletion();

        // Needed by system_monitor_unittest.cc
        PowerMonitor.createForTests(this);

        loadLibraries();
        Bundle extras = this.getIntent().getExtras();
        if (extras != null && extras.containsKey(EXTRA_RUN_IN_SUB_THREAD)) {
            // Create a new thread and run tests on it.
            new Thread() {
                @Override
                public void run() {
                    runTests();
                }
            }.start();
        } else {
            // Post a task to run the tests. This allows us to not block
            // onCreate and still run tests on the main thread.
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    runTests();
                }
            }, RUN_TESTS_DELAY_IN_MS);
        }
    }

    private void runTests() {
        String commandLineFlags = getIntent().getStringExtra(EXTRA_COMMAND_LINE_FLAGS);
        if (commandLineFlags == null) commandLineFlags = "";

        String commandLineFilePath = getIntent().getStringExtra(EXTRA_COMMAND_LINE_FILE);
        if (commandLineFilePath == null) {
            commandLineFilePath = "";
        } else {
            File commandLineFile = new File(commandLineFilePath);
            if (!commandLineFile.isAbsolute()) {
                commandLineFilePath = Environment.getExternalStorageDirectory() + "/"
                        + commandLineFilePath;
            }
            Log.i(TAG, "command line file path: " + commandLineFilePath);
        }

        String stdoutFilePath = getIntent().getStringExtra(EXTRA_STDOUT_FILE);
        boolean stdoutFifo = false;
        if (stdoutFilePath == null) {
            stdoutFilePath = new File(getFilesDir(), "test.fifo").getAbsolutePath();
            stdoutFifo = true;
        }

        // This directory is used by build/android/pylib/test_package_apk.py.
        nativeRunTests(commandLineFlags, commandLineFilePath, stdoutFilePath, stdoutFifo,
                getApplicationContext());
        finish();
    }

    // Signal a failure of the native test loader to python scripts
    // which run tests.  For example, we look for
    // RUNNER_FAILED build/android/test_package.py.
    private void nativeTestFailed() {
        Log.e(TAG, "[ RUNNER_FAILED ] could not load native library");
    }

    private void loadLibraries() {
        for (String library : NativeLibraries.LIBRARIES) {
            Log.i(TAG, "loading: " + library);
            System.loadLibrary(library);
            Log.i(TAG, "loaded: " + library);
        }
    }

    private native void nativeRunTests(String commandLineFlags, String commandLineFilePath,
            String stdoutFilePath, boolean stdoutFifo, Context appContext);
}
