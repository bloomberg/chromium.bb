// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Information about a particular bot. */
function BotInfo(name, category) {
  // Chop off any digits at the beginning of category names.
  if (category && category.length > 0) {
    var splitterIndex = category.indexOf('|');
    if (splitterIndex != -1) {
      category = category.substr(0, splitterIndex);
    }

    while (category[0] >= '0' && category[0] <= '9') {
      category = category.substr(1, category.length);
    }
  }

  this.buildNumbersRunning = null;
  this.builds = {};
  this.category = category;
  this.inFlight = 0;
  this.isSteadyGreen = false;
  this.name = name;
  this.numUpdatesOffline = 0;
  this.state = '';
}

/** Update info about the bot, including info about the builder's builds. */
BotInfo.prototype.update = function(rootJsonUrl, builderJson) {
  // Update the builder's state.
  this.buildNumbersRunning = builderJson.currentBuilds;
  this.numPendingBuilds = builderJson.pendingBuilds;
  this.state = builderJson.state;

  // Check if an offline bot is still offline.
  if (this.state == 'offline') {
    this.numUpdatesOffline++;
    console.log(this.name + ' has been offline for ' +
                this.numUpdatesOffline + ' update(s) in a row');
  } else {
    this.numUpdatesOffline = 0;
  }

  // Send asynchronous requests to get info about the builder's last builds.
  var lastCompletedBuildNumber =
      this.guessLastCompletedBuildNumber(builderJson);
  if (lastCompletedBuildNumber) {
    var startNumber = lastCompletedBuildNumber - NUM_PREVIOUS_BUILDS_TO_SHOW;
    for (var buildNumber = startNumber;
         buildNumber <= lastCompletedBuildNumber;
         ++buildNumber) {
      if (buildNumber < 0) continue;

      // Use cached state after the builder indicates that it has finished.
      if (this.builds[buildNumber] &&
          this.builds[buildNumber].state != 'running') {
        gNumRequestsIgnored++;
        continue;
      }

      this.requestJson(rootJsonUrl, buildNumber);
    }
  }
};

/** Request and save data about a particular build. */
BotInfo.prototype.requestJson = function(rootJsonUrl, buildNumber) {
  this.inFlight++;
  gNumRequestsInFlight++;

  var botInfo = this;
  var url = rootJsonUrl + 'builders/' + this.name + '/builds/' + buildNumber;
  var request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.onreadystatechange = function() {
    if (request.readyState == 4 && request.status == 200) {
      botInfo.inFlight--;
      gNumRequestsInFlight--;

      var json = JSON.parse(request.responseText);
      botInfo.builds[json.number] = new BuildInfo(json);
      botInfo.updateIsSteadyGreen();
      gWaterfallDataIsDirty = true;
    }
  };
  request.send(null);
};

/** Guess the last known build a builder finished. */
BotInfo.prototype.guessLastCompletedBuildNumber = function(builderJson) {
  // The cached builds line doesn't store every build so we can't just take the
  // last number.
  var buildNumbersRunning = builderJson.currentBuilds;
  this.buildNumbersRunning = buildNumbersRunning;

  var buildNumbersCached = builderJson.cachedBuilds;
  if (buildNumbersRunning && buildNumbersRunning.length > 0) {
    var maxBuildNumber =
        Math.max(buildNumbersCached[buildNumbersCached.length - 1],
                 buildNumbersRunning[buildNumbersRunning.length - 1]);

    var completedBuildNumber = maxBuildNumber;
    while (buildNumbersRunning.indexOf(completedBuildNumber) != -1 &&
           completedBuildNumber >= 0) {
      completedBuildNumber--;
    }
    return completedBuildNumber;
  } else {
    // Nothing's currently building.  Assume the last cached build is correct.
    return buildNumbersCached[buildNumbersCached.length - 1];
  }
};

/**
 * Returns true IFF the last few builds are all green.
 * Also alerts the user if the last completed build goes red after being
 * steadily green (if desired).
 */
