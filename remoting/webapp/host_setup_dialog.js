// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Array.<remoting.HostSetupFlow.State>} sequence Sequence of
 * steps for the flow.
 * @constructor
 */
remoting.HostSetupFlow = function(sequence) {
  this.sequence_ = sequence;
  this.currentStep_ = 0;
  this.state_ = sequence[0];

  this.pin = '';
  this.hostConfig = '';
};

/** @enum {number} */
remoting.HostSetupFlow.State = {
  NONE: 0,

  // Dialog states.
  ASK_PIN: 1,

  // Processing states.
  REGISTERING_HOST: 2,
  STARTING_HOST: 3,
  UPDATING_PIN: 4,
  STOPPING_HOST: 5,

  // Done states.
  HOST_STARTED: 6,
  UPDATED_PIN: 7,
  HOST_STOPPED: 8,

  // Failure states.
  REGISTRATION_FAILED: 9,
  START_HOST_FAILED: 10,
  UPDATE_PIN_FAILED: 11,
  STOP_HOST_FAILED: 12
};

/** @return {remoting.HostSetupFlow.State} Current state of the flow. */
remoting.HostSetupFlow.prototype.getState = function() {
  return this.state_;
};

/**
 * @param {remoting.DaemonPlugin.AsyncResult} result Result of the
 * current step.
 * @return {remoting.HostSetupFlow.State} New state.
 */
remoting.HostSetupFlow.prototype.switchToNextStep = function(result) {
  if (this.state_ == remoting.HostSetupFlow.State.NONE) {
    return this.state_;
  }
  if (result == remoting.DaemonPlugin.AsyncResult.OK) {
    // If the current step was successful then switch to the next
    // step in the sequence.
    if (this.currentStep_ < this.sequence_.length - 1) {
      this.currentStep_ += 1;
      this.state_ = this.sequence_[this.currentStep_];
    } else {
      this.state_ = remoting.HostSetupFlow.State.NONE;
    }
  } else if (result == remoting.DaemonPlugin.AsyncResult.CANCELLED) {
    // Stop the setup flow if user rejected one of the actions.
    this.state_ = remoting.HostSetupFlow.State.NONE;
  } else {
    // Current step failed, so switch to corresponding error state.
    if (this.state_ == remoting.HostSetupFlow.State.REGISTERING_HOST) {
      this.state_ = remoting.HostSetupFlow.State.REGISTRATION_FAILED;
    } else if (this.state_ == remoting.HostSetupFlow.State.STARTING_HOST) {
      this.state_ = remoting.HostSetupFlow.State.START_HOST_FAILED;
    } else if (this.state_ == remoting.HostSetupFlow.State.UPDATING_PIN) {
      this.state_ = remoting.HostSetupFlow.State.UPDATE_PIN_FAILED;
    } else if (this.state_ == remoting.HostSetupFlow.State.STOPPING_HOST) {
      this.state_ = remoting.HostSetupFlow.State.STOP_HOST_FAILED;
    } else {
      // TODO(sergeyu): Add other error states and use them here.
      this.state_ = remoting.HostSetupFlow.State.START_HOST_FAILED;
    }
  }
  return this.state_;
};

/**
 * @param {remoting.DaemonPlugin} daemon The parent daemon plugin instance.
 * @constructor
 */
remoting.HostSetupDialog = function(daemon) {
  this.daemon_ = daemon;
  this.pinEntry_ = document.getElementById('daemon-pin-entry');
  this.pinConfirm_ = document.getElementById('daemon-pin-confirm');

  /** @type {remoting.HostSetupFlow} */
  this.flow_ = new remoting.HostSetupFlow([remoting.HostSetupFlow.State.NONE]);

  /** @type {remoting.HostSetupDialog} */
  var that = this;
  /** @param {Event} event The event. */
  var onPinSubmit = function(event) {
    event.preventDefault();
    that.onPinSubmit_();
  };
  var form = document.getElementById('ask-pin-form');
  form.addEventListener('submit', onPinSubmit, false);
};

