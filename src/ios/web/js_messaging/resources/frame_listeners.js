// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. See http://goo.gl/FwOgy

goog.provide('__crWeb.frameListeners');

// Requires __crWeb.common and __crWeb.message.

/* Beginning of anonymous object. */
(function() {

window.addEventListener('unload', function(event) {
  __gCrWeb.common.sendWebKitMessage('FrameBecameUnavailable',
      __gCrWeb.message.getFrameId());
});

/**
 * Listens for messages received by the parent frame to initialize messaging
 * state.
 */
window.addEventListener('message', function(message) {
  var payload = message.data;
  if (typeof payload !== 'object') {
    return;
  }
  if (payload.hasOwnProperty('type') &&
    payload.type == 'org.chromium.registerForFrameMessaging') {
    __gCrWeb.message['getExistingFrames']();
  } else if (payload.hasOwnProperty('type') &&
      payload.type == 'org.chromium.encryptedMessage') {
    if (payload.hasOwnProperty('message_payload') &&
        payload.hasOwnProperty('function_payload') &&
        payload.hasOwnProperty('target_frame_id')) {
      __gCrWeb.message['routeMessage'](
        payload['message_payload'],
        payload['function_payload'],
        payload['target_frame_id']
      );
    }
  }
});

}());  // End of anonymous object
