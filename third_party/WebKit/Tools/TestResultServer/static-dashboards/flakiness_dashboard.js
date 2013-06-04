// Copyright (C) 2012 Google Inc. All rights reserved.
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

//////////////////////////////////////////////////////////////////////////////
// CONSTANTS
//////////////////////////////////////////////////////////////////////////////
var FORWARD = 'forward';
var BACKWARD = 'backward';
var GTEST_MODIFIERS = ['FLAKY', 'FAILS', 'MAYBE', 'DISABLED'];
var TEST_URL_BASE_PATH_FOR_BROWSING = 'http://src.chromium.org/viewvc/blink/trunk/LayoutTests/';
var TEST_URL_BASE_PATH_FOR_XHR = 'http://src.chromium.org/blink/trunk/LayoutTests/';
var TEST_RESULTS_BASE_PATH = 'http://build.chromium.org/f/chromium/layout_test_results/';
var GPU_RESULTS_BASE_PATH = 'http://chromium-browser-gpu-tests.commondatastorage.googleapis.com/runs/'

var RELEASE_TIMEOUT = 6;
var DEBUG_TIMEOUT = 12;
var SLOW_MULTIPLIER = 5;
var CHUNK_SIZE = 25;

// FIXME: Figure out how to make this not be hard-coded.
var VIRTUAL_SUITES = {
    'virtual/gpu/fast/canvas': 'fast/canvas',
    'virtual/gpu/canvas/philip': 'canvas/philip'
};

var resourceLoader;

function generatePage(historyInstance)
{
    if (historyInstance.crossDashboardState.useTestData)
        return;

    document.body.innerHTML = '<div id="loading-ui">LOADING...</div>';
    resourceLoader.showErrors();

    // tests expands to all tests that match the CSV list.
    // result expands to all tests that ever have the given result
    if (historyInstance.dashboardSpecificState.tests || historyInstance.dashboardSpecificState.result)
        generatePageForIndividualTests(individualTests());
    else
        generatePageForBuilder(historyInstance.dashboardSpecificState.builder || currentBuilderGroup().defaultBuilder());

    for (var builder in currentBuilders())
        processTestResultsForBuilderAsync(builder);

    postHeightChangedMessage();
}

function handleValidHashParameter(historyInstance, key, value)
{
    switch(key) {
    case 'result':
    case 'tests':
        history.validateParameter(historyInstance.dashboardSpecificState, key, value,
            function() {
                return string.isValidName(value);
            });
        return true;

    case 'builder':
        history.validateParameter(historyInstance.dashboardSpecificState, key, value,
            function() {
                return value in currentBuilders();
            });

        return true;

    case 'sortColumn':
        history.validateParameter(historyInstance.dashboardSpecificState, key, value,
            function() {
                // Get all possible headers since the actual used set of headers
                // depends on the values in historyInstance.dashboardSpecificState, which are currently being set.
                var headers = tableHeaders(true);
                for (var i = 0; i < headers.length; i++) {
                    if (value == sortColumnFromTableHeader(headers[i]))
                        return true;
                }
                return value == 'test' || value == 'builder';
            });
        return true;

    case 'sortOrder':
        history.validateParameter(historyInstance.dashboardSpecificState, key, value,
            function() {
                return value == FORWARD || value == BACKWARD;
            });
        return true;

    case 'resultsHeight':
    case 'revision':
        history.validateParameter(historyInstance.dashboardSpecificState, key, Number(value),
            function() {
                return value.match(/^\d+$/);
            });
        return true;

    case 'showChrome':
    case 'showExpectations':
    case 'showFlaky':
    case 'showLargeExpectations':
    case 'showNonFlaky':
    case 'showSlow':
    case 'showSkip':
    case 'showUnexpectedPasses':
    case 'showWontFix':
        historyInstance.dashboardSpecificState[key] = value == 'true';
        return true;

    default:
        return false;
    }
}

// @param {Object} params New or modified query parameters as key: value.
function handleQueryParameterChange(historyInstance, params)
{
    for (key in params) {
        if (key == 'tests') {
            // Entering cross-builder view, only keep valid keys for that view.
            for (var currentKey in historyInstance.dashboardSpecificState) {
              if (isInvalidKeyForCrossBuilderView(currentKey)) {
                delete historyInstance.dashboardSpecificState[currentKey];
              }
            }
        } else if (isInvalidKeyForCrossBuilderView(key)) {
            delete historyInstance.dashboardSpecificState.tests;
            delete historyInstance.dashboardSpecificState.result;
        }
    }

    return true;
}

var defaultDashboardSpecificStateValues = {
    sortOrder: BACKWARD,
    sortColumn: 'flakiness',
    showExpectations: false,
    // FIXME: Show flaky tests by default if you have a builder picked.
    // Ideally, we'd fix the dashboard to not pick a default builder and have
    // you pick one. In the interim, this is a good way to make the default
    // page load faster since we don't need to generate/layout a large table.
    showFlaky: false,
    showLargeExpectations: false,
    showChrome: true,
    showWontFix: false,
    showNonFlaky: false,
    showSkip: false,
    showUnexpectedPasses: false,
    resultsHeight: 300,
    revision: null,
    tests: '',
    result: '',
    builder: null
};

var DB_SPECIFIC_INVALIDATING_PARAMETERS = {
    'tests' : 'builder',
    'testType': 'builder',
    'group': 'builder'
};

var flakinessConfig = {
    defaultStateValues: defaultDashboardSpecificStateValues,
    generatePage: generatePage,
    handleValidHashParameter: handleValidHashParameter,
    handleQueryParameterChange: handleQueryParameterChange,
    invalidatingHashParameters: DB_SPECIFIC_INVALIDATING_PARAMETERS
};

// FIXME(jparent): Eventually remove all usage of global history object.
var g_history = new history.History(flakinessConfig);
g_history.parseCrossDashboardParameters();

//////////////////////////////////////////////////////////////////////////////
// GLOBALS
//////////////////////////////////////////////////////////////////////////////

var g_perBuilderFailures = {};
// Maps test path to an array of {builder, testResults} objects.
var g_testToResultsMap = {};

function createResultsObjectForTest(test, builder)
{
    return {
        test: test,
        builder: builder,
        // HTML for display of the results in the flakiness column
        html: '',
        flips: 0,
        slowestTime: 0,
        isFlaky: false,
        bugs: [],
        expectations : '',
        rawResults: '',
        // List of all the results the test actually has.
        actualResults: []
    };
}

var TestTrie = function(builders, resultsByBuilder)
{
    this._trie = {};

    for (var builder in builders) {
        if (!resultsByBuilder[builder]) {
            console.warn("No results for builder: ", builder)
            continue;
        }
        var testsForBuilder = resultsByBuilder[builder].tests;
        for (var test in testsForBuilder)
            this._addTest(test.split('/'), this._trie);
    }
}

