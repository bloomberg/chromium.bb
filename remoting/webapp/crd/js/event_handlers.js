// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

remoting.initGlobalEventHandlers = function() {
  window.addEventListener('resize', remoting.onResize, false);
  // When a window goes full-screen, a resize event is triggered, but the
  // Fullscreen.isActive call is not guaranteed to return true until the
  // full-screen event is triggered. In apps v2, the size of the window's
  // client area is calculated differently in full-screen mode, so register
  // for both events.
  remoting.fullscreen.addListener(remoting.onResize);
  if (!base.isAppsV2()) {
    window.addEventListener('beforeunload', remoting.promptClose, false);
    window.addEventListener('unload', remoting.disconnect, false);
  }
}

/**
 * @param {Array.<{event: string, id: string,
 *     fn: function(Event):void}>} actions Array of actions to register.
 */
function registerEventListeners(actions) {
  for (var i = 0; i < actions.length; ++i) {
    var action = actions[i];
    registerEventListener(action.id, action.event, action.fn);
  }
}

/**
 * Add an event listener to the specified element.
 * @param {string} id Id of element.
 * @param {string} eventname Event name.
 * @param {function(Event):void} fn Event handler.
 */
function registerEventListener(id, eventname, fn) {
  var element = document.getElementById(id);
  if (element) {
    element.addEventListener(eventname, fn, false);
  } else {
    console.error('Could not set ' + eventname +
        ' event handler on element ' + id +
        ': element not found.');
  }
}
