// Copyright (C) 2011 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

module('flakiness_dashboard');

// FIXME(jparent): Rename this once it isn't globals.
function resetGlobals()
{
    allExpectations = null;
    g_resultsByBuilder = {};
    g_allTestsTrie = null;
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    g_testToResultsMap = {};

    for (var key in history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES)
        historyInstance.crossDashboardState[key] = history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES[key];

    LOAD_BUILDBOT_DATA({
        'masters': [{
            name: 'ChromiumWebkit',
            url: 'dummyurl',
            tests: {'layout-tests': {'builders': ['WebKit Linux', 'WebKit Linux (dbg)', 'WebKit Linux (deps)', 'WebKit Mac10.7', 'WebKit Win', 'WebKit Win (dbg)']}},
            groups: ['@ToT - chromium.org', '@DEPS - chromium.org'],
        },{
            name :'ChromiumWin',
            url: 'dummyurl2',
            tests: {'interactive_ui_tests': {'builders': ['XP Tests (1)', 'Win7 Tests (1)']}},
            groups: ['@DEPS - chromium.org'],
        }],
    });

    g_currentBuilderGroup = {};

   return historyInstance;
}

var FAILURE_MAP = {"A": "AUDIO", "C": "CRASH", "F": "TEXT", "I": "IMAGE", "O": "MISSING",
    "N": "NO DATA", "P": "PASS", "T": "TIMEOUT", "Y": "NOTRUN", "X": "SKIP", "Z": "IMAGE+TEXT"}

test('substringList', 2, function() {
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    historyInstance.crossDashboardState.testType = 'gtest';
    historyInstance.dashboardSpecificState.tests = 'test.FLAKY_foo test.FAILS_foo1 test.DISABLED_foo2 test.MAYBE_foo3 test.foo4';
    equal(substringList().toString(), 'test.foo,test.foo1,test.foo2,test.foo3,test.foo4');

    historyInstance.crossDashboardState.testType = 'layout-tests';
    historyInstance.dashboardSpecificState.tests = 'foo/bar.FLAKY_foo.html';
    equal(substringList().toString(), 'foo/bar.FLAKY_foo.html');
});

test('headerForTestTableHtml', 1, function() {
    var container = document.createElement('div');
    container.innerHTML = headerForTestTableHtml();
    equal(container.querySelectorAll('input').length, 4);
});

test('htmlForTestTypeSwitcherGroup', 6, function() {
    resetGlobals();
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    var container = document.createElement('div');
    historyInstance.crossDashboardState.testType = 'interactive_ui_tests';
    container.innerHTML = ui.html.testTypeSwitcher(true);
    var selects = container.querySelectorAll('select');
    equal(selects.length, 2);
    var group = selects[1];
    equal(group.parentNode.textContent.indexOf('Group:'), 0);
    equal(group.children.length, 1);

    historyInstance.crossDashboardState.testType = 'layout-tests';
    container.innerHTML = ui.html.testTypeSwitcher(true);
    var selects = container.querySelectorAll('select');
    equal(selects.length, 2);
    var group = selects[1];
    equal(group.parentNode.textContent.indexOf('Group:'), 0);
    equal(group.children.length, 2);
});

test('htmlForIndividualTestOnAllBuilders', 1, function() {
    resetGlobals();
    builders.loadBuildersList('@ToT - chromium.org', 'layout-tests');
    equal(htmlForIndividualTestOnAllBuilders('foo/nonexistant.html'), '<div class="not-found">Test not found. Either it does not exist, is skipped or passes on all platforms.</div>');
});

test('htmlForIndividualTestOnAllBuildersWithResultsLinksNonexistant', 1, function() {
    resetGlobals();
    builders.loadBuildersList('@ToT - chromium.org', 'layout-tests');
    equal(htmlForIndividualTestOnAllBuildersWithResultsLinks('foo/nonexistant.html'),
        '<div class="not-found">Test not found. Either it does not exist, is skipped or passes on all platforms.</div>' +
        '<div class=expectations test=foo/nonexistant.html>' +
            '<div>' +
                '<span class=link onclick="g_history.setQueryParameter(\'showExpectations\', true)">Show results</span> | ' +
                '<span class=link onclick="g_history.setQueryParameter(\'showLargeExpectations\', true)">Show large thumbnails</span> | ' +
                '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b>' +
            '</div>' +
        '</div>');
});

