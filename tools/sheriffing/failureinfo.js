// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Return the range of intersection between the two lists. Ranges are sets of
 * CLs, so it's inclusive on both ends.
 */
function rangeIntersection(a, b) {
  if (a[0] > b[1])
    return null;
  if (a[1] < b[0])
    return null;
  // We know they intersect, result is the larger lower bound to the smaller
  // upper bound.
  return [Math.max(a[0], b[0]), Math.min(a[1], b[1])];
}

/**
 * Finds the intersections between the blamelists.
 * Input is a object mapping botname => [low, high].
 *
 * Result:
 * [
 *   [ low0, high0 ], [ bot1, bot2, bot3 ],
 *   [ low1, high1 ], [ bot4, ... ],
 *   ...,
 * ]
 */
function findIntersections(testData) {
  var keys = Object.keys(testData);
  var intersections = [];
  var botName = keys[0];
  var range = testData[botName];
  intersections.push([range, [botName]]);
  for (var i = 1; i < keys.length; ++i) {
    botName = keys[i];
    range = testData[botName];
    var intersectedSome = false;
    for (var j = 0; j < intersections.length; ++j) {
      var intersect = rangeIntersection(intersections[j][0], range);
      if (intersect) {
        intersections[j][0] = intersect;
        intersections[j][1].push(botName);
        intersectedSome = true;
        break;
      }
    }
    if (!intersectedSome) {
      intersections.push([range, [botName]]);
    }
  }

  return intersections;
}

/** Flatten out the list of tests and sort them by the binary/test name. */
function flattenAndSortTests(rangesByTest) {
  var rangesByTestNames = Object.keys(rangesByTest);
  var flat = [];
  for (var i = 0; i < rangesByTestNames.length; ++i) {
    var name = rangesByTestNames[i];
    flat.push([name, rangesByTest[name]]);
  }

  flat.sort(function(a, b) {
    if (a[0] < b[0]) return -1;
    if (a[0] > b[0]) return 1;
    return 0;
  });

  return flat;
}

/**
 * Build the HTML table row for a test failure. |test| is [ name, testData ].
 * |testData| contains ranges suitable for input to |findIntersections|.
 */
function buildTestFailureTableRowHTML(test) {
  var row = document.createElement('tr');
  var binaryCell = row.insertCell(-1);
  var nameParts = test[0].split('-');
  binaryCell.innerHTML = nameParts[0];
  binaryCell.className = 'category';
  var flakinessLink = document.createElement('a');
  flakinessLink.href =
      'http://test-results.appspot.com/dashboards/' +
      'flakiness_dashboard.html#testType=' +
      nameParts[0] + '&tests=' + nameParts[1];
  flakinessLink.innerHTML = nameParts[1];
  row.appendChild(flakinessLink);

  var intersections = findIntersections(test[1]);
  for (var j = 0; j < intersections.length; ++j) {
    var intersection = intersections[j];
    var range = row.insertCell(-1);
    range.className = 'failure-range';
    var low = intersection[0][0];
    var high = intersection[0][1];
    var url =
        'http://build.chromium.org/f/chromium/perf/dashboard/ui/' +
        'changelog.html?url=%2Ftrunk%2Fsrc&range=' +
        low + '%3A' + high + '&mode=html';
    range.innerHTML = '<a href="' + url + '">' + low + ' - ' + high + '</a>: ' +
                      truncateStatusText(intersection[1].join(', '));
  }
  return row;
}


/** Updates the correlations contents. */
function updateCorrelationsHTML() {
  // The logic here is to try to narrow blamelists by using information across
  // bots. If a particular test has failed on multiple different
  // configurations, there's a good chance that it has the same root cause, so
  // calculate the intersection of blamelists of the first time it failed on
  // each builder.

  var allFailures = [];
  for (var i = 0; i < gWaterfallData.length; ++i) {
    var waterfallInfo = gWaterfallData[i];
    var botInfo = waterfallInfo.botInfo;
    var allBotNames = Object.keys(botInfo);
    for (var j = 0; j < allBotNames.length; ++j) {
      var botName = allBotNames[j];
      if (botInfo[botName].isSteadyGreen)
        continue;
      var builds = botInfo[botName].builds;
      var buildNames = Object.keys(builds);
      for (var k = 0; k < buildNames.length; ++k) {
        var build = builds[buildNames[k]];
        if (build.failures) {
          for (var l = 0; l < build.failures.length; ++l) {
            allFailures.push(build.failures[l]);
          }
        }
      }
    }
  }

  // allFailures is now a list of lists, each containing:
  // [ botname, binaryname, testname, [low_rev, high_rev] ].

  var rangesByTest = {};
  for (var i = 0; i < allFailures.length; ++i) {
    var failure = allFailures[i];
    var botName = failure[0];
    var binaryName = failure[1];
    var testName = failure[2];
    var range = failure[3];
    if (binaryName.indexOf('steps') != -1)
      continue;
    if (testName.indexOf('preamble') != -1)
      continue;
    var key = binaryName + '-' + testName;

    if (!rangesByTest.hasOwnProperty(key))
      rangesByTest[key] = {};
    // If there's no range, that's all we know.
    if (!rangesByTest[key].hasOwnProperty([botName])) {
      rangesByTest[key][botName] = range;
    } else {
      // Otherwise, track only the lowest range for this bot (we only want
      // when it turned red, not the whole range that it's been red for).
      if (range[0] < rangesByTest[key][botName][0]) {
        rangesByTest[key][botName] = range;
      }
    }
  }

  var table = document.getElementById('failure-info');
  while (table.rows.length > 0) {
    table.deleteRow(-1);
  }

  var headerCell = document.createElement('td');
  headerCell.colSpan = 15;
  headerCell.innerHTML =
      'test failures (ranges are blamelist intersections of first failure on ' +
      'each bot of retrieved data. so, if a test has been failing for a ' +
      'while, the range may be incorrect)';
  headerCell.className = 'section-header';
  var headerRow = table.insertRow(-1);
  headerRow.appendChild(headerCell);

  var flat = flattenAndSortTests(rangesByTest);

  for (var i = 0; i < flat.length; ++i) {
    table.appendChild(buildTestFailureTableRowHTML(flat[i]));
  }
}
