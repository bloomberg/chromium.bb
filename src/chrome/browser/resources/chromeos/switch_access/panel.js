// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle the Switch Access Context Menu panel.
 * @constructor
 */
function Panel() {}

// This must be kept in sync with the div ID in panel.html
Panel.MENU_ID = 'switchaccess_contextmenu_actions';
// This must be kept in sync with the class name in panel.css
Panel.FOCUS_CLASS = 'focus';

Panel.readyReceived = false;
Panel.loaded = false;
window.addEventListener('message', Panel.preMessageHandler);

/**
 * Captures messages before the Panel is initialized.
 */
Panel.preMessageHandler = function() {
  Panel.readyReceived = true;
  if (Panel.loaded)
    Panel.sendReady();
  window.removeEventListener('message', Panel.preMessageHandler);
};

/**
 * Initialize the panel and buttons.
 */
Panel.init = function() {
  Panel.loaded = true;
  if (Panel.readyReceived)
    Panel.sendReady();

  let div = document.getElementById(Panel.MENU_ID);
  for (let button of div.children)
    Panel.setupButton(button);
  window.addEventListener('message', Panel.matchMessage);
};

/**
 * Sends a message to the background when both pages are loaded.
 */
Panel.sendReady = function() {
  MessageHandler.sendMessage(MessageHandler.Destination.BACKGROUND, 'ready');
};

/**
 * Adds an event listener to the given button to send a message when clicked.
 * @param {!HTMLElement} button
 */
Panel.setupButton = function(button) {
  let id = button.id;
  button.addEventListener('click', function() {
    MessageHandler.sendMessage(MessageHandler.Destination.BACKGROUND, id);
  }.bind(id));
};

/**
 * Takes the given message and sees if it matches any expected pattern. If
 * it does, extract the parameters and pass them to the appropriate handler.
 * If not, log as an unrecognized action.
 *
 * @param {Object} message
 */
Panel.matchMessage = function(message) {
  if (message.data === 'ready') {
    clearInterval(Panel.handshakeInterval);
    return;
  }
  let matches = message.data.match(MessageHandler.SET_ACTIONS_REGEX);
  if (matches && matches.length === 2) {
    let actions = matches[1].split(',');
    Panel.setActions(actions);
    return;
  }
  matches = message.data.match(MessageHandler.SET_FOCUS_RING_REGEX);
  if (matches && matches.length === 3) {
    let id = matches[1];
    let shouldAdd = matches[2] === 'on';
    Panel.updateStyle(id, Panel.FOCUS_CLASS, shouldAdd);
    return;
  }
  console.log('Action not recognized: ' + message.data);
};

/**
 * Sets which buttons are enabled/disabled, based on |actions|.
 * @param {!Array<string>} actions
 */
Panel.setActions = function(actions) {
  let div = document.getElementById(Panel.MENU_ID);
  for (let button of div.children)
    button.disabled = !actions.includes(button.id);
};

/**
 * Sets the style for the element with the given |id| by toggling
 * |className|.
 * @param {string} id
 * @param {string} className
 * @param {bool} shouldAdd
 */
Panel.updateStyle = function(id, className, shouldAdd) {
  let htmlNode = document.getElementById(id);
  if (shouldAdd)
    htmlNode.classList.add(className);
  else
    htmlNode.classList.remove(className);
};

window.addEventListener('load', Panel.init);