test('htmlForIndividualTestOnAllBuildersWithResultsLinks', 1, function() {
    resetGlobals();
    builders.loadBuildersList('@ToT - chromium.org', 'layout-tests');

    var builderName = 'WebKit Linux';
    g_resultsByBuilder[builderName] = {buildNumbers: [2, 1], blinkRevision: [1234, 1233], failure_map: FAILURE_MAP};

    var test = 'dummytest.html';
    var resultsObject = createResultsObjectForTest(test, builderName);
    resultsObject.rawResults = [[1, 'F']];
    resultsObject.rawTimes = [[1, 0]];
    resultsObject.bugs = ["crbug.com/1234", "webkit.org/5678"];
    g_testToResultsMap[test] = [resultsObject];

    equal(htmlForIndividualTestOnAllBuildersWithResultsLinks(test),
        '<table class=test-table><thead><tr>' +
                '<th sortValue=test><div class=table-header-content><span></span><span class=header-text>test</span></div></th>' +
                '<th sortValue=bugs><div class=table-header-content><span></span><span class=header-text>bugs</span></div></th>' +
                '<th sortValue=expectations><div class=table-header-content><span></span><span class=header-text>expectations</span></div></th>' +
                '<th sortValue=slowest><div class=table-header-content><span></span><span class=header-text>slowest run</span></div></th>' +
                '<th sortValue=flakiness colspan=10000><div class=table-header-content><span></span><span class=header-text>flakiness (numbers are runtimes in seconds)</span></div></th>' +
            '</tr></thead>' +
            '<tbody><tr>' +
                '<td class="test-link"><span class="link" onclick="g_history.setQueryParameter(\'tests\',\'dummytest.html\');">dummytest.html</span>' +
                '<td class=options-container>' +
                    '<div><a href="http://crbug.com/1234">crbug.com/1234</a></div>' +
                    '<div><a href="http://webkit.org/5678">webkit.org/5678</a></div>' +
                '<td class=options-container><td><td title="TEXT. Click for more info." class="results TEXT" onclick=\'showPopupForBuild(event, "WebKit Linux",0,"dummytest.html")\'>&nbsp;' +
                '<td title="NO DATA. Click for more info." class="results NODATA" onclick=\'showPopupForBuild(event, "WebKit Linux",1,"dummytest.html")\'>&nbsp;' +
            '</tbody>' +
        '</table>' +
        '<div>The following builders either don\'t run this test (e.g. it\'s skipped) or all runs passed:</div>' +
        '<div class=skipped-builder-list>' +
            '<div class=skipped-builder>WebKit Linux (dbg)</div><div class=skipped-builder>WebKit Mac10.7</div><div class=skipped-builder>WebKit Win</div><div class=skipped-builder>WebKit Win (dbg)</div>' +
        '</div>' +
        '<div class=expectations test=dummytest.html>' +
            '<div><span class=link onclick="g_history.setQueryParameter(\'showExpectations\', true)">Show results</span> | ' +
            '<span class=link onclick="g_history.setQueryParameter(\'showLargeExpectations\', true)">Show large thumbnails</span> | ' +
            '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b></div>' +
        '</div>');
});

