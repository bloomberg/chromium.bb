// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Creates the base controller of settings view.
 * @param {string} selector Selector text of the view's root element.
 * @param {Object<string|function(Event=)>} itemHandlers Click-handlers
 *     mapped by element ids.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.BaseSettings = function(selector, itemHandlers) {
  cca.views.View.call(this, selector, true, true);

  // End of properties, seal the object.
  Object.seal(this);

  this.root.querySelector('.menu-header button').addEventListener(
      'click', () => this.leave());
  this.root.querySelectorAll('.menu-item').forEach((element) => {
    var handler = itemHandlers[element.id];
    if (handler) {
      element.addEventListener('click', handler);
    }
  });
};

cca.views.BaseSettings.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * Creates the controller of grid settings view.
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.GridSettings = function() {
  cca.views.BaseSettings.call(this, '#gridsettings', {});

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.GridSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Creates the controller of timer settings view.
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.TimerSettings = function() {
  cca.views.BaseSettings.call(this, '#timersettings', {});

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.TimerSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Creates the controller of master settings view.
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.MasterSettings = function() {
  cca.views.BaseSettings.call(this, '#settings', {
    'settings-gridtype': () => this.openSubSettings('gridsettings'),
    'settings-timerdur': () => this.openSubSettings('timersettings'),
    'settings-feedback': () => this.openFeedback(),
    'settings-help': () => this.openHelp_(),
  });

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelector('#settings-feedback').hidden =
      !cca.util.isChromeVersionAbove(72); // Feedback available since M72.
};

cca.views.MasterSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Opens sub-settings.
 * @param {string} id Settings identifier.
 * @private
 */
cca.views.MasterSettings.prototype.openSubSettings = function(id) {
  // Dismiss master-settings if sub-settings was dimissed by background click.
  cca.nav.open(id).then((cond) => cond && cond.bkgnd && this.leave());
};

/**
 * Opens feedback.
 * @private
 */
cca.views.MasterSettings.prototype.openFeedback = function() {
  var data = {
    'categoryTag': 'chromeos-camera-app',
    'requestFeedback': true,
    'feedbackInfo': {
      'description': '',
      'systemInformation': [
        {key: 'APP ID', value: chrome.runtime.id},
        {key: 'APP VERSION', value: chrome.runtime.getManifest().version},
      ],
    },
  };
  const id = 'gfdkimpbcpahaombhbimeihdjnejgicl'; // Feedback extension id.
  chrome.runtime.sendMessage(id, data);
};

/**
 * Opens help.
 * @private
 */
cca.views.MasterSettings.prototype.openHelp_ = function() {
  window.open(
      'https://support.google.com/chromebook/?p=camera_usage_on_chromebook');
};
