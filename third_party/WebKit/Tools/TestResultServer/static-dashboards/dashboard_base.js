// Copyright (C) 2012 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//         * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//         * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//         * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Keys in the JSON files.
var FAILURES_BY_TYPE_KEY = 'num_failures_by_type';
var FAILURE_MAP_KEY = 'failure_map';
var CHROME_REVISIONS_KEY = 'chromeRevision';
var BLINK_REVISIONS_KEY = 'blinkRevision';
var TIMESTAMPS_KEY = 'secondsSinceEpoch';
var BUILD_NUMBERS_KEY = 'buildNumbers';
var TESTS_KEY = 'tests';

// Failure types.
var PASS = 'PASS';
var NO_DATA = 'NO DATA';
var SKIP = 'SKIP';
var NOTRUN = 'NOTRUN';

var ONE_DAY_SECONDS = 60 * 60 * 24;
var ONE_WEEK_SECONDS = ONE_DAY_SECONDS * 7;

// These should match the testtype uploaded to test-results.appspot.com.
// See http://test-results.appspot.com/testfile.
var TEST_TYPES = [
    'base_unittests',
    'browser_tests',
    'cacheinvalidation_unittests',
    'compositor_unittests',
    'content_browsertests',
    'content_unittests',
    'courgette_unittests',
    'crypto_unittests',
    'googleurl_unittests',
    'gfx_unittests',
    'gl_tests',
    'gpu_tests',
    'gpu_unittests',
    'installer_util_unittests',
    'interactive_ui_tests',
    'ipc_tests',
    'jingle_unittests',
    'layout-tests',
    'media_unittests',
    'mini_installer_test',
    'net_unittests',
    'printing_unittests',
    'remoting_unittests',
    'safe_browsing_tests',
    'sql_unittests',
    'sync_unit_tests',
    'sync_integration_tests',
    'test_shell_tests',
    'ui_unittests',
    'unit_tests',
    'views_unittests',
    'webkit_unit_tests',
    'androidwebview_instrumentation_tests',
    'chromiumtestshell_instrumentation_tests',
    'contentshell_instrumentation_tests',
    'cc_unittests'
];


// Enum for indexing into the run-length encoded results in the JSON files.
// 0 is where the count is length is stored. 1 is the value.
var RLE = {
    LENGTH: 0,
    VALUE: 1
}

var _NON_FAILURE_TYPES = [PASS, NO_DATA, SKIP, NOTRUN];

function isFailingResult(failureMap, failureType)
{
    return _NON_FAILURE_TYPES.indexOf(failureMap[failureType]) == -1;
}

// Generic utility functions.
function $(id)
{
    return document.getElementById(id);
}

function currentBuilderGroupCategory()
{
    switch (g_history.crossDashboardState.testType) {
    case 'gl_tests':
    case 'gpu_tests':
        return CHROMIUM_GPU_TESTS_BUILDER_GROUPS;
    case 'layout-tests':
        return LAYOUT_TESTS_BUILDER_GROUPS;
    case 'test_shell_tests':
    case 'webkit_unit_tests':
        return TEST_SHELL_TESTS_BUILDER_GROUPS;
    case 'androidwebview_instrumentation_tests':
    case 'chromiumtestshell_instrumentation_tests':
    case 'contentshell_instrumentation_tests':
        return CHROMIUM_INSTRUMENTATION_TESTS_BUILDER_GROUPS;
    case 'cc_unittests':
        return CC_UNITTEST_BUILDER_GROUPS;
    default:
        return CHROMIUM_GTESTS_BUILDER_GROUPS;
    }
}

function currentBuilderGroupName()
{
    return g_history.crossDashboardState.group || Object.keys(currentBuilderGroupCategory())[0];
}

function currentBuilderGroup()
{
    return currentBuilderGroupCategory()[currentBuilderGroupName()];
}

function currentBuilders()
{
    return currentBuilderGroup().builders;
}

function isTipOfTreeWebKitBuilder()
{
    return currentBuilderGroup().isToTWebKit;
}

var g_resultsByBuilder = {};

// Create a new function with some of its arguements
// pre-filled.
// Taken from goog.partial in the Closure library.
// @param {Function} fn A function to partially apply.
// @param {...*} var_args Additional arguments that are partially
//         applied to fn.
// @return {!Function} A partially-applied form of the function bind() was
//         invoked as a method of.
function partial(fn, var_args)
{
    var args = Array.prototype.slice.call(arguments, 1);
    return function() {
        // Prepend the bound arguments to the current arguments.
        var newArgs = Array.prototype.slice.call(arguments);
        newArgs.unshift.apply(newArgs, args);
        return fn.apply(this, newArgs);
    };
};

function getTotalTestCounts(failuresByType)
{
    var countData;
    for (var failureType in failuresByType) {
        var failures = failuresByType[failureType];
        if (countData) {
            failures.forEach(function(count, index) {
                countData.totalTests[index] += count;
                if (failureType != PASS)
                    countData.totalFailingTests[index] += count;
            });
        } else {
            countData = {
                totalTests: failures.slice(),
                totalFailingTests: failures.slice(),
            };
        }
    }
    return countData;
}