test('htmlForIndividualTests', 4, function() {
    var historyInstance = resetGlobals();
    builders.loadBuildersList('@ToT - chromium.org', 'layout-tests');
    var test1 = 'foo/nonexistant.html';
    var test2 = 'bar/nonexistant.html';

    historyInstance.dashboardSpecificState.showChrome = false;

    var tests = [test1, test2];
    equal(htmlForIndividualTests(tests),
        '<h2><a href="' + TEST_URL_BASE_PATH_FOR_BROWSING + 'foo/nonexistant.html" target="_blank">foo/nonexistant.html</a></h2>' +
        htmlForIndividualTestOnAllBuilders(test1) +
        '<div class=expectations test=foo/nonexistant.html>' +
            '<div><span class=link onclick=\"g_history.setQueryParameter(\'showExpectations\', true)\">Show results</span> | ' +
            '<span class=link onclick=\"g_history.setQueryParameter(\'showLargeExpectations\', true)\">Show large thumbnails</span> | ' +
            '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b></div>' +
        '</div>' +
        '<hr>' +
        '<h2><a href="' + TEST_URL_BASE_PATH_FOR_BROWSING + 'bar/nonexistant.html" target="_blank">bar/nonexistant.html</a></h2>' +
        htmlForIndividualTestOnAllBuilders(test2) +
        '<div class=expectations test=bar/nonexistant.html>' +
            '<div><span class=link onclick=\"g_history.setQueryParameter(\'showExpectations\', true)\">Show results</span> | ' +
            '<span class=link onclick=\"g_history.setQueryParameter(\'showLargeExpectations\', true)\">Show large thumbnails</span> | ' +
            '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b></div>' +
        '</div>');

    tests = [test1];
    equal(htmlForIndividualTests(tests), htmlForIndividualTestOnAllBuilders(test1) +
        '<div class=expectations test=foo/nonexistant.html>' +
            '<div><span class=link onclick=\"g_history.setQueryParameter(\'showExpectations\', true)\">Show results</span> | ' +
            '<span class=link onclick=\"g_history.setQueryParameter(\'showLargeExpectations\', true)\">Show large thumbnails</span> | ' +
            '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b></div>' +
        '</div>');

    historyInstance.dashboardSpecificState.showChrome = true;

    equal(htmlForIndividualTests(tests),
        '<h2><a href="' + TEST_URL_BASE_PATH_FOR_BROWSING + 'foo/nonexistant.html" target="_blank">foo/nonexistant.html</a></h2>' +
        htmlForIndividualTestOnAllBuildersWithResultsLinks(test1));

    tests = [test1, test2];
    equal(htmlForIndividualTests(tests),
        '<h2><a href="' + TEST_URL_BASE_PATH_FOR_BROWSING + 'foo/nonexistant.html" target="_blank">foo/nonexistant.html</a></h2>' +
        htmlForIndividualTestOnAllBuildersWithResultsLinks(test1) + '<hr>' +
        '<h2><a href="' + TEST_URL_BASE_PATH_FOR_BROWSING + 'bar/nonexistant.html" target="_blank">bar/nonexistant.html</a></h2>' +
        htmlForIndividualTestOnAllBuildersWithResultsLinks(test2));
});

test('linkifyBugs', 4, function() {
    equal(linkifyBugs(["crbug.com/1234", "webkit.org/5678"]),
        '<div><a href="http://crbug.com/1234">crbug.com/1234</a></div><div><a href="http://webkit.org/5678">webkit.org/5678</a></div>');
    equal(linkifyBugs(["crbug.com/1234"]), '<div><a href="http://crbug.com/1234">crbug.com/1234</a></div>');
    equal(linkifyBugs(["Bug(nick)"]), '<div>Bug(nick)</div>');
    equal(linkifyBugs([]), '');
});

test('htmlForSingleTestRow', 1, function() {
    var historyInstance = resetGlobals();
    var builder = 'dummyBuilder';
    // BUILDER_TO_MASTER[builder] = CHROMIUM_WEBKIT_BUILDER_MASTER;
    var test = createResultsObjectForTest('foo/exists.html', builder);
    historyInstance.dashboardSpecificState.showNonFlaky = true;
    g_resultsByBuilder[builder] = {buildNumbers: [2, 1], blinkRevision: [1234, 1233], failure_map: FAILURE_MAP};
    test.rawResults = [[1, 'F'], [2, 'I']];
    test.rawTimes = [[1, 0], [2, 5]];
    var expected = '<tr>' +
        '<td class="test-link"><span class="link" onclick="g_history.setQueryParameter(\'tests\',\'foo/exists.html\');">foo/exists.html</span>' +
        '<td class=options-container><a href="https://code.google.com/p/chromium/issues/entry?template=Layout%20Test%20Failure&summary=Layout%20Test%20foo%2Fexists.html%20is%20failing&comment=The%20following%20layout%20test%20is%20failing%20on%20%5Binsert%20platform%5D%0A%0Afoo%2Fexists.html%0A%0AProbable%20cause%3A%0A%0A%5Binsert%20probable%20cause%5D">File new bug</a>' +
        '<td class=options-container>' +
        '<td>' +
        '<td title="TEXT. Click for more info." class="results TEXT" onclick=\'showPopupForBuild(event, "dummyBuilder",0,"foo/exists.html")\'>&nbsp;' +
        '<td title="IMAGE. Click for more info." class="results IMAGE" onclick=\'showPopupForBuild(event, "dummyBuilder",1,"foo/exists.html")\'>5';
    equal(htmlForSingleTestRow(test), expected);
});