/**
 * Show the dialog in order to get a PIN prior to starting the daemon. When the
 * user clicks OK, the dialog shows a spinner until the daemon has started.
 *
 * @return {void} Nothing.
 */
remoting.HostSetupDialog.prototype.showForStart = function() {
  this.startNewFlow_(
      [remoting.HostSetupFlow.State.ASK_PIN,
       remoting.HostSetupFlow.State.REGISTERING_HOST,
       remoting.HostSetupFlow.State.STARTING_HOST,
       remoting.HostSetupFlow.State.HOST_STARTED]);
};

/**
 * Show the dialog in order to change the PIN associated with a running daemon.
 *
 * @return {void} Nothing.
 */
remoting.HostSetupDialog.prototype.showForPin = function() {
  this.startNewFlow_(
      [remoting.HostSetupFlow.State.ASK_PIN,
       remoting.HostSetupFlow.State.UPDATING_PIN,
       remoting.HostSetupFlow.State.UPDATED_PIN]);
};

/**
 * Show the dialog in order to stop the daemon.
 *
 * @return {void} Nothing.
 */
remoting.HostSetupDialog.prototype.showForStop = function() {
  // TODO(sergeyu): Add another step to unregister the host, crubg.com/121146 .
  this.startNewFlow_(
      [remoting.HostSetupFlow.State.STOPPING_HOST,
       remoting.HostSetupFlow.State.HOST_STOPPED]);
};

/**
 * @return {void} Nothing.
 */
remoting.HostSetupDialog.prototype.hide = function() {
  remoting.setMode(remoting.AppMode.HOME);
};

/**
 * Starts new flow with the specified sequence of steps.
 * @param {Array.<remoting.HostSetupFlow.State>} sequence Sequence of steps.
 * @private
 */
remoting.HostSetupDialog.prototype.startNewFlow_ = function(sequence) {
  this.flow_ = new remoting.HostSetupFlow(sequence);
  this.pinEntry_.text = '';
  this.pinConfirm_.text = '';
  this.updateState_();
};

/**
 * Updates current UI mode according to the current state of the setup
 * flow and start the action corresponding to the current step (if
 * any).
 * @private
 */
remoting.HostSetupDialog.prototype.updateState_ = function() {
  remoting.daemonPlugin.updateDom();

  /** @param {string} tag */
  function showProcessingMessage(tag) {
    var messageDiv = document.getElementById('host-setup-processing-message');
    l10n.localizeElementFromTag(messageDiv, tag);
    remoting.setMode(remoting.AppMode.HOST_SETUP_PROCESSING);
  }
  /** @param {string} tag */
  function showDoneMessage(tag) {
    var messageDiv = document.getElementById('host-setup-done-message');
    l10n.localizeElementFromTag(messageDiv, tag);
    remoting.setMode(remoting.AppMode.HOST_SETUP_DONE);
  }
  /** @param {string} tag */
  function showErrorMessage(tag) {
    var errorDiv = document.getElementById('host-setup-error-message');
    l10n.localizeElementFromTag(errorDiv, tag);
    remoting.setMode(remoting.AppMode.HOST_SETUP_ERROR);
  }

  var state = this.flow_.getState();
  if (state == remoting.HostSetupFlow.State.NONE) {
    this.hide();
  } else if (state == remoting.HostSetupFlow.State.ASK_PIN) {
    remoting.setMode(remoting.AppMode.HOST_SETUP_ASK_PIN);
  } else if (state == remoting.HostSetupFlow.State.REGISTERING_HOST) {
    showProcessingMessage(/*i18n-content*/'HOST_SETUP_STARTING');
    this.registerHost_();
  } else if (state == remoting.HostSetupFlow.State.STARTING_HOST) {
    showProcessingMessage(/*i18n-content*/'HOST_SETUP_STARTING');
    this.startHost_();
  } else if (state == remoting.HostSetupFlow.State.UPDATING_PIN) {
    showProcessingMessage(/*i18n-content*/'HOST_SETUP_UPDATING_PIN');
    this.updatePin_();
  } else if (state == remoting.HostSetupFlow.State.STOPPING_HOST) {
    showProcessingMessage(/*i18n-content*/'HOST_SETUP_STOPPING');
    this.stopHost_();
  } else if (state == remoting.HostSetupFlow.State.HOST_STARTED) {
    showDoneMessage(/*i18n-content*/'HOST_SETUP_STARTED');
  } else if (state == remoting.HostSetupFlow.State.UPDATED_PIN) {
    showDoneMessage(/*i18n-content*/'HOST_SETUP_UPDATED_PIN');
  } else if (state == remoting.HostSetupFlow.State.HOST_STOPPED) {
    showDoneMessage(/*i18n-content*/'HOST_SETUP_STOPPED');
  } else if (state == remoting.HostSetupFlow.State.REGISTRATION_FAILED) {
    showErrorMessage(/*i18n-content*/'HOST_SETUP_REGISTRATION_FAILED');
  } else if (state == remoting.HostSetupFlow.State.START_HOST_FAILED) {
    showErrorMessage(/*i18n-content*/'HOST_SETUP_HOST_FAILED');
  } else if (state == remoting.HostSetupFlow.State.UPDATE_PIN_FAILED) {
    showErrorMessage(/*i18n-content*/'HOST_SETUP_UPDATE_PIN_FAILED');
  } else if (state == remoting.HostSetupFlow.State.STOP_HOST_FAILED) {
    showErrorMessage(/*i18n-content*/'HOST_SETUP_STOP_FAILED');
  }
};

