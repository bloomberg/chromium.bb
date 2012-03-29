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
  ASK_PIN: 1,
  REGISTER_HOST: 2,
  START_HOST: 3,
  HOST_STARTED: 4,
  UPDATE_PIN: 5,
  REGISTRATION_FAILED: 6,
  HOST_START_FAILED: 7
};

/** @return {remoting.HostSetupFlow.State} Current state of the flow. */
remoting.HostSetupFlow.prototype.getState = function() {
  return this.state_;
};

/**
 * @param {boolean} success
 * @return {remoting.HostSetupFlow.State} New state.
 */
remoting.HostSetupFlow.prototype.switchToNextStep = function(success) {
  if (this.state_ == remoting.HostSetupFlow.State.NONE) {
    return this.state_;
  }
  if (success) {
    // If the current step was successful then switch to the next
    // step in the sequence.
    if (this.currentStep_ < this.sequence_.length - 1) {
      this.currentStep_ += 1;
      this.state_ = this.sequence_[this.currentStep_];
    } else {
      this.state_ = remoting.HostSetupFlow.State.NONE;
    }
  } else {
    // Current step failed, so switch to corresponding error state.
    if (this.state_ == remoting.HostSetupFlow.State.REGISTER_HOST) {
      this.state_ = remoting.HostSetupFlow.State.REGISTRATION_FAILED;
    } else {
      // TODO(sergeyu): Add other error states and use them here.
      this.state_ = remoting.HostSetupFlow.State.HOST_START_FAILED;
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
       remoting.HostSetupFlow.State.REGISTER_HOST,
       remoting.HostSetupFlow.State.START_HOST,
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
       remoting.HostSetupFlow.State.UPDATE_PIN]);
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
  /** @param {string} tag */
  function showProcessingMessage(tag) {
    var errorDiv = document.getElementById('host-setup-processing-message');
    l10n.localizeElementFromTag(errorDiv, tag);
    remoting.setMode(remoting.AppMode.HOST_SETUP_PROCESSING);
  }
  /** @param {string} tag */
  function showDoneMessage(tag) {
    var errorDiv = document.getElementById('host-setup-done-message');
    l10n.localizeElementFromTag(errorDiv, tag);
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
  } else if (state == remoting.HostSetupFlow.State.REGISTER_HOST) {
    showProcessingMessage(/*i18n-content*/'HOST_SETUP_STARTING');
    this.registerHost_();
  } else if (state == remoting.HostSetupFlow.State.START_HOST) {
    showProcessingMessage(/*i18n-content*/'HOST_SETUP_STARTING');
    this.startHost_();
  } else if (state == remoting.HostSetupFlow.State.UPDATE_PIN) {
    showProcessingMessage(/*i18n-content*/'HOST_SETUP_UPDATING_PIN');
    this.updatePin_();
  } else if (state == remoting.HostSetupFlow.State.HOST_STARTED) {
    showDoneMessage(/*i18n-content*/'HOST_SETUP_STARTED');
  } else if (state == remoting.HostSetupFlow.State.REGISTRATION_FAILED) {
    showErrorMessage(/*i18n-content*/'HOST_SETUP_REGISTRATION_FAILED');
  } else if (state == remoting.HostSetupFlow.State.HOST_START_FAILED) {
    showErrorMessage(/*i18n-content*/'HOST_SETUP_HOST_FAILED');
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

  /** @param {string} privateKey
   *  @param {XMLHttpRequest} xhr */
  function onRegistered(privateKey, xhr) {
    if (flow !== that.flow_ ||
        flow.getState() != remoting.HostSetupFlow.State.REGISTER_HOST) {
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
          host_secret_hash: 'plain:' + flow.pin,
          private_key: privateKey
        });
    } else {
      console.log('Failed to register the host. Status: ' + xhr.status +
                  ' response: ' + xhr.responseText);
    }

    flow.switchToNextStep(success);
    that.updateState_();
  }

  /**
   * @param {string} privateKey
   * @param {string} publicKey
   * @param {string} oauthToken
   */
  function doRegisterHost(privateKey, publicKey, oauthToken) {
    if (flow !== that.flow_ ||
        flow.getState() != remoting.HostSetupFlow.State.REGISTER_HOST) {
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
  this.daemon_.start(this.flow_.hostConfig);
  this.pollDaemonState_();
};

remoting.HostSetupDialog.prototype.updatePin_ = function() {
  this.daemon_.setPin(this.flow_.pin);
  this.pollDaemonState_();
}

/** @private */
remoting.HostSetupDialog.prototype.onPinSubmit_ = function() {
  if (this.flow_.getState() != remoting.HostSetupFlow.State.ASK_PIN) {
    console.error('PIN submitted in an invalid state', this.flow_.getState());
    return;
  }
  // TODO(jamiewalch): Add validation and error checks when we improve the UI.
  var pin = this.pinEntry_.value;
  this.flow_.pin = pin;
  this.flow_.switchToNextStep(true);
  this.updateState_();
};

/**
 * @return {void} Nothing.
 * @private
 */
remoting.HostSetupDialog.prototype.pollDaemonState_ = function() {
  var state = this.daemon_.state();
  var retry = false;  // Set to true if we haven't finished yet.
  switch (state) {
    case remoting.DaemonPlugin.State.STOPPED:
    case remoting.DaemonPlugin.State.NOT_INSTALLED:
      retry = true;
      break;
    case remoting.DaemonPlugin.State.STARTED:
      if (this.flow_.getState() == remoting.HostSetupFlow.State.START_HOST ||
          this.flow_.getState() == remoting.HostSetupFlow.State.UPDATE_PIN) {
        this.flow_.switchToNextStep(true);
        this.updateState_();
      }
      this.daemon_.updateDom();
      break;
    case remoting.DaemonPlugin.State.START_FAILED:
      if (this.flow_.getState() == remoting.HostSetupFlow.State.START_HOST ||
          this.flow_.getState() == remoting.HostSetupFlow.State.UPDATE_PIN) {
        this.flow_.switchToNextStep(false);
        this.updateState_();
      }
      break;
    default:
      // TODO(jamiewalch): Show an error message.
      console.error('Unexpected daemon state', state);
      break;
  }
  if (retry) {
    /** @type {remoting.HostSetupDialog} */
    var that = this;
    var pollDaemonState = function() { that.pollDaemonState_(); }
    window.setTimeout(pollDaemonState, 1000);
  }
};

/** @type {remoting.HostSetupDialog} */
remoting.hostSetupDialog = null;
