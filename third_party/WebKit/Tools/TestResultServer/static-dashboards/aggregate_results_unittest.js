
module('aggregate_results');

function setupAggregateResultsData()
{
    var builderName = 'Blink Linux';
    LOAD_BUILDBOT_DATA([{
        name: 'ChromiumWebkit',
        url: 'dummyurl',
        tests: {'layout-tests': {'builders': [builderName]}}
    }]);
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
