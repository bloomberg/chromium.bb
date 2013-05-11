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

(function () {

module("model");

var kExampleCommitDataXML =
    '<?xml version="1.0"?>\n' +
    '<log>\n' +
    '<logentry\n' +
    '   revision="147744">\n' +
    '<author>tkent@chromium.org</author>\n' +
    '<date>2013-04-06T13:00:08.314281Z</date>\n' +
    '<msg>Revert 147740 "Remove the ENABLE_QUOTA compile-time flag."\n' +
    '\n' +
    '&gt; Remove the ENABLE_QUOTA compile-time flag.\n' +
    '&gt; \n' +
    '&gt; This patch drops the ~20 occurances of \'#IFDEF ENABLE(QUOTA)\' from the\n' +
    '&gt; codebase. It shouldn\'t effect any web-visible behavior, as the interesting\n' +
    '&gt; bits are still hidden away behind the runtime flag, whose behavior\n' +
    '&gt; is untouched by this patch.\n' +
    '&gt; \n' +
    '&gt; R=eseidel@chromium.org,kinuko@chromium.org\n' +
    '&gt; \n' +
    '&gt; Review URL: https://codereview.chromium.org/13529033\n' +
    '\n' +
    'TBR=mkwst@chromium.org\n' +
    'Review URL: https://codereview.chromium.org/13462004</msg>\n' +
    '</logentry>\n' +
    '<logentry\n' +
    '   revision="147743">\n' +
    '<author>tkent@chromium.org</author>\n' +
    '<date>2013-04-06T12:48:59.078499Z</date>\n' +
    '<msg>Update test expectations.\n' +
    '\n' +
    'BUG=227354,227357</msg>\n' +
    '</logentry>\n' +
    '<logentry\n' +
    '   revision="147742">\n' +
    '<author>gavinp@chromium.org</author>\n' +
    '<date>2013-04-06T12:40:34.111299Z</date>\n' +
    '<msg>Guard &lt;link&gt; beforeload against recursion.\n' +
    '\n' +
    'The beforeload event on a link element can recurse if it mutates its\n' +
    'firing link element. Now guard against this, only allowing the\n' +
    'innermost (last!) change to run. This prevents multiple client\n' +
    'registration and stops the crash in the bug report (which is\n' +
    'reproduced in the test).\n' +
    '\n' +
    'BUG=174920\n' +
    '\n' +
    'Committed: https://src.chromium.org/viewvc/blink?view=rev&amp;revision=147738\n' +
    '\n' +
    'Review URL: https://codereview.chromium.org/13725004</msg>\n' +
    '</logentry>\n' +
    '</log>';

test("rebaselineQueue", 3, function() {
    var queue = model.takeRebaselineQueue();
    deepEqual(queue, []);
    model.queueForRebaseline('failureInfo1');
    model.queueForRebaseline('failureInfo2');
    var queue = model.takeRebaselineQueue();
    deepEqual(queue, ['failureInfo1', 'failureInfo2']);
    var queue = model.takeRebaselineQueue();
    deepEqual(queue, []);
});

test("rebaselineQueue", 3, function() {
    var queue = model.takeExpectationUpdateQueue();
    deepEqual(queue, []);
    model.queueForExpectationUpdate('failureInfo1');
    model.queueForExpectationUpdate('failureInfo2');
    var queue = model.takeExpectationUpdateQueue();
    deepEqual(queue, ['failureInfo1', 'failureInfo2']);
    var queue = model.takeExpectationUpdateQueue();
    deepEqual(queue, []);
});

test("updateRecentCommits", 2, function() {
    var simulator = new NetworkSimulator();

    simulator.get = function(url, callback)
    {
        simulator.scheduleCallback(function() {
            var parser = new DOMParser();
            var responseDOM = parser.parseFromString(kExampleCommitDataXML, "application/xml");
            callback(responseDOM);
        });
    };

    simulator.runTest(function() {
        model.updateRecentCommits(function() {
            var recentCommits = model.state.recentCommits;
            delete model.state.recentCommits;
            $.each(recentCommits, function(index, commitData) {
                delete commitData.message;
            });
            deepEqual(recentCommits, [{
                'revision': '147744',
                'title': 'Revert 147740 "Remove the ENABLE_QUOTA compile-time flag."',
                'time': '2013-04-06T13:00:08.314281Z',
                'summary': 'Revert 147740 "Remove the ENABLE_QUOTA compile-time flag."',
                'author': 'tkent@chromium.org',
                'reviewer': 'eseidel@chromium',
                'bugID': 0,
                'revertedRevision': undefined
            },
            {
                'revision': '147743',
                'title': 'Update test expectations.',
                'time': '2013-04-06T12:48:59.078499Z',
                'summary': 'Update test expectations.',
                'author': 'tkent@chromium.org',
                'reviewer': null,
                'bugID': 227354,
                'revertedRevision': undefined
              },
            {
                'revision': '147742',
                'title': 'Guard <link> beforeload against recursion.',
                'time': '2013-04-06T12:40:34.111299Z',
                'summary': 'Guard <link> beforeload against recursion.',
                'author': 'gavinp@chromium.org',
                'reviewer': null,
                'bugID': 174920,
                'revertedRevision': undefined
            }]);
        });
    });
});

test("commitDataListForRevisionRange", 6, function() {
    var simulator = new NetworkSimulator();

    simulator.get = function(url, callback)
    {
        simulator.scheduleCallback(function() {
            var parser = new DOMParser();
            var responseDOM = parser.parseFromString(kExampleCommitDataXML, "application/xml");
            callback(responseDOM);
        });
    };

    simulator.runTest(function() {
        model.updateRecentCommits(function() {
            function extractBugIDs(commitData)
            {
                return commitData.bugID;
            }

            deepEqual(model.commitDataListForRevisionRange(147742, 147742).map(extractBugIDs), [174920]);
            deepEqual(model.commitDataListForRevisionRange(147740, 147742).map(extractBugIDs), [174920]);
            deepEqual(model.commitDataListForRevisionRange(147739, 147740).map(extractBugIDs), []);
            deepEqual(model.commitDataListForRevisionRange(0, 147742).map(extractBugIDs), [174920]);
            deepEqual(model.commitDataListForRevisionRange(147743, 0).map(extractBugIDs), []);
            delete model.state.recentCommits;
        });
    });
});

test("buildersInFlightForRevision", 3, function() {
    var unmock = model.state.resultsByBuilder;
    model.state.resultsByBuilder = {
        'Mr. Beasley': {blink_revision: '5'},
        'Mr Dixon': {blink_revision: '1'},
        'Mr. Sabatini': {blink_revision: '4'},
        'Bob': {blink_revision: '6'}
    };
    deepEqual(model.buildersInFlightForRevision(1), {});
    deepEqual(model.buildersInFlightForRevision(3), {
      "Mr Dixon": {
        "actual": "BUILDING"
      }
    });
    deepEqual(model.buildersInFlightForRevision(10), {
      "Mr. Beasley": {
        "actual": "BUILDING"
      },
      "Mr Dixon": {
        "actual": "BUILDING"
      },
      "Mr. Sabatini": {
        "actual": "BUILDING"
      },
      "Bob": {
        "actual": "BUILDING"
      }
    });
    model.state.resultsByBuilder = unmock;
});

test("latestRevisionWithNoBuildersInFlight", 1, function() {
    var unmock = model.state.resultsByBuilder;
    model.state.resultsByBuilder = {
        'Mr. Beasley': { },
        'Mr Dixon': {blink_revision: '2'},
        'Mr. Sabatini': {blink_revision: '4'},
        'Bob': {blink_revision: '6'}
    };
    equals(model.latestRevisionWithNoBuildersInFlight(), 2);
    model.state.resultsByBuilder = unmock;
});

})();
