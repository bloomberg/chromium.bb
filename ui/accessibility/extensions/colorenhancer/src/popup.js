// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var site;


/**
 * Update the popup controls based on settings for this site or the default.
 */
function update() {
  if (site) {
    $('delta').value = getSiteDelta(site);
  } else {
    $('delta').value = getDefaultDelta();
  }

  $('severity').value = getDefaultSeverity();
  $('type').value = getDefaultType();
  $('simulate').checked = getDefaultSimulate();
  $('enable').checked = getDefaultEnable();

  debugPrint('update: ' +
      ' del=' + $('delta').value +
      ' sev=' + $('severity').value +
      ' typ=' + $('type').value +
      ' sim=' + $('simulate').checked +
      ' enb=' + $('enable').checked +
      ' for ' + site
  );

  chrome.extension.getBackgroundPage().updateTabs();
}


/**
 * Callback for color rotation slider.
 *
 * @param {number} value Parsed value of slider element.
 */
function onDeltaChange(value) {
  debugPrint('onDeltaChange: ' + value + ' for ' + site);
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
  debugPrint('onSeverityChange: ' + value + ' for ' + site);
  setDefaultSeverity(value);
  update();
}


/**
 * Callback for changing color deficiency type.
 *
 * @param {string} value Value of dropdown element.
 */
function onTypeChange(value) {
  debugPrint('onTypeChange: ' + value + ' for ' + site);
  setDefaultType(value);
  update();
}


/**
 * Callback for simulation setting.
 *
 * @param {boolean} value Value of checkbox element.
l*/
function onSimulateChange(value) {
  debugPrint('onSimulateChange: ' + value + ' for ' + site);
  setDefaultSimulate(value);
  update();
}

/**
 * Callback for enable/disable setting.
 *
 * @param {boolean} value Value of checkbox element.
*/
function onEnableChange(value) {
  debugPrint('onEnableChange: ' + value + ' for ' + site);
  setDefaultEnable(value);
  update();
}

/**
 * Callback for resetting stored per-site values.
 */
function onReset() {
  debugPrint('onReset');
  resetSiteDeltas();
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
  $('simulate').addEventListener('change', function() {
    onSimulateChange(this.checked);
  });
  $('enable').addEventListener('change', function() {
    onEnableChange(this.checked);
  });
  $('resetall').addEventListener('click', function() {
    onReset();
  });

  chrome.windows.getLastFocused({'populate': true}, function(window) {
    for (var i = 0; i < window.tabs.length; i++) {
      var tab = window.tabs[i];
      if (tab.active) {
        site = siteFromUrl(tab.url);
        debugPrint('init: active tab update for ' + site);
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