test('lookupVirtualTestSuite', 2, function() {
    equal(lookupVirtualTestSuite('fast/canvas/foo.html'), '');
    equal(lookupVirtualTestSuite('virtual/gpu/fast/canvas/foo.html'), 'virtual/gpu/fast/canvas');
});

test('baseTest', 2, function() {
    equal(baseTest('fast/canvas/foo.html', ''), 'fast/canvas/foo.html');
    equal(baseTest('virtual/gpu/fast/canvas/foo.html', 'virtual/gpu/fast/canvas'), 'fast/canvas/foo.html');
});

// FIXME: Create builders_tests.js and move this there.

test('isChromiumWebkitTipOfTreeTestRunner', 1, function() {
    var builderList = ["WebKit Linux", "WebKit Linux (dbg)", "WebKit Linux 32", "WebKit Mac10.6", "WebKit Mac10.6 (dbg)",
        "WebKit Mac10.6 (deps)", "WebKit Mac10.7", "WebKit Win", "WebKit Win (dbg)(1)", "WebKit Win (dbg)(2)", "WebKit Win (deps)",
        "WebKit Win7", "Linux (Content Shell)"];
    var expectedBuilders = ["WebKit Linux", "WebKit Linux (dbg)", "WebKit Linux 32", "WebKit Mac10.6",
        "WebKit Mac10.6 (dbg)", "WebKit Mac10.7", "WebKit Win", "WebKit Win (dbg)(1)", "WebKit Win (dbg)(2)", "WebKit Win7"];
    deepEqual(builderList.filter(isChromiumWebkitTipOfTreeTestRunner), expectedBuilders);
});

test('isChromiumWebkitDepsTestRunner', 1, function() {
    var builderList = ["Chrome Frame Tests", "GPU Linux (NVIDIA)", "GPU Linux (dbg) (NVIDIA)", "GPU Mac", "GPU Mac (dbg)", "GPU Win7 (NVIDIA)", "GPU Win7 (dbg) (NVIDIA)", "Linux Perf", "Linux Tests",
        "Linux Valgrind", "Mac Builder (dbg)", "Mac10.6 Perf", "Mac10.6 Tests", "Vista Perf", "Vista Tests", "WebKit Linux", "WebKit Linux ASAN",  "WebKit Linux (dbg)", "WebKit Linux (deps)", "WebKit Linux 32",
        "WebKit Mac10.6", "WebKit Mac10.6 (dbg)", "WebKit Mac10.6 (deps)", "WebKit Mac10.7", "WebKit Win", "WebKit Win (dbg)(1)", "WebKit Win (dbg)(2)", "WebKit Win (deps)",
        "WebKit Win7", "Win (dbg)", "Win Builder"];
    var expectedBuilders = ["WebKit Linux (deps)", "WebKit Mac10.6 (deps)", "WebKit Win (deps)"];
    deepEqual(builderList.filter(isChromiumWebkitDepsTestRunner), expectedBuilders);
});

test('builderGroupIsToTWebKitAttribute', 2, function() {
    resetGlobals();
    builders.loadBuildersList('@ToT - chromium.org', 'layout-tests');
    equal(builders.getBuilderGroup().isToTBlink, true);
    builders.loadBuildersList('@DEPS - chromium.org', 'layout-tests');
    equal(builders.getBuilderGroup().isToTBlink, false);
});

test('sortTests', 4, function() {
    var test1 = createResultsObjectForTest('foo/test1.html', 'dummyBuilder');
    var test2 = createResultsObjectForTest('foo/test2.html', 'dummyBuilder');
    var test3 = createResultsObjectForTest('foo/test3.html', 'dummyBuilder');
    test1.expectations = 'b';
    test2.expectations = 'a';
    test3.expectations = '';

    var tests = [test1, test2, test3];
    sortTests(tests, 'expectations', FORWARD);
    deepEqual(tests, [test2, test1, test3]);
    sortTests(tests, 'expectations', BACKWARD);
    deepEqual(tests, [test3, test1, test2]);

    test1.bugs = 'b';
    test2.bugs = 'a';
    test3.bugs = '';

    var tests = [test1, test2, test3];
    sortTests(tests, 'bugs', FORWARD);
    deepEqual(tests, [test2, test1, test3]);
    sortTests(tests, 'bugs', BACKWARD);
    deepEqual(tests, [test3, test1, test2]);
});