TestTrie.prototype.forEach = function(callback, startingTriePath)
{
    var testsTrie = this._trie;
    if (startingTriePath) {
        var splitPath = startingTriePath.split('/');
        while (splitPath.length && testsTrie)
            testsTrie = testsTrie[splitPath.shift()];
    }

    if (!testsTrie)
        return;

    function traverse(trie, triePath) {
        if (trie == true)
            callback(triePath);
        else {
            for (var member in trie)
                traverse(trie[member], triePath ? triePath + '/' + member : member);
        }
    }
    traverse(testsTrie, startingTriePath);
}

TestTrie.prototype._addTest = function(test, trie)
{
    var rootComponent = test.shift();
    if (!test.length) {
        if (!trie[rootComponent])
            trie[rootComponent] = true;
        return;
    }

    if (!trie[rootComponent] || trie[rootComponent] == true)
        trie[rootComponent] = {};
    this._addTest(test, trie[rootComponent]);
}

// Map of all tests to true values. This is just so we can have the list of
// all tests across all the builders.
var g_allTestsTrie;

function getAllTestsTrie()
{
    if (!g_allTestsTrie)
        g_allTestsTrie = new TestTrie(currentBuilders(), g_resultsByBuilder);

    return g_allTestsTrie;
}

// Returns an array of tests to be displayed in the individual tests view.
// Note that a directory can be listed as a test, so we expand that into all
// tests in the directory.
function individualTests()
{
    if (g_history.dashboardSpecificState.result)
        return allTestsWithResult(g_history.dashboardSpecificState.result);

    if (!g_history.dashboardSpecificState.tests)
        return [];

    return individualTestsForSubstringList();
}

function substringList()
{
    // Convert windows slashes to unix slashes.
    var tests = g_history.dashboardSpecificState.tests.replace(/\\/g, '/');
    var separator = string.contains(tests, ' ') ? ' ' : ',';
    var testList = tests.split(separator);

    if (g_history.isLayoutTestResults())
        return testList;

    var testListWithoutModifiers = [];
    testList.forEach(function(path) {
        GTEST_MODIFIERS.forEach(function(modifier) {
            path = path.replace('.' + modifier + '_', '.');
        });
        testListWithoutModifiers.push(path);
    });
    return testListWithoutModifiers;
}

function individualTestsForSubstringList()
{
    var testList = substringList();

    // Put the tests into an object first and then move them into an array
    // as a way of deduping.
    var testsMap = {};
    for (var i = 0; i < testList.length; i++) {
        var path = testList[i];

        // Ignore whitespace entries as they'd match every test.
        if (path.match(/^\s*$/))
            continue;

        var hasAnyMatches = false;
        getAllTestsTrie().forEach(function(triePath) {
            if (string.caseInsensitiveContains(triePath, path)) {
                testsMap[triePath] = 1;
                hasAnyMatches = true;
            }
        });

        // If a path doesn't match any tests, then assume it's a full path
        // to a test that passes on all builders.
        if (!hasAnyMatches)
            testsMap[path] = 1;
    }

    var testsArray = [];
    for (var test in testsMap)
        testsArray.push(test);
    return testsArray;
}

function allTestsWithResult(result)
{
    processTestRunsForAllBuilders();
    var retVal = [];

    getAllTestsTrie().forEach(function(triePath) {
        for (var i = 0; i < g_testToResultsMap[triePath].length; i++) {
            if (g_testToResultsMap[triePath][i].actualResults.indexOf(result.toUpperCase()) != -1) {
                retVal.push(triePath);
                break;
            }
        }
    });

    return retVal;
}

function processTestResultsForBuilderAsync(builder)
{
    setTimeout(function() { processTestRunsForBuilder(builder); }, 0);
}

function processTestRunsForAllBuilders()
{
    for (var builder in currentBuilders())
        processTestRunsForBuilder(builder);
}

function processTestRunsForBuilder(builderName)
{
    if (g_perBuilderFailures[builderName])
      return;

    if (!g_resultsByBuilder[builderName]) {
        console.error('No tests found for ' + builderName);
        g_perBuilderFailures[builderName] = [];
        return;
    }

    var failures = [];
    var allTestsForThisBuilder = g_resultsByBuilder[builderName].tests;

    for (var test in allTestsForThisBuilder) {
        var resultsForTest = createResultsObjectForTest(test, builderName);

        var rawTest = g_resultsByBuilder[builderName].tests[test];
        resultsForTest.rawTimes = rawTest.times;
        var rawResults = rawTest.results;
        resultsForTest.rawResults = rawResults;

        if (rawTest.expected)
            resultsForTest.expectations = rawTest.expected;

        if (rawTest.bugs)
            resultsForTest.bugs = rawTest.bugs;

        var failureMap = g_resultsByBuilder[builderName][FAILURE_MAP_KEY];
        // FIXME: Switch to resultsByBuild
        var times = resultsForTest.rawTimes;
        var numTimesSeen = 0;
        var numResultsSeen = 0;
        var resultsIndex = 0;
        var resultsMap = {}

        for (var i = 0; i < times.length; i++) {
            numTimesSeen += times[i][RLE.LENGTH];

            while (rawResults[resultsIndex] && numTimesSeen > (numResultsSeen + rawResults[resultsIndex][RLE.LENGTH])) {
                numResultsSeen += rawResults[resultsIndex][RLE.LENGTH];
                resultsIndex++;
            }

            if (rawResults && rawResults[resultsIndex]) {
                var result = rawResults[resultsIndex][RLE.VALUE];
                resultsMap[failureMap[result]] = true;
            }

            resultsForTest.slowestTime = Math.max(resultsForTest.slowestTime, times[i][RLE.VALUE]);
        }

        resultsForTest.actualResults = Object.keys(resultsMap);

        determineFlakiness(failureMap, rawResults, resultsForTest);
        failures.push(resultsForTest);

        if (!g_testToResultsMap[test])
            g_testToResultsMap[test] = [];
        g_testToResultsMap[test].push(resultsForTest);
    }

    g_perBuilderFailures[builderName] = failures;
}

function determineFlakiness(failureMap, results, resultsForTest)
{
    // FIXME: Ideally this heuristic would be a bit smarter and not consider
    // all passes, followed by a few consecutive failures, followed by all passes
    // to be flakiness since that's more likely the test actually failing for a
    // few runs due to a commit.
    var FAILURE_TYPES_TO_IGNORE = ['NOTRUN', 'NO DATA', 'SKIP'];
    var flipCount = 0;
    var mostRecentNonIgnorableFailureType;

    for (var i = 0; i < results.length; i++) {
        var result = results[i][RLE.VALUE];
        var failureType = failureMap[result];
        if (failureType != mostRecentNonIgnorableFailureType && FAILURE_TYPES_TO_IGNORE.indexOf(failureType) == -1) {
            if (mostRecentNonIgnorableFailureType)
                flipCount++;
            mostRecentNonIgnorableFailureType = failureType;
        }
    }

    resultsForTest.flipCount = flipCount;
    resultsForTest.isFlaky = flipCount > 1;
}