/**
 * Registers new host.
 */
remoting.HostSetupDialog.prototype.registerHost_ = function() {
  /** @type {remoting.HostSetupDialog} */
  var that = this;
  /** @type {remoting.HostSetupFlow} */
  var flow = this.flow_;

  /** @return {string} */
  function generateUuid() {
    var random = new Uint16Array(8);
    window.crypto.getRandomValues(random);
    /** @type {Array.<string>} */
    var e = new Array();
    for (var i = 0; i < 8; i++) {
      e[i] = (/** @type {number} */random[i] + 0x10000).
          toString(16).substring(1);
    }
    return e[0] + e[1] + '-' + e[2] + "-" + e[3] + '-' +
        e[4] + '-' + e[5] + e[6] + e[7];
  }

  var newHostId = generateUuid();
  // TODO(jamiewalch): Create an unprivileged API to get the host id from the
  // plugin instead of storing it locally (crbug.com/121518).
  window.localStorage.setItem('me2me-host-id', newHostId);

  /** @param {string} privateKey
   *  @param {XMLHttpRequest} xhr */
  function onRegistered(privateKey, xhr) {
    if (flow !== that.flow_ ||
        flow.getState() != remoting.HostSetupFlow.State.REGISTERING_HOST) {
      console.error('Host setup was interrupted when registering the host');
      return;
    }

    var success = (xhr.status == 200);

    if (success) {
      // TODO(sergeyu): Calculate HMAC of the PIN instead of storing it
      // in plaintext.
      flow.hostConfig = JSON.stringify({
          xmpp_login: remoting.oauth2.getCachedEmail(),
          oauth_refresh_token: remoting.oauth2.getRefreshToken(),
          host_id: newHostId,
          host_name: that.daemon_.getHostName(),
          host_secret_hash: 'plain:' + window.btoa(flow.pin),
          private_key: privateKey
        });
    } else {
      console.log('Failed to register the host. Status: ' + xhr.status +
                  ' response: ' + xhr.responseText);
    }

    flow.switchToNextStep(success ? remoting.DaemonPlugin.AsyncResult.OK :
                          remoting.DaemonPlugin.AsyncResult.FAILED);
    that.updateState_();
  }

  /**
   * @param {string} privateKey
   * @param {string} publicKey
   * @param {string} oauthToken
   */
  function doRegisterHost(privateKey, publicKey, oauthToken) {
    if (flow !== that.flow_ ||
        flow.getState() != remoting.HostSetupFlow.State.REGISTERING_HOST) {
      console.error('Host setup was interrupted when generating key pair');
      return;
    }

    var headers = {
      'Authorization': 'OAuth ' + oauthToken,
      'Content-type' : 'application/json; charset=UTF-8'
    };

    var newHostDetails = { data: {
       hostId: newHostId,
       hostName: that.daemon_.getHostName(),
       publicKey: publicKey
    } };
    remoting.xhr.post(
        'https://www.googleapis.com/chromoting/v1/@me/hosts/',
        /** @param {XMLHttpRequest} xhr */
        function (xhr) { onRegistered(privateKey, xhr); },
        JSON.stringify(newHostDetails),
        headers);
  }

  this.daemon_.generateKeyPair(
      /** @param {string} privateKey
       *  @param {string} publicKey */
      function(privateKey, publicKey) {
      remoting.oauth2.callWithToken(
          /** @param {string} oauthToken */
          function(oauthToken) {
            doRegisterHost(privateKey, publicKey, oauthToken);
          });
      });
};

