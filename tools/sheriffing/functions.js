// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Random constants. */
var kMaxBuildStatusLength = 50;
var kTicksBetweenRefreshes = 60;
var kHistoricBuilds = 4;
var kMaxMillisecondsToWait = 5 * 60 * 1000;

var g_ticks_until_refresh = kTicksBetweenRefreshes;

/** Parsed JSON data. */
var g_waterfall_data = {};
var g_status_data = {};
var g_waterfall_data_dirty = true;
var g_css_loading_rule = null;

/** Statistics. */
var g_requests_in_flight = 0;
var g_requests_ignored = 0;
var g_requests_retried = 0;
var g_start_time = 0;

/** Cut the status message down so it doesn't hog the whole screen. */
function truncateStatusText(text) {
  if (text.length > kMaxBuildStatusLength) {
    return text.substr(0, kMaxBuildStatusLength) + '...';
  }
  return text;
}

/** Information about a particular status page. */
function StatusPageInfo(status_page_url) {
  this.url = status_page_url + 'current?format=json';
  this.in_flight = 0;
  this.message = '';
  this.date = '';
  this.state = '';
}

/** Information about a particular waterfall. */
function WaterfallInfo(waterfall_name, waterfall_url) {
  var waterfall_link_element = document.createElement('a');
  waterfall_link_element.href = waterfall_url;
  waterfall_link_element.target = '_blank';
  waterfall_link_element.innerHTML = waterfall_name;

  var td_element = document.createElement('td');
  td_element.colSpan = 15;
  td_element.appendChild(waterfall_link_element);

  this.bot_info = {};
  this.url = waterfall_url;
  this.in_flight = 0;
  this.td_element = td_element;
  this.last_update = 0;
}

/** Information about a particular bot. */
function BotInfo(category) {
  // Sometimes the categories have digits at the beginning for ordering the
  // category.  Chop it off.
  if (category && category.length > 0) {
    var splitter_index = category.indexOf('|');
    if (splitter_index != -1) {
      category = category.substr(0, splitter_index);
    }
  }

  this.in_flight = 0;
  this.builds = {};
  this.category = category;
  this.build_numbers_running = null;
  this.is_steady_green = false;
  this.state = '';
  this.num_updates_offline = 0;
}

/** Get the revisions involved in a build. */
function getRevisionRange(json) {
  var lowest = parseInt(json.sourceStamp.changes[0].revision, 10);
  var highest = parseInt(json.sourceStamp.changes[0].revision, 10);
  for (var i = 1; i < json.sourceStamp.changes.length; ++i) {
    var rev = parseInt(json.sourceStamp.changes[i].revision, 10);
    if (rev < lowest)
      lowest = rev;
    if (rev > highest)
      highest = rev;
  }
  return [lowest, highest];
}

/** Information about a particular build. */
function BuildInfo(json) {
  // Parse out the status message for the build.
  var status_text;
  if (json.currentStep) {
    status_text = 'running ' + json.currentStep.name;
  } else {
    status_text = json.text.join(' ');
  }

  var truncated_status_text = truncateStatusText(status_text);

  // Determine what state the build is in.
  var state;
  if (status_text.indexOf('exception') != -1) {
    state = 'exception';
  } else if (status_text.indexOf('running') != -1) {
    state = 'running';
  } else if (status_text.indexOf('successful') != -1) {
    state = 'success';
  } else if (status_text.indexOf('failed') != -1) {
    state = 'failed';
  } else if (status_text.indexOf('offline') != -1) {
    state = 'offline';
  } else {
    state = 'unknown';
  }

  if (state == 'failed') {
    // Save data about failed tests and blamelist so we can do intersections.
    var failures = [];
    var revision_range = getRevisionRange(json);
    var bot_name = json.builderName;
    for (var i = 0; i < json.steps.length; ++i) {
      var step = json.steps[i];
      var binary_name = step.name;
      if (step.results[0] != 0) {  // Failed.
        for (var j = 0; j < step.logs.length; ++j) {
          var log = step.logs[j];
          if (log[0] == 'stdio')
            continue;
          var test_name = log[0];
          failures.push([bot_name, binary_name, test_name, revision_range]);
        }
      }
    }
    this.failures = failures;
  } else {
    this.failures = null;
  }

  this.status_text = status_text;
  this.truncated_status_text = truncated_status_text;
  this.state = state;
}

