// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var site;


/**
 * Update the popup controls based on settings for this site or the default.
 */
function update() {
  if (site) {
    debugPrint('updated site ' + site + ' to ' +
        getSiteDelta(site) + ',' + getSiteSeverity(site));
    $('delta').value = getSiteDelta(site);
    $('severity').value = getSiteSeverity(site);
  } else {
    debugPrint('updated site ' + site + ' to ' +
        getDefaultDelta() + ',' + getDefaultSeverity());
    $('delta').value = getDefaultDelta();
    $('severity').value = getDefaultSeverity();
  }

  debugPrint('updated site ' + site + ' type ' + getDefaultType());
  $('type').value = getDefaultType();

  // TODO(mustaq): Finish simulate feature.
  //debugPrint('updated site ' + site + ' simulate ' + getDefaultSimulate());
  //$('simulate').checked = getDefaultSimulate();

  chrome.extension.getBackgroundPage().updateTabs();
}


/**
 * Callback for color rotation slider.
 *
 * @param {number} value Parsed value of slider element.
 */
function onDeltaChange(value) {
  debugPrint('delta changing to ' + value + ' for site ' + site);
  if (site) {
    setSiteDelta(site, value);
  }
  setDefaultDelta(value);
  update();
}


/**
 * Callback for severity slider.
 *
 * @param {number} value Parsed value of slider element.
 */
function onSeverityChange(value) {
  debugPrint('severity changing to ' + value + ' for site ' + site);
  if (site) {
    setSiteSeverity(site, value);
  }
  setDefaultSeverity(value);
  update();
}


/**
 * Callback for changing color deficiency type.
 *
 * @param {string} value Value of dropdown element.
 */
function onTypeChange(value) {
  debugPrint('type changing to ' + value + ' for site ' + site);
  setDefaultType(value);
  update();
}


/**
 * TODO(mustaq): JsDoc.
 */
function onSimulateChange(value) {
  debugPrint('simulate changing to ' + value + ' for site ' + site);
  setDefaultSimulate(value);
  update();
}


/**
 * Reset all stored per-site and default values.
 */
function onReset() {
  debugPrint('resetting sites');
  resetSiteDeltas();
  resetSiteSeverities();
  update();
}


/**
 * Attach event handlers to controls and update the filter config values for the
 * currently visible tab.
 */
function initialize() {
  $('delta').addEventListener('input', function() {
    onDeltaChange(parseFloat(this.value));
  });
  $('severity').addEventListener('input', function() {
    onSeverityChange(parseFloat(this.value));
  });
  $('type').addEventListener('input', function() {
    onTypeChange(this.value);
  });
  // TODO(mustaq): Finish simulate feature.
  //$('simulate').addEventListener('change', function() {
  //  onSimulateChange(!this.checked);
  //});
  $('resetall').addEventListener('click', function() {
    onReset();
  });

  chrome.windows.getLastFocused({'populate': true}, function(window) {
    for (var i = 0; i < window.tabs.length; i++) {
      var tab = window.tabs[i];
      if (tab.active) {
        site = siteFromUrl(tab.url);
        debugPrint('active tab update ' + site);
        update();
        return;
      }
    }
    site = 'unknown site';
    update();
  });
}

// TODO(wnwen): Use Promise instead, more reliable.
window.addEventListener('load', initialize, false);
