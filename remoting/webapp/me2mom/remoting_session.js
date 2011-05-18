// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maximum numer of lines to record in the debug log.
// Only the most recent <n> lines are displayed.
var MAX_DEBUG_LOG_SIZE = 1000;
var remoting = chrome.extension.getBackgroundPage().remoting;

// Message id so that we can identify (and ignore) message fade operations for
// old messages.  This starts at 1 and is incremented for each new message.
remoting.messageId = 1;

remoting.scaleToFit = false;

// Default to trying to sandboxed connections.
remoting.connectMethod = 'sandboxed';
remoting.httpXmppProxy = 'https://chromoting-httpxmpp-dev.corp.google.com';

// This executes a poll loop on the server for more Iq packets, and feeds them
// to the plugin.
function feedIq() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', remoting.httpXmppProxy + '/readIq?host_jid=' +
           encodeURIComponent(remoting.hostjid), true);
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
        remoting.plugin.connectSandboxed(clientjid, remoting.hostjid,
                                         remoting.accessCode);
        // TODO(ajwong): This should just be feedIq();
        window.setTimeout(feedIq, 1000);
      } else {
        addToDebugLog('FailedToConnect: --' + xhr.responseText +
                      '-- (status=' + xhr.status + ')');
        setClientStateMessage('Failed');
      }
    }
  }
  xhr.send('host_jid=' + encodeURIComponent(remoting.hostjid) +
           '&username=' + encodeURIComponent(remoting.username) +
           '&password=' + encodeURIComponent(remoting.xmppAuthToken));
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
           '&host_jid=' + encodeURIComponent(remoting.hostjid));
}

function init() {
  // Kick off the connection.
  var plugin = document.getElementById('remoting');

  remoting.plugin = plugin;

  // Only allow https connections to the httpXmppProxy.
  if (remoting.httpXmppProxy.search(/^ *https:\/\//) == -1) {
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

  addToDebugLog('Connect as user ' + remoting.username);

  // TODO(garykac): Clean exit if |connect| isn't a function.
  if (typeof plugin.connect === 'function') {
    if (remoting.connectMethod == 'sandboxed') {
      registerConnection();
    } else {
      plugin.connect(remoting.username, remoting.hostjid,
                     remoting.xmppAuthToken, remoting.accessCode);
    }
  } else {
    addToDebugLog('ERROR: remoting plugin not loaded');
    setClientStateMessage('Plugin not loaded');
  }

}

function toggleDebugLog() {
  debugLog = document.getElementById('debug_log');
  toggleButton = document.getElementById('debug_log_toggle');

  if (!debugLog.style.display || debugLog.style.display == 'none') {
    debugLog.style.display = 'block';
    toggleButton.value = 'Hide Debug Log';
  } else {
    debugLog.style.display = 'none';
    toggleButton.value = 'Show Debug Log';
  }
}

function toggleScaleToFit() {
  remoting.scaleToFit = !remoting.scaleToFit;
  document.getElementById('scale_to_fit_toggle').value =
      remoting.scaleToFit ? 'No scaling' : 'Scale to fit';
  remoting.plugin.setScaleToFit(remoting.scaleToFit);
}

function submitLogin() {
  var username = document.getElementById('username').value;
  var password = document.getElementById('password').value;

  // Make the login panel invisible and submit login info.
  document.getElementById('login_panel').style.display = 'none';
  remoting.plugin.submitLoginInfo(username, password);
}

/**
 * This is the callback method that the plugin calls to request username and
 * password for logging into the remote host.
 */
function loginChallengeCallback() {
  // Make the login panel visible.
  document.getElementById('login_panel').style.display = 'block';

  // Put the insertion point into the form.
  document.getElementById('username').focus();
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
  var msg = document.getElementById('status_msg');
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
  window.setTimeout("fade('status_msg', " + remoting.messageId + ', ' +
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
      window.setTimeout("fade('status_msg', " + id + ', ' + newVal + ', ' +
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

/**
 * Add the given message to the debug log.
 *
 * @param {string} message The debug info to add to the log.
 */
function addToDebugLog(message) {
  var debugLog = document.getElementById('debug_log');

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

function updateStatusBarStats() {
  if (remoting.plugin.status != remoting.plugin.STATUS_CONNECTED)
    return;
  var videoBandwidth = remoting.plugin.videoBandwidth;
  var videoCaptureLatency = remoting.plugin.videoCaptureLatency;
  var videoEncodeLatency = remoting.plugin.videoEncodeLatency;
  var videoDecodeLatency = remoting.plugin.videoDecodeLatency;
  var videoRenderLatency = remoting.plugin.videoRenderLatency;

  setClientStateMessage(
      'Video stats: bandwidth: ' + videoBandwidth.toFixed(2) + 'Kbps' +
      ', Latency: capture: ' + videoCaptureLatency.toFixed(2) + 'ms' +
      ', encode: ' + videoEncodeLatency.toFixed(2) + 'ms' +
      ', decode: ' + videoDecodeLatency.toFixed(2) + 'ms' +
      ', render: ' + videoRenderLatency.toFixed(2) + 'ms');

  // Update the stats once per second.
  window.setTimeout('updateStatusBarStats()', 1000);
}
