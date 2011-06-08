// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maximum numer of lines to record in the debug log.
// Only the most recent <n> lines are displayed.
var MAX_DEBUG_LOG_SIZE = 1000;

function toggleDebugLog() {
  debugLog = document.getElementById('debug-log');
  toggleButton = document.getElementById('debug-log-toggle');

  if (!debugLog.style.display || debugLog.style.display == 'none') {
    debugLog.style.display = 'block';
    toggleButton.value = 'Hide Debug Log';
  } else {
    debugLog.style.display = 'none';
    toggleButton.value = 'Show Debug Log';
  }
}

/**
 * Add the given message to the debug log.
 *
 * @param {string} message The debug info to add to the log.
 */
function addToDebugLog(message) {
  var debugLog = document.getElementById('debug-log');

  // Remove lines from top if we've hit our max log size.
  if (debugLog.childNodes.length == MAX_DEBUG_LOG_SIZE) {
    debugLog.removeChild(debugLog.firstChild);
  }

  // Add the new <p> to the end of the debug log.
  var p = document.createElement('p');
  p.appendChild(document.createTextNode(message));
  debugLog.appendChild(p);

  // Scroll to bottom of div
  debugLog.scrollTop = debugLog.scrollHeight;
}
