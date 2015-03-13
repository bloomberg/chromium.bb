// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Move to chrome.storage.local, wrap calls, add JsDocs.

var DEFAULT_DELTA = 1.0;
var LOCAL_STORAGE_TAG_DELTA = 'cvd_delta';
var LOCAL_STORAGE_TAG_SITE_DELTA = 'cvd_site_deltas';


function validDelta(delta) {
  return delta >= 0 && delta <= 1;
}


function getDefaultDelta() {
  var delta = localStorage[LOCAL_STORAGE_TAG_DELTA];
  if (validDelta(delta)) {
    return delta;
  }
  delta = DEFAULT_DELTA;
  localStorage[LOCAL_STORAGE_TAG_DELTA] = delta;
  return delta;
}


function setDefaultDelta(delta) {
  if (!validDelta(delta)) {
    delta = DEFAULT_DELTA;
  }
  localStorage[LOCAL_STORAGE_TAG_DELTA] = delta;
}


function getSiteDelta(site) {
  var delta = getDefaultDelta();
  try {
    var siteDeltas = JSON.parse(localStorage[LOCAL_STORAGE_TAG_SITE_DELTA]);
    delta = siteDeltas[site];
    if (!validDelta(delta)) {
      delta = getDefaultDelta();
    }
  } catch (e) {
    delta = getDefaultDelta();
  }
  return delta;
}


function setSiteDelta(site, delta) {
  if (!validDelta(delta)) {
    delta = getDefaultDelta();
  }
  var siteDeltas = {};
  try {
    siteDeltas = JSON.parse(localStorage[LOCAL_STORAGE_TAG_SITE_DELTA]);
  } catch (e) {
    siteDeltas = {};
  }
  siteDeltas[site] = delta;
  localStorage[LOCAL_STORAGE_TAG_SITE_DELTA] = JSON.stringify(siteDeltas);
}


function resetSiteDeltas() {
  var siteDeltas = {};
  localStorage[LOCAL_STORAGE_TAG_SITE_DELTA] = JSON.stringify(siteDeltas);
}


// ======= Severity setting =======

var DEFAULT_SEVERITY = 1.0;
var LOCAL_STORAGE_TAG_SEVERITY = 'cvd_severity';
var LOCAL_STORAGE_TAG_SITE_SEVERITY = 'cvd_severities';


function validSeverity(severity) {
  return severity >= 0 && severity <= 1;
}


function getDefaultSeverity() {
  var severity = localStorage[LOCAL_STORAGE_TAG_SEVERITY];
  if (validSeverity(severity)) {
    return severity;
  }
  severity = DEFAULT_SEVERITY;
  localStorage[LOCAL_STORAGE_TAG_SEVERITY] = severity;
  return severity;
}


function setDefaultSeverity(severity) {
  if (!validSeverity(severity)) {
    severity = DEFAULT_SEVERITY;
  }
  localStorage[LOCAL_STORAGE_TAG_SEVERITY] = severity;
}


// TODO(mustaq): Remove site-specific severity setting.
function getSiteSeverity(site) {
  var severity = getDefaultSeverity();
  try {
    var siteSeverities =
        JSON.parse(localStorage[LOCAL_STORAGE_TAG_SITE_SEVERITY]);
    severity = siteSeverities[site];
    if (!validSeverity(severity)) {
      severity = getDefaultSeverity();
    }
  } catch (e) {
    severity = getDefaultSeverity();
  }
  return severity;
}


function setSiteSeverity(site, severity) {
  if (!validSeverity(severity)) {
    severity = getDefaultSeverity();
  }
  var siteSeverities = {};
  try {
    siteSeverities = JSON.parse(localStorage[LOCAL_STORAGE_TAG_SITE_SEVERITY]);
  } catch (e) {
    siteSeverities = {};
  }
  siteSeverities[site] = severity;
  localStorage[LOCAL_STORAGE_TAG_SITE_SEVERITY] =
      JSON.stringify(siteSeverities);
}


function resetSiteSeverities() {
  var siteSeverities = {};
  localStorage[LOCAL_STORAGE_TAG_SITE_SEVERITY] =
      JSON.stringify(siteSeverities);
}


// ======= Type setting =======

// TODO(wnwen): Use enums rather than strings.
var DEFAULT_TYPE = 'PROTANOMALY';
var LOCAL_STORAGE_TAG_TYPE = 'cvd_type';


function validType(type) {
  return type === 'PROTANOMALY' ||
      type === 'DEUTERANOMALY' ||
      type === 'TRITANOMALY';
}


function getDefaultType() {
  var type = localStorage[LOCAL_STORAGE_TAG_TYPE];
  if (validType(type)) {
    return type;
  }
  type = DEFAULT_TYPE;
  localStorage[LOCAL_STORAGE_TAG_TYPE] = type;
  return type;
}


function setDefaultType(type) {
  if (!validType(type)) {
    type = DEFAULT_TYPE;
  }
  localStorage[LOCAL_STORAGE_TAG_TYPE] = type;
}


// ======= Simulate setting =======

var DEFAULT_SIMULATE = false;
var LOCAL_STORAGE_TAG_SIMULATE = 'cvd_simulate';


function validSimulate(simulate) {
  return simulate == true || simulate == false;
}


function getDefaultSimulate() {
  var simulate = localStorage[LOCAL_STORAGE_TAG_SIMULATE];

  //Make it a boolean if possible
  if (simulate === 'true') {
    simulate = true;
  } else if (simulate === 'false') {
    simulate = false;
  } else {
    simulate = 'undef';
  }

  if (validSimulate(simulate)) {
    return simulate;
  }
  simulate = DEFAULT_SIMULATE;
  localStorage[LOCAL_STORAGE_TAG_SIMULATE] = simulate;
  return simulate;
}


function setDefaultSimulate(simulate) {
  if (!validSimulate(simulate)) {
    simulate = DEFAULT_SIMULATE;
  }
  localStorage[LOCAL_STORAGE_TAG_SIMULATE] = simulate;
}
