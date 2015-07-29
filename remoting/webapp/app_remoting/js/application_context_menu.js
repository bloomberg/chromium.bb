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
 * @param {remoting.ClientPlugin} plugin
 * @param {remoting.ClientSession} clientSession
 * @param {remoting.WindowShape} windowShape
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.ApplicationContextMenu = function(adapter, plugin, clientSession,
                                           windowShape) {
  /** @private */
  this.adapter_ = adapter;

  /** @private */
  this.clientSession_ = clientSession;

  this.adapter_.create(
      remoting.ApplicationContextMenu.kSendFeedbackId,
      l10n.getTranslationOrError(/*i18n-content*/'SEND_FEEDBACK'),
      false);
  this.adapter_.create(
      remoting.ApplicationContextMenu.kShowStatsId,
      l10n.getTranslationOrError(/*i18n-content*/'SHOW_STATS'),
      true);
  this.adapter_.create(
      remoting.ApplicationContextMenu.kShowCreditsId,
      l10n.getTranslationOrError(/*i18n-content*/'CREDITS'),
      true);

  // TODO(kelvinp):Unhook this event on shutdown.
  this.adapter_.addListener(this.onClicked_.bind(this));

  /** @private {string} */
  this.hostId_ = '';

  /** @private */
  this.stats_ = new remoting.ConnectionStats(
      document.getElementById('statistics'), plugin, windowShape);
};

remoting.ApplicationContextMenu.prototype.dispose = function() {
  base.dispose(this.stats_);
  this.stats_ = null;
};

/**
 * @param {string} hostId
 */
remoting.ApplicationContextMenu.prototype.setHostId = function(hostId) {
  this.hostId_ = hostId;
};

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
  var menuId = /** @type {string} */ (info.menuItemId.toString());
  switch (menuId) {

    case remoting.ApplicationContextMenu.kSendFeedbackId:
      var windowAttributes = {
        bounds: {
          width: 400,
          height: 100,
          left: undefined,
          top: undefined
        },
        resizable: false
      };

      /** @type {remoting.ApplicationContextMenu} */
      var that = this;

      /** @param {chrome.app.window.AppWindow} consentWindow */
      var onCreate = function(consentWindow) {
        var onLoad = function() {
          var message = {
            method: 'init',
            appId: remoting.app.getApplicationId(),
            hostId: that.hostId_,
            connectionStats: JSON.stringify(that.stats_.mostRecent()),
            sessionId: that.clientSession_.getLogger().getSessionId(),
            consoleErrors: JSON.stringify(
                remoting.ConsoleWrapper.getInstance().getHistory())
          };
          consentWindow.contentWindow.postMessage(message, '*');
        };
        consentWindow.contentWindow.addEventListener('load', onLoad, false);
      };
      chrome.app.window.create(
          '_modules/koejkfhmphamcgafjmkellhnekdkopod/feedback_consent.html',
          windowAttributes, onCreate);
      break;

    case remoting.ApplicationContextMenu.kShowStatsId:
      this.stats_.show(info.checked);
      break;

    case remoting.ApplicationContextMenu.kShowCreditsId:
      chrome.app.window.create(
          '_modules/koejkfhmphamcgafjmkellhnekdkopod/credits.html',
          {
            'width': 800,
            'height': 600,
            'id' : 'remoting-credits'
          });
      break;
  }
};


/** @type {string} */
remoting.ApplicationContextMenu.kSendFeedbackId = 'send-feedback';

/** @type {string} */
remoting.ApplicationContextMenu.kShowStatsId = 'show-stats';

/** @type {string} */
remoting.ApplicationContextMenu.kShowCreditsId = 'show-credits';
