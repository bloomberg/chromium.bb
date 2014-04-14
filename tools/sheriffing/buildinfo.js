// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Information about a particular build. */
function BuildInfo(json) {
  // Parse out the status message for the build.
  var statusText;
  if (json.currentStep) {
    statusText = 'running ' + json.currentStep.name;
  } else {
    statusText = json.text.join(' ');
  }

  // Determine what state the build is in.
  var state;
  if (statusText.indexOf('exception') != -1) {
    state = 'exception';
  } else if (statusText.indexOf('running') != -1) {
    state = 'running';
  } else if (statusText.indexOf('successful') != -1) {
    state = 'success';
  } else if (statusText.indexOf('failed') != -1) {
    state = 'failed';
  } else if (statusText.indexOf('offline') != -1) {
    state = 'offline';
  } else if (statusText.indexOf('warnings') != -1) {
    state = 'warnings';
  } else {
    state = 'unknown';
  }

  var failures = (state == 'failed') ? this.parseFailures(json) : null;

  this.number = json.number;
  this.state = state;
  this.failures = failures;
  this.statusText = statusText;
  this.truncatedStatusText = truncateStatusText(statusText);
}

/** Save data about failed tests to perform blamelist intersections. */
BuildInfo.prototype.parseFailures = function(json) {
  var revisionRange = this.getRevisionRange(json);
  if (revisionRange == null) return null;

  var failures = [];
  var botName = json.builderName;
  for (var i = 0; i < json.steps.length; ++i) {
    var step = json.steps[i];
    var binaryName = step.name;
    if (step.results[0] != 0) {  // Failed.
      for (var j = 0; j < step.logs.length; ++j) {
        var log = step.logs[j];
        if (log[0] == 'stdio')
          continue;
        var testName = log[0];
        failures.push([botName, binaryName, testName, revisionRange]);
      }
    }
  }

  return failures;
};

/**
 * Get the revisions involved in a build.  Sadly, this only works on Chromium's
 * main builders because downstream trees provide git revision SHA1s through
 * JSON instead of SVN numbers.
 */
BuildInfo.prototype.getRevisionRange = function(json) {
  if (json.sourceStamp.changes.length == 0) {
    return null;
  }

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
};

/** Creates HTML to display info about this build. */
BuildInfo.prototype.createHtml = function(buildNumberCell,
                                          botUrl,
                                          showFullInfo) {
  var fullStatusText = 'Build ' + this.number + ':\n' + this.statusText;
  createBuildHtml(buildNumberCell,
                  botUrl + '/builds/' + this.number,
                  showFullInfo ? this.number : null,
                  fullStatusText,
                  showFullInfo ? this.truncatedStatusText : null,
                  this.state);
};

/** Creates a table cell for a particular build number. */
function createBuildHtml(cellElement,
                         url,
                         buildNumber,
                         fullStatusText,
                         truncatedStatusText,
                         buildState) {
  // Create a link to the build results.
  var linkElement = document.createElement('a');
  linkElement.href = url;

  // Display either the build number (for the last completed build), or show the
  // status of the step.
  var buildIdentifierElement = document.createElement('span');
  if (buildNumber) {
    buildIdentifierElement.className = 'build-identifier';
    buildIdentifierElement.innerHTML = buildNumber;
  } else {
    buildIdentifierElement.className = 'build-letter';
    buildIdentifierElement.innerHTML = buildState.toUpperCase()[0];
  }
  linkElement.appendChild(buildIdentifierElement);

  // Show the status of the build in truncated form so it doesn't take up the
  // whole screen.
  if (truncatedStatusText) {
    var statusElement = document.createElement('span');
    statusElement.className = 'build-status';
    statusElement.innerHTML = truncatedStatusText;
    linkElement.appendChild(statusElement);
  }

  // Tack the cell onto the end of the row.
  cellElement.className = buildState;
  cellElement.title = fullStatusText;
  cellElement.appendChild(linkElement);
}
