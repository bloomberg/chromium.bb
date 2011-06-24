// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var remoting = chrome.extension.getBackgroundPage().remoting;

// Chromoting session API version (for this javascript).
// This is compared with the plugin API version to verify that they are
// compatible.
remoting.apiVersion = 2;

// The oldest API version that we support.
// This will differ from the |apiVersion| if we maintain backward
// compatibility with older API versions.
remoting.apiMinVersion = 1;

// Message id so that we can identify (and ignore) message fade operations for
// old messages.  This starts at 1 and is incremented for each new message.
remoting.messageId = 1;

remoting.scaleToFit = false;

remoting.httpXmppProxy =
    'https://chromoting-httpxmpp-oauth2-dev.corp.google.com';

window.addEventListener("load", init_, false);

// This executes a poll loop on the server for more Iq packets, and feeds them
// to the plugin.
function feedIq() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', remoting.httpXmppProxy + '/readIq?host_jid=' +
           encodeURIComponent(remoting.hostJid), true);
  xhr.withCredentials = true;
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        addToDebugLog('Receiving Iq: --' + xhr.responseText + '--');
        remoting.plugin.onIq(xhr.responseText);
      }
      if (xhr.status == 200 || xhr.status == 204) {
        window.setTimeout(feedIq, 0);
      } else {
        addToDebugLog('HttpXmpp gateway returned code: ' + xhr.status);
        remoting.plugin.disconnect();
        setClientStateMessage('Failed');
      }
    }
  }
  xhr.send(null);
}

function registerConnection() {
  var xhr = new XMLHttpRequest();
  xhr.open('POST', remoting.httpXmppProxy + '/newConnection', true);
  xhr.withCredentials = true;
  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        addToDebugLog('Receiving Iq: --' + xhr.responseText + '--');
        var clientjid = xhr.responseText;

        remoting.plugin.sendIq = sendIq;
        if (remoting.plugin.apiVersion >= 2) {
          remoting.plugin.connect(remoting.hostJid, remoting.hostPublicKey,
                                  clientjid, remoting.accessCode);
        } else {
          remoting.plugin.connect(remoting.hostJid, clientjid,
                                  remoting.accessCode);
        }
        // TODO(ajwong): This should just be feedIq();
        window.setTimeout(feedIq, 1000);
      } else {
        addToDebugLog('FailedToConnect: --' + xhr.responseText +
                      '-- (status=' + xhr.status + ')');
        setClientStateMessage('Failed');
      }
    }
  }
  xhr.send('host_jid=' + encodeURIComponent(remoting.hostJid) +
           '&username=' + encodeURIComponent(remoting.username) +
           '&password=' + encodeURIComponent(remoting.oauth2.getAccessToken()));
  setClientStateMessage('Connecting');
}

function sendIq(msg) {
  addToDebugLog('Sending Iq: ' + msg);

  // Extract the top level fields of the Iq packet.
  // TODO(ajwong): Can the plugin just return these fields broken out.
  parser = new DOMParser();
  iqNode = parser.parseFromString(msg, 'text/xml').firstChild;
  id = iqNode.getAttribute('id');
  type = iqNode.getAttribute('type');
  to = iqNode.getAttribute('to');
  serializer = new XMLSerializer();
  payload_xml = serializer.serializeToString(iqNode.firstChild);

  var xhr = new XMLHttpRequest();
  xhr.open('POST', remoting.httpXmppProxy + '/sendIq', true);
  xhr.withCredentials = true;
  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
  xhr.send('to=' + encodeURIComponent(to) +
           '&payload_xml=' + encodeURIComponent(payload_xml) +
           '&id=' + id + '&type=' + type +
           '&host_jid=' + encodeURIComponent(remoting.hostJid));
}

function checkVersion(plugin) {
  return remoting.apiVersion >= plugin.apiMinVersion &&
      plugin.apiVersion >= remoting.apiMinVersion;
}

function init_() {
  // Kick off the connection.
  var plugin = document.getElementById('remoting');

  remoting.plugin = plugin;

  // Only allow https connections to the httpXmppProxy.
  var regExp = /^ *https:\/\//;
  if (remoting.httpXmppProxy.search(regExp) == -1) {
    addToDebugLog('Aborting. httpXmppProxy does not specify https protocol: ' +
                  remoting.httpXmppProxy);
    return;
  }

  // Setup the callback that the plugin will call when the connection status
  // has changes and the UI needs to be updated. It needs to be an object with
  // a 'callback' property that contains the callback function.
  plugin.connectionInfoUpdate = connectionInfoUpdateCallback;
  plugin.debugInfo = debugInfoCallback;
  plugin.desktopSizeUpdate = desktopSizeChanged;
  plugin.loginChallenge = loginChallengeCallback;

  if (!checkVersion(plugin)) {
    // TODO(garykac): We need better messaging here. Perhaps an install link.
    setClientStateMessage("Out of date. Please re-install.");
    return;
  }

  addToDebugLog('Connect as user ' + remoting.username);

  // TODO(garykac): Clean exit if |connect| isn't a function.
  if (typeof plugin.connect === 'function') {
    registerConnection();
  } else {
    addToDebugLog('ERROR: remoting plugin not loaded');
    setClientStateMessage('Plugin not loaded');
  }

}

function toggleScaleToFit() {
  remoting.scaleToFit = !remoting.scaleToFit;
  document.getElementById('scale-to-fit-toggle').value =
      remoting.scaleToFit ? 'No scaling' : 'Scale to fit';
  remoting.plugin.setScaleToFit(remoting.scaleToFit);
}

