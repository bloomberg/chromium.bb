// Copyright 2018 The Chromium OS Authors. All rights reserved.
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
 * Creates the settings-view controller.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.Settings = function() {
  cca.views.View.call(this, '#settings', true, true);

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelector('#settings-feedback').hidden =
      !cca.util.isChromeVersionAbove(72); // Feedback available since M72.

  cca.views.settings.util.setupMenu(this, {
    'settings-gridtype': () => cca.nav.open('gridsettings'),
    'settings-feedback': () => this.openFeedback(),
    'settings-help': () => this.openHelp_(),
  });

  // Observe the attributes toggled by radios and update the descriptions.
  var observer = new MutationObserver(() => {
    var updateTextByRadio = (selector, name) => {
      var radio = document.querySelector(
          `input[type=radio][name=${name}]:checked`);
      document.querySelector(selector).textContent =
          radio && radio.labels.length && radio.labels[0].textContent.trim();
    };
    updateTextByRadio('#gridtype-desc', 'gridtype');
  });
  observer.observe(document.body, {
    attributes: true,
    attributeFilter: ['golden'],
  });
};

cca.views.Settings.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * Opens feedback.
 * @private
 */
cca.views.Settings.prototype.openFeedback = function() {
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
cca.views.Settings.prototype.openHelp_ = function() {
  window.open('https://support.google.com/chromebook/answer/4487486');
};
