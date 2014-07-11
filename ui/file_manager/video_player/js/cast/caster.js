// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The instance of cast api.
 * @type {cast.ExtensionApi}
 */
var castApi = null;

/**
 * @type {string}
 * @const
 */
var CAST_COMMAND_LINE_FLAG = 'enable-video-player-chromecast-support';

chrome.commandLinePrivate.hasSwitch(CAST_COMMAND_LINE_FLAG, function(result) {
  if (!result)
    return;

  CastExtensionDiscoverer.findInstalledExtension(onCastExtensionFound);
});

function onCastExtensionFound(extensionId) {
  if (!extensionId) {
    console.info('Cast extention is not found.');
    return;
  }

  var api = document.createElement('script');
  api.src = 'chrome-extension://' + extensionId + '/api_script.js';
  api.onload = function() {
    initializeCast(extensionId);
  };
  api.onerror = function() {
    console.error('api_script.js load failed.');
  };
  document.body.appendChild(api);
};

function initializeCast(extensionId) {
  loadCastExtensionApi();

  castApi = new cast.ExtensionApi(extensionId);
  castApi.addReceiverListener('ChromeCast', onReceiverUpdate);
}

function onReceiverUpdate(receivers) {
  player.setCastList(receivers);
}
