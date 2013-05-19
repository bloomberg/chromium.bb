// Copyright (C) 2013 Google Inc. All rights reserved.
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

module('aggregate_results');

function setupAggregateResultsData()
{
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    for (var key in history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES)
        historyInstance.crossDashboardState[key] = history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES[key];

    var builderName = 'Blink Linux';
    LOAD_BUILDBOT_DATA([{
        name: 'ChromiumWebkit',
        url: 'dummyurl',
        tests: {'layout-tests': {'builders': [builderName]}}
    }]);
    for (var group in LAYOUT_TESTS_BUILDER_GROUPS)
        LAYOUT_TESTS_BUILDER_GROUPS[group] = null;

    loadBuildersList('@ToT - chromium.org', 'layout-tests');

    g_resultsByBuilder[builderName] = {
        allFixableCount: [5, 6],
        fixableCount: [4, 2],
        fixableCounts: [
            {"A":0,"C":1,"F":2,"I":1,"O":0,"P":55,"T":0,"X":0,"Z":0},
            {"A":0,"C":1,"F":0,"I":1,"O":0,"P":55,"T":0,"X":0,"Z":0},
        ],
        blinkRevision: [1234, 1233],
        chromeRevision: [4567, 4566]
    }
}

test('htmlForBuilder', 1, function() {
    setupAggregateResultsData();
    g_history.dashboardSpecificState.rawValues = false;

    var expectedHtml = '<div class=container>' +
        '<h2>Blink Linux</h2>' +
        '<a href="timeline_explorer.html#useTestData=true&builder=Blink Linux">' +
            '<img src="http://chart.apis.google.com/chart?cht=lc&chs=600x400&chd=e:gA..&chg=15,15,1,3&chxt=x,x,y&chxl=1:||Blink Revision|&chxr=0,1233,1234|2,0,4&chtt=Total failing"><img src="http://chart.apis.google.com/chart?cht=lc&chs=600x400&chd=e:AAAA,AAAA,AA..,gAgA,gAgA,AAAA,AAAA&chg=15,15,1,3&chxt=x,x,y&chxl=1:||Blink Revision|&chxr=0,1233,1234|2,0,2&chtt=Detailed breakdown&chdl=SKIP|TIMEOUT|TEXT|CRASH|IMAGE|IMAGE+TEXT|MISSING&chco=FF0000,00FF00,0000FF,000000,FF6EB4,FFA812,9B30FF">' +
        '</a></div>';
    equal(expectedHtml, htmlForBuilder('Blink Linux'));
});

test('htmlForBuilderRawResults', 1, function() {
    setupAggregateResultsData();
    g_history.dashboardSpecificState.rawValues = true;

    var expectedHtml = '<div class=container>' +
        '<h2>Blink Linux</h2><h3>Summary</h3>' +
        '<table><tbody>' +
            '<tr><td>Blink Revision</td><td>1234</td><td>1233</td></tr>' +
            '<tr><td>Chrome Revision</td><td>4567</td><td>4566</td></tr>' +
            '<tr><td>Percent passed</td><td>20%</td><td>66.7%</td></tr>' +
            '<tr><td>Failures</td><td>4</td><td>2</td></tr>' +
            '<tr><td>Total Tests</td><td>5</td><td>6</td></tr>' +
        '</tbody></table>' +
        '<h3>All tests for this release</h3>' +
            '<table><tbody>' +
                '<tr><td>Blink Revision</td><td>1234</td><td>1233</td></tr>' +
                '<tr><td>Chrome Revision</td><td>4567</td><td>4566</td></tr>' +
                '<tr><td>PASS</td><td>55</td><td>55</td></tr>' +
                '<tr><td>SKIP</td><td>0</td><td>0</td></tr>' +
                '<tr><td>TIMEOUT</td><td>0</td><td>0</td></tr>' +
                '<tr><td>TEXT</td><td>2</td><td>0</td></tr>' +
                '<tr><td>CRASH</td><td>1</td><td>1</td></tr>' +
                '<tr><td>IMAGE</td><td>1</td><td>1</td></tr>' +
                '<tr><td>IMAGE+TEXT</td><td>0</td><td>0</td></tr>' +
                '<tr><td>MISSING</td><td>0</td><td>0</td></tr>' +
        '</tbody></table></div>';
    equal(expectedHtml, htmlForBuilder('Blink Linux'));
});
