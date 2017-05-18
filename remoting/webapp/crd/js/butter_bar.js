// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * ButterBar class that is used to show a butter bar with deprecation messages.
 * Each message is displayed for at most one week.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.ButterBar = function() {
  /** @private @const */
  this.messages_ = [
    {id: /*i18n-content*/'WEBSITE_INVITE_BETA', dismissable: true},
    {id: /*i18n-content*/'WEBSITE_INVITE_STABLE', dismissable: true},
    {id: /*i18n-content*/'WEBSITE_INVITE_DEPRECATION_1', dismissable: true},
    {id: /*i18n-content*/'WEBSITE_INVITE_DEPRECATION_2', dismissable: false},
  ];
  // TODO(jamiewalch): Read the message index using metricsPrivate.
  this.currentMessage_ = -1;
  /** @private {!Element} */
  this.root_ = document.getElementById(remoting.ButterBar.kId_);
  /** @private {!Element} */
  this.message_ = document.getElementById(remoting.ButterBar.kMessageId_);
  /** @private {!Element} */
  this.dismiss_ = document.getElementById(remoting.ButterBar.kDismissId_);
}

remoting.ButterBar.prototype.init = function() {
  var result = new base.Deferred();
  if (this.currentMessage_ > -1) {
    chrome.storage.sync.get(
        [remoting.ButterBar.kStorageKey_],
        (syncValues) => {
          this.onStateLoaded_(syncValues);
          result.resolve();
        });
  } else {
    result.resolve();
  }
  return result.promise();
}

/**
 * Shows butter bar with the current message.
 *
 * @param {string} messageId
 * @param {string|Array} substitutions
 * @param {boolean} dismissable
 * @private
 */
remoting.ButterBar.prototype.show_ = function() {
  var messageId = this.messages_[this.currentMessage_].id;
  var substitutions = ['<a href="https://example.com" target="_blank">', '</a>'];
  var dismissable = this.messages_[this.currentMessage_].dismissable;
  l10n.localizeElementFromTag(this.message_, messageId, substitutions, true);
  if (dismissable) {
    this.dismiss_.addEventListener('click', this.dismiss.bind(this), false);
  } else {
    this.dismiss_.hidden = true;
    this.root_.classList.add('red');
  }
  this.root_.hidden = false;
}

/**
  * @param {Object} syncValues
  * @private
  */
remoting.ButterBar.prototype.onStateLoaded_ = function(syncValues) {
  /** @type {!Object|undefined} */
  var messageState = syncValues[remoting.ButterBar.kStorageKey_];
  if (!messageState) {
    messageState = {
      index: -1,
      timestamp: new Date().getTime(),
      hidden: false,
    }
  }

  // Show the current message unless it was explicitly dismissed or if it was
  // first shown more than a week ago. If it is marked as not dismissable, show
  // it unconditionally.
  var elapsed = new Date() - messageState.timestamp;
  var show =
      this.currentMessage_ > messageState.index ||
      !this.messages_[this.currentMessage_].dismissable ||
      (!messageState.hidden && elapsed <= remoting.ButterBar.kTimeout_);

  if (show) {
    this.show_();
    // If this is the first time this message is being displayed, update the
    // saved state.
    if (this.currentMessage_ > messageState.index) {
      var value = {};
      value[remoting.ButterBar.kStorageKey_] = {
        index: this.currentMessage_,
        timestamp: new Date().getTime(),
        hidden: false
      };
      chrome.storage.sync.set(value);
    }
  }
};

/**
 * Hide the butter bar request and record the message that was being displayed.
 */
remoting.ButterBar.prototype.dismiss = function() {
  var value = {};
  value[remoting.ButterBar.kStorageKey_] = {
    index: this.currentMessage_,
    timestamp: new Date().getTime(),
    hidden: true
  };
  chrome.storage.sync.set(value);

  this.root_.hidden = true;
};

/** @const @private */
remoting.ButterBar.kId_ = 'butter-bar';

/** @const @private */
remoting.ButterBar.kMessageId_ = 'butter-bar-message';
/** @const @private */
remoting.ButterBar.kDismissId_ = 'butter-bar-dismiss';

/** @const @private */
remoting.ButterBar.kStorageKey_ = 'message-state';

/** @const @private */
remoting.ButterBar.kTimeout_ = 7 * 24 * 60 * 60 * 1000;   // 1 week