function linkHTMLToOpenWindow(url, text)
{
    return '<a href="' + url + '" target="_blank">' + text + '</a>';
}

// Returns whether the result for index'th result for testName on builder was
// a failure.
function isFailure(builder, testName, index)
{
    var currentIndex = 0;
    var rawResults = g_resultsByBuilder[builder].tests[testName].results;
    var failureMap = g_resultsByBuilder[builder][FAILURE_MAP_KEY];
    for (var i = 0; i < rawResults.length; i++) {
        currentIndex += rawResults[i][RLE.LENGTH];
        if (currentIndex > index)
            return isFailingResult(failureMap, rawResults[i][RLE.VALUE]);
    }
    console.error('Index exceeds number of results: ' + index);
}

// Returns an array of indexes for all builds where this test failed.
function indexesForFailures(builder, testName)
{
    var rawResults = g_resultsByBuilder[builder].tests[testName].results;
    var buildNumbers = g_resultsByBuilder[builder].buildNumbers;
    var failureMap = g_resultsByBuilder[builder][FAILURE_MAP_KEY];
    var index = 0;
    var failures = [];
    for (var i = 0; i < rawResults.length; i++) {
        var numResults = rawResults[i][RLE.LENGTH];
        if (isFailingResult(failureMap, rawResults[i][RLE.VALUE])) {
            for (var j = 0; j < numResults; j++)
                failures.push(index + j);
        }
        index += numResults;
    }
    return failures;
}

// Returns the path to the failure log for this non-webkit test.
function pathToFailureLog(testName)
{
    return '/steps/' + g_history.crossDashboardState.testType + '/logs/' + testName.split('.')[1]
}

function showPopupForBuild(e, builder, index, opt_testName)
{
    var html = '';

    var time = g_resultsByBuilder[builder].secondsSinceEpoch[index];
    if (time) {
        var date = new Date(time * 1000);
        html += date.toLocaleDateString() + ' ' + date.toLocaleTimeString();
    }

    var buildNumber = g_resultsByBuilder[builder].buildNumbers[index];
    var master = builderMaster(builder);
    var buildBasePath = master.logPath(builder, buildNumber);

    html += '<ul><li>' + linkHTMLToOpenWindow(buildBasePath, 'Build log');

    if (g_resultsByBuilder[builder][BLINK_REVISIONS_KEY])
        html += '</li><li>Blink: ' + ui.html.blinkRevisionLink(g_resultsByBuilder[builder], index) + '</li>';

    html += '</li><li>Chromium: ' + ui.html.chromiumRevisionLink(g_resultsByBuilder[builder], index) + '</li>';

    var chromeRevision = g_resultsByBuilder[builder].chromeRevision[index];
    if (chromeRevision && g_history.isLayoutTestResults()) {
        html += '<li><a href="' + TEST_RESULTS_BASE_PATH + currentBuilders()[builder] +
            '/' + chromeRevision + '/layout-test-results.zip">layout-test-results.zip</a></li>';
    }

    if (!g_history.isLayoutTestResults() && opt_testName && isFailure(builder, opt_testName, index))
        html += '<li>' + linkHTMLToOpenWindow(buildBasePath + pathToFailureLog(opt_testName), 'Failure log') + '</li>';

    html += '</ul>';
    ui.popup.show(e.target, html);
}

function classNameForFailureString(failure)
{
    return failure.replace(/(\+|\ )/, '');
}

function htmlForTestResults(test)
{
    var html = '';
    var results = test.rawResults.concat();
    var times = test.rawTimes.concat();
    var builder = test.builder;
    var master = builderMaster(builder);
    var buildNumbers = g_resultsByBuilder[builder].buildNumbers;

    var indexToReplaceCurrentResult = -1;
    var indexToReplaceCurrentTime = -1;
    for (var i = 0; i < buildNumbers.length; i++) {
        var currentResultArray, currentTimeArray, innerHTML, resultString;

        if (i > indexToReplaceCurrentResult) {
            currentResultArray = results.shift();
            if (currentResultArray) {
                resultString = g_resultsByBuilder[builder][FAILURE_MAP_KEY][currentResultArray[RLE.VALUE]];
                indexToReplaceCurrentResult += currentResultArray[RLE.LENGTH];
            } else {
                resultString = NO_DATA;
                indexToReplaceCurrentResult += buildNumbers.length;
            }
        }

        if (i > indexToReplaceCurrentTime) {
            currentTimeArray = times.shift();
            var currentTime = 0;
            if (currentResultArray) {
              currentTime = currentTimeArray[RLE.VALUE];
              indexToReplaceCurrentTime += currentTimeArray[RLE.LENGTH];
            } else
              indexToReplaceCurrentTime += buildNumbers.length;

            innerHTML = currentTime || '&nbsp;';
        }

        html += '<td title="' + resultString + '. Click for more info." class="results ' + classNameForFailureString(resultString) +
          '" onclick=\'showPopupForBuild(event, "' + builder + '",' + i + ',"' + test.test + '")\'>' + innerHTML;
    }
    return html;
}

function shouldShowTest(testResult)
{
    if (!g_history.isLayoutTestResults())
        return true;

    if (testResult.expectations == 'WONTFIX')
        return g_history.dashboardSpecificState.showWontFix;

    if (testResult.expectations == 'SKIP')
        return g_history.dashboardSpecificState.showSkip;

    if (testResult.isFlaky)
        return g_history.dashboardSpecificState.showFlaky;

    return g_history.dashboardSpecificState.showNonFlaky;
}

function createBugHTML(test)
{
    var symptom = test.isFlaky ? 'flaky' : 'failing';
    var title = encodeURIComponent('Layout Test ' + test.test + ' is ' + symptom);
    var description = encodeURIComponent('The following layout test is ' + symptom + ' on ' +
        '[insert platform]\n\n' + test.test + '\n\nProbable cause:\n\n' +
        '[insert probable cause]');

    url = 'https://code.google.com/p/chromium/issues/entry?template=Layout%20Test%20Failure&summary=' + title + '&comment=' + description;
    return '<a href="' + url + '">File new bug</a>';
}

function isCrossBuilderView()
{
    return g_history.dashboardSpecificState.tests || g_history.dashboardSpecificState.result;
}

function tableHeaders(opt_getAll)
{
    var headers = [];
    if (isCrossBuilderView() || opt_getAll)
        headers.push('builder');

    if (!isCrossBuilderView() || opt_getAll)
        headers.push('test');

    if (g_history.isLayoutTestResults() || opt_getAll)
        headers.push('bugs', 'expectations');

    headers.push('slowest run', 'flakiness (numbers are runtimes in seconds)');
    return headers;
}

