// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chromoting = {};

function init() {
  setHostMode('unshared');
  setClientMode('unconnected');
  setGlobalMode('host');  // TODO(jamiewalch): Make the initial mode sticky.
}

// Show the div with id |mode| and hide those with other ids in |modes|.
function setMode(mode, modes) {
  for (var i = 0; i < modes.length; ++i) {
    var div = document.getElementById(modes[i]);
    if (mode == modes[i]) {
      div.style.display = 'block';
    } else {
      div.style.display = 'none';
    }
  }
}

function setGlobalMode(mode) {
  setMode(mode, ['host', 'client', 'session']);
}

function setHostMode(mode) {
  setMode(mode, ['unshared', 'ready_to_share', 'shared']);
}

function setClientMode(mode) {
  setMode(mode, ['unconnected', 'connecting']);
}

function tryShare() {
  setHostMode('ready_to_share');
  chromoting.hostTimer = setTimeout(
      function() {
        setHostMode('shared');
      },
      3000);
}

function cancelShare() {
  setHostMode('unshared');
  clearTimeout(chromoting.hostTimer);
}

function tryConnect(form) {
  chromoting.accessCode = form.access_code_entry.value;
  setClientMode('connecting');
  chromoting.clientTimer = setTimeout(
      function() {
        var code = document.getElementById('access_code_proof');
        code.innerHTML = chromoting.accessCode;
        setGlobalMode('session');
      },
      3000);
}

function cancelConnect() {
  chromoting.accessCode = '';
  setClientMode('unconnected');
  clearTimeout(chromoting.clientTimer);
}