test('popup', 2, function() {
    ui.popup.show(document.body, 'dummy content');
    ok(document.querySelector('#popup'));
    ui.popup.hide();
    ok(!document.querySelector('#popup'));
});

test('gpuResultsPath', 3, function() {
  equal(gpuResultsPath('777777', 'Win7 Release (ATI)'), '777777_Win7_Release_ATI_');
  equal(gpuResultsPath('123', 'GPU Linux (dbg)(NVIDIA)'), '123_GPU_Linux_dbg_NVIDIA_');
  equal(gpuResultsPath('12345', 'GPU Mac'), '12345_GPU_Mac');
});

test('TestTrie', 3, function() {
    var builders = {
        "Dummy Chromium Windows Builder": true,
        "Dummy GTK Linux Builder": true,
        "Dummy Apple Mac Lion Builder": true
    };

    var resultsByBuilder = {
        "Dummy Chromium Windows Builder": {
            tests: {
                "foo": true,
                "foo/bar/1.html": true,
                "foo/bar/baz": true
            }
        },
        "Dummy GTK Linux Builder": {
            tests: {
                "bar": true,
                "foo/1.html": true,
                "foo/bar/2.html": true,
                "foo/bar/baz/1.html": true,
            }
        },
        "Dummy Apple Mac Lion Builder": {
            tests: {
                "foo/bar/3.html": true,
                "foo/bar/baz/foo": true,
            }
        }
    };
    var expectedTrie = {
        "foo": {
            "bar": {
                "1.html": true,
                "2.html": true,
                "3.html": true,
                "baz": {
                    "1.html": true,
                    "foo": true
                }
            },
            "1.html": true
        },
        "bar": true
    }

    var trie = new TestTrie(builders, resultsByBuilder);
    deepEqual(trie._trie, expectedTrie);

    var leafsOfCompleteTrieTraversal = [];
    var expectedLeafs = ["foo/bar/1.html", "foo/bar/baz/1.html", "foo/bar/baz/foo", "foo/bar/2.html", "foo/bar/3.html", "foo/1.html", "bar"];
    trie.forEach(function(triePath) {
        leafsOfCompleteTrieTraversal.push(triePath);
    });
    deepEqual(leafsOfCompleteTrieTraversal, expectedLeafs);

    var leafsOfPartialTrieTraversal = [];
    expectedLeafs = ["foo/bar/1.html", "foo/bar/baz/1.html", "foo/bar/baz/foo", "foo/bar/2.html", "foo/bar/3.html"];
    trie.forEach(function(triePath) {
        leafsOfPartialTrieTraversal.push(triePath);
    }, "foo/bar");
    deepEqual(leafsOfPartialTrieTraversal, expectedLeafs);
});

test('changeTestTypeInvalidatesGroup', 1, function() {
    var historyInstance = resetGlobals();
    var originalGroup = '@ToT - chromium.org';
    var originalTestType = 'layout-tests';
    builders.loadBuildersList(originalGroup, originalTestType);
    historyInstance.crossDashboardState.group = originalGroup;
    historyInstance.crossDashboardState.testType = originalTestType;

    historyInstance.invalidateQueryParameters({'testType': 'ui_tests'});
    notEqual(historyInstance.crossDashboardState.group, originalGroup, "group should have been invalidated");
});

test('shouldShowTest', 9, function() {
    var historyInstance = new history.History(flakinessConfig);
    historyInstance.parseParameters();
    // FIXME(jparent): Change to use the flakiness_dashboard's history object
    // once it exists, rather than tracking global.
    g_history = historyInstance;
    var test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');

    equal(shouldShowTest(test), false, 'default layout test, hide it.');
    historyInstance.dashboardSpecificState.showNonFlaky = true;
    equal(shouldShowTest(test), true, 'show correct expectations.');
    historyInstance.dashboardSpecificState.showNonFlaky = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    test.expectations = "WONTFIX";
    equal(shouldShowTest(test), false, 'by default hide wontfix');
    historyInstance.dashboardSpecificState.showWontFix = true;
    equal(shouldShowTest(test), true, 'show wontfix');
    historyInstance.dashboardSpecificState.showWontFix = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    test.expectations = "SKIP";
    equal(shouldShowTest(test), false, 'we hide skip tests by default');
    historyInstance.dashboardSpecificState.showSkip = true;
    equal(shouldShowTest(test), true, 'show skip test');
    historyInstance.dashboardSpecificState.showSkip = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    test.isFlaky = true;
    equal(shouldShowTest(test), false, 'hide flaky tests by default');
    historyInstance.dashboardSpecificState.showFlaky = true;
    equal(shouldShowTest(test), true, 'show flaky test');
    historyInstance.dashboardSpecificState.showFlaky = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    historyInstance.crossDashboardState.testType = 'not layout tests';
    equal(shouldShowTest(test), true, 'show all non layout tests');
});