function linkifyBugs(bugs)
{
    var html = '';
    bugs.forEach(function(bug) {
        var bugHtml;
        if (string.startsWith(bug, 'Bug('))
            bugHtml = bug;
        else
            bugHtml = '<a href="http://' + bug + '">' + bug + '</a>';
        html += '<div>' + bugHtml + '</div>'
    });
    return html;
}

function htmlForSingleTestRow(test)
{
    var headers = tableHeaders();
    var html = '';
    for (var i = 0; i < headers.length; i++) {
        var header = headers[i];
        if (string.startsWith(header, 'test') || string.startsWith(header, 'builder')) {
            // If isCrossBuilderView() is true, we're just viewing a single test
            // with results for many builders, so the first column is builder names
            // instead of test paths.
            var testCellClassName = 'test-link' + (isCrossBuilderView() ? ' builder-name' : '');
            var testCellHTML = isCrossBuilderView() ? test.builder : '<span class="link" onclick="g_history.setQueryParameter(\'tests\',\'' + test.test +'\');">' + test.test + '</span>';

            html += '<tr><td class="' + testCellClassName + '">' + testCellHTML;
        } else if (string.startsWith(header, 'bugs'))
            // FIXME: linkify bugs.
            html += '<td class=options-container>' + (linkifyBugs(test.bugs) || createBugHTML(test));
        else if (string.startsWith(header, 'expectations'))
            html += '<td class=options-container>' + test.expectations;
        else if (string.startsWith(header, 'slowest'))
            html += '<td>' + (test.slowestTime ? test.slowestTime + 's' : '');
        else if (string.startsWith(header, 'flakiness'))
            html += htmlForTestResults(test);
    }
    return html;
}

function sortColumnFromTableHeader(headerText)
{
    return headerText.split(' ', 1)[0];
}

function htmlForTableColumnHeader(headerName, opt_fillColSpan)
{
    // Use the first word of the header title as the sortkey
    var thisSortValue = sortColumnFromTableHeader(headerName);
    var arrowHTML = thisSortValue == g_history.dashboardSpecificState.sortColumn ?
        '<span class=' + g_history.dashboardSpecificState.sortOrder + '>' + (g_history.dashboardSpecificState.sortOrder == FORWARD ? '&uarr;' : '&darr;' ) + '</span>' : '';
    return '<th sortValue=' + thisSortValue +
        // Extend last th through all the rest of the columns.
        (opt_fillColSpan ? ' colspan=10000' : '') +
        // Extra span here is so flex boxing actually centers.
        // There's probably a better way to do this with CSS only though.
        '><div class=table-header-content><span></span>' + arrowHTML +
        '<span class=header-text>' + headerName + '</span>' + arrowHTML + '</div></th>';
}

function htmlForTestTable(rowsHTML, opt_excludeHeaders)
{
    var html = '<table class=test-table>';
    if (!opt_excludeHeaders) {
        html += '<thead><tr>';
        var headers = tableHeaders();
        for (var i = 0; i < headers.length; i++)
            html += htmlForTableColumnHeader(headers[i], i == headers.length - 1);
        html += '</tr></thead>';
    }
    return html + '<tbody>' + rowsHTML + '</tbody></table>';
}

function appendHTML(html)
{
    // InnerHTML to a div that's not in the document. This is
    // ~300ms faster in Safari 4 and Chrome 4 on mac.
    var div = document.createElement('div');
    div.innerHTML = html;
    document.body.appendChild(div);
    postHeightChangedMessage();
}

function alphanumericCompare(column, reverse)
{
    return reversibleCompareFunction(function(a, b) {
        // Put null entries at the bottom
        var a = a[column] ? String(a[column]) : 'z';
        var b = b[column] ? String(b[column]) : 'z';

        if (a < b)
            return -1;
        else if (a == b)
            return 0;
        else
            return 1;
    }, reverse);
}

function numericSort(column, reverse)
{
    return reversibleCompareFunction(function(a, b) {
        a = parseFloat(a[column]);
        b = parseFloat(b[column]);
        return a - b;
    }, reverse);
}

function reversibleCompareFunction(compare, reverse)
{
    return function(a, b) {
        return compare(reverse ? b : a, reverse ? a : b);
    };
}

function changeSort(e)
{
    var target = e.currentTarget;
    e.preventDefault();

    var sortValue = target.getAttribute('sortValue');
    while (target && target.tagName != 'TABLE')
        target = target.parentNode;

    var sort = 'sortColumn';
    var orderKey = 'sortOrder';
    if (sortValue == g_history.dashboardSpecificState[sort] && g_history.dashboardSpecificState[orderKey] == FORWARD)
        order = BACKWARD;
    else
        order = FORWARD;

    g_history.setQueryParameter(sort, sortValue, orderKey, order);
}

function sortTests(tests, column, order)
{
    var resultsProperty, sortFunctionGetter;
    if (column == 'flakiness') {
        sortFunctionGetter = numericSort;
        resultsProperty = 'flips';
    } else if (column == 'slowest') {
        sortFunctionGetter = numericSort;
        resultsProperty = 'slowestTime';
    } else {
        sortFunctionGetter = alphanumericCompare;
        resultsProperty = column;
    }

    tests.sort(sortFunctionGetter(resultsProperty, order == BACKWARD));
}

function htmlForIndividualTestOnAllBuilders(test)
{
    processTestRunsForAllBuilders();

    var testResults = g_testToResultsMap[test];
    if (!testResults)
        return '<div class="not-found">Test not found. Either it does not exist, is skipped or passes on all platforms.</div>';

    var html = '';
    var shownBuilders = [];
    for (var j = 0; j < testResults.length; j++) {
        shownBuilders.push(testResults[j].builder);
        html += htmlForSingleTestRow(testResults[j]);
    }

    var skippedBuilders = []
    for (builder in currentBuilders()) {
        if (shownBuilders.indexOf(builder) == -1)
            skippedBuilders.push(builder);
    }

    var skippedBuildersHtml = '';
    if (skippedBuilders.length) {
        skippedBuildersHtml = '<div>The following builders either don\'t run this test (e.g. it\'s skipped) or all runs passed:</div>' +
            '<div class=skipped-builder-list><div class=skipped-builder>' + skippedBuilders.join('</div><div class=skipped-builder>') + '</div></div>';
    }

    return htmlForTestTable(html) + skippedBuildersHtml;
}

