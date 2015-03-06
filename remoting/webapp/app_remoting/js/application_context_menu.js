// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class representing the application's context menu.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.ContextMenuAdapter} adapter
 * @constructor
 */
remoting.ApplicationContextMenu = function(adapter) {
  /** @private {remoting.ContextMenuAdapter} */
  this.adapter_ = adapter;

  this.adapter_.create(
      remoting.ApplicationContextMenu.kSendFeedbackId,
      l10n.getTranslationOrError(/*i18n-content*/'SEND_FEEDBACK'),
      false);
  this.adapter_.create(
      remoting.ApplicationContextMenu.kShowStatsId,
      l10n.getTranslationOrError(/*i18n-content*/'SHOW_STATS'),
      true);
  this.adapter_.addListener(this.onClicked_.bind(this));

  /** @private {string} */
  this.hostId_ = '';
};

/**
 * @param {string} hostId
 */
remoting.ApplicationContextMenu.prototype.setHostId = function(hostId) {
  this.hostId_ = hostId;
}

/**
 * Add an indication of the connection RTT to the 'Show statistics' menu item.
 *
 * @param {number} rttMs The RTT of the connection, in ms.
 */
remoting.ApplicationContextMenu.prototype.updateConnectionRTT =
    function(rttMs) {
  var rttText =
      rttMs < 50  ? /*i18n-content*/'CONNECTION_QUALITY_GOOD' :
      rttMs < 100 ? /*i18n-content*/'CONNECTION_QUALITY_FAIR' :
                    /*i18n-content*/'CONNECTION_QUALITY_POOR';
  rttText = l10n.getTranslationOrError(rttText);
  this.adapter_.updateTitle(
      remoting.ApplicationContextMenu.kShowStatsId,
      l10n.getTranslationOrError(/*i18n-content*/'SHOW_STATS_WITH_RTT',
                                 rttText));
};

/** @param {OnClickData=} info */
remoting.ApplicationContextMenu.prototype.onClicked_ = function(info) {
  switch (info.menuItemId) {

    case remoting.ApplicationContextMenu.kSendFeedbackId:
      var windowAttributes = {
        bounds: {
          width: 400,
          height: 100
        },
        resizable: false
      };

      /** @type {remoting.ApplicationContextMenu} */
      var that = this;

      /** @param {AppWindow} consentWindow */
      var onCreate = function(consentWindow) {
        var onLoad = function() {
          var message = {
            method: 'init',
            hostId: that.hostId_,
            connectionStats: JSON.stringify(remoting.stats.mostRecent())
          };
          consentWindow.contentWindow.postMessage(message, '*');
        };
        consentWindow.contentWindow.addEventListener('load', onLoad, false);
      };
      chrome.app.window.create(
          'feedback_consent.html', windowAttributes, onCreate);
      break;

    case remoting.ApplicationContextMenu.kShowStatsId:
      if (remoting.stats) {
        remoting.stats.show(info.checked);
      }
      break;
  }
};


/** @type {string} */
remoting.ApplicationContextMenu.kSendFeedbackId = 'send-feedback';

/** @type {string} */
remoting.ApplicationContextMenu.kShowStatsId = 'show-stats';
