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
  chrome.storage.local.get('remoting-host-options', function(options) {
    result.resolve(options);
  })
  return result.promise();
};


}());