function htmlForIndividualTestOnAllBuildersWithResultsLinks(test)
{
    processTestRunsForAllBuilders();

    var testResults = g_testToResultsMap[test];
    var html = '';
    html += htmlForIndividualTestOnAllBuilders(test);

    html += '<div class=expectations test=' + test + '><div>' +
        linkHTMLToToggleState('showExpectations', 'results')

    if (g_history.isLayoutTestResults() || g_history.isGPUTestResults()) {
        if (g_history.isLayoutTestResults())
            html += ' | ' + linkHTMLToToggleState('showLargeExpectations', 'large thumbnails');
            html += ' | <b>Only shows actual results/diffs from the most recent *failure* on each bot.</b>';
    } else {
      html += ' | <span>Results height:<input ' +
          'onchange="g_history.setQueryParameter(\'resultsHeight\',this.value)" value="' +
          g_history.dashboardSpecificState.resultsHeight + '" style="width:2.5em">px</span>';
    }
    html += '</div></div>';
    return html;
}

function getExpectationsContainer(expectationsContainers, parentContainer, expectationsType)
{
    if (!expectationsContainers[expectationsType]) {
        var container = document.createElement('div');
        container.className = 'expectations-container';
        parentContainer.appendChild(container);
        expectationsContainers[expectationsType] = container;
    }
    return expectationsContainers[expectationsType];
}

function ensureTrailingSlash(path)
{
    if (path.match(/\/$/))
        return path;
    return path + '/';
}

function maybeAddPngChecksum(expectationDiv, pngUrl)
{
    // pngUrl gets served from the browser cache since we just loaded it in an
    // <img> tag.
    loader.request(pngUrl,
        function(xhr) {
            // Convert the first 2k of the response to a byte string.
            var bytes = xhr.responseText.substring(0, 2048);
            for (var position = 0; position < bytes.length; ++position)
                bytes[position] = bytes[position] & 0xff;

            // Look for the comment.
            var commentKey = 'tEXtchecksum\x00';
            var checksumPosition = bytes.indexOf(commentKey);
            if (checksumPosition == -1)
                return;

            var checksum = bytes.substring(checksumPosition + commentKey.length, checksumPosition + commentKey.length + 32);
            var checksumContainer = document.createElement('span');
            checksumContainer.innerText = 'Embedded checksum: ' + checksum;
            checksumContainer.setAttribute('class', 'pngchecksum');
            expectationDiv.parentNode.appendChild(checksumContainer);
        },
        function(xhr) {},
        true);
}

// Adds a specific expectation. If it's an image, it's only added on the
// image's onload handler. If it's a text file, then a script tag is appended
// as a hack to see if the file 404s (necessary since it's cross-domain).
// Once all the expectations for a specific type have loaded or errored
// (e.g. all the text results), then we go through and identify which platform
// uses which expectation.
//
// @param {Object} expectationsContainers Map from expectations type to
//     container DIV.
// @param {Element} parentContainer Container element for
//     expectationsContainer divs.
// @param {string} platform Platform string. Empty string for non-platform
//     specific expectations.
// @param {string} path Relative path to the expectation.
// @param {string} base Base path for the expectation URL.
// @param {string} opt_builder Builder whose actual results this expectation
//     points to.
// @param {string} opt_suite "virtual suite" that the test belongs to, if any.
function addExpectationItem(expectationsContainers, parentContainer, platform, path, base, opt_builder, opt_suite)
{
    var parts = path.split('.')
    var fileExtension = parts[parts.length - 1];
    if (fileExtension == 'html')
        fileExtension = 'txt';

    var container = getExpectationsContainer(expectationsContainers, parentContainer, fileExtension);
    var isImage = path.match(/\.png$/);

    // FIXME: Stop using script tags once all the places we pull from support CORS.
    var platformPart = platform ? ensureTrailingSlash(platform) : '';
    var suitePart = opt_suite ? ensureTrailingSlash(opt_suite) : '';

    var childContainer = document.createElement('span');
    childContainer.className = 'unloaded';

    var appendExpectationsItem = function(item) {
        childContainer.appendChild(expectationsTitle(platformPart + suitePart, path, opt_builder));
        childContainer.className = 'expectations-item';
        item.className = 'expectation ' + fileExtension;
        if (g_history.dashboardSpecificState.showLargeExpectations)
            item.className += ' large';
        childContainer.appendChild(item);
        handleFinishedLoadingExpectations(container);
    };

    var url = base + platformPart + path;
    if (isImage) {
        var dummyNode = document.createElement(isImage ? 'img' : 'script');
        dummyNode.src = url;
        dummyNode.onload = function() {
            var item;
            if (isImage) {
                item = dummyNode;
                maybeAddPngChecksum(item, url);
            } else {
                item = document.createElement('iframe');
                item.src = url;
            }
            appendExpectationsItem(item);
        }
        dummyNode.onerror = function() {
            childContainer.parentNode.removeChild(childContainer);
            handleFinishedLoadingExpectations(container);
        }

        // Append script elements now so that they load. Images load without being
        // appended to the DOM.
        if (!isImage)
            childContainer.appendChild(dummyNode);
    } else {
        loader.request(url,
            function(xhr) {
                var item = document.createElement('pre');
                item.innerText = xhr.responseText;
                appendExpectationsItem(item);
            },
            function(xhr) {/* Do nothing on errors since they're expected */});
    }

    container.appendChild(childContainer);
}


// Identifies which expectations are used on which platform once all the
// expectations of a given type have loaded (e.g. the container for png
// expectations for this test had no child elements with the class
// "unloaded").
//
// @param {string} container Element containing the expectations for a given
//     test and a given type (e.g. png).
function handleFinishedLoadingExpectations(container)
{
    if (container.getElementsByClassName('unloaded').length)
        return;

    var titles = container.getElementsByClassName('expectations-title');
    for (var platform in g_fallbacksMap) {
        var fallbacks = g_fallbacksMap[platform];
        var winner = null;
        var winningIndex = -1;
        for (var i = 0; i < titles.length; i++) {
            var title = titles[i];

            if (!winner && title.platform == "") {
                winner = title;
                continue;
            }

            var rawPlatform = title.platform && title.platform.replace('platform/', '');
            for (var j = 0; j < fallbacks.length; j++) {
                if ((winningIndex == -1 || winningIndex > j) && rawPlatform == fallbacks[j]) {
                    winningIndex = j;
                    winner = title;
                    break;
                }
            }
        }
        if (winner)
            winner.getElementsByClassName('platforms')[0].innerHTML += '<div class=used-platform>' + platform + '</div>';
        else {
            console.log('No expectations identified for this test. This means ' +
                'there is a logic bug in the dashboard for which expectations a ' +
                'platform uses or src.chromium.org is giving 5XXs.');
        }
    }

    consolidateUsedPlatforms(container);
}

