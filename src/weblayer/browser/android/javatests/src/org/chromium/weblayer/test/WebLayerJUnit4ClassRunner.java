// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.junit.runners.model.InitializationError;

import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.BaseTestResult.PreTestHook;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.SkipCheck;

import java.util.List;

/**
 * A custom runner for //weblayer JUnit4 tests.
 */
public class WebLayerJUnit4ClassRunner extends BaseJUnit4ClassRunner {
    /**
     * Create a WebLayerJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @throws InitializationError if the test class malformed
     */
    public WebLayerJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass);
    }

    /**
     * Change this static function to add default {@code PreTestHook}s.
     */
    @Override
    protected List<PreTestHook> getPreTestHooks() {
        return addToList(super.getPreTestHooks(), CommandLineFlags.getRegistrationHook());
    }

    @Override
    protected List<SkipCheck> getSkipChecks() {
        return addToList(super.getSkipChecks(), new MinWebLayerVersionSkipCheck());
    }

    @Override
    protected void initCommandLineForTest() {
        CommandLineInitUtil.initCommandLine(CommandLineFlags.getTestCmdLineFile());
    }
}