BotInfo.prototype.updateIsSteadyGreen = function() {
  var ascendingBuildNumbers = Object.keys(this.builds);
  ascendingBuildNumbers.sort();

  var lastNumber =
      ascendingBuildNumbers.length - 1 - NUM_PREVIOUS_BUILDS_TO_SHOW;
  for (var j = ascendingBuildNumbers.length - 1;
       j >= 0 && j >= lastNumber;
       --j) {
    var buildNumber = ascendingBuildNumbers[j];
    if (!buildNumber) continue;

    var buildInfo = this.builds[buildNumber];
    if (!buildInfo) continue;

    // Running builds throw heuristics out of whack.  Keep the bot visible.
    if (buildInfo.state == 'running') return false;

    if (buildInfo.state != 'success') {
      if (this.isSteadyGreen &&
          document.getElementById('checkbox-alert-steady-red').checked) {
        alert(this.name +
              ' has failed for the first time in a while. Consider looking.');
      }
      this.isSteadyGreen = false;
      return;
    }
  }

  this.isSteadyGreen = true;
  return;
};

/** Creates HTML elements to display info about this bot. */
BotInfo.prototype.createHtml = function(waterfallBaseUrl) {
  var botRowElement = document.createElement('tr');

  // Insert a cell for the bot category.
  var categoryCellElement = botRowElement.insertCell(-1);
  categoryCellElement.innerHTML = this.category;
  categoryCellElement.className = 'category';

  // Insert a cell for the bot name.
  var botUrl = waterfallBaseUrl + this.name;
  var botElement = document.createElement('a');
  botElement.href = botUrl;
  botElement.innerHTML = this.name;

  var nameCell = botRowElement.insertCell(-1);
  nameCell.appendChild(botElement);
  nameCell.className = 'bot-name' + (this.inFlight > 0 ? ' in-flight' : '');

  // Create a cell to show how many CLs are waiting for a build.
  var pendingCell = botRowElement.insertCell(-1);
  pendingCell.className = 'pending-count';
  pendingCell.title = 'Pending builds: ' + this.numPendingBuilds;
  if (this.numPendingBuilds) {
    pendingCell.innerHTML = '+' + this.numPendingBuilds;
  }

  // Create a cell to indicate what the bot is currently doing.
  var runningElement = botRowElement.insertCell(-1);
  if (this.buildNumbersRunning && this.buildNumbersRunning.length > 0) {
    // Display the number of the highest numbered running build.
    this.buildNumbersRunning.sort();
    var numRunning = this.buildNumbersRunning.length;
    var buildNumber = this.buildNumbersRunning[numRunning - 1];
    var buildUrl = botUrl + '/builds/' + buildNumber;
    createBuildHtml(runningElement,
                    buildUrl,
                    buildNumber,
                    'Builds running: ' + numRunning,
                    null,
                    'running');
  } else if (this.state == 'offline' && this.numUpdatesOffline >= 3) {
    // The bot's supposedly offline. Waits a few updates since a bot can be
    // marked offline in between builds and during reboots.
    createBuildHtml(runningElement,
                    botUrl,
                    'offline',
                    'Offline for: ' + this.numUpdatesOffline,
                    null,
                    'offline');
  }

  // Display information on the builds we have.
  // This assumes that the build number always increases, but this is a bad
  // assumption since builds get parallelized.
  var buildNumbers = Object.keys(this.builds);
  buildNumbers.sort();
  for (var j = buildNumbers.length - 1;
       j >= 0 && j >= buildNumbers.length - 1 - NUM_PREVIOUS_BUILDS_TO_SHOW;
       --j) {
    var buildNumber = buildNumbers[j];
    if (!buildNumber) continue;

    var buildInfo = this.builds[buildNumber];
    if (!buildInfo) continue;

    var buildNumberCell = botRowElement.insertCell(-1);
    var isLastBuild = (j == buildNumbers.length - 1);

    // Create and append the cell.
    this.builds[buildNumber].createHtml(buildNumberCell, botUrl, isLastBuild);
  }

  return botRowElement;
};