/** Send and parse an asynchronous request to get a repo status JSON. */
function requestStatusPageJSON(repo_name) {
  if (g_status_data[repo_name].in_flight) return;

  var request = new XMLHttpRequest();
  request.open('GET', g_status_data[repo_name].url, true);
  request.onreadystatechange = function() {
    if (request.readyState == 4 && request.status == 200) {
      g_status_data[repo_name].in_flight--;
      g_requests_in_flight--;

      status_page_json = JSON.parse(request.responseText);
      g_status_data[repo_name].message = status_page_json.message;
      g_status_data[repo_name].state = status_page_json.general_state;
      g_status_data[repo_name].date = status_page_json.date;
    }
  };
  g_status_data[repo_name].in_flight++;
  g_requests_in_flight++;
  request.send(null);
}

/** Send an asynchronous request to get the main waterfall's JSON. */
function requestWaterfallJSON(waterfall_info) {
  if (waterfall_info.in_flight) {
    var elapsed = new Date().getTime() - waterfall_info.last_update;
    if (elapsed < kMaxMillisecondsToWait) return;

    waterfall_info.in_flight--;
    g_requests_in_flight--;
    g_requests_retried++;
  }

  var waterfall_url = waterfall_info.url;
  var url = waterfall_url + 'json/builders/';

  var request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.onreadystatechange = function() {
    if (request.readyState == 4 && request.status == 200) {
      waterfall_info.in_flight--;
      g_requests_in_flight--;
      parseWaterfallJSON(JSON.parse(request.responseText), waterfall_info);
    }
  };
  waterfall_info.in_flight++;
  g_requests_in_flight++;
  waterfall_info.last_update = new Date().getTime();
  request.send(null);
}

/** Update info about the bot, including info about the builder's builds. */
function requestBuilderJSON(waterfall_info,
                            root_json_url,
                            builder_json,
                            builder_name) {
  // Prepare the bot info.
  if (!waterfall_info.bot_info[builder_name]) {
    waterfall_info.bot_info[builder_name] = new BotInfo(builder_json.category);
  }
  var bot_info = waterfall_info.bot_info[builder_name];

  // Update the builder's state.
  bot_info.build_numbers_running = builder_json.currentBuilds;
  bot_info.num_pending_builds = builder_json.pendingBuilds;
  bot_info.state = builder_json.state;
  if (bot_info.state == 'offline') {
    bot_info.num_updates_offline++;
    console.log(builder_name + ' has been offline for ' +
                bot_info.num_updates_offline + ' update(s) in a row');
  } else {
    bot_info.num_updates_offline = 0;
  }

  // Send an asynchronous request to get info about the builder's last builds.
  var last_completed_build_number =
      guessLastCompletedBuildNumber(builder_json, bot_info);
  if (last_completed_build_number) {
    for (var build_number = last_completed_build_number - kHistoricBuilds;
         build_number <= last_completed_build_number;
         ++build_number) {
      if (build_number < 0) continue;
      requestBuildJSON(waterfall_info,
                       builder_name,
                       root_json_url,
                       builder_json,
                       build_number);
    }
  }
}

/** Given a builder's JSON, guess the last known build that it completely
 * finished. */