// Consolidate platforms when all sub-platforms for a given platform are represented.
// e.g., if all of the WIN- platforms are there, replace them with just WIN.
function consolidateUsedPlatforms(container)
{
    var allPlatforms = Object.keys(g_fallbacksMap);

    var platformElements = container.getElementsByClassName('platforms');
    for (var i = 0, platformsLength = platformElements.length; i < platformsLength; i++) {
        var usedPlatforms = platformElements[i].getElementsByClassName('used-platform');
        if (!usedPlatforms.length)
            continue;

        var platforms = {};
        platforms['MAC'] = {};
        platforms['WIN'] = {};
        platforms['LINUX'] = {};
        allPlatforms.forEach(function(platform) {
            if (string.startsWith(platform, 'MAC'))
                platforms['MAC'][platform] = 1;
            else if (string.startsWith(platform, 'WIN'))
                platforms['WIN'][platform] = 1;
            else if (string.startsWith(platform, 'LINUX'))
                platforms['LINUX'][platform] = 1;
        });

        for (var j = 0, usedPlatformsLength = usedPlatforms.length; j < usedPlatformsLength; j++) {
            for (var platform in platforms)
                delete platforms[platform][usedPlatforms[j].textContent];
        }

        for (var platform in platforms) {
            if (!Object.keys(platforms[platform]).length) {
                var nodesToRemove = [];
                for (var j = 0, usedPlatformsLength = usedPlatforms.length; j < usedPlatformsLength; j++) {
                    var usedPlatform = usedPlatforms[j];
                    if (string.startsWith(usedPlatform.textContent, platform))
                        nodesToRemove.push(usedPlatform);
                }

                nodesToRemove.forEach(function(element) { element.parentNode.removeChild(element); });
                platformElements[i].insertAdjacentHTML('afterBegin', '<div class=used-platform>' + platform + '</div>');
            }
        }
    }
}

function addExpectations(expectationsContainers, container, base,
    platform, text, png, reftest_html_file, reftest_mismatch_html_file, suite)
{
    var builder = '';
    addExpectationItem(expectationsContainers, container, platform, text, base, builder, suite);
    addExpectationItem(expectationsContainers, container, platform, png, base, builder, suite);
    addExpectationItem(expectationsContainers, container, platform, reftest_html_file, base, builder, suite);
    addExpectationItem(expectationsContainers, container, platform, reftest_mismatch_html_file, base, builder, suite);
}

function expectationsTitle(platform, path, builder)
{
    var header = document.createElement('h3');
    header.className = 'expectations-title';

    var innerHTML;
    if (builder) {
        var resultsType;
        if (string.endsWith(path, '-crash-log.txt'))
            resultsType = 'STACKTRACE';
        else if (string.endsWith(path, '-actual.txt') || string.endsWith(path, '-actual.png'))
            resultsType = 'ACTUAL RESULTS';
        else if (string.endsWith(path, '-wdiff.html'))
            resultsType = 'WDIFF';
        else
            resultsType = 'DIFF';

        innerHTML = resultsType + ': ' + builder;
    } else if (platform === "") {
        var parts = path.split('/');
        innerHTML = parts[parts.length - 1];
    } else
        innerHTML = platform || path;

    header.innerHTML = '<div class=title>' + innerHTML +
        '</div><div style="float:left">&nbsp;</div>' +
        '<div class=platforms style="float:right"></div>';
    header.platform = platform;
    return header;
}

function loadExpectations(expectationsContainer)
{
    var test = expectationsContainer.getAttribute('test');
    if (g_history.isLayoutTestResults())
        loadExpectationsLayoutTests(test, expectationsContainer);
    else {
        var results = g_testToResultsMap[test];
        for (var i = 0; i < results.length; i++)
            if (g_history.isGPUTestResults())
                loadGPUResultsForBuilder(results[i].builder, test, expectationsContainer);
            else
                loadNonWebKitResultsForBuilder(results[i].builder, test, expectationsContainer);
    }
}

function gpuResultsPath(chromeRevision, builder)
{
  return chromeRevision + '_' + builder.replace(/[^A-Za-z0-9]+/g, '_');
}

function loadGPUResultsForBuilder(builder, test, expectationsContainer)
{
    var container = document.createElement('div');
    container.className = 'expectations-container';
    container.innerHTML = '<div><b>' + builder + '</b></div>';
    expectationsContainer.appendChild(container);

    var failureIndex = indexesForFailures(builder, test)[0];

    var buildNumber = g_resultsByBuilder[builder].buildNumbers[failureIndex];
    var pathToLog = builderMaster(builder).logPath(builder, buildNumber) + pathToFailureLog(test);

    var chromeRevision = g_resultsByBuilder[builder].chromeRevision[failureIndex];
    var resultsUrl = GPU_RESULTS_BASE_PATH + gpuResultsPath(chromeRevision, builder);
    var filename = test.split(/\./)[1] + '.png';

    appendNonWebKitResults(container, pathToLog, 'non-webkit-results');
    appendNonWebKitResults(container, resultsUrl + '/gen/' + filename, 'gpu-test-results', 'Generated');
    appendNonWebKitResults(container, resultsUrl + '/ref/' + filename, 'gpu-test-results', 'Reference');
    appendNonWebKitResults(container, resultsUrl + '/diff/' + filename, 'gpu-test-results', 'Diff');
}

function loadNonWebKitResultsForBuilder(builder, test, expectationsContainer)
{
    var failureIndexes = indexesForFailures(builder, test);
    var container = document.createElement('div');
    container.innerHTML = '<div><b>' + builder + '</b></div>';
    expectationsContainer.appendChild(container);
    for (var i = 0; i < failureIndexes.length; i++) {
        // FIXME: This doesn't seem to work anymore. Did the paths change?
        // Once that's resolved, see if we need to try each GTEST_MODIFIERS prefix as well.
        var buildNumber = g_resultsByBuilder[builder].buildNumbers[failureIndexes[i]];
        var pathToLog = builderMaster(builder).logPath(builder, buildNumber) + pathToFailureLog(test);
        appendNonWebKitResults(container, pathToLog, 'non-webkit-results');
    }
}

function appendNonWebKitResults(container, url, itemClassName, opt_title)
{
    // Use a script tag to detect whether the URL 404s.
    // Need to use a script tag since the URL is cross-domain.
    var dummyNode = document.createElement('script');
    dummyNode.src = url;

    dummyNode.onload = function() {
        var item = document.createElement('iframe');
        item.src = dummyNode.src;
        item.className = itemClassName;
        item.style.height = g_history.dashboardSpecificState.resultsHeight + 'px';

        if (opt_title) {
            var childContainer = document.createElement('div');
            childContainer.style.display = 'inline-block';
            var title = document.createElement('div');
            title.textContent = opt_title;
            childContainer.appendChild(title);
            childContainer.appendChild(item);
            container.replaceChild(childContainer, dummyNode);
        } else
            container.replaceChild(item, dummyNode);
    }
    dummyNode.onerror = function() {
        container.removeChild(dummyNode);
    }

    container.appendChild(dummyNode);
}

