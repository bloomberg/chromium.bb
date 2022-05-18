// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used by the time zone subpage page. */

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class TimeZoneBrowserProxy {
  /** Notifies C++ code to show parent access code verification view. */
  showParentAccessForTimeZone() {}

  /** Notifies C++ code that the date_time page is ready. */
  dateTimePageReady() {}

  /** Notifies C++ code to show the chrome://set-time standalone dialog. */
  showSetDateTimeUI() {}

  /** @return {!Promise<!Array<!Array<string>>>} */
  getTimeZones() {}
}

/** @implements {TimeZoneBrowserProxy} */
export class TimeZoneBrowserProxyImpl {
  /** @override */
  showParentAccessForTimeZone() {
    chrome.send('handleShowParentAccessForTimeZone');
  }

  /** @override */
  dateTimePageReady() {
    chrome.send('dateTimePageReady');
  }

  /** @override */
  showSetDateTimeUI() {
    chrome.send('showSetDateTimeUI');
  }

  /** @override */
  getTimeZones() {
    return sendWithPromise('getTimeZones');
  }

  /** @return {!TimeZoneBrowserProxy} */
  static getInstance() {
    return instance || (instance = new TimeZoneBrowserProxyImpl());
  }

  /** @param {!TimeZoneBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?TimeZoneBrowserProxy} */
let instance = null;