function guessLastCompletedBuildNumber(builder_json, bot_info) {
  // The cached builds line doesn't store every build so we can't just take the
  // last number.
  var build_numbers_running = builder_json.currentBuilds;
  bot_info.build_numbers_running = build_numbers_running;

  var build_numbers_cached = builder_json.cachedBuilds;
  if (build_numbers_running && build_numbers_running.length > 0) {
    max_build_number =
        Math.max(build_numbers_cached[build_numbers_cached.length - 1],
                 build_numbers_running[build_numbers_running.length - 1]);

    var completed_build_number = max_build_number;
    while (build_numbers_running.indexOf(completed_build_number) != -1 &&
           completed_build_number >= 0) {
      completed_build_number--;
    }
    return completed_build_number;
  } else {
    // Nothing's currently building.  Just assume the last cached build is
    // correct.
    return build_numbers_cached[build_numbers_cached.length - 1];
  }
}

/** Get the data for a particular build. */
function requestBuildJSON(waterfall_info,
                          builder_name,
                          root_json_url,
                          builder_json,
                          build_number) {
  var bot_info = waterfall_info.bot_info[builder_name];

  // Check if we already have the data on this build.
  if (bot_info.builds[build_number] &&
      bot_info.builds[build_number].state != 'running') {
    g_requests_ignored++;
    return;
  }

  // Grab it.
  var url =
      root_json_url + 'builders/' + builder_name + '/builds/' + build_number;

  var request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.onreadystatechange = function() {
    if (request.readyState == 4 && request.status == 200) {
      bot_info.in_flight--;
      g_requests_in_flight--;

      var json = JSON.parse(request.responseText);
      bot_info.builds[build_number] = new BuildInfo(json);

      checkBotIsSteadyGreen(builder_name, bot_info);
      g_waterfall_data_dirty = true;
    }
  };

  bot_info.in_flight++;
  g_requests_in_flight++;
  request.send(null);
}

function parseWaterfallJSON(builders_json, waterfall_info) {
  var root_json_url = waterfall_info.url + 'json/';

  // Go through each builder on the waterfall and get the latest status.
  var builder_names = Object.keys(builders_json);
  for (var i = 0; i < builder_names.length; ++i) {
    var builder_name = builder_names[i];

    // TODO: Any filtering here.

    var builder_json = builders_json[builder_name];
    requestBuilderJSON(
        waterfall_info, root_json_url, builder_json, builder_name);
    g_waterfall_data_dirty = true;
  }
}

/** Queries all of the servers for their latest statuses. */
function queryServersForInfo() {
  var waterfall_names = Object.keys(g_waterfall_data);
  for (var index = 0; index < waterfall_names.length; ++index) {
    var name = waterfall_names[index];
    var waterfall_info = g_waterfall_data[name];
    requestWaterfallJSON(waterfall_info);
  }

  var status_page_names = Object.keys(g_status_data);
  for (var index = 0; index < status_page_names.length; ++index) {
    requestStatusPageJSON(status_page_names[index]);
  }
}

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

function findIntersections(test_data) {
  // Input is a object mapping botname => [low, high].
  //
  // Result:
  // [
  //   [ low0, high0 ], [ bot1, bot2, bot3 ],
  //   [ low1, high1 ], [ bot4, ... ],
  //   ...,
  // ]

  var keys = Object.keys(test_data);
  var intersections = [];
  var bot_name = keys[0];
  var range = test_data[bot_name];
  intersections.push([range, [bot_name]]);
  for (var i = 1; i < keys.length; ++i) {
    bot_name = keys[i];
    range = test_data[bot_name];
    var intersected_some = false;
    for (var j = 0; j < intersections.length; ++j) {
      var intersect = rangeIntersection(intersections[j][0], range);
      if (intersect) {
        intersections[j][0] = intersect;
        intersections[j][1].push(bot_name);
        intersected_some = true;
        break;
      }
    }
    if (!intersected_some) {
      intersections.push([range, [bot_name]]);
    }
  }

  return intersections;
}

/** Flatten out the list of tests and sort them by the binary/test name. */
function flattenAndSortTests(ranges_by_test) {
  var ranges_by_test_names = Object.keys(ranges_by_test);
  var flat = [];
  for (var i = 0; i < ranges_by_test_names.length; ++i) {
    var name = ranges_by_test_names[i];
    flat.push([name, ranges_by_test[name]]);
  }

  flat.sort(function(a, b) {
    if (a[0] < b[0]) return -1;
    if (a[0] > b[0]) return 1;
    return 0;
  });

  return flat;
}