test('testLoadBuildersList', 4, function() {
    resetGlobals();

    builders.loadBuildersList('@ToT - chromium.org', 'layout-tests');
    var expectedBuilder = 'WebKit Win';
    equal(expectedBuilder in builders.getBuilderGroup().builders, true, expectedBuilder + ' should be among current builders');

    builders.loadBuildersList('@DEPS - chromium.org', 'layout-tests');
    expectedBuilder = 'WebKit Linux (deps)'
    equal(expectedBuilder in builders.getBuilderGroup().builders, true, expectedBuilder + ' should be among current builders');
    expectedBuilder = 'XP Tests (1)'
    equal(expectedBuilder in builders.getBuilderGroup().builders, false, expectedBuilder + ' should not be among current builders');

    builders.loadBuildersList('@DEPS - chromium.org', 'interactive_ui_tests');
    equal(expectedBuilder in builders.getBuilderGroup().builders, true, expectedBuilder + ' should be among current builders');
});

test('testSelectBuilderFilter', 7, function() {
    var filter = selectBuilderFilter('@ToT - chromium.org', 'layout-tests');
    equal(filter('WebKit (Content Shell) Linux'), false, 'don\'t show content shell builders');
    equal(filter('WebKit Linux'), true, 'show generic webkit builder');
    equal(filter('Android Tests (dbg) '), false, 'don\'t show android tests');

    var filter = selectBuilderFilter('Content Shell @ToT - chromium.org', 'layout-tests');
    equal(filter('WebKit (Content Shell) Linux'), true, 'show content shell builder');
    equal(filter('WebKit Linux'), false, 'don\'t show non-content shell builder');

    var filter = selectBuilderFilter('@DEPS - chromium.org', 'webkit_unit_tests');
    equal(filter('WebKit Win7 (deps)'), true, 'show DEPS builder');
    equal(filter('WebKit Win7'), false, 'don\'t show non-deps builder');
});

test('testGroupNamesForTestType', 4, function() {
    var names = groupNamesForTestType('layout-tests');
    equal(names.indexOf('@ToT - chromium.org') != -1, true, 'include layout-tests in ToT');
    equal(names.indexOf('@DEPS - chromium.org') != -1, true, 'include layout-tests in DEPS');

    names = groupNamesForTestType('interactive_ui_tests');
    equal(names.indexOf('@ToT - chromium.org') != -1, false, 'don\'t include interactive_ui_tests in ToT');
    equal(names.indexOf('@DEPS - chromium.org') != -1, true, 'include interactive_ui_tests in DEPS');
});

test('determineFlakiness', 10, function() {
    var failureMap = {
        'C': 'CRASH',
        'P': 'PASS',
        'I': 'IMAGE',
        'T': 'TIMEOUT',
        'N':'NO DATA',
        'Y':'NOTRUN',
        'X':'SKIP'
    };
    var out = {};

    var inputResults = [[1, 'P']];
    determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, false);
    equal(out.flipCount, 0);

    inputResults = [[1, 'P'], [1, 'C']];
    determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, false);
    equal(out.flipCount, 1);

    inputResults = [[1, 'P'], [1, 'C'], [1, 'P']];
    determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, true);
    equal(out.flipCount, 2);

    inputResults = [[1, 'P'], [1, 'C'], [1, 'P'], [1, 'C']];
    determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, true);
    equal(out.flipCount, 3);

    inputResults = [[1, 'P'], [1, 'Y'], [1, 'N'], [1, 'X'], [1, 'P'], [1, 'Y'], [1, 'N'], [1, 'X'], [1, 'C']];
    determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, false);
    equal(out.flipCount, 1);
});