function lookupVirtualTestSuite(test) {
    for (var suite in VIRTUAL_SUITES) {
        if (test.indexOf(suite) != -1)
            return suite;
    }
    return '';
}

function baseTest(test, suite) {
    base = VIRTUAL_SUITES[suite];
    return base ? test.replace(suite, base) : test;
}

function loadBaselinesForTest(expectationsContainers, expectationsContainer, test) {
    var testWithoutSuffix = test.substring(0, test.lastIndexOf('.'));
    var text = testWithoutSuffix + "-expected.txt";
    var png = testWithoutSuffix + "-expected.png";
    var reftest_html_file = testWithoutSuffix + "-expected.html";
    var reftest_mismatch_html_file = testWithoutSuffix + "-expected-mismatch.html";
    var suite = lookupVirtualTestSuite(test);

    if (!suite)
        addExpectationItem(expectationsContainers, expectationsContainer, null, test, TEST_URL_BASE_PATH_FOR_XHR);

    addExpectations(expectationsContainers, expectationsContainer,
        TEST_URL_BASE_PATH_FOR_XHR, '', text, png, reftest_html_file, reftest_mismatch_html_file, suite);

    var fallbacks = allFallbacks();
    for (var i = 0; i < fallbacks.length; i++) {
      var fallback = 'platform/' + fallbacks[i];
      addExpectations(expectationsContainers, expectationsContainer, TEST_URL_BASE_PATH_FOR_XHR, fallback, text, png,
          reftest_html_file, reftest_mismatch_html_file, suite);
    }

    if (suite)
        loadBaselinesForTest(expectationsContainers, expectationsContainer, baseTest(test, suite));
}

function loadExpectationsLayoutTests(test, expectationsContainer)
{
    // Map from file extension to container div for expectations of that type.
    var expectationsContainers = {};

    var revisionContainer = document.createElement('div');
    revisionContainer.textContent = "Showing results for: "
    expectationsContainer.appendChild(revisionContainer);
    loadBaselinesForTest(expectationsContainers, expectationsContainer, test);

    var testWithoutSuffix = test.substring(0, test.lastIndexOf('.'));
    var actualResultSuffixes = ['-actual.txt', '-actual.png', '-crash-log.txt', '-diff.txt', '-wdiff.html', '-diff.png'];

    for (var builder in currentBuilders()) {
        var actualResultsBase = TEST_RESULTS_BASE_PATH + currentBuilders()[builder] + '/results/layout-test-results/';

        for (var i = 0; i < actualResultSuffixes.length; i++) {
            addExpectationItem(expectationsContainers, expectationsContainer, null,
                testWithoutSuffix + actualResultSuffixes[i], actualResultsBase, builder);
        }
    }

    // Add a clearing element so floated elements don't bleed out of their
    // containing block.
    var br = document.createElement('br');
    br.style.clear = 'both';
    expectationsContainer.appendChild(br);
}

var g_allFallbacks;

// Returns the reverse sorted, deduped list of all platform fallback
// directories.
function allFallbacks()
{
    if (!g_allFallbacks) {
        var holder = {};
        for (var platform in g_fallbacksMap) {
            var fallbacks = g_fallbacksMap[platform];
            for (var i = 0; i < fallbacks.length; i++)
                holder[fallbacks[i]] = 1;
        }

        g_allFallbacks = [];
        for (var fallback in holder)
            g_allFallbacks.push(fallback);

        g_allFallbacks.sort(function(a, b) {
            if (a == b)
                return 0;
            return a < b;
        });
    }
    return g_allFallbacks;
}

function appendExpectations()
{
    var expectations = g_history.dashboardSpecificState.showExpectations ? document.getElementsByClassName('expectations') : [];
    // Loading expectations is *very* slow. Use a large timeout to avoid
    // totally hanging the renderer.
    performChunkedAction(expectations, function(chunk) {
        for (var i = 0, len = chunk.length; i < len; i++)
            loadExpectations(chunk[i]);
        postHeightChangedMessage();

    }, hideLoadingUI, 10000);
}

function hideLoadingUI()
{
    var loadingDiv = $('loading-ui');
    if (loadingDiv)
        loadingDiv.style.display = 'none';
    postHeightChangedMessage();
}

function generatePageForIndividualTests(tests)
{
    console.log('Number of tests: ' + tests.length);
    if (g_history.dashboardSpecificState.showChrome)
        appendHTML(htmlForNavBar());
    performChunkedAction(tests, function(chunk) {
        appendHTML(htmlForIndividualTests(chunk));
    }, appendExpectations, 500);
    if (g_history.dashboardSpecificState.showChrome)
        $('tests-input').value = g_history.dashboardSpecificState.tests;
}

function performChunkedAction(tests, handleChunk, onComplete, timeout, opt_index) {
    var index = opt_index || 0;
    setTimeout(function() {
        var chunk = Array.prototype.slice.call(tests, index * CHUNK_SIZE, (index + 1) * CHUNK_SIZE);
        if (chunk.length) {
            handleChunk(chunk);
            performChunkedAction(tests, handleChunk, onComplete, timeout, ++index);
        } else
            onComplete();
    // No need for a timeout on the first chunked action.
    }, index ? timeout : 0);
}

function htmlForIndividualTests(tests)
{
    var testsHTML = [];
    for (var i = 0; i < tests.length; i++) {
        var test = tests[i];
        var testNameHtml = '';
        if (g_history.dashboardSpecificState.showChrome || tests.length > 1) {
            if (g_history.isLayoutTestResults()) {
                var suite = lookupVirtualTestSuite(test);
                var base = suite ? baseTest(test, suite) : test;
                var versionControlUrl = TEST_URL_BASE_PATH_FOR_BROWSING + base;
                testNameHtml += '<h2>' + linkHTMLToOpenWindow(versionControlUrl, test) + '</h2>';
            } else
                testNameHtml += '<h2>' + test + '</h2>';
        }

        testsHTML.push(testNameHtml + htmlForIndividualTestOnAllBuildersWithResultsLinks(test));
    }
    return testsHTML.join('<hr>');
}

function htmlForNavBar()
{
    var extraHTML = '';
    var html = ui.html.testTypeSwitcher(false, extraHTML, isCrossBuilderView());
    html += '<div class=forms><form id=result-form ' +
        'onsubmit="g_history.setQueryParameter(\'result\', result.value);' +
        'return false;">Show all tests with result: ' +
        '<input name=result placeholder="e.g. CRASH" id=result-input>' +
        '</form><form id=tests-form ' +
        'onsubmit="g_history.setQueryParameter(\'tests\', tests.value);' +
        'return false;"><span>Show tests on all platforms: </span>' +
        '<input name=tests ' +
        'placeholder="Comma or space-separated list of tests or partial ' +
        'paths to show test results across all builders, e.g., ' +
        'foo/bar.html,foo/baz,domstorage" id=tests-input></form>' +
        '<span class=link onclick="showLegend()">Show legend [type ?]</span></div>';
    return html;
}