/**
 * Build the HTML table row for a test failure. |test| is [ name, test_data ].
 * |test_data| contains ranges suitable for input to |findIntersections|.
 */
function buildTestFailureTableRowHTML(test) {
  var row = document.createElement('tr');
  var binary_cell = row.insertCell(-1);
  var name_parts = test[0].split('-');
  binary_cell.innerHTML = name_parts[0];
  binary_cell.className = 'category';
  var flakiness_link = document.createElement('a');
  flakiness_link.href =
      'http://test-results.appspot.com/dashboards/' +
      'flakiness_dashboard.html#testType=' +
      name_parts[0] + '&tests=' + name_parts[1];
  flakiness_link.target = '_blank';
  flakiness_link.innerHTML = name_parts[1];
  row.appendChild(flakiness_link);

  var intersections = findIntersections(test[1]);
  for (var j = 0; j < intersections.length; ++j) {
    var intersection = intersections[j];
    var range = row.insertCell(-1);
    range.className = 'failure_range';
    var low = intersection[0][0];
    var high = intersection[0][1];
    var url =
        'http://build.chromium.org/f/chromium/perf/dashboard/ui/' +
        'changelog.html?url=%2Ftrunk%2Fsrc&range=' +
        low + '%3A' + high + '&mode=html';
    range.innerHTML = '<a target="_blank" href="' + url + '">' + low +
                      ' - ' + high + '</a>: ' +
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

  var all_failures = [];
  for (var i = 0; i < kBuilderPages.length; ++i) {
    var waterfall_name = kBuilderPages[i][0];
    var waterfall_info = g_waterfall_data[waterfall_name];
    var bot_info = waterfall_info.bot_info;
    var all_bot_names = Object.keys(bot_info);
    for (var j = 0; j < all_bot_names.length; ++j) {
      var bot_name = all_bot_names[j];
      if (bot_info[bot_name].is_steady_green)
        continue;
      var builds = bot_info[bot_name].builds;
      var build_names = Object.keys(builds);
      for (var k = 0; k < build_names.length; ++k) {
        var build = builds[build_names[k]];
        if (build.failures) {
          for (var l = 0; l < build.failures.length; ++l) {
            all_failures.push(build.failures[l]);
          }
        }
      }
    }
  }

  // all_failures is now a list of lists, each containing:
  // [ botname, binaryname, testname, [low_rev, high_rev] ].

  var ranges_by_test = {};
  for (var i = 0; i < all_failures.length; ++i) {
    var failure = all_failures[i];
    var bot_name = failure[0];
    var binary_name = failure[1];
    var test_name = failure[2];
    var range = failure[3];
    if (binary_name.indexOf('steps') != -1)
      continue;
    if (test_name.indexOf('preamble') != -1)
      continue;
    var key = binary_name + '-' + test_name;

    if (!ranges_by_test.hasOwnProperty(key))
      ranges_by_test[key] = {};
    // If there's no range, that's all we know.
    if (!ranges_by_test[key].hasOwnProperty([bot_name])) {
      ranges_by_test[key][bot_name] = range;
    } else {
      // Otherwise, track only the lowest range for this bot (we only want
      // when it turned red, not the whole range that it's been red for).
      if (range[0] < ranges_by_test[key][bot_name][0]) {
        ranges_by_test[key][bot_name] = range;
      }
    }
  }

  var table = document.getElementById('failure_info');
  while (table.rows.length > 0) {
    table.deleteRow(-1);
  }

  var header_cell = document.createElement('td');
  header_cell.colSpan = 15;
  header_cell.innerHTML =
      'test failures (ranges are blamelist intersections of first failure on ' +
      'each bot of retrieved data. so, if a test has been failing for a ' +
      'while, the range may be incorrect)';
  header_cell.className = 'waterfall_name';
  var header_row = table.insertRow(-1);
  header_row.appendChild(header_cell);

  var flat = flattenAndSortTests(ranges_by_test);

  for (var i = 0; i < flat.length; ++i) {
    table.appendChild(buildTestFailureTableRowHTML(flat[i]));
  }
}

/** Updates the sidebar's contents. */
function updateSidebarHTML() {
  var output = '';

  // Buildbot info.
  var status_names = Object.keys(g_status_data);
  for (var i = 0; i < status_names.length; ++i) {
    var status_name = status_names[i];
    var status_info = g_status_data[status_name];
    var status_url = kStatusPages[i][1];

    output += '<ul class="box ' + status_info.state + '">';
    output += '<li><a target="_blank" href="' + status_url + '">' +
              status_name + '</a></li>';
    output += '<li>' + status_info.date + '</li>';
    output += '<li>' + status_info.message + '</li>';
    output += '</ul>';
  }

  var div = document.getElementById('sidebar_contents');
  div.innerHTML = output;

  // Debugging stats.
  document.getElementById('ticks_until_refresh').innerHTML =
      g_ticks_until_refresh;
  document.getElementById('requests_in_flight').innerHTML =
      g_requests_in_flight;
  document.getElementById('requests_ignored').innerHTML = g_requests_ignored;
  document.getElementById('requests_retried').innerHTML = g_requests_retried;
}

/** Organizes all of the bots by category, then alphabetically within their
 * categories. */
function sortBotNamesByCategory(bot_info) {
  // Bucket all of the bots according to their category.
  var all_bot_names = Object.keys(bot_info);
  var bucketed_names = {};
  for (var i = 0; i < all_bot_names.length; ++i) {
    var bot_name = all_bot_names[i];
    var category = bot_info[bot_name].category;

    if (!bucketed_names[category]) bucketed_names[category] = [];
    bucketed_names[category].push(bot_name);
  }

  // Alphabetically sort bots within their buckets, then append them to the
  // current list.
  var sorted_bot_names = [];
  var all_categories = Object.keys(bucketed_names);
  all_categories.sort();
  for (var i = 0; i < all_categories.length; ++i) {
    var category = all_categories[i];
    var bucket_bots = bucketed_names[category];
    bucket_bots.sort();

    for (var j = 0; j < bucket_bots.length; ++j) {
      sorted_bot_names.push(bucket_bots[j]);
    }
  }

  return sorted_bot_names;
}

/**
 * Returns true IFF the last few builds are all green.
 * Also alerts the user if the last completed build goes red after being
 * steadily green (if desired).
 */
function checkBotIsSteadyGreen(bot_name, bot_info) {
  var ascending_build_numbers = Object.keys(bot_info.builds);
  ascending_build_numbers.sort();

  for (var j = ascending_build_numbers.length - 1;
       j >= 0 && j >= ascending_build_numbers.length - 1 - kHistoricBuilds;
       --j) {
    var build_number = ascending_build_numbers[j];
    if (!build_number) continue;

    var build_info = bot_info.builds[build_number];
    if (!build_info) continue;

    // Running builds throw heuristics out of whack.  Keep the bot visible.
    if (build_info.state == 'running') return false;

    if (build_info.state != 'success') {
      if (bot_info.is_steady_green &&
          document.getElementById('checkbox_alert_steady_red').checked) {
        alert(bot_name +
              ' has failed for the first time in a while. Consider looking.');
      }
      bot_info.is_steady_green = false;
      return false;
    }
  }

  bot_info.is_steady_green = true;
  return true;
}

/** Update all the waterfall data. */
function updateStatusHTML() {
  var table = document.getElementById('build_info');
  while (table.rows.length > 0) {
    table.deleteRow(-1);
  }

  for (var i = 0; i < kBuilderPages.length; ++i) {
    var waterfall_name = kBuilderPages[i][0];
    var waterfall_info = g_waterfall_data[waterfall_name];
    updateWaterfallStatusHTML(waterfall_name, waterfall_info);
  }
}

/** Marks the waterfall data as dirty due to updated filter. */
function filterUpdated() {
  g_waterfall_data_dirty = true;
}

/** Creates a table cell for a particular build number. */
function createBuildNumberCellHTML(build_url,
                                   build_number,
                                   full_status_text,
                                   truncated_status_text,
                                   build_state) {
  // Create a link to the build results.
  var link_element = document.createElement('a');
  link_element.href = build_url;
  link_element.target = '_blank';

  // Display either the build number (for the last completed build), or show the
  // status of the step.
  var build_identifier_element = document.createElement('span');
  if (build_number) {
    build_identifier_element.className = 'build_identifier';
    build_identifier_element.innerHTML = build_number;
  } else {
    build_identifier_element.className = 'build_letter';
    build_identifier_element.innerHTML = build_state.toUpperCase()[0];
  }
  link_element.appendChild(build_identifier_element);

  // Show the status of the build, in truncated form so it doesn't take up the
  // whole screen.
  if (truncated_status_text) {
    var status_span_element = document.createElement('span');
    status_span_element.className = 'build_status';
    status_span_element.innerHTML = truncated_status_text;
    link_element.appendChild(status_span_element);
  }

  // Tack the cell onto the end of the row.
  var build_number_cell = document.createElement('td');
  build_number_cell.className = build_state;
  build_number_cell.title = full_status_text;
  build_number_cell.appendChild(link_element);

  return build_number_cell;
}

/** Updates the HTML for a particular waterfall. */
function updateWaterfallStatusHTML(waterfall_name, waterfall_info) {
  var table = document.getElementById('build_info');

  // Point at the waterfall.
  var header_cell = waterfall_info.td_element;
  header_cell.className =
      'waterfall_name' + (waterfall_info.in_flight > 0 ? ' in_flight' : '');
  var header_row = table.insertRow(-1);
  header_row.appendChild(header_cell);

  // Print out useful bits about the bots.
  var bot_names = sortBotNamesByCategory(waterfall_info.bot_info);
  for (var i = 0; i < bot_names.length; ++i) {
    var bot_name = bot_names[i];
    var bot_info = waterfall_info.bot_info[bot_name];
    var bot_row = document.createElement('tr');

    // Insert a cell for the bot category.  Chop off any numbers used for
    // sorting.
    var category_cell = bot_row.insertCell(-1);
    var category = bot_info.category;
    if (category && category.length > 0 && category[0] >= '0' &&
        category[0] <= '9') {
      category = category.substr(1, category.length);
    }
    category_cell.innerHTML = category;
    category_cell.className = 'category';

    // Insert a cell for the bot name.
    var bot_cell = bot_row.insertCell(-1);
    var builder_url = waterfall_info.url + 'builders/' + bot_name;
    var bot_link =
        '<a target="_blank" href="' + builder_url + '">' + bot_name + '</a>';
    bot_cell.innerHTML = bot_link;
    bot_cell.className =
        'bot_name' + (bot_info.in_flight > 0 ? ' in_flight' : '');

    // Display information on the builds we have.
    // This assumes that the build number always increases, but this is a bad
    // assumption since
    // builds get parallelized.
    var build_numbers = Object.keys(bot_info.builds);
    build_numbers.sort();

    if (bot_info.num_pending_builds) {
      var pending_cell = document.createElement('span');
      pending_cell.className = 'pending_count';
      pending_cell.innerHTML = '+' + bot_info.num_pending_builds;
      bot_row.appendChild(pending_cell);
    } else {
      bot_row.insertCell(-1);
    }

    if (bot_info.build_numbers_running &&
        bot_info.build_numbers_running.length > 0) {
      // Display the number of the highest numbered running build.
      bot_info.build_numbers_running.sort();
      var length = bot_info.build_numbers_running.length;
      var build_number = bot_info.build_numbers_running[length - 1];
      var build_url = builder_url + '/builds/' + build_number;
      var build_number_cell = createBuildNumberCellHTML(
          build_url, build_number, null, null, 'running');
      bot_row.appendChild(build_number_cell);
    } else if (bot_info.state == 'offline' &&
               bot_info.num_updates_offline >= 3) {
      // The bot's supposedly offline.  Wait a few updates to see since a
      // builder can be marked offline in between builds and during reboots.
      var build_number_cell = document.createElement('td');
      build_number_cell.className = bot_info.state + ' build_identifier';
      build_number_cell.innerHTML = 'offline';
      bot_row.appendChild(build_number_cell);
    } else {
      bot_row.insertCell(-1);
    }

    // Display the last few builds.
    for (var j = build_numbers.length - 1;
         j >= 0 && j >= build_numbers.length - 1 - kHistoricBuilds;
         --j) {
      var build_number = build_numbers[j];
      if (!build_number) continue;

      var build_info = bot_info.builds[build_number];
      if (!build_info) continue;

      // Create and append the cell.
      var is_last_build = (j == build_numbers.length - 1);
      var build_url = builder_url + '/builds/' + build_number;
      var status_text_full =
          'Build ' + build_number + ':\n' + build_info.status_text;
      var build_number_cell = createBuildNumberCellHTML(
          build_url,
          is_last_build ? build_number : null,
          status_text_full,
          is_last_build ? build_info.truncated_status_text : null,
          build_info.state);
      bot_row.appendChild(build_number_cell);
    }

    // Determine whether we should apply keyword filter.
    var filter = document.getElementById('text_filter').value.trim();
    if (filter.length > 0) {
      var keywords = filter.split(' ');
      var build_numbers = Object.keys(bot_info.builds);
      var matches_filter = false;
      console.log(bot_info);

      for (var x = 0; x < build_numbers.length && !matches_filter; ++x) {
        var build_status = bot_info.builds[build_numbers[x]].status_text;
        console.log(build_status);

        for (var y = 0; y < keywords.length && !matches_filter; ++y) {
          if (build_status.indexOf(keywords[y]) >= 0)
            matches_filter = true;
        }
      }

      if (!matches_filter)
        continue;
    }

    // If the user doesn't want to see completely green bots, hide it.
    var should_hide_stable =
        document.getElementById('checkbox_hide_stable').checked;
    if (should_hide_stable && bot_info.is_steady_green)
      continue;

    table.appendChild(bot_row);
  }
}

/** Update the page content. */
function updateContent() {
  if (--g_ticks_until_refresh <= 0) {
    g_ticks_until_refresh = kTicksBetweenRefreshes;
    queryServersForInfo();
  }

  // Redraw the page content.
  if (g_waterfall_data_dirty) {
    g_waterfall_data_dirty = false;
    updateStatusHTML();
    updateCorrelationsHTML();
  }
  updateSidebarHTML();
}

/** Initialize all the things. */
function initialize() {
  var g_start_time = new Date().getTime();

  // Initialize the waterfall pages.
  for (var i = 0; i < kBuilderPages.length; ++i) {
    g_waterfall_data[kBuilderPages[i][0]] =
        new WaterfallInfo(kBuilderPages[i][0], kBuilderPages[i][1]);
  }

  // Initialize the status pages.
  for (var i = 0; i < kStatusPages.length; ++i) {
    g_status_data[kStatusPages[i][0]] = new StatusPageInfo(kStatusPages[i][1]);
  }

  // Set up the useful links HTML in the sidebar.
  var useful_links_ul = document.getElementById('useful_links');
  for (var i = 0; i < kLinks.length; ++i) {
    var link_html = '<a target="_blank" href="' + kLinks[i][1] + '">' +
                    kLinks[i][0] + '</a>';
    var li_element = document.createElement('li');
    li_element.innerHTML = link_html;
    useful_links_ul.appendChild(li_element);
  }

  // Kick off the main loops.
  queryServersForInfo();
  updateStatusHTML();
  setInterval('updateContent()', 1000);
}
