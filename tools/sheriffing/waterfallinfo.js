// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Information about a particular waterfall. */
function WaterfallInfo(waterfallData) {
  var waterfallName = waterfallData[0];
  var waterfallUrl = waterfallData[1];
  var waterfallShowsAllBots = waterfallData[2];

  // Create a table cell that acts as a header for its bot section.
  var linkElement = document.createElement('a');
  linkElement.href = waterfallUrl;
  linkElement.innerHTML = waterfallName;
  var thElement = document.createElement('th');
  thElement.colSpan = 15;
  thElement.className = 'section-header';
  thElement.appendChild(linkElement);

  this.botInfo = {};
  this.inFlight = 0;
  this.name = waterfallName;
  this.showsAllBots = waterfallShowsAllBots;
  this.thElement = thElement;
  this.timeLastRequested = 0;
  this.rootJsonUrl = waterfallUrl + 'json/';
  this.url = waterfallUrl;
}

/** Send an asynchronous request to get the main waterfall's JSON. */
WaterfallInfo.prototype.requestJson = function() {
  if (this.inFlight) {
    var elapsed = new Date().getTime() - this.timeLastRequested;
    if (elapsed < MAX_MILLISECONDS_TO_WAIT) return;

    // A response was not received in a reasonable timeframe. Try again.
    this.inFlight--;
    gNumRequestsInFlight--;
    gNumRequestsRetried++;
  }

  this.inFlight++;
  this.timeLastRequested = new Date().getTime();
  gNumRequestsInFlight++;

  // Create the request and send it off.
  var waterfallInfo = this;
  var url = this.url + 'json/builders/';
  var request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.onreadystatechange = function() {
    if (request.readyState == 4 && request.status == 200) {
      waterfallInfo.parseJSON(JSON.parse(request.responseText));
    }
  };
  request.send(null);
};

/** Parse out the data received about the waterfall. */
WaterfallInfo.prototype.parseJSON = function(buildersJson) {
  this.inFlight--;
  gNumRequestsInFlight--;

  // Go through each builder on the waterfall and get the latest status.
  var builderNames = Object.keys(buildersJson);
  for (var i = 0; i < builderNames.length; ++i) {
    var builderName = builderNames[i];

    if (!this.showsAllBots && !this.shouldShowBot(builderName)) continue;

    // Prepare the bot info.
    var builderJson = buildersJson[builderName];
    if (!this.botInfo[builderName]) {
      this.botInfo[builderName] = new BotInfo(builderName,
                                              builderJson.category);
    }
    this.botInfo[builderName].update(this.rootJsonUrl, builderJson);
    gWaterfallDataIsDirty = true;
  }
};

/** Override this function to filter out particular bots. */
WaterfallInfo.prototype.shouldShowBot = function(builderName) {
  return true;
};

/** Updates the HTML. */
WaterfallInfo.prototype.updateWaterfallStatusHTML = function() {
  var table = document.getElementById('build-info');

  // Point at the waterfall.
  var headerCell = this.thElement;
  headerCell.className =
      'section-header' + (this.inFlight > 0 ? ' in-flight' : '');
  var headerRow = table.insertRow(-1);
  headerRow.appendChild(headerCell);

  // Print out useful bits about the bots.
  var botNames = sortBotNamesByCategory(this.botInfo);
  for (var i = 0; i < botNames.length; ++i) {
    var botName = botNames[i];
    var botInfo = this.botInfo[botName];
    var waterfallBaseUrl = this.url + 'builders/';

    var botRowElement = botInfo.createHtml(waterfallBaseUrl);

    // Determine whether we should apply keyword filter.
    var filter = document.getElementById('text-filter').value.trim();
    if (filter.length > 0) {
      var keywords = filter.split(' ');
      var buildNumbers = Object.keys(botInfo.builds);
      var matchesFilter = false;

      for (var x = 0; x < buildNumbers.length && !matchesFilter; ++x) {
        var buildStatus = botInfo.builds[buildNumbers[x]].statusText;
        for (var y = 0; y < keywords.length && !matchesFilter; ++y) {
          if (buildStatus.indexOf(keywords[y]) >= 0)
            matchesFilter = true;
        }
      }

      if (!matchesFilter)
        continue;
    }

    // If the user doesn't want to see completely green bots, hide it.
    var shouldHideStable =
        document.getElementById('checkbox-hide-stable').checked;
    if (shouldHideStable && botInfo.isSteadyGreen)
      continue;

    table.appendChild(botRowElement);
  }
};
