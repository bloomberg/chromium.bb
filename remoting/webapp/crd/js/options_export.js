// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function(){

'use strict';

/**
 * @constructor
 */
remoting.OptionsExporter = function() {
  base.Ipc.getInstance().register('getSettings',
                                  remoting.OptionsExporter.migrateSettings_,
                                  true);
};

remoting.OptionsExporter.migrateSettings_ = function() {
  var result = new base.Deferred();
  chrome.storage.local.get(KEY_NAME, function(options) {
    // If there are no host options stored, reformat the message response so
    // that the sender doesn't interpret it as an error.
    if (Object.keys(options).length == 0) {
      options[KEY_NAME] = '{}';
    }
    result.resolve(options);
  })
  return result.promise();
};

var KEY_NAME = 'remoting-host-options';

}());