/**
 * This is the callback method that the plugin calls to request username and
 * password for logging into the remote host. For Me2Mom we are pre-authorized
 * so this is a no-op.
 */
function loginChallengeCallback() {
}

/**
 * This is a callback that gets called when the desktop size contained in the
 * the plugin has changed.
 */
function desktopSizeChanged() {
  var width = remoting.plugin.desktopWidth;
  var height = remoting.plugin.desktopHeight;

  addToDebugLog('desktop size changed: ' + width + 'x' + height);
  remoting.plugin.style.width = width + 'px';
  remoting.plugin.style.height = height + 'px';
}

/**
 * This is that callback that the plugin invokes to indicate that the
 * host/client connection status has changed.
 */
function connectionInfoUpdateCallback() {
  var status = remoting.plugin.status;
  var quality = remoting.plugin.quality;

  if (status == remoting.plugin.STATUS_UNKNOWN) {
    setClientStateMessage('');
  } else if (status == remoting.plugin.STATUS_CONNECTING) {
    setClientStateMessage('Connecting as ' + remoting.username);
  } else if (status == remoting.plugin.STATUS_INITIALIZING) {
    setClientStateMessageFade('Initializing connection');
  } else if (status == remoting.plugin.STATUS_CONNECTED) {
    desktopSizeChanged();
    setClientStateMessageFade('Connected', 1000);
    window.setTimeout(updateStatusBarStats, 1000);
  } else if (status == remoting.plugin.STATUS_CLOSED) {
    setClientStateMessage('Closed');
  } else if (status == remoting.plugin.STATUS_FAILED) {
    setClientStateMessage('Failed');
  }
}

/**
 * Show a client message that stays on the screeen until the state changes.
 *
 * @param {string} message The message to display.
 */
function setClientStateMessage(message) {
  // Increment message id to ignore any previous fadeout requests.
  remoting.messageId++;

  // Update the status message.
  var msg = document.getElementById('status-msg');
  msg.innerText = message;
  msg.style.opacity = 1;
  msg.style.display = '';
}

/**
 * Show a client message for the specified amount of time.
 *
 * @param {string} message The message to display.
 * @param {number} duration Milliseconds to show message before fading.
 */
function setClientStateMessageFade(message, duration) {
  setClientStateMessage(message);

  // Set message duration.
  window.setTimeout("fade('status-msg', " + remoting.messageId + ', ' +
                          '100, 10, 200)',
                    duration);
}

/**
 * Fade the specified element.
 * For example, to have element 'foo' fade away over 2 seconds, you could use
 * either:
 *   fade('foo', 100, 10, 200)
 *     - Start at 100%, decrease by 10% each time, wait 200ms between updates.
 *   fade('foo', 100, 5, 100)
 *     - Start at 100%, decrease by 5% each time, wait 100ms between updates.
 *
 * @param {string} name Name of element to fade.
 * @param {number} id The id of the message associated with this fade request.
 * @param {number} val The new opacity value (0-100) for this element.
 * @param {number} delta Amount to adjust the opacity each iteration.
 * @param {number} delay Delay (in ms) to wait between each update.
 */
function fade(name, id, val, delta, delay) {
  // Ignore the fade call if it does not apply to the current message.
  if (id != remoting.messageId) {
    return;
  }

  var e = document.getElementById(name);
  if (e) {
    var newVal = val - delta;
    if (newVal > 0) {
      // Decrease opacity and set timer for next fade event.
      e.style.opacity = newVal / 100;
      window.setTimeout("fade('status-msg', " + id + ', ' + newVal + ', ' +
                              delta + ', ' + delay + ')',
                        delay);
    } else {
      // Completely hide the text and stop fading.
      e.style.opacity = 0;
      e.style.display = 'none';
    }
  }
}

/**
 * This is that callback that the plugin invokes to indicate that there
 * is additional debug log info to display.
 */
function debugInfoCallback(msg) {
  addToDebugLog('plugin: ' + msg);
}

function updateStatusBarStats() {
  if (remoting.plugin.status != remoting.plugin.STATUS_CONNECTED)
    return;
  var videoBandwidth = remoting.plugin.videoBandwidth;
  var videoCaptureLatency = remoting.plugin.videoCaptureLatency;
  var videoEncodeLatency = remoting.plugin.videoEncodeLatency;
  var videoDecodeLatency = remoting.plugin.videoDecodeLatency;
  var videoRenderLatency = remoting.plugin.videoRenderLatency;

  var units = '';
  if (videoBandwidth < 1024) {
    units = 'Bps';
  } else if (videoBandwidth < 1048576) {
    units = 'KiBps';
    videoBandwidth = videoBandwidth / 1024;
  } else if (videoBandwidth < 1073741824) {
    units = 'MiBps';
    videoBandwidth = videoBandwidth / 1048576;
  } else {
    units = 'GiBps';
    videoBandwidth = videoBandwidth / 1073741824;
  }

  setClientStateMessage(
      'Bandwidth: ' + videoBandwidth.toFixed(2) + units +
      ', Capture: ' + videoCaptureLatency.toFixed(2) + 'ms' +
      ', Encode: ' + videoEncodeLatency.toFixed(2) + 'ms' +
      ', Decode: ' + videoDecodeLatency.toFixed(2) + 'ms' +
      ', Render: ' + videoRenderLatency.toFixed(2) + 'ms');

  // Update the stats once per second.
  window.setTimeout('updateStatusBarStats()', 1000);
}