/**
 * Starts the host process after it's registered.
 */
remoting.HostSetupDialog.prototype.startHost_ = function() {
  /** @type {remoting.HostSetupDialog} */
  var that = this;
  /** @type {remoting.HostSetupFlow} */
  var flow = this.flow_;

  /** @param {remoting.DaemonPlugin.AsyncResult} result */
  function onHostStarted(result) {
    if (flow !== that.flow_ ||
        flow.getState() != remoting.HostSetupFlow.State.STARTING_HOST) {
      console.error('Host setup was interrupted when starting the host');
      return;
    }

    flow.switchToNextStep(result);
    that.updateState_();
  }
  this.daemon_.start(this.flow_.hostConfig, onHostStarted);
};

remoting.HostSetupDialog.prototype.updatePin_ = function() {
  /** @type {remoting.HostSetupDialog} */
  var that = this;
  /** @type {remoting.HostSetupFlow} */
  var flow = this.flow_;

  /** @param {remoting.DaemonPlugin.AsyncResult} result */
  function onPinUpdated(result) {
    if (flow !== that.flow_ ||
        flow.getState() != remoting.HostSetupFlow.State.UPDATING_PIN) {
      console.error('Host setup was interrupted when updating PIN');
      return;
    }

    flow.switchToNextStep(result);
    that.updateState_();
  }
  this.daemon_.setPin(this.flow_.pin, onPinUpdated);
}

/**
 * Stops the host.
 */
remoting.HostSetupDialog.prototype.stopHost_ = function() {
  /** @type {remoting.HostSetupDialog} */
  var that = this;
  /** @type {remoting.HostSetupFlow} */
  var flow = this.flow_;

  /** @param {remoting.DaemonPlugin.AsyncResult} result */
  function onHostStopped(result) {
    if (flow !== that.flow_ ||
        flow.getState() != remoting.HostSetupFlow.State.STOPPING_HOST) {
      console.error('Host setup was interrupted when stopping the host');
      return;
    }

    flow.switchToNextStep(result);
    that.updateState_();
  }
  this.daemon_.stop(onHostStopped);
};

/** @private */
remoting.HostSetupDialog.prototype.onPinSubmit_ = function() {
  if (this.flow_.getState() != remoting.HostSetupFlow.State.ASK_PIN) {
    console.error('PIN submitted in an invalid state', this.flow_.getState());
    return;
  }
  // TODO(jamiewalch): Add validation and error checks when we improve the UI.
  var pin = this.pinEntry_.value;
  this.flow_.pin = pin;
  this.flow_.switchToNextStep(remoting.DaemonPlugin.AsyncResult.OK);
  this.updateState_();
};

/** @type {remoting.HostSetupDialog} */
remoting.hostSetupDialog = null;
