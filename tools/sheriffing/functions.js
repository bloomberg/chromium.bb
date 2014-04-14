// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Random constants. */
var MAX_BUILD_STATUS_LENGTH = 50;
var TICKS_BETWEEN_REFRESHES = 60;
var NUM_PREVIOUS_BUILDS_TO_SHOW = 3;
var MAX_MILLISECONDS_TO_WAIT = 5 * 60 * 1000;

/** Parsed JSON data. */
var gWaterfallData = [];
var gStatusData = [];
var gWaterfallDataIsDirty = true;

/** Global state. */
var gTicksUntilRefresh = TICKS_BETWEEN_REFRESHES;

/** Statistics. */
var gNumRequestsInFlight = 0;
var gNumRequestsIgnored = 0;
var gNumRequestsRetried = 0;
var gStartTimestamp = 0;

/** Cut the status message down so it doesn't hog the whole screen. */
function truncateStatusText(text) {
  if (text.length > MAX_BUILD_STATUS_LENGTH) {
    return text.substr(0, MAX_BUILD_STATUS_LENGTH) + '...';
  }
  return text;
}

/** Queries all of the servers for their latest statuses. */
function queryServersForInfo() {
  for (var index = 0; index < gWaterfallData.length; ++index) {
    gWaterfallData[index].requestJson();
  }

  for (var index = 0; index < gStatusData.length; ++index) {
    gStatusData[index].requestJson();
  }
}

/** Updates the sidebar's contents. */
function updateSidebarHTML() {
  // Update all of the project info.
  var divElement = document.getElementById('sidebar-contents');
  while (divElement.firstChild) {
    divElement.removeChild(divElement.firstChild);
  }

  for (var i = 0; i < gStatusData.length; ++i) {
    divElement.appendChild(gStatusData[i].createHtml());
  }

  // Debugging stats.
  document.getElementById('num-ticks-until-refresh').innerHTML =
      gTicksUntilRefresh;
  document.getElementById('num-requests-in-flight').innerHTML =
      gNumRequestsInFlight;
  document.getElementById('num-requests-ignored').innerHTML =
      gNumRequestsIgnored;
  document.getElementById('num-requests-retried').innerHTML =
      gNumRequestsRetried;
}

/**
 * Organizes all of the bots by category, then alphabetically within their
 * categories.
 */
function sortBotNamesByCategory(botInfo) {
  // Bucket all of the bots according to their category.
  var allBotNames = Object.keys(botInfo);
  var bucketedNames = {};
  for (var i = 0; i < allBotNames.length; ++i) {
    var botName = allBotNames[i];
    var category = botInfo[botName].category;

    if (!bucketedNames[category]) bucketedNames[category] = [];
    bucketedNames[category].push(botName);
  }

  // Alphabetically sort bots within their buckets, then append them to the
  // current list.
  var sortedBotNames = [];
  var allCategories = Object.keys(bucketedNames);
  allCategories.sort();
  for (var i = 0; i < allCategories.length; ++i) {
    var category = allCategories[i];
    var bucketBots = bucketedNames[category];
    bucketBots.sort();

    for (var j = 0; j < bucketBots.length; ++j) {
      sortedBotNames.push(bucketBots[j]);
    }
  }

  return sortedBotNames;
}

/** Update all the waterfall data. */
function updateStatusHTML() {
  var table = document.getElementById('build-info');
  while (table.rows.length > 0) {
    table.deleteRow(-1);
  }

  for (var i = 0; i < gWaterfallData.length; ++i) {
    gWaterfallData[i].updateWaterfallStatusHTML();
  }
}

/** Marks the waterfall data as dirty due to updated filter. */
function filterUpdated() {
  gWaterfallDataIsDirty = true;
}

/** Update the page content. */
function updateContent() {
  if (--gTicksUntilRefresh <= 0) {
    gTicksUntilRefresh = TICKS_BETWEEN_REFRESHES;
    queryServersForInfo();
  }

  // Redraw the page content.
  if (gWaterfallDataIsDirty) {
    gWaterfallDataIsDirty = false;
    updateStatusHTML();

    if (document.getElementById('failure-info')) {
      updateCorrelationsHTML();
    }
  }
  updateSidebarHTML();
}

/** Initialize all the things. */
function initialize() {
  var gStartTimestamp = new Date().getTime();

  // Initialize the waterfall pages.
  for (var i = 0; i < kBuilderPages.length; ++i) {
    gWaterfallData.push(new WaterfallInfo(kBuilderPages[i]));
  }

  // Initialize the status pages.
  for (var i = 0; i < kStatusPages.length; ++i) {
    gStatusData.push(new StatusPageInfo(kStatusPages[i][0],
                                        kStatusPages[i][1]));
  }

  // Kick off the main loops.
  queryServersForInfo();
  updateStatusHTML();
  setInterval('updateContent()', 1000);
}
