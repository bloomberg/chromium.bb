// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message id so that we can identify (and ignore) message fade operations for
// old messages.  This starts at 1 and is incremented for each new message.
chromoting.messageId = 1;

function init() {
  // This page should only get one request, and it should be
  // from the chromoting extension asking for initial connection.
  // Later we may need to create a more persistent channel for
  // better UI communication. Then, we should probably switch
  // to chrome.extension.connect().
  chrome.extension.onRequest.addListener(requestListener);
}

function submitLogin() {
  var username = document.getElementById("username").value;
  var password = document.getElementById("password").value;

  // Make the login panel invisible and submit login info.
  document.getElementById("login_panel").style.display = "none";
  chromoting.plugin.submitLoginInfo(username, password);
}

/**
 * A listener function to be called when the extension fires a request.
 *
 * @param request The request sent by the calling script.
 * @param sender The MessageSender object with info about the calling script's
 *               context.
 * @param sendResponse Function to call with response.
 */
function requestListener(request, sender, sendResponse) {
  console.log(sender.tab ?
              'from a content script: ' + sender.tab.url :
              'from the extension');

  // Kick off the connection.
  var plugin = document.getElementById('chromoting');

  chromoting.plugin = plugin;
  chromoting.username = request.username;
  chromoting.hostname = request.hostName;

  // Setup the callback that the plugin will call when the connection status
  // has changes and the UI needs to be updated. It needs to be an object with
  // a 'callback' property that contains the callback function.
  plugin.connectionInfoUpdate = pluginCallback;
  plugin.loginChallenge = pluginLoginChallenge;

  console.log('connect request received: ' + chromoting.hostname + ' by ' +
              chromoting.username);

  // TODO(garykac): Clean exit if |connect| isn't a funtion.
  if (typeof plugin.connect === 'function') {
    plugin.connect(request.username, request.hostJid,
                   request.xmppAuth);
  } else {
    console.log('ERROR: chromoting plugin not loaded');
  }

  document.getElementById('title').innerText = request.hostName;

  // Send an empty response since we have nothing to say.
  sendResponse({});
}

/**
 * This is the callback method that the plugin calls to request username and
 * password for logging into the remote host.
 */
function pluginLoginChallenge() {
  // Make the login panel visible.
  document.getElementById("login_panel").style.display = "block";
}

/**
 * This is a callback that gets called when the desktop size contained in the
 * the plugin has changed.
 */
function desktopSizeChanged() {
  var width = chromoting.plugin.desktopWidth;
  var height = chromoting.plugin.desktopHeight;

  console.log('desktop size changed: ' + width + 'x' + height);
  chromoting.plugin.style.width = width + "px";
  chromoting.plugin.style.height = height + "px";
}

/**
 * Show a client message on the screen.
 * If duration is specified, the message fades out after the duration expires.
 * Otherwise, the message stays until the state changes.
 *
 * @param {string} message The message to display.
 * @param {number} duration Milliseconds to show message before fading.
 */
function showClientStateMessage(message, duration) {
  // Increment message id to ignore any previous fadeout requests.
  chromoting.messageId++;
  console.log('setting message ' + chromoting.messageId + '!');

  // Update the status message.
  var msg = document.getElementById('status_msg');
  msg.innerText = message;
  msg.style.opacity = 1;

  if (duration) {
    // Set message duration.
    window.setTimeout("fade('status_msg', " + chromoting.messageId + ", " +
                            "100, 10, 200)",
                      duration);
  }
}

/**
 * This is that callback that the plugin invokes to indicate that the
 * host/client connection status has changed.
 */
function pluginCallback() {
  var status = chromoting.plugin.status;
  var quality = chromoting.plugin.quality;

  if (status == chromoting.plugin.STATUS_UNKNOWN) {
    setClientStateMessage('');
  } else if (status == chromoting.plugin.STATUS_CONNECTING) {
    setClientStateMessage('Connecting to ' + chromoting.hostname
                   + ' as ' + chromoting.username);
  } else if (status == chromoting.plugin.STATUS_INITIALIZING) {
    setClientStateMessageFade('Initializing connection to ' +
                              chromoting.hostname);
  } else if (status == chromoting.plugin.STATUS_CONNECTED) {
    desktopSizeChanged();
    setClientStateMessageFade('Connected to ' + chromoting.hostname, 1000);
  } else if (status == chromoting.plugin.STATUS_CLOSED) {
    setClientStateMessage('Closed');
  } else if (status == chromoting.plugin.STATUS_FAILED) {
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
  chromoting.messageId++;
  console.log('setting message ' + chromoting.messageId);

  // Update the status message.
  var msg = document.getElementById('status_msg');
  msg.innerText = message;
  msg.style.opacity = 1;
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
  window.setTimeout("fade('status_msg', " + chromoting.messageId + ", " +
                          "100, 10, 200)",
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
  if (id != chromoting.messageId) {
    return;
  }

  var e = document.getElementById(name);
  if (e) {
    var newVal = val - delta;
    if (newVal > 0) {
      // Decrease opacity and set timer for next fade event.
      e.style.opacity = newVal / 100;
      window.setTimeout("fade('status_msg', " + id + ", " + newVal + ", " +
                              delta + ", " + delay + ")",
                        delay);
    } else {
      // Completely hide the text and stop fading.
      e.style.opacity = 0;
    }
  }
}
