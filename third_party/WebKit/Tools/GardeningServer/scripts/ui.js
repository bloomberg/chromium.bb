/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var ui = ui || {};

(function () {

// FIXME: Put this all in a more appropriate place.

// FIXME: Replace the flakiness dashboard's sense of groups with sheriff-o-matic's
// sense of trees and get rid of this mapping.
var treeToDashboardGroup = {
    blink: '@ToT%20Blink',
    chromium: '@ToT%20Chromium',
};

ui.displayNameForBuilder = function(builderName)
{
    return builderName.replace(/Webkit /i, '');
}

// FIXME: Take a master name argument as well.
ui.urlForFlakinessDashboard = function(testNames, testType, tree)
{
    if (Array.isArray(testNames))
        testNames = testNames.join(',');

    // FIXME: Remove this once the flakiness dashboard stops having webkit_tests
    // masquerade as layout-tests.
    if (testType == 'webkit_tests')
        testType = 'layout-tests';

    // FIXME: sugarjs's toQueryString makes spaces into pluses instead of %20, which confuses
    // the flakiness dashboard, which just uses decodeURIComponent.
    return 'http://test-results.appspot.com/dashboards/flakiness_dashboard.html#group=' +
        treeToDashboardGroup[tree] + '&' +
        Object.toQueryString({
            tests: testNames,
            testType: testType,
        });
}

ui.urlForEmbeddedFlakinessDashboard = function(testNames, testType, tree)
{
    return ui.urlForFlakinessDashboard(testNames, testType, tree) + '&showChrome=false';
}

})();