function checkBoxToToggleState(key, text)
{
    var stateEnabled = g_history.dashboardSpecificState[key];
    return '<label><input type=checkbox ' + (stateEnabled ? 'checked ' : '') + 'onclick="g_history.setQueryParameter(\'' + key + '\', ' + !stateEnabled + ')">' + text + '</label> ';
}

function linkHTMLToToggleState(key, linkText)
{
    var stateEnabled = g_history.dashboardSpecificState[key];
    return '<span class=link onclick="g_history.setQueryParameter(\'' + key + '\', ' + !stateEnabled + ')">' + (stateEnabled ? 'Hide' : 'Show') + ' ' + linkText + '</span>';
}

function headerForTestTableHtml()
{
    return '<h2 style="display:inline-block">Failing tests</h2>' +
        checkBoxToToggleState('showFlaky', 'Show flaky') +
        checkBoxToToggleState('showNonFlaky', 'Show non-flaky') +
        checkBoxToToggleState('showSkip', 'Show Skip') +
        checkBoxToToggleState('showWontFix', 'Show WontFix');
}

function generatePageForBuilder(builderName)
{
    processTestRunsForBuilder(builderName);

    var results = g_perBuilderFailures[builderName].filter(shouldShowTest);
    sortTests(results, g_history.dashboardSpecificState.sortColumn, g_history.dashboardSpecificState.sortOrder);

    var testsHTML = '';
    if (results.length) {
        var tableRowsHTML = '';
        for (var i = 0; i < results.length; i++)
            tableRowsHTML += htmlForSingleTestRow(results[i])
        testsHTML = htmlForTestTable(tableRowsHTML);
    } else {
        if (g_history.isLayoutTestResults())
            testsHTML += '<div>Fill in one of the text inputs or checkboxes above to show failures.</div>';
        else
            testsHTML += '<div>No tests have failed!</div>';
    }

    var html = htmlForNavBar();

    if (g_history.isLayoutTestResults())
        html += headerForTestTableHtml();

    html += '<br>' + testsHTML;
    appendHTML(html);

    var ths = document.getElementsByTagName('th');
    for (var i = 0; i < ths.length; i++) {
        ths[i].addEventListener('click', changeSort, false);
        ths[i].className = "sortable";
    }

    hideLoadingUI();
}

var VALID_KEYS_FOR_CROSS_BUILDER_VIEW = {
    tests: 1,
    result: 1,
    showChrome: 1,
    showExpectations: 1,
    showLargeExpectations: 1,
    resultsHeight: 1,
    revision: 1
};

function isInvalidKeyForCrossBuilderView(key)
{
    return !(key in VALID_KEYS_FOR_CROSS_BUILDER_VIEW) && !(key in history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES);
}

function hideLegend()
{
    var legend = $('legend');
    if (legend)
        legend.parentNode.removeChild(legend);
}

var g_fallbacksMap = {};
g_fallbacksMap['WIN-XP'] = ['chromium-win-xp', 'chromium-win', 'chromium'];
g_fallbacksMap['WIN-7'] = ['chromium-win', 'chromium'];
g_fallbacksMap['MAC-SNOWLEOPARD'] = ['chromium-mac-snowleopard', 'chromium-mac', 'chromium'];
g_fallbacksMap['MAC-LION'] = ['chromium-mac', 'chromium'];
g_fallbacksMap['LINUX-32'] = ['chromium-linux-x86', 'chromium-linux', 'chromium-win', 'chromium'];
g_fallbacksMap['LINUX-64'] = ['chromium-linux', 'chromium-win', 'chromium'];

function htmlForFallbackHelp(fallbacks)
{
    return '<ol class=fallback-list><li>' + fallbacks.join('</li><li>') + '</li></ol>';
}

function showLegend()
{
    var legend = $('legend');
    if (!legend) {
        legend = document.createElement('div');
        legend.id = 'legend';
        document.body.appendChild(legend);
    }

    var html = '<div id=legend-toggle onclick="hideLegend()">Hide ' +
        'legend [type esc]</div><div id=legend-contents>';

    // Just grab the first failureMap. Technically, different builders can have different maps if they
    // haven't all cycled after the map was changed, but meh.
    var failureMap = g_resultsByBuilder[Object.keys(g_resultsByBuilder)[0]][FAILURE_MAP_KEY];
    for (var expectation in failureMap) {
        var failureString = failureMap[expectation];
        html += '<div class=' + classNameForFailureString(failureString) + '>' + failureString + '</div>';
    }

    if (g_history.isLayoutTestResults()) {
      html += '</div><br style="clear:both">' +
          '</div><h3>Test expectations fallback order.</h3>';

      for (var platform in g_fallbacksMap)
          html += '<div class=fallback-header>' + platform + '</div>' + htmlForFallbackHelp(g_fallbacksMap[platform]);

      html += '<div>RELEASE TIMEOUTS:</div>' +
          htmlForSlowTimes(RELEASE_TIMEOUT) +
          '<div>DEBUG TIMEOUTS:</div>' +
          htmlForSlowTimes(DEBUG_TIMEOUT);
    }

    legend.innerHTML = html;
}

function htmlForSlowTimes(minTime)
{
    return '<ul><li>' + minTime + ' seconds</li><li>' +
        SLOW_MULTIPLIER * minTime + ' seconds if marked Slow in TestExpectations</li></ul>';
}

function postHeightChangedMessage()
{
    if (window == parent)
        return;

    var root = document.documentElement;
    var height = root.offsetHeight;
    if (root.offsetWidth < root.scrollWidth) {
        // We have a horizontal scrollbar. Include it in the height.
        var dummyNode = document.createElement('div');
        dummyNode.style.overflow = 'scroll';
        document.body.appendChild(dummyNode);
        var scrollbarWidth = dummyNode.offsetHeight - dummyNode.clientHeight;
        document.body.removeChild(dummyNode);
        height += scrollbarWidth;
    }
    parent.postMessage({command: 'heightChanged', height: height}, '*')
}

if (window != parent)
    window.addEventListener('blur', ui.popup.hide);

document.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+003F' || e.keyIdentifier == 'U+00BF') {
        // WebKit MAC retursn 3F. WebKit WIN returns BF. This is a bug!
        // ? key
        showLegend();
    } else if (e.keyIdentifier == 'U+001B') {
        // escape key
        hideLegend();
        ui.popup.hide();
    }
}, false);

window.addEventListener('load', function() {
    resourceLoader = new loader.Loader();
    resourceLoader.load();
}, false);
